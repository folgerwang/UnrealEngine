// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureAllocator.h"
#include "AllocatedVirtualTexture.h"
#include "VirtualTexturing.h"

FVirtualTextureAllocator::FVirtualTextureAllocator(uint32 Dimensions )
	: vDimensions( Dimensions )
	, LogSize(0u)
	, NumAllocations(0u)
	, NumAllocatedPages(0u)
{
}

void FVirtualTextureAllocator::Initialize(uint32 InSize)
{
	check(LogSize == 0u);
	LogSize = FMath::CeilLogTwo(InSize);

	AddressBlocks.Reset(1);
	SortedAddresses.Reset(1);
	SortedIndices.Reset(1);
	FreeList.Reset(LogSize + 1);

	// Start with one empty block
	AddressBlocks.Add(FAddressBlock(LogSize));
	SortedAddresses.Add(0u);
	SortedIndices.Add(0u);

	// Init free list
	FreeList.AddUninitialized(LogSize + 1);
	for (uint8 i = 0; i < LogSize; i++)
	{
		FreeList[i] = 0xffff;
	}
	FreeList[LogSize] = 0;
}

void FVirtualTextureAllocator::Grow()
{
	FAddressBlock RootBlock(LogSize);
	++LogSize;

	// Add entry for for free list of next LogSize (currently empty)
	FreeList.Add(0xffff);

	const uint32 NumSiblings = (1 << vDimensions) - 1;
	SortedAddresses.InsertUninitialized(0, NumSiblings);
	SortedIndices.InsertUninitialized(0, NumSiblings);
	check(SortedAddresses.Num() == SortedIndices.Num());

	for (uint32 Sibling = NumSiblings; Sibling > 0; Sibling--)
	{
		const uint32 Index = AddressBlocks.Add(FAddressBlock(RootBlock, Sibling, vDimensions));
		FAddressBlock& AddressBlock = AddressBlocks[Index];

		// Place on free list
		AddressBlock.NextFree = FreeList[AddressBlock.vLogSize];
		if (AddressBlock.NextFree != 0xffff)
		{
			AddressBlocks[AddressBlock.NextFree].PrevFree = Index;
		}
		FreeList[AddressBlock.vLogSize] = Index;

		// Add to sorted list
		SortedAddresses[NumSiblings - Sibling] = AddressBlock.vAddress;
		SortedIndices[NumSiblings - Sibling] = Index;
	}
}

// returns SortedIndex
uint32 FVirtualTextureAllocator::Find(uint32 vAddress ) const
{
	uint32 Min = 0;
	uint32 Max = SortedAddresses.Num();
	
	// Binary search for lower bound
	while( Min != Max )
	{
		const uint32 Mid = Min + (Max - Min) / 2;
		const uint32 Key = SortedAddresses[ Mid ];

		if( vAddress < Key )
			Min = Mid + 1;
		else
			Max = Mid;
	}

	return Min;
}

FAllocatedVirtualTexture* FVirtualTextureAllocator::Find(uint32 vAddress, uint32& Local_vAddress ) const
{
	const uint32 SortedIndex = Find( vAddress );

	const uint16 Index = SortedIndices[SortedIndex];
	const FAddressBlock& AddressBlock = AddressBlocks[ Index ];
	checkSlow( SortedAddresses[SortedIndex] == AddressBlock.vAddress );

	const uint32 BlockSize = 1 << ( vDimensions * AddressBlock.vLogSize );
	if( vAddress >= AddressBlock.vAddress &&
		vAddress <  AddressBlock.vAddress + BlockSize )
	{
		Local_vAddress = vAddress - AddressBlock.vAddress;
		// TODO mip bias
		return AddressBlock.VT;
	}

	return nullptr;
}

uint32 FVirtualTextureAllocator::Alloc(FAllocatedVirtualTexture* VT )
{
	uint32 BlockSize = FMath::Max( VT->GetWidthInTiles(), VT->GetHeightInTiles() );
	uint8 vLogSize = FMath::CeilLogTwo( BlockSize );

	// Find smallest free that fits
	for( int i = vLogSize; i < FreeList.Num(); i++ )
	{
		uint16 FreeIndex = FreeList[i];
		if( FreeIndex != 0xffff )
		{
			// Found free
			FAddressBlock *AllocBlock = &AddressBlocks[ FreeIndex ];
			checkSlow( AllocBlock->VT == nullptr );
			checkSlow( AllocBlock->PrevFree == 0xffff );

			// Remove from free list
			FreeList[i] = AllocBlock->NextFree;
			if( AllocBlock->NextFree != 0xffff )
			{
				AddressBlocks[ AllocBlock->NextFree ].PrevFree = 0xffff;
				AllocBlock->NextFree = 0xffff;
			}

			AllocBlock->VT = VT;

			// Add to hash table
			uint16 Key = reinterpret_cast< UPTRINT >( VT ) / 16;
			HashTable.Add( Key, FreeIndex );

			// Recursive subdivide until the right size
			int NumNewBlocks = 0;
			while( AllocBlock->vLogSize > vLogSize )
			{
				AllocBlock->vLogSize--;
				const uint32 NumSiblings = (1 << vDimensions) - 1;
				for( uint32 Sibling = NumSiblings; Sibling > 0; Sibling-- )
				{
					AddressBlocks.Add( FAddressBlock( *AllocBlock, Sibling, vDimensions ) );
					AllocBlock = &AddressBlocks[FreeIndex]; // Adding items may reallocate the list so we need to grab the pointer to the item at index FreeIndex again...
				}
				NumNewBlocks += NumSiblings;
			}

			if (NumNewBlocks)
			{
				const int32 SortedIndex = Find( AllocBlock->vAddress );
				checkSlow( AllocBlock->vAddress == SortedAddresses[ SortedIndex ] );

				// Make room for newly added
				SortedAddresses.InsertUninitialized( SortedIndex, NumNewBlocks);
				SortedIndices.InsertUninitialized(SortedIndex, NumNewBlocks);
				check(SortedAddresses.Num() == SortedIndices.Num());

				for (int Block = 0; Block < NumNewBlocks; Block++)
				{
					const uint32 Index = AddressBlocks.Num() - NumNewBlocks + Block;
					FAddressBlock& AddressBlock = AddressBlocks[ Index ];

					// Place on free list
					AddressBlock.NextFree = FreeList[ AddressBlock.vLogSize ];
					if (AddressBlock.NextFree != 0xffff)
					{
						AddressBlocks[ AddressBlock.NextFree ].PrevFree = Index;
					}
					FreeList[ AddressBlock.vLogSize ] = Index;

					// Add to sorted list
					SortedAddresses[ SortedIndex + Block ] = AddressBlock.vAddress;
					SortedIndices[ SortedIndex + Block ] = Index;
				}
			}

			++NumAllocations;
			NumAllocatedPages += 1u << (vDimensions * vLogSize);
			return AllocBlock->vAddress;
		}
	}

	return ~0u;
}

void FVirtualTextureAllocator::Free( FAllocatedVirtualTexture* VT )
{
	// Find block index
	uint16 Key = reinterpret_cast< UPTRINT >( VT ) / 16;
	uint32 Index;
	for( Index = HashTable.First( Key ); HashTable.IsValid( Index ); Index = HashTable.Next( Index ) )
	{
		if( AddressBlocks[ Index ].VT == VT )
		{
			break;
		}
	}
	if( HashTable.IsValid( Index ) )
	{
		FAddressBlock& AddressBlock = AddressBlocks[ Index ];
		checkSlow( AddressBlock.VT == VT );
		checkSlow( AddressBlock.NextFree == 0xffff );
		checkSlow( AddressBlock.PrevFree == 0xffff );
		
		check(NumAllocations > 0u);
		--NumAllocations;

		const uint32 NumPagesForBlock = 1u << (vDimensions * AddressBlock.vLogSize);
		check(NumAllocatedPages >= NumPagesForBlock);
		NumAllocatedPages -= NumPagesForBlock;

		AddressBlock.VT = nullptr;

		// TODO merge with sibling free blocks

		// Place on free list
		AddressBlock.NextFree = FreeList[ AddressBlock.vLogSize ];
		if (AddressBlock.NextFree != 0xffff)
		{
			AddressBlocks[ AddressBlock.NextFree ].PrevFree = Index;
		}
		FreeList[ AddressBlock.vLogSize ] = Index;

		// Remove the index from the hash table as it may be reused later
		HashTable.Remove(Key, Index);
	}
}

void FVirtualTextureAllocator::DumpToConsole(bool verbose)
{
	for (int32 BlockID = 0; BlockID < AddressBlocks.Num(); BlockID++)
	{
		FAddressBlock &Block = AddressBlocks[BlockID];
		uint32 Size = 1 << Block.vLogSize;
		UE_LOG(LogConsoleResponse, Display, TEXT("Block: vAddress %i, size: %ix%i (tiles),  "), Block.vAddress, Size, Size);
		if (Block.VT != nullptr)
		{
			Block.VT->DumpToConsole(verbose);
		}
		else
		{
			if (verbose)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("NULL VT"));
			}
		}
	}
}

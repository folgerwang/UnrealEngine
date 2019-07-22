// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "Containers/HashTable.h"

class FAllocatedVirtualTexture;

// Allocates virtual memory address space
class RENDERER_API FVirtualTextureAllocator
{
public:
						FVirtualTextureAllocator(uint32 Dimensions);
						~FVirtualTextureAllocator() {}

	inline uint32 GetNumAllocations() const { return NumAllocations; }
	inline uint32 GetNumAllocatedPages() const { return NumAllocatedPages; }

	void Initialize(uint32 InSize);

	/**
	 * Increase size of region managed by allocator by factor of 2 in each dimension
	 */
	void Grow();

	/**
	 * Translate a virtual page address in the address space to a local page address within
	 * a virtual texture.
	 * @return nullptr If there is no virtual texture allocated at this address
	 */
	FAllocatedVirtualTexture* Find(uint32 vAddress, uint32& Local_vAddress ) const;
	
	/**
	 * Allocate address space for the virtual texture.
	 * @return (~0) if no space left, the virtual page address if successfully allocated
	 */
	uint32 Alloc(FAllocatedVirtualTexture* VT );
	
	/**
	 * Free the virtual texture from 
	*/
	void Free(FAllocatedVirtualTexture* VT );
	
	// TODO				Realloc

	void DumpToConsole(bool verbose);

private:
	uint32				Find( uint32 vAddress ) const;

	struct FAddressBlock
	{
		FAllocatedVirtualTexture*	VT;
		uint32						vAddress : 24;
		uint32						vLogSize : 4;
		uint32						MipBias : 4;
		uint16						NextFree;
		uint16						PrevFree;

		FAddressBlock()
		{}

		FAddressBlock( uint8 LogSize )
			: VT( nullptr )
			, vAddress( 0 )
			, vLogSize( LogSize )
			, MipBias( 0 )
			, NextFree(0xffff)
			, PrevFree(0xffff)
		{}

		FAddressBlock( const FAddressBlock& Block, uint32 Offset, uint32 Dimensions )
			: VT( nullptr )
			, vAddress( Block.vAddress + ( Offset << ( Dimensions * Block.vLogSize ) ) )
			, vLogSize( Block.vLogSize )
			, MipBias( 0 )
			, NextFree(0xffff)
			, PrevFree(0xffff)
		{}
	};

	uint32	vDimensions;

	TArray< FAddressBlock >	AddressBlocks;
	TArray< uint16 >		FreeList;
	TArray< uint32 >		SortedAddresses;
	TArray< uint16 >		SortedIndices;
	FHashTable				HashTable;
	uint32                  LogSize;
	uint32					NumAllocations;
	uint32					NumAllocatedPages;
};

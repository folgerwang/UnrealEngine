// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/HashTable.h"
#include "VirtualTexturing.h"
#include "VirtualTextureSystem.h"
#include "VirtualTextureProducer.h"
#include "VirtualTexturePhysicalSpace.h"

union FMappingRequest
{
	inline FMappingRequest() {}
	inline FMappingRequest(uint16 InLoadIndex, uint8 InLocalLayerIndex, uint8 InSpaceID, uint8 InLayerIndex, uint32 InAddress, uint8 InLevel, uint8 InLocalLevel)
		: vAddress(InAddress), vLevel(InLevel), SpaceID(InSpaceID), LoadRequestIndex(InLoadIndex), Local_vLevel(InLocalLevel), LocalLayerIndex(InLocalLayerIndex), LayerIndex(InLayerIndex), Pad(0)
	{}

	uint64 PackedValue;
	struct
	{
		uint32 vAddress : 24;
		uint32 vLevel : 4;
		uint32 SpaceID : 4;
		uint32 LoadRequestIndex : 16;
		uint32 Local_vLevel : 4;
		uint32 LocalLayerIndex : 4;
		uint32 LayerIndex : 4;
		uint32 Pad : 4;
	};
};
static_assert(sizeof(FMappingRequest) == sizeof(uint64), "Bad packing");
inline bool operator==(const FMappingRequest& Lhs, const FMappingRequest& Rhs) { return Lhs.PackedValue == Rhs.PackedValue; }
inline bool operator!=(const FMappingRequest& Lhs, const FMappingRequest& Rhs) { return Lhs.PackedValue != Rhs.PackedValue; }

union FDirectMappingRequest
{
	inline FDirectMappingRequest() {}
	inline FDirectMappingRequest(uint8 InSpaceID, uint16 InPhysicalSpaceID, uint8 InLayer, uint8 InLogSize, uint32 InAddress, uint8 InLevel, uint16 InPhysicalAddress)
		: vAddress(InAddress), vLevel(InLevel), SpaceID(InSpaceID), pAddress(InPhysicalAddress), PhysicalSpaceID(InPhysicalSpaceID), Local_vLevel(InLogSize), LayerIndex(InLayer)
	{}

	uint64 PackedValue;
	struct
	{
		uint32 vAddress : 24;
		uint32 vLevel : 4;
		uint32 SpaceID : 4;
		uint32 pAddress : 16;
		uint32 PhysicalSpaceID : 8;
		uint32 Local_vLevel : 4;
		uint32 LayerIndex : 4;
	};
};
static_assert(sizeof(FDirectMappingRequest) == sizeof(uint64), "Bad packing");
inline bool operator==(const FDirectMappingRequest& Lhs, const FDirectMappingRequest& Rhs) { return Lhs.PackedValue == Rhs.PackedValue; }
inline bool operator!=(const FDirectMappingRequest& Lhs, const FDirectMappingRequest& Rhs) { return Lhs.PackedValue != Rhs.PackedValue; }

class FUniqueRequestList
{
public:
	// Make separate allocations to avoid any single MemStack allocation larger than FPageAllocator::PageSize (65536)
	// MemStack also allocates extra bytes to ensure proper alignment, so actual size we can allocate is typically 8 bytes less than this
	explicit FUniqueRequestList(FMemStack& MemStack)
		: LoadRequestHash(NoInit)
		, MappingRequestHash(NoInit)
		, DirectMappingRequestHash(NoInit)
		, LoadRequests(new(MemStack) FVirtualTextureLocalTile[LoadRequestCapacity])
		, MappingRequests(new(MemStack) FMappingRequest[MappingRequestCapacity])
		, DirectMappingRequests(new(MemStack) FDirectMappingRequest[DirectMappingRequestCapacity])
		, LoadRequestCount(new(MemStack) uint16[LoadRequestCapacity])
		, LoadRequestLayerMask(new(MemStack) uint8[LoadRequestCapacity])
		, NumLoadRequests(0u)
		, NumLockRequests(0u)
		, NumMappingRequests(0u)
		, NumDirectMappingRequests(0u)
	{
	}

	inline void Initialize()
	{
		LoadRequestHash.Clear();
		MappingRequestHash.Clear();
		DirectMappingRequestHash.Clear();
	}

	inline uint32 GetNumLoadRequests() const { return NumLoadRequests; }
	inline uint32 GetNumMappingRequests() const { return NumMappingRequests; }
	inline uint32 GetNumDirectMappingRequests() { return NumDirectMappingRequests; }

	inline const FVirtualTextureLocalTile& GetLoadRequest(uint32 i) const { checkSlow(i < NumLoadRequests); return LoadRequests[i]; }
	inline const FMappingRequest& GetMappingRequest(uint32 i) const { checkSlow(i < NumMappingRequests); return MappingRequests[i]; }
	inline const FDirectMappingRequest& GetDirectMappingRequest(uint32 i) const { checkSlow(i < NumDirectMappingRequests); return DirectMappingRequests[i]; }
	inline uint8 GetLocalLayerMask(uint32 i) const { checkSlow(i < NumLoadRequests); return LoadRequestLayerMask[i]; }
	inline bool IsLocked(uint32 i) const { checkSlow(i < NumLoadRequests); return i < NumLockRequests; }

	uint16 AddLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 LayerMask, uint16 Count);
	uint16 LockLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 LayerMask);

	void AddMappingRequest(uint16 LoadRequestIndex, uint8 LocalLayerIndex, uint8 SpaceID, uint8 LayerIndex, uint32 vAddress, uint8 vLevel, uint8 Local_vLevel);

	void AddDirectMappingRequest(uint8 InSpaceID, uint16 InPhysicalSpaceID, uint8 InLayer, uint8 InLogSize, uint32 InAddress, uint8 InLevel, uint16 InPhysicalAddress);
	void AddDirectMappingRequest(const FDirectMappingRequest& Request);

	void MergeRequests(const FUniqueRequestList* RESTRICT Other, FMemStack& MemStack);

	void SortRequests(FVirtualTextureProducerCollection& Producers, FMemStack& MemStack, uint32 MaxNumRequests);

private:
	static const uint32 LoadRequestCapacity = 4u * 1024;
	static const uint32 MappingRequestCapacity = 8u * 1024 - 256u;
	static const uint32 DirectMappingRequestCapacity = MappingRequestCapacity;

	TStaticHashTable<1024u, LoadRequestCapacity> LoadRequestHash;
	TStaticHashTable<1024u, MappingRequestCapacity> MappingRequestHash;
	TStaticHashTable<512u, DirectMappingRequestCapacity> DirectMappingRequestHash;

	FVirtualTextureLocalTile* LoadRequests;
	FMappingRequest* MappingRequests;
	FDirectMappingRequest* DirectMappingRequests;
	uint16* LoadRequestCount;
	uint8* LoadRequestLayerMask;

	uint32 NumLoadRequests;
	uint32 NumLockRequests;
	uint32 NumMappingRequests;
	uint32 NumDirectMappingRequests;
};


inline uint16 FUniqueRequestList::AddLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 LayerMask, uint16 Count)
{
	const uint16 Hash = MurmurFinalize64(Tile.PackedValue);
	check(LayerMask != 0u);

	for (uint16 Index = LoadRequestHash.First(Hash); LoadRequestHash.IsValid(Index); Index = LoadRequestHash.Next(Index))
	{
		if (Tile == LoadRequests[Index])
		{
			const uint32 PrevCount = LoadRequestCount[Index];
			if (PrevCount != 0xffff)
			{
				// Don't adjust count if already locked, don't allow request to transition to lock
				LoadRequestCount[Index] = FMath::Min<uint32>(PrevCount + Count, 0xfffe);
			}
			LoadRequestLayerMask[Index] |= LayerMask;
			return Index;
		}
	}
	
	if (NumLoadRequests < LoadRequestCapacity)
	{
		const uint32 Index = NumLoadRequests++;
		LoadRequestHash.Add(Hash, Index);
		LoadRequests[Index] = Tile;
		LoadRequestCount[Index] = FMath::Min<uint32>(Count, 0xfffe);
		LoadRequestLayerMask[Index] = LayerMask;
		return Index;
	}
	return 0xffff;
}

inline uint16 FUniqueRequestList::LockLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 LayerMask)
{
	const uint16 Hash = MurmurFinalize64(Tile.PackedValue);
	check(LayerMask != 0u);

	for (uint16 Index = LoadRequestHash.First(Hash); LoadRequestHash.IsValid(Index); Index = LoadRequestHash.Next(Index))
	{
		if (Tile == LoadRequests[Index])
		{
			if (LoadRequestCount[Index] != 0xffff)
			{
				LoadRequestCount[Index] = 0xffff;
				++NumLockRequests;
			}
			LoadRequestLayerMask[Index] |= LayerMask;
			return Index;
		}
	}

	if (NumLoadRequests < LoadRequestCapacity)
	{
		const uint32 Index = NumLoadRequests++;
		LoadRequestHash.Add(Hash, Index);
		LoadRequests[Index] = Tile;
		LoadRequestCount[Index] = 0xffff;
		LoadRequestLayerMask[Index] = LayerMask;
		++NumLockRequests;
		return Index;
	}

	return 0xffff;
}

inline void FUniqueRequestList::AddMappingRequest(uint16 LoadRequestIndex, uint8 LocalLayerIndex, uint8 SpaceID, uint8 LayerIndex, uint32 vAddress, uint8 vLevel, uint8 Local_vLevel)
{
	check(LoadRequestIndex < NumLoadRequests);
	const FMappingRequest Request(LoadRequestIndex, LocalLayerIndex, SpaceID, LayerIndex, vAddress, vLevel, Local_vLevel);
	const uint16 Hash = MurmurFinalize64(Request.PackedValue);

	for (uint16 Index = MappingRequestHash.First(Hash); MappingRequestHash.IsValid(Index); Index = MappingRequestHash.Next(Index))
	{
		if (Request == MappingRequests[Index])
		{
			return;
		}
	}

	if (ensure(NumMappingRequests < MappingRequestCapacity))
	{
		const uint32 Index = NumMappingRequests++;
		MappingRequestHash.Add(Hash, Index);
		MappingRequests[Index] = Request;
	}
}

inline void FUniqueRequestList::AddDirectMappingRequest(uint8 InSpaceID, uint16 InPhysicalSpaceID, uint8 InLayer, uint8 InLogSize, uint32 InAddress, uint8 InLevel, uint16 InPhysicalAddress)
{
	const FDirectMappingRequest Request(InSpaceID, InPhysicalSpaceID, InLayer, InLogSize, InAddress, InLevel, InPhysicalAddress);
	AddDirectMappingRequest(Request);
}

inline void FUniqueRequestList::AddDirectMappingRequest(const FDirectMappingRequest& Request)
{
	const uint16 Hash = MurmurFinalize64(Request.PackedValue);
	for (uint16 Index = DirectMappingRequestHash.First(Hash); DirectMappingRequestHash.IsValid(Index); Index = DirectMappingRequestHash.Next(Index))
	{
		if (Request == DirectMappingRequests[Index])
		{
			return;
		}
	}

	if (ensure(NumDirectMappingRequests < DirectMappingRequestCapacity))
	{
		const uint32 Index = NumDirectMappingRequests++;
		DirectMappingRequestHash.Add(Hash, Index);
		DirectMappingRequests[Index] = Request;
	}
}

inline void FUniqueRequestList::MergeRequests(const FUniqueRequestList* RESTRICT Other, FMemStack& MemStack)
{
	FMemMark Mark(MemStack);

	uint16* LoadRequestIndexRemap = new(MemStack) uint16[Other->NumLoadRequests];
	for (uint32 Index = 0u; Index < Other->NumLoadRequests; ++Index)
	{
		if (Other->IsLocked(Index))
		{
			LoadRequestIndexRemap[Index] = LockLoadRequest(Other->GetLoadRequest(Index), Other->LoadRequestLayerMask[Index]);
		}
		else
		{
			LoadRequestIndexRemap[Index] = AddLoadRequest(Other->GetLoadRequest(Index), Other->LoadRequestLayerMask[Index], Other->LoadRequestCount[Index]);
		}
	}

	for (uint32 Index = 0u; Index < Other->NumMappingRequests; ++Index)
	{
		const FMappingRequest& Request = Other->GetMappingRequest(Index);
		check(Request.LoadRequestIndex < Other->NumLoadRequests);
		const uint16 LoadRequestIndex = LoadRequestIndexRemap[Request.LoadRequestIndex];
		if (LoadRequestIndex != 0xffff)
		{
			AddMappingRequest(LoadRequestIndex, Request.LocalLayerIndex, Request.SpaceID, Request.LayerIndex, Request.vAddress, Request.vLevel, Request.Local_vLevel);
		}
	}

	for (uint32 Index = 0u; Index < Other->NumDirectMappingRequests; ++Index)
	{
		AddDirectMappingRequest(Other->GetDirectMappingRequest(Index));
	}
}

inline void FUniqueRequestList::SortRequests(FVirtualTextureProducerCollection& Producers, FMemStack& MemStack, uint32 MaxNumRequests)
{
	struct FPriorityAndIndex
	{
		uint32 Priroity;
		uint16 Index;

		// sort from largest to smallest
		inline bool operator<(const FPriorityAndIndex& Rhs) const{ return Priroity > Rhs.Priroity; }
	};

	FMemMark Mark(MemStack);

	// Compute priority of each load request
	uint32 CheckNumLockRequests = 0u;
	FPriorityAndIndex* SortedKeys = new(MemStack) FPriorityAndIndex[NumLoadRequests];
	for (uint32 i = 0u; i < NumLoadRequests; ++i)
	{
		const uint32 Count = LoadRequestCount[i];
		SortedKeys[i].Index = i;
		if (Count == 0xffff)
		{
			// Lock request, use max priority
			SortedKeys[i].Priroity = 0xffffffff;
			++CheckNumLockRequests;
		}
		else
		{
			// Try to load higher mips first
			const FVirtualTextureLocalTile& TileToLoad = GetLoadRequest(i);
			const uint32 Priority = Count * (1u + TileToLoad.Local_vLevel);
			SortedKeys[i].Priroity = Priority;
		}
	}
	checkSlow(CheckNumLockRequests == NumLockRequests);

	// Sort so highest priority requests are at the front of the list
	::Sort(SortedKeys, NumLoadRequests);

	// Clamp number of load requests to maximum, but also ensure all lock requests are considered
	const uint32 NewNumLoadRequests = FMath::Min(NumLoadRequests, FMath::Max(NumLockRequests, MaxNumRequests));

	// Re-index load request list, using sorted indices
	FVirtualTextureLocalTile* SortedLoadRequests = new(MemStack) FVirtualTextureLocalTile[NewNumLoadRequests];
	uint8* SortedLayerMask = new(MemStack) uint8[NewNumLoadRequests];
	uint16* LoadIndexToSortedLoadIndex = new(MemStack, MEM_Oned) uint16[NumLoadRequests];
	for (uint32 i = 0u; i < NewNumLoadRequests; ++i)
	{
		const uint32 SortedIndex = SortedKeys[i].Index;
		SortedLoadRequests[i] = LoadRequests[SortedIndex];
		SortedLayerMask[i] = LoadRequestLayerMask[SortedIndex];
		checkSlow(SortedIndex < NumLoadRequests);
		LoadIndexToSortedLoadIndex[SortedIndex] = i;
	}
	FMemory::Memcpy(LoadRequests, SortedLoadRequests, sizeof(FVirtualTextureLocalTile) * NewNumLoadRequests);
	FMemory::Memcpy(LoadRequestLayerMask, SortedLayerMask, sizeof(uint8) * NewNumLoadRequests);

	// Remap LoadRequest indices for all the mapping requests
	// Can discard any mapping request that refers to a LoadRequest that's no longer being performed this frame
	uint32 NewNumMappingRequests = 0u;
	for (uint32 i = 0u; i < NumMappingRequests; ++i)
	{
		FMappingRequest Request = GetMappingRequest(i);
		checkSlow(Request.LoadRequestIndex < NumLoadRequests);
		const uint16 SortedLoadIndex = LoadIndexToSortedLoadIndex[Request.LoadRequestIndex];
		if (SortedLoadIndex != 0xffff)
		{
			check(SortedLoadIndex < NewNumLoadRequests);
			Request.LoadRequestIndex = SortedLoadIndex;
			MappingRequests[NewNumMappingRequests++] = Request;
		}
	}

	NumLoadRequests = NewNumLoadRequests;
	NumMappingRequests = NewNumMappingRequests;
}

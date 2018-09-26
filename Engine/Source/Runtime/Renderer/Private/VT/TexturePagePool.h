// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "Containers/BinaryHeap.h"
#include "Containers/HashTable.h"

// 16m x 16m virtual pages
// 256 x 256 physical pages
struct FTexturePage
{
	// Address is Morton order, relative to mip 0
	uint64	vAddress;
	uint16	pAddress;
	uint8	vLevel;
	uint8	ID;
};

/**
 * Some terminology as it deviates a bit from standard programmer speak:
 * - Allocate -> Allocate a page-sized physical address space
 * - Free -> Flag the page as available for eviciton, it it still calid to do things with a freed page. Eg.g call update on it. A freed page may also be returned by the "Find" functions.
 * - Update -> Update the usage (of a freed page) so the eviction system can make a more informed choice of what to evict.
 * - Evict -> Actually remove the page from memory, after an evict the page is really gone and the address should never be touched again untill Allocate returns it anew..
 * - Map/Unmap -> Map an allocated page to a certain virtual address
 * - Find -> Lookup in the mapped pages to translate a virtual page to a resident page address.
 */
class FTexturePagePool
{
public:
				FTexturePagePool( uint32 Size, uint32 Dimensions );
				~FTexturePagePool();

	/**
	 * Reset the page pool. This can be used to flush any caches. Mainly useful for debug and testing purposes.
	 */
	void EvictAllPages();

	/**
	* Evict all pages for the given id.
	* This may be needed when pages have actually externally changed (e.g. texture content was updated in the editor) and
	* we want them to be force-reloaded.
	*/
	void EvictPages(uint8 ID);

	uint32				GetSize() const						{ return Pages.Num(); }
	const FTexturePage&	GetPage( uint16 pAddress ) const	{ return Pages[ pAddress ]; }
				
	/**
	 * Check if there are any free pages available at the moment.
	 */
	bool		AnyFreeAvailable( uint32 Frame ) const;
	
	/**
	 * Allocate a physical address. Note the returned address may still be mapped to a virtual address
	 * it is the responsibility of the caller to call Unmap on this page.
	 * ?! Why not automatically unmap it before we return it ?!
	 * 
	 * The returned address is pinned in memory and will not be available for re-use until free is called on it.
	 */
	uint32		Alloc( uint32 Frame );

	/**
	 * Mark a physical address as free. This allows it to be evicted when new allocations are needed but it will
	 * not remove the page from memory.
	 */
	void		Free( uint32 Frame, uint32 PageIndex );

	/**
	 * Update a physical address marked as free as recently used. Making it less likely to be evicted soon.
	 */
	void		UpdateUsage( uint32 Frame, uint32 PageIndex );

	/**
	 * Find the physical address for the given virtual address.
	 * Returns ~0u if not found.
	 */
	uint32		FindPage( uint8 ID, uint8 vLevel, uint64 vAddress ) const;

	/**
	* Find the best matching physical address along the mipmap fall back chain for the given virtual address.
	* Returns ~0u if none found at all.
	*/
	uint32		FindNearestPage( uint8 ID, uint8 vLevel, uint64 vAddress ) const;

	/**
	 * Unmap the physical address from any virtual address it was mapped to before.
	 */
	void		UnmapPage( uint16 pAddress );

	/**
	* Map the physical address to a specific virtual address.
	*/
	void		MapPage( uint8 ID, uint8 vLevel, uint64 vAddress, uint16 pAddress );

	void		RefreshEntirePageTable( uint8 ID, TArray< FPageTableUpdate >* Output );
	void		ExpandPageTableUpdatePainters( uint8 ID, FPageUpdate Update, TArray< FPageTableUpdate >* Output );
	void		ExpandPageTableUpdateMasked( uint8 ID, FPageUpdate Update, TArray< FPageTableUpdate >* Output );

private:
	void		BuildSortedKeys();
	uint32		LowerBound( uint32 Min, uint32 Max, uint64 SearchKey, uint64 Mask ) const;
	uint32		UpperBound( uint32 Min, uint32 Max, uint64 SearchKey, uint64 Mask ) const;
	uint32		EqualRange( uint32 Min, uint32 Max, uint64 SearchKey, uint64 Mask ) const;

	uint32		vDimensions;

	TArray< FTexturePage >	Pages;
	
	FHashTable						HashTable;
	FBinaryHeap< uint32, uint16 >	FreeHeap;

	TArray< uint64 >	UnsortedKeys;
	TArray< uint16 >	UnsortedIndexes;
	TArray< uint64 >	SortedKeys;
	TArray< uint16 >	SortedIndexes;
	bool				SortedKeysDirty;
	
	TArray< uint32 >	SortedSubIndexes;
	TArray< uint32 >	SortedAddIndexes;
};

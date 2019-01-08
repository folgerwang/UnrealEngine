// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"

/**
 * Simple ResisableCircularQueue for trivially copyable types
 * Relies on unsigned arithmetics and ever increasing head and tail indices to avoid having to store 
 * an extra element or maintain explicit empty state.
 * Capacity most be power of two and it supports growing
 */
class FDefaultAllocator;

template<typename T, typename AllocatorT = FDefaultAllocator>
class TResizableCircularQueue
{
public:
	typedef T ElementT;

	/** Construct Empty Queue with the given initial Capacity, Capacity must be a power of two since we rely on unsigned arithmetics for wraparound */
	explicit TResizableCircularQueue(SIZE_T InitialCapacity);

	/** Returns true if the queue is empty */
	bool IsEmpty() const { return Head == Tail; }

	/** Gets the number of elements in the queue. */
	SIZE_T Count() const { return Head - Tail; }

	/** Current allocated Capacity */
	SIZE_T AllocatedCapacity() const { return Storage.Num(); }

	/** Push single element to the back of the Queue */
	void Enqueue(const ElementT& SrcData);

	/** Pop elements from the front of the queue */
	void Pop();

	/** Pop Count elements from the front of the queue */
	void Pop(SIZE_T Count);

	/**  Unchecked version, Pop a single element from the front of the queue */
	void PopNoCheck() { ++Tail; }

	/** Unchecked version, Pop Count elements from the front of the queue */
	void PopNoCheck(SIZE_T Count) { Tail += Count; }

	/** Peek with the given offset from the front of the queue */
	const ElementT& PeekAtOffset(SIZE_T Offset = 0) const { check(Offset < Count()); return PeekAtOffsetNoCheck(Offset); }

	/** Peek at front element */
	const ElementT& Peek() const { return PeekAtOffset(0); }

	/**  Unchecked version, Peek with the given offset from the front of the queue */
	const ElementT& PeekAtOffsetNoCheck(SIZE_T Offset = 0) const { return Storage.GetData()[(Tail + Offset) & IndexMask]; }

	/** Peek at front element with no check */
	const ElementT& PeekNoCheck() const { return PeekAtOffsetNoCheck(0); }

	/**  Trim memory usage to the next power of two for the current size */
	void Trim();

	/** Empty queue without releasing memory */
	void Reset() { Head = Tail = 0u; }

	/** Empty queue and release memory */
	void Empty() { Reset(); Storage.Empty(); IndexMask = IndexT(-1); }
	
private:

#if WITH_DEV_AUTOMATION_TESTS
	friend struct FResizableCircularQueueTestUtil;
#endif

	// Resize buffer maintaining validity of stored data.
	void SetCapacity(SIZE_T NewCapacity);

	// TIsTriviallyCopyable (a.k.a. std::is_trivially_copyable) should be used but
	// TIsTriviallyCopyable is not provided by the core module at the moment. Core's
	// implementation of TIsTrivial is actually equivalent to std::is_trivially_copyable
	// since std::is_trivial<T> requires T to have a trivial constructor but TIsTrivial
	// doesn't
	static_assert(TIsTrivial<ElementT>::Value, "ResizableCircularQueue only supports trivially copyable elements");

	typedef uint32 IndexT;
	typedef TArray<ElementT, AllocatorT> StorageT;

	IndexT Head;
	IndexT Tail;

	IndexT IndexMask;
	StorageT Storage;
};


template<typename T, typename AllocatorT>
TResizableCircularQueue<T, AllocatorT>::TResizableCircularQueue(SIZE_T InitialCapacity)
: Head(0u)
, Tail(0u)
, IndexMask(0u)
{
	// Capacity should be power of two, it will be rounded up but lets warn the user anyway in debug builds.
	checkSlow((InitialCapacity & (InitialCapacity - 1)) == 0);
	if (InitialCapacity > 0)
	{
		SetCapacity(InitialCapacity);
	}
}

template<typename T, typename AllocatorT>
void TResizableCircularQueue<T, AllocatorT>::Enqueue(const ElementT& SrcData)
{ 
	const SIZE_T RequiredCapacity = Count() + 1;
	if (RequiredCapacity > AllocatedCapacity())
	{
		// Capacity must be power of two
		SetCapacity(RequiredCapacity);
	}

	T* DstData = Storage.GetData();
	const IndexT MaskedIndex = Head & IndexMask;
	DstData[MaskedIndex] = SrcData;
	++Head;		
}

template<typename T, typename AllocatorT>
void
TResizableCircularQueue<T, AllocatorT>::Pop(SIZE_T PopCount)
{
	if (ensure(Count() > PopCount))
	{
		PopNoCheck(PopCount);
	}
}

template<typename T, typename AllocatorT>
void TResizableCircularQueue<T, AllocatorT>::Pop()
{
	if (ensure(Count() > 0))
	{
		PopNoCheck();
	}
}

template<typename T, typename AllocatorT>
void TResizableCircularQueue<T, AllocatorT>::SetCapacity(SIZE_T RequiredCapacity)
{
	SIZE_T NewCapacity = FMath::RoundUpToPowerOfTwo(RequiredCapacity);

	if ((NewCapacity == Storage.Num()) || (NewCapacity < Count()))
	{
		return;
	}

	if (Storage.Num() > 0)
	{
		StorageT NewStorage;
		NewStorage.AddUninitialized(NewCapacity);
		
		// copy data to new storage
		const IndexT MaskedTail = Tail & IndexMask;
		const IndexT MaskedHead = Head & IndexMask;

		const ElementT* SrcData = Storage.GetData();
		const SIZE_T SrcCapacity = Storage.Num();
		const SIZE_T SrcSize = Count();
		ElementT* DstData = NewStorage.GetData();

		if (MaskedTail >= MaskedHead)
		{
			const SIZE_T CopyCount = (SrcCapacity - MaskedTail);
			FPlatformMemory::Memcpy(DstData, SrcData + MaskedTail, CopyCount * sizeof(ElementT));
			FPlatformMemory::Memcpy(DstData + CopyCount, SrcData, MaskedHead * sizeof(ElementT));
		}
		else
		{
			FPlatformMemory::Memcpy(DstData, SrcData + MaskedTail, SrcSize * sizeof(ElementT));
		}

		this->Storage = MoveTemp(NewStorage);
		IndexMask = NewCapacity - 1;
		Tail = 0u;
		Head = SrcSize;
	}
	else
	{
		IndexMask = NewCapacity - 1;
		Storage.AddUninitialized(NewCapacity);
	}
}

template<typename T, typename AllocatorT>
void TResizableCircularQueue<T, AllocatorT>::Trim()
{
	if (IsEmpty())
	{
		Empty();
	}
	else
	{
		SetCapacity(Count());
	}
}



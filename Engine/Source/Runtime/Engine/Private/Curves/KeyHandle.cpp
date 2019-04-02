// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Curves/KeyHandle.h"


FKeyHandle::FKeyHandle()
{
	static uint32 LastKeyHandleIndex = 1;
	Index = ++LastKeyHandleIndex;

	check(LastKeyHandleIndex != 0); // check in the unlikely event that this overflows
}

FKeyHandle::FKeyHandle(uint32 SpecificIndex)
	: Index(SpecificIndex)
{}

FKeyHandle FKeyHandle::Invalid()
{
	return FKeyHandle(0);
}

/* FKeyHandleMap interface
 *****************************************************************************/

void FKeyHandleMap::Add( const FKeyHandle& InHandle, int32 InIndex )
{
	for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
	{
		int32& KeyIndex = It.Value();
		if (KeyIndex >= InIndex) { ++KeyIndex; }
	}

	if (InIndex > KeyHandles.Num())
	{
		KeyHandles.Reserve(InIndex+1);
		for(int32 NewIndex = KeyHandles.Num(); NewIndex < InIndex; ++NewIndex)
		{
			KeyHandles.AddDefaulted();
			KeyHandlesToIndices.Add(KeyHandles.Last(), NewIndex);
		}
		KeyHandles.Add(InHandle);
	}
	else
	{
		KeyHandles.Insert(InHandle, InIndex);
	}

	KeyHandlesToIndices.Add(InHandle, InIndex);
}


void FKeyHandleMap::Empty()
{
	KeyHandlesToIndices.Empty();
	KeyHandles.Empty();
}


void FKeyHandleMap::Remove( const FKeyHandle& InHandle )
{
	int32 Index = INDEX_NONE;
	if (KeyHandlesToIndices.RemoveAndCopyValue(InHandle, Index))
	{
		// update key indices
		for (auto It = KeyHandlesToIndices.CreateIterator(); It; ++It)
		{
			int32& KeyIndex = It.Value();
			if (KeyIndex >= Index) { --KeyIndex; }
		}

		KeyHandles.RemoveAt(Index);
	}
}

const FKeyHandle* FKeyHandleMap::FindKey( int32 KeyIndex ) const
{
	if (KeyIndex >= 0 && KeyIndex < KeyHandles.Num())
	{
		return &KeyHandles[KeyIndex];
	}
	return nullptr;
}

bool FKeyHandleMap::Serialize(FArchive& Ar)
{
	// only allow this map to be saved to the transaction buffer
	if( Ar.IsTransacting() )
	{
		Ar << KeyHandlesToIndices;
		Ar << KeyHandles;
	}

	return true;
}

void FKeyHandleMap::EnsureAllIndicesHaveHandles(int32 NumIndices)
{
	if (KeyHandles.Num() > NumIndices)
	{
		for (int32 Index = NumIndices; Index < KeyHandles.Num(); ++Index)
		{
			KeyHandlesToIndices.Remove(KeyHandles[Index]);
		}

		KeyHandles.SetNum(NumIndices);
	}
	else if (KeyHandles.Num() < NumIndices)
	{
		KeyHandles.Reserve(NumIndices);
		for (int32 NewIndex = KeyHandles.Num(); NewIndex < NumIndices; ++NewIndex)
		{
			KeyHandles.AddDefaulted();
			KeyHandlesToIndices.Add(KeyHandles.Last(), NewIndex);
		}
	}
}

void FKeyHandleMap::EnsureIndexHasAHandle(int32 KeyIndex)
{
	const FKeyHandle* KeyHandle = FindKey(KeyIndex);
	if (!KeyHandle)
	{
		Add(FKeyHandle(), KeyIndex);
	}
}

int32 FKeyHandleLookupTable::GetIndex(FKeyHandle KeyHandle)
{
	const int32* Index = KeyHandlesToIndices.Find(KeyHandle);
	if (!Index)
	{
		// If it's not even in the map, there's no way this could be a valid handle for this container
		return INDEX_NONE;
	}
	else if (KeyHandles.IsValidIndex(*Index) && KeyHandles[*Index] == KeyHandle)
	{
		return *Index;
	}

	// slow lookup and cache
	const int32 NewCacheIndex = KeyHandles.IndexOfByPredicate(
		[KeyHandle](const TOptional<FKeyHandle>& PredKeyHandle)
		{
			return PredKeyHandle.IsSet() && PredKeyHandle.GetValue() == KeyHandle;
		}
	);

	if (NewCacheIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	KeyHandlesToIndices.Add(KeyHandle, NewCacheIndex);
	return NewCacheIndex;
}

FKeyHandle FKeyHandleLookupTable::FindOrAddKeyHandle(int32 Index)
{
	if (KeyHandles.IsValidIndex(Index) && KeyHandles[Index].IsSet())
	{
		return KeyHandles[Index].GetValue();
	}
	
	int32 NumToAdd = Index + 1 - KeyHandles.Num();
	if (NumToAdd > 0)
	{
		KeyHandles.AddDefaulted(NumToAdd);
	}

	// Allocate a new key handle
	FKeyHandle NewKeyHandle;

	KeyHandles[Index] = NewKeyHandle;
	KeyHandlesToIndices.Add(NewKeyHandle, Index);

	return NewKeyHandle;
}

void FKeyHandleLookupTable::MoveHandle(int32 OldIndex, int32 NewIndex)
{
	if (KeyHandles.IsValidIndex(OldIndex))
	{
		TOptional<FKeyHandle> Handle = KeyHandles[OldIndex];

		KeyHandles.RemoveAt(OldIndex, 1, false);
		KeyHandles.Insert(Handle, NewIndex);
		if (Handle.IsSet())
		{
			KeyHandlesToIndices.Add(Handle.GetValue(), NewIndex);
		}
	}
}

FKeyHandle FKeyHandleLookupTable::AllocateHandle(int32 Index)
{
	FKeyHandle NewKeyHandle;

	int32 NumToAdd = Index + 1 - KeyHandles.Num();
	if (NumToAdd > 0)
	{
		KeyHandles.AddDefaulted(NumToAdd);
	}

	KeyHandles.Insert(NewKeyHandle, Index);
	KeyHandlesToIndices.Add(NewKeyHandle, Index);
	return NewKeyHandle;
}

void FKeyHandleLookupTable::DeallocateHandle(int32 Index)
{
	TOptional<FKeyHandle> KeyHandle = KeyHandles[Index];
	KeyHandles.RemoveAt(Index, 1, false);
	if (KeyHandle.IsSet())
	{
		KeyHandlesToIndices.Remove(KeyHandle.GetValue());
	}
}

void FKeyHandleLookupTable::Reset()
{
	KeyHandles.Reset();
	KeyHandlesToIndices.Reset();
}

bool FKeyHandleLookupTable::Serialize(FArchive& Ar)
{
	// We're only concerned with Undo/Redo transactions
	if (Ar.IsTransacting())
	{
		Ar << KeyHandles;
		Ar << KeyHandlesToIndices;
	}

	return true;
}
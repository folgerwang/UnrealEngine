// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveUObjectFromStructuredArchive.h"

FArchiveUObjectFromStructuredArchive::FArchiveUObjectFromStructuredArchive(FStructuredArchive::FSlot Slot)
	: FArchiveFromStructuredArchive(Slot)
	, bPendingSerialize(true)
{
}

FArchiveUObjectFromStructuredArchive::~FArchiveUObjectFromStructuredArchive()
{
	Commit();
}

FArchive& FArchiveUObjectFromStructuredArchive::operator<<(FLazyObjectPtr& Value)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			int32 ObjectIdx = 0;
			Serialize(&ObjectIdx, sizeof(ObjectIdx));
			Value = LazyObjectPtrs[ObjectIdx];
		}
		else
		{
			int32* IdxPtr = LazyObjectPtrToIndex.Find(Value);
			if (IdxPtr == nullptr)
			{
				IdxPtr = &(LazyObjectPtrToIndex.Add(Value));
				*IdxPtr = LazyObjectPtrs.Add(Value);
			}
			Serialize(IdxPtr, sizeof(*IdxPtr));
		}
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

FArchive& FArchiveUObjectFromStructuredArchive::operator<<(FSoftObjectPtr& Value)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			int32 ObjectIdx = 0;
			Serialize(&ObjectIdx, sizeof(ObjectIdx));
			Value = SoftObjectPtrs[ObjectIdx];
		}
		else
		{
			int32* IdxPtr = SoftObjectPtrToIndex.Find(Value);
			if (IdxPtr == nullptr)
			{
				IdxPtr = &(SoftObjectPtrToIndex.Add(Value));
				*IdxPtr = SoftObjectPtrs.Add(Value);
			}
			Serialize(IdxPtr, sizeof(*IdxPtr));
		}
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

FArchive& FArchiveUObjectFromStructuredArchive::operator<<(FSoftObjectPath& Value)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			int32 ObjectIdx = 0;
			Serialize(&ObjectIdx, sizeof(ObjectIdx));
			Value = SoftObjectPaths[ObjectIdx];
		}
		else
		{
			int32* IdxPtr = SoftObjectPathToIndex.Find(Value);
			if (IdxPtr == nullptr)
			{
				IdxPtr = &(SoftObjectPathToIndex.Add(Value));
				*IdxPtr = SoftObjectPaths.Add(Value);
			}

#if 1
			// Temp workaround for emulating the behaviour of soft asset path serialization. Use these thread specific overrides to determine if we should actually save
			// a reference to the path. If not, we still record the path in our list so that we get the correct behaviour later on when we pass these references down to the underlying
			// archive
			FName PackageName, PropertyName;
			ESoftObjectPathCollectType CollectType = ESoftObjectPathCollectType::AlwaysCollect;
			ESoftObjectPathSerializeType SerializeType = ESoftObjectPathSerializeType::AlwaysSerialize;
			FSoftObjectPathThreadContext& ThreadContext = FSoftObjectPathThreadContext::Get();
			ThreadContext.GetSerializationOptions(PackageName, PropertyName, CollectType, SerializeType);

			if (SerializeType == ESoftObjectPathSerializeType::AlwaysSerialize)
			{
				Serialize(IdxPtr, sizeof(*IdxPtr));
			}
#else
			Serialize(IdxPtr, sizeof(*IdxPtr));
#endif
		}
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

FArchive& FArchiveUObjectFromStructuredArchive::operator<<(FWeakObjectPtr& Value)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			int32 ObjectIdx = 0;
			Serialize(&ObjectIdx, sizeof(ObjectIdx));
			Value = WeakObjectPtrs[ObjectIdx];
		}
		else
		{
			int32* IdxPtr = WeakObjectPtrToIndex.Find(Value);
			if (IdxPtr == nullptr)
			{
				IdxPtr = &(WeakObjectPtrToIndex.Add(Value));
				*IdxPtr = WeakObjectPtrs.Add(Value);
			}
			Serialize(IdxPtr, sizeof(*IdxPtr));
		}
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

void FArchiveUObjectFromStructuredArchive::SerializeInternal(FStructuredArchive::FRecord Record)
{
	FArchiveFromStructuredArchive::SerializeInternal(Record);

	if (bPendingSerialize)
	{
		TOptional<FStructuredArchive::FSlot> LazyObjectPtrsSlot = Record.TryEnterField(FIELD_NAME_TEXT("LazyObjectPtrs"), LazyObjectPtrs.Num() > 0);
		if (LazyObjectPtrsSlot.IsSet())
		{
			LazyObjectPtrsSlot.GetValue() << LazyObjectPtrs;
		}

		TOptional<FStructuredArchive::FSlot> SoftObjectPtrsSlot = Record.TryEnterField(FIELD_NAME_TEXT("SoftObjectPtrs"), SoftObjectPtrs.Num() > 0);
		if (SoftObjectPtrsSlot.IsSet())
		{
			SoftObjectPtrsSlot.GetValue() << SoftObjectPtrs;
		}

		TOptional<FStructuredArchive::FSlot> SoftObjectPathsSlot = Record.TryEnterField(FIELD_NAME_TEXT("SoftObjectPaths"), SoftObjectPaths.Num() > 0);
		if (SoftObjectPathsSlot.IsSet())
		{
			SoftObjectPathsSlot.GetValue() << SoftObjectPaths;
		}

		TOptional<FStructuredArchive::FSlot> WeakObjectPtrsSlot = Record.TryEnterField(FIELD_NAME_TEXT("WeakObjectPtrs"), WeakObjectPtrs.Num() > 0);
		if (WeakObjectPtrsSlot.IsSet())
		{
			WeakObjectPtrsSlot.GetValue() << WeakObjectPtrs;
		}

		bPendingSerialize = true;
	}
}
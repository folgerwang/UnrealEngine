// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/ArchiveFromStructuredArchive.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/ObjectResource.h"
#include "UObject/WeakObjectPtr.h"

class FArchiveUObjectFromStructuredArchive : public FArchiveFromStructuredArchive
{
public:

	FArchiveUObjectFromStructuredArchive(FStructuredArchive::FSlot Slot)
		: FArchiveFromStructuredArchive(Slot)
		, ExternalObjectIndicesMap(nullptr)
	{

	}

	void SetExternalObjectIndicesMap(const TMap<UObject*, FPackageIndex>* InObjectIndicesMap)
	{
		ExternalObjectIndicesMap = InObjectIndicesMap;
	}

	using FArchive::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override
	{
		if (InnerArchive.IsTextFormat())
		{
			FArchiveUObject::SerializeLazyObjectPtr(*this, Value);
		}
		else
		{
			InnerArchive << Value;
		}
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPtr& Value) override
	{
		if (InnerArchive.IsTextFormat())
		{
			FArchiveUObject::SerializeSoftObjectPtr(*this, Value);
		}
		else
		{
			InnerArchive << Value;
		}
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPath& Value) override
	{
		if (InnerArchive.IsTextFormat())
		{
			FArchiveUObject::SerializeSoftObjectPath(*this, Value);
		}
		else
		{
			InnerArchive << Value;
		}
		return *this;
	}

	virtual FArchive& operator<<(FWeakObjectPtr& Value) override
	{
		if (InnerArchive.IsTextFormat())
		{
			UObject* Object = Value.IsValid() ? Value.Get() : nullptr;

			if (!IsObjectAllowed(Object))
			{
				Object = nullptr;
			}

			*this << Object;

			if (IsLoading())
			{
				Value = Object;
			}
		}
		else
		{
			InnerArchive << Value;
		}
		return *this;
	}
	//~ End FArchive Interface

private:

	const TMap<UObject *, FPackageIndex>* ExternalObjectIndicesMap;

	bool IsObjectAllowed(UObject* InObject) const
	{
		return IsLoading() || (ExternalObjectIndicesMap == nullptr) || (ExternalObjectIndicesMap->Contains(InObject));
	}
};
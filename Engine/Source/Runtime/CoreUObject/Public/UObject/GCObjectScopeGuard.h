// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"

/**
 * Specific implementation of FGCObject that prevents a single UObject-based pointer from being GC'd while this guard is in scope.
 * @note This is the lean version of TStrongObjectPtr which uses an inline FGCObject so *can't* safely be used with containers that treat types as trivially relocatable.
 */
class FGCObjectScopeGuard : public FGCObject
{
public:
	explicit FGCObjectScopeGuard(const UObject* InObject)
		: Object(InObject)
	{
	}

	virtual ~FGCObjectScopeGuard()
	{
	}

	/** Non-copyable */
	FGCObjectScopeGuard(const FGCObjectScopeGuard&) = delete;
	FGCObjectScopeGuard& operator=(const FGCObjectScopeGuard&) = delete;

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Object);
	}

private:
	const UObject* Object;
};

/**
 * Specific implementation of FGCObject that prevents an array of UObject-based pointers from being GC'd while this guard is in scope.
 */
template <typename ObjectType>
class TGCObjectsScopeGuard : public FGCObject
{
	static_assert(TPointerIsConvertibleFromTo<ObjectType, const UObjectBase>::Value, "TGCObjectsScopeGuard: ObjectType must be pointers to a type derived from UObject");

public:
	explicit TGCObjectsScopeGuard(const TArray<ObjectType*>& InObjects)
		: Objects(InObjects)
	{
	}

	virtual ~TGCObjectsScopeGuard()
	{
	}

	/** Non-copyable */
	TGCObjectsScopeGuard(const TGCObjectsScopeGuard&) = delete;
	TGCObjectsScopeGuard& operator=(const TGCObjectsScopeGuard&) = delete;

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(Objects);
	}

private:
	TArray<ObjectType*> Objects;
};

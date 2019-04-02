// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "Templates/UniquePtr.h"

/**
 * Specific implementation of FGCObject that prevents a single UObject-based pointer from being GC'd while this guard is in scope.
 * @note This is the "full-fat" version of FGCObjectScopeGuard which uses a heap-allocated FGCObject so *can* safely be used with containers that treat types as trivially relocatable.
 */
template <typename ObjectType>
class TStrongObjectPtr
{
public:
	FORCEINLINE_DEBUGGABLE TStrongObjectPtr()
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(nullptr))
	{
		static_assert(TPointerIsConvertibleFromTo<ObjectType, const volatile UObject>::Value, "TStrongObjectPtr can only be constructed with UObject types");
	}

	FORCEINLINE_DEBUGGABLE explicit TStrongObjectPtr(ObjectType* InObject)
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(InObject))
	{
		static_assert(TPointerIsConvertibleFromTo<ObjectType, const volatile UObject>::Value, "TStrongObjectPtr can only be constructed with UObject types");
	}

	FORCEINLINE_DEBUGGABLE TStrongObjectPtr(const TStrongObjectPtr& InOther)
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(InOther.Get()))
	{
	}

	template <typename OtherObjectType>
	FORCEINLINE_DEBUGGABLE TStrongObjectPtr(const TStrongObjectPtr<OtherObjectType>& InOther)
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(InOther.Get()))
	{
	}

	FORCEINLINE_DEBUGGABLE TStrongObjectPtr& operator=(const TStrongObjectPtr& InOther)
	{
		ReferenceCollector->Set(InOther.Get());
		return *this;
	}

	template <typename OtherObjectType>
	FORCEINLINE_DEBUGGABLE TStrongObjectPtr& operator=(const TStrongObjectPtr<OtherObjectType>& InOther)
	{
		ReferenceCollector->Set(InOther.Get());
		return *this;
	}

	FORCEINLINE_DEBUGGABLE TStrongObjectPtr(TStrongObjectPtr&& InOther)
	{
		ReferenceCollector = MoveTemp(InOther.ReferenceCollector);
	}

	template <typename OtherObjectType>
	FORCEINLINE_DEBUGGABLE TStrongObjectPtr(TStrongObjectPtr<OtherObjectType>&& InOther)
	{
		ReferenceCollector = MoveTemp(InOther.ReferenceCollector);
	}

	FORCEINLINE_DEBUGGABLE TStrongObjectPtr& operator=(TStrongObjectPtr&& InOther)
	{
		ReferenceCollector = MoveTemp(InOther.ReferenceCollector);
		return *this;
	}

	template <typename OtherObjectType>
	FORCEINLINE_DEBUGGABLE TStrongObjectPtr<OtherObjectType>& operator=(TStrongObjectPtr<OtherObjectType>&& InOther)
	{
		ReferenceCollector = MoveTemp(InOther.ReferenceCollector);
		return *this;
	}

	FORCEINLINE_DEBUGGABLE ObjectType& operator*() const
	{
		check(IsValid());
		return *Get();
	}

	FORCEINLINE_DEBUGGABLE ObjectType* operator->() const
	{
		check(IsValid());
		return Get();
	}

	FORCEINLINE_DEBUGGABLE bool IsValid() const
	{
		return Get() != nullptr;
	}

	FORCEINLINE_DEBUGGABLE explicit operator bool() const
	{
		return Get() != nullptr;
	}

	FORCEINLINE_DEBUGGABLE ObjectType* Get() const
	{
		return ReferenceCollector->Get();
	}

	FORCEINLINE_DEBUGGABLE void Reset(ObjectType* InNewObject = nullptr)
	{
		ReferenceCollector->Set(InNewObject);
	}

	FORCEINLINE_DEBUGGABLE friend uint32 GetTypeHash(const TStrongObjectPtr& InStrongObjectPtr)
	{
		return GetTypeHash(InStrongObjectPtr.Get());
	}

private:
	class FInternalReferenceCollector : public FGCObject
	{
	public:
		FInternalReferenceCollector(ObjectType* InObject)
			: Object(InObject)
		{
			check(IsInGameThread());
		}

		virtual ~FInternalReferenceCollector()
		{
			check(IsInGameThread());
		}

		FORCEINLINE ObjectType* Get() const
		{
			return Object;
		}

		FORCEINLINE void Set(ObjectType* InObject)
		{
			Object = InObject;
		}

		//~ FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Object);
		}

	private:
		ObjectType* Object;
	};

	TUniquePtr<FInternalReferenceCollector> ReferenceCollector;
};

template <typename LHSObjectType, typename RHSObjectType>
FORCEINLINE bool operator==(const TStrongObjectPtr<LHSObjectType>& InLHS, const TStrongObjectPtr<RHSObjectType>& InRHS)
{
	return InLHS.Get() == InRHS.Get();
}

template <typename LHSObjectType, typename RHSObjectType>
FORCEINLINE bool operator!=(const TStrongObjectPtr<LHSObjectType>& InLHS, const TStrongObjectPtr<RHSObjectType>& InRHS)
{
	return InLHS.Get() != InRHS.Get();
}

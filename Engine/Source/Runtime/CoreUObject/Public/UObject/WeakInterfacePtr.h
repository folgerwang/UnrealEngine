// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/Casts.h"
#include "ScriptInterface.h"

template<class TInterface>
struct TWeakInterfacePtr
{
	TWeakInterfacePtr() : InterfaceInstance(nullptr) {}

	TWeakInterfacePtr(const UObject* Object)
	{
		InterfaceInstance = Cast<TInterface>(Object);
		if (InterfaceInstance != nullptr)
		{
			ObjectInstance = Object;
		}
	}

	TWeakInterfacePtr(TInterface& Interace)
	{
		InterfaceInstance = &Interace;
		ObjectInstance = Cast<UObject>(&Interace);
	}

	FORCEINLINE bool IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest = false) const
	{
		return InterfaceInstance != nullptr && ObjectInstance.IsValid(bEvenIfPendingKill, bThreadsafeTest);
	}

	FORCEINLINE bool IsValid() const
	{
		return InterfaceInstance != nullptr && ObjectInstance.IsValid();
	}

	FORCEINLINE bool IsStale(bool bEvenIfPendingKill = false, bool bThreadsafeTest = false) const
	{
		return InterfaceInstance != nullptr && ObjectInstance.IsStale(bEvenIfPendingKill, bThreadsafeTest);
	}

	FORCEINLINE UObject* GetObject() const
	{
		return ObjectInstance.Get();
	}

	FORCEINLINE TInterface& operator*() const
	{
		return *InterfaceInstance;
	}

	FORCEINLINE TInterface* operator->() const
	{
		return InterfaceInstance;
	}

	FORCEINLINE void operator=(const TWeakInterfacePtr<TInterface>& Other)
	{
		ObjectInstance = Other.ObjectInstance;
		InterfaceInstance = Other.InterfaceInstance;
	}

	FORCEINLINE void operator=(const TScriptInterface<TInterface>& Other)
	{
		ObjectInstance = Other.GetObject();
		InterfaceInstance = (TInterface*)Other.GetInterface();
	}

	FORCEINLINE bool operator==(const TWeakInterfacePtr<TInterface>& Other) const
	{
		return InterfaceInstance == Other.InterfaceInstance && ObjectInstance == Other.ObjectInstance;
	}

	FORCEINLINE bool operator!=(const TWeakInterfacePtr<TInterface>& Other) const
	{
		return InterfaceInstance != Other.InterfaceInstance || ObjectInstance != Other.ObjectInstance;
	}

	FORCEINLINE bool operator==(const UObject* Other)
	{
		return Other == ObjectInstance.Get();
	}

	FORCEINLINE TScriptInterface<TInterface> ToScriptInterface()
	{
		UObject* Object = ObjectInstance.Get();
		if (Object)
		{
			return TScriptInterface<TInterface>(Object);
		}

		return TScriptInterface<TInterface>();
	}

private:
	TWeakObjectPtr<UObject> ObjectInstance;
	TInterface* InterfaceInstance;
};

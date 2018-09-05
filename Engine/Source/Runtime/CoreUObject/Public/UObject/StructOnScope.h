// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"

class FStructOnScope
{
protected:
	TWeakObjectPtr<const UStruct> ScriptStruct;
	uint8* SampleStructMemory;
	TWeakObjectPtr<UPackage> Package;
	/** Whether the struct memory is owned by this instance. */
	bool OwnsMemory;

	virtual void Initialize()
	{
		if (const UStruct* ScriptStructPtr = ScriptStruct.Get())
		{
			SampleStructMemory = (uint8*)FMemory::Malloc(ScriptStructPtr->GetStructureSize() ? ScriptStructPtr->GetStructureSize() : 1);
			ScriptStructPtr->InitializeStruct(SampleStructMemory);
			OwnsMemory = true;
		}
	}

public:

	FStructOnScope()
		: SampleStructMemory(nullptr)
		, OwnsMemory(false)
	{
	}

	FStructOnScope(const UStruct* InScriptStruct)
		: ScriptStruct(InScriptStruct)
		, SampleStructMemory(nullptr)
		, OwnsMemory(false)
	{
		Initialize();
	}

	FStructOnScope(const UStruct* InScriptStruct, uint8* InData)
		: ScriptStruct(InScriptStruct)
		, SampleStructMemory(InData)
		, OwnsMemory(false)
	{
	}

	FStructOnScope(FStructOnScope&& InOther)
	{
		ScriptStruct = InOther.ScriptStruct;
		SampleStructMemory = InOther.SampleStructMemory;
		OwnsMemory = InOther.OwnsMemory;

		InOther.OwnsMemory = false;
		InOther.Reset();
	}

	FStructOnScope& operator=(FStructOnScope&& InOther)
	{
		if (this != &InOther)
		{
			Reset();

			ScriptStruct = InOther.ScriptStruct;
			SampleStructMemory = InOther.SampleStructMemory;
			OwnsMemory = InOther.OwnsMemory;

			InOther.OwnsMemory = false;
			InOther.Reset();
		}
		return *this;
	}

	FStructOnScope(const FStructOnScope&) = delete;
	FStructOnScope& operator=(const FStructOnScope&) = delete;

	virtual bool OwnsStructMemory() const
	{
		return OwnsMemory;
	}

	virtual uint8* GetStructMemory()
	{
		return SampleStructMemory;
	}

	virtual const uint8* GetStructMemory() const
	{
		return SampleStructMemory;
	}

	virtual const UStruct* GetStruct() const
	{
		return ScriptStruct.Get();
	}

	virtual UPackage* GetPackage() const
	{
		return Package.Get();
	}

	virtual void SetPackage(UPackage* InPackage)
	{
		Package = InPackage;
	}

	virtual bool IsValid() const
	{
		return ScriptStruct.IsValid() && SampleStructMemory;
	}

	virtual void Destroy()
	{
		if (!OwnsMemory)
		{
			return;
		}

		if (const UStruct* ScriptStructPtr = ScriptStruct.Get())
		{
			if (SampleStructMemory)
			{
				ScriptStructPtr->DestroyStruct(SampleStructMemory);
			}
			ScriptStruct = nullptr;
		}

		if (SampleStructMemory)
		{
			FMemory::Free(SampleStructMemory);
			SampleStructMemory = nullptr;
		}
	}

	virtual void Reset()
	{
		Destroy();

		ScriptStruct = nullptr;
		SampleStructMemory = nullptr;
		OwnsMemory = false;
	}

	virtual ~FStructOnScope()
	{
		Destroy();
	}

	/** Re-initializes the scope with a specified UStruct */
	void Initialize(TWeakObjectPtr<const UStruct> InScriptStruct)
	{
		Destroy();
		ScriptStruct = InScriptStruct;
		Initialize();
	}
};

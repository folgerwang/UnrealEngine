// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/SubsystemCollection.h"

#include "Subsystems/Subsystem.h"
#include "UObject/UObjectHash.h"

FSubsystemCollectionBase::FSubsystemCollectionBase()
	: Outer(nullptr)
	, bPopulating(false)
{
}

FSubsystemCollectionBase::FSubsystemCollectionBase(UObject* InOuter, TSubclassOf<USubsystem> InBaseType)
	: BaseType(InBaseType)
	, Outer(InOuter)
	, bPopulating(false)
{
	check(BaseType);
	check(Outer);
}

USubsystem* FSubsystemCollectionBase::GetSubsystemInternal(TSubclassOf<USubsystem> SubsystemClass) const
{
	USubsystem* SystemPtr = SubsystemMap.FindRef(SubsystemClass);

	if (SystemPtr)
	{
		return SystemPtr;
	}
	else
	{
		const TArray<USubsystem*>& SystemPtrs = GetSubsystemArrayInternal(SubsystemClass);
		if (SystemPtrs.Num() > 0)
		{
			return SystemPtrs[0];
		}
	}

	return nullptr;
}

const TArray<USubsystem*>& FSubsystemCollectionBase::GetSubsystemArrayInternal(TSubclassOf<USubsystem> SubsystemClass) const
{
	if (!SubsystemArrayMap.Contains(SubsystemClass))
	{
		TArray<USubsystem*>& NewList = SubsystemArrayMap.Add(SubsystemClass);

		for (auto Iter = SubsystemMap.CreateConstIterator(); Iter; ++Iter)
		{
			UClass* KeyClass = Iter.Key();
			if (KeyClass->IsChildOf(SubsystemClass))
			{
				NewList.Add(Iter.Value());
			}
		}

		return NewList;
	}

	const TArray<USubsystem*>& List = SubsystemArrayMap.FindChecked(SubsystemClass);
	return List;
}

void FSubsystemCollectionBase::Initialize()
{
	if (ensure(BaseType) && ensureMsgf(SubsystemMap.Num() == 0, TEXT("Currently don't support repopulation of Subsystem Collections.")))
	{
		check(Outer);
		check(!bPopulating); //Populating collections on multiple threads?

		TGuardValue<bool> PopulatingGuard(bPopulating, true);

		TArray<UClass*> SubsystemClasses;
		GetDerivedClasses(BaseType, SubsystemClasses, true);

		for (UClass* SubsystemClass : SubsystemClasses)
		{
			InteralAddSystem(SubsystemClass);
		}
	}
}

void FSubsystemCollectionBase::Deinitialize()
{
	SubsystemArrayMap.Empty();
	for (auto Iter = SubsystemMap.CreateIterator(); Iter; ++Iter)
	{
		UClass* KeyClass = Iter.Key();
		USubsystem* Subsystem = Iter.Value();
		if (Subsystem->GetClass() == KeyClass)
		{
			Subsystem->Deinitialize();
		}
	}
	SubsystemMap.Empty();
}

bool FSubsystemCollectionBase::InitializeDependancy(TSubclassOf<USubsystem> SubsystemClass)
{
	if (ensureMsgf(SubsystemClass, TEXT("Attempting to add invalid subsystem as dependancy."))
		&& ensureMsgf(bPopulating, TEXT("InitializeDependancy() should only be called from System USubsystem::Initialization() implementations."))
		&& ensureMsgf(SubsystemClass->IsChildOf(BaseType), TEXT("ClassType (%s) must be a subclass of BaseType(%s)."), *SubsystemClass->GetName(), *BaseType->GetName()))
	{
		return InteralAddSystem(SubsystemClass);
	}
	return false;
}

void FSubsystemCollectionBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Outer);
	Collector.AddReferencedObjects(SubsystemMap);
}

bool FSubsystemCollectionBase::InteralAddSystem(UClass* SubsystemClass)
{
	if (!SubsystemMap.Contains(SubsystemClass))
	{
		// Only add instances for non abstract Subsystems
		if (SubsystemClass && !SubsystemClass->HasAllClassFlags(CLASS_Abstract))
		{
			const USubsystem* CDO = SubsystemClass->GetDefaultObject<USubsystem>();
			if (CDO->ShouldCreateSubsystem(Outer))
			{
				USubsystem*& Subsystem = SubsystemMap.Add(SubsystemClass);
				Subsystem = NewObject<USubsystem>(Outer, SubsystemClass);

				Subsystem->Initialize(*this);

				return true;
			}
		}
		return false;
	}
	return true;
}
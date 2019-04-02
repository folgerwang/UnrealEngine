// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/SubsystemCollection.h"

#include "Subsystems/Subsystem.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"

/** FSubsystemModuleWatcher class to hide the implementation of keeping the DynamicSystemModuleMap up to date*/
class FSubsystemModuleWatcher
{
public:
	static void OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange);

	/** Init / Deinit the Module watcher, this tracks module startup and shutdown to ensure only the appropriate dynamic subsystems are instantiated */
	static void InitializeModuleWatcher();
	static void DeinitializeModuleWatcher();

private:
	static void AddClassesForModule(const FName& InModuleName);
	static void RemoveClassesForModule(const FName& InModuleName);

	static FDelegateHandle ModulesChangedHandle;
};

FDelegateHandle FSubsystemModuleWatcher::ModulesChangedHandle;



TArray<FSubsystemCollectionBase*> FSubsystemCollectionBase::SubsystemCollections;
TMap<FName, TArray<TSubclassOf<UDynamicSubsystem>>> FSubsystemCollectionBase::DynamicSystemModuleMap;

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
		
		if (SubsystemCollections.Num() == 0)
		{
			FSubsystemModuleWatcher::InitializeModuleWatcher();
		}
		
		TGuardValue<bool> PopulatingGuard(bPopulating, true);

		if (BaseType->IsChildOf(UDynamicSubsystem::StaticClass()))
		{
			for (const TPair<FName, TArray<TSubclassOf<UDynamicSubsystem>>>& SubsystemClasses : DynamicSystemModuleMap)
			{
				for (const TSubclassOf<UDynamicSubsystem>& SubsystemClass : SubsystemClasses.Value)
				{
					if (SubsystemClass->IsChildOf(BaseType))
					{
						AddAndInitializeSubsystem(SubsystemClass);
					}
				}
			}
		}
		else
		{
			TArray<UClass*> SubsystemClasses;
			GetDerivedClasses(BaseType, SubsystemClasses, true);

			for (UClass* SubsystemClass : SubsystemClasses)
			{
				AddAndInitializeSubsystem(SubsystemClass);
			}
		}

		// Statically track collections
		SubsystemCollections.Add(this);
	}
}

void FSubsystemCollectionBase::Deinitialize()
{
	// Remove static tracking 
	SubsystemCollections.Remove(this);
	if (SubsystemCollections.Num() == 0)
	{
		FSubsystemModuleWatcher::DeinitializeModuleWatcher();
	}

	// Deinit and clean up existing systems
	SubsystemArrayMap.Empty();
	for (auto Iter = SubsystemMap.CreateIterator(); Iter; ++Iter)
	{
		UClass* KeyClass = Iter.Key();
		USubsystem* Subsystem = Iter.Value();
		if (Subsystem->GetClass() == KeyClass)
		{
			Subsystem->Deinitialize();
			Subsystem->InternalOwningSubsystem = nullptr;
		}
	}
	SubsystemMap.Empty();
}

bool FSubsystemCollectionBase::InitializeDependency(TSubclassOf<USubsystem> SubsystemClass)
{
	if (ensureMsgf(SubsystemClass, TEXT("Attempting to add invalid subsystem as dependancy."))
		&& ensureMsgf(bPopulating, TEXT("InitializeDependancy() should only be called from System USubsystem::Initialization() implementations."))
		&& ensureMsgf(SubsystemClass->IsChildOf(BaseType), TEXT("ClassType (%s) must be a subclass of BaseType(%s)."), *SubsystemClass->GetName(), *BaseType->GetName()))
	{
		return AddAndInitializeSubsystem(SubsystemClass);
	}
	return false;
}

void FSubsystemCollectionBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Outer);
	Collector.AddReferencedObjects(SubsystemMap);
}

bool FSubsystemCollectionBase::AddAndInitializeSubsystem(UClass* SubsystemClass)
{
	if (!SubsystemMap.Contains(SubsystemClass))
	{
		// Only add instances for non abstract Subsystems
		if (SubsystemClass && !SubsystemClass->HasAllClassFlags(CLASS_Abstract))
		{
			// Catch any attempt to add a subsystem of the wrong type
			checkf(SubsystemClass->IsChildOf(BaseType), TEXT("ClassType (%s) must be a subclass of BaseType(%s)."), *SubsystemClass->GetName(), *BaseType->GetName());

			const USubsystem* CDO = SubsystemClass->GetDefaultObject<USubsystem>();
			if (CDO->ShouldCreateSubsystem(Outer))
			{
				USubsystem*& Subsystem = SubsystemMap.Add(SubsystemClass);
				Subsystem = NewObject<USubsystem>(Outer, SubsystemClass);

				Subsystem->InternalOwningSubsystem = this;
				Subsystem->Initialize(*this);
				
				return true;
			}
		}
		return false;
	}
	return true;
}

void FSubsystemCollectionBase::RemoveAndDeinitializeSubsystem(USubsystem* Subsystem)
{
	check(Subsystem);
	USubsystem* SubsystemFound = SubsystemMap.FindAndRemoveChecked(Subsystem->GetClass());
	check(Subsystem == SubsystemFound);

	Subsystem->Deinitialize();
	Subsystem->InternalOwningSubsystem = nullptr;
}

void FSubsystemCollectionBase::AddAllInstances(UClass* SubsystemClass)
{
	for (FSubsystemCollectionBase* SubsystemCollection : SubsystemCollections)
	{
		if (SubsystemClass->IsChildOf(SubsystemCollection->BaseType))
		{
			SubsystemCollection->AddAndInitializeSubsystem(SubsystemClass);
		}
	}
}

void FSubsystemCollectionBase::RemoveAllInstances(UClass* SubsystemClass)
{
	ForEachObjectOfClass(SubsystemClass, [](UObject* SubsystemObj)
	{
		USubsystem* Subsystem = CastChecked<USubsystem>(SubsystemObj);

		if (Subsystem->InternalOwningSubsystem)
		{
			Subsystem->InternalOwningSubsystem->RemoveAndDeinitializeSubsystem(Subsystem);
		}
	});
}




/** FSubsystemModuleWatcher Implementations */
void FSubsystemModuleWatcher::OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange)
{

	switch (ReasonForChange)
	{
	case EModuleChangeReason::ModuleLoaded:
		AddClassesForModule(ModuleThatChanged);
		break;

	case EModuleChangeReason::ModuleUnloaded:
		RemoveClassesForModule(ModuleThatChanged);
		break;
	}
}


void FSubsystemModuleWatcher::InitializeModuleWatcher()
{
	check(!ModulesChangedHandle.IsValid());

	// Add Loaded Modules
	TArray<UClass*> SubsystemClasses;
	GetDerivedClasses(UDynamicSubsystem::StaticClass(), SubsystemClasses, true);

	for (UClass* SubsystemClass : SubsystemClasses)
	{
		if (!SubsystemClass->HasAllClassFlags(CLASS_Abstract))
		{
			UPackage* const ClassPackage = SubsystemClass->GetOuterUPackage();
			if (ClassPackage)
			{
				const FName ModuleName = FPackageName::GetShortFName(ClassPackage->GetFName());
				if (FModuleManager::Get().IsModuleLoaded(ModuleName))
				{
					TArray<TSubclassOf<UDynamicSubsystem>>& ModuleSubsystemClasses = FSubsystemCollectionBase::DynamicSystemModuleMap.FindOrAdd(ModuleName);
					ModuleSubsystemClasses.Add(SubsystemClass);
				}
			}
		}
	}

	ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddStatic(&FSubsystemModuleWatcher::OnModulesChanged);
}

void FSubsystemModuleWatcher::DeinitializeModuleWatcher()
{
	if (ModulesChangedHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
	}
}

void FSubsystemModuleWatcher::AddClassesForModule(const FName& InModuleName)
{
	check(!FSubsystemCollectionBase::DynamicSystemModuleMap.Contains(InModuleName));

	// Find the class package for this module
	const UPackage* const ClassPackage = FindPackage(nullptr, *(FString("/Script/") + InModuleName.ToString()));
	if (!ClassPackage)
	{
		return;
	}

	TArray<TSubclassOf<UDynamicSubsystem>> SubsystemClasses;
	TArray<UObject*> PackageObjects;
	GetObjectsWithOuter(ClassPackage, PackageObjects, false);
	for (UObject* Object : PackageObjects)
	{
		UClass* const CurrentClass = Cast<UClass>(Object);
		if (CurrentClass && !CurrentClass->HasAllClassFlags(CLASS_Abstract) && CurrentClass->IsChildOf(UDynamicSubsystem::StaticClass()))
		{
			SubsystemClasses.Add(CurrentClass);
			FSubsystemCollectionBase::AddAllInstances(CurrentClass);
		}
	}
	if (SubsystemClasses.Num() > 0)
	{
		FSubsystemCollectionBase::DynamicSystemModuleMap.Add(InModuleName, MoveTemp(SubsystemClasses));
	}
}
void FSubsystemModuleWatcher::RemoveClassesForModule(const FName& InModuleName)
{
	TArray<TSubclassOf<UDynamicSubsystem>>* SubsystemClasses = FSubsystemCollectionBase::DynamicSystemModuleMap.Find(InModuleName);
	if (SubsystemClasses)
	{
		for (TSubclassOf<UDynamicSubsystem>& SubsystemClass : *SubsystemClasses)
		{
			FSubsystemCollectionBase::RemoveAllInstances(SubsystemClass);
		}
		FSubsystemCollectionBase::DynamicSystemModuleMap.Remove(InModuleName);
	}
}
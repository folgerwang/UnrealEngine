// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/GCObject.h"

class USubsystem;
class UDynamicSubsystem;

class ENGINE_API FSubsystemCollectionBase : public FGCObject
{
public:
	/** Initialize the collection of systems, systems will be created and initialized */
	void Initialize();

	/* Clears the collection, while deinitializing the systems */
	void Deinitialize();

	/** 
	 * Only call from Initialize() of Systems to ensure initialization order
	 * Note: Dependencies only work within a collection
	 */
	bool InitializeDependency(TSubclassOf<USubsystem> SubsystemClass);

	/* FGCObject Interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

protected:
	/** protected constructor - for use by the template only(FSubsystemCollection<TBaseType>) */
	FSubsystemCollectionBase(UObject* InOuter, TSubclassOf<USubsystem> InBaseType);

	/** protected constructor - Use the FSubsystemCollection<TBaseType> class */
	FSubsystemCollectionBase();

	/** Get a Subsystem by type */
	USubsystem* GetSubsystemInternal(TSubclassOf<USubsystem> SubsystemClass) const;

	/** Get a list of Subsystems by type */
	const TArray<USubsystem*>& GetSubsystemArrayInternal(TSubclassOf<USubsystem> SubsystemClass) const;

	/** Get the collection BaseType */
	const TSubclassOf<USubsystem>& GetBaseType() const { return BaseType; }

private:
	bool AddAndInitializeSubsystem(UClass* SubsystemClass);

	void RemoveAndDeinitializeSubsystem(USubsystem* Subsystem);

	TMap<TSubclassOf<USubsystem>, USubsystem*> SubsystemMap;

	mutable TMap<TSubclassOf<USubsystem>, TArray<USubsystem*>> SubsystemArrayMap;

	TSubclassOf<USubsystem> BaseType;

	UObject* Outer;

	bool bPopulating;

private:
	friend class FSubsystemModuleWatcher;

	/** Add Instances of the specified Subsystem class to all existing SubsystemCollections of the correct type */
	static void AddAllInstances(UClass* SubsystemClass);

	/** Remove Instances of the specified Subsystem class from all existing SubsystemCollections of the correct type */
	static void RemoveAllInstances(UClass* SubsystemClass);

	static TArray<FSubsystemCollectionBase*> SubsystemCollections;
	static TMap<FName, TArray<TSubclassOf<UDynamicSubsystem>>> DynamicSystemModuleMap;
};

template<typename TBaseType>
class FSubsystemCollection : public FSubsystemCollectionBase
{
public:
	/** Get a Subsystem by type */
	template <typename TSubsystemClass>
	TSubsystemClass* GetSubsystem(TSubclassOf<TSubsystemClass> SubsystemClass) const
	{
		static_assert(TIsDerivedFrom<TSubsystemClass, TBaseType>::IsDerived, "TSubsystemClass must be derived from TBaseType");
		return Cast<TSubsystemClass>(GetSubsystemInternal(SubsystemClass));
	}

	/** Get a list of Subsystems by type */
	template <typename TSubsystemClass>
	const TArray<TSubsystemClass*>& GetSubsystemArray(TSubclassOf<TSubsystemClass> SubsystemClass) const
	{
		// Force a compile time check that TSubsystemClass derives from TBaseType, the internal code only enforces it's a USubsystem
		TSubclassOf<TBaseType> SubsystemBaseClass = SubsystemClass;

		const TArray<USubsystem*>& Array = GetSubsystemArrayInternal(SubsystemBaseClass);
		TArray<TSubsystemClass*>* SpecificArray = reinterpret_cast<TArray<TSubsystemClass*>*>(&Array);
		return *SpecificArray;
	}

public:

	/** Construct a FSubsystemCollection, pass in the owning object almost certainly (this). */
	FSubsystemCollection(UObject* InOuter)
		: FSubsystemCollectionBase(InOuter, TBaseType::StaticClass())
	{
	}

	/** DO NOT USE - required for default UObject default constructors unfortunately. */
	FSubsystemCollection() {}
};


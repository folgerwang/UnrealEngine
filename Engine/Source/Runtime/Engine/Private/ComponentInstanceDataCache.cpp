// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ComponentInstanceDataCache.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/DuplicatedObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/Package.h"
#include "UObject/UObjectAnnotation.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

class FComponentPropertyWriter : public FObjectWriter
{
public:
	FComponentPropertyWriter(const UActorComponent* InComponent, FActorComponentInstanceData& InActorInstanceData)
		: FObjectWriter(InActorInstanceData.SavedProperties)
		, Component(InComponent)
		, ActorInstanceData(InActorInstanceData)
	{
		// Include properties that would normally skip tagged serialization (e.g. bulk serialization of array properties).
		ArPortFlags |= PPF_ForceTaggedSerialization;

		if (Component)
		{
			UClass* ComponentClass = Component->GetClass();

			Component->GetUCSModifiedProperties(PropertiesToSkip);

			if (AActor* ComponentOwner = Component->GetOwner())
			{
				// If this is the owning Actor's root scene component, don't include relative transform properties. This is handled elsewhere.
				if (Component == ComponentOwner->GetRootComponent())
				{
					PropertiesToSkip.Add(ComponentClass->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation)));
					PropertiesToSkip.Add(ComponentClass->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation)));
					PropertiesToSkip.Add(ComponentClass->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D)));
				}
			}

			ComponentClass->SerializeTaggedProperties(*this, (uint8*)Component, ComponentClass, (uint8*)Component->GetArchetype());
		}
	}

	virtual ~FComponentPropertyWriter()
	{
		DuplicatedObjectAnnotation.RemoveAllAnnotations();
	}

	virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
	{
		// Immutable structs expect to serialize all properties so don't skip regardless of other conditions
		UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InProperty->GetOuter());
		const bool bPropertyInImmutableStruct = ScriptStruct && ((ScriptStruct->StructFlags & STRUCT_Immutable) != 0);
		
		return (!bPropertyInImmutableStruct
		        && (   InProperty->HasAnyPropertyFlags(CPF_Transient)
		            || !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)
		            || PropertiesToSkip.Contains(InProperty)
		           )
		       );
	}


	UObject* GetDuplicatedObject(UObject* Object)
	{
		UObject* Result = Object;
		if (IsValid(Object))
		{
			// Check for an existing duplicate of the object.
			FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( Object );
			if( !DupObjectInfo.IsDefault() )
			{
				Result = DupObjectInfo.DuplicatedObject;
			}
			else if (Object->GetOuter() == Component)
			{
				Result = DuplicateObject(Object, GetTransientPackage());
				ActorInstanceData.DuplicatedObjects.Emplace(Result);
			}
			else
			{
				check(Object->IsIn(Component));

				// Check to see if the object's outer is being duplicated.
				UObject* DupOuter = GetDuplicatedObject(Object->GetOuter());
				if (DupOuter != nullptr)
				{
					// First check if the duplicated outer already has an allocated duplicate of this object
					Result = static_cast<UObject*>(FindObjectWithOuter(DupOuter, Object->GetClass(), Object->GetFName()));

					if (Result == nullptr)
					{
						// The object's outer is being duplicated, create a duplicate of this object.
						Result = DuplicateObject(Object, DupOuter);
					}

					DuplicatedObjectAnnotation.AddAnnotation( Object, FDuplicatedObject( Result ) );
				}
			}
		}

		return Result;
	}

	virtual FArchive& operator<<(FName& Name) override
	{
		// store the reference to this name in the array instead of the global table, this allow to for persistence
		int32 ReferenceIndex = ActorInstanceData.ReferencedNames.AddUnique(Name);
		// save the name as an index in the referenced name array
		*this << ReferenceIndex;

		return *this;
	}

	virtual FArchive& operator<<(UObject*& Object) override
	{
		UObject* SerializedObject = Object;
		if (Object && Component && Object->IsIn(Component))
		{
			SerializedObject = GetDuplicatedObject(Object);
		}

		// store the pointer to this object
		int32 ReferenceIndex = INDEX_NONE;
		if (SerializedObject)
		{
			ReferenceIndex = ActorInstanceData.ReferencedObjects.Num();
			ActorInstanceData.ReferencedObjects.Add(SerializedObject);
		}
		// save the pointer as an index in the referenced object array
		*this << ReferenceIndex;

		return *this;
	}

	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
	{
		UObject* Obj = LazyObjectPtr.Get();
		*this << Obj;
		return *this;
	}

private:
	const UActorComponent* Component;
	FActorComponentInstanceData& ActorInstanceData;
	TSet<const UProperty*> PropertiesToSkip;
	FUObjectAnnotationSparse<FDuplicatedObject,false> DuplicatedObjectAnnotation;
};

class FComponentPropertyReader : public FObjectReader
{
public:
	FComponentPropertyReader(UActorComponent* InComponent, FActorComponentInstanceData& InActorInstanceData)
		: FObjectReader(InActorInstanceData.SavedProperties)
		, ActorInstanceData(InActorInstanceData)
	{
		// Include properties that would normally skip tagged serialization (e.g. bulk serialization of array properties).
		ArPortFlags |= PPF_ForceTaggedSerialization;

		InComponent->GetUCSModifiedProperties(PropertiesToSkip);

		UClass* Class = InComponent->GetClass();
		Class->SerializeTaggedProperties(*this, (uint8*)InComponent, Class, (uint8*)InComponent->GetArchetype());
	}

	virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
	{
		return PropertiesToSkip.Contains(InProperty);
	}

	virtual FArchive& operator<<(FName& Name) override
	{
		// FName are serialized as Index in ActorInstanceData instead of the normal FName table
		int32 ReferenceIndex = INDEX_NONE;
		*this << ReferenceIndex;
		Name = ActorInstanceData.ReferencedNames.IsValidIndex(ReferenceIndex) ? ActorInstanceData.ReferencedNames[ReferenceIndex] : FName();
		return *this;
	}

	virtual FArchive& operator<<(UObject*& Object) override
	{
		// UObject pointer are serialized as Index in ActorInstanceData
		int32 ReferenceIndex = INDEX_NONE;
		*this << ReferenceIndex;
		Object = ActorInstanceData.ReferencedObjects.IsValidIndex(ReferenceIndex) ? ActorInstanceData.ReferencedObjects[ReferenceIndex] : nullptr;
		return *this;
	}

	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
	{
		UObject* Obj = LazyObjectPtr.Get();
		*this << Obj;
		return *this;
	}

	FActorComponentInstanceData& ActorInstanceData;
	TSet<const UProperty*> PropertiesToSkip;
};


FActorComponentDuplicatedObjectData::FActorComponentDuplicatedObjectData(UObject* InObject)
	: DuplicatedObject(InObject)
	, ObjectPathDepth(0)
{
	if (DuplicatedObject)
	{
		for (UObject* Outer = DuplicatedObject; Outer; Outer = Outer->GetOuter())
		{
			++ObjectPathDepth;
		}
	}
}

bool FActorComponentDuplicatedObjectData::Serialize(FArchive& Ar)
{
	enum class EVersion : uint8
	{
		InitialVersion = 0,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	EVersion Version = EVersion::LatestVersion;
	Ar << Version;

	if (Version > EVersion::LatestVersion)
	{
		Ar.SetError();
		return true;
	}

	FString ObjectClassPath;
	FString ObjectOuterPath;
	FName ObjectName;
	uint32 ObjectPersistentFlags = 0;
	TArray<uint8> ObjectData;

	if (Ar.IsSaving() && DuplicatedObject)
	{
		UClass* ObjectClass = DuplicatedObject->GetClass();
		ObjectClassPath = ObjectClass ? ObjectClass->GetPathName() : FString();
		ObjectOuterPath = DuplicatedObject->GetOuter() ? DuplicatedObject->GetOuter()->GetPathName() : FString();
		ObjectName = DuplicatedObject->GetFName();
		ObjectPersistentFlags = DuplicatedObject->GetFlags() & RF_Load;
		if (ObjectClass)
		{
			FMemoryWriter Writer(ObjectData);
			ObjectClass->SerializeTaggedProperties(Writer, (uint8*)DuplicatedObject, ObjectClass, nullptr);
		}
	}

	Ar << ObjectClassPath;
	Ar << ObjectOuterPath;
	Ar << ObjectName;
	Ar << ObjectPersistentFlags;
	Ar << ObjectData;

	// If loading use the de-serialized property to assign DuplicatedObject
	if (Ar.IsLoading())
	{
		DuplicatedObject = nullptr;
		// Get the object class
		if (UClass* ObjectClass = LoadObject<UClass>(nullptr, *ObjectClassPath))
		{
			// Get the object outer
			if (UObject* FoundOuter = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectOuterPath))
			{
				// Create the duplicated object
				DuplicatedObject = NewObject<UObject>(FoundOuter, ObjectClass, *ObjectName.ToString(), (EObjectFlags)ObjectPersistentFlags);

				// Deserialize the duplicated object
				FMemoryReader Reader(ObjectData);
				ObjectClass->SerializeTaggedProperties(Reader, (uint8*)DuplicatedObject, ObjectClass, nullptr);
			}
		}
	}

	return true;
}

FActorComponentInstanceData::FActorComponentInstanceData()
	: SourceComponentTemplate(nullptr)
	, SourceComponentCreationMethod(EComponentCreationMethod::Native)
	, SourceComponentTypeSerializedIndex(INDEX_NONE)
{}

FActorComponentInstanceData::FActorComponentInstanceData(const UActorComponent* SourceComponent)
{
	check(SourceComponent);
	SourceComponentTemplate = SourceComponent->GetArchetype();
	SourceComponentCreationMethod = SourceComponent->CreationMethod;
	SourceComponentTypeSerializedIndex = INDEX_NONE;

	// UCS components can share the same template (e.g. an AddComponent node inside a loop), so we also cache their serialization index here (relative to the shared template) as a means for identification
	if(SourceComponentCreationMethod == EComponentCreationMethod::UserConstructionScript)
	{
		AActor* ComponentOwner = SourceComponent->GetOwner();
		if (ComponentOwner)
		{
			bool bFound = false;
			for (const UActorComponent* BlueprintCreatedComponent : ComponentOwner->BlueprintCreatedComponents)
			{
				if (BlueprintCreatedComponent)
				{
					if (BlueprintCreatedComponent == SourceComponent)
					{
						++SourceComponentTypeSerializedIndex;
						bFound = true;
						break;
					}
					else if (   BlueprintCreatedComponent->CreationMethod == SourceComponentCreationMethod
						&& BlueprintCreatedComponent->GetArchetype() == SourceComponentTemplate)
					{
						++SourceComponentTypeSerializedIndex;
					}
				}
			}
			if (!bFound)
			{
				SourceComponentTypeSerializedIndex = INDEX_NONE;
			}
		}
	}

	if (SourceComponent->IsEditableWhenInherited())
	{
		FComponentPropertyWriter ComponentPropertyWriter(SourceComponent, *this);

		// Cache off the length of an array that will come from SerializeTaggedProperties that had no properties saved in to it.
		auto GetSizeOfEmptyArchive = []() -> int32
		{
			const UActorComponent* DummyComponent = GetDefault<UActorComponent>();
			FActorComponentInstanceData DummyInstanceData;
			FComponentPropertyWriter NullWriter(nullptr, DummyInstanceData);
			UClass* ComponentClass = DummyComponent->GetClass();
			
			// By serializing the component with itself as its defaults we guarantee that no properties will be written out
			ComponentClass->SerializeTaggedProperties(NullWriter, (uint8*)DummyComponent, ComponentClass, (uint8*)DummyComponent);

			check(DummyInstanceData.DuplicatedObjects.Num() == 0 && DummyInstanceData.ReferencedObjects.Num() == 0);
			return DummyInstanceData.SavedProperties.Num();
		};

		static const int32 SizeOfEmptyArchive = GetSizeOfEmptyArchive();

		// SerializeTaggedProperties will always put a sentinel NAME_None at the end of the Archive. 
		// If that is the only thing in the buffer then empty it because we want to know that we haven't stored anything.
		if (SavedProperties.Num() == SizeOfEmptyArchive)
		{
			SavedProperties.Empty();
		}
	}

	// Sort duplicated objects, if any, this ensures that lower depth duplicated objects are first in the array, which ensures proper creation order when deserializing
	if (DuplicatedObjects.Num() > 0)
	{
		DuplicatedObjects.StableSort([](const FActorComponentDuplicatedObjectData& One, const FActorComponentDuplicatedObjectData& Two) -> bool
		{
			return One.ObjectPathDepth < Two.ObjectPathDepth;
		});
	}
}

bool FActorComponentInstanceData::MatchesComponent(const UActorComponent* Component, const UObject* ComponentTemplate, const TMap<UActorComponent*, const UObject*>& ComponentToArchetypeMap) const
{
	bool bMatches = false;
	if (   Component
		&& Component->CreationMethod == SourceComponentCreationMethod
		&& (ComponentTemplate == SourceComponentTemplate || (GIsReinstancing && ComponentTemplate->GetFName() == SourceComponentTemplate->GetFName())))
	{
		if (SourceComponentCreationMethod != EComponentCreationMethod::UserConstructionScript)
		{
			bMatches = true;
		}
		else if (SourceComponentTypeSerializedIndex >= 0)
		{
			int32 FoundSerializedComponentsOfType = -1;
			AActor* ComponentOwner = Component->GetOwner();
			if (ComponentOwner)
			{
				for (const UActorComponent* BlueprintCreatedComponent : ComponentOwner->BlueprintCreatedComponents)
				{
					if (BlueprintCreatedComponent != nullptr && BlueprintCreatedComponent->CreationMethod == SourceComponentCreationMethod)
					{
						const UObject* BlueprintComponentTemplate = ComponentToArchetypeMap.FindChecked(BlueprintCreatedComponent);
						if (   (BlueprintComponentTemplate == SourceComponentTemplate || (GIsReinstancing && BlueprintComponentTemplate->GetFName() == SourceComponentTemplate->GetFName()))
							&& (++FoundSerializedComponentsOfType == SourceComponentTypeSerializedIndex))
						{
							bMatches = (BlueprintCreatedComponent == Component);
							break;
						}
					}
				}
			}
		}
	}
	return bMatches;
}

void FActorComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	// After the user construction script has run we will re-apply all the cached changes that do not conflict
	// with a change that the user construction script made.
	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript && SavedProperties.Num() > 0)
	{
		Component->DetermineUCSModifiedProperties();

		for (const FActorComponentDuplicatedObjectData& DuplicatedObjectData : DuplicatedObjects)
		{
			if (DuplicatedObjectData.DuplicatedObject)
			{
				DuplicatedObjectData.DuplicatedObject->Rename(nullptr, Component, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			}
		}

		FComponentPropertyReader ComponentPropertyReader(Component, *this);

		if (Component->IsRegistered())
		{
			Component->ReregisterComponent();
		}
	}
}

void FActorComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SourceComponentTemplate);
	Collector.AddReferencedObjects(ReferencedObjects);
}

FComponentInstanceDataCache::FComponentInstanceDataCache(const AActor* Actor)
{
	if (Actor != nullptr)
	{
		const bool bIsChildActor = Actor->IsChildActor();

		TInlineComponentArray<UActorComponent*> Components(Actor);

		ComponentsInstanceData.Reserve(Components.Num());

		// Grab per-instance data we want to persist
		for (UActorComponent* Component : Components)
		{
			if (bIsChildActor || Component->IsCreatedByConstructionScript()) // Only cache data from 'created by construction script' components
			{
				TStructOnScope<FActorComponentInstanceData> ComponentInstanceData = Component->GetComponentInstanceData();
				if (ComponentInstanceData.IsValid())
				{
					ComponentsInstanceData.Add(MoveTemp(ComponentInstanceData));
				}
			}
			else if (Component->CreationMethod == EComponentCreationMethod::Instance)
			{
				// If the instance component is attached to a BP component we have to be prepared for the possibility that it will be deleted
				if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
				{
					if (SceneComponent->GetAttachParent() && SceneComponent->GetAttachParent()->IsCreatedByConstructionScript())
					{
						// In rare cases the root component can be unset so walk the hierarchy and find what is probably the root component for the purposes of storing off the relative transform
						USceneComponent* RelativeToComponent = Actor->GetRootComponent();
						if (RelativeToComponent == nullptr)
						{
							RelativeToComponent = SceneComponent->GetAttachParent();
							while (RelativeToComponent->GetAttachParent() && RelativeToComponent->GetAttachParent()->GetOwner() == Actor)
							{
								RelativeToComponent = RelativeToComponent->GetAttachParent();
							}
						}

						SceneComponent->ConditionalUpdateComponentToWorld();
						InstanceComponentTransformToRootMap.Add(SceneComponent, SceneComponent->GetComponentTransform().GetRelativeTransform(RelativeToComponent->GetComponentTransform()));
					}
				}
			}
		}
	}
}

void FComponentInstanceDataCache::Serialize(FArchive& Ar)
{
	enum class EVersion : uint8
	{
		InitialVersion = 0,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	EVersion Version = EVersion::LatestVersion;
	Ar << Version;

	if (Version > EVersion::LatestVersion)
	{
		Ar.SetError();
		return;
	}


	if (Ar.IsLoading())
	{
		// Since not all properties are serializable we don't want to deserialize the map directly,
		// so we deserialize it in a temp array and copy it over
		TArray<TStructOnScope<FActorComponentInstanceData>> TempInstanceData;
		Ar << TempInstanceData;
		CopySerializableProperties(MoveTemp(TempInstanceData));
	}
	else
	{
		Ar << ComponentsInstanceData;
	}

	Ar << InstanceComponentTransformToRootMap;
}

void FComponentInstanceDataCache::ApplyToActor(AActor* Actor, const ECacheApplyPhase CacheApplyPhase) const
{
	if (Actor != nullptr)
	{
		const bool bIsChildActor = Actor->IsChildActor();

		// We want to apply instance data from the root node down to ensure changes such as transforms 
		// propagate correctly so we will build the components list in a breadth-first manner.
		TInlineComponentArray<UActorComponent*> Components;
		Components.Reserve(Actor->GetComponents().Num());

		auto AddComponentHierarchy = [Actor, &Components](USceneComponent* Component)
		{
			int32 FirstProcessIndex = Components.Num();

			// Add this to our list and make it our starting node
			Components.Add(Component);

			int32 CompsToProcess = 1;

			while (CompsToProcess)
			{
				// track how many elements were here
				const int32 StartingProcessedCount = Components.Num();

				// process the currently unprocessed elements
				for (int32 ProcessIndex = 0; ProcessIndex < CompsToProcess; ++ProcessIndex)
				{
					USceneComponent* SceneComponent = CastChecked<USceneComponent>(Components[FirstProcessIndex + ProcessIndex]);

					// add all children to the end of the array
					for (int32 ChildIndex = 0; ChildIndex < SceneComponent->GetNumChildrenComponents(); ++ChildIndex)
					{
						if (USceneComponent* ChildComponent = SceneComponent->GetChildComponent(ChildIndex))
						{
							// We don't want to recurse in to child actors (or any other attached actor) when applying the instance cache, 
							// components within a child actor are handled by applying the instance data to the child actor component
							if (ChildComponent->GetOwner() == Actor)
							{
								Components.Add(ChildComponent);
							}
						}
					}
				}

				// next loop start with the nodes we just added
				FirstProcessIndex = StartingProcessedCount;
				CompsToProcess = Components.Num() - StartingProcessedCount;
			}
		};

		if (USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			AddComponentHierarchy(RootComponent);
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				USceneComponent* ParentComponent = SceneComponent->GetAttachParent();
				if ((ParentComponent == nullptr && SceneComponent != Actor->GetRootComponent()) || (ParentComponent && ParentComponent->GetOwner() != Actor))
				{
					AddComponentHierarchy(SceneComponent);
				}
			}
			else if (Component)
			{
				Components.Add(Component);
			}
		}

		// Cache all archetype objects
		TMap<UActorComponent*, const UObject*> ComponentToArchetypeMap;
		ComponentToArchetypeMap.Reserve(Components.Num());

		for (UActorComponent* ComponentInstance : Components)
		{
			if (ComponentInstance && (bIsChildActor || ComponentInstance->IsCreatedByConstructionScript()))
			{
				ComponentToArchetypeMap.Add(ComponentInstance, ComponentInstance->GetArchetype());
			}
		}

		// Apply per-instance data.
		for (UActorComponent* ComponentInstance : Components)
		{
			if (ComponentInstance && (bIsChildActor || ComponentInstance->IsCreatedByConstructionScript())) // Only try and apply data to 'created by construction script' components
			{
				// Cache template here to avoid redundant calls in the loop below
				const UObject* ComponentTemplate = ComponentToArchetypeMap.FindChecked(ComponentInstance);

				for (const TStructOnScope<FActorComponentInstanceData>& ComponentInstanceData : ComponentsInstanceData)
				{
					if (	ComponentInstanceData.IsValid()
						&&	ComponentInstanceData->GetComponentClass() == ComponentTemplate->GetClass() // filter on class early to avoid unnecessary virtual and expensive tests
						&&	ComponentInstanceData->MatchesComponent(ComponentInstance, ComponentTemplate, ComponentToArchetypeMap))
					{
						ComponentInstanceData->ApplyToComponent(ComponentInstance, CacheApplyPhase);
						break;
					}
				}
			}
		}

		// Once we're done attaching, if we have any unattached instance components move them to the root
		for (auto InstanceTransformPair : InstanceComponentTransformToRootMap)
		{
			check(Actor->GetRootComponent());

			USceneComponent* SceneComponent = InstanceTransformPair.Key;
			if (SceneComponent && (SceneComponent->GetAttachParent() == nullptr || SceneComponent->GetAttachParent()->IsPendingKill()))
			{
				SceneComponent->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
				SceneComponent->SetRelativeTransform(InstanceTransformPair.Value);
			}
		}
	}
}

void FComponentInstanceDataCache::FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (TStructOnScope<FActorComponentInstanceData>& ComponentInstanceData : ComponentsInstanceData)
	{
		if (ComponentInstanceData.IsValid())
		{
			ComponentInstanceData->FindAndReplaceInstances(OldToNewInstanceMap);
		}
	}
	TArray<USceneComponent*> SceneComponents;
	InstanceComponentTransformToRootMap.GenerateKeyArray(SceneComponents);

	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (UObject* const* NewSceneComponent = OldToNewInstanceMap.Find(SceneComponent))
		{
			if (*NewSceneComponent)
			{
				InstanceComponentTransformToRootMap.Add(CastChecked<USceneComponent>(*NewSceneComponent), InstanceComponentTransformToRootMap.FindAndRemoveChecked(SceneComponent));
			}
			else
			{
				InstanceComponentTransformToRootMap.Remove(SceneComponent);
			}
		}
	}
}

void FComponentInstanceDataCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(InstanceComponentTransformToRootMap);

	for (TStructOnScope<FActorComponentInstanceData>& ComponentInstanceData : ComponentsInstanceData)
	{
		if (ComponentInstanceData.IsValid())
		{
			ComponentInstanceData->AddReferencedObjects(Collector);
		}
	}
}

void FComponentInstanceDataCache::CopySerializableProperties(TArray<TStructOnScope<FActorComponentInstanceData>> InComponentsInstanceData)
{
	auto CopyProperties = [](TStructOnScope<FActorComponentInstanceData>& DestData, const TStructOnScope<FActorComponentInstanceData>& SrcData)
	{
		for (TFieldIterator<const UProperty> PropertyIt(SrcData.GetStruct(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::IncludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces); PropertyIt; ++PropertyIt)
		{
			const void* SrcValuePtr = PropertyIt->ContainerPtrToValuePtr<void>(SrcData.Get());
			void* DestValuePtr = PropertyIt->ContainerPtrToValuePtr<void>(DestData.Get());
			PropertyIt->CopyCompleteValue(DestValuePtr, SrcValuePtr);
		}
	};

	for (TStructOnScope<FActorComponentInstanceData>& InstanceData : InComponentsInstanceData)
	{
		TStructOnScope<FActorComponentInstanceData>* DestInstanceData = ComponentsInstanceData.FindByPredicate([&InstanceData](TStructOnScope<FActorComponentInstanceData>& InInstanceData)
		{
			return InstanceData->GetComponentTemplate() == InInstanceData->GetComponentTemplate() && InstanceData.GetStruct() == InInstanceData.GetStruct();
		});
		// if we find a ComponentInstanceData to apply it too, we copy the properties over
		if (DestInstanceData)
		{
			CopyProperties(*DestInstanceData, InstanceData);
		}
		// otherwise we just add our to the list, since no component instance data was created for it
		else
		{
			ComponentsInstanceData.Add(MoveTemp(InstanceData));
		}
	}
}

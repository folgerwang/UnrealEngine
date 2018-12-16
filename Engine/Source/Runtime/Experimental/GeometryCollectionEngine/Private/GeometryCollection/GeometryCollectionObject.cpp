// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: UGeometryCollection methods.
=============================================================================*/
#include "GeometryCollection/GeometryCollectionObject.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionCache.h"
#include "UObject/DestructionObjectVersion.h"
#include "Serialization/ArchiveCountMem.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "Materials/MaterialInstance.h"

DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionLogging, NoLogging, All);

UGeometryCollection::UGeometryCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GeometryCollection(new FGeometryCollection())
{
	check(ObjectInitializer.GetClass() == GetClass());
	if (UGeometryCollection* InputComponent = static_cast<UGeometryCollection*>(ObjectInitializer.GetObj()))
	{
		if (InputComponent->GetGeometryCollection())
		{
			GeometryCollection = TSharedPtr<FGeometryCollection>(
				new FGeometryCollection(*static_cast<UGeometryCollection*>(ObjectInitializer.GetObj())->GetGeometryCollection()));
		}
	}

	PersistentGuid = FGuid::NewGuid();
	InvalidateCollection();
}

//** Initialize */
void UGeometryCollection::Initialize(FManagedArrayCollection & CollectionIn)
{
	Modify();
	GeometryCollection->Initialize(CollectionIn);
	InvalidateCollection();
}


/** AppendGeometry */
int32 UGeometryCollection::AppendGeometry(const UGeometryCollection & Element)
{
	Modify();
	InvalidateCollection();
	return GeometryCollection->AppendGeometry(*Element.GetGeometryCollection());
}

/** NumElements */
int32 UGeometryCollection::NumElements(const FName & Group)
{
	return GeometryCollection->NumElements(Group);
}


/** RemoveElements */
void UGeometryCollection::RemoveElements(const FName & Group, const TArray<int32>& SortedDeletionList)
{
	Modify();
	GeometryCollection->RemoveElements(Group, SortedDeletionList);
	InvalidateCollection();
}


/** ReindexMaterialSections */
void UGeometryCollection::ReindexMaterialSections()
{
	Modify();
	GeometryCollection->ReindexMaterials();
	InvalidateCollection();
}

void UGeometryCollection::AppendStandardMaterials()
{
	// Second to last material is the interior material
	// #todo(dmp): This will be replaced when we support multiple internal materials
	UMaterialInterface* InteriorMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/GeometryCollectionPlugin/InMaterial.InMaterial"), NULL, LOAD_None, NULL);
	InteriorMaterialIndex = Materials.Add(InteriorMaterial);	

	// Last Material is the selection one
	UMaterialInterface* BoneSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/GeometryCollectionPlugin/SelectedGeometryMaterial.SelectedGeometryMaterial"), NULL, LOAD_None, NULL);
	BoneSelectedMaterialIndex = Materials.Add(BoneSelectedMaterial);	
}

/** Returns true if there is anything to render */
bool UGeometryCollection::HasVisibleGeometry()
{
	return GeometryCollection->HasVisibleGeometry();
}

/** Serialize */
void UGeometryCollection::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FDestructionObjectVersion::GUID);

	GeometryCollection->Serialize(Ar);

	if(Ar.CustomVer(FDestructionObjectVersion::GUID) < FDestructionObjectVersion::AddedTimestampedGeometryComponentCache)
	{
		if(Ar.IsLoading())
		{
			// Strip old recorded cache data
			int32 DummyNumFrames;
			TArray<TArray<FTransform>> DummyTransforms;

			Ar << DummyNumFrames;
			DummyTransforms.SetNum(DummyNumFrames);
			for(int32 Index = 0; Index < DummyNumFrames; ++Index)
			{
				Ar << DummyTransforms[Index];
			}
		}
	}
	else
	{
		// Push up the chain to hit tagged properties too
		// This should have always been in here but because we have saved assets
		// from before this line was here it has to be gated
		Super::Serialize(Ar);
	}
}

void UGeometryCollection::InvalidateCollection()
{
	StateGuid = FGuid::NewGuid();
}

FGuid UGeometryCollection::GetIdGuid() const
{
	return PersistentGuid;
}

FGuid UGeometryCollection::GetStateGuid() const
{
	return StateGuid;
}

#if WITH_EDITOR
void UGeometryCollection::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	InvalidateCollection();
}
#endif

bool UGeometryCollection::Modify(bool bAlwaysMarkDirty /*= true*/)
{
	bool bSuperResult = Super::Modify(bAlwaysMarkDirty);

	UPackage* Package = GetOutermost();
	if(Package->IsDirty())
	{
		InvalidateCollection();
	}

	return bSuperResult;
}

void UGeometryCollection::PostLoad()
{
	Super::PostLoad();
}

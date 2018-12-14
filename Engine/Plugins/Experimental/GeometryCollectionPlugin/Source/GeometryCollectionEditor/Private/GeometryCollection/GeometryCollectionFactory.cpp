// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionFactory.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"
#include "GeometryCollection/GeometryCollectionConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "GeometryCollection"

/////////////////////////////////////////////////////
// GeometryCollectionFactory

UGeometryCollectionFactory::UGeometryCollectionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UGeometryCollection::StaticClass();
}

UGeometryCollection* UGeometryCollectionFactory::StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return static_cast<UGeometryCollection*>(NewObject<UGeometryCollection>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone));
}

UObject* UGeometryCollectionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	FTransform LastTransform = FTransform::Identity;
	TArray< TPair<const UStaticMesh *, FTransform> > StaticMeshList;
	TArray< TPair<const USkeletalMesh *, FTransform> > SkeletalMeshList;

	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetAsset()->IsA<UStaticMesh>())
		{
			StaticMeshList.Add(TPair<const UStaticMesh *, FTransform>(static_cast<const UStaticMesh *>(AssetData.GetAsset()), FTransform()));
		}
		else if (AssetData.GetAsset()->IsA<USkeletalMesh>())
		{
			SkeletalMeshList.Add(TPair<const USkeletalMesh *, FTransform>(static_cast<const USkeletalMesh *>(AssetData.GetAsset()), FTransform()));
		}
	}

	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AActor* Actor = Cast<AActor>(*Iter))
			{
				TArray<UStaticMeshComponent *> StaticMeshComponents;
				Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

				if (StaticMeshComponents.Num() > 0)
				{
					for (int Index = 0; Index < StaticMeshComponents.Num(); Index++)
					{
						if (StaticMeshComponents[Index]->GetStaticMesh())
						{
							StaticMeshList.Add(TPair<const UStaticMesh *, FTransform>(
								StaticMeshComponents[Index]->GetStaticMesh(),
								Actor->GetTransform()));
						}
					}
				}

				TArray < USkeletalMeshComponent * > SkeletalMeshComponents;
				Actor->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);

				if (SkeletalMeshComponents.Num() > 0)
				{
					for (int Index = 0; Index < SkeletalMeshComponents.Num(); Index++)
					{
						if (SkeletalMeshComponents[Index]->SkeletalMesh)
						{
							SkeletalMeshList.Add(TPair<const USkeletalMesh *, FTransform>(
								SkeletalMeshComponents[Index]->SkeletalMesh,
								Actor->GetTransform()));
						}
					}
				}

				if (SelectedActors->GetBottom<AActor>() == Actor)
				{
					LastTransform = Actor->GetTransform();
				}
			}
		}
	}

	UGeometryCollection* NewGeometryCollection = StaticFactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);

	for (TPair<const UStaticMesh *, FTransform> & StaticMeshData : StaticMeshList)
	{
		FGeometryCollectionConversion::AppendStaticMesh(StaticMeshData.Key, StaticMeshData.Value, NewGeometryCollection, false);
	}

	for (TPair<const USkeletalMesh *, FTransform> & SkeletalMeshData : SkeletalMeshList)
	{
		FGeometryCollectionConversion::AppendSkeletalMesh(SkeletalMeshData.Key, SkeletalMeshData.Value, NewGeometryCollection, false);
	}

	// Add internal material and selection material
	NewGeometryCollection->AppendStandardMaterials();

	NewGeometryCollection->ReindexMaterialSections();
	GeometryCollectionAlgo::PrepareForSimulation(NewGeometryCollection->GetGeometryCollection().Get());

	// Initial pivot : 
	// Offset everything from the last selected element so the transform will align with the null space. 
	if (TSharedPtr<FGeometryCollection> Collection = NewGeometryCollection->GetGeometryCollection())
	{
		TManagedArray<FGeometryCollectionBoneNode> & Bone = *Collection->BoneHierarchy;
		TManagedArray<FTransform>& Transform = *Collection->Transform;

		for(int TransformGroupIndex =0; TransformGroupIndex<Collection->Transform->Num(); TransformGroupIndex++)
		{
			if (Bone[TransformGroupIndex].Parent == FGeometryCollection::Invalid)
			{
				Transform[TransformGroupIndex] = Transform[TransformGroupIndex].GetRelativeTransform(LastTransform);
			}
		}
	}

	NewGeometryCollection->Modify();
	return NewGeometryCollection;
}

#undef LOCTEXT_NAMESPACE




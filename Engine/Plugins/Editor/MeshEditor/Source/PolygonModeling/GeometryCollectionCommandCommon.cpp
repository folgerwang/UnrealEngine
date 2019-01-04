// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionCommandCommon.h"
#include "IMeshEditorModeEditingContract.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "PackageTools.h"
#include "Layers/ILayers.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "EditableMeshFactory.h"
#include "EditorSupportDelegates.h"
#include "MeshFractureSettings.h"
#include "AssetRegistryModule.h"
#include "GeometryCollection/GeometryCollectionFactory.h"
#include "GeometryCollection/GeometryCollectionConversion.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionFactory.h"
#include "AssetToolsModule.h"
#include "FractureToolDelegates.h"

#define LOCTEXT_NAMESPACE "LogGeometryCommandCommon"

DEFINE_LOG_CATEGORY(LogGeometryCommandCommon);

namespace CommandCommon
{
	static TArray<AActor*> GetSelectedActors()
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		TArray<AActor*> Actors;
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AActor* Actor = Cast<AActor>(*Iter);
			if (Actor)
			{
				Actors.Add(Actor);
			}
		}
		return Actors;
	}

	static ULevel* GetSelectedLevel()
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		TArray<ULevel*> UniqueLevels;
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AActor* Actor = Cast<AActor>(*Iter);
			if (Actor)
			{
				UniqueLevels.AddUnique(Actor->GetLevel());
			}
		}
		check(UniqueLevels.Num() == 1);
		return UniqueLevels[0];
	}

	static AActor* AddActor(ULevel* InLevel, UClass* Class)
	{
		check(Class);

		UWorld* World = InLevel->OwningWorld;
		ULevel* DesiredLevel = InLevel;

		// Transactionally add the actor.
		AActor* Actor = NULL;
		{
			FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "AddActor", "Add Actor"));

			FActorSpawnParameters SpawnInfo;
			SpawnInfo.OverrideLevel = DesiredLevel;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags = RF_Transactional;
			const auto Location = FVector(0);
			const auto Rotation = FTransform(FVector(0)).GetRotation().Rotator();
			Actor = World->SpawnActor(Class, &Location, &Rotation, SpawnInfo);

			check(Actor);
			Actor->InvalidateLightingCache();
			Actor->PostEditMove(true);
		}

		// If this actor is part of any layers (set in its default properties), add them into the visible layers list.
		GEditor->Layers->SetLayersVisibility(Actor->Layers, true);

		// Clean up.
		Actor->MarkPackageDirty();
		ULevel::LevelDirtiedEvent.Broadcast();

		return Actor;
	}

} // namespace CommandCommon


AGeometryCollectionActor* FGeometryCollectionCommandCommon::CreateNewGeometryActor(const FString& Name, const FTransform& Transform, UEditableMesh* SourceMesh)
{
	// create an asset package first
	FString NewPackageName = FPackageName::FilenameToLongPackageName(FPaths::ProjectContentDir() + Name);

	FString UniquePackageName;
	FString UniqueAssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(NewPackageName, TEXT(""), UniquePackageName, UniqueAssetName);

	UPackage* Package = CreatePackage(NULL, *UniquePackageName);
	UGeometryCollection* GeometryCollection = static_cast<UGeometryCollection*>(
		UGeometryCollectionFactory::StaticFactoryCreateNew(UGeometryCollection::StaticClass(), Package,
			FName(*UniqueAssetName), RF_Standalone | RF_Public, NULL, GWarn));

	// Create the new Geometry Collection actor
	AGeometryCollectionActor* NewActor = Cast<AGeometryCollectionActor>(CommandCommon::AddActor(CommandCommon::GetSelectedLevel(), AGeometryCollectionActor::StaticClass()));
	check(NewActor->GetGeometryCollectionComponent());

	// Set the Geometry Collection asset in the new actor
	NewActor->GetGeometryCollectionComponent()->SetRestCollection(GeometryCollection);

	// copy transform of original static mesh actor to this new actor
	NewActor->SetActorLabel(Name);
	NewActor->SetActorTransform(Transform);

	// copy the original material(s) across
	TPolygonGroupAttributesConstRef<FName> MaterialSlotNames = SourceMesh->GetMeshDescription()->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::MaterialAssetName);

	int CurrSlot = 0;
	if (MaterialSlotNames.GetNumElements() > 0)
	{
		for (const FPolygonGroupID PolygonGroupID : SourceMesh->GetMeshDescription()->PolygonGroups().GetElementIDs())
		{
			FString MaterialName = MaterialSlotNames[PolygonGroupID].ToString();
			UMaterialInterface* OriginalMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialName);

			if (OriginalMaterial)
			{			
				// sync materials on the UObject
				GeometryCollection->Materials.Add(OriginalMaterial);
			}
		}

		// add slot for internal and selection materials
		// #todo(dmp): support arbitrary internal materials and a good interface
		GeometryCollection->AppendStandardMaterials();		

		// set materials on the component
		NewActor->GetGeometryCollectionComponent()->InitializeMaterials(GeometryCollection->Materials, GeometryCollection->GetInteriorMaterialIndex(), GeometryCollection->GetBoneSelectedMaterialIndex());
	}

	// Mark relevant stuff dirty
	FAssetRegistryModule::AssetCreated(GeometryCollection);
	GeometryCollection->MarkPackageDirty();
	Package->SetDirtyFlag(true);

	return NewActor;
}

void FGeometryCollectionCommandCommon::RemoveActor(AActor* Actor)
{
	UWorld* World = CommandCommon::GetSelectedLevel()->OwningWorld;
	GEditor->SelectActor(Actor, false, true);
	bool ItWorked = World->DestroyActor(Actor, true, true);
}


void FGeometryCollectionCommandCommon::LogHierarchy(const UGeometryCollection* GeometryCollectionObject)
{
	if (GeometryCollectionObject)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{

			UE_LOG(LogGeometryCommandCommon, Log, TEXT("Sizes: VerticesGroup %d, FacesGroup %d, GeometryGroup %d, TransformGroup %d"),
				GeometryCollection->NumElements(FGeometryCollection::VerticesGroup),
				GeometryCollection->NumElements(FGeometryCollection::FacesGroup),
				GeometryCollection->NumElements(FGeometryCollection::GeometryGroup),
				GeometryCollection->NumElements(FGeometryCollection::TransformGroup));

			const TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
			const TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
			const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> > HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
			const TSharedRef<TManagedArray<FTransform> > TransformsArray = GeometryCollection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
			const TSharedRef<TManagedArray<FString> > BoneNamesArray = GeometryCollection->GetAttribute<FString>("BoneName", FGeometryCollection::TransformGroup);

			const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;
			const TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
			const TManagedArray<FTransform>& Transforms = *TransformsArray;
			const TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
			const TManagedArray<FString>& BoneNames = *BoneNamesArray;

			for (int BoneIndex = 0; BoneIndex < Hierarchy.Num(); BoneIndex++)
			{
				const FTransform& Transform = Transforms[BoneIndex];
				const FVector& LocalLocation = ExplodedTransforms[BoneIndex].GetLocation();

				UE_LOG(LogGeometryCommandCommon, Log, TEXT("Location %3.2f, %3.2f, %3.2f"), Transform.GetLocation().X, Transform.GetLocation().Y, Transform.GetLocation().Z);
				UE_LOG(LogGeometryCommandCommon, Log, TEXT("Scaling %3.2f, %3.2f, %3.2f"), Transform.GetScale3D().X, Transform.GetScale3D().Y, Transform.GetScale3D().Z);
				UE_LOG(LogGeometryCommandCommon, Log, TEXT("Local Location %3.2f, %3.2f, %3.2f"), LocalLocation.X, LocalLocation.Y, LocalLocation.Z);

				const FVector& Vector = ExplodedVectors[BoneIndex];
				UE_LOG(LogGeometryCommandCommon, Log, TEXT("BoneID %d, Name %s, Level %d, IsGeometry %d, ParentBoneID %d, Offset (%3.2f, %3.2f, %3.2f), Vector (%3.2f, %3.2f, %3.2f)"),
					BoneIndex, BoneNames[BoneIndex].GetCharArray().GetData(), Hierarchy[BoneIndex].Level, Hierarchy[BoneIndex].IsGeometry(), Hierarchy[BoneIndex].Parent, LocalLocation.X, LocalLocation.Y, LocalLocation.Z, Vector.X, Vector.Y, Vector.Z);

				for (const int32 & ChildIndex : Hierarchy[BoneIndex].Children)
				{
					UE_LOG(LogGeometryCommandCommon, Log, TEXT("..ChildBoneID %d"), ChildIndex);
				}

				check((Hierarchy[BoneIndex].Children.Num() > 0) == Hierarchy[BoneIndex].IsTransform());

			}
		}
	}
}

void FGeometryCollectionCommandCommon::UpdateExplodedView(class IMeshEditorModeEditingContract &MeshEditorMode, EViewResetType ResetType)
{
	// Update the exploded view in the UI based on the current exploded view slider position
	FFractureToolDelegates::Get().OnUpdateExplodedView.Broadcast(static_cast<uint8>(ResetType), static_cast<uint8>(MeshEditorMode.GetFractureSettings()->CommonSettings->ViewMode));

	FFractureToolDelegates::Get().OnComponentsUpdated.Broadcast();
}

UGeometryCollectionComponent* FGeometryCollectionCommandCommon::GetGeometryCollectionComponent(UEditableMesh* SourceMesh)
{
	check(SourceMesh);
	UGeometryCollectionComponent* GeometryCollectionComponent = nullptr;
	AActor* Actor = GetEditableMeshActor(SourceMesh);
	AGeometryCollectionActor* GeometryCollectionActor = Cast<AGeometryCollectionActor>(Actor);
	if (GeometryCollectionActor)
	{
		GeometryCollectionComponent = GeometryCollectionActor->GetGeometryCollectionComponent();
	}

	return GeometryCollectionComponent;
}

class UStaticMesh* FGeometryCollectionCommandCommon::GetStaticMesh(UEditableMesh* SourceMesh)
{
	check(SourceMesh);
	const FEditableMeshSubMeshAddress& SubMeshAddress = SourceMesh->GetSubMeshAddress();
	return Cast<UStaticMesh>(static_cast<UObject*>(SubMeshAddress.MeshObjectPtr));
}

AActor* FGeometryCollectionCommandCommon::GetEditableMeshActor(UEditableMesh* EditableMesh)
{
	AActor* ReturnActor = nullptr;
	const TArray<AActor*>& SelectedActors = CommandCommon::GetSelectedActors();

	for (int i = 0; i < SelectedActors.Num(); i++)
	{
		TArray<UActorComponent*> PrimitiveComponents = SelectedActors[i]->GetComponentsByClass(UPrimitiveComponent::StaticClass());
		for (UActorComponent* PrimitiveActorComponent : PrimitiveComponents)
		{
			UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(PrimitiveActorComponent);
			FEditableMeshSubMeshAddress SubMeshAddress = UEditableMeshFactory::MakeSubmeshAddress(Component, 0);

			if (EditableMesh->GetSubMeshAddress() == SubMeshAddress)
			{
				ReturnActor = Component->GetOwner();
				break;
			}
		}
	}

	return ReturnActor;
}

UEditableMesh* FGeometryCollectionCommandCommon::GetEditableMeshForActor(AActor* Actor, TArray<UEditableMesh *>& SelectedMeshes)
{
	check(Actor);
	TArray<UActorComponent*> PrimitiveComponents = Actor->GetComponentsByClass(UPrimitiveComponent::StaticClass());
	for (UActorComponent* PrimitiveActorComponent : PrimitiveComponents)
	{
		UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(PrimitiveActorComponent);
		FEditableMeshSubMeshAddress SubMeshAddress = UEditableMeshFactory::MakeSubmeshAddress(Component, 0);

		for (UEditableMesh* EditableMesh : SelectedMeshes)
		{
			if (EditableMesh->GetSubMeshAddress() == SubMeshAddress)
			{
				return EditableMesh;
			}
		}
	}

	return nullptr;
}

UPackage* FGeometryCollectionCommandCommon::CreateGeometryCollectionPackage(UGeometryCollection*& GeometryCollection)
{
	UPackage* Package = CreatePackage(NULL, TEXT("/Game/GeometryCollectionAsset"));
	GeometryCollection = static_cast<UGeometryCollection*>(
		UGeometryCollectionFactory::StaticFactoryCreateNew(UGeometryCollection::StaticClass(), Package,
			FName("GeometryCollectionAsset"), RF_Standalone | RF_Public, NULL, GWarn));		
	return Package;
}

void FGeometryCollectionCommandCommon::AddSingleRootNodeIfRequired(UGeometryCollection* GeometryCollectionObject)
{
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(GeometryCollection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(GeometryCollection);
		}
	}
}

void FGeometryCollectionCommandCommon::AddAdditionalAttributesIfRequired(UGeometryCollection* GeometryCollectionObject)
{
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		if (!GeometryCollection->HasAttribute("ExplodedVector", FGeometryCollection::TransformGroup))
		{
			GeometryCollection->AddAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
			GeometryCollection->AddAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
		}
	}
}

int FGeometryCollectionCommandCommon::GetRootBone(const UGeometryCollection* GeometryCollectionObject)
{
	// Note - it is possible for their to be 2 roots briefly since FGeometryCollectionConversion::AppendStaticMesh puts new
	// geometry at the root, but this is very quickly fixed up in those situations, see AppendMeshesToGeometryCollection
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		TArray<int32> RootBones;
		FGeometryCollectionClusteringUtility::GetRootBones(GeometryCollection, RootBones);
		return RootBones[0];
	}
	check(false);
	return -1;
}

void FGeometryCollectionCommandCommon::AppendMeshesToGeometryCollection(TArray<UEditableMesh*>& SelectedMeshes, UEditableMesh* SourceMesh, FTransform &SourceActorTransform, UGeometryCollection* GeometryCollectionObject, bool DeleteSourceMesh, TArray<int32>& OutNewNodeElements)
{
	if (GeometryCollectionObject)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{

			AddAdditionalAttributesIfRequired(GeometryCollectionObject);

			for (UEditableMesh* EditableMesh : SelectedMeshes)
			{
				// don't want to add duplicate of itself
				if (EditableMesh == SourceMesh)
					continue;
				UStaticMesh* StaticMesh = GetStaticMesh(EditableMesh);
				AActor* MeshActor = GetEditableMeshActor(EditableMesh);
				FTransform MeshTransform = MeshActor->GetTransform();

				MeshTransform.SetLocation(MeshTransform.GetLocation() - SourceActorTransform.GetLocation());
				//this should be Parent relative transform
				FGeometryCollectionConversion::AppendStaticMesh(StaticMesh, MeshTransform, GeometryCollectionObject, false);

				// fix up the additional information required by fracture UI slider
				TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
				TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
				TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
				TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

				int LastElement = GeometryCollection->NumElements(FGeometryCollection::TransformGroup) - 1;
				ExplodedVectors[LastElement] = MeshTransform.GetLocation();
				ExplodedTransforms[LastElement] = MeshTransform;
				TManagedArray<FString>& BoneName = *GeometryCollection->BoneName;
				BoneName[LastElement] = "Root";

				OutNewNodeElements.Add(LastElement);

				if (DeleteSourceMesh)
				{
					RemoveActor(MeshActor);
				}
			}

			GeometryCollection->ReindexMaterials();
		}
	}
}

void FGeometryCollectionCommandCommon::MergeSelections(const UGeometryCollectionComponent* SourceComponent, const TArray<int32>& SelectionB, TArray<int32>& MergedSelectionOut)
{
	if (SourceComponent)
	{
		for (int32 NewElement : SourceComponent->GetSelectedBones())
		{
			MergedSelectionOut.AddUnique(NewElement);
		}
	}

	for (int32 NewElement : SelectionB)
	{
		MergedSelectionOut.AddUnique(NewElement);
	}
}

void FGeometryCollectionCommandCommon::GetCenterOfBone(UGeometryCollection* GeometryCollectionObject, int Element, FVector& CentreOut)
{
	if (GeometryCollectionObject)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{
			TArray<FTransform> Transforms;
			GeometryCollectionAlgo::GlobalMatrices(GeometryCollection, Transforms);
			check(GeometryCollection);
			const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

			FVector SumCOM(0, 0, 0);
			int Count = 0;
			CombineCenterOfGeometryRecursive(Transforms, Hierarchy, Element, SumCOM, Count);

			if (Count > 0)
			{
				SumCOM /= Count;
			}

			CentreOut = SumCOM;
		}
	}
}

void FGeometryCollectionCommandCommon::CombineCenterOfGeometryRecursive(TArray<FTransform>& Transforms, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, int Element, FVector& SumCOMOut, int& CountOut)
{
	if (Hierarchy[Element].IsGeometry())
	{
		SumCOMOut += Transforms[Element].GetLocation();
		CountOut++;
	}

	for (int ChildElement : Hierarchy[Element].Children)
	{
		CombineCenterOfGeometryRecursive(Transforms, Hierarchy, ChildElement, SumCOMOut, CountOut);
	}
}

TArray<AActor*> FGeometryCollectionCommandCommon::GetSelectedActors()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

#undef LOCTEXT_NAMESPACE

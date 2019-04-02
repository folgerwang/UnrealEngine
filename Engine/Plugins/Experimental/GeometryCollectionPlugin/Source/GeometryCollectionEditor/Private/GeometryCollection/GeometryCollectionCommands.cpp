// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionCommands.h"

#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionEngineUtility.h"

#include "Logging/LogMacros.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"


DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionCommandsLogging, NoLogging, All);

void FGeometryCollectionCommands::ToString(UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				GeometryCollectionAlgo::PrintParentHierarchy(RestCollection->GetGeometryCollection().Get());
			}
		}
	}
}

void FGeometryCollectionCommands::WriteToHeaderFile(const TArray<FString>& Args, UWorld * World)
{
	if (Args.Num() > 0)
	{
		if (USelection* SelectedActors = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
			{
				if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
				{
					FString Name = *Args[0];
					ensure(!Name.IsEmpty());
					FString Path = *Args[1];
					if (Path.IsEmpty())
					{
						Path = "";
					}
					UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... %s %s"), *Name, *Path);

					const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
					RestCollection->GetGeometryCollection().Get()->WriteDataToHeaderFile(Name, Path);

					return;
				}
			}
		}
	}
}

void FGeometryCollectionCommands::WriteToOBJFile(const TArray<FString>& Args, UWorld * World)
{
	if (Args.Num() > 0)
	{
		if (USelection* SelectedActors = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
			{
				if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
				{
					FString Name = *Args[0];
					ensure(!Name.IsEmpty());
					FString Path = *Args[1];
					if (Path.IsEmpty())
					{
						Path = "";
					}
					UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... %s %s"), *Name, *Path);

					const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
					RestCollection->GetGeometryCollection().Get()->WriteDataToOBJFile(Name, Path);

					return;
				}
			}
		}
	}
}

void FGeometryCollectionCommands::PrintStatistics(UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				const FGeometryCollection* GeometryCollection = RestCollection->GetGeometryCollection().Get();

				GeometryCollectionAlgo::PrintStatistics(GeometryCollection);
				return;
			}
		}
	}
}

void FGeometryCollectionCommands::PrintDetailedStatistics(UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				const FGeometryCollection* GeometryCollection = RestCollection->GetGeometryCollection().Get();
				const UGeometryCollectionCache* Cache = Actor->GetGeometryCollectionComponent()->CacheParameters.TargetCache;

				GeometryCollectionEngineUtility::PrintDetailedStatistics(GeometryCollection, Cache);
				return;
			}
		}
	}
}

void FGeometryCollectionCommands::DeleteCoincidentVertices(const TArray<FString>& Args, UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				float Tol = 1e-2;
				if (Args.Num() > 0)
					Tol = FCString::Atof(*Args[0]);
				ensure(Tol > 0.f);
				UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... %f"), Tol);

				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				GeometryCollectionAlgo::DeleteCoincidentVertices(RestCollection->GetGeometryCollection().Get(), Tol);
			}
		}
	}
}

void FGeometryCollectionCommands::DeleteZeroAreaFaces(const TArray<FString>& Args, UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				float Tol = 1e-4;
				if (Args.Num() > 0)
					Tol = FCString::Atof(*Args[0]);
				ensure(Tol > 0.f);
				UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... %f"), Tol);

				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				GeometryCollectionAlgo::DeleteZeroAreaFaces(RestCollection->GetGeometryCollection().Get(), Tol);
			}
		}
	}
}

void FGeometryCollectionCommands::DeleteHiddenFaces(UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				GeometryCollectionAlgo::DeleteHiddenFaces(RestCollection->GetGeometryCollection().Get());
			}
		}
	}
}

void FGeometryCollectionCommands::DeleteStaleVertices(UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				GeometryCollectionAlgo::DeleteStaleVertices(RestCollection->GetGeometryCollection().Get());
			}
		}
	}
}

int32 FGeometryCollectionCommands::EnsureSingleRoot(UGeometryCollection* RestCollection)
{
	if (RestCollection)
	{
		TManagedArray<FTransform>& Transform = *RestCollection->GetGeometryCollection()->Transform;
		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *RestCollection->GetGeometryCollection()->BoneHierarchy;
		int32 NumElements = RestCollection->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup);
		if (GeometryCollectionAlgo::HasMultipleRoots(RestCollection->GetGeometryCollection().Get()))
		{
			TArray<int32> RootIndices;
			for (int32 Index = 0; Index < NumElements; Index++)
			{
				if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
				{
					RootIndices.Add(Index);
				}
			}
			int32 RootIndex = RestCollection->GetGeometryCollection()->AddElements(1, FGeometryCollection::TransformGroup);
			Transform[RootIndex].SetTranslation(GeometryCollectionAlgo::AveragePosition(RestCollection->GetGeometryCollection().Get(), RootIndices));
			GeometryCollectionAlgo::ParentTransforms(RestCollection->GetGeometryCollection().Get(), RootIndex, RootIndices);
			return RootIndex;
		}
		else
		{
			for (int32 Index = 0; Index < NumElements; Index++)
			{
				if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
				{
					return Index;
				}
			}
		}
		check(false);
	}
	return -1;
}



void SplitAcrossYZPlaneRecursive(uint32 RootIndex, const FTransform & ParentTransform, UGeometryCollection* Collection)
{
	TSet<uint32> RootIndices;
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *Collection->GetGeometryCollection()->BoneHierarchy;
	TManagedArray<FTransform>& Transform = *Collection->GetGeometryCollection()->Transform;

	TArray<int32> SelectedBonesA, SelectedBonesB;
	for (auto& ChildIndex : Hierarchy[RootIndex].Children)
	{
		if (Hierarchy[ChildIndex].Children.Num())
		{
			SplitAcrossYZPlaneRecursive(ChildIndex, ParentTransform, Collection);
		}

		FVector Translation = (Transform[ChildIndex]*ParentTransform).GetTranslation();
		UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... [%d] global:(%3.5f,%3.5f,%3.5f) local:(%3.5f,%3.5f,%3.5f)"),
			ChildIndex, Translation.X, Translation.Y, Translation.Z, Transform[ChildIndex].GetTranslation().X,
			Transform[ChildIndex].GetTranslation().Y, Transform[ChildIndex].GetTranslation().Z );

		if (Translation.X > 0.f)
		{
			SelectedBonesA.Add(ChildIndex);
		}
		else
		{
			SelectedBonesB.Add(ChildIndex);
		}
	}

	if (SelectedBonesB.Num() && SelectedBonesA.Num())
	{
		int32 BoneAIndex = Collection->GetGeometryCollection()->AddElements(1, FGeometryCollection::TransformGroup);
		GeometryCollectionAlgo::ParentTransform(Collection->GetGeometryCollection().Get(), RootIndex, BoneAIndex);
		Transform[BoneAIndex].SetTranslation(GeometryCollectionAlgo::AveragePosition(Collection->GetGeometryCollection().Get(), SelectedBonesA));
		GeometryCollectionAlgo::ParentTransforms(Collection->GetGeometryCollection().Get(), BoneAIndex, SelectedBonesA);

		int32 BoneBIndex = Collection->GetGeometryCollection()->AddElements(1, FGeometryCollection::TransformGroup);
		GeometryCollectionAlgo::ParentTransform(Collection->GetGeometryCollection().Get(), RootIndex, BoneBIndex);
		Transform[BoneBIndex].SetTranslation(GeometryCollectionAlgo::AveragePosition(Collection->GetGeometryCollection().Get(), SelectedBonesB));
		GeometryCollectionAlgo::ParentTransforms(Collection->GetGeometryCollection().Get(), BoneBIndex, SelectedBonesB);
	}
}

void FGeometryCollectionCommands::SplitAcrossYZPlane(UWorld * World)
{
	UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("FGeometryCollectionCommands::SplitAcrossXZPlane"));
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				FGeometryCollectionEdit RestCollectionEdit = Actor->GetGeometryCollectionComponent()->EditRestCollection();
				UGeometryCollection* RestCollection = RestCollectionEdit.GetRestCollection();
				ensure(RestCollection);

				FGeometryCollectionCommands::EnsureSingleRoot(RestCollection);

				TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *RestCollection->GetGeometryCollection()->BoneHierarchy;
				for (int32 Index = 0; Index < Hierarchy.Num(); Index++)
				{
					if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
					{
						SplitAcrossYZPlaneRecursive(Index, Actor->GetTransform(), RestCollection);
					}
				}

				// post update all dependent actors.
				for (TActorIterator<AGeometryCollectionActor> ActorItr(Actor->GetWorld(), AGeometryCollectionActor::StaticClass()); ActorItr; ++ActorItr)
				{
					if (AGeometryCollectionActor* LocalActor = Cast<AGeometryCollectionActor>(*ActorItr))
					{
						if (LocalActor->GetGeometryCollectionComponent()->GetRestCollection() == RestCollection)
						{
							UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("...%s"), *LocalActor->GetActorLabel());
							LocalActor->GetGeometryCollectionComponent()->ResetDynamicCollection();
						}
					}
				}
			}
		}

	}
}


void FGeometryCollectionCommands::DeleteGeometry(const TArray<FString>& Args, UWorld* World)
{
	UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("FGeometryCollectionCommands::SplitAcrossXZPlane"));
	if (Args.Num() > 0)
	{
		TArray<FAssetData> SelectedAssets;
		GEditor->GetContentBrowserSelections(SelectedAssets);
		for (const FAssetData& AssetData : SelectedAssets)
		{
			if (AssetData.GetAsset()->IsA<UGeometryCollection>())
			{
				if (UGeometryCollection* Collection = static_cast<UGeometryCollection *>(AssetData.GetAsset()))
				{
					int32 Arg = FCString::Atoi(*Args[0]);
					for (int32 i = 0; i < Args.Num(); i++)
					{
						FString EntryName(Args[i]);
						UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... %s"), *EntryName);

						int32 IndexToRemove = Collection->GetGeometryCollection()->BoneName->Find(EntryName);
						if (0 <= IndexToRemove)
						{
							Collection->RemoveElements(FGeometryCollection::TransformGroup, { IndexToRemove });

							// @todo(MaterialReindexing) Deleteing the materials for now, until we support reindexing. 
							int32 NumMaterials = Collection->NumElements(FGeometryCollection::MaterialGroup);
							TSharedPtr< TArray<int32> > MaterialIndices = GeometryCollectionAlgo::ContiguousArray(NumMaterials);
							Collection->RemoveElements(FGeometryCollection::MaterialGroup, *MaterialIndices);
						}
					}
				}
			}
		}
	}
}

void FGeometryCollectionCommands::SelectAllGeometry(const TArray<FString>& Args, UWorld* World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				FScopedColorEdit ColorEdit = Actor->GetGeometryCollectionComponent()->EditBoneSelection();
				ColorEdit.SelectBones(GeometryCollection::ESelectionMode::AllGeometry);
			}
		}
	}
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

}

void FGeometryCollectionCommands::SelectNone(const TArray<FString>& Args, UWorld* World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				FScopedColorEdit ColorEdit = Actor->GetGeometryCollectionComponent()->EditBoneSelection();
				ColorEdit.SelectBones(GeometryCollection::ESelectionMode::None);
			}
		}
	}
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

}

void FGeometryCollectionCommands::SelectInverseGeometry(const TArray<FString>& Args, UWorld* World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				FScopedColorEdit ColorEdit = Actor->GetGeometryCollectionComponent()->EditBoneSelection();
				ColorEdit.SelectBones(GeometryCollection::ESelectionMode::InverseGeometry);
			}
		}
	}
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

}

void FGeometryCollectionCommands::BuildProximityDatabase(const TArray<FString>& Args, UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();

				if (2 <= RestCollection->GetGeometryCollection().Get()->NumElements(FGeometryCollection::GeometryGroup))
				{
					FGeometryCollectionProximityUtility::UpdateProximity(RestCollection->GetGeometryCollection().Get());
				}
			}
		}
	}
}

void FGeometryCollectionCommands::SetupNestedBoneAsset(UWorld * World)
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetAsset()->IsA<UGeometryCollection>())
		{
			if (UGeometryCollection* Collection = static_cast<UGeometryCollection *>(AssetData.GetAsset()))
			{
				GeometryCollection::SetupNestedBoneCollection(Collection->GetGeometryCollection().Get());
			}
		}
	}
}

void FGeometryCollectionCommands::SetupTwoClusteredCubesAsset(UWorld * World)
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetAsset()->IsA<UGeometryCollection>())
		{
			if (UGeometryCollection* Collection = static_cast<UGeometryCollection *>(AssetData.GetAsset()))
			{
				GeometryCollection::SetupTwoClusteredCubesCollection(Collection->GetGeometryCollection().Get());
			}
		}
	}
}



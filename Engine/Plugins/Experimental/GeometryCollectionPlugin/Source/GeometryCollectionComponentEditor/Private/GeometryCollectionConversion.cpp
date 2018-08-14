// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionConversion.h"

#include "AssetRegistryModule.h"
#include "AnimationRuntime.h"
#include "Async/ParallelFor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection.h"
#include "GeometryCollectionActor.h"
#include "GeometryCollectionAlgo.h"
#include "GeometryCollectionFactory.h"
#include "GeometryCollectionComponent.h"
#include "Logging/LogMacros.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "GeometryCollectionBoneNode.h"
#include "GeometryCollectionClusteringUtility.h"

DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionConversionLogging, Log, All);

void FGeometryCollectionConversion::AppendStaticMesh(const UStaticMesh * StaticMesh, const FTransform & StaticMeshTransform, UGeometryCollection * GeometryCollection)
{
	//UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("FGeometryCollectionConversion::AppendStaticMesh()"));
	check(StaticMesh);
	check(GeometryCollection);

	// @todo : Discuss how to handle multiple LOD's
	if (StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
	{
		FStaticMeshVertexBuffers & VertexBuffer = StaticMesh->RenderData->LODResources[0].VertexBuffers;

		// vertex information
		TManagedArray<FVector>& Vertex = *GeometryCollection->Vertex;
		TManagedArray<FVector>& TangentU = *GeometryCollection->TangentU;
		TManagedArray<FVector>& TangentV = *GeometryCollection->TangentV;
		TManagedArray<FVector>& Normal = *GeometryCollection->Normal;
		TManagedArray<FVector2D>& UV = *GeometryCollection->UV;
		TManagedArray<FLinearColor>& Color = *GeometryCollection->Color;
		TManagedArray<int32>& BoneMap = *GeometryCollection->BoneMap;

		const int32 VertexCount = VertexBuffer.PositionVertexBuffer.GetNumVertices();
		int VertexStart = GeometryCollection->AddElements(VertexCount, UGeometryCollection::VerticesGroup);
		for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			int VertexOffset = VertexStart + VertexIndex;
			Vertex[VertexOffset] = VertexBuffer.PositionVertexBuffer.VertexPosition(VertexIndex);
			BoneMap[VertexOffset] = GeometryCollection->NumElements(UGeometryCollection::TransformGroup);

			TangentU[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.VertexTangentX(VertexIndex);
			TangentV[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.VertexTangentY(VertexIndex);
			Normal[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex);

			// @todo : Support multiple UV's per vertex based on MAX_STATIC_TEXCOORDS
			UV[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, 0);
			if (VertexBuffer.ColorVertexBuffer.GetNumVertices() == VertexCount)
				Color[VertexOffset] = VertexBuffer.ColorVertexBuffer.VertexColor(VertexIndex);
		}

		// Triangle Indices
		TManagedArray<FIntVector>& Indices = *GeometryCollection->Indices;
		TManagedArray<bool>& Visible = *GeometryCollection->Visible;

		FRawStaticIndexBuffer & IndexBuffer = StaticMesh->RenderData->LODResources[0].IndexBuffer;
		FIndexArrayView IndexBufferView = IndexBuffer.GetArrayView();
		const int32 IndicesCount = IndexBuffer.GetNumIndices() / 3;
		int IndicesStart = GeometryCollection->AddElements(IndicesCount, UGeometryCollection::GeometryGroup);
		for (int32 IndicesIndex = 0, StaticIndex = 0; IndicesIndex < IndicesCount; IndicesIndex++, StaticIndex += 3)
		{
			int32 IndicesOffset = IndicesStart + IndicesIndex;
			Indices[IndicesOffset] = FIntVector(
				IndexBufferView[StaticIndex] + VertexStart,
				IndexBufferView[StaticIndex + 1] + VertexStart,
				IndexBufferView[StaticIndex + 2] + VertexStart);
			Visible[IndicesOffset] = true;
		}

		// Geometry transform
		TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;

		int32 TransformIndex = GeometryCollection->AddElements(1, UGeometryCollection::TransformGroup);
		Transform[TransformIndex] = StaticMeshTransform;

		// Bone Hierarchy - Added at root with no common parent
		TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchy = *GeometryCollection->BoneHierarchy;
		BoneHierarchy[TransformIndex].Level = 0;
		BoneHierarchy[TransformIndex].Parent = FGeometryCollectionBoneNode::InvalidBone;
		BoneHierarchy[TransformIndex].StatusFlags = FGeometryCollectionBoneNode::FS_Geometry;
	}
}


void FGeometryCollectionConversion::AppendSkeletalMesh(const USkeletalMesh* SkeletalMesh, const FTransform & SkeletalMeshTransform, UGeometryCollection * GeometryCollection)
{
	//UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("FGeometryCollectionConversion::AppendSkeletalMesh()"));
	check(SkeletalMesh);
	check(GeometryCollection);

	if (USkeleton * Skeleton = SkeletalMesh->Skeleton)
	{
		if (const FSkeletalMeshRenderData * SkelMeshRenderData = SkeletalMesh->GetResourceForRendering())
		{
			if (SkelMeshRenderData->LODRenderData.Num())
			{
				const FSkeletalMeshLODRenderData & SkeletalMeshLODRenderData = SkelMeshRenderData->LODRenderData[0];
				const FSkinWeightVertexBuffer & SkinWeightVertexBuffer = SkeletalMeshLODRenderData.SkinWeightVertexBuffer;

				// @todo : Add support for multiple render sections.
				check(SkeletalMeshLODRenderData.RenderSections.Num() == 1);
				const FSkelMeshRenderSection & RenderSection = SkeletalMeshLODRenderData.RenderSections[0];
				const TArray<FBoneIndexType> & SkeletalBoneMap = RenderSection.BoneMap;

				//
				// The Component transform for each Mesh will become the FTransform that drives
				// its associated VerticesGroup. The Skeleton will contain a nested transform hierarchy
				// that is evaluated using the GetComponentSpaceTransformRefPose. The resulting
				// Transforms array stored in the GeometryCollection will be the same size as
				// the SkeletalBoneMap. Note the @todo: the SkeletalBoneMap is pulled from only
				// the first render section, this will need to be expanded to include all render
				// sections.
				//
				TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;
				int32 TransformBaseIndex = GeometryCollection->AddElements(SkeletalBoneMap.Num(), UGeometryCollection::TransformGroup);
				const FReferenceSkeleton & ReferenceSkeletion = Skeleton->GetReferenceSkeleton();
				const TArray<FTransform> & RestArray = Skeleton->GetRefLocalPoses();
				for (int32 BoneIndex = 0; BoneIndex < SkeletalBoneMap.Num(); BoneIndex++)
				{
					FTransform BoneTransform = FAnimationRuntime::GetComponentSpaceTransformRefPose(ReferenceSkeletion, SkeletalBoneMap[BoneIndex]);
					Transform[TransformBaseIndex + BoneIndex] = BoneTransform;
				}


				//
				// The Triangle Indices
				//
				TManagedArray<FIntVector>& Indices = *GeometryCollection->Indices;
				TManagedArray<bool>& Visible = *GeometryCollection->Visible;

				TArray<uint32> IndexBuffer;
				SkeletalMeshLODRenderData.MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);

				const int32 IndicesCount = IndexBuffer.Num() / 3;
				int NumVertices = GeometryCollection->NumElements(UGeometryCollection::VerticesGroup);
				int IndicesBaseIndex = GeometryCollection->AddElements(IndicesCount, UGeometryCollection::GeometryGroup);
				for (int32 IndicesIndex = 0, StaticIndex = 0; IndicesIndex < IndicesCount; IndicesIndex++, StaticIndex += 3)
				{
					int32 IndicesOffset = IndicesBaseIndex + IndicesIndex;
					Indices[IndicesOffset] = FIntVector(
						IndexBuffer[StaticIndex] + NumVertices,
						IndexBuffer[StaticIndex + 1] + NumVertices,
						IndexBuffer[StaticIndex + 2] + NumVertices);
					Visible[IndicesOffset] = true;
				}

				//
				// Vertex Attributes
				//
				TManagedArray<FVector>& Vertex = *GeometryCollection->Vertex;
				TManagedArray<FVector>& TangentU = *GeometryCollection->TangentU;
				TManagedArray<FVector>& TangentV = *GeometryCollection->TangentV;
				TManagedArray<FVector>& Normal = *GeometryCollection->Normal;
				TManagedArray<FVector2D>& UV = *GeometryCollection->UV;
				TManagedArray<FLinearColor>& Color = *GeometryCollection->Color;
				TManagedArray<int32>& BoneMap = *GeometryCollection->BoneMap;

				const FStaticMeshVertexBuffers & VertexBuffers = SkeletalMeshLODRenderData.StaticVertexBuffers;
				const FPositionVertexBuffer & PositionVertexBuffer = VertexBuffers.PositionVertexBuffer;

				const int32 VertexCount = PositionVertexBuffer.GetNumVertices();
				int VertexBaseIndex = GeometryCollection->AddElements(VertexCount, UGeometryCollection::VerticesGroup);
				for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
				{
					int VertexOffset = VertexBaseIndex + VertexIndex;
					BoneMap[VertexOffset] = -1;
					if (const TSkinWeightInfo<false> * SkinWeightInfo = SkinWeightVertexBuffer.GetSkinWeightPtr<false>(VertexIndex))
					{
						uint8 SkeletalBoneIndex = -1;
						check(SkinWeightInfo->GetRigidWeightBone(SkeletalBoneIndex));
						BoneMap[VertexOffset] = SkeletalBoneIndex + TransformBaseIndex;
						Vertex[VertexOffset] = Transform[BoneMap[VertexOffset]].ToInverseMatrixWithScale().TransformPosition(PositionVertexBuffer.VertexPosition(VertexIndex));
					}
					check(BoneMap[VertexOffset] != -1);
					TangentU[VertexOffset] = VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(VertexIndex);
					TangentV[VertexOffset] = VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(VertexIndex);
					Normal[VertexOffset] = VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex);

					// @todo : Support multiple UV's per vertex based on MAX_STATIC_TEXCOORDS
					UV[VertexOffset] = VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, 0);
					if (VertexBuffers.ColorVertexBuffer.GetNumVertices() == VertexCount)
						Color[VertexOffset] = VertexBuffers.ColorVertexBuffer.VertexColor(VertexIndex);
				}

				// transform based on position of the actor. 
				for (int32 BoneIndex = 0; BoneIndex < SkeletalBoneMap.Num(); BoneIndex++)
				{
					Transform[TransformBaseIndex + BoneIndex] *= SkeletalMeshTransform;
				}
			}
		}
	}
}

void FGeometryCollectionConversion::CreateFromSelectedActorsCommand(UWorld * World)
{
	//UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("FGeometryCollectionConversion::CreateCommand()"));

	TArray< TPair<const UStaticMesh *, FTransform> > StaticMeshList;
	TArray< TPair<const USkeletalMesh *, FTransform> > SkeletalMeshList;

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
			}
		}
	}

	if (StaticMeshList.Num() || SkeletalMeshList.Num())
	{
		UPackage* Package = CreatePackage(NULL, TEXT("/Game/GeometryCollectionAsset"));
		auto GeometryCollectionFactory = NewObject<UGeometryCollectionFactory>();
		UGeometryCollection* GeometryCollection = static_cast<UGeometryCollection*>(
			GeometryCollectionFactory->FactoryCreateNew(UGeometryCollection::StaticClass(), Package,
				FName("GeometryCollectionAsset"), RF_Standalone | RF_Public, NULL, GWarn));

		for (TPair<const UStaticMesh *, FTransform> & StaticMeshData : StaticMeshList)
		{
			FGeometryCollectionConversion::AppendStaticMesh(StaticMeshData.Key, StaticMeshData.Value, GeometryCollection);
		}

		for (TPair<const USkeletalMesh *, FTransform> & SkeletalMeshData : SkeletalMeshList)
		{
			FGeometryCollectionConversion::AppendSkeletalMesh(SkeletalMeshData.Key, SkeletalMeshData.Value, GeometryCollection);
		}


		GeometryCollectionAlgo::PrepareForSimulation(GeometryCollection);

		FAssetRegistryModule::AssetCreated(GeometryCollection);
		GeometryCollection->MarkPackageDirty();
		Package->SetDirtyFlag(true);
	}

}

void FGeometryCollectionConversion::CreateFromSelectedAssetsCommand(UWorld * World)
{
	//UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("FGeometryCollectionConversion::CreateCommand()"));

	TArray< TPair<const UStaticMesh *, FTransform> > StaticMeshList;
	TArray< TPair<const USkeletalMesh *, FTransform> > SkeletalMeshList;

	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetAsset()->IsA<UStaticMesh>())
		{
			UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("Static Mesh Content Browser : %s"), *AssetData.GetClass()->GetName());
			StaticMeshList.Add(TPair<const UStaticMesh *, FTransform>(static_cast<const UStaticMesh *>(AssetData.GetAsset()), FTransform()));
		}
		else if (AssetData.GetAsset()->IsA<USkeletalMesh>())
		{
			UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("Skeletal Mesh Content Browser : %s"), *AssetData.GetClass()->GetName());
			SkeletalMeshList.Add(TPair<const USkeletalMesh *, FTransform>(static_cast<const USkeletalMesh *>(AssetData.GetAsset()), FTransform()));
		}
	}

	if (StaticMeshList.Num() || SkeletalMeshList.Num())
	{
		UPackage* Package = CreatePackage(NULL, TEXT("/Game/GeometryCollectionAsset"));
		auto GeometryCollectionFactory = NewObject<UGeometryCollectionFactory>();
		UGeometryCollection* GeometryCollection = static_cast<UGeometryCollection*>(
			GeometryCollectionFactory->FactoryCreateNew(UGeometryCollection::StaticClass(), Package,
				FName("GeometryCollectionAsset"), RF_Standalone | RF_Public, NULL, GWarn));

		for (TPair<const UStaticMesh *, FTransform> & StaticMeshData : StaticMeshList)
		{
			FGeometryCollectionConversion::AppendStaticMesh(StaticMeshData.Key, StaticMeshData.Value, GeometryCollection);
		}

		for (TPair<const USkeletalMesh *, FTransform> & SkeletalMeshData : SkeletalMeshList)
		{
			FGeometryCollectionConversion::AppendSkeletalMesh(SkeletalMeshData.Key, SkeletalMeshData.Value, GeometryCollection);
		}

		GeometryCollectionAlgo::PrepareForSimulation(GeometryCollection);

		FAssetRegistryModule::AssetCreated(GeometryCollection);
		GeometryCollection->MarkPackageDirty();
		Package->SetDirtyFlag(true);
	}

}

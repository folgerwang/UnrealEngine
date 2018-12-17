// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionConversion.h"

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
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionFactory.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "Logging/LogMacros.h"
#include "Rendering/SkeletalMeshRenderData.h"


DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionConversionLogging, Log, All);

void FGeometryCollectionConversion::AppendStaticMesh(const UStaticMesh * StaticMesh, const FTransform & StaticMeshTransform, UGeometryCollection * GeometryCollectionObject, bool ReindexMaterials)
{
	//UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("FGeometryCollectionConversion::AppendStaticMesh()"));
	check(StaticMesh);
	check(GeometryCollectionObject);
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get();
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
		TManagedArray<FLinearColor>& BoneColor = *GeometryCollection->BoneColor;
		TManagedArray<FString>& BoneName = *GeometryCollection->BoneName;

		const int32 VertexCount = VertexBuffer.PositionVertexBuffer.GetNumVertices();
		int InitialNumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
		int VertexStart = GeometryCollection->AddElements(VertexCount, FGeometryCollection::VerticesGroup);
		for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			int VertexOffset = VertexStart + VertexIndex;
			Vertex[VertexOffset] = VertexBuffer.PositionVertexBuffer.VertexPosition(VertexIndex);
			BoneMap[VertexOffset] = GeometryCollection->NumElements(FGeometryCollection::TransformGroup);

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
		TManagedArray<int32>& MaterialID = *GeometryCollection->MaterialID;
		TManagedArray<int32>& MaterialIndex = *GeometryCollection->MaterialIndex;

		FRawStaticIndexBuffer & IndexBuffer = StaticMesh->RenderData->LODResources[0].IndexBuffer;
		FIndexArrayView IndexBufferView = IndexBuffer.GetArrayView();
		const int32 IndicesCount = IndexBuffer.GetNumIndices() / 3;
		int InitialNumIndices = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
		int IndicesStart = GeometryCollection->AddElements(IndicesCount, FGeometryCollection::FacesGroup);
		for (int32 IndicesIndex = 0, StaticIndex = 0; IndicesIndex < IndicesCount; IndicesIndex++, StaticIndex += 3)
		{
			int32 IndicesOffset = IndicesStart + IndicesIndex;
			Indices[IndicesOffset] = FIntVector(
				IndexBufferView[StaticIndex] + VertexStart,
				IndexBufferView[StaticIndex + 1] + VertexStart,
				IndexBufferView[StaticIndex + 2] + VertexStart);
			Visible[IndicesOffset] = true;
			MaterialID[IndicesOffset] = 0;
			MaterialIndex[IndicesOffset] = IndicesOffset;
		}

		// Geometry transform
		TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;

		int32 TransformIndex1 = GeometryCollection->AddElements(1, FGeometryCollection::TransformGroup);
		Transform[TransformIndex1] = StaticMeshTransform;

		// Bone Hierarchy - Added at root with no common parent
		TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchy = *GeometryCollection->BoneHierarchy;
		BoneHierarchy[TransformIndex1].Level = 0;
		BoneHierarchy[TransformIndex1].Parent = FGeometryCollectionBoneNode::InvalidBone;
		BoneHierarchy[TransformIndex1].StatusFlags = FGeometryCollectionBoneNode::FS_Geometry;

		const FColor RandBoneColor(FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, 255);
		BoneColor[TransformIndex1] = FLinearColor(RandBoneColor);
		BoneName[TransformIndex1] = StaticMesh->GetName();

		// GeometryGroup
		int GeometryIndex = GeometryCollection->AddElements(1, FGeometryCollection::GeometryGroup);

		TManagedArray<int32>& TransformIndex = *GeometryCollection->TransformIndex;
		TManagedArray<FBox>& BoundingBox = *GeometryCollection->BoundingBox;
		TManagedArray<float>& InnerRadius = *GeometryCollection->InnerRadius;
		TManagedArray<float>& OuterRadius = *GeometryCollection->OuterRadius;
		TManagedArray<int32>& VertexStartArray = *GeometryCollection->VertexStart;
		TManagedArray<int32>& VertexCountArray = *GeometryCollection->VertexCount;
		TManagedArray<int32>& FaceStartArray = *GeometryCollection->FaceStart;
		TManagedArray<int32>& FaceCountArray = *GeometryCollection->FaceCount;

		TransformIndex[GeometryIndex] = BoneMap[VertexStart];
		VertexStartArray[GeometryIndex] = InitialNumVertices;
		VertexCountArray[GeometryIndex] = VertexCount;
		FaceStartArray[GeometryIndex] = InitialNumIndices;
		FaceCountArray[GeometryIndex] = IndicesCount;

		FVector Center(0);
		for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart+VertexCount; VertexIndex++)
		{
			Center += Vertex[VertexIndex];
		}
		if (VertexCount) Center /= VertexCount;

		// Inner/Outer edges, bounding box
		BoundingBox[GeometryIndex] = FBox(ForceInitToZero);
		InnerRadius[GeometryIndex] = FLT_MAX;
		OuterRadius[GeometryIndex] = -FLT_MAX;
		for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart+VertexCount; VertexIndex++)
		{
			BoundingBox[GeometryIndex] += Vertex[VertexIndex];

			float Delta = (Center - Vertex[VertexIndex]).Size();
			InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
			OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
		}

		// Inner/Outer centroid
		for (int fdx = IndicesStart; fdx < IndicesStart + IndicesCount; fdx++)
		{
			FVector Centroid(0);
			for (int e = 0; e < 3; e++)
			{
				Centroid += Vertex[Indices[fdx][e]];
			}
			Centroid /= 3;

			float Delta = (Center - Centroid).Size();
			InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
			OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
		}

		// Inner/Outer edges
		for (int fdx = IndicesStart; fdx < IndicesStart + IndicesCount; fdx++)
		{
			for (int e = 0; e < 3; e++)
			{
				int i = e, j = (e + 1) % 3;
				FVector Edge = Vertex[Indices[fdx][i]] + 0.5*(Vertex[Indices[fdx][j]] - Vertex[Indices[fdx][i]]);
				float Delta = (Center - Edge).Size();
				InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
				OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
			}
		}

		// for each material, add a reference in our GeometryCollectionObject
		int CurrIdx = 0;
		UMaterialInterface *CurrMaterial = StaticMesh->GetMaterial(CurrIdx);

		int MaterialStart = GeometryCollectionObject->Materials.Num();
		while (CurrMaterial)
		{
			GeometryCollectionObject->Materials.Add(CurrMaterial);
			CurrMaterial = StaticMesh->GetMaterial(++CurrIdx);
		}

		TManagedArray<FGeometryCollectionSection> & Sections = *GeometryCollection->Sections;
	
		// We make sections that mirror what is in the static mesh.  Note that this isn't explicitly
		// necessary since we reindex after all the meshes are added, but it is a good step to have
		// optimal min/max vertex index right from the static mesh.  All we really need to do is
		// assign material ids and rely on reindexing, in theory
		const TArray<FStaticMeshSection> &StaticMeshSections = StaticMesh->RenderData->LODResources[0].Sections;
		for (const FStaticMeshSection &CurrSection : StaticMeshSections)
		{			
			// create new section
			int32 SectionIndex = GeometryCollection->AddElements(1, FGeometryCollection::MaterialGroup);
						
			Sections[SectionIndex].MaterialID = MaterialStart + CurrSection.MaterialIndex;

			Sections[SectionIndex].FirstIndex = IndicesStart*3 + CurrSection.FirstIndex;
			Sections[SectionIndex].MinVertexIndex = VertexStart + CurrSection.MinVertexIndex;

			Sections[SectionIndex].NumTriangles = CurrSection.NumTriangles;
			Sections[SectionIndex].MaxVertexIndex = VertexStart + CurrSection.MaxVertexIndex;		

			// set the MaterialID for all of the faces
			// note the divide by 3 - the GeometryCollection stores indices in tuples of 3 rather than in a flat array
 			for (int32 i = Sections[SectionIndex].FirstIndex/3; i < Sections[SectionIndex].FirstIndex/3 + Sections[SectionIndex].NumTriangles; ++i)
			{
				MaterialID[i] = SectionIndex;				
			}
		}

		if (ReindexMaterials) {
			GeometryCollection->ReindexMaterials();
		}
	}
}


void FGeometryCollectionConversion::AppendSkeletalMesh(const USkeletalMesh* SkeletalMesh, const FTransform & SkeletalMeshTransform, UGeometryCollection * GeometryCollectionObject, bool ReindexMaterials)
{
	//UE_LOG(UGeometryCollectionConversionLogging, Log, TEXT("FGeometryCollectionConversion::AppendSkeletalMesh()"));
	check(SkeletalMesh);
	if (GeometryCollectionObject)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{

			if (USkeleton * Skeleton = SkeletalMesh->Skeleton)
			{
				if (const FSkeletalMeshRenderData * SkelMeshRenderData = SkeletalMesh->GetResourceForRendering())
				{
					if (SkelMeshRenderData->LODRenderData.Num())
					{
						const FSkeletalMeshLODRenderData & SkeletalMeshLODRenderData = SkelMeshRenderData->LODRenderData[0];
						const FSkinWeightVertexBuffer & SkinWeightVertexBuffer = SkeletalMeshLODRenderData.SkinWeightVertexBuffer;

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
						int32 TransformBaseIndex = GeometryCollection->AddElements(SkeletalBoneMap.Num(), FGeometryCollection::TransformGroup);
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
						TManagedArray<int32>& MaterialID = *GeometryCollection->MaterialID;
						TManagedArray<int32>& MaterialIndex = *GeometryCollection->MaterialIndex;

						TArray<uint32> IndexBuffer;
						SkeletalMeshLODRenderData.MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);

						const int32 IndicesCount = IndexBuffer.Num() / 3;
						int NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
						int InitialNumIndices = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
						int IndicesBaseIndex = GeometryCollection->AddElements(IndicesCount, FGeometryCollection::FacesGroup);
						for (int32 IndicesIndex = 0, StaticIndex = 0; IndicesIndex < IndicesCount; IndicesIndex++, StaticIndex += 3)
						{
							int32 IndicesOffset = IndicesBaseIndex + IndicesIndex;
							Indices[IndicesOffset] = FIntVector(
								IndexBuffer[StaticIndex] + NumVertices,
								IndexBuffer[StaticIndex + 1] + NumVertices,
								IndexBuffer[StaticIndex + 2] + NumVertices);
							Visible[IndicesOffset] = true;
							MaterialID[IndicesOffset] = 0;
							MaterialIndex[IndicesOffset] = IndicesOffset;
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
						TManagedArray<FLinearColor>& BoneColor = *GeometryCollection->BoneColor;
						TManagedArray<FString>& BoneName = *GeometryCollection->BoneName;
						TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchy = *GeometryCollection->BoneHierarchy;

						const FStaticMeshVertexBuffers & VertexBuffers = SkeletalMeshLODRenderData.StaticVertexBuffers;
						const FPositionVertexBuffer & PositionVertexBuffer = VertexBuffers.PositionVertexBuffer;

						const int32 VertexCount = PositionVertexBuffer.GetNumVertices();
						int InitialNumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
						int VertexBaseIndex = GeometryCollection->AddElements(VertexCount, FGeometryCollection::VerticesGroup);
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

						int32 InitialIndex = -1;
						int32 LastParentIndex = -1;
						int32 CurrentLevel = 0;
						for (int32 BoneIndex = 0; BoneIndex < SkeletalBoneMap.Num(); BoneIndex++)
						{
							// transform based on position of the actor. 
							Transform[TransformBaseIndex + BoneIndex] = SkeletalMeshTransform * Transform[TransformBaseIndex + BoneIndex];

							// bone attributes
							BoneName[TransformBaseIndex + BoneIndex] = ReferenceSkeletion.GetBoneName(SkeletalBoneMap[BoneIndex]).ToString();
							const FColor RandBoneColor(FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, 255);
							BoneColor[TransformBaseIndex + BoneIndex] = FLinearColor(RandBoneColor);

							// Bone Hierarchy - Added at root with no common parent
							int32 ParentIndex = ReferenceSkeletion.GetParentIndex(SkeletalBoneMap[BoneIndex]);
							int32 UseParentIndex = ParentIndex + InitialIndex;
							if (LastParentIndex != UseParentIndex)
							{
								LastParentIndex = UseParentIndex;
								CurrentLevel++;
							}
							BoneHierarchy[TransformBaseIndex + BoneIndex].Level = CurrentLevel;
							BoneHierarchy[TransformBaseIndex + BoneIndex].Parent = UseParentIndex;
							BoneHierarchy[TransformBaseIndex + BoneIndex].StatusFlags = FGeometryCollectionBoneNode::FS_Geometry;
						}

						// Geometry Group
						TSharedPtr< TArray<int32> > GeometryIndices = GeometryCollectionAlgo::ContiguousArray(GeometryCollection->NumElements(FGeometryCollection::GeometryGroup));
						GeometryCollection->RemoveDependencyFor(FGeometryCollection::GeometryGroup);
						GeometryCollection->RemoveElements(FGeometryCollection::GeometryGroup, *GeometryIndices);
						::GeometryCollection::AddGeometryProperties(GeometryCollection);

						// for each material, add a reference in our GeometryCollectionObject
						int CurrIdx = 0;
						UMaterialInterface *CurrMaterial = SkeletalMesh->Materials[CurrIdx].MaterialInterface;

						int MaterialStart = GeometryCollectionObject->Materials.Num();
						while (CurrMaterial)
						{
							GeometryCollectionObject->Materials.Add(CurrMaterial);
							CurrMaterial = SkeletalMesh->Materials[++CurrIdx].MaterialInterface;
						}

						const TArray<FSkelMeshRenderSection> &StaticMeshSections = SkeletalMesh->GetResourceForRendering()->LODRenderData[0].RenderSections;

						TManagedArray<FGeometryCollectionSection> & Sections = *GeometryCollection->Sections;

						for (const FSkelMeshRenderSection &CurrSection : StaticMeshSections)
						{
							// create new section
							int32 SectionIndex = GeometryCollection->AddElements(1, FGeometryCollection::MaterialGroup);
						
							Sections[SectionIndex].MaterialID = MaterialStart + CurrSection.MaterialIndex;

							Sections[SectionIndex].FirstIndex = IndicesBaseIndex * 3 + CurrSection.BaseIndex;
							Sections[SectionIndex].MinVertexIndex = VertexBaseIndex + CurrSection.BaseVertexIndex;

							Sections[SectionIndex].NumTriangles = CurrSection.NumTriangles;

							// #todo(dmp): what should we set this to?  SkeletalMesh sections are different
							// but we are resetting this when the re indexing happens
							Sections[SectionIndex].MaxVertexIndex = VertexBaseIndex + CurrSection.NumVertices;

							// set the materialid for all of the faces
							for (int32 i = Sections[SectionIndex].FirstIndex / 3; i < Sections[SectionIndex].FirstIndex / 3 + Sections[SectionIndex].NumTriangles; ++i)
							{
								MaterialID[i] = SectionIndex;
							}
						}

					}
				}
			}

			if (ReindexMaterials) {
				GeometryCollection->ReindexMaterials();
			}
		}
	}
}

void FGeometryCollectionConversion::CreateGeometryCollectionCommand(UWorld * World)
{
	UPackage* Package = CreatePackage(NULL, TEXT("/Game/GeometryCollectionAsset"));
	auto GeometryCollectionFactory = NewObject<UGeometryCollectionFactory>();
	UGeometryCollection* GeometryCollection = static_cast<UGeometryCollection*>(
		GeometryCollectionFactory->FactoryCreateNew(UGeometryCollection::StaticClass(), Package,
			FName("GeometryCollectionAsset"), RF_Standalone | RF_Public, NULL, GWarn));
	FAssetRegistryModule::AssetCreated(GeometryCollection);
	Package->SetDirtyFlag(true);
}



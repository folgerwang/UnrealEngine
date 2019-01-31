// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderStatic.cpp: CPU skinned skeletal mesh rendering code.
=============================================================================*/

#include "SkeletalRenderStatic.h"
#include "EngineStats.h"
#include "Components/SkeletalMeshComponent.h"
#include "SceneManagement.h"
#include "SkeletalRender.h"
#include "Rendering/SkeletalMeshRenderData.h"

FSkeletalMeshObjectStatic::FSkeletalMeshObjectStatic(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshRenderData* InSkelMeshRenderData, ERHIFeatureLevel::Type InFeatureLevel)
	: FSkeletalMeshObject(InMeshComponent, InSkelMeshRenderData, InFeatureLevel)
{
	// create LODs to match the base mesh
	for (int32 LODIndex = 0; LODIndex < InSkelMeshRenderData->LODRenderData.Num(); LODIndex++)
	{
		new(LODs) FSkeletalMeshObjectLOD(InFeatureLevel, InSkelMeshRenderData, LODIndex);
	}

	InitResources(InMeshComponent);
}

FSkeletalMeshObjectStatic::~FSkeletalMeshObjectStatic()
{
}

void FSkeletalMeshObjectStatic::InitResources(USkinnedMeshComponent* InMeshComponent)
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		
		// Skip LODs that have their render data stripped
		if (SkelLOD.SkelMeshRenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			FSkelMeshComponentLODInfo* CompLODInfo = nullptr;
			if (InMeshComponent->LODInfo.IsValidIndex(LODIndex))
			{
				CompLODInfo = &InMeshComponent->LODInfo[LODIndex];
			}

			SkelLOD.InitResources(CompLODInfo);
		}
	}
}

void FSkeletalMeshObjectStatic::ReleaseResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		
		// Skip LODs that have their render data stripped
		if (SkelLOD.SkelMeshRenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			SkelLOD.ReleaseResources();
		}
	}
}

const FVertexFactory* FSkeletalMeshObjectStatic::GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx) const
{
	check(LODs.IsValidIndex(LODIndex));
	return &LODs[LODIndex].VertexFactory; 
}

TArray<FTransform>* FSkeletalMeshObjectStatic::GetComponentSpaceTransforms() const
{
	return nullptr;
}

const TArray<FMatrix>& FSkeletalMeshObjectStatic::GetReferenceToLocalMatrices() const
{
	static TArray<FMatrix> ReferenceToLocalMatrices;
	return ReferenceToLocalMatrices;
}

void FSkeletalMeshObjectStatic::FSkeletalMeshObjectLOD::InitResources(FSkelMeshComponentLODInfo* CompLODInfo)
{
	check(SkelMeshRenderData);
	check(SkelMeshRenderData->LODRenderData.IsValidIndex(LODIndex));

	FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[LODIndex];
	
	FPositionVertexBuffer* PositionVertexBufferPtr = &LODData.StaticVertexBuffers.PositionVertexBuffer;
	FStaticMeshVertexBuffer* StaticMeshVertexBufferPtr = &LODData.StaticVertexBuffers.StaticMeshVertexBuffer;
	
	// If we have a vertex color override buffer (and it's the right size) use it
	if (CompLODInfo &&
		CompLODInfo->OverrideVertexColors &&
		CompLODInfo->OverrideVertexColors->GetNumVertices() == PositionVertexBufferPtr->GetNumVertices())
	{
		ColorVertexBuffer = CompLODInfo->OverrideVertexColors;
	}
	else
	{
		ColorVertexBuffer = &LODData.StaticVertexBuffers.ColorVertexBuffer;
	}

	FLocalVertexFactory* VertexFactoryPtr = &VertexFactory;
	FColorVertexBuffer* ColorVertexBufferPtr = ColorVertexBuffer;

	ENQUEUE_RENDER_COMMAND(InitSkeletalMeshStaticSkinVertexFactory)(
		[VertexFactoryPtr, PositionVertexBufferPtr, StaticMeshVertexBufferPtr, ColorVertexBufferPtr](FRHICommandListImmediate& RHICmdList)
		{
			FLocalVertexFactory::FDataType Data;
			PositionVertexBufferPtr->InitResource();
			StaticMeshVertexBufferPtr->InitResource();
			ColorVertexBufferPtr->InitResource();

			PositionVertexBufferPtr->BindPositionVertexBuffer(VertexFactoryPtr, Data);
			StaticMeshVertexBufferPtr->BindTangentVertexBuffer(VertexFactoryPtr, Data);
			StaticMeshVertexBufferPtr->BindTexCoordVertexBuffer(VertexFactoryPtr, Data);
			ColorVertexBufferPtr->BindColorVertexBuffer(VertexFactoryPtr, Data);

			VertexFactoryPtr->SetData(Data);
			VertexFactoryPtr->InitResource();
		});


#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		check(SkelMeshRenderData);
		check(SkelMeshRenderData->LODRenderData.IsValidIndex(LODIndex));
		FSkeletalMeshLODRenderData& LODModel = SkelMeshRenderData->LODRenderData[LODIndex];
		FVertexBufferRHIRef VertexBufferRHI = LODModel.StaticVertexBuffers.PositionVertexBuffer.VertexBufferRHI;
		FIndexBufferRHIRef IndexBufferRHI = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->IndexBufferRHI;
		uint32 VertexBufferStride = LODModel.StaticVertexBuffers.PositionVertexBuffer.GetStride();

		//#dxr_todo: do we need support for separate sections in FRayTracingGeometryData?
		uint32 TrianglesCount = 0;
		for (int32 SectionIndex = 0; SectionIndex < LODModel.RenderSections.Num(); SectionIndex++)
		{
			const FSkelMeshRenderSection& Section = LODModel.RenderSections[SectionIndex];
			TrianglesCount += Section.NumTriangles;
		}

		TArray<FSkelMeshRenderSection>* RenderSections = &LODModel.RenderSections;
		ENQUEUE_RENDER_COMMAND(InitSkeletalRenderStaticRayTracingGeometry)(
			[this, VertexBufferRHI, IndexBufferRHI, VertexBufferStride, TrianglesCount, RenderSections](FRHICommandListImmediate& RHICmdList)
			{
				FRayTracingGeometryInitializer Initializer;
				Initializer.PositionVertexBuffer = VertexBufferRHI;
				Initializer.IndexBuffer = IndexBufferRHI;
				Initializer.BaseVertexIndex = 0;
				Initializer.VertexBufferStride = VertexBufferStride;
				Initializer.VertexBufferByteOffset = 0;
				Initializer.TotalPrimitiveCount = TrianglesCount;
				Initializer.VertexBufferElementType = VET_Float3;
				Initializer.PrimitiveType = PT_TriangleList;
				Initializer.bFastBuild = false;

				TArray<FRayTracingGeometrySegment> GeometrySections;
				GeometrySections.Reserve(RenderSections->Num());
				for (const FSkelMeshRenderSection& Section : *RenderSections)
				{
					FRayTracingGeometrySegment Segment;
					Segment.FirstPrimitive = Section.BaseIndex / 3;
					Segment.NumPrimitives = Section.NumTriangles;
					GeometrySections.Add(Segment);
				}
				Initializer.Segments = GeometrySections;

				RayTracingGeometry.SetInitializer(Initializer);
				RayTracingGeometry.InitResource();
			}
		);
	}
#endif // RHI_RAYTRACING

	bResourcesInitialized = true;
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectStatic::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	BeginReleaseResource(&VertexFactory);
#if RHI_RAYTRACING
	BeginReleaseResource(&RayTracingGeometry);
#endif // RHI_RAYTRACING
	bResourcesInitialized = false;
}

#if RHI_RAYTRACING
// #dxr_todo: this looks like dead code
void FSkeletalMeshObjectStatic::FSkeletalMeshObjectLOD::BuildRayTracingAccelerationStructure()
{
	if (RayTracingGeometry.Initializer.PositionVertexBuffer && RayTracingGeometry.Initializer.IndexBuffer)
	{
		ENQUEUE_RENDER_COMMAND(SkeletalRenderStaticBuildRayTracingAccelerationStructure)(
			[this](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.BuildAccelerationStructure(RayTracingGeometry.RayTracingGeometryRHI);
		}
		);
	}
}
#endif // RHI_RAYTRACING

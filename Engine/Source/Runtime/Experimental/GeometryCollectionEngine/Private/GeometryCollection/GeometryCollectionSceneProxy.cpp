// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionSceneProxy.h"

#include "Async/ParallelFor.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

DEFINE_LOG_CATEGORY_STATIC(FGeometryCollectionSceneProxyLogging, Log, All);

FGeometryCollectionSceneProxy::FGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, NumVertices(0)
	, NumIndices(0)
	, VertexFactory(GetScene().GetFeatureLevel(), "FGeometryCollectionSceneProxy")
	, DynamicData(nullptr)
	, ConstantData(nullptr)
	, ShowBoneColors(Component->GetShowBoneColors())
	, ShowSelectedBones(Component->GetShowSelectedBones())
	, BoneSelectionMaterialID(Component->GetBoneSelectedMaterialID())
{
	Materials.Empty();
	for (int MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); MaterialIndex++)
	{
		Materials.Push(Component->GetMaterial(MaterialIndex));
		if (Materials[MaterialIndex] == nullptr)
		{
			Materials[MaterialIndex] = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}
}

FGeometryCollectionSceneProxy::~FGeometryCollectionSceneProxy()
{
	ReleaseResources();

	if (DynamicData != nullptr)
	{
		delete DynamicData;
	}

	if (ConstantData != nullptr)
	{
		delete ConstantData;
	}
}

void FGeometryCollectionSceneProxy::InitResources()
{
	check(ConstantData);

	NumVertices = ConstantData->Vertices.Num();
	NumIndices = ConstantData->Indices.Num()*3;

	VertexBuffers.InitWithDummyData(&VertexFactory, GetRequiredVertexCount());

	IndexBuffer.NumIndices = GetRequiredIndexCount();

	BeginInitResource(&IndexBuffer);
}

void FGeometryCollectionSceneProxy::ReleaseResources()
{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
}

void FGeometryCollectionSceneProxy::BuildGeometry( const FGeometryCollectionConstantData* ConstantDataIn, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices)
{
	OutVertices.SetNumUninitialized(ConstantDataIn->Vertices.Num());
	ParallelFor(ConstantData->Vertices.Num(), [&](int32 PointIdx)
	{
		OutVertices[PointIdx] =
			FDynamicMeshVertex(
				ConstantDataIn->Vertices[PointIdx],
				ConstantDataIn->UVs[PointIdx],
				ShowBoneColors||ShowSelectedBones ?
				ConstantDataIn->BoneColors[PointIdx].ToFColor(true) :
				ConstantDataIn->Colors[PointIdx].ToFColor(true)
			);
		OutVertices[PointIdx].SetTangents(ConstantDataIn->TangentU[PointIdx], ConstantDataIn->TangentV[PointIdx], ConstantDataIn->Normals[PointIdx]);
	});

	check(ConstantDataIn->Indices.Num() * 3 == NumIndices);

	OutIndices.SetNumUninitialized(NumIndices);
	ParallelFor (ConstantDataIn->Indices.Num(), [&](int32 IndexIdx)
	{
		OutIndices[IndexIdx * 3 ]    = ConstantDataIn->Indices[IndexIdx].X;
		OutIndices[IndexIdx * 3 + 1] = ConstantDataIn->Indices[IndexIdx].Y;
		OutIndices[IndexIdx * 3 + 2] = ConstantDataIn->Indices[IndexIdx].Z;
	});
}

void FGeometryCollectionSceneProxy::SetConstantData_RenderThread(FGeometryCollectionConstantData* NewConstantData, bool ForceInit)
{
	check(IsInRenderingThread());
	check(NewConstantData);

	if (ConstantData)
	{
		delete ConstantData;
		ConstantData = nullptr;
	}
	ConstantData = NewConstantData;

	if (ConstantData->Vertices.Num() != VertexBuffers.PositionVertexBuffer.GetNumVertices() || ForceInit)
	{
		ReleaseResources();
		InitResources();
	}

	TArray<int32> Indices;
	TArray<FDynamicMeshVertex> Vertices;
	BuildGeometry(ConstantData, Vertices, Indices);
	check(Vertices.Num() == GetRequiredVertexCount());
	check(Indices.Num() == GetRequiredIndexCount());

	if (GetRequiredVertexCount())
	{
		ParallelFor(Vertices.Num(), [&](int32 i)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			VertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
			VertexBuffers.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
		});

		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.ColorVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
			RHIUnlockVertexBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTexCoordSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), VertexBuffer.GetTexCoordSize());
			RHIUnlockVertexBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
		}

		{
			void* IndexBufferData = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
			FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
			RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);
		}

		Sections.Empty();
		for (FGeometryCollectionSection Section : ConstantData->Sections)
		{
			if (Section.NumTriangles > 0)
			{
				FGeometryCollectionSection NewSection;
				NewSection.MaterialID = Section.MaterialID;
				NewSection.FirstIndex = Section.FirstIndex;
				NewSection.NumTriangles = Section.NumTriangles;
				NewSection.MinVertexIndex = Section.MinVertexIndex;
				NewSection.MaxVertexIndex = Section.MaxVertexIndex;
				Sections.Add(NewSection);
			}
		}
	}


}

void FGeometryCollectionSceneProxy::SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData)
{
	check(IsInRenderingThread());
	if (GetRequiredVertexCount())
	{
		if (DynamicData)
		{
			delete DynamicData;
			DynamicData = nullptr;
		}
		DynamicData = NewDynamicData;

		check(VertexBuffers.PositionVertexBuffer.GetNumVertices() == (uint32)ConstantData->Vertices.Num());

		ParallelFor(ConstantData->Vertices.Num(), [&](int32 i)
		{
			VertexBuffers.PositionVertexBuffer.VertexPosition(i) = DynamicData->Transforms[ConstantData->BoneMap[i]].TransformPosition(ConstantData->Vertices[i]);
		});

		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}
	}
}

FMaterialRenderProxy* FGeometryCollectionSceneProxy::GetMaterial(FMeshElementCollector& Collector, int32 MaterialIndex) const
{
	// material for wireframe
	auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
		GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
		FLinearColor(0, 0.5f, 1.f)
	);
	Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

	// material for colored bones
	UMaterial* VertexColorVisualizationMaterial = GEngine->VertexColorMaterial;
	auto VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
		VertexColorVisualizationMaterial->GetRenderProxy(),
		GetSelectionColor(FLinearColor::White, false, false)
	);
	Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);

	FMaterialRenderProxy* MaterialProxy = nullptr;

	if (ShowBoneColors)
	{
		MaterialProxy = VertexColorVisualizationMaterialInstance;
	}
	else
	{
		MaterialProxy = Materials[MaterialIndex]->GetRenderProxy();
	}

	if (MaterialProxy == nullptr)
	{
		MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	}

	return MaterialProxy;
}

void FGeometryCollectionSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GeometryCollectionSceneProxy_GetDynamicMeshElements);
	if (GetRequiredVertexCount())
	{
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			//Grab the material proxies we'll be using for each section
			TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;
			for (int SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
			{
				const FGeometryCollectionSection& Section = Sections[SectionIndex];
				FMaterialRenderProxy* MaterialProxy = GetMaterial(Collector, Section.MaterialID);
				MaterialProxies.Add(MaterialProxy);
			}

			// Render Batches
			for (int SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
			{
				const FGeometryCollectionSection& Section = Sections[SectionIndex];

				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];
					// Draw the mesh.
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &IndexBuffer;
					Mesh.bWireframe = bWireframe;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = MaterialProxies[SectionIndex];
                    
                    // Collector owns the uniform buffer
                    FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
                    DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, UseEditorDepthTest());
                    BatchElement.PrimitiveUniformBuffer = DynamicPrimitiveUniformBuffer.UniformBuffer.GetUniformBufferRHI();
                    
					BatchElement.FirstIndex = Section.FirstIndex;
					BatchElement.NumPrimitives = Section.NumTriangles;
					BatchElement.MinVertexIndex = Section.MinVertexIndex;
					BatchElement.MaxVertexIndex = Section.MaxVertexIndex;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
			}

			// Highlight selected bone using specialized material - when rendering bones as colors we don't need to run this code as the
			// bone selection is already contained in the rendered colors
			if (ShowBoneColors||ShowSelectedBones)
			{
				FMaterialRenderProxy* MaterialRenderProxy = Materials[BoneSelectionMaterialID]->GetRenderProxy();

				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &IndexBuffer;
					Mesh.bWireframe = bWireframe;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = MaterialRenderProxy;
                    
                    FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
                    DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, UseEditorDepthTest());
                    BatchElement.PrimitiveUniformBuffer = DynamicPrimitiveUniformBuffer.UniformBuffer.GetUniformBufferRHI();
                    
					BatchElement.FirstIndex = 0;
					BatchElement.NumPrimitives = GetRequiredIndexCount() / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = GetRequiredVertexCount();
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if(VisibilityMap & (1 << ViewIndex))
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
#endif
		}
	}
}

FPrimitiveViewRelevance FGeometryCollectionSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

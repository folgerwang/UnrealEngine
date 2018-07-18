// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererMeshes.h"
#include "ParticleResources.h"
#include "NiagaraMeshVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"
#include "Async/ParallelFor.h"
#include "Engine/StaticMesh.h"

DECLARE_CYCLE_STAT(TEXT("Generate Mesh Vertex Data"), STAT_NiagaraGenMeshVertexData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Meshes"), STAT_NiagaraRenderMeshes, STATGROUP_Niagara);



extern int32 GbNiagaraParallelEmitterRenderers;




class FNiagaraMeshCollectorResourcesMesh : public FOneFrameResource
{
public:
	FNiagaraMeshVertexFactory VertexFactory;
	FNiagaraMeshUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesMesh()
	{
		VertexFactory.ReleaseResource();
	}
};


NiagaraRendererMeshes::NiagaraRendererMeshes(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) :
	NiagaraRenderer()
	, PositionOffset(INDEX_NONE)
	, VelocityOffset(INDEX_NONE)
	, ColorOffset(INDEX_NONE)
	, ScaleOffset(INDEX_NONE)
	, SizeOffset(INDEX_NONE)
	, MaterialParamOffset(INDEX_NONE)
	, MaterialParamOffset1(INDEX_NONE)
	, MaterialParamOffset2(INDEX_NONE)
	, MaterialParamOffset3(INDEX_NONE)
	, TransformOffset(INDEX_NONE)
	, CustomSortingOffset(INDEX_NONE)
	, LastSyncedId(INDEX_NONE)
{
	//check(InProps);
	VertexFactory = ConstructNiagaraMeshVertexFactory(NVFT_Mesh, FeatureLevel);
	Properties = Cast<UNiagaraMeshRendererProperties>(InProps);


	if (Properties && Properties->ParticleMesh)
	{
		if (Properties->bOverrideMaterials && Properties->OverrideMaterials.Num() != 0)
		{
			for (UMaterialInterface* Interface : Properties->OverrideMaterials)
			{
				if (Interface)
				{
					Interface->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraMeshParticles);
				}
			}
		}

		const FStaticMeshLODResources& LODModel = Properties->ParticleMesh->RenderData->LODResources[0];
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			//FMaterialRenderProxy* MaterialProxy = MeshMaterials[SectionIndex]->GetRenderProxy(bSelected);
			const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
			UMaterialInterface* ParticleMeshMaterial = Properties->ParticleMesh->GetMaterial(Section.MaterialIndex);
			if (ParticleMeshMaterial && Properties->bOverrideMaterials == false)
			{
				if (ParticleMeshMaterial)
				{
					ParticleMeshMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraMeshParticles);
				}
			}
		}

		BaseExtents = Properties->ParticleMesh->GetBounds().BoxExtent;
	}

}

void NiagaraRendererMeshes::SetupVertexFactory(FNiagaraMeshVertexFactory *InVertexFactory, const FStaticMeshLODResources& LODResources) const
{
	FStaticMeshDataType Data;

	LODResources.VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(InVertexFactory, Data);
	LODResources.VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(InVertexFactory, Data);
	LODResources.VertexBuffers.StaticMeshVertexBuffer.BindTexCoordVertexBuffer(InVertexFactory, Data, MAX_TEXCOORDS);
	LODResources.VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(InVertexFactory, Data);
	InVertexFactory->SetData(Data);
}



void NiagaraRendererMeshes::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void NiagaraRendererMeshes::CreateRenderThreadResources()
{
	VertexFactory->InitResource();
}


void NiagaraRendererMeshes::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderMeshes);

	SimpleTimer MeshElementsTimer;

	FNiagaraDynamicDataMesh *DynamicDataMesh = (static_cast<FNiagaraDynamicDataMesh*>(DynamicDataRender));
	//if (!DynamicDataMesh || !DynamicDataMesh->DataSet || DynamicDataMesh->DataSet->GetNumInstances()==0)
	if (!DynamicDataMesh 
		|| DynamicDataMesh->RTParticleData.GetNumInstancesAllocated() == 0
		|| DynamicDataMesh->RTParticleData.GetNumInstances() == 0
		|| nullptr == Properties)
	{
		return;
	}

	int32 NumInstances = DynamicDataMesh->RTParticleData.GetNumInstances();

	int32 TotalFloatSize = DynamicDataMesh->RTParticleData.GetFloatBuffer().Num() / sizeof(float);
	FNiagaraGlobalReadBuffer::FAllocation ParticleData;
	
	//For cpu sims we allocate render buffers from the global pool. GPU sims own their own.
	if (DynamicDataMesh->DataSet->GetSimTarget() == ENiagaraSimTarget::CPUSim)
	{
		ParticleData = FNiagaraGlobalReadBuffer::Get().AllocateFloat(TotalFloatSize);
		FMemory::Memcpy(ParticleData.Buffer, DynamicDataMesh->RTParticleData.GetFloatBuffer().GetData(), DynamicDataMesh->RTParticleData.GetFloatBuffer().Num());
	}

	//check(DynamicDataMesh->DataSet->PrevDataRender().GetNumInstances() > 0);
	//check(DynamicDataMesh->DataSet->PrevDataRender().GetNumInstancesAllocated() > 0);
	//check(DynamicDataMesh->DataSet->PrevData().GetGPUBufferFloat()->NumBytes > 0 || DynamicDataMesh->DataSet->PrevData().GetGPUBufferInt()->NumBytes > 0)
	
	{
		// Update the primitive uniform buffer if needed.
		if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
		{
			FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(
				FMatrix::Identity,
				SceneProxy->GetActorPosition(),
				SceneProxy->GetBounds(),
				SceneProxy->GetLocalBounds(),
				SceneProxy->ReceivesDecals(),
				false,
				false,
				false,
				false,
				SceneProxy->UseEditorDepthTest(),
				SceneProxy->GetLightingChannelMask()
				);
			WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
			WorldSpacePrimitiveUniformBuffer.InitResource();
		}

		// Compute the per-view uniform buffers.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				const FStaticMeshLODResources& LODModel = Properties->ParticleMesh->RenderData->LODResources[0];

				FNiagaraMeshCollectorResourcesMesh& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesMesh>();
				SetupVertexFactory(&CollectorResources.VertexFactory, LODModel);
				FNiagaraMeshUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.LocalToWorld = bLocalSpace ? SceneProxy->GetLocalToWorld() : FMatrix::Identity;//For now just handle local space like this but maybe in future have a VF variant to avoid the transform entirely?
				PerViewUniformParameters.LocalToWorldInverseTransposed = bLocalSpace ? SceneProxy->GetLocalToWorld().Inverse().GetTransposed() : FMatrix::Identity;
				PerViewUniformParameters.PrevTransformAvailable = false;
				PerViewUniformParameters.DeltaSeconds = ViewFamily.DeltaWorldTime;
				PerViewUniformParameters.PositionDataOffset = PositionOffset;
				PerViewUniformParameters.VelocityDataOffset = VelocityOffset;
				PerViewUniformParameters.ColorDataOffset = ColorOffset;
				PerViewUniformParameters.TransformDataOffset = TransformOffset;
				PerViewUniformParameters.ScaleDataOffset = ScaleOffset;
				PerViewUniformParameters.SizeDataOffset = SizeOffset;
				PerViewUniformParameters.MaterialParamDataOffset = MaterialParamOffset;
				PerViewUniformParameters.MaterialParam1DataOffset = MaterialParamOffset1;
				PerViewUniformParameters.MaterialParam2DataOffset = MaterialParamOffset2;
				PerViewUniformParameters.MaterialParam3DataOffset = MaterialParamOffset3;
				PerViewUniformParameters.DefaultPos = bLocalSpace ? FVector4(0.0f, 0.0f, 0.0f, 1.0f) : FVector4(SceneProxy->GetLocalToWorld().GetOrigin());
				/*
				if (Properties)
				{
				PerViewUniformParameters.SubImageSize = FVector4(Properties->SubImageInfo.X, Properties->SubImageInfo.Y, 1.0f / Properties->SubImageInfo.X, 1.0f / Properties->SubImageInfo.Y);
				}
				*/

				//Grab the material proxies we'll be using for each section and check them for translucency.
				TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;
				bool bHasTranslucentMaterials = false;
				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
					UMaterialInterface* ParticleMeshMaterial = Properties->ParticleMesh->GetMaterial(Section.MaterialIndex);
					FMaterialRenderProxy* MaterialProxy = nullptr;

					if (Properties->bOverrideMaterials && Properties->OverrideMaterials.Num() > Section.MaterialIndex &&
						Properties->OverrideMaterials[Section.MaterialIndex] != nullptr)
					{
						MaterialProxy = Properties->OverrideMaterials[Section.MaterialIndex]->GetRenderProxy(false, false);
					}

					if (MaterialProxy == nullptr && ParticleMeshMaterial)
					{
						MaterialProxy = ParticleMeshMaterial->GetRenderProxy(false, false);
					}

					if (MaterialProxy == nullptr)
					{
						MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
					}

					MaterialProxies.Add(MaterialProxy);
					if (MaterialProxy)
					{
						EBlendMode BlendMode = MaterialProxy->GetMaterial(VertexFactory->GetFeatureLevel())->GetBlendMode();
						bHasTranslucentMaterials |= BlendMode == BLEND_AlphaComposite || BlendMode == BLEND_Translucent;
					}
				}

				//Sort particles if needed.
				FNiagaraGlobalReadBuffer::FAllocation SortedIndices;
				CollectorResources.VertexFactory.SetSortedIndices(nullptr, 0xFFFFFFFF);
				if (DynamicDataMesh->DataSet->GetSimTarget() == ENiagaraSimTarget::CPUSim)//TODO: Compute shader for sorting gpu sims and larger cpu sims.
				{
					check(ParticleData.IsValid());
					if (bHasTranslucentMaterials || !Properties->bSortOnlyWhenTranslucent)
					{
						ENiagaraSortMode SortMode = Properties->SortMode;
						bool bCustomSortMode = SortMode == ENiagaraSortMode::CustomAscending || SortMode == ENiagaraSortMode::CustomDecending;
						int32 SortAttributeOffest = bCustomSortMode ? CustomSortingOffset : PositionOffset;
						if (SortMode != ENiagaraSortMode::None && SortAttributeOffest != INDEX_NONE)
						{
							SortedIndices = FNiagaraGlobalReadBuffer::Get().AllocateInt32(NumInstances);
							SortIndices(SortMode, SortAttributeOffest, DynamicDataMesh->RTParticleData, SceneProxy->GetLocalToWorld(), View, SortedIndices);
							CollectorResources.VertexFactory.SetSortedIndices(SortedIndices.ReadBuffer->SRV, SortedIndices.FirstIndex / sizeof(float));
						}
					}
					CollectorResources.VertexFactory.SetParticleData(ParticleData.ReadBuffer->SRV, ParticleData.FirstIndex / sizeof(float), DynamicDataMesh->RTParticleData.GetFloatStride() / sizeof(float));
				}
				else
				{
					CollectorResources.VertexFactory.SetParticleData(DynamicDataMesh->DataSet->CurrData().GetGPUBufferFloat()->SRV, 0, DynamicDataMesh->DataSet->CurrData().GetFloatStride() / sizeof(float));
				}

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
				CollectorResources.VertexFactory.SetParticleFactoryType(NVFT_Mesh);
				CollectorResources.VertexFactory.SetMeshFacingMode((uint32)Properties->FacingMode);
				CollectorResources.UniformBuffer = FNiagaraMeshUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetUniformBuffer(CollectorResources.UniformBuffer);
			
				const bool bIsWireframe = AllowDebugViewmodes() && View->Family->EngineShowFlags.Wireframe;

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
					FMaterialRenderProxy* MaterialProxy = MaterialProxies[SectionIndex];
					if ((Section.NumTriangles == 0) || (MaterialProxy == NULL))
					{
						//@todo. This should never occur, but it does occasionally.
						continue;
					}

					FMeshBatch& Mesh = Collector.AllocateMesh();
					Mesh.VertexFactory = &CollectorResources.VertexFactory;
					Mesh.LCI = NULL;
					Mesh.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
					Mesh.CastShadow = SceneProxy->CastsDynamicShadow();
					Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)SceneProxy->GetDepthPriorityGroup(View);

					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.PrimitiveUniformBufferResource = &WorldSpacePrimitiveUniformBuffer;
					BatchElement.FirstIndex = 0;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = 0;
					BatchElement.NumInstances = NumInstances;
					if (DynamicDataMesh->DataSet->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim)
					{
						BatchElement.IndirectArgsBuffer = DynamicDataMesh->DataSet->GetCurDataSetIndices().Buffer;
					}

					if (bIsWireframe)
					{
						if (LODModel.WireframeIndexBuffer.IsInitialized())
						{
							Mesh.Type = PT_LineList;
							Mesh.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
							BatchElement.FirstIndex = 0;
							BatchElement.IndexBuffer = &LODModel.WireframeIndexBuffer;
							BatchElement.NumPrimitives = LODModel.WireframeIndexBuffer.GetNumIndices() / 2;

						}
						else
						{
							Mesh.Type = PT_TriangleList;
							Mesh.MaterialRenderProxy = MaterialProxy;
							Mesh.bWireframe = true;
							BatchElement.FirstIndex = 0;
							BatchElement.IndexBuffer = &LODModel.IndexBuffer;
							BatchElement.NumPrimitives = LODModel.IndexBuffer.GetNumIndices() / 3;
						}
					}
					else
					{
						Mesh.Type = PT_TriangleList;
						Mesh.MaterialRenderProxy = MaterialProxy;
						BatchElement.IndexBuffer = &LODModel.IndexBuffer;
						BatchElement.FirstIndex = Section.FirstIndex;
						BatchElement.NumPrimitives = Section.NumTriangles;
					}

					Mesh.bCanApplyViewModeOverrides = true;
					Mesh.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

					check(BatchElement.NumPrimitives > 0);
					Collector.AddMesh(ViewIndex, Mesh);
				}
			}
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}



bool NiagaraRendererMeshes::SetMaterialUsage()
{
	//Causes deadlock :S Need to look at / rework the setting of materials and render modules.
	return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraMeshParticles);
}



/** Update render data buffer from attributes */
FNiagaraDynamicDataBase *NiagaraRendererMeshes::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderGT);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenMeshVertexData);

	if (!Properties || Properties->ParticleMesh == nullptr || !bEnabled)
	{
		return nullptr;
	}

	SimpleTimer VertexDataTimer;
//	TArray<FNiagaraMeshInstanceVertex>& RenderData = DynamicData->VertexData;
	//TArray< FNiagaraMeshInstanceVertexDynamicParameter>& RenderMaterialVertexData = DynamicData->MaterialParameterVertexData;

	if (PositionOffset == INDEX_NONE || LastSyncedId != Properties->SyncId)
	{
		// optional attributes
		int32 IntDummy;
		SizeOffset = -1;
		Data.GetVariableComponentOffsets(Properties->PositionBinding.DataSetVariable, PositionOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->VelocityBinding.DataSetVariable, VelocityOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->ColorBinding.DataSetVariable, ColorOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->ScaleBinding.DataSetVariable, ScaleOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterialBinding.DataSetVariable, MaterialParamOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial1Binding.DataSetVariable, MaterialParamOffset1, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial2Binding.DataSetVariable, MaterialParamOffset2, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial3Binding.DataSetVariable, MaterialParamOffset3, IntDummy);
		Data.GetVariableComponentOffsets(Properties->MeshOrientationBinding.DataSetVariable, TransformOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->CustomSortingBinding.DataSetVariable, CustomSortingOffset, IntDummy);
		LastSyncedId = Properties->SyncId;
	}

	//Bail if we don't have the required attributes to render this emitter.
	if (!bEnabled)
	{
		return nullptr;
	}

	FNiagaraDynamicDataMesh *DynamicData = nullptr;

	if (Data.CurrData().GetNumInstances() > 0)
	{
		DynamicData = new FNiagaraDynamicDataMesh;

		//TODO: This buffer is far fatter than needed. Just pull out the data needed for rendering.
		Data.CurrData().CopyTo(DynamicData->RTParticleData);

		DynamicData->DataSet = &Data;
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();
	return DynamicData;  
}



void NiagaraRendererMeshes::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataMesh*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = NewDynamicData;
}

int NiagaraRendererMeshes::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataMesh);
	if (DynamicDataRender && static_cast<FNiagaraDynamicDataMesh*>(DynamicDataRender)->DataSet)
	{
		//Size += (static_cast<FNiagaraDynamicDataMesh*>(DynamicDataRender))->DataSet->PrevDataRender().GetNumInstances() * sizeof(float);
	}

	return Size;
}

bool NiagaraRendererMeshes::HasDynamicData()
{
	return DynamicDataRender != nullptr;
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererMeshes::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererMeshes::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif
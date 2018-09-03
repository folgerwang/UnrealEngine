// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRenderer.h"
#include "ParticleResources.h"
#include "NiagaraSpriteVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"

DECLARE_CYCLE_STAT(TEXT("Generate Sprite Vertex Data"), STAT_NiagaraGenSpriteVertexData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Sprites"), STAT_NiagaraRenderSprites, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("Genereate GPU Buffers"), STAT_NiagaraGenSpriteGpuBuffers, STATGROUP_Niagara);

struct FNiagaraDynamicDataSprites : public FNiagaraDynamicDataBase
{
	//Direct ptr to the dataset. ONLY FOR USE BE GPU EMITTERS.
	//TODO: Even this needs to go soon.
	const FNiagaraDataSet *DataSet;
};



/* Mesh collector classes */
class FNiagaraMeshCollectorResourcesSprite : public FOneFrameResource
{
public:
	FNiagaraSpriteVertexFactory VertexFactory;
	FNiagaraSpriteUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesSprite()
	{
		VertexFactory.ReleaseResource();
	}
};



NiagaraRendererSprites::NiagaraRendererSprites(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) 
	: NiagaraRenderer()
	, PositionOffset(INDEX_NONE)
	, VelocityOffset(INDEX_NONE)
	, RotationOffset(INDEX_NONE)
	, SizeOffset(INDEX_NONE)
	, ColorOffset(INDEX_NONE)
	, FacingOffset(INDEX_NONE)
	, AlignmentOffset(INDEX_NONE)
	, SubImageOffset(INDEX_NONE)
	, MaterialParamOffset(INDEX_NONE)
	, MaterialParamOffset1(INDEX_NONE)
	, MaterialParamOffset2(INDEX_NONE)
	, MaterialParamOffset3(INDEX_NONE)
	, CameraOffsetOffset(INDEX_NONE)
	, UVScaleOffset(INDEX_NONE)
	, NormalizedAgeOffset(INDEX_NONE)
	, MaterialRandomOffset(INDEX_NONE)
	, CustomSortingOffset(INDEX_NONE)
	, LastSyncId(INDEX_NONE)
{
	//check(InProps);
	VertexFactory = new FNiagaraSpriteVertexFactory(NVFT_Sprite, FeatureLevel);
	Properties = Cast<UNiagaraSpriteRendererProperties>(InProps);
	BaseExtents = FVector(0.5f, 0.5f, 0.5f);
}


void NiagaraRendererSprites::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void NiagaraRendererSprites::CreateRenderThreadResources()
{
	VertexFactory->SetNumVertsInInstanceBuffer(4);
	VertexFactory->InitResource();
}

void NiagaraRendererSprites::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderSprites);

	SimpleTimer MeshElementsTimer;

	//check(DynamicDataRender)

	FNiagaraDynamicDataSprites *DynamicDataSprites = static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender);

	if (!DynamicDataSprites
		|| DynamicDataSprites->RTParticleData.GetNumInstancesAllocated() == 0
		|| DynamicDataSprites->RTParticleData.GetNumInstances() == 0
		|| nullptr == Properties
		|| !GSupportsResourceView // Current shader requires SRV to draw properly in all cases.
		)
	{
		return;
	}

	int32 NumInstances = DynamicDataSprites->RTParticleData.GetNumInstances();

	int32 TotalFloatSize = DynamicDataSprites->RTParticleData.GetFloatBuffer().Num() / sizeof(float);
	FNiagaraGlobalReadBuffer::FAllocation ParticleData;
	
	if (DynamicDataSprites->DataSet->GetSimTarget() == ENiagaraSimTarget::CPUSim)
	{
		ParticleData = FNiagaraGlobalReadBuffer::Get().AllocateFloat(TotalFloatSize);
		FMemory::Memcpy(ParticleData.Buffer, DynamicDataSprites->RTParticleData.GetFloatBuffer().GetData(), DynamicDataSprites->RTParticleData.GetFloatBuffer().Num());
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());

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
			SceneProxy->UseSingleSampleShadowFromStationaryLights(),
			SceneProxy->GetScene().HasPrecomputedVolumetricLightmap_RenderThread(),
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
			{
				const FSceneView* View = Views[ViewIndex];

				FNiagaraMeshCollectorResourcesSprite& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesSprite>();
				FNiagaraSpriteUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.LocalToWorld = bLocalSpace ? SceneProxy->GetLocalToWorld() : FMatrix::Identity;//For now just handle local space like this but maybe in future have a VF variant to avoid the transform entirely?
				PerViewUniformParameters.LocalToWorldInverseTransposed = bLocalSpace ? SceneProxy->GetLocalToWorld().Inverse().GetTransposed() : FMatrix::Identity;
				PerViewUniformParameters.RotationBias = 0.0f;
				PerViewUniformParameters.RotationScale = 1.0f;
				PerViewUniformParameters.TangentSelector = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.DeltaSeconds = ViewFamily.DeltaWorldTime;
				PerViewUniformParameters.NormalsType = 0.0f;
				PerViewUniformParameters.NormalsSphereCenter = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.NormalsCylinderUnitDirection = FVector4(0.0f, 0.0f, 1.0f, 0.0f);
				PerViewUniformParameters.PivotOffset = Properties->PivotInUVSpace * -1.0f; // We do this because we want to slide the coordinates back since 0,0 is the upper left corner.
				PerViewUniformParameters.MacroUVParameters = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
				PerViewUniformParameters.CameraFacingBlend = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.RemoveHMDRoll = Properties->bRemoveHMDRollInVR;
				PerViewUniformParameters.CustomFacingVectorMask = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.SubImageSize = FVector4(Properties->SubImageSize.X, Properties->SubImageSize.Y, 1.0f / Properties->SubImageSize.X, 1.0f / Properties->SubImageSize.Y);
				PerViewUniformParameters.PositionDataOffset = PositionOffset;
				PerViewUniformParameters.VelocityDataOffset = VelocityOffset;
				PerViewUniformParameters.RotationDataOffset = RotationOffset;
				PerViewUniformParameters.SizeDataOffset = SizeOffset;
				PerViewUniformParameters.ColorDataOffset = ColorOffset;
				PerViewUniformParameters.MaterialParamDataOffset = MaterialParamOffset;
				PerViewUniformParameters.MaterialParam1DataOffset = MaterialParamOffset1;
				PerViewUniformParameters.MaterialParam2DataOffset = MaterialParamOffset2;
				PerViewUniformParameters.MaterialParam3DataOffset = MaterialParamOffset3;
				PerViewUniformParameters.SubimageDataOffset = SubImageOffset;
				PerViewUniformParameters.FacingDataOffset = FacingOffset;
				PerViewUniformParameters.AlignmentDataOffset = AlignmentOffset;
				PerViewUniformParameters.SubImageBlendMode = Properties->bSubImageBlend;
				PerViewUniformParameters.CameraOffsetDataOffset = CameraOffsetOffset;
				PerViewUniformParameters.UVScaleDataOffset = UVScaleOffset;
				PerViewUniformParameters.NormalizedAgeDataOffset = NormalizedAgeOffset;
				PerViewUniformParameters.MaterialRandomDataOffset = MaterialRandomOffset;
				PerViewUniformParameters.DefaultPos = bLocalSpace ? FVector4(0.0f, 0.0f, 0.0f, 1.0f) : FVector4(SceneProxy->GetLocalToWorld().GetOrigin());

				//UE_LOG(LogNiagara, Log, TEXT("SubImageSize: %f %f offset: %d"), Properties->SubImageSize.X, Properties->SubImageSize.Y, SubImageOffset);

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
				ENiagaraSpriteFacingMode FacingMode = Properties->FacingMode;
				ENiagaraSpriteAlignment AlignmentMode = Properties->Alignment;

				if (FacingOffset == -1 && FacingMode == ENiagaraSpriteFacingMode::CustomFacingVector)
				{
					FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
				}

				if (AlignmentOffset == -1 && AlignmentMode == ENiagaraSpriteAlignment::CustomAlignment)
				{
					AlignmentMode = ENiagaraSpriteAlignment::Unaligned;
				}

				if (FacingMode == ENiagaraSpriteFacingMode::FaceCameraDistanceBlend)
				{
					float DistanceBlendMinSq = Properties->MinFacingCameraBlendDistance * Properties->MinFacingCameraBlendDistance;
					float DistanceBlendMaxSq = Properties->MaxFacingCameraBlendDistance * Properties->MaxFacingCameraBlendDistance;
					float InvBlendRange = 1.0f / FMath::Max(DistanceBlendMaxSq - DistanceBlendMinSq, 1.0f);
					float BlendScaledMinDistance = DistanceBlendMinSq * InvBlendRange;

					PerViewUniformParameters.CameraFacingBlend.X = 1.0f;
					PerViewUniformParameters.CameraFacingBlend.Y = InvBlendRange;
					PerViewUniformParameters.CameraFacingBlend.Z = BlendScaledMinDistance;
				}

				if (Properties->Alignment == ENiagaraSpriteAlignment::VelocityAligned)
				{
					// velocity aligned
					PerViewUniformParameters.RotationScale = 0.0f;
					PerViewUniformParameters.TangentSelector = FVector4(0.0f, 1.0f, 0.0f, 0.0f);
				}

				if (Properties->FacingMode == ENiagaraSpriteFacingMode::CustomFacingVector)
				{
					PerViewUniformParameters.CustomFacingVectorMask = Properties->CustomFacingVectorMask;
				}

				//Sort particles if needed.
				EBlendMode BlendMode = MaterialRenderProxy->GetMaterial(VertexFactory->GetFeatureLevel())->GetBlendMode();
				FNiagaraGlobalReadBuffer::FAllocation SortedIndices;
				CollectorResources.VertexFactory.SetSortedIndices(nullptr, 0xFFFFFFFF);
				if (DynamicDataSprites->DataSet->GetSimTarget() == ENiagaraSimTarget::CPUSim)//TODO: Compute shader for sorting gpu sims and larger cpu sims.
				{
					check(ParticleData.IsValid());
					if (BlendMode == BLEND_AlphaComposite || BlendMode == BLEND_Translucent || !Properties->bSortOnlyWhenTranslucent)
					{
						ENiagaraSortMode SortMode = Properties->SortMode;
						bool bCustomSortMode = SortMode == ENiagaraSortMode::CustomAscending || SortMode == ENiagaraSortMode::CustomDecending;
						int32 SortAttributeOffest = bCustomSortMode ? CustomSortingOffset : PositionOffset;
						if (SortMode != ENiagaraSortMode::None && SortAttributeOffest != INDEX_NONE)
						{
							SortedIndices = FNiagaraGlobalReadBuffer::Get().AllocateInt32(NumInstances);
							SortIndices(SortMode, SortAttributeOffest, DynamicDataSprites->RTParticleData, SceneProxy->GetLocalToWorld(), View, SortedIndices);
							CollectorResources.VertexFactory.SetSortedIndices(SortedIndices.ReadBuffer->SRV, SortedIndices.FirstIndex / sizeof(float));
						}
					}
					CollectorResources.VertexFactory.SetParticleData(ParticleData.ReadBuffer->SRV, ParticleData.FirstIndex / sizeof(float), DynamicDataSprites->RTParticleData.GetFloatStride() / sizeof(float));
				}
				else
				{
					CollectorResources.VertexFactory.SetParticleData(DynamicDataSprites->DataSet->CurrData().GetGPUBufferFloat()->SRV, 0, DynamicDataSprites->DataSet->CurrData().GetFloatStride() / sizeof(float));
				}

				CollectorResources.VertexFactory.SetAlignmentMode((uint32)AlignmentMode);
				CollectorResources.VertexFactory.SetFacingMode((uint32)FacingMode);

				CollectorResources.VertexFactory.SetParticleFactoryType(NVFT_Sprite);


				CollectorResources.UniformBuffer = FNiagaraSpriteUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.SetNumVertsInInstanceBuffer(4);
				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetSpriteUniformBuffer(CollectorResources.UniformBuffer);

				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MeshBatch.VertexFactory = &CollectorResources.VertexFactory;
				MeshBatch.CastShadow = SceneProxy->CastsDynamicShadow();
				MeshBatch.bUseAsOccluder = false;
				MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
				MeshBatch.Type = PT_TriangleList;
				MeshBatch.DepthPriorityGroup = SceneProxy->GetDepthPriorityGroup(View);
				MeshBatch.bCanApplyViewModeOverrides = true;
				MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

				if (bIsWireframe)
				{
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
				}
				else
				{
					MeshBatch.MaterialRenderProxy = MaterialRenderProxy;
				}

				FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
				MeshElement.IndexBuffer = &GParticleIndexBuffer;
				MeshElement.FirstIndex = 0;
				MeshElement.NumPrimitives = 2;
				MeshElement.NumInstances = FMath::Max(0, NumInstances);	//->VertexData.Num();
				MeshElement.MinVertexIndex = 0;
				MeshElement.MaxVertexIndex = 0;// MeshElement.NumInstances * 4 - 1;
				MeshElement.PrimitiveUniformBufferResource = &WorldSpacePrimitiveUniformBuffer;
				if (DynamicDataSprites->DataSet->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim)
				{
					MeshElement.IndirectArgsBuffer = DynamicDataSprites->DataSet->GetCurDataSetIndices().Buffer;
				}
				
				Collector.AddMesh(ViewIndex, MeshBatch);				
			}
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}

bool NiagaraRendererSprites::SetMaterialUsage()
{
	//Causes deadlock :S Need to look at / rework the setting of materials and render modules.
	return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraSprites);
}

void NiagaraRendererSprites::TransformChanged()
{
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

/** Update render data buffer from attributes */
FNiagaraDynamicDataBase *NiagaraRendererSprites::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	FNiagaraDynamicDataSprites *DynamicData = nullptr;

	if (bEnabled && Properties)
	{
		SimpleTimer VertexDataTimer;

		SCOPE_CYCLE_COUNTER(STAT_NiagaraGenSpriteVertexData);

		if (PositionOffset == INDEX_NONE || LastSyncId != Properties->SyncId)
		{			
			// optional attributes; we pass -1 as the offset so the VF can branch
			int32 IntDummy;
			Data.GetVariableComponentOffsets(Properties->PositionBinding.DataSetVariable, PositionOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->VelocityBinding.DataSetVariable, VelocityOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->SpriteRotationBinding.DataSetVariable, RotationOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->SpriteSizeBinding.DataSetVariable, SizeOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->ColorBinding.DataSetVariable, ColorOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->SpriteFacingBinding.DataSetVariable, FacingOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->SpriteAlignmentBinding.DataSetVariable, AlignmentOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->SubImageIndexBinding.DataSetVariable, SubImageOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->DynamicMaterialBinding.DataSetVariable, MaterialParamOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->DynamicMaterial1Binding.DataSetVariable, MaterialParamOffset1, IntDummy);
			Data.GetVariableComponentOffsets(Properties->DynamicMaterial2Binding.DataSetVariable, MaterialParamOffset2, IntDummy);
			Data.GetVariableComponentOffsets(Properties->DynamicMaterial3Binding.DataSetVariable, MaterialParamOffset3, IntDummy);
			Data.GetVariableComponentOffsets(Properties->CameraOffsetBinding.DataSetVariable, CameraOffsetOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->UVScaleBinding.DataSetVariable, UVScaleOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->NormalizedAgeBinding.DataSetVariable, NormalizedAgeOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->MaterialRandomBinding.DataSetVariable, MaterialRandomOffset, IntDummy);
			Data.GetVariableComponentOffsets(Properties->CustomSortingBinding.DataSetVariable, CustomSortingOffset, IntDummy);

			if (CustomSortingOffset == INDEX_NONE && (Properties->SortMode == ENiagaraSortMode::CustomAscending || Properties->SortMode == ENiagaraSortMode::CustomDecending))
			{
				UE_LOG(LogNiagara, Warning, TEXT("Niagara Sprite Emitter using custom sorting but does not have a valid custom sorting attribute binding."));
			}

			LastSyncId = Properties->SyncId;
		}
		
		if(Data.CurrData().GetNumInstances() > 0)
		{
			DynamicData = new FNiagaraDynamicDataSprites;

			//TODO: This buffer is far fatter than needed. Just pull out the data needed for rendering.
			Data.CurrData().CopyTo(DynamicData->RTParticleData);

			DynamicData->DataSet = &Data;
		}

		CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();
	}

	return DynamicData;  // for VF that can fetch from particle data directly
}



void NiagaraRendererSprites::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = NewDynamicData;
}

int NiagaraRendererSprites::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataSprites);
	return Size;
}

bool NiagaraRendererSprites::HasDynamicData()
{
//	return DynamicDataRender && static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender)->VertexData.Num() > 0;
	return DynamicDataRender!=nullptr;// && static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender)->NumInstances > 0;
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererSprites::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererSprites::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif
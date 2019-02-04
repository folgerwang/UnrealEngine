// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererRibbons.h"
#include "ParticleResources.h"
#include "NiagaraRibbonVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"

DECLARE_CYCLE_STAT(TEXT("Generate Ribbon Vertex Data [GT]"), STAT_NiagaraGenRibbonVertexData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Ribbons [RT]"), STAT_NiagaraRenderRibbons, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("Genereate GPU Buffers"), STAT_NiagaraGenRibbonGpuBuffers, STATGROUP_Niagara);


struct FNiagaraDynamicDataRibbon : public FNiagaraDynamicDataBase
{
	TArray<int16> IndexData;
	TArray<int32> SortedIndices;
	TArray<float> TotalDistances;
	TArray<uint32> MultiRibbonIndices;
	TArray<float> PackedPerRibbonDataByIndex;

	//Direct ptr to the dataset. ONLY FOR USE BE GPU EMITTERS.
	//TODO: Even this needs to go soon.
	const FNiagaraDataSet *DataSet;

	// start and end world space position of the ribbon, to figure out draw direction
	FVector StartPos;
	FVector EndPos;

	void PackPerRibbonData(float U0Scale, float U0Offset, float U1Scale, float U1Offset, uint32 NumSegments, uint32 StartVertexIndex)
	{
		PackedPerRibbonDataByIndex.Add(U0Scale);
		PackedPerRibbonDataByIndex.Add(U0Offset);
		PackedPerRibbonDataByIndex.Add(U1Scale);
		PackedPerRibbonDataByIndex.Add(U1Offset);
		PackedPerRibbonDataByIndex.Add(*(float*)&NumSegments);
		PackedPerRibbonDataByIndex.Add(*(float*)&StartVertexIndex);
	}
};

class FNiagaraMeshCollectorResourcesRibbon : public FOneFrameResource
{
public:
	FNiagaraRibbonVertexFactory VertexFactory;
	FNiagaraRibbonUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesRibbon()
	{
		VertexFactory.ReleaseResource();
	}
};


NiagaraRendererRibbons::NiagaraRendererRibbons(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) :
	NiagaraRenderer()
	, PositionDataOffset(INDEX_NONE)
	, VelocityDataOffset(INDEX_NONE)
	, WidthDataOffset(INDEX_NONE)
	, TwistDataOffset(INDEX_NONE)
	, FacingDataOffset(INDEX_NONE)
	, ColorDataOffset(INDEX_NONE)
	, NormalizedAgeDataOffset(INDEX_NONE)
	, MaterialRandomDataOffset(INDEX_NONE)
	, LastSyncedId(INDEX_NONE)
{
	VertexFactory = new FNiagaraRibbonVertexFactory(NVFT_Ribbon, FeatureLevel);
	Properties = Cast<UNiagaraRibbonRendererProperties>(InProps);
}


void NiagaraRendererRibbons::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

// FPrimitiveSceneProxy interface.
void NiagaraRendererRibbons::CreateRenderThreadResources()
{
	VertexFactory->InitResource();
}




void NiagaraRendererRibbons::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderRibbons);

	SimpleTimer MeshElementsTimer;
	FNiagaraDynamicDataRibbon *DynamicDataRibbon = static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender);
	if (!DynamicDataRibbon 
		|| DynamicDataRibbon->IndexData.Num() == 0
		|| nullptr == Properties
		|| !GSupportsResourceView // Current shader requires SRV to draw properly in all cases.
		)
	{
		return;
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();

	FGlobalDynamicIndexBuffer& DynamicIndexBuffer = Collector.GetDynamicIndexBuffer();
	FGlobalDynamicIndexBuffer::FAllocation DynamicIndexAllocation = DynamicIndexBuffer.Allocate(DynamicDataRibbon->IndexData.Num(), sizeof(int16));

	int32 TotalFloatSize = DynamicDataRibbon->RTParticleData.GetFloatBuffer().Num() / sizeof(float);
	FGlobalDynamicReadBuffer::FAllocation ParticleData;

	if (DynamicDataRibbon->DataSet->GetSimTarget() == ENiagaraSimTarget::CPUSim)
	{
		FGlobalDynamicReadBuffer& DynamicReadBuffer = Collector.GetDynamicReadBuffer();
		ParticleData = DynamicReadBuffer.AllocateFloat(TotalFloatSize);
		FMemory::Memcpy(ParticleData.Buffer, DynamicDataRibbon->RTParticleData.GetFloatBuffer().GetData(), DynamicDataRibbon->RTParticleData.GetFloatBuffer().Num());
	}

	if (DynamicIndexAllocation.IsValid())
	{
		// Update the primitive uniform buffer if needed.
		if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
		{
			FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(
				FMatrix::Identity,
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
				SceneProxy->GetLightingChannelMask(),
				0,
				INDEX_NONE,
				INDEX_NONE
				);
			WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
			WorldSpacePrimitiveUniformBuffer.InitResource();
		}

		// Copy the vertex data over.
		FMemory::Memcpy(DynamicIndexAllocation.Buffer, DynamicDataRibbon->IndexData.GetData(), DynamicDataRibbon->IndexData.Num() * sizeof(int16));

		// Compute the per-view uniform buffers.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				// figure out whether start is closer to the view plane than end
				float StartDist = FVector::DotProduct( View->GetViewDirection(), DynamicDataRibbon->StartPos - View->ViewLocation);
				float EndDist = FVector::DotProduct( View->GetViewDirection(), DynamicDataRibbon->EndPos - View->ViewLocation);
				bool bInvertOrder = ((StartDist > EndDist) && Properties->DrawDirection == ENiagaraRibbonDrawDirection::BackToFront)
					|| ((StartDist < EndDist) && Properties->DrawDirection == ENiagaraRibbonDrawDirection::FrontToBack);

				FNiagaraMeshCollectorResourcesRibbon& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesRibbon>();
				FNiagaraRibbonUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.LocalToWorld = bLocalSpace ? SceneProxy->GetLocalToWorld() : FMatrix::Identity;//For now just handle local space like this but maybe in future have a VF variant to avoid the transform entirely?
				PerViewUniformParameters.LocalToWorldInverseTransposed = bLocalSpace ? SceneProxy->GetLocalToWorld().Inverse().GetTransposed() : FMatrix::Identity;
				PerViewUniformParameters.DeltaSeconds = ViewFamily.DeltaWorldTime;
				PerViewUniformParameters.CameraUp = View->GetViewUp(); // FVector4(0.0f, 0.0f, 1.0f, 0.0f);
				PerViewUniformParameters.CameraRight = View->GetViewRight();//	FVector4(1.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.ScreenAlignment = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.UseCustomFacing = Properties->FacingMode == ENiagaraRibbonFacingMode::Custom ? 1 : 0;
				PerViewUniformParameters.TotalNumInstances = DynamicDataRibbon->RTParticleData.GetNumInstances();

				PerViewUniformParameters.PositionDataOffset = PositionDataOffset;
				PerViewUniformParameters.VelocityDataOffset = VelocityDataOffset;
				PerViewUniformParameters.ColorDataOffset = ColorDataOffset;
				PerViewUniformParameters.WidthDataOffset = WidthDataOffset;
				PerViewUniformParameters.TwistDataOffset = TwistDataOffset;
				PerViewUniformParameters.FacingDataOffset = FacingDataOffset;
				PerViewUniformParameters.NormalizedAgeDataOffset = NormalizedAgeDataOffset;
				PerViewUniformParameters.MaterialRandomDataOffset = MaterialRandomDataOffset;
				PerViewUniformParameters.MaterialParamDataOffset = MaterialParamOffset;
				PerViewUniformParameters.MaterialParam1DataOffset = MaterialParamOffset1;
				PerViewUniformParameters.MaterialParam2DataOffset = MaterialParamOffset2;
				PerViewUniformParameters.MaterialParam3DataOffset = MaterialParamOffset3;
				PerViewUniformParameters.InvertDrawOrder = bInvertOrder ? 1 : 0;
				PerViewUniformParameters.UV0TilingDistance = Properties->UV0TilingDistance;
				PerViewUniformParameters.UV1TilingDistance = Properties->UV1TilingDistance;
				PerViewUniformParameters.PackedVData = FVector4(Properties->UV0Scale.Y, Properties->UV0Offset.Y, Properties->UV1Scale.Y, Properties->UV1Offset.Y);
				CollectorResources.VertexFactory.SetParticleData(ParticleData.ReadBuffer->SRV, ParticleData.FirstIndex / sizeof(float), DynamicDataRibbon->RTParticleData.GetFloatStride() / sizeof(float));;

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
				CollectorResources.VertexFactory.SetParticleFactoryType(NVFT_Ribbon);

				CollectorResources.UniformBuffer = FNiagaraRibbonUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetRibbonUniformBuffer(CollectorResources.UniformBuffer);

				if (!DynamicDataRibbon->SortedIndices.Num())
				{
					return;
				}

				// TODO: need to make these two a global alloc buffer as well, not recreate
				// pass in the sorted indices so the VS can fetch the particle data in order
				FReadBuffer SortedIndicesBuffer;
				SortedIndicesBuffer.Initialize(sizeof(int32), DynamicDataRibbon->SortedIndices.Num(), EPixelFormat::PF_R32_SINT, BUF_Volatile);
				void *IndexPtr = RHILockVertexBuffer(SortedIndicesBuffer.Buffer, 0, DynamicDataRibbon->SortedIndices.Num() * sizeof(int32), RLM_WriteOnly);
				FMemory::Memcpy(IndexPtr, DynamicDataRibbon->SortedIndices.GetData(), DynamicDataRibbon->SortedIndices.Num() * sizeof(int32));
				RHIUnlockVertexBuffer(SortedIndicesBuffer.Buffer);
				CollectorResources.VertexFactory.SetSortedIndices(SortedIndicesBuffer.SRV, 0);

				// pass in the CPU generated total segment distance (for tiling distance modes); needs to be a buffer so we can fetch them in the correct order based on Draw Direction (front->back or back->front)
				//	otherwise UVs will pop when draw direction changes based on camera view point
				FReadBuffer TotalDistancesBuffer;
				TotalDistancesBuffer.Initialize(sizeof(float), DynamicDataRibbon->TotalDistances.Num(), EPixelFormat::PF_R32_FLOAT, BUF_Volatile);
				void *TotalDistancesPtr = RHILockVertexBuffer(TotalDistancesBuffer.Buffer, 0, DynamicDataRibbon->TotalDistances.Num() * sizeof(float), RLM_WriteOnly);
				FMemory::Memcpy(TotalDistancesPtr, DynamicDataRibbon->TotalDistances.GetData(), DynamicDataRibbon->TotalDistances.Num() * sizeof(float));
				RHIUnlockVertexBuffer(TotalDistancesBuffer.Buffer);
				CollectorResources.VertexFactory.SetSegmentDistances(TotalDistancesBuffer.SRV);

				// Copy a buffer which has the per particle multi ribbon index.
				FReadBuffer MultiRibbonIndicesBuffer;
				MultiRibbonIndicesBuffer.Initialize(sizeof(uint32), DynamicDataRibbon->MultiRibbonIndices.Num(), EPixelFormat::PF_R32_UINT, BUF_Volatile);
				void* MultiRibbonIndexPtr = RHILockVertexBuffer(MultiRibbonIndicesBuffer.Buffer, 0, DynamicDataRibbon->MultiRibbonIndices.Num() * sizeof(uint32), RLM_WriteOnly);
				FMemory::Memcpy(MultiRibbonIndexPtr, DynamicDataRibbon->MultiRibbonIndices.GetData(), DynamicDataRibbon->MultiRibbonIndices.Num() * sizeof(uint32));
				RHIUnlockVertexBuffer(MultiRibbonIndicesBuffer.Buffer);
				CollectorResources.VertexFactory.SetMultiRibbonIndicesSRV(MultiRibbonIndicesBuffer.SRV);

				// Copy the packed u data for stable age based uv generation.
				FReadBuffer PackedPerRibbonDataByIndexBuffer;
				PackedPerRibbonDataByIndexBuffer.Initialize(sizeof(float), DynamicDataRibbon->PackedPerRibbonDataByIndex.Num(), EPixelFormat::PF_R32_FLOAT, BUF_Volatile);
				void *PackedPerRibbonDataByIndexPtr = RHILockVertexBuffer(PackedPerRibbonDataByIndexBuffer.Buffer, 0, DynamicDataRibbon->PackedPerRibbonDataByIndex.Num() * sizeof(float), RLM_WriteOnly);
				FMemory::Memcpy(PackedPerRibbonDataByIndexPtr, DynamicDataRibbon->PackedPerRibbonDataByIndex.GetData(), DynamicDataRibbon->PackedPerRibbonDataByIndex.Num() * sizeof(float));
				RHIUnlockVertexBuffer(PackedPerRibbonDataByIndexBuffer.Buffer);
				CollectorResources.VertexFactory.SetPackedPerRibbonDataByIndexSRV(PackedPerRibbonDataByIndexBuffer.SRV);

				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MeshBatch.VertexFactory = &CollectorResources.VertexFactory;
				MeshBatch.CastShadow = SceneProxy->CastsDynamicShadow();
				MeshBatch.bUseAsOccluder = false;
				MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
				MeshBatch.bDisableBackfaceCulling = true;
				MeshBatch.Type = PT_TriangleList;
				MeshBatch.DepthPriorityGroup = SceneProxy->GetDepthPriorityGroup(View);
				MeshBatch.bCanApplyViewModeOverrides = true;
				MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

				if (bIsWireframe)
				{
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
				}
				else
				{
					MeshBatch.MaterialRenderProxy = MaterialRenderProxy;
				}

				FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
				MeshElement.IndexBuffer = DynamicIndexAllocation.IndexBuffer;
				MeshElement.FirstIndex = DynamicIndexAllocation.FirstIndex;
				MeshElement.NumPrimitives = DynamicDataRibbon->IndexData.Num() / 3;
				check(MeshElement.NumPrimitives > 0);
				MeshElement.NumInstances = 1;
				MeshElement.MinVertexIndex = 0;
				MeshElement.MaxVertexIndex = 0;
				MeshElement.PrimitiveUniformBuffer = WorldSpacePrimitiveUniformBuffer.GetUniformBufferRHI();

				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}

void NiagaraRendererRibbons::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = NewDynamicData;
}

int NiagaraRendererRibbons::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataRibbon);
	if (DynamicDataRender)
	{
		FNiagaraDynamicDataRibbon* RibbonDynamicData = static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender);
		Size += RibbonDynamicData->IndexData.GetAllocatedSize();
		Size += RibbonDynamicData->SortedIndices.GetAllocatedSize();
		Size += RibbonDynamicData->TotalDistances.GetAllocatedSize();
		Size += RibbonDynamicData->MultiRibbonIndices.GetAllocatedSize();
		Size += RibbonDynamicData->PackedPerRibbonDataByIndex.GetAllocatedSize();
	}

	return Size;
}

bool NiagaraRendererRibbons::HasDynamicData()
{
	return DynamicDataRender && (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->IndexData.Num() > 0;
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererRibbons::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererRibbons::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif


bool NiagaraRendererRibbons::SetMaterialUsage()
{
	return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraRibbons);
}

void NiagaraRendererRibbons::TransformChanged()
{
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void CalculateUVScaleAndOffsets(
	const FNiagaraDataSetAccessor<float>& SortKeyData, const TArray<int32>& RibbonIndices,
	bool bSortKeyIsAge, int32 StartIndex, int32 EndIndex, int32 NumSegments,
	float InUTilingDistance, float InUScale, float InUOffset, ENiagaraRibbonAgeOffsetMode InAgeOffsetMode,
	float& OutUScale, float& OutUOffset)
{
	if (EndIndex - StartIndex > 0 && bSortKeyIsAge && InUTilingDistance == 0)
	{
		float AgeUScale;
		float AgeUOffset;
		if (InAgeOffsetMode == ENiagaraRibbonAgeOffsetMode::Scale)
		{
			// In scale mode we scale and offset the UVs so that no part of the texture is clipped. In order to prevent
			// clipping at the ends we'll have to move the UVs in up to the size of a single segment of the ribbon since
			// that's the distance we'll need to to smoothly interpolate when a new segment is added, or when an old segment
			// is removed.  We calculate the end offset when the end of the ribbon is within a single time step of 0 or 1
			// which is then normalized to the range of a single segment.  We can then calculate how many segments we actually
			// have to draw the scaled ribbon, and can offset the start by the correctly scaled offset.
			float FirstAge = SortKeyData[RibbonIndices[StartIndex]];
			float SecondAge = SortKeyData[RibbonIndices[StartIndex + 1]];
			float SecondToLastAge = SortKeyData[RibbonIndices[EndIndex - 1]];
			float LastAge = SortKeyData[RibbonIndices[EndIndex]];

			float StartTimeStep = SecondAge - FirstAge;
			float StartTimeOffset = FirstAge < StartTimeStep ? StartTimeStep - FirstAge : 0;
			float StartSegmentOffset = StartTimeOffset / StartTimeStep;

			float EndTimeStep = LastAge - SecondToLastAge;
			float EndTimeOffset = 1 - LastAge < EndTimeStep ? EndTimeStep - (1 - LastAge) : 0;
			float EndSegmentOffset = EndTimeOffset / EndTimeStep;

			float AvailableSegments = NumSegments - (StartSegmentOffset + EndSegmentOffset);
			AgeUScale = NumSegments / AvailableSegments;
			AgeUOffset = -((StartSegmentOffset / NumSegments) * AgeUScale);
		}
		else
		{
			float FirstAge = SortKeyData[RibbonIndices[StartIndex]];
			float LastAge = SortKeyData[RibbonIndices[EndIndex]];

			AgeUScale = LastAge - FirstAge;
			AgeUOffset = FirstAge;
		}

		OutUScale = AgeUScale * InUScale;
		OutUOffset = (AgeUOffset * InUScale) + InUOffset;
	}
	else
	{
		OutUScale = InUScale;
		OutUOffset = InUOffset;
	}
}

FNiagaraDynamicDataBase *NiagaraRendererRibbons::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenRibbonVertexData);

	SimpleTimer VertexDataTimer;
	if (!bEnabled)
	{
		return nullptr;
	}
	FNiagaraDynamicDataRibbon* DynamicData = new FNiagaraDynamicDataRibbon;
	TArray<int16>& IndexData = DynamicData->IndexData;

	// TODO : deal with the dynamic vertex material parameter should the user have specified it as an output...
	int32 NumTotalVerts = 0;

	FNiagaraDataSetAccessor<FVector> PosData(Data, Properties->PositionBinding.DataSetVariable);

	bool bSortKeyIsAge = false;
	FNiagaraDataSetAccessor<float> SortKeyData(Data, Properties->RibbonLinkOrderBinding.DataSetVariable);
	if (SortKeyData.IsValid() == false)
	{
		SortKeyData = FNiagaraDataSetAccessor<float>(Data, Properties->NormalizedAgeBinding.DataSetVariable);
		bSortKeyIsAge = true;
	}

	//Bail if we don't have the required attributes to render this emitter.
	if (Data.GetNumInstances() < 2 || !PosData.IsValid() || !SortKeyData.IsValid())
	{
		return DynamicData;
	}

	if (PositionDataOffset == INDEX_NONE || LastSyncedId != Properties->SyncId)
	{
		// required attributes
		int32 IntDummy;
		Data.GetVariableComponentOffsets(Properties->PositionBinding.DataSetVariable, PositionDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->VelocityBinding.DataSetVariable, VelocityDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->ColorBinding.DataSetVariable, ColorDataOffset, IntDummy);

		// optional attributes
		Data.GetVariableComponentOffsets(Properties->RibbonWidthBinding.DataSetVariable, WidthDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->RibbonTwistBinding.DataSetVariable, TwistDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->RibbonFacingBinding.DataSetVariable, FacingDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->NormalizedAgeBinding.DataSetVariable, NormalizedAgeDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->MaterialRandomBinding.DataSetVariable, MaterialRandomDataOffset, IntDummy);

		Data.GetVariableComponentOffsets(Properties->DynamicMaterialBinding.DataSetVariable, MaterialParamOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial1Binding.DataSetVariable, MaterialParamOffset1, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial2Binding.DataSetVariable, MaterialParamOffset2, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial3Binding.DataSetVariable, MaterialParamOffset3, IntDummy);

		LastSyncedId = Properties->SyncId;
	}

	DynamicData->DataSet = &Data;

	////////
	FNiagaraDataSetAccessor<float> SizeData(Data, Properties->RibbonWidthBinding.DataSetVariable);
	FNiagaraDataSetAccessor<float> TwistData(Data, Properties->RibbonTwistBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector> AlignData(Data, Properties->RibbonFacingBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParamData(Data, Properties->DynamicMaterialBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParam1Data(Data, Properties->DynamicMaterial1Binding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParam2Data(Data, Properties->DynamicMaterial2Binding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParam3Data(Data, Properties->DynamicMaterial3Binding.DataSetVariable);

	FNiagaraDataSetAccessor<int32> RibbonIdData;
	FNiagaraDataSetAccessor<FNiagaraID> RibbonFullIDData;
	if (Properties->RibbonIdBinding.DataSetVariable.GetType() == FNiagaraTypeDefinition::GetIDDef())
	{
		RibbonFullIDData.Create(&Data, Properties->RibbonIdBinding.DataSetVariable);
		RibbonFullIDData.InitForAccess(true);
	}
	else
	{
		RibbonIdData.Create(&Data, Properties->RibbonIdBinding.DataSetVariable);
		RibbonIdData.InitForAccess(true);
	}

	bool bFullIDs = RibbonFullIDData.IsValid();
	bool bSimpleIDs = !bFullIDs && RibbonIdData.IsValid();
	bool bMultiRibbons = bFullIDs || bSimpleIDs;

	auto AddRibbonVerts = [&](TArray<int32>& RibbonIndices, uint32 RibbonIndex)
	{
		int32 StartIndex = DynamicData->SortedIndices.Num();
		int32 NumIndices = RibbonIndices.Num();
		if (NumIndices > 1)
		{
			FVector PrevPos, PrevPos2, PrevDir(0.0f, 0.0f, 0.1f);
			float TotalDistance = 0.0f;

			uint32 LastParticipatingParticle = RibbonIndices[0];
			for (int32 i = 0; i < NumIndices; i++)
			{
				uint32 Index1 = RibbonIndices[i];
				uint32 Index2 = 0;
				const FVector ParticlePos = PosData[Index1];
				FVector ParticleDir;
				if (i < NumIndices - 1)
				{
					Index2 = RibbonIndices[i + 1];
					ParticleDir = PosData[Index2] - PosData[LastParticipatingParticle];
				}
				else
				{
					Index2 = RibbonIndices[i - 1];
					ParticleDir = PosData[LastParticipatingParticle] - PosData[Index2];
				}

				// if two ribbon particles were spawned too close together, we skip one
				// but never skip the last, because that will result in invalid indices from the prev loop
				if (ParticleDir.SizeSquared() > 0.002 || i == NumIndices - 1)
				{
					DynamicData->SortedIndices.Add(Index1);

					LastParticipatingParticle = Index2;
					FVector NormDir = ParticleDir.GetSafeNormal();
					PrevDir = NormDir;

					DynamicData->TotalDistances.Add(TotalDistance);
					DynamicData->MultiRibbonIndices.Add(RibbonIndex);

					if (i < NumIndices - 1)
					{
						IndexData.Add(NumTotalVerts);
						IndexData.Add(NumTotalVerts + 1);
						IndexData.Add(NumTotalVerts + 2);
						IndexData.Add(NumTotalVerts + 1);
						IndexData.Add(NumTotalVerts + 3);
						IndexData.Add(NumTotalVerts + 2);
					}
					NumTotalVerts += 2;
				}

				TotalDistance += ParticleDir.Size();
			}
		}

		float U0Offset;
		float U0Scale;
		float U1Offset;
		float U1Scale;

		int32 EndIndex = DynamicData->SortedIndices.Num() - 1;
		int32 NumSegments = EndIndex - StartIndex;
		int32 StartVertexIndex = StartIndex * 2; // We add two vertices for each particle.

		CalculateUVScaleAndOffsets(SortKeyData, DynamicData->SortedIndices, bSortKeyIsAge, StartIndex, DynamicData->SortedIndices.Num() - 1, NumSegments,
			Properties->UV0TilingDistance,	Properties->UV0Scale.X, Properties->UV0Offset.X, Properties->UV0AgeOffsetMode, U0Scale, U0Offset);
		CalculateUVScaleAndOffsets(SortKeyData, DynamicData->SortedIndices, bSortKeyIsAge, StartIndex, DynamicData->SortedIndices.Num() - 1, NumSegments,
			Properties->UV1TilingDistance, Properties->UV1Scale.X, Properties->UV1Offset.X, Properties->UV1AgeOffsetMode, U1Scale, U1Offset);

		DynamicData->PackPerRibbonData(U0Scale, U0Offset, U1Scale, U1Offset, NumSegments, StartVertexIndex);
	};

	// store the start and end positions for the ribbon for draw distance flipping 
	DynamicData->StartPos = PosData[0];
	DynamicData->EndPos = PosData[Data.GetNumInstances() - 1];
	
	//TODO: Move sorting to share code with sprite and mesh sorting and support the custom sorting key.
	int32 TotalIndices = Data.GetNumInstances();

	if (!bMultiRibbons)
	{
		TArray<int32> SortedIndices;
		for (int32 i = 0; i < TotalIndices; ++i)
		{
			SortedIndices.Add(i);
		}

		SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });

		AddRibbonVerts(SortedIndices, 0);
	}
	else
	{
		if (bFullIDs)
		{
			TMap<FNiagaraID, TArray<int32>> MultiRibbonSortedIndices;
			
			for (int32 i = 0; i < TotalIndices; ++i)
			{
				TArray<int32>& Indices = MultiRibbonSortedIndices.FindOrAdd(RibbonFullIDData[i]);
				Indices.Add(i);
			}

			uint32 RibbonIndex = 0;
			for (TPair<FNiagaraID, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				TArray<int32>& SortedIndices = Pair.Value;
				SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });
				AddRibbonVerts(SortedIndices, RibbonIndex);
				
				RibbonIndex++;
			};
		}
		else
		{
			//TODO: Remove simple ID path
			check(bSimpleIDs);

			TMap<int32, TArray<int32>> MultiRibbonSortedIndices;

			for (int32 i = 0; i < TotalIndices; ++i)
			{
				TArray<int32>& Indices = MultiRibbonSortedIndices.FindOrAdd(RibbonIdData[i]);
				Indices.Add(i);
			}

			uint32 RibbonIndex = 0;
			for (TPair<int32, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				TArray<int32>& SortedIndices = Pair.Value;
				SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });
				AddRibbonVerts(SortedIndices, RibbonIndex);
				RibbonIndex++;
			};
		}
	}

	if (Data.CurrData().GetNumInstances() > 0)
	{
		//TODO: This buffer is far fatter than needed. Just pull out the data needed for rendering.
		Data.CurrData().CopyTo(DynamicData->RTParticleData);

		DynamicData->DataSet = &Data;
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();

	return DynamicData;
}



// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererRibbons.h"
#include "ParticleResources.h"
#include "NiagaraRibbonVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"

DECLARE_CYCLE_STAT(TEXT("Generate Ribbon Vertex Data"), STAT_NiagaraGenRibbonVertexData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Ribbons"), STAT_NiagaraRenderRibbons, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("Genereate GPU Buffers"), STAT_NiagaraGenRibbonGpuBuffers, STATGROUP_Niagara);


struct FNiagaraDynamicDataRibbon : public FNiagaraDynamicDataBase
{
	TArray<FNiagaraRibbonVertex> VertexData;
	TArray<int16> IndexData;
	TArray<FNiagaraRibbonVertexDynamicParameter> MaterialParameterVertexData[4];

	//Direct ptr to the dataset. ONLY FOR USE BE GPU EMITTERS.
	//TODO: Even this needs to go soon.
	const FNiagaraDataSet *DataSet;
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
	, WidthDataOffset(INDEX_NONE)
	, TwistDataOffset(INDEX_NONE)
	, ColorDataOffset(INDEX_NONE)
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
		|| DynamicDataRibbon->VertexData.Num() < 4
		|| DynamicDataRibbon->IndexData.Num() == 0
		|| nullptr == Properties
		)
	{
		return;
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());

	int32 SizeInBytes = DynamicDataRibbon->VertexData.GetTypeSize() * DynamicDataRibbon->VertexData.Num();
	FGlobalDynamicVertexBuffer::FAllocation LocalDynamicVertexAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(SizeInBytes);
	FGlobalDynamicVertexBuffer::FAllocation LocalDynamicVertexMaterialParamsAllocation[4];
	FGlobalDynamicIndexBuffer::FAllocation DynamicIndexAllocation = FGlobalDynamicIndexBuffer::Get().Allocate(DynamicDataRibbon->IndexData.Num(), sizeof(int16));

	auto AllocDynamicParameterBuffer = [&](int32 DynamicParamIndex)
	{
		if (DynamicDataRibbon->MaterialParameterVertexData[DynamicParamIndex].Num() > 0)
		{
			int32 MatParamSizeInBytes = DynamicDataRibbon->MaterialParameterVertexData[DynamicParamIndex].GetTypeSize() * DynamicDataRibbon->MaterialParameterVertexData[DynamicParamIndex].Num();
			LocalDynamicVertexMaterialParamsAllocation[DynamicParamIndex] = FGlobalDynamicVertexBuffer::Get().Allocate(MatParamSizeInBytes);

			if (LocalDynamicVertexMaterialParamsAllocation[DynamicParamIndex].IsValid())
			{
				// Copy the extra material vertex data over.
				FMemory::Memcpy(LocalDynamicVertexMaterialParamsAllocation[DynamicParamIndex].Buffer, DynamicDataRibbon->MaterialParameterVertexData[DynamicParamIndex].GetData(), MatParamSizeInBytes);
			}
		}
	};
	AllocDynamicParameterBuffer(0);
	AllocDynamicParameterBuffer(1);
	AllocDynamicParameterBuffer(2);
	AllocDynamicParameterBuffer(3);


	if (LocalDynamicVertexAllocation.IsValid())
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
				SceneProxy->UseSingleSampleShadowFromStationaryLights(),
				SceneProxy->GetScene().HasPrecomputedVolumetricLightmap_RenderThread(),
				SceneProxy->UseEditorDepthTest(),
				SceneProxy->GetLightingChannelMask()
				);
			WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
			WorldSpacePrimitiveUniformBuffer.InitResource();
		}

		// Copy the vertex data over.
		FMemory::Memcpy(LocalDynamicVertexAllocation.Buffer, DynamicDataRibbon->VertexData.GetData(), SizeInBytes);
		FMemory::Memcpy(DynamicIndexAllocation.Buffer, DynamicDataRibbon->IndexData.GetData(), DynamicDataRibbon->IndexData.Num() * sizeof(int16));

		// Compute the per-view uniform buffers.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				FNiagaraMeshCollectorResourcesRibbon& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesRibbon>();
				FNiagaraRibbonUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.CameraUp = View->GetViewUp(); // FVector4(0.0f, 0.0f, 1.0f, 0.0f);
				PerViewUniformParameters.CameraRight = View->GetViewRight();//	FVector4(1.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.ScreenAlignment = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.UseCustomFacing = Properties->FacingMode == ENiagaraRibbonFacingMode::Custom;

				PerViewUniformParameters.PositionDataOffset = PositionDataOffset;
				PerViewUniformParameters.ColorDataOffset = ColorDataOffset;
				PerViewUniformParameters.WidthDataOffset = WidthDataOffset;
				PerViewUniformParameters.TwistDataOffset = TwistDataOffset;
				CollectorResources.VertexFactory.SetParticleData(DynamicDataRibbon->DataSet);

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
				CollectorResources.VertexFactory.SetParticleFactoryType(NVFT_Ribbon);

				CollectorResources.UniformBuffer = FNiagaraRibbonUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetBeamTrailUniformBuffer(CollectorResources.UniformBuffer);
				CollectorResources.VertexFactory.SetVertexBuffer(LocalDynamicVertexAllocation.VertexBuffer, LocalDynamicVertexAllocation.VertexOffset, sizeof(FNiagaraRibbonVertex));

				auto SetDynamicParameterBuffer = [&](int32 DynamicParameterIndex)
				{
					if (DynamicDataRibbon->MaterialParameterVertexData[DynamicParameterIndex].Num() > 0 && LocalDynamicVertexMaterialParamsAllocation[DynamicParameterIndex].IsValid())
					{
						CollectorResources.VertexFactory.SetDynamicParameterBuffer(LocalDynamicVertexMaterialParamsAllocation[DynamicParameterIndex].VertexBuffer, DynamicParameterIndex, LocalDynamicVertexMaterialParamsAllocation[DynamicParameterIndex].VertexOffset, sizeof(FNiagaraRibbonVertexDynamicParameter));
					}
					else
					{
						CollectorResources.VertexFactory.SetDynamicParameterBuffer(NULL, DynamicParameterIndex, 0, 0);
					}
				};
				SetDynamicParameterBuffer(0);
				SetDynamicParameterBuffer(1);
				SetDynamicParameterBuffer(2);
				SetDynamicParameterBuffer(3);

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
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
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
				MeshElement.MaxVertexIndex = DynamicDataRibbon->VertexData.Num() - 1;
				MeshElement.PrimitiveUniformBufferResource = &WorldSpacePrimitiveUniformBuffer;

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
		Size += (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->VertexData.GetAllocatedSize();
		Size += (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->IndexData.GetAllocatedSize();
		Size += (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->MaterialParameterVertexData[0].GetAllocatedSize();
		Size += (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->MaterialParameterVertexData[1].GetAllocatedSize();
		Size += (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->MaterialParameterVertexData[2].GetAllocatedSize();
		Size += (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->MaterialParameterVertexData[3].GetAllocatedSize();
	}

	return Size;
}

bool NiagaraRendererRibbons::HasDynamicData()
{
	return DynamicDataRender && (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->VertexData.Num() > 0;
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

void CalculateUVScaleAndOffsets(const FNiagaraDataSetAccessor<float>& SortKeyData, const TArray<int32>& RibbonIndices, bool bSortKeyIsAge, int32 NumSegments,
	float InUVTilingDistance, const FVector2D& InUVScale, const FVector2D& InUVOffset, ENiagaraRibbonAgeOffsetMode InAgeOffsetMode, FVector2D& OutUVScale, FVector2D& OutUVOffset)
{
	if (bSortKeyIsAge && InUVTilingDistance == 0)
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
			float FirstAge = SortKeyData[RibbonIndices[0]];
			float SecondAge = SortKeyData[RibbonIndices[1]];
			float SecondToLastAge = SortKeyData[RibbonIndices[RibbonIndices.Num() - 2]];
			float LastAge = SortKeyData[RibbonIndices.Last()];

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
			float FirstAge = SortKeyData[RibbonIndices[0]];
			float LastAge = SortKeyData[RibbonIndices.Last()];

			AgeUScale = LastAge - FirstAge;
			AgeUOffset = FirstAge;
		}

		OutUVScale = FVector2D(AgeUScale * InUVScale.X, InUVScale.Y);
		OutUVOffset = FVector2D((AgeUOffset * InUVScale.X) + InUVOffset.X, InUVOffset.Y);
	}
	else
	{
		OutUVScale = InUVScale;
		OutUVOffset = InUVOffset;
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
	FNiagaraDynamicDataRibbon *DynamicData = new FNiagaraDynamicDataRibbon;
	TArray<FNiagaraRibbonVertex>& RenderData = DynamicData->VertexData;
	TArray<int16>& IndexData = DynamicData->IndexData;

	RenderData.Empty();
	IndexData.Empty();

	// TODO : deal with the dynamic vertex material parameter should the user have specified it as an output...

	FVector2D UVs[2] = { FVector2D(1.0f, 1.0f), FVector2D(1.0f, 0.0f) };
	int32 NumTotalVerts = 0;

	FNiagaraDataSetAccessor<FVector> PosData(Data, Properties->PositionBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector> VelData(Data, Properties->VelocityBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FLinearColor> ColData(Data, Properties->ColorBinding.DataSetVariable);

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

		const FNiagaraVariableLayoutInfo* PositionLayout = Data.GetVariableLayout(Properties->PositionBinding.DataSetVariable);
		const FNiagaraVariableLayoutInfo* TwistLayout = Data.GetVariableLayout(Properties->RibbonTwistBinding.DataSetVariable);
		const FNiagaraVariableLayoutInfo* WidthLayout = Data.GetVariableLayout(Properties->RibbonWidthBinding.DataSetVariable);
		const FNiagaraVariableLayoutInfo* ColorLayout = Data.GetVariableLayout(Properties->ColorBinding.DataSetVariable);

		// required attributes
		PositionDataOffset = PositionLayout ? PositionLayout->FloatComponentStart : INDEX_NONE;
		ColorDataOffset = ColorLayout ? ColorLayout->FloatComponentStart : INDEX_NONE;

		// optional attributes
		int32 IntDummy;
		Data.GetVariableComponentOffsets(Properties->RibbonWidthBinding.DataSetVariable, WidthDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->RibbonTwistBinding.DataSetVariable, TwistDataOffset, IntDummy);
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

	auto AddRibbonVerts = [&](TArray<int32>& RibbonIndices)
	{
		int32 NumIndices = RibbonIndices.Num();
		if (NumIndices > 1)
		{
			FVector PrevPos, PrevPos2, PrevDir(0.0f, 0.0f, 0.1f);
			float TotalDistance = 0.0f;

			int32 NumSegments = NumIndices - 1;

			FVector2D UV0Offset;
			FVector2D UV0Scale;
			FVector2D UV1Offset;
			FVector2D UV1Scale;

			CalculateUVScaleAndOffsets(SortKeyData, RibbonIndices, bSortKeyIsAge, NumSegments, Properties->UV0TilingDistance,
				Properties->UV0Scale, Properties->UV0Offset, Properties->UV0AgeOffsetMode, UV0Scale, UV0Offset);
			CalculateUVScaleAndOffsets(SortKeyData, RibbonIndices, bSortKeyIsAge, NumSegments, Properties->UV1TilingDistance,
				Properties->UV1Scale, Properties->UV1Offset, Properties->UV1AgeOffsetMode, UV1Scale, UV1Offset);

			for (int32 i = 0; i < NumIndices; i++)
			{
				uint32 Index1 = RibbonIndices[i];
				uint32 Index2 = 0;
				const FVector ParticlePos = PosData[Index1];
				FVector ParticleDir;
				if (i < NumIndices - 1)
				{
					Index2 = RibbonIndices[i + 1];
					ParticleDir = PosData[Index2] - ParticlePos;
				}
				else
				{
					Index2 = RibbonIndices[i - 1];
					ParticleDir = ParticlePos - PosData[Index2];
				}

				// if two ribbon particles were spawned too close together, we skip one
				// but never skip the last, because that will result in invalid indices from the prev loop
				if (ParticleDir.SizeSquared() > 0.002 || i == NumIndices - 1)
				{
					FVector NormDir = ParticleDir.GetSafeNormal();
					PrevDir = NormDir;

					// Calculate sub-uv for this segment based on the either the distance or index, depending on whether the distance tile factor was set.
					float U0ForSegment = Properties->UV0TilingDistance
						? TotalDistance / Properties->UV0TilingDistance
						: (float)i / NumSegments;
					float U1ForSegment = Properties->UV1TilingDistance
						? TotalDistance / Properties->UV1TilingDistance
						: (float)i / NumSegments;

					FVector2D UV0ForSegment(U0ForSegment, 1.0f);
					FVector2D UV1ForSegment(U1ForSegment, 1.0f);
					
					//Todo. Possibly pull out to templated func to avoid safety code in loop here.
					FLinearColor Color = ColData.GetSafe(Index1, FLinearColor::White);
					float Twist = TwistData.GetSafe(Index1, 0.0f);
					float Size = SizeData.GetSafe(Index1, 1.0f);
					FVector Align = AlignData.GetSafe(Index1, FVector::UpVector);

					TotalDistance += ParticleDir.Size();
					AddRibbonVert(RenderData, ParticlePos, UVs[0] * UV0ForSegment * UV0Scale + UV0Offset, UVs[0] * UV1ForSegment * UV1Scale + UV1Offset, Color, Twist, Size, NormDir, Align);
					AddRibbonVert(RenderData, ParticlePos, UVs[1] * UV0ForSegment * UV0Scale + UV0Offset, UVs[1] * UV1ForSegment * UV1Scale + UV1Offset, Color, Twist, Size, NormDir, Align);
					if (MaterialParamData.IsValid())
					{
						AddDynamicParam(DynamicData->MaterialParameterVertexData[0], MaterialParamData[Index1]);
						AddDynamicParam(DynamicData->MaterialParameterVertexData[0], MaterialParamData[Index1]);
					}
					if (MaterialParam1Data.IsValid())
					{
						AddDynamicParam(DynamicData->MaterialParameterVertexData[1], MaterialParam1Data[Index1]);
						AddDynamicParam(DynamicData->MaterialParameterVertexData[1], MaterialParam1Data[Index1]);
					}
					if (MaterialParam2Data.IsValid())
					{
						AddDynamicParam(DynamicData->MaterialParameterVertexData[2], MaterialParam2Data[Index1]);
						AddDynamicParam(DynamicData->MaterialParameterVertexData[2], MaterialParam2Data[Index1]);
					}
					if (MaterialParam3Data.IsValid())
					{
						AddDynamicParam(DynamicData->MaterialParameterVertexData[3], MaterialParam3Data[Index1]);
						AddDynamicParam(DynamicData->MaterialParameterVertexData[3], MaterialParam3Data[Index1]);
					}
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
			}
		}
	};
	
	//TODO: Move sorting to share code with sprite and mesh sorting and support the custom sorting key.
	int32 TotalIndices = Data.GetNumInstances();

	if (!bMultiRibbons)
	{
		int32 CurIdx = 0;
		TArray<int32> SortedIndices;
		for (int32 i = 0; i < TotalIndices; ++i)
		{
			SortedIndices.Add(i);
		}

		SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });

		AddRibbonVerts(SortedIndices);
	}
	else
	{
		int32 CurIdx = 0;
		if (bFullIDs)
		{
			TMap<FNiagaraID, TArray<int32>> MultiRibbonSortedIndices;
			
			for (int32 i = 0; i < TotalIndices; ++i)
			{
				TArray<int32>& Indices = MultiRibbonSortedIndices.FindOrAdd(RibbonFullIDData[i]);
				Indices.Add(i);
			}

			for (TPair<FNiagaraID, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				FNiagaraID& ID = Pair.Key;
				TArray<int32>& SortedIndices = Pair.Value;

				SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });
				AddRibbonVerts(SortedIndices);			
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

			for (TPair<int32, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				int32 ID = Pair.Key;
				TArray<int32>& SortedIndices = Pair.Value;

				SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });
				AddRibbonVerts(SortedIndices);
			};
		}
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();

	return DynamicData;
}



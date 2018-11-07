// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRenderer.h"
#include "ParticleResources.h"
#include "ParticleBeamTrailVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"
#include "NiagaraVertexFactory.h"
#include "Engine/Engine.h"

DECLARE_CYCLE_STAT(TEXT("Generate Particle Lights"), STAT_NiagaraGenLights, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Sort Particles"), STAT_NiagaraSortParticles, STATGROUP_Niagara);


/** Enable/disable parallelized System renderers */
int32 GbNiagaraParallelEmitterRenderers = 1;
static FAutoConsoleVariableRef CVarParallelEmitterRenderers(
	TEXT("niagara.ParallelEmitterRenderers"),
	GbNiagaraParallelEmitterRenderers,
	TEXT("Whether to run Niagara System renderers in parallel"),
	ECVF_Default
	);



void FNiagaraDummyRWBufferFloat::InitRHI() 
{
	UE_LOG(LogNiagara, Log, TEXT("FNiagaraDummyRWBufferFloat InitRHI %s"), *DebugId);
	Buffer.Initialize(sizeof(float), 1, EPixelFormat::PF_R32_FLOAT, BUF_Static);
}

void FNiagaraDummyRWBufferFloat::ReleaseRHI() 
{
	UE_LOG(LogNiagara, Log, TEXT("FNiagaraDummyRWBufferFloat ReleaseRHI %s"), *DebugId);
	Buffer.Release();
}

void FNiagaraDummyRWBufferInt::InitRHI() 
{
	UE_LOG(LogNiagara, Log, TEXT("FNiagaraDummyRWBufferInt InitRHI %s"), *DebugId);
	Buffer.Initialize(sizeof(int32), 1, EPixelFormat::PF_R32_SINT, BUF_Static);
}
void FNiagaraDummyRWBufferInt::ReleaseRHI() 
{
	UE_LOG(LogNiagara, Log, TEXT("FNiagaraDummyRWBufferInt ReleaseRHI %s"), *DebugId);
	Buffer.Release();
}


FRWBuffer& NiagaraRenderer::GetDummyFloatBuffer()
{
	check(IsInRenderingThread());
	static TGlobalResource<FNiagaraDummyRWBufferFloat> DummyFloatBuffer(TEXT("NiagaraRenderer::DummyFloat"));
	return DummyFloatBuffer.Buffer;
}

FRWBuffer& NiagaraRenderer::GetDummyIntBuffer()
{
	check(IsInRenderingThread());
	static TGlobalResource<FNiagaraDummyRWBufferInt> DummyIntBuffer(TEXT("NiagaraRenderer::DummyInt"));
	return DummyIntBuffer.Buffer;
}

NiagaraRenderer::NiagaraRenderer()
	: CPUTimeMS(0.0f)
	, bLocalSpace(false)
	, bEnabled(true)
	, DynamicDataRender(nullptr)
	, BaseExtents(1.0f, 1.0f, 1.0f)
{
	Material = UMaterial::GetDefaultMaterial(MD_Surface);
}


NiagaraRenderer::~NiagaraRenderer() 
{
}

void NiagaraRenderer::Release()
{
	check(IsInGameThread());
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		NiagaraRendererDeletion,
		NiagaraRenderer*, Renderer, this,
		{
			delete Renderer;
		}
	);
}

void NiagaraRenderer::SortIndices(ENiagaraSortMode SortMode, int32 SortAttributeOffset, const FNiagaraDataBuffer& Buffer, const FMatrix& LocalToWorld, const FSceneView* View, FNiagaraGlobalReadBuffer::FAllocation& OutIndices)const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSortParticles);

	uint32 NumInstances = Buffer.GetNumInstances();
	check(OutIndices.ReadBuffer->NumBytes >= OutIndices.FirstIndex + NumInstances * sizeof(int32));
	check(SortMode != ENiagaraSortMode::None);
	check(SortAttributeOffset != INDEX_NONE);

	int32* RESTRICT IndexBuffer = (int32*)(OutIndices.Buffer);

	struct FParticleOrder
	{
		int32 Index;
		float Order;
	};
	FMemMark Mark(FMemStack::Get());
	FParticleOrder* RESTRICT ParticleOrder = (FParticleOrder*)FMemStack::Get().Alloc(sizeof(FParticleOrder) * NumInstances, alignof(FParticleOrder));

	auto SortAscending = [](const FParticleOrder& A, const FParticleOrder& B) { return A.Order < B.Order; };
	auto SortDescending = [](const FParticleOrder& A, const FParticleOrder& B) { return A.Order > B.Order; };

	if (SortMode == ENiagaraSortMode::ViewDepth || SortMode == ENiagaraSortMode::ViewDistance)
	{
		float* RESTRICT PositionX = (float*)Buffer.GetComponentPtrFloat(SortAttributeOffset);
		float* RESTRICT PositionY = (float*)Buffer.GetComponentPtrFloat(SortAttributeOffset + 1);
		float* RESTRICT PositionZ = (float*)Buffer.GetComponentPtrFloat(SortAttributeOffset + 2);
		auto GetPos = [&PositionX, &PositionY, &PositionZ](int32 Idx)
		{
			return FVector(PositionX[Idx], PositionY[Idx], PositionZ[Idx]);
		};

		//TODO Parallelize in batches? Move to GPU for large emitters?
		if (SortMode == ENiagaraSortMode::ViewDepth)
		{
			FMatrix ViewProjMatrix = View->ViewMatrices.GetViewProjectionMatrix();
			if (bLocalSpace)
			{
				for (uint32 i = 0; i < NumInstances; ++i)
				{
					ParticleOrder[i].Index = i;
					ParticleOrder[i].Order = ViewProjMatrix.TransformPosition(LocalToWorld.TransformPosition(GetPos(i))).W;
				}
			}
			else
			{
				for (uint32 i = 0; i < NumInstances; ++i)
				{
					ParticleOrder[i].Index = i;
					ParticleOrder[i].Order = ViewProjMatrix.TransformPosition(GetPos(i)).W;
				}
			}

			Sort(ParticleOrder, NumInstances, SortDescending);
		}
		else 
		{
			// check(SortMode == ENiagaraSortMode::ViewDistance); Should always be true because we are within this block. If code is moved or copied, please change.
			FVector ViewOrigin = View->ViewMatrices.GetViewOrigin();
			if (bLocalSpace)
			{
				for (uint32 i = 0; i < NumInstances; ++i)
				{
					ParticleOrder[i].Index = i;
					ParticleOrder[i].Order = (ViewOrigin - LocalToWorld.TransformPosition(GetPos(i))).SizeSquared();
				}
			}
			else
			{
				for (uint32 i = 0; i < NumInstances; ++i)
				{
					ParticleOrder[i].Index = i;
					ParticleOrder[i].Order = (ViewOrigin - GetPos(i)).SizeSquared();
				}
			}

			Sort(ParticleOrder, NumInstances, SortDescending);
		}
	}
	else
	{
		float* RESTRICT CustomSorting = (float*)Buffer.GetComponentPtrFloat(SortAttributeOffset);
		for (uint32 i = 0; i < NumInstances; ++i)
		{
			ParticleOrder[i].Index = i;
			ParticleOrder[i].Order = CustomSorting[i];
		}
		if (SortMode == ENiagaraSortMode::CustomAscending)
		{
			Sort(ParticleOrder, NumInstances, SortAscending);
		}
		else if (SortMode == ENiagaraSortMode::CustomDecending)
		{
			Sort(ParticleOrder, NumInstances, SortDescending);
		}
	}

	//Now transfer to the real index buffer.
	for (uint32 i = 0; i < NumInstances; ++i)
	{
		IndexBuffer[i] = ParticleOrder[i].Index;
	}
}

	
//////////////////////////////////////////////////////////////////////////
// Light renderer

NiagaraRendererLights::NiagaraRendererLights(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) :
	NiagaraRenderer()
{
	Properties = Cast<UNiagaraLightRendererProperties>(InProps);
}


void NiagaraRendererLights::ReleaseRenderThreadResources()
{
}

void NiagaraRendererLights::CreateRenderThreadResources()
{
}



/** Update render data buffer from attributes */
FNiagaraDynamicDataBase *NiagaraRendererLights::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenLights);

	SimpleTimer VertexDataTimer;

	//Bail if we don't have the required attributes to render this emitter.
	if (!bEnabled || !Properties)
	{
		return nullptr;
	}

	//I'm not a great fan of pulling scalar components out to a structured vert buffer like this.
	//TODO: Experiment with a new VF that reads the data directly from the scalar layout.
	FNiagaraDataSetIterator<FVector> PosItr(Data, Properties->PositionBinding.DataSetVariable);
	FNiagaraDataSetIterator<FLinearColor> ColItr(Data, Properties->ColorBinding.DataSetVariable);
	FNiagaraDataSetIterator<float> RadiusItr(Data, Properties->RadiusBinding.DataSetVariable);

	FNiagaraDynamicDataLights *DynamicData = new FNiagaraDynamicDataLights;
	FVector DefaultColor = FVector(Properties->ColorBinding.DefaultValueIfNonExistent.GetValue<FLinearColor>());
	FVector DefaultPos = FVector4(Proxy->GetLocalToWorld().GetOrigin());
	float DefaultRadius = Properties->RadiusBinding.DefaultValueIfNonExistent.GetValue<float>();

	DynamicData->LightArray.Empty();

	for (uint32 ParticleIndex = 0; ParticleIndex < Data.GetNumInstances(); ParticleIndex++)
	{
		SimpleLightData LightData;
		LightData.LightEntry.Radius = (RadiusItr.IsValid() ? (*RadiusItr) :  DefaultRadius) * Properties->RadiusScale;	//LightPayload->RadiusScale * (Size.X + Size.Y) / 2.0f;
		LightData.LightEntry.Color = (ColItr.IsValid() ? FVector((*ColItr)) : DefaultColor) + Properties->ColorAdd;				//FVector(Particle.Color) * Particle.Color.A * LightPayload->ColorScale;
		LightData.LightEntry.Exponent = 1.0;
		LightData.LightEntry.bAffectTranslucency = true;
		LightData.PerViewEntry.Position = PosItr.IsValid() ? (*PosItr) : DefaultPos;

		DynamicData->LightArray.Add(LightData);

		PosItr.Advance();
		ColItr.Advance();
		RadiusItr.Advance();
	}

	if (bLocalSpace)
	{
		FMatrix Mat = Proxy->GetLocalToWorld();
		for (uint32 ParticleIndex = 0; ParticleIndex < Data.GetNumInstances(); ParticleIndex++)
		{
			DynamicData->LightArray[ParticleIndex].PerViewEntry.Position = Mat.TransformPosition(DynamicData->LightArray[ParticleIndex].PerViewEntry.Position);
		}
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();

	return DynamicData;
}


void NiagaraRendererLights::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{

}

void NiagaraRendererLights::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataLights*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = static_cast<FNiagaraDynamicDataLights*>(NewDynamicData);
}

int NiagaraRendererLights::GetDynamicDataSize()
{
	return 0;
}
bool NiagaraRendererLights::HasDynamicData()
{
	return false;
}

bool NiagaraRendererLights::SetMaterialUsage()
{
	return false;
}

void NiagaraRendererLights::TransformChanged()
{
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererLights::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererLights::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif




//////////////////////////////////////////////////////////////////////////
// FNiagaraGlobalReadBuffer
//////////////////////////////////////////////////////////////////////////

int32 GMaxNiagaraRenderingBytesAllocatedPerFrame = 32 * 1024 * 1024;

FAutoConsoleVariableRef CVarMaxNiagaraRenderingBytesAllocatedPerFrame(
	TEXT("fx.MaxRenderingBytesAllocatedPerFrame"),
	GMaxNiagaraRenderingBytesAllocatedPerFrame,
	TEXT("The maximum number of transient rendering buffer bytes to allocate before we start panic logging who is doing the allocations"));


int32 GMinNiagaraRenderingBufferSize = 8 * 1024;
FAutoConsoleVariableRef CVarMinNiagaraRenderingBufferSize(
	TEXT("fx.MinNiagaraRenderingBufferSize"),
	GMinNiagaraRenderingBufferSize,
	TEXT("The minimum size (in instances) to allocate in blocks for niagara rendering buffers."));


struct FDynamicReadBufferPool
{
	/** List of vertex buffers. */
	TIndirectArray<FDynamicAllocReadBuffer> Buffers;
	/** The current buffer from which allocations are being made. */
	FDynamicAllocReadBuffer* CurrentBuffer;

	/** Default constructor. */
	FDynamicReadBufferPool()
		: CurrentBuffer(NULL)
	{
	}

	/** Destructor. */
	~FDynamicReadBufferPool()
	{
		int32 NumVertexBuffers = Buffers.Num();
		for (int32 BufferIndex = 0; BufferIndex < NumVertexBuffers; ++BufferIndex)
		{
			Buffers[BufferIndex].Release();
		}
	}

	FCriticalSection CriticalSection;
};



FNiagaraGlobalReadBuffer::FNiagaraGlobalReadBuffer()
	: TotalAllocatedSinceLastCommit(0)
{
	FloatBufferPool = new FDynamicReadBufferPool();
	Int32BufferPool = new FDynamicReadBufferPool();

	CommitCallbackHandle = GEngine->GetPreRenderDelegate().AddRaw(this, &FNiagaraGlobalReadBuffer::Commit);
}

FNiagaraGlobalReadBuffer::~FNiagaraGlobalReadBuffer()
{
	Cleanup();

	//GEngine->GetPostInitViewsDelegate().Remove(CommitCallbackHandle);
}

void FNiagaraGlobalReadBuffer::Cleanup()
{
	if (FloatBufferPool)
	{
		UE_LOG(LogNiagara, Log, TEXT("FNiagaraGlobalReadBuffer::Cleanup()"));
		delete FloatBufferPool;
		FloatBufferPool = nullptr;
	}

	if (Int32BufferPool)
	{
		delete Int32BufferPool;
		Int32BufferPool = nullptr;
	}
}
void FNiagaraGlobalReadBuffer::InitRHI()
{
	UE_LOG(LogNiagara, Log, TEXT("FNiagaraGlobalReadBuffer::InitRHI"));
}

void FNiagaraGlobalReadBuffer::ReleaseRHI() 
{
	UE_LOG(LogNiagara, Log, TEXT("FNiagaraGlobalReadBuffer::ReleaseRHI"));
	Cleanup();
}

FNiagaraGlobalReadBuffer::FAllocation FNiagaraGlobalReadBuffer::AllocateFloat(uint32 Num)
{
	FScopeLock ScopeLock(&FloatBufferPool->CriticalSection);

	FAllocation Allocation;

	TotalAllocatedSinceLastCommit += Num;
	if (IsRenderAlarmLoggingEnabled())
	{
		UE_LOG(LogNiagara, Warning, TEXT("FNiagaraGlobalReadBuffer::AllocateFloat(%u), will have allocated %u total this frame"), Num, TotalAllocatedSinceLastCommit);
	}
	uint32 SizeInBytes = sizeof(float) * Num;
	FDynamicAllocReadBuffer* Buffer = FloatBufferPool->CurrentBuffer;
	if (Buffer == NULL || Buffer->AllocatedByteCount + SizeInBytes > Buffer->NumBytes)
	{
		// Find a buffer in the pool big enough to service the request.
		Buffer = NULL;
		for (int32 BufferIndex = 0, NumBuffers = FloatBufferPool->Buffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
		{
			FDynamicAllocReadBuffer& BufferToCheck = FloatBufferPool->Buffers[BufferIndex];
			if (BufferToCheck.AllocatedByteCount + SizeInBytes <= BufferToCheck.NumBytes)
			{
				Buffer = &BufferToCheck;
				break;
			}
		}

		// Create a new vertex buffer if needed.
		if (Buffer == NULL)
		{
			uint32 NewBufferSize = FMath::Max(Num, (uint32)GMinNiagaraRenderingBufferSize);
			Buffer = new(FloatBufferPool->Buffers) FDynamicAllocReadBuffer();
			Buffer->Initialize(sizeof(float), NewBufferSize, PF_R32_FLOAT, BUF_Dynamic);
		}

		// Lock the buffer if needed.
		if (Buffer->MappedBuffer == NULL)
		{
			Buffer->Lock();
		}

		// Remember this buffer, we'll try to allocate out of it in the future.
		FloatBufferPool->CurrentBuffer = Buffer;
	}

	check(Buffer != NULL);
	checkf(Buffer->AllocatedByteCount + SizeInBytes <= Buffer->NumBytes, TEXT("Niagara global float buffer allocation failed: BufferSize=%d AllocatedByteCount=%d SizeInBytes=%d"), Buffer->NumBytes, Buffer->AllocatedByteCount, SizeInBytes);
	Allocation.Buffer = Buffer->MappedBuffer + Buffer->AllocatedByteCount;
	Allocation.ReadBuffer = Buffer;
	Allocation.FirstIndex = Buffer->AllocatedByteCount;
	Buffer->AllocatedByteCount += SizeInBytes;

	return Allocation;
}

FNiagaraGlobalReadBuffer::FAllocation FNiagaraGlobalReadBuffer::AllocateInt32(uint32 Num)
{
	FScopeLock ScopeLock(&Int32BufferPool->CriticalSection);
	FAllocation Allocation;

	TotalAllocatedSinceLastCommit += Num;
	if (IsRenderAlarmLoggingEnabled())
	{
		UE_LOG(LogNiagara, Warning, TEXT("FNiagaraGlobalReadBuffer::AllocateInt32(%u), will have allocated %u total this frame"), Num, TotalAllocatedSinceLastCommit);
	}
	uint32 SizeInBytes = sizeof(int32) * Num;
	FDynamicAllocReadBuffer* Buffer = Int32BufferPool->CurrentBuffer;
	if (Buffer == NULL || Buffer->AllocatedByteCount + SizeInBytes > Buffer->NumBytes)
	{
		// Find a buffer in the pool big enough to service the request.
		Buffer = NULL;
		for (int32 BufferIndex = 0, NumBuffers = Int32BufferPool->Buffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
		{
			FDynamicAllocReadBuffer& BufferToCheck = Int32BufferPool->Buffers[BufferIndex];
			if (BufferToCheck.AllocatedByteCount + SizeInBytes <= BufferToCheck.NumBytes)
			{
				Buffer = &BufferToCheck;
				break;
			}
		}

		// Create a new vertex buffer if needed.
		if (Buffer == NULL)
		{
			uint32 NewBufferSize = FMath::Max(Num, (uint32)GMinNiagaraRenderingBufferSize);
			Buffer = new(Int32BufferPool->Buffers) FDynamicAllocReadBuffer();
			Buffer->Initialize(sizeof(int32), NewBufferSize, PF_R32_SINT, BUF_Dynamic);
		}

		// Lock the buffer if needed.
		if (Buffer->MappedBuffer == NULL)
		{
			Buffer->Lock();
		}

		// Remember this buffer, we'll try to allocate out of it in the future.
		Int32BufferPool->CurrentBuffer = Buffer;
	}

	check(Buffer != NULL);
	checkf(Buffer->AllocatedByteCount + SizeInBytes <= Buffer->NumBytes, TEXT("Niagara global int32 buffer allocation failed: BufferSize=%d AllocatedByteCount=%d SizeInBytes=%d"), Buffer->NumBytes, Buffer->AllocatedByteCount, SizeInBytes);
	Allocation.Buffer = Buffer->MappedBuffer + Buffer->AllocatedByteCount;
	Allocation.ReadBuffer = Buffer;
	Allocation.FirstIndex = Buffer->AllocatedByteCount;
	Buffer->AllocatedByteCount += SizeInBytes;

	return Allocation;
}

bool FNiagaraGlobalReadBuffer::IsRenderAlarmLoggingEnabled() const
{
	return GMaxNiagaraRenderingBytesAllocatedPerFrame > 0 && TotalAllocatedSinceLastCommit >= (size_t)GMaxNiagaraRenderingBytesAllocatedPerFrame;
}

void FNiagaraGlobalReadBuffer::Commit()
{
	for (int32 BufferIndex = 0, NumBuffers = FloatBufferPool->Buffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
	{
		FDynamicAllocReadBuffer& Buffer = FloatBufferPool->Buffers[BufferIndex];
		if (Buffer.MappedBuffer != NULL)
		{
			Buffer.Unlock();
		}
	}
	FloatBufferPool->CurrentBuffer = NULL;
	for (int32 BufferIndex = 0, NumBuffers = Int32BufferPool->Buffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
	{
		FDynamicAllocReadBuffer& Buffer = Int32BufferPool->Buffers[BufferIndex];
		if (Buffer.MappedBuffer != NULL)
		{
			Buffer.Unlock();
		}
	}
	Int32BufferPool->CurrentBuffer = NULL;
	TotalAllocatedSinceLastCommit = 0;
}


FNiagaraGlobalReadBuffer& FNiagaraGlobalReadBuffer::Get()
{
	check(IsInRenderingThread());
	static TGlobalResource<FNiagaraGlobalReadBuffer> GlobalDynamicReadBuffer;
	return GlobalDynamicReadBuffer;
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureChunkManager.h"

#include "RHI.h"
#include "ScreenRendering.h"
#include "UploadingVirtualTexture.h"
#include "VT/VirtualTexture.h"
#include "VirtualTextureBuiltData.h"
#include "BlockCodingHelpers.h"
#include "CrunchCompression.h"
#include "FileCache/FileCache.h"
#include "Containers/LruCache.h"

#if WITH_EDITOR
#include "VirtualTextureChunkDDCCache.h"
#endif

static int32 NumTranscodeRequests = 128;
static FAutoConsoleVariableRef CVarNumTranscodeRequests(
	TEXT("r.VT.NumTranscodeRequests"),
	NumTranscodeRequests,
	TEXT("Number of transcode request that can be in flight. default 128\n")
	, ECVF_ReadOnly
);

static FVirtualTextureChunkStreamingManager* VirtualTexturePageStreamingManager = nullptr;
struct FVirtualTextureChunkStreamingManager& FVirtualTextureChunkStreamingManager::Get()
{
	if (VirtualTexturePageStreamingManager == nullptr)
	{
		VirtualTexturePageStreamingManager = new FVirtualTextureChunkStreamingManager();
	}
	return *VirtualTexturePageStreamingManager;
}

FVirtualTextureChunkStreamingManager::FVirtualTextureChunkStreamingManager()
{
	IStreamingManager::Get().AddStreamingManager(this);
#if WITH_EDITOR
	GetVirtualTextureChunkDDCCache()->Initialize();
#endif
}

FVirtualTextureChunkStreamingManager::~FVirtualTextureChunkStreamingManager()
{
#if WITH_EDITOR
	GetVirtualTextureChunkDDCCache()->ShutDown();
#endif
	IStreamingManager::Get().RemoveStreamingManager(this);
}

void FVirtualTextureChunkStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything /*= false*/)
{
	FVirtualTextureChunkStreamingManager* StreamingManager = this;

	ENQUEUE_RENDER_COMMAND(UpdateVirtualTextureStreaming)(
		[StreamingManager](FRHICommandListImmediate& RHICmdList)
	{
#if WITH_EDITOR
		GetVirtualTextureChunkDDCCache()->UpdateRequests();
#endif // WITH_EDITOR
		StreamingManager->TranscodeCache.RetireOldTasks(StreamingManager->UploadCache);
		StreamingManager->UploadCache.UpdateFreeList();

		FVirtualTextureCodec::RetireOldCodecs();
	});
}

int32 FVirtualTextureChunkStreamingManager::BlockTillAllRequestsFinished(float TimeLimit /*= 0.0f*/, bool bLogResults /*= false*/)
{
	int32 Result = 0;
	return Result;
}

void FVirtualTextureChunkStreamingManager::CancelForcedResources()
{

}

static EAsyncIOPriorityAndFlags GetAsyncIOPriority(EVTRequestPagePriority Priority)
{
	switch (Priority)
	{
	case EVTRequestPagePriority::High: return AIOP_High;
	case EVTRequestPagePriority::Normal: return AIOP_Normal;
	default: check(false); return AIOP_Normal;
	}
}

FVTRequestPageResult FVirtualTextureChunkStreamingManager::RequestTile(FUploadingVirtualTexture* VTexture, const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint32 vAddress, EVTRequestPagePriority Priority)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_RequestTile);

	const FVirtualTextureBuiltData* VTData = VTexture->GetVTData();
	const uint32 TileIndex = VTData->GetTileIndex(vLevel, vAddress);
	const int32 ChunkIndex = VTData->GetChunkIndex(TileIndex);
	if (ChunkIndex == -1)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VT.Verbose"));
		if (CVar->GetValueOnRenderThread())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("vAddr %i@%i has an invalid tile (-1)."), vAddress, vLevel);
		}

		return EVTRequestPageStatus::Invalid;
	}

	// tile is being transcoded/is done transcoding
	const FVTTranscodeKey TranscodeKey = FVirtualTextureTranscodeCache::GetKey(ProducerHandle, LayerMask, vLevel, vAddress);
	FVTTranscodeTileHandle TranscodeHandle = TranscodeCache.FindTask(TranscodeKey);
	if (TranscodeHandle.IsValid())
	{
		const EVTRequestPageStatus Status = TranscodeCache.IsTaskFinished(TranscodeHandle) ? EVTRequestPageStatus::Available : EVTRequestPageStatus::Pending;
		return FVTRequestPageResult(Status, TranscodeHandle.PackedData);
	}

	// we limit the number of pending upload tiles in order to limit the memory required to store all the staging buffers
	if (UploadCache.GetNumPendingTiles() >= (uint32)NumTranscodeRequests)
	{
		INC_DWORD_STAT(STAT_VTP_NumTranscodeDropped);
		return EVTRequestPageStatus::Saturated;
	}

	const EAsyncIOPriorityAndFlags AsyncIOPriority = GetAsyncIOPriority(Priority);
	FGraphEventArray GraphCompletionEvents;
	const FVTCodecAndStatus CodecResult = VTexture->GetCodecForChunk(GraphCompletionEvents, ChunkIndex, AsyncIOPriority);
	if (!VTRequestPageStatus_HasData(CodecResult.Status))
	{
		// May fail to get codec if the file cache is saturated
		return CodecResult.Status;
	}

	uint32 MinLayerIndex = ~0u;
	uint32 MaxLayerIndex = 0u;
	for (uint32 LayerIndex = 0u; LayerIndex < VTData->GetNumLayers(); ++LayerIndex)
	{
		if (LayerMask & (1u << LayerIndex))
		{
			MinLayerIndex = FMath::Min(MinLayerIndex, LayerIndex);
			MaxLayerIndex = LayerIndex;
		}
	}

	// make a single read request that covers region of all requested tiles
	const uint32 OffsetStart = VTData->GetTileOffset(ChunkIndex, TileIndex + MinLayerIndex);
	const uint32 OffsetEnd = VTData->GetTileOffset(ChunkIndex, TileIndex + MaxLayerIndex + 1u);
	const uint32 RequestSize = OffsetEnd - OffsetStart;

	const FVTDataAndStatus TileDataResult = VTexture->ReadData(GraphCompletionEvents, ChunkIndex, OffsetStart, RequestSize, AsyncIOPriority);
	if (!VTRequestPageStatus_HasData(TileDataResult.Status))
	{
		return TileDataResult.Status;
	}
	check(TileDataResult.Data);

	FVTTranscodeParams TranscodeParams;
	TranscodeParams.Data = TileDataResult.Data;
	TranscodeParams.VTData = VTData;
	TranscodeParams.ChunkIndex = ChunkIndex;
	TranscodeParams.vAddress = vAddress;
	TranscodeParams.vLevel = vLevel;
	TranscodeParams.LayerMask = LayerMask;
	TranscodeParams.Codec = CodecResult.Codec;
	TranscodeHandle = TranscodeCache.SubmitTask(UploadCache, TranscodeKey, TranscodeParams, &GraphCompletionEvents);
	return FVTRequestPageResult(EVTRequestPageStatus::Pending, TranscodeHandle.PackedData);
}

IVirtualTextureFinalizer* FVirtualTextureChunkStreamingManager::ProduceTile(FRHICommandListImmediate& RHICmdList, uint32 SkipBorderSize, uint8 NumLayers, uint8 LayerMask, uint64 RequestHandle, const FVTProduceTargetLayer* TargetLayers)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_ProduceTile);

	const FVTUploadTileHandle* StageTileHandles = TranscodeCache.AcquireTaskResult(FVTTranscodeTileHandle(RequestHandle));
	for (uint32 LayerIndex = 0u; LayerIndex < NumLayers; ++LayerIndex)
	{
		if (LayerMask & (1u << LayerIndex))
		{
			const FVTProduceTargetLayer& Target = TargetLayers[LayerIndex];
			UploadCache.SubmitTile(RHICmdList, StageTileHandles[LayerIndex], Target.TextureRHI->GetTexture2D(), Target.pPageLocation.X, Target.pPageLocation.Y, SkipBorderSize);
		}
	}

	return &UploadCache;
}

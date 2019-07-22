// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VirtualTextureBuiltData.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/MemoryReadStream.h"
#include "Containers/BitArray.h"
#include "Containers/List.h"
#include "Templates/UniquePtr.h"

class FUploadingVirtualTexture;
class IFileCacheHandle;

class FVirtualTextureCodec : public TIntrusiveLinkedList<FVirtualTextureCodec>
{
public:
	static void RetireOldCodecs();

	~FVirtualTextureCodec();
	void Init(IMemoryReadStreamRef& HeaderData);

	inline bool IsComplete() const { return !CompletedEvent || CompletedEvent->IsComplete(); }

	void LinkGlobalHead();
	void LinkGlobalTail();

	static FVirtualTextureCodec* ListHead;
	static FVirtualTextureCodec ListTail;
	static uint32 NumCodecs;

	FGraphEventRef CompletedEvent;

	class FUploadingVirtualTexture* Owner;
	void* Contexts[VIRTUALTEXTURE_DATA_MAXLAYERS] = { nullptr };
	uint32 ChunkIndex = 0u;
	uint32 LastFrameUsed = 0u;
};

struct FVTCodecAndStatus
{
	FVTCodecAndStatus(EVTRequestPageStatus InStatus, const FVirtualTextureCodec* InCodec = nullptr) : Codec(InCodec), Status(InStatus) {}
	const FVirtualTextureCodec* Codec;
	EVTRequestPageStatus Status;
};

struct FVTDataAndStatus
{
	FVTDataAndStatus(EVTRequestPageStatus InStatus, const IMemoryReadStreamRef& InData = nullptr) : Data(InData), Status(InStatus) {}
	IMemoryReadStreamRef Data;
	EVTRequestPageStatus Status;
};

// IVirtualTexture implementation that is uploading from CPU to GPU and gets its data from a FChunkProvider
class FUploadingVirtualTexture : public IVirtualTexture
{
public:
	FUploadingVirtualTexture(FVirtualTextureBuiltData* InData, int32 FirstMipToUse);
	virtual ~FUploadingVirtualTexture();

	// IVirtualTexture interface
	virtual uint32 GetLocalMipBias(uint8 vLevel, uint32 vAddress) const override;
	virtual FVTRequestPageResult RequestPageData(const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint32 vAddress, EVTRequestPagePriority Priority) override;
	virtual IVirtualTextureFinalizer* ProducePageData(FRHICommandListImmediate& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint32 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers) override;
	virtual void DumpToConsole(bool verbose) override;
	// End IVirtualTexture interface

	inline const FVirtualTextureBuiltData* GetVTData() const { return Data; }

	// gets the codec for the given chunk, data is not valid until returned OutCompletionEvents are complete
	FVTCodecAndStatus GetCodecForChunk(FGraphEventArray& OutCompletionEvents, uint32 ChunkIndex, EAsyncIOPriorityAndFlags Priority);

	// read a portion of a chunk
	FVTDataAndStatus ReadData(FGraphEventArray& OutCompletionEvents, uint32 ChunkIndex, size_t Offset, size_t Size, EAsyncIOPriorityAndFlags Priority);

private:
	friend class FVirtualTextureCodec;

	FVirtualTextureBuiltData* Data;
	TArray< TUniquePtr<IFileCacheHandle> > HandlePerChunk;
	TArray< TUniquePtr<FVirtualTextureCodec> > CodecPerChunk;
	TBitArray<> InvalidChunks; // marks chunks that failed to load
	int32 FirstMipOffset;
};

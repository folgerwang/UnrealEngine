#pragma once

#include "VirtualTextureTypes.h"
#include "ContentStreaming.h"
#include "Containers/Queue.h"
#include "Containers/List.h"
#include "VirtualTextureBuiltData.h"

class FChunkProvider;
struct FVirtualTextureChunkStreamingManager;
class FUploadingVirtualTexture;
class IFileCacheReadBuffer;

DECLARE_STATS_GROUP(TEXT("Virtual Texturing Paging"), STATGROUP_VTP, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("RequestTile"), STAT_VTP_RequestTile, STATGROUP_VTP);
DECLARE_CYCLE_STAT(TEXT("MapTile"), STAT_VTP_Map, STATGROUP_VTP);
DECLARE_CYCLE_STAT(TEXT("UnMapTile"), STAT_VTP_UnMap, STATGROUP_VTP);
DECLARE_CYCLE_STAT(TEXT("upload"), STAT_VTP_Upload, STATGROUP_VTP);
DECLARE_CYCLE_STAT(TEXT("stage upload"), STAT_VTP_StageUpload, STATGROUP_VTP);
DECLARE_CYCLE_STAT(TEXT("flush upload"), STAT_VTP_FlushUpload, STATGROUP_VTP);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num generates"), STAT_VTP_NumGenerate, STATGROUP_VTP);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num transcodes"), STAT_VTP_NumTranscode, STATGROUP_VTP);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num transcodes dropped"), STAT_VTP_NumTranscodeDropped, STATGROUP_VTP);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num transcodes retired"), STAT_VTP_NumTranscodeRetired, STATGROUP_VTP);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Intraframe upload flushes"), STAT_VTP_NumIntraFrameFlush, STATGROUP_VTP);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num uploads"), STAT_VTP_NumUploads, STATGROUP_VTP);

struct FTranscodeRequest 
{
	TileID id = INVALID_TILE_ID;
	FGraphEventRef event;
	void* memory = nullptr;
	IFileCacheReadBuffer* header = nullptr;
	ChunkID globalChunkid = INVALID_CHUNK_ID;
	uint64 FrameID = 0;
	int32 mapped = 0;
	IFileCacheReadBuffer *Data;

	void Reset();

	void Init(TileID tid, size_t size, IFileCacheReadBuffer *SetData, ChunkID gci, IFileCacheReadBuffer* inHeader)
	{
		ensure(memory == nullptr);
		memory = FMemory::Malloc(size);
		Data = SetData;
		id = tid;
		ensure(inHeader);
		header = inHeader;
		globalChunkid = gci;
	}
};

static int32 TranscodeRetireAge = 60; //1 frames @ 60 fps
static FAutoConsoleVariableRef CVarVTTranscodeRetireAge(
	TEXT("r.VT.TranscodeRetireAge"),
	TranscodeRetireAge,
	TEXT("If a VT transcode request is not picked up after this number of frames, drop it and put request in cache as free. default 60\n")
	, ECVF_Default
);

class FTranscodeCache
{
public:
	void Init(uint32 MaxNumRequests)
	{
		Capacity = MaxNumRequests;
		requests.AddDefaulted(Capacity);
		for (uint32 i = 0; i < Capacity; ++i)
		{
			lru.AddTail(&requests[i]);
		}
	}

	FTranscodeRequest* Get()
	{
		if (lru.Num())
		{
			INC_DWORD_STAT(STAT_VTP_NumTranscode);
			auto node = lru.GetHead();
			FTranscodeRequest* value = node->GetValue();
			lru.RemoveNode(node);
			activeRequests.Add(value);
			return value;
		}
		else
		{
			return nullptr;
		}
	}

	void Free(FTranscodeRequest* req)
	{
		req->Reset();
		lru.AddTail(req);
		DEC_DWORD_STAT(STAT_VTP_NumTranscode);
	}

	void* Map(const TileID& id)
	{
		FTranscodeRequest* treq = FindRequestForTile(id, true);
		checkf(treq, TEXT("0x%p"), treq);
		ensure(treq->event->IsComplete() == true);
		ensure(treq->mapped >= 0);
		treq->mapped++;
		return treq->memory;
	}

	void Unmap(const TileID& id)
	{
		FTranscodeRequest* treq = FindRequestForTile(id);
		checkf(treq, TEXT("0x%p"), treq);
		ensure(treq->mapped > 0);
		treq->mapped--;
		if (treq->mapped <= 0)
		{
			Free(treq);
		}
	}

	void RetireOldRequests()
	{
		static TArray<FTranscodeRequest*> temp;

		temp.Empty();
		for (auto request : activeRequests)
		{
			if (request->event->IsComplete() == false || request->mapped > 0)
			{
				continue;
			}
			
			if (request->FrameID == 0) //must be the first frame that this is done
			{
				request->FrameID = frameIndex;
				continue;
			}

			if (frameIndex > request->FrameID + TranscodeRetireAge) // if after TranscodeRetireAge frames not picked up, move to lru (and recycle probably)
			{
				//retire
				INC_DWORD_STAT(STAT_VTP_NumTranscodeRetired);
				temp.Add(request);
				ensure(request->mapped == false);
				Free(request);
			}
		}
		for (auto r : temp)
		{
			activeRequests.RemoveSwap(r);
		}

		frameIndex++;
	}

	FTranscodeRequest* FindRequestForTile(const TileID& id, bool removeFromRetirelist = false)
	{
		FTranscodeRequest* request = requests.FindByPredicate([&id](const FTranscodeRequest& req) -> bool {return req.id == id; });
		if (request)
		{
			if (removeFromRetirelist && activeRequests.Contains(request))
			{
				activeRequests.RemoveSwap(request);
			}
			return request;
		}
		else
		{
			return nullptr;
		}
	}

private:
	uint32 Capacity = 0;
	TArray<FTranscodeRequest> requests;
	TArray<FTranscodeRequest*> activeRequests;
	TDoubleLinkedList<FTranscodeRequest*> lru;
	uint64 frameIndex = 0;
};

struct FVirtualTextureCodec
{
	void* Contexts[MAX_NUM_LAYERS] = { nullptr };
	EVirtualTextureCodec Codecs[MAX_NUM_LAYERS] = { EVirtualTextureCodec::Max };
	uint8* HeaderData = nullptr;
	bool Initialized = false;

	void Init(FChunkProvider* Provider, IFileCacheReadBuffer *Data);
	~FVirtualTextureCodec();
};

struct FVirtualTextureChunkStreamingManager final  : public IStreamingManager
{
	/*RENDERER_API*/ static struct FVirtualTextureChunkStreamingManager& Get();
private:
	FVirtualTextureChunkStreamingManager();
	virtual ~FVirtualTextureChunkStreamingManager();

public:
	// IStreamingManager interface
	virtual void UpdateResourceStreaming(float DeltaTime, bool bProcessEverything = false) override;
	virtual int32 BlockTillAllRequestsFinished(float TimeLimit = 0.0f, bool bLogResults = false) override;
	virtual void CancelForcedResources() override;
	virtual void NotifyLevelChange() override {}
	virtual void SetDisregardWorldResourcesForFrames(int32 NumFrames) override {}
	virtual void AddLevel(class ULevel* Level) override {}
	virtual void RemoveLevel(class ULevel* Level) override {}
	virtual void NotifyLevelOffset(class ULevel* Level, const FVector& Offset) override {}
	// End IStreamingManager interface


	// Add/Remove a PageProvider. primary way of adding VTextures to the system atm
	// this will create a FUploadingVirtualTexture
	void AddChunkProvider(FChunkProvider* Provider);
	void RemoveChunkProvider(FChunkProvider* Provider);

	// Start loading a specific page. returns true if the tile is ready for uploading
	bool RequestTile(FUploadingVirtualTexture* VTexture, const TileID& id);
	// Map a page for reading (aka uploading to gpu)
	void MapTile(const TileID& id, void* RESTRICT& outData) ;
	void UnMapTile(const TileID& id);

private:
	TMap<FUploadingVirtualTexture*, FChunkProvider*> VirtualTextures;
	FTranscodeCache TranscodeCache;
	FCriticalSection cs;
};


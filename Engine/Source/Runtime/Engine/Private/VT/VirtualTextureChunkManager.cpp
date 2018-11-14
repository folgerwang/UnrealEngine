#include "VirtualTextureChunkManager.h"

#include "RHI.h"
#include "ScreenRendering.h"
#include "UploadingVirtualTexture.h"
#include "VT/VirtualTexture.h"
#include "VT/VirtualTextureSpace.h"
#include "VirtualTextureBuiltData.h"
#include "VirtualTextureChunkProviders.h"
#include "BlockCodingHelpers.h"

#ifndef CRUNCH_SUPPORT
#define CRUNCH_SUPPORT 0
#endif

#if CRUNCH_SUPPORT
#include "CrunchCompression.h"
#endif
#include "FileCache/FileCache.h"
#include "Containers/LruCache.h"

static int32 NumTranscodeRequests = 128;
static FAutoConsoleVariableRef CVarNumTranscodeRequests(
	TEXT("r.VT.NumTranscodeRequests"),
	NumTranscodeRequests,
	TEXT("Number of transcode request that can be in flight. default 128\n")
	, ECVF_ReadOnly
);

static int32 TLSTranscodeCodecCacheSize = 16;
static FAutoConsoleVariableRef CVarTLSTranscodeCodecCacheSize(
	TEXT("r.VT.TLSTranscodeCodecCacheSize"),
	TLSTranscodeCodecCacheSize,
	TEXT("Number of transcode codecs inside each TLS LRU cache. default 32\n")
	, ECVF_ReadOnly
);


namespace TextureBorderGenerator
{
	static int32 Enabled = 0;
	static FAutoConsoleVariableRef CVarEnableDebugBorders(
		TEXT("r.VT.Borders"),
		Enabled,
		TEXT("If > 0, debug borders will enabled\n")
		/*,ECVF_ReadOnly*/
	);
}

uint32 GTranscodingCacheTLSIndex = ~0;
struct FTranscodeCodecCache
{
	using LRUKey = ChunkID;
	TLruCache<LRUKey, TSharedPtr<FVirtualTextureCodec>> Cache;

	FTranscodeCodecCache()
	{
		Cache.Empty(TLSTranscodeCodecCacheSize);
	}

	FVirtualTextureCodec* GetCodec(FChunkProvider* Provider, ChunkID Id, IFileCacheReadBuffer* PageBufferData)
	{
		auto codec = Cache.FindAndTouch(Id);
		if (codec)
		{
			return codec->Get();
		}
		
		TSharedPtr<FVirtualTextureCodec> newCodec = MakeShared<FVirtualTextureCodec>();
		Cache.Add(Id, newCodec);
		newCodec->Init(Provider, PageBufferData);
		return newCodec.Get();
	}
};

class TranscodeJob
{
public:

	TranscodeJob(FChunkProvider* provider, uint8 level, FTranscodeRequest* request)
		: Provider(provider), vLevel(level), Request(request)
	{}

	FTranscodeCodecCache* GetTLS()
	{
		checkSlow(FPlatformTLS::IsValidTlsSlot(GTranscodingCacheTLSIndex));
		FTranscodeCodecCache* TLS = (FTranscodeCodecCache*)FPlatformTLS::GetTlsValue(GTranscodingCacheTLSIndex);
		if (!TLS)
		{
			TLS = new FTranscodeCodecCache();
			FPlatformTLS::SetTlsValue(GTranscodingCacheTLSIndex, TLS);
		}
		return TLS;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(Request->Data);

		const uint32 TileSize = Provider->GetTilePixelSize();
		const uint32 TileBorderSize = Provider->GetTileBorderSize();
		uint8* OutBuffer = (uint8*)Request->memory;
		uint8* InputBuffer = (uint8 *)Request->Data->GetData();

		FTranscodeCodecCache* CodecCache = GetTLS();
		auto Codec = CodecCache->GetCodec(Provider, Request->globalChunkid, Request->header);

		for (uint32 Layer = 0; Layer < Provider->GetNumLayers(); ++Layer)
		{
			const EPixelFormat LayerFormat = Provider->GetSpace()->GetPhysicalTextureFormat(Layer);
			const uint32 TilePitch = FMath::DivideAndRoundUp(TileSize, (uint32)GPixelFormats[LayerFormat].BlockSizeX) * GPixelFormats[LayerFormat].BlockBytes;
			const size_t TileMemSize = TilePitch * (TileSize / (uint32)GPixelFormats[LayerFormat].BlockSizeY);

			static uint8 Black[4] = { 0,0,0,0 };
			static uint8 OpaqueBlack[4] = { 0,0,0,255 };
			static uint8 White[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
			static uint8 Flat[4] = { 127,127,255,255 };

			EVirtualTextureCodec VTCodec = Codec->Codecs[Layer];
			uint32 tileLayerSize = Provider->GetTileLayerSize(Request->id, Layer);

			switch (VTCodec)
			{
			case EVirtualTextureCodec::Black:
				UniformColorPixels(OutBuffer, TileSize, TileSize, LayerFormat, vLevel, Black);
				break;
			case EVirtualTextureCodec::OpaqueBlack:
				UniformColorPixels(OutBuffer, TileSize, TileSize, LayerFormat, vLevel, OpaqueBlack);
				break;
			case EVirtualTextureCodec::White:
				UniformColorPixels(OutBuffer, TileSize, TileSize, LayerFormat, vLevel, White);
				break;
			case EVirtualTextureCodec::Flat:
				UniformColorPixels(OutBuffer, TileSize, TileSize, LayerFormat, vLevel, Flat);
				break;
			case EVirtualTextureCodec::RawGPU:
				FMemory::Memcpy(OutBuffer, InputBuffer, TileMemSize);
				break;
			case EVirtualTextureCodec::Crunch:
			{
#if CRUNCH_SUPPORT
				const bool result = CrunchCompression::Decode(Codec->Contexts[Layer], (uint8*)InputBuffer, tileLayerSize, OutBuffer, TileMemSize, TilePitch);
				check(result);
#endif
				check(false);
				break;
			}
			case EVirtualTextureCodec::ZippedGPU:
			default:
				//checkf(false, TEXT("Unknown codec id"));
				UniformColorPixels(OutBuffer, TileSize, TileSize, LayerFormat, vLevel, Black);
			}

			// Bake debug borders directly into the tile pixels
			if (TextureBorderGenerator::Enabled)
			{
				BakeDebugInfo(OutBuffer, TileSize, TileSize, TileBorderSize + 4, LayerFormat, vLevel);
			}

			OutBuffer += TileMemSize;
			InputBuffer += tileLayerSize;
		}

		// We're done with the compressed data
		// The uncompressed data will be freed once it's uploaded to the GPU
 		delete Request->Data;
 		Request->Data = nullptr;	
		delete Request->header;
		Request->header = nullptr;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(TranscodeJob, STATGROUP_VTP);
	}
private:
	FChunkProvider* Provider;
	uint8 vLevel;
	FTranscodeRequest* Request;
};

void FTranscodeRequest::Reset()
{
	if (memory)
	{
		FMemory::Free(memory);
		memory = nullptr;
	}
	event = nullptr;
	if (Data) delete Data;
	Data = nullptr;
	FrameID = 0;
	check(mapped <= 0);
	mapped = 0;
	id = INVALID_TILE_ID;

	if (header) delete header;
	header = nullptr;
	globalChunkid = INVALID_CHUNK_ID;
}

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
	TranscodeCache.Init(NumTranscodeRequests);
	IStreamingManager::Get().AddStreamingManager(this);
	GTranscodingCacheTLSIndex = FPlatformTLS::AllocTlsSlot();
}

FVirtualTextureChunkStreamingManager::~FVirtualTextureChunkStreamingManager()
{
	IStreamingManager::Get().RemoveStreamingManager(this);
}

void FVirtualTextureChunkStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything /*= false*/)
{
	FScopeLock Lock(&cs);
	TranscodeCache.RetireOldRequests();
}

int32 FVirtualTextureChunkStreamingManager::BlockTillAllRequestsFinished(float TimeLimit /*= 0.0f*/, bool bLogResults /*= false*/)
{
	int32 Result = 0;
	return Result;
}

void FVirtualTextureChunkStreamingManager::CancelForcedResources()
{

}

void FVirtualTextureChunkStreamingManager::AddChunkProvider(FChunkProvider* Provider)
{
	// TODO MT lock ?

	for (auto vt : VirtualTextures)
	{
		ensure(vt.Key->Provider != Provider);
	}

	{
		FUploadingVirtualTexture* VTexture = new FUploadingVirtualTexture(Provider->GetNumTilesX(), Provider->GetNumTilesY(), 1);
		VTexture->Provider = Provider;
		Provider->v_Address = Provider->GetSpace()->AllocateVirtualTexture(VTexture);
		VirtualTextures.Add(VTexture, Provider);
	}
}

void FVirtualTextureChunkStreamingManager::RemoveChunkProvider(FChunkProvider* Provider)
{
	FUploadingVirtualTexture* VTexture = nullptr;
	for (auto vt : VirtualTextures)
	{
		if (vt.Key->Provider == Provider)
		{
			VTexture = vt.Key;
			break;
		}
	}
	if (VTexture)
	{
		FChunkProvider* RemovedProvider = VirtualTextures.FindAndRemoveChecked(VTexture);
		check(RemovedProvider == Provider);
		Provider->GetSpace()->FreeVirtualTexture(VTexture);
		delete VTexture;
	}
}

bool FVirtualTextureChunkStreamingManager::RequestTile(FUploadingVirtualTexture* VTexture, const TileID& id)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_RequestTile);
	FScopeLock Lock(&cs);

	FChunkProvider* Provider = VirtualTextures.FindRef(VTexture);
	checkf(Provider, TEXT("0x%p"), Provider);

	const int32 ChunkIndex = Provider->GetChunkIndex(id);
	if (ChunkIndex == -1)
	{
		return false;
	}

	// tile is being transcoded/is done transcoding
	FTranscodeRequest* treq = TranscodeCache.FindRequestForTile(id);
	if (treq)
	{
		return treq->event->IsComplete();
	}

	// get the chunk headers
	uint32 headerSize = Provider->GetChunkHeaderSize(ChunkIndex);
	IFileCacheReadBuffer *Header = Provider->GetData(ChunkIndex, 0, headerSize);
	if (Header == nullptr)
	{
		return false;
	}

	uint32 TileSize = Provider->GetTileSize(id);
	uint32 TileOffset = Provider->GetTileOffset(id);
	IFileCacheReadBuffer *Buffer = Provider->GetData(ChunkIndex, TileOffset, TileSize);

	if (Buffer)
	{
		FTranscodeRequest* request = TranscodeCache.Get();
		if (request == nullptr)
		{
			delete Buffer;
			// no more requests available
			INC_DWORD_STAT(STAT_VTP_NumTranscodeDropped);
			return false;
		}

		uint8 vLevel = 0;
		uint64 vAddress = 0;
		FromTileID(id, vLevel, vAddress);

		const ChunkID pid = LocalChunkIdToGlobal(ChunkIndex, VTexture);
		request->Init(id, Provider->GetTileMemSize(), Buffer, pid, Header);
		
		FGraphEventArray preq = {};
		request->event = TGraphTask<TranscodeJob>::CreateTask(&preq)
			.ConstructAndDispatchWhenReady(Provider, vLevel, request);
	}

	return false;
}

void FVirtualTextureChunkStreamingManager::MapTile(const TileID& id, void* RESTRICT& outData)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_Map);
	FScopeLock Lock(&cs);
	outData = TranscodeCache.Map(id);
}

void FVirtualTextureChunkStreamingManager::UnMapTile(const TileID& id)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_UnMap);
	FScopeLock Lock(&cs);
	TranscodeCache.Unmap(id);

	DEC_DWORD_STAT(STAT_VTP_NumTranscode);
}

void FVirtualTextureCodec::Init(FChunkProvider* Provider, IFileCacheReadBuffer *Data)
{
	if (Initialized)
	{
		return;
	}
	
	HeaderData = (uint8*)FMemory::Malloc(Data->GetSize());
	FMemory::Memcpy(HeaderData, Data->GetData(), Data->GetSize());

	const uint32 Layers = Provider->GetNumLayers();
	for (uint32 Layer = 0; Layer < Layers; ++Layer)
	{
		Codecs[Layer] = (EVirtualTextureCodec)Provider->GetCodecId(HeaderData, Layer);
		switch (Codecs[Layer])
		{
		case EVirtualTextureCodec::Crunch:
		{
#if CRUNCH_SUPPORT
			uint8* Payload = nullptr;
			size_t PayloadSize = 0;
			const bool result = Provider->GetCodecPayload(HeaderData, Layer, Payload, PayloadSize);
			ensure(result);
			Contexts[Layer] = CrunchCompression::InitializeDecoderContext(Payload, PayloadSize);
#endif
			check(false);
			break;
		}
		default:
			break;
		}
	}
	Initialized = true;
}

FVirtualTextureCodec::~FVirtualTextureCodec()
{
	if (!Initialized)
	{
		return;
	}

	for (int32 Layer = 0; Layer < MAX_NUM_LAYERS; ++Layer)
	{
		if (Contexts[Layer] == nullptr)
		{
			continue;
		}
		switch (Codecs[Layer])
		{
		case EVirtualTextureCodec::Crunch:
		{
#if CRUNCH_SUPPORT
			CrunchCompression::DestroyDecoderContext(Contexts[Layer]);
#endif
			check(false);
			break;
		}
		default:
			break;
		}
	}

	if (HeaderData)
	{
		FMemory::Free(HeaderData);
		HeaderData = nullptr;
	}
}
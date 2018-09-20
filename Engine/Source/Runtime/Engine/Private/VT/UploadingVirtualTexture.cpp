#include "UploadingVirtualTexture.h"
#include "VirtualTextureChunkProviders.h"
#include "VirtualTextureChunkManager.h"
#include "VirtualTexturing.h"
#include "SceneUtils.h"

static int32 NumUploadsBatched = 96;
static FAutoConsoleVariableRef CVarNumUploadsBatched(
	TEXT("r.VT.NumUploadsBatched"),
	NumUploadsBatched,
	TEXT("Number of tiles that can fit into one single upload batch. If more tiles are requested, batches are flushed immediately. default 96\n")
	, ECVF_ReadOnly
);
static const int NumStagingTextures = 3;

DECLARE_GPU_STAT_NAMED(Stat_GPU_VTCopy, TEXT("VT Tile Copy"));

struct FBatchUploader final : public IVirtualTextureProducer
{
	struct FBatch
	{
		struct FItem
		{
			int32 TileSize = 0;
			int32 SrcPositionY = 0;
			uint16 pAddress = ~0;
			FTextureRHIRef dst;
			uint32 SrcStagingIdx = 0;
		};

		struct FStagingTexture
		{
			FTexture2DRHIRef Texture;
			void* MappedPtr = nullptr;
			uint32 MappedStride = 0;

			void Lock()
			{
				if (MappedPtr == nullptr)
				{
					MappedPtr = RHILockTexture2D(Texture, 0, RLM_WriteOnly, MappedStride, false, false);
				}
				ensure(MappedPtr);
			}

			void Unlock()
			{
				if (MappedPtr)
				{
					RHIUnlockTexture2D(Texture, 0, false, false);
					MappedPtr = nullptr;
					MappedStride = 0;
				}
			}
		};
 				
		uint32 MaxTileSize = 0;
		uint32 CurrentPixelPositionY = 0;
		uint32 ActiveStrip = 0;
		FStagingTexture StagingTextures[NumStagingTextures];
		uint32 StagingHeight = 0;
		FPixelFormatInfo FormatInfo;
		TArray<FItem> Tiles;
		
		void StageTile(FChunkProvider* InProvider, int layerIdx, uint8* InPtr, uint16 InpAddress)
		{
			SCOPE_CYCLE_COUNTER(STAT_VTP_StageUpload)

			if (CurrentPixelPositionY + InProvider->GetTilePixelSize() > StagingHeight)
			{
				ActiveStrip++;
				if (ActiveStrip >= NumStagingTextures)
				{
					INC_DWORD_STAT(STAT_VTP_NumIntraFrameFlush);
					Flush();
				}
				CurrentPixelPositionY = 0;
			}

			checkf(ActiveStrip >= 0 && ActiveStrip < NumStagingTextures, TEXT("%i"), ActiveStrip);
			FStagingTexture& activeStagingTexture = StagingTextures[ActiveStrip];
			activeStagingTexture.Lock();

			// copy the data to the staging resource
			const int ScanlineOffset = FMath::DivideAndRoundUp(CurrentPixelPositionY, (uint32)FormatInfo.BlockSizeY) * activeStagingTexture.MappedStride;
			const uint32 NumBlocksY = FMath::DivideAndRoundUp(InProvider->GetTilePixelSize(), (uint32)FormatInfo.BlockSizeY);
			const uint32 ScanlineSize = FMath::DivideAndRoundUp(InProvider->GetTilePixelSize(), (uint32)FormatInfo.BlockSizeX) * FormatInfo.BlockBytes;

			for (uint32 scanLine = 0; scanLine < NumBlocksY; ++scanLine)
			{
				FMemory::Memcpy(((unsigned char*)activeStagingTexture.MappedPtr + ScanlineOffset) + scanLine * activeStagingTexture.MappedStride,
					InPtr + scanLine * ScanlineSize,
					ScanlineSize);
			}

			// queue the rect for copying
			FItem BatchedItem;
			BatchedItem.TileSize = InProvider->GetTilePixelSize();
			BatchedItem.SrcPositionY = CurrentPixelPositionY;
			BatchedItem.pAddress = InpAddress;
			BatchedItem.dst = InProvider->GetSpace()->GetPhysicalTexture(layerIdx);
			BatchedItem.SrcStagingIdx = ActiveStrip;
			Tiles.Add(BatchedItem);
			
			// align to the block size
			CurrentPixelPositionY += FMath::DivideAndRoundUp(InProvider->GetTilePixelSize(), (uint32)FormatInfo.BlockSizeY) * FormatInfo.BlockSizeY;
			ensure((CurrentPixelPositionY % FormatInfo.BlockSizeY) == 0);
		}

		void ConditionalCreateResources(uint32 InTileSize, EPixelFormat format)
		{
			if (MaxTileSize >= InTileSize
				&& StagingTextures[0].Texture.IsValid()) //if 0 is valid, assume all are valid
			{
				return;
			}

			Flush();

			MaxTileSize = FMath::Max(MaxTileSize, InTileSize);
			StagingHeight = FMath::Min(MaxTileSize * NumUploadsBatched, (uint32)GMaxTextureDimensions);
			FormatInfo = GPixelFormats[format];
			
			FRHIResourceCreateInfo createInfo;
			for (int i = 0; i < NumStagingTextures; ++i)
			{
				StagingTextures[i].Texture.SafeRelease();
				StagingTextures[i].Texture = RHICreateTexture2D(MaxTileSize, StagingHeight, format, 1, 1, TexCreate_CPUWritable, createInfo);
			}
			CurrentPixelPositionY = 0;
		}

		void Flush()
		{
			SCOPE_CYCLE_COUNTER(STAT_VTP_FlushUpload)

			auto& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			SCOPED_GPU_STAT(RHICmdList, Stat_GPU_VTCopy);
			
			for (int i = 0; i < NumStagingTextures; ++i)
			{
				StagingTextures[i].Unlock();
			}
						
			for (auto item : Tiles)
			{
				const uint32 pPageX = FMath::ReverseMortonCode2(item.pAddress);
				const uint32 pPageY = FMath::ReverseMortonCode2(item.pAddress >> 1);

				FBox2D SourceBox(FVector2D(0.0f, item.SrcPositionY), FVector2D(item.TileSize, item.SrcPositionY + item.TileSize));

				FVector2D DestinationBoxStart = FVector2D(pPageX * item.TileSize, pPageY * item.TileSize);
				FBox2D DestinationBox(DestinationBoxStart, DestinationBoxStart + FVector2D(item.TileSize, item.TileSize));
				RHICmdList.CopySubTextureRegion(StagingTextures[item.SrcStagingIdx].Texture, item.dst->GetTexture2D(), SourceBox, DestinationBox);
			}
			Tiles.Empty();
			

			ActiveStrip = (ActiveStrip + 1) % NumStagingTextures;
			CurrentPixelPositionY = 0;
		}
	};

	TMap<EPixelFormat, FBatch> Batches;

	void Add(FChunkProvider* InProvider, int layerIdx, uint8* InPtr, uint16 InpAddress)
	{
		for (uint32 Layer = 0; Layer < InProvider->GetNumLayers(); ++Layer)
		{
			EPixelFormat format = InProvider->GetSpace()->GetPhysicalTextureFormat(Layer);
			FBatch* Batch = Batches.Find(format);
			if (Batch == nullptr)
			{
				Batch = &Batches.Add(format);				
			}

			Batch->ConditionalCreateResources(InProvider->GetTilePixelSize(), format);
			Batch->StageTile(InProvider, Layer, InPtr, InpAddress);

			InPtr += CalculateImageBytes(InProvider->GetTilePixelSize(), InProvider->GetTilePixelSize(), 1, format);
		}
	}

	virtual void Finalize() override
	{
		for (auto& batch : Batches)
		{
			batch.Value.Flush();
		}
	}
} BatchUploader;

FUploadingVirtualTexture::FUploadingVirtualTexture(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ)
	: IVirtualTexture(InSizeX, InSizeY, InSizeZ)
{}


bool FUploadingVirtualTexture::LocatePageData(uint8 vLevel, uint64 vAddress, void* RESTRICT& Location) /*const*/
{
	const TileID id = GetTileID(this, vLevel, vAddress);

	const bool resident = FVirtualTextureChunkStreamingManager::Get().RequestTile(this, id);
	if (resident)
	{
		FVirtualTextureChunkStreamingManager::Get().MapTile(id, Location);
		return true;
	}
	else
	{
		return false;
	}
}

IVirtualTextureProducer* FUploadingVirtualTexture::ProducePageData(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint8 vLevel, uint64 vAddress, uint16 pAddress, void* Location) /*const*/
{
 	const TileID id = GetTileID(this, vLevel, vAddress);
	BatchUploader.Add(Provider, id, (uint8*)Location, pAddress);
	FVirtualTextureChunkStreamingManager::Get().UnMapTile(id);
	INC_DWORD_STAT(STAT_VTP_NumUploads);
	return &BatchUploader;
}

void FUploadingVirtualTexture::DumpToConsole()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Uploading virtual texture"));
	Provider->DumpToConsole();
}
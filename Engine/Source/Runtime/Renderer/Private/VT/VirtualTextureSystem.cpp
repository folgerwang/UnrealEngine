// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSystem.h"

#include "TexturePagePool.h"
#include "VirtualTextureSpace.h"
#include "VirtualTexturePhysicalSpace.h"
#include "AllocatedVirtualTexture.h"
#include "VirtualTextureFeedback.h"
#include "VirtualTexturing.h"
#include "UniquePageList.h"
#include "UniqueRequestList.h"
#include "Stats/Stats.h"
#include "SceneUtils.h"
#include "HAL/IConsoleManager.h"
#include "PostProcess/SceneRenderTargets.h"


DECLARE_CYCLE_STAT(TEXT("Feedback Analysis"), STAT_FeedbackAnalysis, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("VirtualTextureSystem Update"), STAT_VirtualTextureSystem_Update, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Page Table Updates"), STAT_PageTableUpdates, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Gather Requests"), STAT_ProcessRequests_Gather, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Sort Requests"), STAT_ProcessRequests_Sort, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Submit Requests"), STAT_ProcessRequests_Submit, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Map Requests"), STAT_ProcessRequests_Map, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Finalize Requests"), STAT_ProcessRequests_Finalize, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Merge Unique Pages"), STAT_ProcessRequests_MergePages, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Merge Requests"), STAT_ProcessRequests_MergeRequests, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Submit Tasks"), STAT_ProcessRequests_SubmitTasks, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible"), STAT_NumPageVisible, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible resident"), STAT_NumPageVisibleResident, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible not resident"), STAT_NumPageVisibleNotResident, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page prefetch"), STAT_NumPagePrefetch, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page update"), STAT_NumPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num continuous page update"), STAT_NumContinuousPageUpdate, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num stacks requested"), STAT_NumStacksRequested, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num stacks produced"), STAT_NumStacksProduced, STATGROUP_VirtualTexturing);

DECLARE_MEMORY_STAT_POOL(TEXT("Total Physical Memory"), STAT_TotalPhysicalMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);
DECLARE_MEMORY_STAT_POOL(TEXT("Total Pagetable Memory"), STAT_TotalPagetableMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);

DECLARE_GPU_STAT( VirtualTexture );

static TAutoConsoleVariable<int32> CVarVTMaxUploadsPerFrame(
	TEXT("r.VT.MaxUploadsPerFrame"),
	64,
	TEXT("Max number of page uploads per frame"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVTEnableFeedBack(
	TEXT("r.VT.EnableFeedBack"),
	1,
	TEXT("process readback buffer? dev option."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVTVerbose(
	TEXT("r.VT.Verbose"),
	0,
	TEXT("Be pedantic about certain things that shouln't occur unless something is wrong. This may cause a lot of logspam 100's of lines per frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVTNumFeedbackTasks(
	TEXT("r.VT.NumFeedbackTasks"),
	4,
	TEXT("Number of tasks to create to process virtual texture updates."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTNumGatherTasks(
	TEXT("r.VT.NumGatherTasks"),
	4,
	TEXT("Number of tasks to create to process virtual texture updates."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTPageUpdateFlushCount(
	TEXT("r.VT.PageUpdateFlushCount"),
	8,
	TEXT("Number of page updates to buffer before attempting to flush by taking a lock."),
	ECVF_RenderThreadSafe
);
static const int32 MaxNumTasks = 16;

static FORCEINLINE uint32 EncodePage(uint32 ID, uint32 vLevel, uint32 vTileX, uint32 vTileY)
{
	uint32 Page;
	Page = vTileX << 0;
	Page |= vTileY << 12;
	Page |= vLevel << 24;
	Page |= ID << 28;
	return Page;
}

struct FPageUpdateBuffer
{
	static const uint32 PageCapacity = 128u;
	uint16 PhysicalAddresses[PageCapacity];
	uint32 PrevPhysicalAddress = ~0u;
	uint32 NumPages = 0u;
	uint32 NumPageUpdates = 0u;
	uint32 WorkingSetSize = 0u;
};

struct FFeedbackAnalysisParameters
{
	FVirtualTextureSystem* System = nullptr;
	const uint32* FeedbackBuffer = nullptr;
	FUniquePageList* UniquePageList = nullptr;
	uint32 FeedbackWidth = 0u;
	uint32 FeedbackHeight = 0u;
	uint32 FeedbackPitch = 0u;
};

struct FGatherRequestsParameters
{
	FVirtualTextureSystem* System = nullptr;
	const FUniquePageList* UniquePageList = nullptr;
	FPageUpdateBuffer* PageUpdateBuffers = nullptr;
	FUniqueRequestList* RequestList = nullptr;
	uint32 PageUpdateFlushCount = 0u;
	uint32 PageStartIndex = 0u;
	uint32 NumPages = 0u;
	uint32 FrameRequested;
};

class FFeedbackAnalysisTask
{
public:
	explicit FFeedbackAnalysisTask(const FFeedbackAnalysisParameters& InParams) : Parameters(InParams) {}

	FFeedbackAnalysisParameters Parameters;

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Parameters.UniquePageList->Initialize();
		Parameters.System->FeedbackAnalysisTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId GetStatId() const { return TStatId(); }
};

class FGatherRequestsTask
{
public:
	explicit FGatherRequestsTask(const FGatherRequestsParameters& InParams) : Parameters(InParams) {}

	FGatherRequestsParameters Parameters;

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Parameters.RequestList->Initialize();
		Parameters.System->GatherRequestsTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId GetStatId() const { return TStatId(); }
};

static FVirtualTextureSystem* GVirtualTextureSystem = nullptr;

void FVirtualTextureSystem::Initialize()
{
	if (!GVirtualTextureSystem)
	{
		GVirtualTextureSystem = new FVirtualTextureSystem();
	}
}

void FVirtualTextureSystem::Shutdown()
{
	if (GVirtualTextureSystem)
	{
		delete GVirtualTextureSystem;
		GVirtualTextureSystem = nullptr;
	}
}

FVirtualTextureSystem& FVirtualTextureSystem::Get()
{
	check(GVirtualTextureSystem);
	return *GVirtualTextureSystem;
}

FVirtualTextureSystem::FVirtualTextureSystem()
	: Frame(1u), // Need to start on Frame 1, otherwise the first call to update will fail to allocate any pages
	bFlushCaches(false),
	FlushCachesCommand(TEXT("r.VT.Flush"), TEXT("Flush all the physical caches in the VT system."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::FlushCachesFromConsole)),
	DumpCommand(TEXT("r.VT.Dump"), TEXT("Lot a whole lot of info on the VT system state."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::DumpFromConsole)),
	ListPhysicalPools(TEXT("r.VT.ListPhysicalPools"), TEXT("Lot a whole lot of info on the VT system state."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::ListPhysicalPoolsFromConsole))
{
}

FVirtualTextureSystem::~FVirtualTextureSystem()
{
	DestroyPendingVirtualTextures();

	check(AllocatedVTs.Num() == 0);

	for (uint32 SpaceID = 0u; SpaceID < MaxSpaces; ++SpaceID)
	{
		FVirtualTextureSpace* Space = Spaces[SpaceID].Get();
		if (Space)
		{
			check(Space->GetRefCount() == 0u);
			BeginReleaseResource(Space);
		}
	}
	for(int i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i].Get();
		check(PhysicalSpace->GetRefCount() == 0u);
		BeginReleaseResource(PhysicalSpace);
	}
}

void FVirtualTextureSystem::FlushCachesFromConsole()
{
	FlushCache();
}

void FVirtualTextureSystem::FlushCache()
{
	// We defer the actual flush to the render thread in the Update function
	bFlushCaches = true;
}

void FVirtualTextureSystem::DumpFromConsole()
{
	bool verbose = false;
	for (int ID = 0; ID < 16; ID++)
	{
		FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space)
		{
			Space->DumpToConsole(verbose);
		}
	}
}

void FVirtualTextureSystem::ListPhysicalPoolsFromConsole()
{
	for(int i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		const FVirtualTexturePhysicalSpace& PhysicalSpace = *PhysicalSpaces[i];
		const FVTPhysicalSpaceDescription& Desc = PhysicalSpace.GetDescription();
		const FTexturePagePool& PagePool = PhysicalSpace.GetPagePool();
		const uint32 TotalSizeInBytes = PhysicalSpace.GetSizeInBytes();

		UE_LOG(LogConsoleResponse, Display, TEXT("PhysicaPool: [%i] PF_%s %ix%i:"), i, GPixelFormats[Desc.Format].Name, Desc.TileSize, Desc.TileSize);
		UE_LOG(LogConsoleResponse, Display, TEXT("  SizeInMegabyte= %f"), (float)TotalSizeInBytes / 1024.0f / 1024.0f);
		UE_LOG(LogConsoleResponse, Display, TEXT("  Dimensions= %ix%i"), PhysicalSpace.GetTextureSize(), PhysicalSpace.GetTextureSize());
		UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles= %i"), PhysicalSpace.GetNumTiles());
		UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Mapped= %i"), PagePool.GetNumMappedPages());

		const int32 LockedTiles = PagePool.GetNumLockedPages();
		const float LockedLoad = (float)LockedTiles / (float)PhysicalSpace.GetNumTiles();
		const float LockedMemory = LockedLoad * TotalSizeInBytes / 1024.0f / 1024.0f;
		UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Locked= %i (%fMB)"), LockedTiles, LockedMemory);
	}


	for (int ID = 0; ID < 16; ID++)
	{
		const FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space == nullptr)
		{
			continue;
		}

		const FVTSpaceDescription& Desc = Space->GetDescription();
		const FVirtualTextureAllocator& Allocator = Space->GetAllocator();
		const uint32 PageTableSize = Space->GetPageTableSize();
		const uint32 TotalSizeInBytes = Space->GetSizeInBytes();
		const uint32 NumAllocatedPages = Allocator.GetNumAllocatedPages();
		const uint32 NumTotalPages = PageTableSize * PageTableSize;
		const double AllocatedRatio = (double)NumAllocatedPages / NumTotalPages;

		const uint32 PhysicalTileSize = Desc.TileSize + Desc.TileBorderSize * 2u;
		const TCHAR* FormatName = nullptr;
		switch (Desc.Format)
		{
		case EVTPageTableFormat::UInt16: FormatName = TEXT("UInt16"); break;
		case EVTPageTableFormat::UInt32: FormatName = TEXT("UInt32"); break;
		default: checkNoEntry(); break;
		}

		UE_LOG(LogConsoleResponse, Display, TEXT("Pool: [%i] %s (%ix%i) x %i:"), ID, FormatName, PhysicalTileSize, PhysicalTileSize, Desc.NumLayers);
		UE_LOG(LogConsoleResponse, Display, TEXT("  PageTableSize= %ix%i"), PageTableSize, PageTableSize);
		UE_LOG(LogConsoleResponse, Display, TEXT("  Allocations= %i, %i%% (%fMB)"),
			Allocator.GetNumAllocations(),
			(int)(AllocatedRatio * 100.0),
			(float)(AllocatedRatio * TotalSizeInBytes / 1024.0 / 1024.0));
	}
}

uint32 GetTypeHash(const FAllocatedVTDescription& Description)
{
	return FCrc::MemCrc32(&Description, sizeof(Description));
}

IAllocatedVirtualTexture* FVirtualTextureSystem::AllocateVirtualTexture(const FAllocatedVTDescription& Desc)
{
	check(Desc.NumLayers <= VIRTUALTEXTURE_SPACE_MAXLAYERS);

	// Make sure any pending VTs are destroyed before attempting to allocate a new one
	// Otherwise, we might find/return an existing IAllocatedVirtualTexture* that's pending deletion
	DestroyPendingVirtualTextures();

	// Check to see if we already have an allocated VT that matches this description
	// This can happen often as multiple material instances will share the same textures
	FAllocatedVirtualTexture*& AllocatedVT = AllocatedVTs.FindOrAdd(Desc);
	if (AllocatedVT)
	{
		AllocatedVT->IncrementRefCount();
		return AllocatedVT;
	}

	uint32 WidthInTiles = 0u;
	uint32 HeightInTiles = 0u;
	uint32 DepthInTiles = 0u;
	bool bSupport16BitPageTable = true;
	FVirtualTextureProducer* ProducerForLayer[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { nullptr };
	bool bAnyLayerProducerWantsPersistentHighestMip = false;
	for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumLayers; ++LayerIndex)
	{
		FVirtualTextureProducer* Producer = Producers.FindProducer(Desc.ProducerHandle[LayerIndex]);
		ProducerForLayer[LayerIndex] = Producer;
		if (Producer)
		{
			WidthInTiles = FMath::Max(WidthInTiles, Producer->GetWidthInTiles());
			HeightInTiles = FMath::Max(HeightInTiles, Producer->GetHeightInTiles());
			DepthInTiles = FMath::Max(DepthInTiles, Producer->GetDepthInTiles());
			FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpace(Desc.LocalLayerToProduce[LayerIndex]);
			if (!PhysicalSpace->DoesSupport16BitPageTable())
			{
				bSupport16BitPageTable = false;
			}
			bAnyLayerProducerWantsPersistentHighestMip |= Producer->GetDescription().bPersistentHighestMip;
		}
	}

	check(WidthInTiles > 0u);
	check(HeightInTiles > 0u);
	check(DepthInTiles > 0u);

	FVTSpaceDescription SpaceDesc;
	SpaceDesc.Dimensions = Desc.Dimensions;
	SpaceDesc.NumLayers = Desc.NumLayers;
	SpaceDesc.TileSize = Desc.TileSize;
	SpaceDesc.TileBorderSize = Desc.TileBorderSize;
	SpaceDesc.bPrivateSpace = Desc.bPrivateSpace;
	SpaceDesc.Format = bSupport16BitPageTable ? EVTPageTableFormat::UInt16 : EVTPageTableFormat::UInt32;
	FVirtualTextureSpace* Space = AcquireSpace(SpaceDesc, FMath::Max(WidthInTiles, HeightInTiles));

	AllocatedVT = new FAllocatedVirtualTexture(Frame, Desc, Space, ProducerForLayer, WidthInTiles, HeightInTiles, DepthInTiles);
	if (bAnyLayerProducerWantsPersistentHighestMip)
	{
		AllocatedVTsToMap.Add(AllocatedVT);
	}
	return AllocatedVT;
}

void FVirtualTextureSystem::DestroyVirtualTexture(IAllocatedVirtualTexture* AllocatedVT)
{
	AllocatedVT->Destroy(this);
}

void FVirtualTextureSystem::ReleaseVirtualTexture(FAllocatedVirtualTexture* AllocatedVT)
{
	if (IsInRenderingThread())
	{
		AllocatedVT->Release(this);
	}
	else
	{
		FScopeLock Lock(&PendingDeleteLock);
		PendingDeleteAllocatedVTs.Add(AllocatedVT);
	}
}

void FVirtualTextureSystem::RemoveAllocatedVT(FAllocatedVirtualTexture* AllocatedVT)
{
	// shouldn't be more than 1 instance of this in the list
	verify(AllocatedVTsToMap.Remove(AllocatedVT) <= 1);
	// should always exist in this map
	verify(AllocatedVTs.Remove(AllocatedVT->GetDescription()) == 1);
}

void FVirtualTextureSystem::DestroyPendingVirtualTextures()
{
	check(IsInRenderingThread());
	TArray<FAllocatedVirtualTexture*> AllocatedVTsToDelete;
	{
		FScopeLock Lock(&PendingDeleteLock);
		AllocatedVTsToDelete = MoveTemp(PendingDeleteAllocatedVTs);
	}
	for (FAllocatedVirtualTexture* AllocatedVT : AllocatedVTsToDelete)
	{
		AllocatedVT->Release(this);
	}
}

FVirtualTextureProducerHandle FVirtualTextureSystem::RegisterProducer(const FVTProducerDescription& InDesc, IVirtualTexture* InProducer)
{
	return Producers.RegisterProducer(this, InDesc, InProducer);
}

void FVirtualTextureSystem::ReleaseProducer(const FVirtualTextureProducerHandle& Handle)
{
	Producers.ReleaseProducer(this, Handle);
}

FVirtualTextureSpace* FVirtualTextureSystem::AcquireSpace(const FVTSpaceDescription& InDesc, uint32 InSizeNeeded)
{
	// If InDesc requests a private space, don't reuse any existing spaces
	if (!InDesc.bPrivateSpace)
	{
		for (uint32 SpaceIndex = 0u; SpaceIndex < MaxSpaces; ++SpaceIndex)
		{
			FVirtualTextureSpace* Space = Spaces[SpaceIndex].Get();
			if (Space && Space->GetDescription() == InDesc)
			{
				Space->AddRef();
				return Space;
			}
		}
	}

	for (uint32 SpaceIndex = 0u; SpaceIndex < MaxSpaces; ++SpaceIndex)
	{
		if (!Spaces[SpaceIndex])
		{
			FVirtualTextureSpace* Space = new FVirtualTextureSpace(this, SpaceIndex, InDesc, InSizeNeeded);
			Spaces[SpaceIndex].Reset(Space);
			INC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
			BeginInitResource(Space);
			Space->AddRef();
			return Space;
		}
	}

	// out of space slots
	check(false);
	return nullptr;
}

void FVirtualTextureSystem::ReleaseSpace(FVirtualTextureSpace* Space)
{
	check(IsInRenderingThread());
	const uint32 NumRefs = Space->Release();
	if (NumRefs == 0u && Space->GetDescription().bPrivateSpace)
	{
		// Private spaces are destroyed when ref count reaches 0
		// This can only happen on render thread, so we can call ReleaseResource() directly and then delete the pointer immediately
		Space->ReleaseResource();
		Spaces[Space->GetID()].Release();
	}
}

FVirtualTexturePhysicalSpace* FVirtualTextureSystem::AcquirePhysicalSpace(const FVTPhysicalSpaceDescription& InDesc)
{
	for (int i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i].Get();
		if (PhysicalSpace->GetDescription() == InDesc)
		{
			PhysicalSpace->AddRef();
			return PhysicalSpace;
		}
	}

	const uint32 ID = PhysicalSpaces.Num();
	check(ID <= 0x0fff);

	TUniquePtr<FVirtualTexturePhysicalSpace>& PhysicalSpace = PhysicalSpaces.AddDefaulted_GetRef();
	PhysicalSpace.Reset(new FVirtualTexturePhysicalSpace(InDesc, ID));
	INC_MEMORY_STAT_BY(STAT_TotalPhysicalMemory, PhysicalSpace->GetSizeInBytes());
	BeginInitResource(PhysicalSpace.Get());
	PhysicalSpace->AddRef();
	return PhysicalSpace.Get();
}

void FVirtualTextureSystem::ReleasePhysicalSpace(FVirtualTexturePhysicalSpace* Space)
{
	const uint32 NumRefs = Space->Release();
	// Don't delete physical space when ref count hits 0, as they are likely to be reused/recreated in future
	// Might need to have some mechanism to explicitly delete unreferenced spaces, or delete unreferenced spaces after some fixed number of frames
}

void FVirtualTextureSystem::LockTile(const FVirtualTextureLocalTile& Tile)
{
	check(IsInRenderingThread());
	TilesToLock.Add(Tile);
}

void FVirtualTextureSystem::UnlockTile(const FVirtualTextureLocalTile& Tile)
{
	check(IsInRenderingThread());

	// Tile is no longer locked
	TilesToLock.Remove(Tile);

	const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
	const FVirtualTextureProducer* Producer = Producers.FindProducer(ProducerHandle);
	if (Producer)
	{
		for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
		{
			FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
			FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
			const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, Tile.Local_vAddress, Tile.Local_vLevel);
			if (pAddress != ~0u)
			{
				PagePool.Unlock(Frame, pAddress);
			}
		}
	}
}

static float ComputeMipLevel(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize)
{
	const uint32 TextureWidth = AllocatedVT->GetWidthInPixels();
	const uint32 TextureHeight = AllocatedVT->GetHeightInPixels();
	const FVector2D dfdx(TextureWidth / InScreenSpaceSize.X, 0.0f);
	const FVector2D dfdy(0.0f, TextureHeight / InScreenSpaceSize.Y);
	const float ppx = FVector2D::DotProduct(dfdx, dfdx);
	const float ppy = FVector2D::DotProduct(dfdy, dfdy);
	return 0.5f * FMath::Log2(FMath::Max(ppx, ppy));
}

void FVirtualTextureSystem::RequestTilesForRegion(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FIntRect& InTextureRegion, int32 InMipLevel)
{
	FIntRect TextureRegion(InTextureRegion);
	if (TextureRegion.IsEmpty())
	{
		TextureRegion.Max.X = AllocatedVT->GetWidthInPixels();
		TextureRegion.Max.Y = AllocatedVT->GetHeightInPixels();
	}
	else
	{
		TextureRegion.Clip(FIntRect(0, 0, AllocatedVT->GetWidthInPixels(), AllocatedVT->GetHeightInPixels()));
	}

	if (InMipLevel >= 0)
	{
		FScopeLock Lock(&RequestedTilesLock);
		RequestTilesForRegionInternal(AllocatedVT, TextureRegion, InMipLevel);
	}
	else
	{
		const uint32 vMaxLevel = AllocatedVT->GetMaxLevel();
		const float vLevel = ComputeMipLevel(AllocatedVT, InScreenSpaceSize);
		const int32 vMipLevelDown = FMath::Clamp((int32)FMath::FloorToInt(vLevel), 0, (int32)vMaxLevel);

		FScopeLock Lock(&RequestedTilesLock);
		RequestTilesForRegionInternal(AllocatedVT, TextureRegion, vMipLevelDown);
		if (vMipLevelDown + 1u <= vMaxLevel)
		{
			// Need to fetch 2 levels to support trilinear filtering
			RequestTilesForRegionInternal(AllocatedVT, TextureRegion, vMipLevelDown + 1u);
		}
	}
}

void FVirtualTextureSystem::LoadPendingTiles(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	TArray<uint32> PackedTiles;
	if (RequestedPackedTiles.Num() > 0)
	{
		FScopeLock Lock(&RequestedTilesLock);
		PackedTiles = MoveTemp(RequestedPackedTiles);
		RequestedPackedTiles.Reset();
	}

	if (PackedTiles.Num() > 0)
	{
		FMemStack& MemStack = FMemStack::Get();
		FMemMark Mark(MemStack);

		FUniquePageList* UniquePageList = new(MemStack) FUniquePageList;
		UniquePageList->Initialize();
		for (uint32 Tile : PackedTiles)
		{
			UniquePageList->Add(Tile, 0xffff);
		}

		FUniqueRequestList* RequestList = new(MemStack) FUniqueRequestList(MemStack);
		RequestList->Initialize();
		GatherRequests(RequestList, UniquePageList, Frame, MemStack);
		// No need to sort requests, since we're submitting all of them here (no throttling)
		AllocateResources(RHICmdList, FeatureLevel);
		SubmitRequests(RHICmdList, FeatureLevel, MemStack, RequestList, false);
	}
}

void FVirtualTextureSystem::RequestTilesForRegionInternal(const IAllocatedVirtualTexture* AllocatedVT, const FIntRect& InTextureRegion, uint32 vLevel)
{
	const FIntRect TextureRegionForLevel(InTextureRegion.Min.X >> vLevel, InTextureRegion.Min.Y >> vLevel, InTextureRegion.Max.X >> vLevel, InTextureRegion.Max.Y >> vLevel);
	const FIntRect TileRegionForLevel = FIntRect::DivideAndRoundUp(TextureRegionForLevel, AllocatedVT->GetVirtualTileSize());

	// RequestedPackedTiles stores packed tiles with vPosition shifted relative to current mip level
	const uint32 vBaseTileX = FMath::ReverseMortonCode2(AllocatedVT->GetVirtualAddress()) >> vLevel;
	const uint32 vBaseTileY = FMath::ReverseMortonCode2(AllocatedVT->GetVirtualAddress() >> 1) >> vLevel;

	for (uint32 TileY = TileRegionForLevel.Min.Y; TileY < (uint32)TileRegionForLevel.Max.Y; ++TileY)
	{
		const uint32 vGlobalTileY = vBaseTileY + TileY;
		for (uint32 TileX = TileRegionForLevel.Min.X; TileX < (uint32)TileRegionForLevel.Max.X; ++TileX)
		{
			const uint32 vGlobalTileX = vBaseTileX + TileX;
			const uint32 EncodedTile = EncodePage(AllocatedVT->GetSpaceID(), vLevel, vGlobalTileX, vGlobalTileY);
			RequestedPackedTiles.Add(EncodedTile);
		}
	}
}

void FVirtualTextureSystem::FeedbackAnalysisTask(const FFeedbackAnalysisParameters& Parameters)
{
	FUniquePageList* RESTRICT RequestedPageList = Parameters.UniquePageList;
	const uint32* RESTRICT Buffer = Parameters.FeedbackBuffer;
	const uint32 Width = Parameters.FeedbackWidth;
	const uint32 Height = Parameters.FeedbackHeight;
	const uint32 Pitch = Parameters.FeedbackPitch;

	// Combine simple runs of identical requests
	uint32 LastPixel = 0xffffffff;
	uint32 LastCount = 0;

	for (uint32 y = 0; y < Height; y++)
	{
		const uint32* RESTRICT BufferRow = Buffer + y * Pitch;
		for (uint32 x = 0; x < Width; x++)
		{
			const uint32 Pixel = BufferRow[x];
			if (Pixel == LastPixel)
			{
				LastCount++;
				continue;
			}

			if (LastPixel != 0xffffffff)
			{
				RequestedPageList->Add(LastPixel, LastCount);
			}

			LastPixel = Pixel;
			LastCount = 1;
		}
	}

	if (LastPixel != 0xffffffff)
	{
		RequestedPageList->Add(LastPixel, LastCount);
	}
}

void FVirtualTextureSystem::Update(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_VirtualTextureSystem_Update);
	SCOPED_GPU_STAT(RHICmdList, VirtualTexture);

	if (bFlushCaches)
	{
		for (int i = 0; i < PhysicalSpaces.Num(); ++i)
		{
			FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i].Get();
			// Collect locked pages to be produced again
			PhysicalSpace->GetPagePool().GetAllLockedPages(this, MappedTilesToProduce);
			// Flush unlocked pages
			PhysicalSpace->GetPagePool().EvictAllPages(this);
		}

		bFlushCaches = false;
	}

	DestroyPendingVirtualTextures();

	FMemStack& MemStack = FMemStack::Get();
	FMemMark Mark(MemStack);
	FUniquePageList* MergedUniquePageList = new(MemStack) FUniquePageList;
	MergedUniquePageList->Initialize();
	{
		FMemMark FeedbackMark(MemStack);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// Gather all outstanding feedback buffers
		uint32 FeedbackBufferCount = 0;
		FVirtualTextureFeedback::MapResult MappedFeedbackBuffers[FVirtualTextureFeedback::TargetCapacity];

		if (CVarVTEnableFeedBack.GetValueOnRenderThread())
		{
			for (FeedbackBufferCount = 0; FeedbackBufferCount < FVirtualTextureFeedback::TargetCapacity; ++FeedbackBufferCount)
			{
				if (!SceneContext.VirtualTextureFeedback.Map(RHICmdList, MappedFeedbackBuffers[FeedbackBufferCount]))
				{
					break;
				}
			}
		}

		// Create tasks to read all the buffers
		uint32 NumFeedbackTasks = 0u;

		FFeedbackAnalysisParameters FeedbackAnalysisParameters[MaxNumTasks];
		const uint32 MaxNumFeedbackTasks = FMath::Clamp(CVarVTNumFeedbackTasks.GetValueOnRenderThread(), 1, MaxNumTasks);
			
		for (uint32 i=0; i<FeedbackBufferCount; ++i)
		{
			FVirtualTextureFeedback::MapResult& FeedbackBuffer = MappedFeedbackBuffers[i];

			// Give each task a section of a feedback buffer to analyze
			//todo[vt]: For buffers of different sizes we will have different task payload sizes which is not efficient
			const uint32 FeedbackTasksPerBuffer = MaxNumFeedbackTasks / FeedbackBufferCount;
			const uint32 FeedbackRowsPerTask = FMath::DivideAndRoundUp((uint32)FeedbackBuffer.Rect.Size().Y, FeedbackTasksPerBuffer);
			const uint32 NumRows = (uint32)FeedbackBuffer.Rect.Size().Y;
			
			uint32 CurrentRow = 0;
			while (CurrentRow < NumRows)
			{
				const uint32 CurrentHeight = FMath::Min(FeedbackRowsPerTask, NumRows - CurrentRow);
				if (CurrentHeight > 0u)
				{
					const uint32 TaskIndex = NumFeedbackTasks++;
					FFeedbackAnalysisParameters& Params = FeedbackAnalysisParameters[TaskIndex];
					Params.System = this;
					if (TaskIndex == 0u)
					{
						Params.UniquePageList = MergedUniquePageList;
					}
					else
					{
						Params.UniquePageList = new(MemStack) FUniquePageList;
					}
					Params.FeedbackBuffer = FeedbackBuffer.Buffer + (FeedbackBuffer.Rect.Min.Y + CurrentRow) * FeedbackBuffer.Pitch + FeedbackBuffer.Rect.Min.X;
					Params.FeedbackWidth = FeedbackBuffer.Rect.Size().X;
					Params.FeedbackHeight = CurrentHeight;
					Params.FeedbackPitch = FeedbackBuffer.Pitch;
					CurrentRow += CurrentHeight;
				}
			}
		}

		// Kick the tasks
		FGraphEventArray Tasks;
		if(NumFeedbackTasks > 1u)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_SubmitTasks);
			Tasks.Reserve(NumFeedbackTasks - 1u);
			for (uint32 TaskIndex = 1u; TaskIndex < NumFeedbackTasks; ++TaskIndex)
			{
				Tasks.Add(TGraphTask<FFeedbackAnalysisTask>::CreateTask().ConstructAndDispatchWhenReady(FeedbackAnalysisParameters[TaskIndex]));
			}
		}

		if (NumFeedbackTasks > 0u)
		{
			SCOPE_CYCLE_COUNTER(STAT_FeedbackAnalysis);

			FeedbackAnalysisTask(FeedbackAnalysisParameters[0]);

			// Wait for them to complete
			if (Tasks.Num() > 0)
			{
				FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GetRenderThread_Local());
			}
		}

		for (uint32 i = 0; i < FeedbackBufferCount; ++i)
		{
			SceneContext.VirtualTextureFeedback.Unmap(RHICmdList, MappedFeedbackBuffers[i].MapHandle);
		}

		if(NumFeedbackTasks > 1u)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MergePages);
			for (uint32 TaskIndex = 1u; TaskIndex < NumFeedbackTasks; ++TaskIndex)
			{
				MergedUniquePageList->MergePages(FeedbackAnalysisParameters[TaskIndex].UniquePageList);
			}
		}
	}

	FUniqueRequestList* MergedRequestList = new(MemStack) FUniqueRequestList(MemStack);
	MergedRequestList->Initialize();

	// Collect tiles to lock
	{
		for (const FVirtualTextureLocalTile& Tile : TilesToLock)
		{
			const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
			const FVirtualTextureProducer* Producer = Producers.FindProducer(ProducerHandle);
			if (Producer)
			{
				uint8 LocalLayerMaskToLoad = 0u;
				for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
				{
					FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
					FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
					const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, Tile.Local_vAddress, Tile.Local_vLevel);
					if (pAddress == ~0u)
					{
						LocalLayerMaskToLoad |= (1u << LocalLayerIndex);
					}
					else
					{
						PagePool.Lock(pAddress);
					}
				}
				if (LocalLayerMaskToLoad != 0u)
				{
					MergedRequestList->LockLoadRequest(FVirtualTextureLocalTile(Tile.GetProducerHandle(), Tile.Local_vAddress, Tile.Local_vLevel), LocalLayerMaskToLoad);
				}
			}
		}

		TilesToLock.Reset();
	}

	TArray<uint32> PackedTiles;
	if(RequestedPackedTiles.Num() > 0)
	{
		FScopeLock Lock(&RequestedTilesLock);
		PackedTiles = MoveTemp(RequestedPackedTiles);
		RequestedPackedTiles.Reset();
	}

	if (PackedTiles.Num() > 0)
	{
		// Collect explicitly requested tiles
		// These tiles are generated on the current frame, so they are collected/processed in a separate list
		FMemMark RequestPageMark(MemStack);
		FUniquePageList* RequestedPageList = new(MemStack) FUniquePageList;
		RequestedPageList->Initialize();
		for (uint32 Tile : PackedTiles)
		{
			RequestedPageList->Add(Tile, 0xffff);
		}
		GatherRequests(MergedRequestList, RequestedPageList, Frame, MemStack);
	}

	// Pages from feedback buffer were generated several frames ago, so they may no longer be valid for newly allocated VTs
	static uint32 PendingFrameDelay = 3u;
	if (Frame >= PendingFrameDelay)
	{
		GatherRequests(MergedRequestList, MergedUniquePageList, Frame - PendingFrameDelay, MemStack);
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Sort);
		// Limit the number of uploads
		// Are all pages equal? Should there be different limits on different types of pages?
		// if not async, 'infinite' uploads
		const uint32 MaxNumUploads = CVarVTMaxUploadsPerFrame.GetValueOnRenderThread();
		MergedRequestList->SortRequests(Producers, MemStack, MaxNumUploads);
	}

	// Submit the requests to produce pages that are already mapped
	SubmitPreMappedRequests(RHICmdList, FeatureLevel);
	// Submit the merged requests
	SubmitRequests(RHICmdList, FeatureLevel, MemStack, MergedRequestList, true);
}

void FVirtualTextureSystem::GatherRequests(FUniqueRequestList* MergedRequestList, const FUniquePageList* UniquePageList, uint32 FrameRequested, FMemStack& MemStack)
{
	FMemMark GatherMark(MemStack);

	const uint32 MaxNumGatherTasks = FMath::Clamp(CVarVTNumGatherTasks.GetValueOnRenderThread(), 1, MaxNumTasks);
	const uint32 PageUpdateFlushCount = FMath::Min<uint32>(CVarVTPageUpdateFlushCount.GetValueOnRenderThread(), FPageUpdateBuffer::PageCapacity);

	FGatherRequestsParameters GatherRequestsParameters[MaxNumTasks];
	uint32 NumGatherTasks = 0u;
	{
		const uint32 MinNumPagesPerTask = 64u;
		const uint32 NumPagesPerTask = FMath::Max(FMath::DivideAndRoundUp(UniquePageList->GetNum(), MaxNumGatherTasks), MinNumPagesPerTask);
		const uint32 NumPages = UniquePageList->GetNum();
		uint32 StartPageIndex = 0u;
		while (StartPageIndex < NumPages)
		{
			const uint32 NumPagesForTask = FMath::Min(NumPagesPerTask, NumPages - StartPageIndex);
			if (NumPagesForTask > 0u)
			{
				const uint32 TaskIndex = NumGatherTasks++;
				FGatherRequestsParameters& Params = GatherRequestsParameters[TaskIndex];
				Params.System = this;
				Params.FrameRequested = FrameRequested;
				Params.UniquePageList = UniquePageList;
				Params.PageUpdateFlushCount = PageUpdateFlushCount;
				Params.PageUpdateBuffers = new(MemStack) FPageUpdateBuffer[PhysicalSpaces.Num()];
				if (TaskIndex == 0u)
				{
					Params.RequestList = MergedRequestList;
				}
				else
				{
					Params.RequestList = new(MemStack) FUniqueRequestList(MemStack);
				}
				Params.PageStartIndex = StartPageIndex;
				Params.NumPages = NumPagesForTask;
				StartPageIndex += NumPagesForTask;
			}
		}
	}

	// Kick all of the tasks
	FGraphEventArray Tasks;
	if (NumGatherTasks > 1u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_SubmitTasks);
		Tasks.Reserve(NumGatherTasks - 1u);
		for (uint32 TaskIndex = 1u; TaskIndex < NumGatherTasks; ++TaskIndex)
		{
			Tasks.Add(TGraphTask<FGatherRequestsTask>::CreateTask().ConstructAndDispatchWhenReady(GatherRequestsParameters[TaskIndex]));
		}
	}

	if (NumGatherTasks > 0u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Gather);

		// first task can run on this thread
		GatherRequestsTask(GatherRequestsParameters[0]);

		// Wait for them to complete
		if (Tasks.Num() > 0)
		{
			FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GetRenderThread_Local());
		}
	}

	// Merge request lists for all tasks
	if (NumGatherTasks > 1u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MergeRequests);
		for (uint32 TaskIndex = 1u; TaskIndex < NumGatherTasks; ++TaskIndex)
		{
			MergedRequestList->MergeRequests(GatherRequestsParameters[TaskIndex].RequestList, MemStack);
		}
	}
}

void FVirtualTextureSystem::AddPageUpdate(FPageUpdateBuffer* Buffers, uint32 FlushCount, uint32 PhysicalSpaceID, uint16 pAddress)
{
	FPageUpdateBuffer& RESTRICT Buffer = Buffers[PhysicalSpaceID];
	if (pAddress == Buffer.PrevPhysicalAddress)
	{
		return;
	}
	Buffer.PrevPhysicalAddress = pAddress;

	bool bLocked = false;
	if (Buffer.NumPages >= FlushCount)
	{
		// Once we've passed a certain threshold of pending pages to update, try to take the lock then apply the updates
		FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceID);
		FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
		FCriticalSection& RESTRICT Lock = PagePool.GetLock();

		if (Buffer.NumPages >= FPageUpdateBuffer::PageCapacity)
		{
			// If we've reached capacity, need to take the lock no matter what, may potentially block here
			Lock.Lock();
			bLocked = true;
		}
		else
		{
			// try to take the lock, but avoid stalling
			bLocked = Lock.TryLock();
		}

		if(bLocked)
		{
			const uint32 CurrentFrame = Frame;
			PagePool.UpdateUsage(CurrentFrame, pAddress); // Update current request now, if we manage to get the lock
			for (uint32 i = 0u; i < Buffer.NumPages; ++i)
			{
				PagePool.UpdateUsage(CurrentFrame, Buffer.PhysicalAddresses[i]);
			}
			Lock.Unlock();
			Buffer.NumPageUpdates += (Buffer.NumPages + 1u);
			Buffer.NumPages = 0u;
		}
	}

	// Only need to buffer if we didn't lock (otherwise this has already been updated)
	if (!bLocked)
	{
		check(Buffer.NumPages < FPageUpdateBuffer::PageCapacity);
		Buffer.PhysicalAddresses[Buffer.NumPages++] = pAddress;
	}
}

void FVirtualTextureSystem::GatherRequestsTask(const FGatherRequestsParameters& Parameters)
{
	const FUniquePageList* RESTRICT UniquePageList = Parameters.UniquePageList;
	FPageUpdateBuffer* RESTRICT PageUpdateBuffers = Parameters.PageUpdateBuffers;
	FUniqueRequestList* RESTRICT RequestList = Parameters.RequestList;
	const uint32 PageUpdateFlushCount = Parameters.PageUpdateFlushCount;
	const uint32 PageEndIndex = Parameters.PageStartIndex + Parameters.NumPages;

	uint32 NumRequestsPages = 0u;
	uint32 NumResidentPages = 0u;
	uint32 NumNonResidentPages = 0u;
	uint32 NumPrefetchPages = 0u;

#if WITH_EDITOR
	TSet<FVirtualTextureLocalTile> ContinuousUpdateTilesToProduceThreadLocal;
#endif

	for (uint32 i = Parameters.PageStartIndex; i < PageEndIndex; ++i)
	{
		const uint32 PageEncoded = UniquePageList->GetPage(i);
		const uint32 PageCount = UniquePageList->GetCount(i);

		// Decode page
		const uint32 ID = (PageEncoded >> 28);
		const FVirtualTextureSpace* RESTRICT Space = GetSpace(ID);
		if (Space == nullptr)
		{
			continue;
		}

		const uint32 vPageX = PageEncoded & 0xfff;
		const uint32 vPageY = (PageEncoded >> 12) & 0xfff;
		const uint32 vLevel = (PageEncoded >> 24) & 0x0f;
		const uint32 vPosition = FMath::MortonCode2(vPageX) | (FMath::MortonCode2(vPageY) << 1);

		// vPosition holds morton interleaved tileX/Y position, shifted down relative to current mip
		// vAddress is the same quantity, but shifted to be relative to mip0
		const uint32 vDimensions = Space->GetDimensions();
		const uint32 vAddress = vPosition << (vLevel * vDimensions);

		uint32 LayersToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0 };
		uint32 NumLayersToLoad = 0u;
		{
			const FTexturePage VirtualPage(vLevel, vAddress);
			const uint16 VirtualPageHash = MurmurFinalize32(VirtualPage.Packed);
			for (uint32 LayerIndex = 0u; LayerIndex < Space->GetNumLayers(); ++LayerIndex)
			{
				const FTexturePageMap& RESTRICT PageMap = Space->GetPageMap(LayerIndex);

				++NumRequestsPages;
				const FPhysicalSpaceIDAndAddress PhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(VirtualPage, VirtualPageHash);
				if (PhysicalSpaceIDAndAddress.Packed != ~0u)
				{
#if DO_GUARD_SLOW
					const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID);
					checkSlow(PhysicalSpaceIDAndAddress.pAddress < PhysicalSpace->GetNumTiles());
#endif // DO_GUARD_SLOW

					// Page is already resident, just need to update LRU free list
					AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpaceIDAndAddress.PhysicalSpaceID, PhysicalSpaceIDAndAddress.pAddress);

				#if WITH_EDITOR
					{
						if (GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID)->GetDescription().bContinuousUpdate)
						{
							FTexturePagePool& RESTRICT PagePool = GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID)->GetPagePool();

							ContinuousUpdateTilesToProduceThreadLocal.Add(PagePool.GetLocalTileFromPhysicalAddress(PhysicalSpaceIDAndAddress.pAddress));
						}
					}
				#endif

					++PageUpdateBuffers[PhysicalSpaceIDAndAddress.PhysicalSpaceID].WorkingSetSize;
					++NumResidentPages;
				}
				else
				{
					// Page not resident, store for later processing
					LayersToLoad[NumLayersToLoad++] = LayerIndex;
				}
			}
		}

		if (NumLayersToLoad == 0u)
		{
			// All pages are resident and properly mapped, we're done
			// This is the fast path, as most frames should generally have the majority of tiles already mapped
			continue;
		}

		// Need to resolve AllocatedVT in order to determine which pages to load
		uint32 AllocatedLocal_vAddress = 0;
		const FAllocatedVirtualTexture* RESTRICT AllocatedVT = Space->GetAllocator().Find(vAddress, AllocatedLocal_vAddress);
		if (!AllocatedVT)
		{
			if (CVarVTVerbose.GetValueOnRenderThread())
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Space %i, vAddr %i@%i is not allocated to any AllocatedVT but was still requested."), ID, vAddress, vLevel);
			}
			continue;
		}

		if (AllocatedVT->GetFrameAllocated() > Parameters.FrameRequested)
		{
			// If the VT was allocated after the frame that generated this feedback, it's no longer valid
			continue;
		}

		checkSlow(AllocatedVT->GetNumLayers() == Space->GetNumLayers());
		if (vLevel > AllocatedVT->GetMaxLevel())
		{
			// Requested level is outside the given allocated VT
			// This can happen for requests made by expanding mips, since we don't know the current allocated VT in that context
			check(NumLayersToLoad == Space->GetNumLayers()); // no pages from this request should have been resident
			check(NumRequestsPages >= Space->GetNumLayers()); // don't want to track these requests, since it turns out they're not valid
			NumRequestsPages -= Space->GetNumLayers();
			continue;
		}

		const uint32 NumUniqueProducers = AllocatedVT->GetNumUniqueProducers();
		uint8 LocalLayerMaskToLoadForProducer[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0u };
		for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumLayersToLoad; ++LoadLayerIndex)
		{
			const uint32 LayerIndex = LayersToLoad[LoadLayerIndex];
			const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = AllocatedVT->GetPhysicalSpace(LayerIndex);
			const uint32 ProducerIndexForLayer = AllocatedVT->GetUniqueProducerIndexForLayer(LayerIndex);
			if (ProducerIndexForLayer < NumUniqueProducers)
			{
				const uint32 LocalLayerToProduce = AllocatedVT->GetLocalLayerToProduce(LayerIndex);
				LocalLayerMaskToLoadForProducer[ProducerIndexForLayer] |= (1u << LocalLayerToProduce);
				++PageUpdateBuffers[PhysicalSpace->GetID()].WorkingSetSize;
			}
		}

		for (uint32 ProducerIndex = 0u; ProducerIndex < NumUniqueProducers; ++ProducerIndex)
		{
			uint8 LocalLayerMaskToLoad = LocalLayerMaskToLoadForProducer[ProducerIndex];
			if (LocalLayerMaskToLoad == 0u)
			{
				continue;
			}

			const FVirtualTextureProducerHandle ProducerHandle = AllocatedVT->GetUniqueProducerHandle(ProducerIndex);
			const FVirtualTextureProducer* RESTRICT Producer = Producers.FindProducer(ProducerHandle);
			if (!Producer)
			{
				continue;
			}

			const uint32 ProducerMipBias = AllocatedVT->GetUniqueProducerMipBias(ProducerIndex);
			uint32 Mapping_vLevel = FMath::Max(vLevel, ProducerMipBias);

			// rescale vAddress to the correct tile within the given mip level
			// here vLevel is clamped against ProducerMipBias, as ProducerMipBias represents the most detailed level of this producer, relative to the allocated VT
			uint32 Local_vAddress = AllocatedLocal_vAddress >> (Mapping_vLevel * vDimensions);

			// Local_vLevel is the level within the producer that we want to allocate/map
			// here we subtract ProducerMipBias (clamped to ensure we don't fall below 0),
			// which effectively matches more detailed mips of lower resolution producers with less detailed mips of higher resolution producers
			uint8 Local_vLevel = vLevel - FMath::Min(vLevel, ProducerMipBias);

			const uint32 LocalMipBias = Producer->GetVirtualTexture()->GetLocalMipBias(Local_vLevel, Local_vAddress);
			if (LocalMipBias > 0u)
			{
				Local_vLevel += LocalMipBias;
				if (Local_vLevel > Producer->GetMaxLevel())
				{
					continue;
				}
				Local_vAddress >>= (LocalMipBias * vDimensions);
				Mapping_vLevel = FMath::Max(vLevel, LocalMipBias + ProducerMipBias);
			}

			uint8 LocalLayerMaskToPrefetchForLevel[16] = { 0u };
			uint32 MaxPrefetchLocal_vLevel = Local_vLevel;

			for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
			{
				if ((LocalLayerMaskToLoad & (1u << LocalLayerIndex)) == 0u)
				{
					continue;
				}

				const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
				const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();

				// Find the highest resolution tile that's currently loaded
				const uint32 pAddress = PagePool.FindNearestPageAddress(ProducerHandle, LocalLayerIndex, Local_vAddress, Local_vLevel, Producer->GetMaxLevel());
				uint32 AllocatedLocal_vLevel = Producer->GetMaxLevel() + 1u;
				if (pAddress != ~0u)
				{
					AllocatedLocal_vLevel = PagePool.GetLocalLevelForAddress(pAddress);
					check(AllocatedLocal_vLevel >= Local_vLevel);

					const uint32 Allocated_vLevel = AllocatedLocal_vLevel + ProducerMipBias;
					check(Allocated_vLevel <= AllocatedVT->GetMaxLevel());

					const uint32 AllocatedMapping_vLevel = FMath::Max(Allocated_vLevel, ProducerMipBias);
					const uint32 Allocated_vAddress = vAddress & (0xffffffff << (Allocated_vLevel * vDimensions));

					AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpace->GetID(), pAddress);

					uint32 NumMappedPages = 0u;
					for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumLayersToLoad; ++LoadLayerIndex)
					{
						const uint32 LayerIndex = LayersToLoad[LoadLayerIndex];
						if (AllocatedVT->GetLocalLayerToProduce(LayerIndex) == LocalLayerIndex &&
							AllocatedVT->GetUniqueProducerIndexForLayer(LayerIndex) == ProducerIndex)
						{
							bool bPageWasMapped = false;
							if (Allocated_vLevel != vLevel)
							{
								// if we found a lower resolution tile than was requested, it may have already been mapped, check for that first
								// don't need to check this if the allocated page is at the level that was requested...if that was already mapped we wouldn't have gotten this far
								const FTexturePageMap& PageMap = Space->GetPageMap(LayerIndex);
								const FPhysicalSpaceIDAndAddress PrevPhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(Allocated_vLevel, Allocated_vAddress);
								if (PrevPhysicalSpaceIDAndAddress.Packed != ~0u)
								{
									// if this address was previously mapped, ensure that it was mapped by the same physical space
									ensure(PrevPhysicalSpaceIDAndAddress.PhysicalSpaceID == PhysicalSpace->GetID());
									// either it wasn't mapped, or it's mapped to the current physical address...
									// otherwise that means that the same local tile is mapped to two separate physical addresses, which is an error
									ensure(PrevPhysicalSpaceIDAndAddress.pAddress == pAddress);
									bPageWasMapped = true;
								}
							}
#if DO_GUARD_SLOW
							else
							{
								// verify our assumption that the page shouldn't be mapped yet
								const FTexturePageMap& PageMap = Space->GetPageMap(LayerIndex);
								const FPhysicalSpaceIDAndAddress PrevPhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(Allocated_vLevel, Allocated_vAddress);
								checkSlow(PrevPhysicalSpaceIDAndAddress.Packed == ~0u);
							}
#endif // DO_GUARD_SLOW

							if (!bPageWasMapped)
							{
								// map the page now if it wasn't already mapped
								RequestList->AddDirectMappingRequest(Space->GetID(), PhysicalSpace->GetID(), LayerIndex, Allocated_vLevel, Allocated_vAddress, AllocatedMapping_vLevel, pAddress);
							}
							++NumMappedPages;
						}
					}
					check(NumMappedPages > 0u);
				}

				if (AllocatedLocal_vLevel == Local_vLevel)
				{
					// page at the requested level was already resident, no longer need to load
					LocalLayerMaskToLoad &= ~(1u << LocalLayerIndex);
					++NumResidentPages;
				}
				else
				{
					// page not resident...see if we want to prefetch a page with resolution incrementally larger than what's currently resident
					// this means we'll ultimately load more data, but these lower resolution pages should load much faster than the requested high resolution page
					// this should make popping less noticeable
					const uint32 PrefetchLocal_vLevel = AllocatedLocal_vLevel - FMath::Min(2u, AllocatedLocal_vLevel);
					if (PrefetchLocal_vLevel > Local_vLevel)
					{
						LocalLayerMaskToPrefetchForLevel[PrefetchLocal_vLevel] |= (1u << LocalLayerIndex);
						MaxPrefetchLocal_vLevel = FMath::Max(MaxPrefetchLocal_vLevel, PrefetchLocal_vLevel);
						++NumPrefetchPages;
					}
					++NumNonResidentPages;
				}
			}

			// Check to see if we have any levels to prefetch
			for (uint32 PrefetchLocal_vLevel = Local_vLevel + 1u; PrefetchLocal_vLevel <= MaxPrefetchLocal_vLevel; ++PrefetchLocal_vLevel)
			{
				uint32 LocalLayerMaskToPrefetch = LocalLayerMaskToPrefetchForLevel[PrefetchLocal_vLevel];
				if (LocalLayerMaskToPrefetch != 0u)
				{
					const uint32 PrefetchLocal_vAddress = Local_vAddress >> ((PrefetchLocal_vLevel - Local_vLevel) * vDimensions);

					// If we want to prefetch any layers for a given level, need to ensure that we request all the layers that aren't currently loaded
					// This is required since the VT producer interface needs to be able to write data for all layers if desired, so we need to make sure that all layers are allocated
					for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
					{
						if ((LocalLayerMaskToPrefetch & (1u << LocalLayerIndex)) == 0u)
						{
							const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
							const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, PrefetchLocal_vAddress, PrefetchLocal_vLevel);
							if (pAddress == ~0u)
							{
								LocalLayerMaskToPrefetch |= (1u << LocalLayerIndex);
								++NumPrefetchPages;
							}
						}
					}

					const uint16 LoadRequestIndex = RequestList->AddLoadRequest(FVirtualTextureLocalTile(ProducerHandle, PrefetchLocal_vAddress, PrefetchLocal_vLevel), LocalLayerMaskToPrefetch, PageCount);
					if (LoadRequestIndex != 0xffff)
					{
						const uint32 Prefetch_vLevel = PrefetchLocal_vLevel + ProducerMipBias;
						check(Prefetch_vLevel <= AllocatedVT->GetMaxLevel());
						const uint32 PrefetchMapping_vLevel = FMath::Max(Prefetch_vLevel, ProducerMipBias);
						const uint32 Prefetch_vAddress = vAddress & (0xffffffff << (Prefetch_vLevel * vDimensions));
						for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumLayersToLoad; ++LoadLayerIndex)
						{
							const uint32 LayerIndex = LayersToLoad[LoadLayerIndex];
							if (AllocatedVT->GetUniqueProducerIndexForLayer(LayerIndex) == ProducerIndex)
							{
								const uint32 LocalLayerToProduce = AllocatedVT->GetLocalLayerToProduce(LayerIndex);
								if (LocalLayerMaskToPrefetch & (1u << LocalLayerToProduce))
								{
									RequestList->AddMappingRequest(LoadRequestIndex, LocalLayerToProduce, ID, LayerIndex, Prefetch_vAddress, Prefetch_vLevel, PrefetchMapping_vLevel);
								}
							}
						}
					}
				}
			}

			// it's possible that 'LocalLayerMaskToLoad' is now 0, if all the required pages were already resident and simply needed to be mapped
			if (LocalLayerMaskToLoad != 0u)
			{
				const uint16 LoadRequestIndex = RequestList->AddLoadRequest(FVirtualTextureLocalTile(ProducerHandle, Local_vAddress, Local_vLevel), LocalLayerMaskToLoad, PageCount);
				if (LoadRequestIndex != 0xffff)
				{
					for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumLayersToLoad; ++LoadLayerIndex)
					{
						const uint32 LayerIndex = LayersToLoad[LoadLayerIndex];
						if (AllocatedVT->GetUniqueProducerIndexForLayer(LayerIndex) == ProducerIndex)
						{
							const uint32 LocalLayerToProduce = AllocatedVT->GetLocalLayerToProduce(LayerIndex);
							if (LocalLayerMaskToLoad & (1u << LocalLayerToProduce))
							{
								RequestList->AddMappingRequest(LoadRequestIndex, LocalLayerToProduce, ID, LayerIndex, vAddress, vLevel, Mapping_vLevel);
							}
						}
					}
				}
			}
		}
	}

	for (uint32 PhysicalSpaceID = 0u; PhysicalSpaceID < (uint32)PhysicalSpaces.Num(); ++PhysicalSpaceID)
	{
		FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceID);
		FPageUpdateBuffer& RESTRICT Buffer = PageUpdateBuffers[PhysicalSpaceID];

		if (Buffer.WorkingSetSize > 0u)
		{
			PhysicalSpace->IncrementWorkingSetSize(Buffer.WorkingSetSize);
		}

		if (Buffer.NumPages > 0u)
		{
			Buffer.NumPageUpdates += Buffer.NumPages;
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();

			FScopeLock Lock(&PagePool.GetLock());
			for (uint32 i = 0u; i < Buffer.NumPages; ++i)
			{
				PagePool.UpdateUsage(Frame, Buffer.PhysicalAddresses[i]);
			}

		#if WITH_EDITOR
			if (PhysicalSpace->GetDescription().bContinuousUpdate)
			{
				FScopeLock ScopeLock(&ContinuousUpdateTilesToProduceCS);
				ContinuousUpdateTilesToProduce.Append(ContinuousUpdateTilesToProduceThreadLocal);
			}
		#endif
		}
		
		INC_DWORD_STAT_BY(STAT_NumPageUpdate, Buffer.NumPageUpdates);
	}

	INC_DWORD_STAT_BY(STAT_NumPageVisible, NumRequestsPages);
	INC_DWORD_STAT_BY(STAT_NumPageVisibleResident, NumResidentPages);
	INC_DWORD_STAT_BY(STAT_NumPageVisibleNotResident, NumNonResidentPages);
	INC_DWORD_STAT_BY(STAT_NumPagePrefetch, NumPrefetchPages);
}

void FVirtualTextureSystem::SubmitRequestsFromLocalTileList(const TSet<FVirtualTextureLocalTile>& LocalTileList, EVTProducePageFlags Flags, FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	for (const FVirtualTextureLocalTile& Tile : LocalTileList)
	{
		const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
		const FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);

		// Fill targets for each layer
		uint32 LayerMask = 0;
		FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
		for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer.GetNumLayers(); ++LocalLayerIndex)
		{
			FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpace(LocalLayerIndex);
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
			const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, Tile.Local_vAddress, Tile.Local_vLevel);
			if (pAddress != ~0u)
			{
				ProduceTarget[LocalLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture();
				if (PhysicalSpace->GetDescription().bCreateRenderTarget)
				{
					ProduceTarget[LocalLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget();
				}
				ProduceTarget[LocalLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
				LayerMask |= 1 << LocalLayerIndex;
			}
		}

		if (LayerMask == 0)
		{
			// If we don't have anything mapped then we can ignore (since we only want to refresh existing mapped data)
			continue;
		}

		FVTRequestPageResult RequestPageResult = Producer.GetVirtualTexture()->RequestPageData(
			ProducerHandle, LayerMask, Tile.Local_vLevel, Tile.Local_vAddress, EVTRequestPagePriority::High);

		if (RequestPageResult.Status != EVTRequestPageStatus::Available)
		{
			//todo[vt]: Should we unmap? Or maybe keep the request for the next frame?
			continue;
		}

		IVirtualTextureFinalizer* VTFinalizer = Producer.GetVirtualTexture()->ProducePageData(
			RHICmdList, FeatureLevel,
			Flags,
			ProducerHandle, LayerMask, Tile.Local_vLevel, Tile.Local_vAddress,
			RequestPageResult.Handle,
			ProduceTarget);

		if (VTFinalizer != nullptr)
		{
			// Add the finalizer here but note that we don't call Finalize until SubmitRequests()
			Finalizers.AddUnique(VTFinalizer);
		}
	}
}

void FVirtualTextureSystem::SubmitPreMappedRequests(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	{
		SubmitRequestsFromLocalTileList(MappedTilesToProduce, EVTProducePageFlags::None, RHICmdList, FeatureLevel);
		MappedTilesToProduce.Reset();
	}

	{
		INC_DWORD_STAT_BY(STAT_NumContinuousPageUpdate, ContinuousUpdateTilesToProduce.Num());
		SubmitRequestsFromLocalTileList(ContinuousUpdateTilesToProduce, EVTProducePageFlags::None, RHICmdList, FeatureLevel);
		ContinuousUpdateTilesToProduce.Reset();
	}
}

void FVirtualTextureSystem::SubmitRequests(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FMemStack& MemStack, FUniqueRequestList* RequestList, bool bAsync)
{
	FMemMark Mark(MemStack);

	// Allocate space to hold the physical address we allocate for each page load (1 page per layer per request)
	uint32* RequestPhysicalAddress = new(MemStack, MEM_Oned) uint32[RequestList->GetNumLoadRequests() * VIRTUALTEXTURE_SPACE_MAXLAYERS];
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Submit);

		uint32 NumStacksProduced = 0u;
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumLoadRequests(); ++RequestIndex)
		{
			const FVirtualTextureLocalTile TileToLoad = RequestList->GetLoadRequest(RequestIndex);
			const uint32 LocalLayerMask = RequestList->GetLocalLayerMask(RequestIndex);
			const bool bLockTile = RequestList->IsLocked(RequestIndex);

			const FVirtualTextureProducerHandle ProducerHandle = TileToLoad.GetProducerHandle();
			const FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);

			const EVTRequestPagePriority Priority = bLockTile ? EVTRequestPagePriority::High : EVTRequestPagePriority::Normal;
			FVTRequestPageResult RequestPageResult = Producer.GetVirtualTexture()->RequestPageData(ProducerHandle, LocalLayerMask, TileToLoad.Local_vLevel, TileToLoad.Local_vAddress, Priority);
			if (RequestPageResult.Status == EVTRequestPageStatus::Pending && (bLockTile || !bAsync))
			{
				// If we're trying to lock this tile, we're OK producing data now (and possibly waiting) as long as data is pending
				// If we render a frame without all locked tiles loaded, may render garbage VT data, as there won't be low mip fallback for unloaded tiles
				RequestPageResult.Status = EVTRequestPageStatus::Available;
			}

			bool bTileLoaded = false;
			if (RequestPageResult.Status == EVTRequestPageStatus::Invalid)
			{
				if (CVarVTVerbose.GetValueOnRenderThread())
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("vAddr %i@%i is not a valid request for AllocatedVT but is still requested."), TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
				}
			}
			else if (RequestPageResult.Status == EVTRequestPageStatus::Available)
			{
				FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
				uint32 Allocate_pAddress[VIRTUALTEXTURE_SPACE_MAXLAYERS];
				FMemory::Memset(Allocate_pAddress, 0xff);

				// try to allocate a page for each layer we need to load
				bool bProduceTargetValid = true;
				for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer.GetNumLayers(); ++LocalLayerIndex)
				{
					// If mask isn't set, we must already have a physical tile allocated for this layer, don't need to allocate another one
					if (LocalLayerMask & (1u << LocalLayerIndex))
					{
						FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpace(LocalLayerIndex);
						FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
						if (PagePool.AnyFreeAvailable(Frame))
						{
							const uint32 pAddress = PagePool.Alloc(this, Frame, ProducerHandle, LocalLayerIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel, bLockTile);
							check(pAddress != ~0u);

							ProduceTarget[LocalLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture();
							if (PhysicalSpace->GetDescription().bCreateRenderTarget)
							{
								ProduceTarget[LocalLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget();
							}
							ProduceTarget[LocalLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
							Allocate_pAddress[LocalLayerIndex] = pAddress;
						}
						else
						{
							const FPixelFormatInfo& PoolFormatInfo = GPixelFormats[PhysicalSpace->GetFormat()];
							UE_LOG(LogConsoleResponse, Display, TEXT("Failed to allocate VT page from pool PF_%s"), PoolFormatInfo.Name);
							bProduceTargetValid = false;
							break;
						}
					}
				}

				if (bProduceTargetValid)
				{
					// Successfully allocated required pages, now we can make the request
					for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer.GetNumLayers(); ++LocalLayerIndex)
					{
						if (LocalLayerMask & (1u << LocalLayerIndex))
						{
							// Associate the addresses we allocated with this request, so they can be mapped if required
							const uint32 pAddress = Allocate_pAddress[LocalLayerIndex];
							check(pAddress != ~0u);
							RequestPhysicalAddress[RequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + LocalLayerIndex] = pAddress;
						}
						else
						{
							// Fill in pAddress for layers that are already resident
							FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpace(LocalLayerIndex);
							FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
							check(pAddress != ~0u);
							ProduceTarget[LocalLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture();
							ProduceTarget[LocalLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
						}
					}

					IVirtualTextureFinalizer* VTFinalizer = Producer.GetVirtualTexture()->ProducePageData(RHICmdList, FeatureLevel,
						EVTProducePageFlags::None,
						ProducerHandle, LocalLayerMask, TileToLoad.Local_vLevel, TileToLoad.Local_vAddress,
						RequestPageResult.Handle,
						ProduceTarget);
					if (VTFinalizer)
					{
						Finalizers.AddUnique(VTFinalizer); // we expect the number of unique finalizers to be very limited. if this changes, we might have to do something better then gathering them every update
					}

					bTileLoaded = true;
					++NumStacksProduced;
				}
				else
				{
					// Failed to allocate required physical pages for the tile, free any pages we did manage to allocate
					for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer.GetNumLayers(); ++LocalLayerIndex)
					{
						const uint32 pAddress = Allocate_pAddress[LocalLayerIndex];
						if (pAddress != ~0u)
						{
							FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpace(LocalLayerIndex);
							FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							PagePool.Free(this, pAddress);
						}
					}
				}
			}

			if (bLockTile && !bTileLoaded)
			{
				// Want to lock this tile, but didn't manage to load it this frame, add it back to the list to try the lock again next frame
				TilesToLock.Add(TileToLoad);
			}
		}

		INC_DWORD_STAT_BY(STAT_NumStacksRequested, RequestList->GetNumLoadRequests());
		INC_DWORD_STAT_BY(STAT_NumStacksProduced, NumStacksProduced);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Map);

		// Update page mappings that were directly requested
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumDirectMappingRequests(); ++RequestIndex)
		{
			const FDirectMappingRequest MappingRequest = RequestList->GetDirectMappingRequest(RequestIndex);
			FVirtualTextureSpace* Space = GetSpace(MappingRequest.SpaceID);
			FVirtualTexturePhysicalSpace* PhysicalSpace = GetPhysicalSpace(MappingRequest.PhysicalSpaceID);

			PhysicalSpace->GetPagePool().MapPage(Space, PhysicalSpace, MappingRequest.LayerIndex, MappingRequest.vLevel, MappingRequest.vAddress, MappingRequest.Local_vLevel, MappingRequest.pAddress);
		}

		// Update page mappings for any requested page that completed allocation this frame
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumMappingRequests(); ++RequestIndex)
		{
			const FMappingRequest MappingRequest = RequestList->GetMappingRequest(RequestIndex);
			const uint32 pAddress = RequestPhysicalAddress[MappingRequest.LoadRequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + MappingRequest.LocalLayerIndex];
			if (pAddress != ~0u)
			{
				const FVirtualTextureLocalTile& TileToLoad = RequestList->GetLoadRequest(MappingRequest.LoadRequestIndex);
				const FVirtualTextureProducerHandle ProducerHandle = TileToLoad.GetProducerHandle();
				FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);
				FVirtualTexturePhysicalSpace* PhysicalSpace = Producer.GetPhysicalSpace(MappingRequest.LocalLayerIndex);
				FVirtualTextureSpace* Space = GetSpace(MappingRequest.SpaceID);
				check(RequestList->GetLocalLayerMask(MappingRequest.LoadRequestIndex) & (1u << MappingRequest.LocalLayerIndex));

				PhysicalSpace->GetPagePool().MapPage(Space, PhysicalSpace, MappingRequest.LayerIndex, MappingRequest.vLevel, MappingRequest.vAddress, MappingRequest.Local_vLevel, pAddress);
			}
		}
	}

	// Map any resident tiles to newly allocated VTs
	{
		uint32 Index = 0u;
		while (Index < (uint32)AllocatedVTsToMap.Num())
		{
			const FAllocatedVirtualTexture* AllocatedVT = AllocatedVTsToMap[Index];
			const uint32 vDimensions = AllocatedVT->GetDimensions();
			const uint32 WidthInTiles = AllocatedVT->GetWidthInTiles();
			const uint32 HeightInTiles = AllocatedVT->GetHeightInTiles();
			const uint32 BaseTileX = FMath::ReverseMortonCode2(AllocatedVT->GetVirtualAddress());
			const uint32 BaseTileY = FMath::ReverseMortonCode2(AllocatedVT->GetVirtualAddress() >> 1);
			FVirtualTextureSpace* Space = AllocatedVT->GetSpace();

			uint32 NumFullyMappedLayers = 0u;
			for (uint32 LayerIndex = 0u; LayerIndex < AllocatedVT->GetNumLayers(); ++LayerIndex)
			{
				const uint32 ProducerIndex = AllocatedVT->GetUniqueProducerIndexForLayer(LayerIndex);
				const uint32 ProducerMipBias = AllocatedVT->GetUniqueProducerMipBias(ProducerIndex);
				const uint32 LocalLayerIndex = AllocatedVT->GetLocalLayerToProduce(LayerIndex);
				const FVirtualTextureProducerHandle ProducerHandle = AllocatedVT->GetUniqueProducerHandle(ProducerIndex);
				const FVirtualTextureProducer* Producer = Producers.FindProducer(ProducerHandle);
				FVirtualTexturePhysicalSpace* PhysicalSpace = AllocatedVT->GetPhysicalSpace(LayerIndex);
				FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
				FTexturePageMap& PageMap = Space->GetPageMap(LayerIndex);
				
				bool bIsLayerFullyMapped = false;
				for (uint32 Local_vLevel = 0u; Local_vLevel <= Producer->GetMaxLevel(); ++Local_vLevel)
				{
					const uint32 vLevel = Local_vLevel + ProducerMipBias;
					const uint32 LevelWidthInTiles = FMath::Max(WidthInTiles >> vLevel, 1u);
					const uint32 LevelHeightInTiles = FMath::Max(HeightInTiles >> vLevel, 1u);

					uint32 NumNonResidentPages = 0u;
					for (uint32 TileY = 0u; TileY < LevelHeightInTiles; ++TileY)
					{
						for (uint32 TileX = 0u; TileX < LevelWidthInTiles; ++TileX)
						{
							const uint32 vAddress = FMath::MortonCode2(BaseTileX + (TileX << vLevel)) | (FMath::MortonCode2(BaseTileY + (TileY << vLevel)) << 1);
							uint32 pAddress = PageMap.FindPageAddress(vLevel, vAddress);
							if (pAddress == ~0u)
							{
								const uint32 Local_vAddress = FMath::MortonCode2(TileX) | (FMath::MortonCode2(TileY) << 1);

								pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, Local_vAddress, Local_vLevel);
								if (pAddress != ~0u)
								{
									PagePool.MapPage(Space, PhysicalSpace, LayerIndex, vLevel, vAddress, vLevel, pAddress);
								}
								else
								{
									++NumNonResidentPages;
								}
							}
						}
					}

					if (NumNonResidentPages == 0u && !bIsLayerFullyMapped)
					{
						bIsLayerFullyMapped = true;
						++NumFullyMappedLayers;
					}
				}
			}

			if (NumFullyMappedLayers < AllocatedVT->GetNumLayers())
			{
				++Index;
			}
			else
			{
				// Remove from list as long as we can fully map at least one mip level of the VT....this way we guarantee all tiles at least have some valid data (even if low resolution)
				// Normally we expect to be able to at least map the least-detailed mip, since those tiles should always be locked/resident
				// It's possible during loading that they may not be available for a few frames however
				AllocatedVTsToMap.RemoveAtSwap(Index, 1, false);
			}
		}

		AllocatedVTsToMap.Shrink();
	}

	// Finalize requests
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Finalize);
		for (IVirtualTextureFinalizer* VTFinalizer : Finalizers)
		{
			VTFinalizer->Finalize(RHICmdList);
		}
		Finalizers.Reset();
	}

	// Update page tables
	{
		SCOPE_CYCLE_COUNTER(STAT_PageTableUpdates);
		for (uint32 ID = 0; ID < MaxSpaces; ID++)
		{
			if (Spaces[ID])
			{
				Spaces[ID]->ApplyUpdates(this, RHICmdList);
			}
		}
	}

	Frame++;
}

void FVirtualTextureSystem::AllocateResources(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	for (uint32 ID = 0; ID < MaxSpaces; ID++)
	{
		if (Spaces[ID])
		{
			Spaces[ID]->AllocateTextures(RHICmdList);
		}
	}
}

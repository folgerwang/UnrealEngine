// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "Templates/UniquePtr.h"
#include "VT/VirtualTextureProducer.h"
#include "VirtualTexturing.h"

class FAllocatedVirtualTexture;
class FScene;
class FUniquePageList;
class FUniqueRequestList;
class FVirtualTexturePhysicalSpace;
class FVirtualTextureProducer;
class FVirtualTextureSpace;
struct FVTSpaceDescription;
struct FVTPhysicalSpaceDescription;
union FPhysicalSpaceIDAndAddress;
struct FFeedbackAnalysisParameters;
struct FGatherRequestsParameters;
struct FPageUpdateBuffer;

extern uint32 GetTypeHash(const FAllocatedVTDescription& Description);

class FVirtualTextureSystem
{
public:
	static void Initialize();
	static void Shutdown();
	static FVirtualTextureSystem& Get();

	void AllocateResources(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel);
	void Update( FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel );

	IAllocatedVirtualTexture* AllocateVirtualTexture(const FAllocatedVTDescription& Desc);
	void DestroyVirtualTexture(IAllocatedVirtualTexture* AllocatedVT);
	void ReleaseVirtualTexture(FAllocatedVirtualTexture* AllocatedVT);
	void RemoveAllocatedVT(FAllocatedVirtualTexture* AllocatedVT);

	FVirtualTextureProducerHandle RegisterProducer(const FVTProducerDescription& InDesc, IVirtualTexture* InProducer);
	void ReleaseProducer(const FVirtualTextureProducerHandle& Handle);

	FVirtualTextureSpace* AcquireSpace(const FVTSpaceDescription& InDesc, uint32 InSizeNeeded);
	void ReleaseSpace(FVirtualTextureSpace* Space);

	FVirtualTexturePhysicalSpace* AcquirePhysicalSpace(const FVTPhysicalSpaceDescription& InDesc);
	void ReleasePhysicalSpace(FVirtualTexturePhysicalSpace* Space);

	FVirtualTextureSpace* GetSpace(uint8 ID) const { check(ID < MaxSpaces); return Spaces[ID].Get(); }
	FVirtualTexturePhysicalSpace* GetPhysicalSpace(uint16 ID) const { return PhysicalSpaces[ID].Get(); }

	TArray<IVirtualTextureFinalizer*>& GetFinalizers() { return Finalizers; }
	FVirtualTextureProducer* FindProducer(const FVirtualTextureProducerHandle& ProducerHandle) { return Producers.FindProducer(ProducerHandle); }
	uint32 GetFrame() { return Frame;  }

	void LockTile(const FVirtualTextureLocalTile& Tile);
	void UnlockTile(const FVirtualTextureLocalTile& Tile);
	void RequestTilesForRegion(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FIntRect& InTextureRegion, int32 InMipLevel = -1);
	void LoadPendingTiles(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel);
	void FlushCache();

private:
	friend class FFeedbackAnalysisTask;
	friend class FGatherRequestsTask;

	FVirtualTextureSystem();
	~FVirtualTextureSystem();

	void DestroyPendingVirtualTextures();

	void RequestTilesForRegionInternal(const IAllocatedVirtualTexture* AllocatedVT, const FIntRect& InTextureRegion, uint32 vLevel);
	
	void SubmitRequestsFromLocalTileList(const TSet<FVirtualTextureLocalTile>& LocalTileList, EVTProducePageFlags Flags, FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel);

	void SubmitPreMappedRequests(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel);

	void SubmitRequests(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FMemStack& MemStack, FUniqueRequestList* RequestList, bool bAsync);

	void GatherRequests(FUniqueRequestList* MergedRequestList, const FUniquePageList* UniquePageList, uint32 FrameRequested, FMemStack& MemStack);

	void AddPageUpdate(FPageUpdateBuffer* Buffers, uint32 FlushCount, uint32 PhysicalSpaceID, uint16 pAddress);

	void GatherRequestsTask(const FGatherRequestsParameters& Parameters);
	void FeedbackAnalysisTask(const FFeedbackAnalysisParameters& Parameters);

	uint32	Frame;

	static const uint32 MaxSpaces = 16;
	TUniquePtr<FVirtualTextureSpace> Spaces[MaxSpaces];
	TArray< TUniquePtr<FVirtualTexturePhysicalSpace> > PhysicalSpaces;
	FVirtualTextureProducerCollection Producers;

	FCriticalSection PendingDeleteLock;
	TArray<FAllocatedVirtualTexture*> PendingDeleteAllocatedVTs;

	TMap<FAllocatedVTDescription, FAllocatedVirtualTexture*> AllocatedVTs;

	bool bFlushCaches;
	void FlushCachesFromConsole();
	FAutoConsoleCommand FlushCachesCommand;

	void DumpFromConsole();
	FAutoConsoleCommand DumpCommand;

	void ListPhysicalPoolsFromConsole();
	FAutoConsoleCommand ListPhysicalPools;

	FCriticalSection RequestedTilesLock;
	TArray<uint32> RequestedPackedTiles;

	TArray<FVirtualTextureLocalTile> TilesToLock;
	FCriticalSection ContinuousUpdateTilesToProduceCS;
	TSet<FVirtualTextureLocalTile> ContinuousUpdateTilesToProduce;
	TSet<FVirtualTextureLocalTile> MappedTilesToProduce;
	TArray<FAllocatedVirtualTexture*> AllocatedVTsToMap;
	TArray<IVirtualTextureFinalizer*> Finalizers;
};



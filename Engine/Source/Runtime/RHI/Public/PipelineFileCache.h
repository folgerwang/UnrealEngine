// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

DECLARE_STATS_GROUP(TEXT("ShaderPipelineCache"),STATGROUP_PipelineStateCache, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Total Graphics Pipeline State Count"), STAT_TotalGraphicsPipelineStateCount, STATGROUP_PipelineStateCache, RHI_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Total Compute Pipeline State Count"), STAT_TotalComputePipelineStateCount, STATGROUP_PipelineStateCache, RHI_API);

#define PIPELINE_CACHE_DEFAULT_ENABLED (!WITH_EDITOR && PLATFORM_MAC)

struct RHI_API FPipelineFileCacheRasterizerState
{
	FPipelineFileCacheRasterizerState() { FMemory::Memzero(*this); }
	FPipelineFileCacheRasterizerState(FRasterizerStateInitializerRHI const& Other) { operator=(Other); }
	
	float DepthBias;
	float SlopeScaleDepthBias;
	TEnumAsByte<ERasterizerFillMode> FillMode;
	TEnumAsByte<ERasterizerCullMode> CullMode;
	bool bAllowMSAA;
	bool bEnableLineAA;
	
	FPipelineFileCacheRasterizerState& operator=(FRasterizerStateInitializerRHI const& Other)
	{
		DepthBias = Other.DepthBias;
		SlopeScaleDepthBias = Other.SlopeScaleDepthBias;
		FillMode = Other.FillMode;
		CullMode = Other.CullMode;
		bAllowMSAA = Other.bAllowMSAA;
		bEnableLineAA = Other.bEnableLineAA;
		return *this;
	}
	
	operator FRasterizerStateInitializerRHI () const
	{
		FRasterizerStateInitializerRHI Initializer = {FillMode, CullMode, DepthBias, SlopeScaleDepthBias, bAllowMSAA, bEnableLineAA};
		return Initializer;
	}
	
	friend RHI_API FArchive& operator<<(FArchive& Ar,FPipelineFileCacheRasterizerState& RasterizerStateInitializer)
	{
		Ar << RasterizerStateInitializer.DepthBias;
		Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
		Ar << RasterizerStateInitializer.FillMode;
		Ar << RasterizerStateInitializer.CullMode;
		Ar << RasterizerStateInitializer.bAllowMSAA;
		Ar << RasterizerStateInitializer.bEnableLineAA;
		return Ar;
	}
	
	friend RHI_API uint32 GetTypeHash(const FPipelineFileCacheRasterizerState &Key)
	{
		uint32 KeyHash = (*((uint32*)&Key.DepthBias) ^ *((uint32*)&Key.SlopeScaleDepthBias));
		KeyHash ^= (Key.FillMode << 8);
		KeyHash ^= Key.CullMode;
		KeyHash ^= Key.bAllowMSAA ? 2 : 0;
		KeyHash ^= Key.bEnableLineAA ? 1 : 0;
		return KeyHash;
	}
	FString ToString() const;
	void FromString(const FString& Src);
};

/**
 * Tracks stats for the current session between opening & closing the file-cache.
 */
struct RHI_API FPipelineStateStats
{
	FPipelineStateStats()
	: FirstFrameUsed(-1)
	, LastFrameUsed(-1)
	, CreateCount(0)
	, TotalBindCount(0)
	, PSOHash(0)
	{
	}
	
	~FPipelineStateStats()
	{
	}

	static void UpdateStats(FPipelineStateStats* Stats);
	
	friend RHI_API FArchive& operator<<( FArchive& Ar, FPipelineStateStats& Info );

	int64 FirstFrameUsed;
	int64 LastFrameUsed;
	uint64 CreateCount;
	int64 TotalBindCount;
	uint32 PSOHash;
};

struct RHI_API FPipelineCacheFileFormatPSO
{
	struct RHI_API ComputeDescriptor
	{
		FSHAHash ComputeShader;

		FString ToString() const;
		static FString HeaderLine();
		void FromString(const FString& Src);
	};
	struct RHI_API GraphicsDescriptor
	{
		FSHAHash VertexShader;
		FSHAHash FragmentShader;
		FSHAHash GeometryShader;
		FSHAHash HullShader;
		FSHAHash DomainShader;
		
		FVertexDeclarationElementList VertexDescriptor;
		FBlendStateInitializerRHI BlendState;
		FPipelineFileCacheRasterizerState RasterizerState;
		FDepthStencilStateInitializerRHI DepthStencilState;
		
		EPixelFormat RenderTargetFormats[MaxSimultaneousRenderTargets];
		uint32 RenderTargetFlags[MaxSimultaneousRenderTargets];
		uint32 RenderTargetsActive;
		uint32 MSAASamples;
		
		EPixelFormat DepthStencilFormat;
		uint32 DepthStencilFlags;
		ERenderTargetLoadAction DepthLoad;
		ERenderTargetLoadAction StencilLoad;
		ERenderTargetStoreAction DepthStore;
		ERenderTargetStoreAction StencilStore;
		
		EPrimitiveType PrimitiveType;

		FString ToString() const;
		static FString HeaderLine();
		void FromString(const FString& Src);

		FString ShadersToString() const;
		static FString ShaderHeaderLine();
		void ShadersFromString(const FString& Src);

		FString StateToString() const;
		static FString StateHeaderLine();
		void StateFromString(const FString& Src);
	};
	enum class DescriptorType : uint32
	{
		Compute = 0,
		Graphics = 1
	};
	
	DescriptorType Type;
	ComputeDescriptor ComputeDesc;
	GraphicsDescriptor GraphicsDesc;
	mutable volatile uint32 Hash;
	
	FPipelineCacheFileFormatPSO();
	~FPipelineCacheFileFormatPSO();
	
	FPipelineCacheFileFormatPSO& operator=(const FPipelineCacheFileFormatPSO& Other);
	FPipelineCacheFileFormatPSO(const FPipelineCacheFileFormatPSO& Other);
	
	bool operator==(const FPipelineCacheFileFormatPSO& Other) const;

	friend RHI_API uint32 GetTypeHash(const FPipelineCacheFileFormatPSO &Key);
	friend RHI_API FArchive& operator<<( FArchive& Ar, FPipelineCacheFileFormatPSO& Info );
	
	static bool Init(FPipelineCacheFileFormatPSO& PSO, FRHIComputeShader const* Init);
	static bool Init(FPipelineCacheFileFormatPSO& PSO, FGraphicsPipelineStateInitializer const& Init);


};
	
struct RHI_API FPipelineCacheFileFormatPSORead
{	
	FPipelineCacheFileFormatPSORead()
	: Ar(nullptr)
	, Hash(0)
	, bReadCompleted(false)
    , bValid(false)
	{}
	
	~FPipelineCacheFileFormatPSORead()
	{
		if(Ar)
		{
			delete Ar;
		}
	}
	
	TArray<uint8> Data;
	FArchive* Ar;
	uint32 Hash;
	bool bReadCompleted;
    bool bValid;
	
	TSharedPtr<class IAsyncReadRequest, ESPMode::ThreadSafe> ReadRequest;
	TSharedPtr<class IAsyncReadFileHandle, ESPMode::ThreadSafe> ParentFileHandle;
};

struct RHI_API FPipelineCachePSOHeader
{
	TSet<FSHAHash> Shaders;
	uint32 Hash;
};

extern RHI_API const uint32 FPipelineCacheFileFormatCurrentVersion;

/**
 * FPipelineFileCache:
 * The RHI-level backend for FShaderPipelineCache, responsible for tracking PSOs and their usage stats as well as dealing with the pipeline cache files.
 * It is not expected that games or end-users invoke this directly, they should be calling FShaderPipelineCache which exposes this functionality in a usable form. 
 */
class RHI_API FPipelineFileCache
{
    friend class FPipelineCacheFile;
public:
	enum class SaveMode : uint32
	{
		Incremental = 0, // Fast(er) approach which saves new entries incrementally at the end of the file, replacing the table-of-contents, but leaves everything else alone.
        BoundPSOsOnly = 1, // Slower approach which consolidates and saves all PSOs used in this run of the program, removing any entry that wasn't seen, and sorted by the desired sort-mode.
		SortedBoundPSOs = 2 // Slow save consolidates all PSOs used on this device that were never part of a cache file delivered in game-content, sorts entries into the desired order and will thus read-back from disk.
	};
	
	enum class PSOOrder : uint32
	{
		Default = 0, // Whatever order they are already in.
		FirstToLatestUsed = 1, // Start with the PSOs with the lowest first-frame used and work toward those with the highest.
		MostToLeastUsed = 2 // Start with the most often used PSOs working toward the least.
	};

public:
	
	static void Initialize(uint32 GameVersion);
	static void Shutdown();
	
	static bool LoadPipelineFileCacheInto(FString const& Path, TSet<FPipelineCacheFileFormatPSO>& PSOs);
	static bool SavePipelineFileCacheFrom(uint32 GameVersion, EShaderPlatform Platform, FString const& Path, const TSet<FPipelineCacheFileFormatPSO>& PSOs);
	static bool MergePipelineFileCaches(FString const& PathA, FString const& PathB, FPipelineFileCache::PSOOrder Order, FString const& OutputPath);

	static bool OpenPipelineFileCache(FString const& Name, EShaderPlatform Platform);
	static bool SavePipelineFileCache(FString const& Name, SaveMode Mode);
	static void ClosePipelineFileCache();
	
	static void CacheGraphicsPSO(uint32 RunTimeHash, FGraphicsPipelineStateInitializer const& Initializer);
	static void CacheComputePSO(uint32 RunTimeHash, FRHIComputeShader const* Initializer);
	static FPipelineStateStats* RegisterPSOStats(uint32 RunTimeHash);
	
	/**
	 * Event signature for being notified that a new PSO has been logged
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPipelineStateLoggedEvent, FPipelineCacheFileFormatPSO&);

	/**
	 * Gets the event delegate to register for pipeline state logging events.
	 */
	static FPipelineStateLoggedEvent& OnPipelineStateLogged();
	
	static void GetOrderedPSOHashes(TArray<FPipelineCachePSOHeader>& PSOHashes, PSOOrder Order);
	static void FetchPSODescriptors(TArray<FPipelineCacheFileFormatPSORead*>& LoadedBatch);
	static uint32 NumPSOsLogged();
	
	static bool IsPipelineFileCacheEnabled();
	static bool LogPSOtoFileCache();
    static bool ReportNewPSOs();
	
private:
	static FRWLock FileCacheLock;
	static class FPipelineCacheFile* FileCache;
	static TMap<uint32, uint32> RunTimeToFileHashes;
	static TMap<uint32, FPipelineStateStats*> Stats;
	static TSet<FPipelineCacheFileFormatPSO> NewPSOs;
    static uint32 NumNewPSOs;
	static PSOOrder RequestedOrder;
	static bool FileCacheEnabled;
	static FPipelineStateLoggedEvent PSOLoggedEvent;
};

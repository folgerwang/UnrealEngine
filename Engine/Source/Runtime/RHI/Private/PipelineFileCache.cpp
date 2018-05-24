// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PipelineFileCache.cpp: Pipeline state cache implementation.
=============================================================================*/

#include "PipelineFileCache.h"
#include "PipelineStateCache.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersion.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/Paths.h"
#include "Async/AsyncFileHandle.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"

// Loaded + New created
#if STATS // If STATS are not enabled RHI_API will DLLEXPORT on an empty line
RHI_API DEFINE_STAT(STAT_TotalGraphicsPipelineStateCount);
RHI_API DEFINE_STAT(STAT_TotalComputePipelineStateCount);
#endif

// New Saved count
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Serialized Graphics Pipeline State Count"), STAT_SerializedGraphicsPipelineStateCount, STATGROUP_PipelineStateCache );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Serialized Compute Pipeline State Count"), STAT_SerializedComputePipelineStateCount, STATGROUP_PipelineStateCache );

// New created - Cache Miss count
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("New Graphics Pipeline State Count"), STAT_NewGraphicsPipelineStateCount, STATGROUP_PipelineStateCache );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("New Compute Pipeline State Count"), STAT_NewComputePipelineStateCount, STATGROUP_PipelineStateCache );

// Memory - Only track the file representation and new state cache stats
DECLARE_MEMORY_STAT(TEXT("New Cached PSO"), STAT_NewCachedPSOMemory, STATGROUP_PipelineStateCache);
DECLARE_MEMORY_STAT(TEXT("PSO Stat"), STAT_PSOStatMemory, STATGROUP_PipelineStateCache);
DECLARE_MEMORY_STAT(TEXT("File Cache"), STAT_FileCacheMemory, STATGROUP_PipelineStateCache);


enum class EPipelineCacheFileFormatVersions : uint32
{
    FirstWorking = 7,
    LibraryID = 9,
    ShaderMetaData = 10,
	SortedVertexDesc = 11
};

const uint64 FPipelineCacheFileFormatMagic = 0x5049504543414348; // PIPECACH
const uint32 FPipelineCacheFileFormatCurrentVersion = (uint32)EPipelineCacheFileFormatVersions::SortedVertexDesc;

/**
  * PipelineFileCache API access
  **/

static TAutoConsoleVariable<int32> CVarPSOFileCacheEnabled(
														   TEXT("r.ShaderPipelineCache.Enabled"),
														   PIPELINE_CACHE_DEFAULT_ENABLED,
														   TEXT("1 Enables the PipelineFileCache, 0 disables it."),
														   ECVF_Default | ECVF_RenderThreadSafe
														   );

static TAutoConsoleVariable<int32> CVarPSOFileCacheLogPSO(
														   TEXT("r.ShaderPipelineCache.LogPSO"),
														   PIPELINE_CACHE_DEFAULT_ENABLED,
														   TEXT("1 Logs new PSO entries into the file cache and allow saving, 0 uses existing PSO file cache in readonly mode (if enabled)."),
														   ECVF_Default | ECVF_RenderThreadSafe
														   );

static TAutoConsoleVariable<int32> CVarPSOFileCacheReportPSO(
														   TEXT("r.ShaderPipelineCache.ReportPSO"),
														   PIPELINE_CACHE_DEFAULT_ENABLED,
														   TEXT("1 reports new PSO entries via a delegate, but does not record or modify any cache file."),
														   ECVF_Default | ECVF_RenderThreadSafe
														   );

static int32 GPSOFileCachePrintNewPSODescriptors = !UE_BUILD_SHIPPING;
static FAutoConsoleVariableRef CVarPSOFileCachePrintNewPSODescriptors(
														   TEXT("r.ShaderPipelineCache.PrintNewPSODescriptors"),
														   GPSOFileCachePrintNewPSODescriptors,
														   TEXT("1 prints descriptions for all new PSO entries to the log/console while 0 does not. Defaults to 0 in *Shipping* builds, otherwise 1."),
														   ECVF_Default
														   );

static TAutoConsoleVariable<int32> CVarPSOFileCacheSaveUserCache(
                                                            TEXT("r.ShaderPipelineCache.SaveUserCache"),
#if PIPELINE_CACHE_DEFAULT_ENABLED
															(int32)1,
#else
															(int32)0,
#endif
                                                            TEXT("If > 0 then any missed PSOs will be saved to a writable user cache file for subsequent runs to load and avoid in-game hitches. Enabled by default on macOS only."),
                                                            ECVF_Default | ECVF_RenderThreadSafe
                                                            );

FRWLock FPipelineFileCache::FileCacheLock;
FPipelineCacheFile* FPipelineFileCache::FileCache = nullptr;
TMap<uint32, uint32> FPipelineFileCache::RunTimeToFileHashes;
TMap<uint32, FPipelineStateStats*> FPipelineFileCache::Stats;
TSet<FPipelineCacheFileFormatPSO> FPipelineFileCache::NewPSOs;
uint32 FPipelineFileCache::NumNewPSOs;
FPipelineFileCache::PSOOrder FPipelineFileCache::RequestedOrder = FPipelineFileCache::PSOOrder::Default;
bool FPipelineFileCache::FileCacheEnabled = false;
FPipelineFileCache::FPipelineStateLoggedEvent FPipelineFileCache::PSOLoggedEvent;


void FRHIComputeShader::UpdateStats()
{
	FPipelineStateStats::UpdateStats(Stats);
}

void FPipelineStateStats::UpdateStats(FPipelineStateStats* Stats)
{
	if (Stats)
	{
		FPlatformAtomics::InterlockedExchange(&Stats->LastFrameUsed, GFrameCounter);
		FPlatformAtomics::InterlockedIncrement(&Stats->TotalBindCount);
		FPlatformAtomics::InterlockedCompareExchange(&Stats->FirstFrameUsed, GFrameCounter, -1);
	}
}

struct FPipelineCacheFileFormatHeader
{
	uint64 Magic;			// Sanity check
	uint32 Version; 		// File version must match engine version, otherwise we ignore
	uint32 GameVersion; 	// Same as above but game specific code can invalidate
	TEnumAsByte<EShaderPlatform> Platform; // The shader platform for all referenced PSOs.
	FGuid Guid;				// Guid to identify the file uniquely
	uint64 TableOffset;		// absolute file offset to TOC
	
	friend FArchive& operator<<(FArchive& Ar, FPipelineCacheFileFormatHeader& Info)
	{
		Ar << Info.Magic;
		Ar << Info.Version;
		Ar << Info.GameVersion;
		Ar << Info.Platform;
		Ar << Info.Guid;
		Ar << Info.TableOffset;
		
		return Ar;
	}
};

FArchive& operator<<( FArchive& Ar, FPipelineStateStats& Info )
{
	Ar << Info.FirstFrameUsed;
	Ar << Info.LastFrameUsed;
	Ar << Info.CreateCount;
	Ar << Info.TotalBindCount;
	Ar << Info.PSOHash;
	
	return Ar;
}

struct FPipelineCacheFileFormatPSOMetaData
{
	FPipelineCacheFileFormatPSOMetaData()
	: FileOffset(0)
	{
	}
	
	~FPipelineCacheFileFormatPSOMetaData()
	{
	}

	uint64 FileOffset;
	uint64 FileSize;
	FGuid FileGuid;
	FPipelineStateStats Stats;
	TSet<FSHAHash> Shaders;
	
	friend FArchive& operator<<(FArchive& Ar, FPipelineCacheFileFormatPSOMetaData& Info)
	{
		Ar << Info.FileOffset;
		Ar << Info.FileSize;
		Ar << Info.FileGuid;
		Ar << Info.Stats;
        if (Ar.GameNetVer() == (uint32)EPipelineCacheFileFormatVersions::LibraryID)
        {
            TSet<uint32> IDs;
            Ar << IDs;
        }
        else if (Ar.GameNetVer() >= (uint32)EPipelineCacheFileFormatVersions::ShaderMetaData)
        {
            Ar << Info.Shaders;
        }
		
		return Ar;
	}
};


FString FPipelineFileCacheRasterizerState::ToString() const
{
	return FString::Printf(TEXT("<%f %f %u %u %u %u>")
		, DepthBias
		, SlopeScaleDepthBias
		, uint32(FillMode)
		, uint32(CullMode)
		, uint32(!!bAllowMSAA)
		, uint32(!!bEnableLineAA)
	);
}

void FPipelineFileCacheRasterizerState::FromString(const FString& InSrc)
{
	FString Src = InSrc;
	Src.ReplaceInline(TEXT("\r"), TEXT(" "));
	Src.ReplaceInline(TEXT("\n"), TEXT(" "));
	Src.ReplaceInline(TEXT("\t"), TEXT(" "));
	Src.ReplaceInline(TEXT("<"), TEXT(" "));
	Src.ReplaceInline(TEXT(">"), TEXT(" "));
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(" "));

	check(Parts.Num() == 6 && sizeof(FillMode) == 1 && sizeof(CullMode) == 1 && sizeof(bAllowMSAA) == 1 && sizeof(bEnableLineAA) == 1); //not a very robust parser
	LexFromString(DepthBias, *Parts[0]);
	LexFromString(SlopeScaleDepthBias, *Parts[1]);
	LexFromString((uint8&)FillMode, *Parts[2]);
	LexFromString((uint8&)CullMode, *Parts[3]);
	LexFromString((uint8&)bAllowMSAA, *Parts[4]);
	LexFromString((uint8&)bEnableLineAA, *Parts[5]);
}

FString FPipelineCacheFileFormatPSO::ComputeDescriptor::ToString() const
{
	return ComputeShader.ToString();
}
void FPipelineCacheFileFormatPSO::ComputeDescriptor::FromString(const FString& Src)
{
	ComputeShader.FromString(Src.TrimStartAndEnd());
}

FString FPipelineCacheFileFormatPSO::ComputeDescriptor::HeaderLine()
{
	return FString(TEXT("ComputeShader"));
}

FString FPipelineCacheFileFormatPSO::GraphicsDescriptor::ShadersToString() const
{
	FString Result;

	Result += FString::Printf(TEXT("%s,%s,%s,%s,%s")
		, *VertexShader.ToString()
		, *FragmentShader.ToString()
		, *GeometryShader.ToString()
		, *HullShader.ToString()
		, *DomainShader.ToString()
	);

	return Result;
}
void FPipelineCacheFileFormatPSO::GraphicsDescriptor::ShadersFromString(const FString& Src)
{
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(","));
	int32 PartIndex = 0;
	check(Parts.Num() == 5); //not a very robust parser

	VertexShader.FromString(Parts[PartIndex++]);
	FragmentShader.FromString(Parts[PartIndex++]);
	GeometryShader.FromString(Parts[PartIndex++]);
	HullShader.FromString(Parts[PartIndex++]);
	DomainShader.FromString(Parts[PartIndex++]);
}

FString FPipelineCacheFileFormatPSO::GraphicsDescriptor::ShaderHeaderLine()
{
	return FString(TEXT("VertexShader,FragmentShader,GeometryShader,HullShader,DomainShader"));
}

FString FPipelineCacheFileFormatPSO::GraphicsDescriptor::StateToString() const
{
	FString Result;

	Result += FString::Printf(TEXT("%s,%s,%s,")
		, *BlendState.ToString()
		, *RasterizerState.ToString()
		, *DepthStencilState.ToString()
	);
	Result += FString::Printf(TEXT("%d,%d,%d,")
		, MSAASamples
		, uint32(DepthStencilFormat)
		, DepthStencilFlags
	);
	Result += FString::Printf(TEXT("%d,%d,%d,%d,%d,")
		, uint32(DepthLoad)
		, uint32(StencilLoad)
		, uint32(DepthStore)
		, uint32(StencilStore)
		, uint32(PrimitiveType)
	);

	Result += FString::Printf(TEXT("%d,")
		, RenderTargetsActive
	);
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		Result += FString::Printf(TEXT("%d,%d,%d,%d,")
			, uint32(RenderTargetFormats[Index])
			, RenderTargetFlags[Index]
			, uint32(RenderTargetsLoad[Index])
			, uint32(RenderTargetsStore[Index])
		);
	}

	FVertexElement NullVE;
	FMemory::Memzero(NullVE);
	Result += FString::Printf(TEXT("%d,")
		, VertexDescriptor.Num()
	);
	for (int32 Index = 0; Index < MaxVertexElementCount; Index++)
	{
		if (Index < VertexDescriptor.Num())
		{
			Result += FString::Printf(TEXT("%s,")
				, *VertexDescriptor[Index].ToString()
			);
		}
		else
		{
			Result += FString::Printf(TEXT("%s,")
				, *NullVE.ToString()
			);
		}
	}
	return Result.Left(Result.Len() - 1); // remove trailing comma
}
void FPipelineCacheFileFormatPSO::GraphicsDescriptor::StateFromString(const FString& Src)
{
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(","));
	int32 PartIndex = 0;




	check(Parts.Num() - PartIndex >= 3); //not a very robust parser
	BlendState.FromString(Parts[PartIndex++]);
	RasterizerState.FromString(Parts[PartIndex++]);
	DepthStencilState.FromString(Parts[PartIndex++]);

	check(Parts.Num() - PartIndex >= 3 && sizeof(EPixelFormat) == sizeof(uint32)); //not a very robust parser
	LexFromString(MSAASamples, *Parts[PartIndex++]);
	LexFromString((uint32&)DepthStencilFormat, *Parts[PartIndex++]);
	LexFromString(DepthStencilFlags, *Parts[PartIndex++]);

	check(Parts.Num() - PartIndex >= 5 && sizeof(DepthLoad) == 1 && sizeof(StencilLoad) == 1 && sizeof(DepthStore) == 1 && sizeof(StencilStore) == 1 && sizeof(PrimitiveType) == 4); //not a very robust parser
	LexFromString((uint32&)DepthLoad, *Parts[PartIndex++]);
	LexFromString((uint32&)StencilLoad, *Parts[PartIndex++]);
	LexFromString((uint32&)DepthStore, *Parts[PartIndex++]);
	LexFromString((uint32&)StencilStore, *Parts[PartIndex++]);
	LexFromString((uint32&)PrimitiveType, *Parts[PartIndex++]);

	check(Parts.Num() - PartIndex >= 1); //not a very robust parser
	LexFromString(RenderTargetsActive, *Parts[PartIndex++]);

	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		check(Parts.Num() - PartIndex >= 4 && sizeof(ERenderTargetLoadAction) == 1 && sizeof(ERenderTargetStoreAction) == 1 && sizeof(EPixelFormat) == sizeof(uint32)); //not a very robust parser
		LexFromString((uint32&)(RenderTargetFormats[Index]), *Parts[PartIndex++]);
		LexFromString(RenderTargetFlags[Index], *Parts[PartIndex++]);
		LexFromString((uint8&)RenderTargetsLoad[Index], *Parts[PartIndex++]);
		LexFromString((uint8&)RenderTargetsStore[Index], *Parts[PartIndex++]);
	}

	check(Parts.Num() - PartIndex >= 1); //not a very robust parser
	int32 VertDescNum = 0;
	LexFromString(VertDescNum, *Parts[PartIndex++]);
	check(VertDescNum >= 0 && VertDescNum <= MaxVertexElementCount);

	VertexDescriptor.Empty(VertDescNum);
	VertexDescriptor.AddZeroed(VertDescNum);

	check(Parts.Num() - PartIndex == MaxVertexElementCount); //not a very robust parser
	for (int32 Index = 0; Index < VertDescNum; Index++)
	{
		VertexDescriptor[Index].FromString(Parts[PartIndex++]);
	}
	
	VertexDescriptor.Sort([](FVertexElement const& A, FVertexElement const& B)
	  {
		  if (A.StreamIndex < B.StreamIndex)
		  {
			  return true;
		  }
		  if (A.StreamIndex > B.StreamIndex)
		  {
			  return false;
		  }
		  if (A.Offset < B.Offset)
		  {
			  return true;
		  }
		  if (A.Offset > B.Offset)
		  {
			  return false;
		  }
		  if (A.AttributeIndex < B.AttributeIndex)
		  {
			  return true;
		  }
		  if (A.AttributeIndex > B.AttributeIndex)
		  {
			  return false;
		  }
		  return false;
	  });
}

FString FPipelineCacheFileFormatPSO::GraphicsDescriptor::StateHeaderLine()
{
	FString Result;

	Result += FString::Printf(TEXT("%s,%s,%s,")
		, TEXT("BlendState")
		, TEXT("RasterizerState")
		, TEXT("DepthStencilState")
	);
	Result += FString::Printf(TEXT("%s,%s,%s,")
		, TEXT("MSAASamples")
		, TEXT("DepthStencilFormat")
		, TEXT("DepthStencilFlags")
	);
	Result += FString::Printf(TEXT("%s,%s,%s,%s,%s,")
		, TEXT("DepthLoad")
		, TEXT("StencilLoad")
		, TEXT("DepthStore")
		, TEXT("StencilStore")
		, TEXT("PrimitiveType")
	);

	Result += FString::Printf(TEXT("%s,")
		, TEXT("RenderTargetsActive")
	);
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		Result += FString::Printf(TEXT("%s%d,%s%d,%s%d,%s%d,")
			, TEXT("RenderTargetFormats"), Index
			, TEXT("RenderTargetFlags"), Index
			, TEXT("RenderTargetsLoad"), Index
			, TEXT("RenderTargetsStore"), Index
		);
	}

	Result += FString::Printf(TEXT("%s,")
		, TEXT("VertexDescriptorNum")
	);
	for (int32 Index = 0; Index < MaxVertexElementCount; Index++)
	{
		Result += FString::Printf(TEXT("%s%d,")
			, TEXT("VertexDescriptor"), Index
		);
	}
	return Result.Left(Result.Len() - 1); // remove trailing comma
}

FString FPipelineCacheFileFormatPSO::GraphicsDescriptor::ToString() const
{
	return FString::Printf(TEXT("%s,%s"), *ShadersToString(), *StateToString());
}

void FPipelineCacheFileFormatPSO::GraphicsDescriptor::FromString(const FString& Src)
{
	static const int32 NumShaderParts = 5;
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(","));
	check(Parts.Num() > NumShaderParts);
	TArray<FString> StateParts(Parts);
	StateParts.RemoveAt(0, NumShaderParts);
	Parts.RemoveAt(NumShaderParts, Parts.Num() - 5);
	ShadersFromString(FString::Join(Parts, TEXT(",")));
	StateFromString(FString::Join(StateParts, TEXT(",")));
}

FString FPipelineCacheFileFormatPSO::GraphicsDescriptor::HeaderLine()
{
	return FString::Printf(TEXT("%s,%s"), *ShaderHeaderLine(), *StateHeaderLine());
}



/**
  * FPipelineCacheFileFormatPSO
  **/

/*friend*/ uint32 GetTypeHash(const FPipelineCacheFileFormatPSO &Key)
{
	if(FPlatformAtomics::AtomicRead((volatile int32*)&Key.Hash) == 0)
	{
		uint32 KeyHash = GetTypeHash(Key.Type);
		switch(Key.Type)
		{
			case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
			{
				KeyHash ^= GetTypeHash(Key.ComputeDesc.ComputeShader);
				break;
			}
			case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
			{
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RenderTargetsActive, sizeof(Key.GraphicsDesc.RenderTargetsActive), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.MSAASamples, sizeof(Key.GraphicsDesc.MSAASamples), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.PrimitiveType, sizeof(Key.GraphicsDesc.PrimitiveType), KeyHash);
				
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.VertexShader.Hash, sizeof(Key.GraphicsDesc.VertexShader.Hash), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.FragmentShader.Hash, sizeof(Key.GraphicsDesc.FragmentShader.Hash), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.GeometryShader.Hash, sizeof(Key.GraphicsDesc.GeometryShader.Hash), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.HullShader.Hash, sizeof(Key.GraphicsDesc.HullShader.Hash), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DomainShader.Hash, sizeof(Key.GraphicsDesc.DomainShader.Hash), KeyHash);
				
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilFormat, sizeof(Key.GraphicsDesc.DepthStencilFormat), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilFlags, sizeof(Key.GraphicsDesc.DepthStencilFlags), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthLoad, sizeof(Key.GraphicsDesc.DepthLoad), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.StencilLoad, sizeof(Key.GraphicsDesc.StencilLoad), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStore, sizeof(Key.GraphicsDesc.DepthStore), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.StencilStore, sizeof(Key.GraphicsDesc.StencilStore), KeyHash);
				
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.bUseIndependentRenderTargetBlendStates, sizeof(Key.GraphicsDesc.BlendState.bUseIndependentRenderTargetBlendStates), KeyHash);
				for( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
				{
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].ColorBlendOp, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].ColorBlendOp), KeyHash);
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].ColorSrcBlend, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].ColorSrcBlend), KeyHash);
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].ColorDestBlend, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].ColorDestBlend), KeyHash);
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].ColorWriteMask, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].ColorWriteMask), KeyHash);
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].AlphaBlendOp, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].AlphaBlendOp), KeyHash);
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].AlphaSrcBlend, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].AlphaSrcBlend), KeyHash);
					KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.BlendState.RenderTargets[i].AlphaDestBlend, sizeof(Key.GraphicsDesc.BlendState.RenderTargets[i].AlphaDestBlend), KeyHash);
				}

				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RenderTargetFormats, sizeof(Key.GraphicsDesc.RenderTargetFormats), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RenderTargetFlags, sizeof(Key.GraphicsDesc.RenderTargetFlags), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RenderTargetsLoad, sizeof(Key.GraphicsDesc.RenderTargetsLoad), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RenderTargetsStore, sizeof(Key.GraphicsDesc.RenderTargetsStore), KeyHash);

				for(auto const& Element : Key.GraphicsDesc.VertexDescriptor)
				{
					KeyHash = FCrc::MemCrc32(&Element, sizeof(FVertexElement), KeyHash);
				}
				
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RasterizerState.DepthBias, sizeof(Key.GraphicsDesc.RasterizerState.DepthBias), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RasterizerState.SlopeScaleDepthBias, sizeof(Key.GraphicsDesc.RasterizerState.SlopeScaleDepthBias), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RasterizerState.FillMode, sizeof(Key.GraphicsDesc.RasterizerState.FillMode), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RasterizerState.CullMode, sizeof(Key.GraphicsDesc.RasterizerState.CullMode), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RasterizerState.bAllowMSAA, sizeof(Key.GraphicsDesc.RasterizerState.bAllowMSAA), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.RasterizerState.bEnableLineAA, sizeof(Key.GraphicsDesc.RasterizerState.bEnableLineAA), KeyHash);
				
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.bEnableDepthWrite, sizeof(Key.GraphicsDesc.DepthStencilState.bEnableDepthWrite), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.DepthTest, sizeof(Key.GraphicsDesc.DepthStencilState.DepthTest), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.bEnableFrontFaceStencil, sizeof(Key.GraphicsDesc.DepthStencilState.bEnableFrontFaceStencil), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.FrontFaceStencilTest, sizeof(Key.GraphicsDesc.DepthStencilState.FrontFaceStencilTest), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.FrontFaceStencilFailStencilOp, sizeof(Key.GraphicsDesc.DepthStencilState.FrontFaceStencilFailStencilOp), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.FrontFaceDepthFailStencilOp, sizeof(Key.GraphicsDesc.DepthStencilState.FrontFaceDepthFailStencilOp), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.FrontFacePassStencilOp, sizeof(Key.GraphicsDesc.DepthStencilState.FrontFacePassStencilOp), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.bEnableBackFaceStencil, sizeof(Key.GraphicsDesc.DepthStencilState.bEnableBackFaceStencil), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.BackFaceStencilTest, sizeof(Key.GraphicsDesc.DepthStencilState.BackFaceStencilTest), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.BackFaceStencilFailStencilOp, sizeof(Key.GraphicsDesc.DepthStencilState.BackFaceStencilFailStencilOp), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.BackFaceDepthFailStencilOp, sizeof(Key.GraphicsDesc.DepthStencilState.BackFaceDepthFailStencilOp), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.BackFacePassStencilOp, sizeof(Key.GraphicsDesc.DepthStencilState.BackFacePassStencilOp), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.StencilReadMask, sizeof(Key.GraphicsDesc.DepthStencilState.StencilReadMask), KeyHash);
				KeyHash = FCrc::MemCrc32(&Key.GraphicsDesc.DepthStencilState.StencilWriteMask, sizeof(Key.GraphicsDesc.DepthStencilState.StencilWriteMask), KeyHash);
				
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
		FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Key.Hash, KeyHash, 0);
	}
	return FPlatformAtomics::AtomicRead((volatile int32*)&Key.Hash);
}

/*friend*/ FArchive& operator<<( FArchive& Ar, FPipelineCacheFileFormatPSO& Info )
{
	Ar << Info.Type;
	switch (Info.Type)
	{
		case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
		{
			Ar << Info.ComputeDesc.ComputeShader;
            if (Ar.GameNetVer() == (uint32)EPipelineCacheFileFormatVersions::LibraryID)
            {
                uint32 ID = 0;
                Ar << ID;
            }
			break;
		}
		case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
		{
			Ar << Info.GraphicsDesc.VertexShader;
			Ar << Info.GraphicsDesc.FragmentShader;
			Ar << Info.GraphicsDesc.GeometryShader;
			Ar << Info.GraphicsDesc.HullShader;
			Ar << Info.GraphicsDesc.DomainShader;
            if (Ar.GameNetVer() == (uint32)EPipelineCacheFileFormatVersions::LibraryID)
            {
				for (uint32 i = 0; i < SF_Compute; i++)
				{
                    uint32 ID = 0;
                    Ar << ID;
                }
			}
			if (Ar.GameNetVer() < (uint32)EPipelineCacheFileFormatVersions::SortedVertexDesc)
			{
				check(Ar.IsLoading());
				
				FVertexDeclarationElementList Elements;
				Ar << Elements;
				Elements.Sort([](FVertexElement const& A, FVertexElement const& B)
							  {
								  if (A.StreamIndex < B.StreamIndex)
								  {
									  return true;
								  }
								  if (A.StreamIndex > B.StreamIndex)
								  {
									  return false;
								  }
								  if (A.Offset < B.Offset)
								  {
									  return true;
								  }
								  if (A.Offset > B.Offset)
								  {
									  return false;
								  }
								  if (A.AttributeIndex < B.AttributeIndex)
								  {
									  return true;
								  }
								  if (A.AttributeIndex > B.AttributeIndex)
								  {
									  return false;
								  }
								  return false;
							  });
				
				Info.GraphicsDesc.VertexDescriptor.AddZeroed(Elements.Num());
				for (uint32 i = 0; i < (uint32)Elements.Num(); i++)
				{
					Info.GraphicsDesc.VertexDescriptor[i].StreamIndex = Elements[i].StreamIndex;
					Info.GraphicsDesc.VertexDescriptor[i].Offset = Elements[i].Offset;
					Info.GraphicsDesc.VertexDescriptor[i].Type = Elements[i].Type;
					Info.GraphicsDesc.VertexDescriptor[i].AttributeIndex = Elements[i].AttributeIndex;
					Info.GraphicsDesc.VertexDescriptor[i].Stride = Elements[i].Stride;
					Info.GraphicsDesc.VertexDescriptor[i].bUseInstanceIndex = Elements[i].bUseInstanceIndex;
				}
			}
			else
			{
				Ar << Info.GraphicsDesc.VertexDescriptor;
			}
			Ar << Info.GraphicsDesc.BlendState;
			Ar << Info.GraphicsDesc.RasterizerState;
			Ar << Info.GraphicsDesc.DepthStencilState;
			for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
			{
				uint32 Format = (uint32)Info.GraphicsDesc.RenderTargetFormats[i];
				Ar << Format;
				Info.GraphicsDesc.RenderTargetFormats[i] = (EPixelFormat)Format;
				Ar << Info.GraphicsDesc.RenderTargetFlags[i];
				Ar << Info.GraphicsDesc.RenderTargetsLoad[i];
				Ar << Info.GraphicsDesc.RenderTargetsStore[i];
			}
			Ar << Info.GraphicsDesc.RenderTargetsActive;
			Ar << Info.GraphicsDesc.MSAASamples;
			uint32 PrimType = (uint32)Info.GraphicsDesc.PrimitiveType;
			Ar << PrimType;
			Info.GraphicsDesc.PrimitiveType = (EPrimitiveType)PrimType;
			uint32 Format = (uint32)Info.GraphicsDesc.DepthStencilFormat;
			Ar << Format;
			Info.GraphicsDesc.DepthStencilFormat = (EPixelFormat)Format;
			Ar << Info.GraphicsDesc.DepthStencilFlags;
			Ar << Info.GraphicsDesc.DepthLoad;
			Ar << Info.GraphicsDesc.StencilLoad;
			Ar << Info.GraphicsDesc.DepthStore;
			Ar << Info.GraphicsDesc.StencilStore;
			break;
		}
		default:
		{
			check(false);
			break;
		}
	}
	return Ar;
}

FPipelineCacheFileFormatPSO::FPipelineCacheFileFormatPSO()
: Hash(0)
{
}

/*static*/ bool FPipelineCacheFileFormatPSO::Init(FPipelineCacheFileFormatPSO& PSO, FRHIComputeShader const* Init)
{
	check(Init);

	PSO.Hash = 0;
	PSO.Type = DescriptorType::Compute;
	
	// Because of the cheat in the copy constructor - lets play this safe
	FMemory::Memset(&PSO.ComputeDesc, 0, sizeof(ComputeDescriptor));
	
	PSO.ComputeDesc.ComputeShader = Init->GetHash();
	
	return true;
}

/*static*/ bool FPipelineCacheFileFormatPSO::Init(FPipelineCacheFileFormatPSO& PSO, FGraphicsPipelineStateInitializer const& Init)
{
	bool bOK = true;
	
	PSO.Hash = 0;
	PSO.Type = DescriptorType::Graphics;
	
	// Because of the cheat in the copy constructor - lets play this safe
	FMemory::Memset(&PSO.GraphicsDesc, 0, sizeof(GraphicsDescriptor));
	
	check (Init.BoundShaderState.VertexDeclarationRHI);
	{
		bOK &= Init.BoundShaderState.VertexDeclarationRHI->GetInitializer(PSO.GraphicsDesc.VertexDescriptor);
		check(bOK);
		
		PSO.GraphicsDesc.VertexDescriptor.Sort([](FVertexElement const& A, FVertexElement const& B)
		{
			if (A.StreamIndex < B.StreamIndex)
			{
				return true;
			}
			if (A.StreamIndex > B.StreamIndex)
			{
				return false;
			}
			if (A.Offset < B.Offset)
			{
				return true;
			}
			if (A.Offset > B.Offset)
			{
				return false;
			}
			if (A.AttributeIndex < B.AttributeIndex)
			{
				return true;
			}
			if (A.AttributeIndex > B.AttributeIndex)
			{
				return false;
			}
			return false;
		});
	}
	
	if (Init.BoundShaderState.VertexShaderRHI)
	{
		PSO.GraphicsDesc.VertexShader = Init.BoundShaderState.VertexShaderRHI->GetHash();
	}
	
	if (Init.BoundShaderState.HullShaderRHI)
	{
		PSO.GraphicsDesc.HullShader = Init.BoundShaderState.HullShaderRHI->GetHash();
	}
	
	if (Init.BoundShaderState.DomainShaderRHI)
	{
		PSO.GraphicsDesc.DomainShader = Init.BoundShaderState.DomainShaderRHI->GetHash();
	}
	
	if (Init.BoundShaderState.PixelShaderRHI)
	{
		PSO.GraphicsDesc.FragmentShader = Init.BoundShaderState.PixelShaderRHI->GetHash();
	}
	
	if (Init.BoundShaderState.GeometryShaderRHI)
	{
		PSO.GraphicsDesc.GeometryShader = Init.BoundShaderState.GeometryShaderRHI->GetHash();
	}
	
	check (Init.BlendState);
	{
		bOK &= Init.BlendState->GetInitializer(PSO.GraphicsDesc.BlendState);
		check(bOK);
	}
	
	check (Init.RasterizerState);
	{
		FRasterizerStateInitializerRHI Temp;
		bOK &= Init.RasterizerState->GetInitializer(Temp);
		check(bOK);
		
		PSO.GraphicsDesc.RasterizerState = Temp;
	}
	
	check (Init.DepthStencilState);
	{
		bOK &= Init.DepthStencilState->GetInitializer(PSO.GraphicsDesc.DepthStencilState);
		check(bOK);
	}
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		PSO.GraphicsDesc.RenderTargetFormats[i] = Init.RenderTargetFormats[i];
		PSO.GraphicsDesc.RenderTargetFlags[i] = Init.RenderTargetFlags[i];
		PSO.GraphicsDesc.RenderTargetsLoad[i] = Init.RenderTargetLoadActions[i];
		PSO.GraphicsDesc.RenderTargetsStore[i] = Init.RenderTargetStoreActions[i];
	}
	
	PSO.GraphicsDesc.RenderTargetsActive = Init.RenderTargetsEnabled;
	PSO.GraphicsDesc.MSAASamples = Init.NumSamples;
	
	PSO.GraphicsDesc.DepthStencilFormat = Init.DepthStencilTargetFormat;
	PSO.GraphicsDesc.DepthStencilFlags = Init.DepthStencilTargetFlag;
	PSO.GraphicsDesc.DepthLoad = Init.DepthTargetLoadAction;
	PSO.GraphicsDesc.StencilLoad = Init.StencilTargetLoadAction;
	PSO.GraphicsDesc.DepthStore = Init.DepthTargetStoreAction;
	PSO.GraphicsDesc.StencilStore = Init.StencilTargetStoreAction;
	
	PSO.GraphicsDesc.PrimitiveType = Init.PrimitiveType;
	
	return bOK;
}

FPipelineCacheFileFormatPSO::~FPipelineCacheFileFormatPSO()
{
	
}

bool FPipelineCacheFileFormatPSO::operator==(const FPipelineCacheFileFormatPSO& Other) const
{
	bool bSame = true;
	if (this != &Other)
	{
		bSame = false;
		//if (Type == Other.Type && GetTypeHash(*this) == GetTypeHash(Other))
		{
			switch(Type)
			{
				case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
				{
					// If we implement a classic copy constructor without memcpy - remove memset in ::Init() function above
					bSame = (FMemory::Memcmp(&ComputeDesc, &Other.ComputeDesc, sizeof(ComputeDescriptor)) == 0);
					break;
				}
				case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
				{
					// If we implement a classic copy constructor without memcpy - remove memset in ::Init() function above
					
					bSame = GraphicsDesc.VertexDescriptor.Num() == Other.GraphicsDesc.VertexDescriptor.Num();
					for (uint32 i = 0; i < (uint32)FMath::Min(GraphicsDesc.VertexDescriptor.Num(), Other.GraphicsDesc.VertexDescriptor.Num()); i++)
					{
						bSame &= (FMemory::Memcmp(&GraphicsDesc.VertexDescriptor[i], &Other.GraphicsDesc.VertexDescriptor[i], sizeof(FVertexElement)) == 0);
					}
					bSame &= GraphicsDesc.PrimitiveType == Other.GraphicsDesc.PrimitiveType && GraphicsDesc.VertexShader == Other.GraphicsDesc.VertexShader && GraphicsDesc.FragmentShader == Other.GraphicsDesc.FragmentShader && GraphicsDesc.GeometryShader == Other.GraphicsDesc.GeometryShader && GraphicsDesc.HullShader == Other.GraphicsDesc.HullShader && GraphicsDesc.DomainShader == Other.GraphicsDesc.DomainShader && GraphicsDesc.RenderTargetsActive == Other.GraphicsDesc.RenderTargetsActive &&
					GraphicsDesc.MSAASamples == Other.GraphicsDesc.MSAASamples && GraphicsDesc.DepthStencilFormat == Other.GraphicsDesc.DepthStencilFormat &&
					GraphicsDesc.DepthStencilFlags == Other.GraphicsDesc.DepthStencilFlags && GraphicsDesc.DepthLoad == Other.GraphicsDesc.DepthLoad &&
					GraphicsDesc.DepthStore == Other.GraphicsDesc.DepthStore && GraphicsDesc.StencilLoad == Other.GraphicsDesc.StencilLoad && GraphicsDesc.StencilStore == Other.GraphicsDesc.StencilStore &&
					FMemory::Memcmp(&GraphicsDesc.BlendState, &Other.GraphicsDesc.BlendState, sizeof(FBlendStateInitializerRHI)) == 0 &&
					FMemory::Memcmp(&GraphicsDesc.RasterizerState, &Other.GraphicsDesc.RasterizerState, sizeof(FPipelineFileCacheRasterizerState)) == 0 &&
					FMemory::Memcmp(&GraphicsDesc.DepthStencilState, &Other.GraphicsDesc.DepthStencilState, sizeof(FDepthStencilStateInitializerRHI)) == 0 &&
					FMemory::Memcmp(&GraphicsDesc.RenderTargetFormats, &Other.GraphicsDesc.RenderTargetFormats, sizeof(GraphicsDesc.RenderTargetFormats)) == 0 &&
					FMemory::Memcmp(&GraphicsDesc.RenderTargetFlags, &Other.GraphicsDesc.RenderTargetFlags, sizeof(GraphicsDesc.RenderTargetFlags)) == 0 &&
					FMemory::Memcmp(&GraphicsDesc.RenderTargetsLoad, &Other.GraphicsDesc.RenderTargetsLoad, sizeof(GraphicsDesc.RenderTargetsLoad)) == 0 &&
					FMemory::Memcmp(&GraphicsDesc.RenderTargetsStore, &Other.GraphicsDesc.RenderTargetsStore, sizeof(GraphicsDesc.RenderTargetsStore)) == 0;
					break;
				}
				default:
				{
					check(false);
					break;
				}
			}
		}
	}
	return bSame;
}

FPipelineCacheFileFormatPSO::FPipelineCacheFileFormatPSO(const FPipelineCacheFileFormatPSO& Other)
: Type(Other.Type)
, Hash(Other.Hash)
{
	switch(Type)
	{
		case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
		{
			// If we implement a classic copy constructor without memcpy - remove memset in ::Init() function above
			FMemory::Memcpy(&ComputeDesc, &Other.ComputeDesc, sizeof(ComputeDescriptor));
			break;
		}
		case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
		{
			// If we implement a classic copy constructor without memcpy - remove memset in ::Init() function above
			FMemory::Memcpy(&GraphicsDesc, &Other.GraphicsDesc, sizeof(GraphicsDescriptor));
			break;
		}
		default:
		{
			check(false);
			break;
		}
	}
}

FPipelineCacheFileFormatPSO& FPipelineCacheFileFormatPSO::operator=(const FPipelineCacheFileFormatPSO& Other)
{
	if(this != &Other)
	{
		Type = Other.Type;
		Hash = Other.Hash;
		switch(Type)
		{
			case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
			{
				// If we implement a classic copy constructor without memcpy - remove memset in ::Init() function above
				FMemory::Memcpy(&ComputeDesc, &Other.ComputeDesc, sizeof(ComputeDescriptor));
				break;
			}
			case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
			{
				// If we implement a classic copy constructor without memcpy - remove memset in ::Init() function above
				FMemory::Memcpy(&GraphicsDesc, &Other.GraphicsDesc, sizeof(GraphicsDescriptor));
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	return *this;
}

struct FPipelineCacheFileFormatTOC
{
	FPipelineCacheFileFormatTOC()
	: SortedOrder(FPipelineFileCache::PSOOrder::Default)
	{}
	
	FPipelineFileCache::PSOOrder SortedOrder;
	TMap<uint32, FPipelineCacheFileFormatPSOMetaData> MetaData;
	
	friend FArchive& operator<<(FArchive& Ar, FPipelineCacheFileFormatTOC& Info)
	{
		Ar << Info.SortedOrder;
		Ar << Info.MetaData;
		
		return Ar;
	}
};

class FPipelineCacheFile
{
public:
	static uint32 GameVersion;

	FPipelineCacheFile()
	: TOCOffset(0)
	, UserFileGuid(FGuid::NewGuid())
	, UserAsyncFileHandle(nullptr)
	, GameAsyncFileHandle(nullptr)
	{
	}
	
	bool OpenPipelineFileCache(FString const& FilePath, FGuid& Guid, TSharedPtr<IAsyncReadFileHandle, ESPMode::ThreadSafe>& Handle, FPipelineCacheFileFormatTOC& Content)
	{
		bool bOK = false;
		FArchive* FileReader = IFileManager::Get().CreateFileReader(*FilePath);
		if (FileReader)
		{
			FPipelineCacheFileFormatHeader Header;
			*FileReader << Header;
            FileReader->SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
			if (Header.Magic == FPipelineCacheFileFormatMagic && Header.Version == FPipelineCacheFileFormatCurrentVersion && Header.GameVersion == GameVersion && Header.Platform == ShaderPlatform)
			{
				check(Header.TableOffset > 0);
				TOCOffset = Header.TableOffset;
				Guid = Header.Guid;
				FileReader->Seek(Header.TableOffset);
				
				*FileReader << Content;
				bOK = !FileReader->IsError();
			}
			
			if(!FileReader->Close())
			{
				bOK = false;
			}
			delete FileReader;
			
			if(bOK)
			{
				UE_LOG(LogRHI, Log, TEXT("Opened FPipelineCacheFile: %s (GUID: %s) with %d entries."), *FilePath, *Guid.ToString(), Content.MetaData.Num());
				Handle = MakeShareable(FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*FilePath));
			}
		}
		else
		{
			UE_LOG(LogRHI, Log, TEXT("Could not open FPipelineCacheFile: %s"), *FilePath);
		}
		
		return bOK;
	}
	
    bool ShouldDeleteExistingUserCache()
    {
        static bool bOnce = false;
        static bool bCmdLineForce = false;
        if (!bOnce)
        {
            bOnce = true;
            bCmdLineForce = FParse::Param(FCommandLine::Get(), TEXT("deleteuserpsocache"));
            UE_CLOG(bCmdLineForce, LogRHI, Warning, TEXT("****************************** Deleting user-writable PSO cache as requested on command line"));
        }
        return bCmdLineForce;
    }
    
	bool OpenPipelineFileCache(FString const& FileName, EShaderPlatform Platform)
	{
		SET_DWORD_STAT(STAT_TotalGraphicsPipelineStateCount, 0);
		SET_DWORD_STAT(STAT_TotalComputePipelineStateCount, 0);
		SET_DWORD_STAT(STAT_SerializedGraphicsPipelineStateCount, 0);
		SET_DWORD_STAT(STAT_SerializedComputePipelineStateCount, 0);
		SET_DWORD_STAT(STAT_NewGraphicsPipelineStateCount, 0);
		SET_DWORD_STAT(STAT_NewComputePipelineStateCount, 0);
		
		TOC.SortedOrder = FPipelineFileCache::PSOOrder::Default;
		TOC.MetaData.Empty();
		
		Name = FileName;
		
		ShaderPlatform = Platform;
		PlatformName = LegacyShaderPlatformToShaderFormat(Platform);
		

		FString GamePathStable = FPaths::ProjectContentDir() / TEXT("PipelineCaches") / ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()) / FString::Printf(TEXT("%s_%s.stable.upipelinecache"), *FileName, *PlatformName.ToString());
		FString GamePath = FPaths::ProjectContentDir() / TEXT("PipelineCaches") / ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()) / FString::Printf(TEXT("%s_%s.upipelinecache"), *FileName, *PlatformName.ToString());

#if PLATFORM_MAC
		static bool bCommandLineNotStable = FParse::Param(FCommandLine::Get(), TEXT("nostablepipelinecache")) || IsRHIDeviceNVIDIA();
#else
		static bool bCommandLineNotStable = FParse::Param(FCommandLine::Get(), TEXT("nostablepipelinecache"));
#endif
		if (!bCommandLineNotStable && IFileManager::Get().FileExists(*GamePathStable))
		{
			GamePath = GamePathStable;
		}

		FString FilePath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("%s_%s.upipelinecache"), *FileName, *PlatformName.ToString());

		RecordingFilename = FString::Printf(TEXT("%s-CL-%u-"), *FEngineVersion::Current().GetBranchDescriptor(), FEngineVersion::Current().GetChangelist());

		FGuid UniqueFileGuid;
		FPlatformMisc::CreateGuid(UniqueFileGuid);  // not very unique on android, but won't matter much here
		RecordingFilename += FString::Printf(TEXT("%s_%s_%s.rec.upipelinecache"), *FileName, *PlatformName.ToString(), *UniqueFileGuid.ToString());
		RecordingFilename = FPaths::ProjectSavedDir() / TEXT("CollectedPSOs") / RecordingFilename;

		UE_LOG(LogRHI, Log, TEXT("Base name for record PSOs is %s"), *RecordingFilename);

		FString JournalPath = FilePath + TEXT(".tmp");
        bool const bJournalFileExists = IFileManager::Get().FileExists(*JournalPath);
		if (bJournalFileExists || ShouldDeleteExistingUserCache())
		{
			UE_LOG(LogRHI, Log, TEXT("Deleting FPipelineCacheFile: %s"), *FilePath);
            // If either of the above are true we need to dispose of this case as we consider it invalid
            if (IFileManager::Get().FileExists(*FilePath))
            {
                IFileManager::Get().Delete(*FilePath);
            }
            if (bJournalFileExists)
            {
                IFileManager::Get().Delete(*JournalPath);
            }
		}

		const bool bGameFileOk = OpenPipelineFileCache(GamePath, GameFileGuid, GameAsyncFileHandle, GameTOC);
        bool bUserFileOk = false;
        
        if (FPipelineFileCache::LogPSOtoFileCache() && (CVarPSOFileCacheSaveUserCache.GetValueOnAnyThread() > 0))
        {
            bUserFileOk = OpenPipelineFileCache(FilePath, UserFileGuid, UserAsyncFileHandle, TOC);
            if (!bUserFileOk)
            {
                // Start the file again!
                IFileManager::Get().Delete(*FilePath);
                TOCOffset = 0;
            }
            else
            {
                for (auto const& Entry : GameTOC.MetaData)
                {
					auto* MetaPtr = TOC.MetaData.Find(Entry.Key);
                    if (!MetaPtr || (MetaPtr->FileGuid != UserFileGuid && MetaPtr->FileGuid != GameFileGuid))
                    {
                        TOC.MetaData.Add(Entry.Key, Entry.Value);
                    }
                }
            }
        }
        
        if (!bUserFileOk)
        {
            TOC = GameTOC;
        }
        for (auto const& Entry : TOC.MetaData)
        {
            FPipelineStateStats* Stat = FPipelineFileCache::Stats.FindRef(Entry.Key);
            if (!Stat)
            {
                Stat = new FPipelineStateStats;
                Stat->PSOHash = Entry.Key;
                Stat->TotalBindCount = -1;
                FPipelineFileCache::Stats.Add(Entry.Key, Stat);
            }
        }
		
		SET_MEMORY_STAT(STAT_FileCacheMemory, TOC.MetaData.GetAllocatedSize());
		
		return bGameFileOk || bUserFileOk;
	}
	bool SavePipelineFileCache(FString const& FilePath, FPipelineFileCache::SaveMode Mode, TMap<uint32, FPipelineStateStats*> const& Stats, TSet<FPipelineCacheFileFormatPSO>& NewEntries, FPipelineFileCache::PSOOrder Order)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_SavePipelineFileCache);
		double StartTime = FPlatformTime::Seconds();
		FString SaveFilePath = FilePath;
		
		if (FPipelineFileCache::SaveMode::BoundPSOsOnly == Mode)
		{
			SaveFilePath = GetRecordingFilename();
		}
		
		bool bFileWriteSuccess = false;
		bool bPerformWrite = true;
		if (FPipelineFileCache::SaveMode::Incremental == Mode)
		{
			bPerformWrite = NewEntries.Num() || Order != TOC.SortedOrder;
			bFileWriteSuccess = !bPerformWrite;
		}
		
		if (bPerformWrite)
		{
            uint32 NumNewEntries = 0;
            
			FString JournalPath;
			if (Mode != FPipelineFileCache::SaveMode::BoundPSOsOnly)
			{
				JournalPath = SaveFilePath + TEXT(".jnl");
				FArchive* JournalWriter = IFileManager::Get().CreateFileWriter(*JournalPath);
				check(JournalWriter);

				// Header
				{
					FPipelineCacheFileFormatHeader Header;

					Header.Magic = FPipelineCacheFileFormatMagic;
					Header.Version = FPipelineCacheFileFormatCurrentVersion;
					Header.GameVersion = GameVersion;
					Header.Platform = ShaderPlatform;
					Header.Guid = UserFileGuid;
					Header.TableOffset = 0;

					*JournalWriter << Header;
				}

				check(!JournalWriter->IsError());
				JournalWriter->Close();
				delete JournalWriter;
				bPerformWrite = IFileManager::Get().FileExists(*JournalPath);
			}
			if (bPerformWrite)
			{
                FString GamePathStable = FPaths::ProjectContentDir() / TEXT("PipelineCaches") / ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()) / FString::Printf(TEXT("%s_%s.stable.upipelinecache"), *Name, *PlatformName.ToString());
                FString GamePath = FPaths::ProjectContentDir() / TEXT("PipelineCaches") / ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()) / FString::Printf(TEXT("%s_%s.upipelinecache"), *Name, *PlatformName.ToString());
#if PLATFORM_MAC
                static bool bCommandLineNotStable = FParse::Param(FCommandLine::Get(), TEXT("nostablepipelinecache")) || IsRHIDeviceNVIDIA();
#else
				static bool bCommandLineNotStable = FParse::Param(FCommandLine::Get(), TEXT("nostablepipelinecache"));
#endif
                if (!bCommandLineNotStable && IFileManager::Get().FileExists(*GamePathStable))
                {
                    GamePath = GamePathStable;
                }

                int64 GameFileSize = IFileManager::Get().FileSize(*GamePath);
				TArray<uint8> GameFileBytes;
				int64 FileSize = IFileManager::Get().FileSize(*FilePath);
				TArray<uint8> UserFileBytes;
				if(FPipelineFileCache::SaveMode::Incremental != Mode)
				{
					if (GameFileSize > 0)
					{
						if (GameAsyncFileHandle.IsValid())
						{
							GameFileBytes.AddZeroed(GameFileSize);
							IAsyncReadRequest* Request = GameAsyncFileHandle->ReadRequest(0, GameFileSize, AIOP_Normal, nullptr, GameFileBytes.GetData());
							Request->WaitCompletion();
							delete Request;
							// Can't report errors here because the AsyncIO requests have no such mechanism.
						}
						else
						{
							bool bReadOK = FFileHelper::LoadFileToArray(GameFileBytes, *GamePath);
							UE_CLOG(!bReadOK, LogRHI, Warning, TEXT("Failed to read %lld bytes from %s while re-saving the PipelineFileCache!"), GameFileSize, *GamePath);
						}
					}
		 
					if (FileSize > 0)
					{
						if (UserAsyncFileHandle.IsValid())
						{
							UserFileBytes.AddZeroed(FileSize);
							IAsyncReadRequest* Request = UserAsyncFileHandle->ReadRequest(0, FileSize, AIOP_Normal, nullptr, UserFileBytes.GetData());
							Request->WaitCompletion();
							delete Request;
							// Can't report errors here because the AsyncIO requests have no such mechanism.
						}
						else
						{
							bool bReadOK = FFileHelper::LoadFileToArray(UserFileBytes, *FilePath);
							UE_CLOG(!bReadOK, LogRHI, Warning, TEXT("Failed to read %lld bytes from %s while re-saving the PipelineFileCache!"), FileSize, *FilePath);
						}
					}
				}
		 
				// Assume caller has handled Platform specifc path + filename
				TArray<uint8> SaveBytes;
				FArchive* FileWriter;
				bool bUseMemoryWriter = (Mode == FPipelineFileCache::SaveMode::BoundPSOsOnly);
				FString TempPath = SaveFilePath;
				// Only use a file switcheroo on Apple platforms as they are the only ones tested so far.
				// At least two other platforms MoveFile implementation looks broken when moving from a writable source file to a writeable destination.
				// They only handle moves/renames between the read-only -> writeable directories/devices.
				if ((PLATFORM_APPLE || PLATFORM_ANDROID) && Mode != FPipelineFileCache::SaveMode::Incremental)
				{
					TempPath += TEXT(".tmp");
				}
				if (bUseMemoryWriter)
				{
					FileWriter = new FMemoryWriter(SaveBytes, true, false, FName(*SaveFilePath));
				}
				else
				{
					FileWriter = IFileManager::Get().CreateFileWriter(*TempPath, FILEWRITE_Append);
				}
				if (FileWriter)
				{
                    FileWriter->SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
					FileWriter->Seek(0);
		 
					// Header
					FPipelineCacheFileFormatHeader Header;
					{
						Header.Magic = FPipelineCacheFileFormatMagic;
						Header.Version = FPipelineCacheFileFormatCurrentVersion;
						Header.GameVersion = GameVersion;
						Header.Platform = ShaderPlatform;
						Header.Guid = UserFileGuid;
						Header.TableOffset = 0;
		 
						*FileWriter << Header;
		 
						TOCOffset = FMath::Max(TOCOffset, (uint64)FileWriter->Tell());
					}
		 
					uint32 TotalEntries = 0;
                    uint32 ConsolidatedEntries = 0;
                    uint32 RemovedEntries = 0;
                    switch (Mode)
                    {
						// This mode just writes new, used, entries to the end of the file and updates the TOC which will contain entries from the Game-Content file that are redundant.
                        case FPipelineFileCache::SaveMode::Incremental:
                        {
                            // PSO Descriptors
                            uint64 PSOOffset = TOCOffset;
                            
                            FileWriter->Seek(PSOOffset);
                            
                            // Add new entries
							TotalEntries = NewEntries.Num();
							for(auto It = NewEntries.CreateIterator(); It; ++It)
                            {
								FPipelineCacheFileFormatPSO& NewEntry = *It;
                                check(!IsPSOEntryCached(NewEntry));
                                
                                uint32 PSOHash = GetTypeHash(NewEntry);
								
								FPipelineStateStats const* Stat = Stats.FindRef(PSOHash);
                                if (Stat && Stat->TotalBindCount > 0)
								{
									FPipelineCacheFileFormatPSOMetaData Meta;
									Meta.Stats.PSOHash = PSOHash;
									Meta.FileOffset = PSOOffset;
									Meta.FileGuid = UserFileGuid;
									
									switch(NewEntry.Type)
									{
										case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
										{
											INC_DWORD_STAT(STAT_SerializedComputePipelineStateCount);
											Meta.Shaders.Add(NewEntry.ComputeDesc.ComputeShader);
											break;
										}
										case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
										{
											INC_DWORD_STAT(STAT_SerializedGraphicsPipelineStateCount);
											
											if (NewEntry.GraphicsDesc.VertexShader != FSHAHash())
												Meta.Shaders.Add(NewEntry.GraphicsDesc.VertexShader);
											
											if (NewEntry.GraphicsDesc.FragmentShader != FSHAHash())
												Meta.Shaders.Add(NewEntry.GraphicsDesc.FragmentShader);
											
											if (NewEntry.GraphicsDesc.HullShader != FSHAHash())
												Meta.Shaders.Add(NewEntry.GraphicsDesc.HullShader);
											
											if (NewEntry.GraphicsDesc.DomainShader != FSHAHash())
												Meta.Shaders.Add(NewEntry.GraphicsDesc.DomainShader);
											
											if (NewEntry.GraphicsDesc.GeometryShader != FSHAHash())
												Meta.Shaders.Add(NewEntry.GraphicsDesc.GeometryShader);
											
											break;
										}
										default:
										{
											check(false);
											break;
										}
									}
									
									TArray<uint8> Bytes;
									FMemoryWriter Wr(Bytes);
									Wr.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
									Wr << NewEntry;
									
									FileWriter->Serialize(Bytes.GetData(), Wr.TotalSize());
									
									Meta.FileSize = Wr.TotalSize();
									
									TOC.MetaData.Add(PSOHash, Meta);
									PSOOffset += Meta.FileSize;
									
									check(PSOOffset == FileWriter->Tell());
									
									NumNewEntries++;
									
									It.RemoveCurrent();
								}
                            }
                            
                            if(Order != FPipelineFileCache::PSOOrder::Default)
                            {
                                SortMetaData(TOC.MetaData, Order);
                                TOC.SortedOrder = Order;
                            }
                            else
                            {
                                // Added new entries and not re-sorted - the sort order invalid - reset to default
                                TOC.SortedOrder = FPipelineFileCache::PSOOrder::Default;
                            }
                            
                            Header.TableOffset = PSOOffset;
                            TOCOffset = PSOOffset;
                            
                            FileWriter->Seek(Header.TableOffset);
                            *FileWriter << TOC;
                            break;
                        }
						// These modes actually save to a separate file that records only PSOs that were bound.
						// BoundPSOsOnly will record all those PSOs used in this run of the game.
                        case FPipelineFileCache::SaveMode::BoundPSOsOnly:
                        {
							FMemoryReader UserFileBytesReader(UserFileBytes);
							FMemoryReader GameFileBytesReader(GameFileBytes);
							UserFileBytesReader.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
							GameFileBytesReader.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);

                            FPipelineCacheFileFormatTOC TempTOC = TOC;
                            TMap<uint32, FPipelineCacheFileFormatPSO> PSOs;
							
							Header.Guid = FGuid::NewGuid();
							
                            for (auto& Entry : NewEntries)
                            {
                                FPipelineCacheFileFormatPSOMetaData Meta;
                                Meta.Stats.PSOHash = GetTypeHash(Entry);
                                Meta.FileOffset = 0;
                                Meta.FileSize = 0;
                                Meta.FileGuid = Header.Guid;
                                
                                switch(Entry.Type)
                                {
                                    case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
                                    {
                                        INC_DWORD_STAT(STAT_SerializedComputePipelineStateCount);
                                        Meta.Shaders.Add(Entry.ComputeDesc.ComputeShader);
                                        break;
                                    }
                                    case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
                                    {
                                        INC_DWORD_STAT(STAT_SerializedGraphicsPipelineStateCount);
                                        
                                        if (Entry.GraphicsDesc.VertexShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.VertexShader);
                                        
                                        if (Entry.GraphicsDesc.FragmentShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.FragmentShader);
                                        
                                        if (Entry.GraphicsDesc.HullShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.HullShader);
                                        
                                        if (Entry.GraphicsDesc.DomainShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.DomainShader);
                                        
                                        if (Entry.GraphicsDesc.GeometryShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.GeometryShader);
                                        
                                        break;
                                    }
                                    default:
                                    {
                                        check(false);
                                        break;
                                    }
                                }
                                
                                TempTOC.MetaData.Add(Meta.Stats.PSOHash, Meta);
                                PSOs.Add(Meta.Stats.PSOHash, Entry);
                            }
                            
                            for (auto& Pair : Stats)
                            {
                                auto* MetaPtr = TempTOC.MetaData.Find(Pair.Key);
                                if (MetaPtr)
                                {
                                    auto& Meta = *MetaPtr;
                                    check(Meta.Stats.PSOHash == Pair.Value->PSOHash);
                                    Meta.Stats.CreateCount += Pair.Value->CreateCount;
                                    if (Pair.Value->FirstFrameUsed > Meta.Stats.FirstFrameUsed)
                                    {
                                        Meta.Stats.FirstFrameUsed = Pair.Value->FirstFrameUsed;
                                    }
                                    if (Pair.Value->LastFrameUsed > Meta.Stats.LastFrameUsed)
                                    {
                                        Meta.Stats.LastFrameUsed = Pair.Value->LastFrameUsed;
                                    }
									Meta.Stats.TotalBindCount = (int64)FMath::Min((uint64)INT64_MAX, (uint64)FMath::Max(Meta.Stats.TotalBindCount, 0ll) + (uint64)FMath::Max(Pair.Value->TotalBindCount, 0ll));
                                }
                            }
                            
                            for (auto It = TempTOC.MetaData.CreateIterator(); It; ++It)
                            {
                                FPipelineStateStats const* Stat = Stats.FindRef(It->Key);
								
								bool bUsed = (Stat && (Stat->TotalBindCount > 0));
								if (bUsed)
                                {
                                    if (!PSOs.Contains(It->Key))
                                    {
                                        FPipelineCacheFileFormatPSO PSO;
                                        check(It->Value.FileSize > 0);
                                        if (It->Value.FileGuid == UserFileGuid)
                                        {
											check(It->Value.FileOffset < (uint32)UserFileBytes.Num());
											UserFileBytesReader.Seek(It->Value.FileOffset);
											UserFileBytesReader << PSO;
                                        }
                                        else if (It->Value.FileGuid == GameFileGuid)
                                        {
											check(It->Value.FileOffset < (uint32)GameFileBytes.Num());
											GameFileBytesReader.Seek(It->Value.FileOffset);
											GameFileBytesReader << PSO;
                                        }
										else
										{
											UE_LOG(LogRHI, Verbose, TEXT("Trying to reconcile from unknown file GUID: %s but bound log file is: %s user file is: %s and game file is: %s - this means you have stale entries in a local cache file or the game content file is filled with bogus entries whose FileGUID doesn't match."), *(It->Value.FileGuid.ToString()), *(Header.Guid.ToString()), *(UserFileGuid.ToString()), *(GameFileGuid.ToString()));
											
											RemovedEntries++;
											It.RemoveCurrent();
										}

										PSOs.Add(It->Key, PSO);
                                    }
                                }
                                else
                                {
                                    RemovedEntries++;
                                    It.RemoveCurrent();
                                }
                            }
							TotalEntries = TempTOC.MetaData.Num();
                            
                            SortMetaData(TempTOC.MetaData, Order);
                            TempTOC.SortedOrder = Order;
                            
                            uint64 TempTOCOffset = (uint64)FileWriter->Tell();
                            
                            uint64 PSOOffset = TempTOCOffset;
                            
                            for (auto& Entry : TempTOC.MetaData)
                            {
                                FPipelineCacheFileFormatPSO& PSO = PSOs.FindChecked(Entry.Key);
                                
                                FileWriter->Seek(PSOOffset);
                                
                                Entry.Value.FileGuid = Header.Guid;
                                Entry.Value.FileOffset = PSOOffset;

								int64 At = FileWriter->Tell();
                                
                                (*FileWriter) << PSO;
                                
                                Entry.Value.FileSize = FileWriter->Tell() - At;
                                
                                PSOOffset += Entry.Value.FileSize;
                                check(PSOOffset == FileWriter->Tell());
								
								NumNewEntries++;
                            }
                            
                            Header.TableOffset = PSOOffset;
                            TempTOCOffset = PSOOffset;
                            
                            FileWriter->Seek(Header.TableOffset);
                            *FileWriter << TempTOC;
                            
                            break;
                        }
						// This mode should store all the PSOs that this device binds that weren't in a game-content cache.
						// It will store the meta-data for all the PSOs that are ever bound, but it will omit PSO descriptors for entries that were cached in the game-content file
						// This way the user builds up a log of uncaught entries but doesn't have to replicate the entire game-content file.
                        case FPipelineFileCache::SaveMode::SortedBoundPSOs:
                        {
							TMap<uint32, FPipelineCacheFileFormatPSO> PSOs;
                            for (auto& Entry : TOC.MetaData)
                            {
                                FPipelineCacheFileFormatPSO PSO;
                                uint8* Bytes = nullptr;
                                check(Entry.Value.FileSize > 0);
                                if (Entry.Value.FileGuid == UserFileGuid)
                                {
                                    Bytes = &UserFileBytes[Entry.Value.FileOffset];
									
									TArray<uint8> PSOData(Bytes, Entry.Value.FileSize);
									FMemoryReader Ar(PSOData);
									Ar.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
									Ar << PSO;
									PSOs.Add(Entry.Key, PSO);
                                }
								else if (Entry.Value.FileGuid != GameFileGuid)
								{
									UE_LOG(LogRHI, Verbose, TEXT("Trying to reconcile from unknown file GUID: %s but user file is: %s and game file is: %s - this means you have stale entries in a local cache file that reference a previous version of the game content cache."), *(Entry.Value.FileGuid.ToString()), *(UserFileGuid.ToString()), *(GameFileGuid.ToString()));
								}
                            }
                            
                            for (auto& Entry : NewEntries)
                            {
                                FPipelineCacheFileFormatPSOMetaData Meta;
                                Meta.Stats.PSOHash = GetTypeHash(Entry);
                                Meta.FileOffset = 0;
                                Meta.FileSize = 0;
                                Meta.FileGuid = UserFileGuid;
                                
                                switch(Entry.Type)
                                {
                                    case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
                                    {
                                        INC_DWORD_STAT(STAT_SerializedComputePipelineStateCount);
                                        Meta.Shaders.Add(Entry.ComputeDesc.ComputeShader);
                                        break;
                                    }
                                    case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
                                    {
                                        INC_DWORD_STAT(STAT_SerializedGraphicsPipelineStateCount);
                                        
                                        if (Entry.GraphicsDesc.VertexShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.VertexShader);
                                        
                                        if (Entry.GraphicsDesc.FragmentShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.FragmentShader);
                                        
                                        if (Entry.GraphicsDesc.HullShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.HullShader);
                                        
                                        if (Entry.GraphicsDesc.DomainShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.DomainShader);
                                        
                                        if (Entry.GraphicsDesc.GeometryShader != FSHAHash())
                                            Meta.Shaders.Add(Entry.GraphicsDesc.GeometryShader);
                                        
                                        break;
                                    }
                                    default:
                                    {
                                        check(false);
                                        break;
                                    }
                                }
                                
                                TOC.MetaData.Add(Meta.Stats.PSOHash, Meta);
                                PSOs.Add(Meta.Stats.PSOHash, Entry);
                            }
							
							FPipelineCacheFileFormatTOC TempTOC = TOC;
							// Update PSO usage stats for new and old
							for (auto& Pair : Stats)
							{
								auto* MetaPtr = TempTOC.MetaData.Find(Pair.Key);
								if (MetaPtr)
								{
									auto& Meta = *MetaPtr;
									check(Meta.Stats.PSOHash == Pair.Value->PSOHash);
									Meta.Stats.CreateCount += Pair.Value->CreateCount;
									if (Pair.Value->FirstFrameUsed > Meta.Stats.FirstFrameUsed)
									{
										Meta.Stats.FirstFrameUsed = Pair.Value->FirstFrameUsed;
									}
									if (Pair.Value->LastFrameUsed > Meta.Stats.LastFrameUsed)
									{
										Meta.Stats.LastFrameUsed = Pair.Value->LastFrameUsed;
									}
									Meta.Stats.TotalBindCount = (int64)FMath::Min((uint64)INT64_MAX, (uint64)FMath::Max(Meta.Stats.TotalBindCount, 0ll) + (uint64)FMath::Max(Pair.Value->TotalBindCount, 0ll));
								}
							}
							
							for (auto It = TempTOC.MetaData.CreateIterator(); It; ++It)
							{
								// If the entry doesn't belong to the game content or user local cache then remove it as it is invalid
								// Anything that has never been compiled (BindCount < 0) is invalid and can be removed
								// Or, if the BindCount is >= 0 and the same as in the GameTOC we have never seen it and we don't need to store it in the game cache
								FPipelineCacheFileFormatPSOMetaData* GameData = GameTOC.MetaData.Find(It->Key);
								if ((It->Value.FileGuid != UserFileGuid && It->Value.FileGuid != GameFileGuid)
									|| It->Value.Stats.TotalBindCount < 0
									|| (GameData && (It->Value.Stats.TotalBindCount == GameData->Stats.TotalBindCount)))
								{
									RemovedEntries++;
									It.RemoveCurrent();
								}
							}
							TotalEntries = TempTOC.MetaData.Num();
							
                            SortMetaData(TempTOC.MetaData, Order);
                            TOC.SortedOrder = TempTOC.SortedOrder = Order;
                            
                            TOCOffset = (uint64)FileWriter->Tell();
                            
                            uint64 PSOOffset = TOCOffset;
                            
                            for (auto& Entry : TempTOC.MetaData)
                            {
                                // When saved in this mode the user local file only stores the PSO descriptor for entries that weren't in the game-content cache
								// We don't need to store the PSO data for entries that come from the game cache
								// We do store the meta-data for all PSOs that this device has ever seen and that are valid with the current game-content and user cache.
								auto& CurrentMeta = TOC.MetaData.FindChecked(Entry.Key);
								if (CurrentMeta.FileGuid == UserFileGuid)
								{
									CurrentMeta.FileOffset = PSOOffset;
									Entry.Value.FileOffset = PSOOffset;
									
									FPipelineCacheFileFormatPSO& PSO = PSOs.FindChecked(Entry.Key);
									
									FileWriter->Seek(PSOOffset);
									
									TArray<uint8> Bytes;
									FMemoryWriter Wr(Bytes);
									Wr.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
									Wr << PSO;
									
									NewEntries.Remove(PSO);
									
									FileWriter->Serialize(Bytes.GetData(), Wr.TotalSize());
									
									CurrentMeta.FileSize = Wr.TotalSize();
									Entry.Value.FileSize = Wr.TotalSize();
									
									PSOOffset += Entry.Value.FileSize;
									check(PSOOffset == FileWriter->Tell());
									
									NumNewEntries++;
								}
                            }
                            
                            Header.TableOffset = PSOOffset;
                            TOCOffset = PSOOffset;
                            
                            FileWriter->Seek(Header.TableOffset);
                            *FileWriter << TempTOC;
							
                            break;
                        }
                        default:
                        {
                            check(false);
                            break;
                        }
                    }
		 
					// Overwrite the header now that we have the TOC location.
					FileWriter->Seek(0);
					*FileWriter << Header;
		 
					FileWriter->Flush();
		 
					bFileWriteSuccess = !FileWriter->IsError();
		 
					if(!FileWriter->Close())
					{
						bFileWriteSuccess = false;
					}
					if (bFileWriteSuccess && bUseMemoryWriter)
					{
						if (TotalEntries > 0)
						{
							bFileWriteSuccess = FFileHelper::SaveArrayToFile(SaveBytes, *TempPath);
						}
						else
						{
							delete FileWriter;
							float ThisTimeMS = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
							UE_LOG(LogRHI, Log, TEXT("FPipelineFileCache skipping saving empty .upipelinecache (took %6.2fms): %s."), ThisTimeMS, *SaveFilePath);
							return false;
						}
					}
		 
					if (bFileWriteSuccess)
                    {
						delete FileWriter;
						
						// As on POSIX only file moves on the same device are atomic
						if ((SaveFilePath == TempPath) || IFileManager::Get().Move(*SaveFilePath, *TempPath, true, true, true, true))
						{
							float ThisTimeMS = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;

							TCHAR const* ModeName = nullptr;
							switch (Mode)
							{
							case FPipelineFileCache::SaveMode::Incremental:
								ModeName = TEXT("Incremental");
								break;
							case FPipelineFileCache::SaveMode::BoundPSOsOnly:
								ModeName = TEXT("BoundPSOsOnly");
								break;
							case FPipelineFileCache::SaveMode::SortedBoundPSOs:
							default:
								ModeName = TEXT("SortedBoundPSOs");
								break;
							}
							UE_LOG(LogRHI, Log, TEXT("FPipelineFileCache %s saved %u total, %u new, %u removed, %u cons .upipelinecache (took %6.2fms): %s."), ModeName, TotalEntries, NumNewEntries, RemovedEntries, ConsolidatedEntries, ThisTimeMS, *SaveFilePath);
							
							if (JournalPath.Len())
							{
								IFileManager::Get().Delete(*JournalPath);
							}
						}
						else
						{
							float ThisTimeMS = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
							UE_LOG(LogRHI, Error, TEXT("Failed to move .upipelinecache from %s to %s (took %6.2fms)."), *TempPath, *SaveFilePath, ThisTimeMS);
						}
					}
                    else
                    {
						delete FileWriter;
						IFileManager::Get().Delete(*TempPath);
						float ThisTimeMS = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
						UE_LOG(LogRHI, Error, TEXT("Failed to write .upipelinecache, (took %6.2fms): %s."), ThisTimeMS, *SaveFilePath);
                    }
		 
				}
                else
                {
                    UE_LOG(LogRHI, Error, TEXT("Failed to open .upipelinecache for write: %s."), *SaveFilePath);
                }
			}
		}
		
		SET_MEMORY_STAT(STAT_FileCacheMemory, TOC.MetaData.GetAllocatedSize());
		
		return bFileWriteSuccess;
	}
	
	bool IsPSOEntryCached(FPipelineCacheFileFormatPSO const& NewEntry) const
	{
		uint32 PSOHash = GetTypeHash(NewEntry);
		FPipelineCacheFileFormatPSOMetaData const * const Existing = TOC.MetaData.Find(PSOHash);
		//check(!Existing || TOC.PSOs[Existing->Offset] == NewEntry);  // @todo re-instate this check
		return (Existing != nullptr);
	}
	
	static void SortMetaData(TMap<uint32, FPipelineCacheFileFormatPSOMetaData>& MetaData, FPipelineFileCache::PSOOrder Order)
	{
		// Only sorting metadata ordering - this should not affect PSO data offsets / lookups
		switch(Order)
		{
			case FPipelineFileCache::PSOOrder::FirstToLatestUsed:
			{
				MetaData.ValueSort([](const FPipelineCacheFileFormatPSOMetaData& A, const FPipelineCacheFileFormatPSOMetaData& B) {return A.Stats.FirstFrameUsed > B.Stats.FirstFrameUsed;});
				break;
			}
			case FPipelineFileCache::PSOOrder::MostToLeastUsed:
			{
				MetaData.ValueSort([](const FPipelineCacheFileFormatPSOMetaData& A, const FPipelineCacheFileFormatPSOMetaData& B) {return A.Stats.TotalBindCount > B.Stats.TotalBindCount;});
				break;
			}
			case FPipelineFileCache::PSOOrder::Default:
			default:
			{
				// NOP - leave as is
				break;
			}
		}
	}
	
	void GetOrderedPSOHashes(TArray<FPipelineCachePSOHeader>& PSOHashes, FPipelineFileCache::PSOOrder Order)
	{
		if(Order != TOC.SortedOrder)
		{
			SortMetaData(TOC.MetaData, Order);
			TOC.SortedOrder = Order;
		}
		for (auto const& Hash : TOC.MetaData)
		{
				FPipelineCachePSOHeader Header;
				Header.Hash = Hash.Key;
				Header.Shaders = Hash.Value.Shaders;
				PSOHashes.Add(Header);
			}
		}
	
	bool OnExternalReadCallback(FPipelineCacheFileFormatPSORead* Entry, double RemainingTime)
	{
		TSharedPtr<IAsyncReadRequest, ESPMode::ThreadSafe> LocalReadRequest = Entry->ReadRequest;
		
		check(LocalReadRequest.IsValid());
		
		if (RemainingTime < 0.0 && !LocalReadRequest->PollCompletion())
		{
			return false;
		}
		else if (RemainingTime >= 0.0 && !LocalReadRequest->WaitCompletion(RemainingTime))
		{
			return false;
		}
		
		Entry->bReadCompleted = 1;
		
		return true;
	}
	
	void FetchPSODescriptors(TArray<FPipelineCacheFileFormatPSORead*>& Batch)
	{
		for (FPipelineCacheFileFormatPSORead* Entry : Batch)
		{
			FPipelineCacheFileFormatPSOMetaData const& Meta = TOC.MetaData.FindChecked(Entry->Hash);
			
			if (Meta.FileGuid == GameFileGuid)
			{
                FPipelineCacheFileFormatPSOMetaData const* GameMeta = GameTOC.MetaData.Find(Entry->Hash);
                if (GameMeta)
                {
                    Entry->Data.SetNum(GameMeta->FileSize);
                    Entry->ParentFileHandle = GameAsyncFileHandle;
                    Entry->ReadRequest = MakeShareable(GameAsyncFileHandle->ReadRequest(GameMeta->FileOffset, GameMeta->FileSize, AIOP_Normal, nullptr, Entry->Data.GetData()));
                }
                else
                {
                    UE_LOG(LogRHI, Verbose, TEXT("Encountered a PSO entry %u that has been removed from the game-content file: %s"), Entry->Hash, *Meta.FileGuid.ToString());
                    Entry->bValid = false;
                    continue;
                }
			}
			else if (Meta.FileGuid == UserFileGuid)
			{
                Entry->Data.SetNum(Meta.FileSize);
				Entry->ParentFileHandle = UserAsyncFileHandle;
				Entry->ReadRequest = MakeShareable(UserAsyncFileHandle->ReadRequest(Meta.FileOffset, Meta.FileSize, AIOP_Normal, nullptr, Entry->Data.GetData()));
			}
            else
            {
                UE_LOG(LogRHI, Verbose, TEXT("Encountered a PSO entry %u that references unknown file ID: %s"), Entry->Hash, *Meta.FileGuid.ToString());
                Entry->bValid = false;
                continue;
            }
			
            Entry->bValid = true;
			FExternalReadCallback ExternalReadCallback = [this, Entry](double ReaminingTime)
			{
				return this->OnExternalReadCallback(Entry, ReaminingTime);
			};
			
			if (!Entry->Ar || !Entry->Ar->AttachExternalReadDependency(ExternalReadCallback))
			{
				ExternalReadCallback(0.0);
				check(Entry->bReadCompleted);
			}			
		}
	}
	
	FName GetPlatformName() const
	{
		return PlatformName;
	}

	const FString& GetRecordingFilename() const
	{
		return RecordingFilename;
	}
	
private:
	FString Name;
	EShaderPlatform ShaderPlatform;
	FName PlatformName;
	uint64 TOCOffset;
    FPipelineCacheFileFormatTOC GameTOC; // < The game TOC is kept around separately to handle cases where a fast-saved user cache tries to load removed entries from the game file.
	FPipelineCacheFileFormatTOC TOC;
	FGuid UserFileGuid;
	FGuid GameFileGuid;
	TSharedPtr<IAsyncReadFileHandle, ESPMode::ThreadSafe> UserAsyncFileHandle;
	TSharedPtr<IAsyncReadFileHandle, ESPMode::ThreadSafe> GameAsyncFileHandle;

	FString RecordingFilename;
};
uint32 FPipelineCacheFile::GameVersion = 0;

bool FPipelineFileCache::IsPipelineFileCacheEnabled()
{
	static bool bOnce = false;
	static bool bCmdLineForce = false;
	if (!bOnce)
	{
		bOnce = true;
		bCmdLineForce = FParse::Param(FCommandLine::Get(), TEXT("psocache"));
		UE_CLOG(bCmdLineForce, LogRHI, Warning, TEXT("****************************** Forcing PSO cache from command line"));
	}
	return FileCacheEnabled && (bCmdLineForce || CVarPSOFileCacheEnabled.GetValueOnAnyThread() == 1);
}

bool FPipelineFileCache::LogPSOtoFileCache()
{
	static bool bOnce = false;
	static bool bCmdLineForce = false;
	if (!bOnce)
	{
		bOnce = true;
		bCmdLineForce = FParse::Param(FCommandLine::Get(), TEXT("logpso"));
		UE_CLOG(bCmdLineForce, LogRHI, Warning, TEXT("****************************** Forcing logging of PSOs from command line"));
	}
	return (bCmdLineForce || CVarPSOFileCacheLogPSO.GetValueOnAnyThread() == 1);
}

bool FPipelineFileCache::ReportNewPSOs()
{
    static bool bOnce = false;
    static bool bCmdLineForce = false;
    if (!bOnce)
    {
        bOnce = true;
        bCmdLineForce = FParse::Param(FCommandLine::Get(), TEXT("reportpso"));
        UE_CLOG(bCmdLineForce, LogRHI, Warning, TEXT("****************************** Forcing reporting of new PSOs from command line"));
    }
    return (bCmdLineForce || CVarPSOFileCacheReportPSO.GetValueOnAnyThread() == 1);
}

void FPipelineFileCache::Initialize(uint32 InGameVersion)
{
	// Make enabled explicit on a flag not the existence of "FileCache" object as we are using that behind a lock and in Open / Close operations
	FileCacheEnabled = true;
	FPipelineCacheFile::GameVersion = InGameVersion;
	if (FPipelineCacheFile::GameVersion == 0)
	{
		// Defaulting the CL is fine though
		FPipelineCacheFile::GameVersion = (uint32)FEngineVersion::Current().GetChangelist();
	}
	
	SET_MEMORY_STAT(STAT_NewCachedPSOMemory, 0);
	SET_MEMORY_STAT(STAT_PSOStatMemory, 0);
}

void FPipelineFileCache::Shutdown()
{
	if(IsPipelineFileCacheEnabled())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_Write);
		for (auto const& Pair : Stats)
		{
			delete Pair.Value;
		}
		Stats.Empty();
		NewPSOs.Empty();
        NumNewPSOs = 0;
		
		FileCacheEnabled = false;
		
		SET_MEMORY_STAT(STAT_NewCachedPSOMemory, 0);
		SET_MEMORY_STAT(STAT_PSOStatMemory, 0);
	}
}

bool FPipelineFileCache::OpenPipelineFileCache(FString const& Name, EShaderPlatform Platform)
{
	bool bOk = false;
	
	if(IsPipelineFileCacheEnabled())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_Write);
		
		if(FileCache == nullptr)
		{
			FileCache = new FPipelineCacheFile();
			
			bOk = FileCache->OpenPipelineFileCache(Name, Platform);
			
			// File Cache now exists - these caches should be empty for this file otherwise will have false positives from any previous file caching - if not something has been caching when it should not be
			check(NewPSOs.Num() == 0);
			check(RunTimeToFileHashes.Num() == 0);
		}
	}
	
	return bOk;
}

bool FPipelineFileCache::SavePipelineFileCache(FString const& Name, SaveMode Mode)
{
	bool bOk = false;
	
	if(IsPipelineFileCacheEnabled() && LogPSOtoFileCache())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_Write);
		
		if(FileCache)
		{
			FName PlatformName = FileCache->GetPlatformName();
			FString Path = FPaths::ProjectSavedDir() / FString::Printf(TEXT("%s_%s.upipelinecache"), *Name, *PlatformName.ToString());
			bOk = FileCache->SavePipelineFileCache(Path, Mode, Stats, NewPSOs, RequestedOrder);
			
			// If successful clear new PSO's as they should have been saved out
			// Leave everything else in-tact (e.g stats) for subsequent in place save operations
			if (bOk)
			{
                NumNewPSOs = NewPSOs.Num();
				SET_MEMORY_STAT(STAT_NewCachedPSOMemory, (NumNewPSOs * (sizeof(FPipelineCacheFileFormatPSO) + sizeof(uint32) + sizeof(uint32))));
			}
		}
	}
	
	return bOk;
}

void FPipelineFileCache::ClosePipelineFileCache()
{
	if(IsPipelineFileCacheEnabled())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_Write);
		
		if(FileCache)
		{
			delete FileCache;
			FileCache = nullptr;
			
			// Reset stats tracking for the next file.
			for (auto const& Pair : Stats)
			{
				FPlatformAtomics::InterlockedExchange((int64*)&Pair.Value->TotalBindCount, -1);
				FPlatformAtomics::InterlockedExchange((int64*)&Pair.Value->FirstFrameUsed, -1);
				FPlatformAtomics::InterlockedExchange((int64*)&Pair.Value->LastFrameUsed, -1);
			}
			
			// Reset serialized counts
			SET_DWORD_STAT(STAT_SerializedGraphicsPipelineStateCount, 0);
			SET_DWORD_STAT(STAT_SerializedComputePipelineStateCount, 0);
			
			// Not tracking when there is no file clear other stats as well
			SET_DWORD_STAT(STAT_TotalGraphicsPipelineStateCount, 0);
			SET_DWORD_STAT(STAT_TotalComputePipelineStateCount, 0);
			SET_DWORD_STAT(STAT_NewGraphicsPipelineStateCount, 0);
			SET_DWORD_STAT(STAT_NewComputePipelineStateCount, 0);
			
			// Clear Runtime hashes otherwise we can't start adding newPSO's for a newly opened file
			RunTimeToFileHashes.Empty();
			NewPSOs.Empty();
            NumNewPSOs = 0;
			
			SET_MEMORY_STAT(STAT_NewCachedPSOMemory, 0);
			SET_MEMORY_STAT(STAT_FileCacheMemory, 0);
		}
	}
}

void FPipelineFileCache::CacheGraphicsPSO(uint32 RunTimeHash, FGraphicsPipelineStateInitializer const& Initializer)
{
	if(IsPipelineFileCacheEnabled() && (LogPSOtoFileCache() || ReportNewPSOs()))
	{
		FRWScopeLock Lock(FileCacheLock, SLT_ReadOnly);
	
		if(FileCache)
		{
			if(!RunTimeToFileHashes.Contains(RunTimeHash))
			{
				Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
				if(!RunTimeToFileHashes.Contains(RunTimeHash))
				{
					FPipelineCacheFileFormatPSO NewEntry;
					bool bOK = FPipelineCacheFileFormatPSO::Init(NewEntry, Initializer);
					check(bOK);
					
					uint32 PSOHash = GetTypeHash(NewEntry);
					RunTimeToFileHashes.Add(RunTimeHash, PSOHash);
					if (!FileCache->IsPSOEntryCached(NewEntry))
					{
				        UE_LOG(LogRHI, Warning, TEXT("Encountered a new graphics PSO: %u"), PSOHash);
						if (GPSOFileCachePrintNewPSODescriptors > 0)
						{
							UE_LOG(LogRHI, Warning, TEXT("New Graphics PSO (%u) Description: %s"), PSOHash, *NewEntry.GraphicsDesc.ToString());
						}
                        if (LogPSOtoFileCache())
                        {
                            NewPSOs.Add(NewEntry);
                            INC_MEMORY_STAT_BY(STAT_NewCachedPSOMemory, sizeof(FPipelineCacheFileFormatPSO) + sizeof(uint32) + sizeof(uint32));
                        }

                        NumNewPSOs++;
						INC_DWORD_STAT(STAT_NewGraphicsPipelineStateCount);
						INC_DWORD_STAT(STAT_TotalGraphicsPipelineStateCount);
						
						if (ReportNewPSOs() && PSOLoggedEvent.IsBound())
						{
							PSOLoggedEvent.Broadcast(NewEntry);
						}
					}
				}
			}
		}
	}
}

void FPipelineFileCache::CacheComputePSO(uint32 RunTimeHash, FRHIComputeShader const* Initializer)
{
	if(IsPipelineFileCacheEnabled() && (LogPSOtoFileCache() || ReportNewPSOs()))
	{
		FRWScopeLock Lock(FileCacheLock, SLT_ReadOnly);
		
		if(FileCache)
		{
			if(!RunTimeToFileHashes.Contains(RunTimeHash))
			{
				Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
				if(!RunTimeToFileHashes.Contains(RunTimeHash))
				{
					FPipelineCacheFileFormatPSO NewEntry;
					bool bOK = FPipelineCacheFileFormatPSO::Init(NewEntry, Initializer);
					check(bOK);
					
					uint32 PSOHash = GetTypeHash(NewEntry);
					RunTimeToFileHashes.Add(RunTimeHash, PSOHash);
					if (!FileCache->IsPSOEntryCached(NewEntry))
					{
				        UE_LOG(LogRHI, Warning, TEXT("Encountered a new compute PSO: %u"), PSOHash);
						if (GPSOFileCachePrintNewPSODescriptors > 0)
						{
							UE_LOG(LogRHI, Warning, TEXT("New compute PSO (%u) Description: %s"), PSOHash, *NewEntry.ComputeDesc.ComputeShader.ToString());
						}

                        if (LogPSOtoFileCache())
                        {
                            NewPSOs.Add(NewEntry);
                            INC_MEMORY_STAT_BY(STAT_NewCachedPSOMemory, sizeof(FPipelineCacheFileFormatPSO) + sizeof(uint32) + sizeof(uint32));
                        }
						
                        NumNewPSOs++;
						INC_DWORD_STAT(STAT_NewComputePipelineStateCount);
						INC_DWORD_STAT(STAT_TotalComputePipelineStateCount);
						
						if (ReportNewPSOs() && PSOLoggedEvent.IsBound())
						{
							PSOLoggedEvent.Broadcast(NewEntry);
						}
					}
				}
			}
		}
	}
}

FPipelineStateStats* FPipelineFileCache::RegisterPSOStats(uint32 RunTimeHash)
{
	FPipelineStateStats* Stat = nullptr;
	if(IsPipelineFileCacheEnabled() && LogPSOtoFileCache())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_ReadOnly);
		
		if(FileCache)
		{
			uint32 PSOHash = RunTimeToFileHashes.FindChecked(RunTimeHash);
			Stat = Stats.FindRef(PSOHash);
			if (!Stat)
			{
				Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
				Stat = Stats.FindRef(PSOHash);
				if(!Stat)
				{
					Stat = new FPipelineStateStats;
					Stat->PSOHash = PSOHash;
					Stats.Add(PSOHash, Stat);
					
					INC_MEMORY_STAT_BY(STAT_PSOStatMemory, sizeof(FPipelineStateStats) + sizeof(uint32));
				}
			}
			Stat->CreateCount++;
		}
	}
	return Stat;
}

void FPipelineFileCache::GetOrderedPSOHashes(TArray<FPipelineCachePSOHeader>& PSOHashes, PSOOrder Order)
{
	if(IsPipelineFileCacheEnabled())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_Write);
		
		RequestedOrder = Order;
		
		if(FileCache)
		{
			FileCache->GetOrderedPSOHashes(PSOHashes, Order);
		}
	}
}

void FPipelineFileCache::FetchPSODescriptors(TArray<FPipelineCacheFileFormatPSORead*>& Batch)
{
	if(IsPipelineFileCacheEnabled())
	{
		FRWScopeLock Lock(FileCacheLock, SLT_ReadOnly);
		
		if(FileCache)
		{
			FileCache->FetchPSODescriptors(Batch);
		}
	}
}

struct FPipelineCacheFileData
{
	FPipelineCacheFileFormatHeader Header;
	TMap<uint32, FPipelineCacheFileFormatPSO> PSOs;
	FPipelineCacheFileFormatTOC TOC;
	
	static FPipelineCacheFileData Open(FString const& FilePath)
	{
		FPipelineCacheFileData Data;
		Data.Header.Magic = 0;
		FArchive* FileAReader = IFileManager::Get().CreateFileReader(*FilePath);
		if (FileAReader)
		{
			*FileAReader << Data.Header;
			if (Data.Header.Magic == FPipelineCacheFileFormatMagic && Data.Header.Version >= (uint32)EPipelineCacheFileFormatVersions::FirstWorking)
			{
                FileAReader->SetGameNetVer(Data.Header.Version);
				check(Data.Header.TableOffset > 0);
				FileAReader->Seek(Data.Header.TableOffset);
				
				*FileAReader << Data.TOC;
				if (!FileAReader->IsError())
				{
					for (auto& Entry : Data.TOC.MetaData)
					{
                        if (Entry.Value.FileGuid == Data.Header.Guid)
                        {
                            FPipelineCacheFileFormatPSO PSO;
                            FileAReader->Seek(Entry.Value.FileOffset);
                            *FileAReader << PSO;
                            
                            Data.PSOs.Add(Entry.Key, PSO);
                        }
					}
				}
				
				if (FileAReader->IsError())
				{
                    UE_LOG(LogRHI, Error, TEXT("Failed to read: %s."), *FilePath);
					Data.Header.Magic = 0;
				}
				else
				{
					if (Data.Header.Version < (uint32)EPipelineCacheFileFormatVersions::ShaderMetaData)
					{
						for (auto& Entry : Data.TOC.MetaData)
						{
							FPipelineCacheFileFormatPSO& PSO = Data.PSOs.FindChecked(Entry.Key);
							switch(PSO.Type)
							{
								case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
									Entry.Value.Shaders.Add(PSO.ComputeDesc.ComputeShader);
									break;
								case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
									Entry.Value.Shaders.Add(PSO.GraphicsDesc.VertexShader);
									
									if (PSO.GraphicsDesc.FragmentShader != FSHAHash())
									{
										Entry.Value.Shaders.Add(PSO.GraphicsDesc.FragmentShader);
									}
									
									if (PSO.GraphicsDesc.GeometryShader != FSHAHash())
									{
										Entry.Value.Shaders.Add(PSO.GraphicsDesc.GeometryShader);
									}
									
									if (PSO.GraphicsDesc.HullShader != FSHAHash())
									{
										Entry.Value.Shaders.Add(PSO.GraphicsDesc.HullShader);
									}
									
									if (PSO.GraphicsDesc.DomainShader != FSHAHash())
									{
										Entry.Value.Shaders.Add(PSO.GraphicsDesc.DomainShader);
									}
									break;
								default:
									check(false);
									break;
							}
						}
					}
					
					if (Data.Header.Version < (uint32)EPipelineCacheFileFormatVersions::SortedVertexDesc)
					{
						TMap<uint32, FPipelineCacheFileFormatPSOMetaData> MetaData;
						TMap<uint32, FPipelineCacheFileFormatPSO> PSOs;
						for (auto& Entry : Data.TOC.MetaData)
						{
							FPipelineCacheFileFormatPSO& PSO = Data.PSOs.FindChecked(Entry.Key);
							PSOs.Add(GetTypeHash(PSO), PSO);
							MetaData.Add(GetTypeHash(PSO), Entry.Value);
						}
						
						Data.TOC.MetaData = MetaData;
						Data.PSOs = PSOs;
					}
                    
                    Data.Header.Version = FPipelineCacheFileFormatCurrentVersion;
                }
			}
			
			FileAReader->Close();
			
			delete FileAReader;
		}
        else
        {
            UE_LOG(LogRHI, Error, TEXT("Failed to open: %s."), *FilePath);
        }
		return Data;
	}
};
		 
uint32 FPipelineFileCache::NumPSOsLogged()
{
	uint32 Result = 0;
	if(IsPipelineFileCacheEnabled() && LogPSOtoFileCache())
	{
		// Only count PSOs that are both new and have at least one bind, otherwise we can ignore them.
		FRWScopeLock Lock(FileCacheLock, SLT_ReadOnly);
		if (NumNewPSOs)
		{
			for (auto& PSO : NewPSOs)
			{
				FPipelineStateStats const* Stat = Stats.FindRef(GetTypeHash(PSO));
				if (Stat && Stat->TotalBindCount > 0)
				{
					Result++;
				}
			}
		}
	}
	return Result;
}

FPipelineFileCache::FPipelineStateLoggedEvent& FPipelineFileCache::OnPipelineStateLogged()
{
	return PSOLoggedEvent;
}

bool FPipelineFileCache::LoadPipelineFileCacheInto(FString const& Path, TSet<FPipelineCacheFileFormatPSO>& PSOs)
{
	FPipelineCacheFileData A = FPipelineCacheFileData::Open(Path);
	bool bAny = false;
	for (const auto& Pair : A.PSOs)
	{
		PSOs.Add(Pair.Value);
		bAny = true;
	}
	return bAny;
}

bool FPipelineFileCache::SavePipelineFileCacheFrom(uint32 GameVersion, EShaderPlatform Platform, FString const& Path, const TSet<FPipelineCacheFileFormatPSO>& PSOs)
{
	FPipelineCacheFileData Output;
	Output.Header.Magic = FPipelineCacheFileFormatMagic;
	Output.Header.Version = FPipelineCacheFileFormatCurrentVersion;
	Output.Header.GameVersion = GameVersion;
	Output.Header.Platform = Platform;
	Output.Header.TableOffset = 0;
	Output.Header.Guid = FGuid::NewGuid();

	Output.TOC.MetaData.Reserve(PSOs.Num());

	for (const FPipelineCacheFileFormatPSO& Item : PSOs)
	{
		FPipelineCacheFileFormatPSOMetaData Meta;
		Meta.Stats.PSOHash = GetTypeHash(Item);
		Meta.FileGuid = Output.Header.Guid;
		Meta.FileSize = 0;
		switch (Item.Type)
		{
			case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
			{
				INC_DWORD_STAT(STAT_SerializedComputePipelineStateCount);
				Meta.Shaders.Add(Item.ComputeDesc.ComputeShader);
				break;
			}
			case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
			{
				INC_DWORD_STAT(STAT_SerializedGraphicsPipelineStateCount);

				if (Item.GraphicsDesc.VertexShader != FSHAHash())
					Meta.Shaders.Add(Item.GraphicsDesc.VertexShader);

				if (Item.GraphicsDesc.FragmentShader != FSHAHash())
					Meta.Shaders.Add(Item.GraphicsDesc.FragmentShader);

				if (Item.GraphicsDesc.HullShader != FSHAHash())
					Meta.Shaders.Add(Item.GraphicsDesc.HullShader);

				if (Item.GraphicsDesc.DomainShader != FSHAHash())
					Meta.Shaders.Add(Item.GraphicsDesc.DomainShader);

				if (Item.GraphicsDesc.GeometryShader != FSHAHash())
					Meta.Shaders.Add(Item.GraphicsDesc.GeometryShader);

				break;
			}
			default:
			{
				check(false);
				break;
			}
		}

		Output.TOC.MetaData.Add(Meta.Stats.PSOHash, Meta);
		Output.PSOs.Add(Meta.Stats.PSOHash, Item);
	}

	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*Path);
	if (!FileWriter)
	{
		return false;
	}
	FileWriter->SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
	*FileWriter << Output.Header;

	uint64 PSOOffset = (uint64)FileWriter->Tell();

	for (auto& Entry : Output.TOC.MetaData)
	{
		FPipelineCacheFileFormatPSO& PSO = Output.PSOs.FindChecked(Entry.Key);

		uint32 PSOHash = Entry.Key;

		Entry.Value.FileOffset = PSOOffset;
		Entry.Value.FileGuid = Output.Header.Guid;

		TArray<uint8> Bytes;
		FMemoryWriter Wr(Bytes);
		Wr.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
		Wr << PSO;

		FileWriter->Serialize(Bytes.GetData(), Wr.TotalSize());

		Entry.Value.FileSize = Wr.TotalSize();
		PSOOffset += Entry.Value.FileSize;
	}

	FileWriter->Seek(0);

	Output.Header.TableOffset = PSOOffset;
	*FileWriter << Output.Header;

	FileWriter->Seek(PSOOffset);
	*FileWriter << Output.TOC;

	FileWriter->Flush();

	bool bOK = !FileWriter->IsError();

	FileWriter->Close();

	delete FileWriter;
	return bOK;
}


bool FPipelineFileCache::MergePipelineFileCaches(FString const& PathA, FString const& PathB, FPipelineFileCache::PSOOrder Order, FString const& OutputPath)
{
	bool bOK = false;
	
	FPipelineCacheFileData A = FPipelineCacheFileData::Open(PathA);
	FPipelineCacheFileData B = FPipelineCacheFileData::Open(PathB);
	
	if (A.Header.Magic == FPipelineCacheFileFormatMagic && B.Header.Magic == FPipelineCacheFileFormatMagic && A.Header.GameVersion == B.Header.GameVersion && A.Header.Platform == B.Header.Platform && A.Header.Version == FPipelineCacheFileFormatCurrentVersion && B.Header.Version == FPipelineCacheFileFormatCurrentVersion)
	{
		FPipelineCacheFileData Output;
		Output.Header.Magic = FPipelineCacheFileFormatMagic;
		Output.Header.Version = FPipelineCacheFileFormatCurrentVersion;
		Output.Header.GameVersion = A.Header.GameVersion;
		Output.Header.Platform = A.Header.Platform;
		Output.Header.TableOffset = 0;
		Output.Header.Guid = FGuid::NewGuid();
		
		for (auto const& Entry : A.TOC.MetaData)
		{
			Output.TOC.MetaData.Add(Entry.Key, Entry.Value);
		}
		for (auto const& Entry : B.TOC.MetaData)
		{
			Output.TOC.MetaData.Add(Entry.Key, Entry.Value);
		}
		FPipelineCacheFile::SortMetaData(Output.TOC.MetaData, Order);
		Output.TOC.SortedOrder = Order;

		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*OutputPath);
		if (FileWriter)
		{
            FileWriter->SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
			FileWriter->Seek(0);
			*FileWriter << Output.Header;
			
			uint64 PSOOffset = (uint64)FileWriter->Tell();
			
            TSet<uint32> HashesToRemove;
            
			for (auto& Entry : Output.TOC.MetaData)
			{
				FPipelineCacheFileFormatPSO PSO;
				if (Entry.Value.FileGuid == A.Header.Guid)
				{
					PSO = A.PSOs.FindChecked(Entry.Key);
				}
				else if (Entry.Value.FileGuid == B.Header.Guid)
				{
					PSO = B.PSOs.FindChecked(Entry.Key);
				}
                else
                {
                    HashesToRemove.Add(Entry.Key);
                    continue;
                }
				
				uint32 PSOHash = Entry.Key;
				
				Entry.Value.FileOffset = PSOOffset;
				Entry.Value.FileGuid = Output.Header.Guid;
				
				TArray<uint8> Bytes;
				FMemoryWriter Wr(Bytes);
				Wr.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
				Wr << PSO;
				
				FileWriter->Serialize(Bytes.GetData(), Wr.TotalSize());
				
				Entry.Value.FileSize = Wr.TotalSize();
				PSOOffset += Entry.Value.FileSize;
			}
            
            for (uint32 Key : HashesToRemove)
            {
                Output.TOC.MetaData.Remove(Key);
            }
			
			FileWriter->Seek(0);
			
			Output.Header.TableOffset = PSOOffset;
			*FileWriter << Output.Header;
			
			FileWriter->Seek(PSOOffset);
			*FileWriter << Output.TOC;
			
			FileWriter->Flush();
			
			bOK = !FileWriter->IsError();
            
            UE_CLOG(!bOK, LogRHI, Error, TEXT("Failed to write output file: %s."), *OutputPath);
			
			FileWriter->Close();
			
			delete FileWriter;
		}
        else
        {
            UE_LOG(LogRHI, Error, TEXT("Failed to open output file: %s."), *OutputPath);
        }
	}
    else if (A.Header.GameVersion != B.Header.GameVersion)
    {
        UE_LOG(LogRHI, Error, TEXT("Incompatible game versions: %u vs. %u."), A.Header.GameVersion, B.Header.GameVersion);
    }
    else if (A.Header.Platform != B.Header.Platform)
    {
        UE_LOG(LogRHI, Error, TEXT("Incompatible shader platforms: %s vs. %s."), *LegacyShaderPlatformToShaderFormat(A.Header.Platform).ToString(), *LegacyShaderPlatformToShaderFormat(B.Header.Platform).ToString());
    }
    else if (A.Header.Version != B.Header.Version)
    {
        UE_LOG(LogRHI, Error, TEXT("Incompatible file versions: %u vs. %u."), A.Header.Version, B.Header.Version);
    }
    else
    {
        UE_LOG(LogRHI, Error, TEXT("Incompatible file headers: %u vs. %u: expected %u."), A.Header.Magic, B.Header.Magic, FPipelineCacheFileFormatMagic);
    }
	return bOK;
}


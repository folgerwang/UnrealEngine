// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHI.cpp: Render Hardware Interface implementation.
=============================================================================*/

#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "RHIShaderFormatDefinitions.inl"
#include "ProfilingDebugging/CsvProfiler.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, RHI);

/** RHI Logging. */
DEFINE_LOG_CATEGORY(LogRHI);
CSV_DEFINE_CATEGORY(RHI, true);

// Define counter stats.
DEFINE_STAT(STAT_RHIDrawPrimitiveCalls);
DEFINE_STAT(STAT_RHITriangles);
DEFINE_STAT(STAT_RHILines);

// Define memory stats.
DEFINE_STAT(STAT_RenderTargetMemory2D);
DEFINE_STAT(STAT_RenderTargetMemory3D);
DEFINE_STAT(STAT_RenderTargetMemoryCube);
DEFINE_STAT(STAT_TextureMemory2D);
DEFINE_STAT(STAT_TextureMemory3D);
DEFINE_STAT(STAT_TextureMemoryCube);
DEFINE_STAT(STAT_UniformBufferMemory);
DEFINE_STAT(STAT_IndexBufferMemory);
DEFINE_STAT(STAT_VertexBufferMemory);
DEFINE_STAT(STAT_StructuredBufferMemory);
DEFINE_STAT(STAT_PixelBufferMemory);

static FAutoConsoleVariable CVarUseVulkanRealUBs(
	TEXT("r.Vulkan.UseRealUBs"),
	0,
	TEXT("0: Emulate uniform buffers on Vulkan SM4/SM5 (debugging ONLY)\n")
	TEXT("1: Use real uniform buffers [default]"),
	ECVF_ReadOnly
	);

static FAutoConsoleVariable CVarVulkanEnableTessellation(
	TEXT("r.Vulkan.EnableTessellation"),
	0,
	TEXT("0: Tessellation disabled[default]\n")
	TEXT("1: Enable flat tessellation (experimental)"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarDisableEngineAndAppRegistration(
	TEXT("r.DisableEngineAndAppRegistration"),
	0,
	TEXT("If true, disables engine and app registration, to disable GPU driver optimizations during debugging and development\n")
	TEXT("Changes will only take effect in new game/editor instances - can't be changed at runtime.\n"),
	ECVF_Default);


const FString FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)EResourceTransitionAccess::EMaxAccess + 1] =
{
	FString(TEXT("EReadable")),
	FString(TEXT("EWritable")),	
	FString(TEXT("ERWBarrier")),
	FString(TEXT("ERWNoBarrier")),
	FString(TEXT("ERWSubResBarrier")),
	FString(TEXT("EMetaData")),
	FString(TEXT("EMaxAccess")),
};

#if STATS
#include "Stats/StatsData.h"
static void DumpRHIMemory(FOutputDevice& OutputDevice)
{
	TArray<FStatMessage> Stats;
	GetPermanentStats(Stats);

	FName NAME_STATGROUP_RHI(FStatGroup_STATGROUP_RHI::GetGroupName());
	OutputDevice.Logf(TEXT("RHI resource memory (not tracked by our allocator)"));
	int64 TotalMemory = 0;
	for (int32 Index = 0; Index < Stats.Num(); Index++)
	{
		FStatMessage const& Meta = Stats[Index];
		FName LastGroup = Meta.NameAndInfo.GetGroupName();
		if (LastGroup == NAME_STATGROUP_RHI && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
		{
			OutputDevice.Logf(TEXT("%s"), *FStatsUtils::DebugPrint(Meta));
			TotalMemory += Meta.GetValue_int64();
		}
	}
	OutputDevice.Logf(TEXT("%.3fMB total"), TotalMemory / 1024.f / 1024.f);
}

static FAutoConsoleCommandWithOutputDevice GDumpRHIMemoryCmd(
	TEXT("rhi.DumpMemory"),
	TEXT("Dumps RHI memory stats to the log"),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic(DumpRHIMemory)
	);
#endif

//DO NOT USE THE STATIC FLINEARCOLORS TO INITIALIZE THIS STUFF.  
//Static init order is undefined and you will likely end up with bad values on some platforms.
const FClearValueBinding FClearValueBinding::None(EClearBinding::ENoneBound);
const FClearValueBinding FClearValueBinding::Black(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
const FClearValueBinding FClearValueBinding::White(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
const FClearValueBinding FClearValueBinding::Transparent(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
const FClearValueBinding FClearValueBinding::DepthOne(1.0f, 0);
const FClearValueBinding FClearValueBinding::DepthZero(0.0f, 0);
const FClearValueBinding FClearValueBinding::DepthNear((float)ERHIZBuffer::NearPlane, 0);
const FClearValueBinding FClearValueBinding::DepthFar((float)ERHIZBuffer::FarPlane, 0);
const FClearValueBinding FClearValueBinding::Green(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
// Note: this is used as the default normal for DBuffer decals.  It must decode to a value of 0 in DecodeDBufferData.
const FClearValueBinding FClearValueBinding::DefaultNormal8Bit(FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f));

TLockFreePointerListUnordered<FRHIResource, PLATFORM_CACHE_LINE_SIZE> FRHIResource::PendingDeletes;
FRHIResource* FRHIResource::CurrentlyDeleting = nullptr;
TArray<FRHIResource::ResourcesToDelete> FRHIResource::DeferredDeletionQueue;
uint32 FRHIResource::CurrentFrame = 0;

FString FVertexElement::ToString() const
{
	return FString::Printf(TEXT("<%u %u %u %u %u %u>")
		, uint32(StreamIndex)
		, uint32(Offset)
		, uint32(Type)
		, uint32(AttributeIndex)
		, uint32(Stride)
		, uint32(bUseInstanceIndex)
	);
}

void FVertexElement::FromString(const FString& InSrc)
{
	FString Src = InSrc;
	Src.ReplaceInline(TEXT("\r"), TEXT(" "));
	Src.ReplaceInline(TEXT("\n"), TEXT(" "));
	Src.ReplaceInline(TEXT("\t"), TEXT(" "));
	Src.ReplaceInline(TEXT("<"), TEXT(" "));
	Src.ReplaceInline(TEXT(">"), TEXT(" "));
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(" "));

	check(Parts.Num() == 6 && sizeof(Type) == 1); //not a very robust parser
	LexFromString(StreamIndex, *Parts[0]);
	LexFromString(Offset, *Parts[1]);
	LexFromString((uint8&)Type, *Parts[2]);
	LexFromString(AttributeIndex, *Parts[3]);
	LexFromString(Stride, *Parts[4]);
	LexFromString(bUseInstanceIndex, *Parts[5]);
}

FString FDepthStencilStateInitializerRHI::ToString() const
{
	return
		FString::Printf(TEXT("<%u %u ")
			, uint32(!!bEnableDepthWrite)
			, uint32(DepthTest)
		)
		+ FString::Printf(TEXT("%u %u %u %u %u ")
			, uint32(!!bEnableFrontFaceStencil)
			, uint32(FrontFaceStencilTest)
			, uint32(FrontFaceStencilFailStencilOp)
			, uint32(FrontFaceDepthFailStencilOp)
			, uint32(FrontFacePassStencilOp)
		)
		+ FString::Printf(TEXT("%u %u %u %u %u ")
			, uint32(!!bEnableBackFaceStencil)
			, uint32(BackFaceStencilTest)
			, uint32(BackFaceStencilFailStencilOp)
			, uint32(BackFaceDepthFailStencilOp)
			, uint32(BackFacePassStencilOp)
		)
		+ FString::Printf(TEXT("%u %u>")
			, uint32(StencilReadMask)
			, uint32(StencilWriteMask)
		);
}
void FDepthStencilStateInitializerRHI::FromString(const FString& InSrc)
{
	FString Src = InSrc;
	Src.ReplaceInline(TEXT("\r"), TEXT(" "));
	Src.ReplaceInline(TEXT("\n"), TEXT(" "));
	Src.ReplaceInline(TEXT("\t"), TEXT(" "));
	Src.ReplaceInline(TEXT("<"), TEXT(" "));
	Src.ReplaceInline(TEXT(">"), TEXT(" "));
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(" "));

	check(Parts.Num() == 14 && sizeof(bool) == 1 && sizeof(FrontFaceStencilFailStencilOp) == 1 && sizeof(BackFaceStencilTest) == 1 && sizeof(BackFaceDepthFailStencilOp) == 1); //not a very robust parser

	LexFromString((uint8&)bEnableDepthWrite, *Parts[0]);
	LexFromString((uint8&)DepthTest, *Parts[1]);

	LexFromString((uint8&)bEnableFrontFaceStencil, *Parts[2]);
	LexFromString((uint8&)FrontFaceStencilTest, *Parts[3]);
	LexFromString((uint8&)FrontFaceStencilFailStencilOp, *Parts[4]);
	LexFromString((uint8&)FrontFaceDepthFailStencilOp, *Parts[5]);
	LexFromString((uint8&)FrontFacePassStencilOp, *Parts[6]);

	LexFromString((uint8&)bEnableBackFaceStencil, *Parts[7]);
	LexFromString((uint8&)BackFaceStencilTest, *Parts[8]);
	LexFromString((uint8&)BackFaceStencilFailStencilOp, *Parts[9]);
	LexFromString((uint8&)BackFaceDepthFailStencilOp, *Parts[10]);
	LexFromString((uint8&)BackFacePassStencilOp, *Parts[11]);

	LexFromString(StencilReadMask, *Parts[12]);
	LexFromString(StencilWriteMask, *Parts[13]);
}

FString FBlendStateInitializerRHI::ToString() const
{
	FString Result = TEXT("<");
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		Result += RenderTargets[Index].ToString();
	}
	Result += FString::Printf(TEXT("%d>"), uint32(!!bUseIndependentRenderTargetBlendStates));
	return Result;
}

void FBlendStateInitializerRHI::FromString(const FString& InSrc)
{
	FString Src = InSrc;
	Src.ReplaceInline(TEXT("\r"), TEXT(" "));
	Src.ReplaceInline(TEXT("\n"), TEXT(" "));
	Src.ReplaceInline(TEXT("\t"), TEXT(" "));
	Src.ReplaceInline(TEXT("<"), TEXT(" "));
	Src.ReplaceInline(TEXT(">"), TEXT(" "));
	TArray<FString> Parts;
	Src.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(" "));


	check(Parts.Num() == MaxSimultaneousRenderTargets * FRenderTarget::NUM_STRING_FIELDS + 1 && sizeof(bool) == 1); //not a very robust parser
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		RenderTargets[Index].FromString(Parts, FRenderTarget::NUM_STRING_FIELDS * Index);
	}
	LexFromString((int8&)bUseIndependentRenderTargetBlendStates, *Parts[MaxSimultaneousRenderTargets * FRenderTarget::NUM_STRING_FIELDS]);
}


FString FBlendStateInitializerRHI::FRenderTarget::ToString() const
{
	return FString::Printf(TEXT("%u %u %u %u %u %u %u ")
		, uint32(ColorBlendOp)
		, uint32(ColorSrcBlend)
		, uint32(ColorDestBlend)
		, uint32(AlphaBlendOp)
		, uint32(AlphaSrcBlend)
		, uint32(AlphaDestBlend)
		, uint32(ColorWriteMask)
	);
}

void FBlendStateInitializerRHI::FRenderTarget::FromString(const TArray<FString>& Parts, int32 Index)
{
	check(Index + NUM_STRING_FIELDS <= Parts.Num());
	LexFromString((uint8&)ColorBlendOp, *Parts[Index++]);
	LexFromString((uint8&)ColorSrcBlend, *Parts[Index++]);
	LexFromString((uint8&)ColorDestBlend, *Parts[Index++]);
	LexFromString((uint8&)AlphaBlendOp, *Parts[Index++]);
	LexFromString((uint8&)AlphaSrcBlend, *Parts[Index++]);
	LexFromString((uint8&)AlphaDestBlend, *Parts[Index++]);
	LexFromString((uint8&)ColorWriteMask, *Parts[Index++]);
}

bool FRHIResource::Bypass()
{
	return GRHICommandList.Bypass();
}

DECLARE_CYCLE_STAT(TEXT("Delete Resources"), STAT_DeleteResources, STATGROUP_RHICMDLIST);

void FRHIResource::FlushPendingDeletes(bool bFlushDeferredDeletes)
{
	SCOPE_CYCLE_COUNTER(STAT_DeleteResources);

	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	FRHICommandListExecutor::CheckNoOutstandingCmdLists();
	if (GDynamicRHI)
	{
		GDynamicRHI->RHIPerFrameRHIFlushComplete();
	}

	auto Delete = [](TArray<FRHIResource*>& ToDelete)
	{
		for (int32 Index = 0; Index < ToDelete.Num(); Index++)
		{
			FRHIResource* Ref = ToDelete[Index];
			check(Ref->MarkedForDelete == 1);
			if (Ref->GetRefCount() == 0) // caches can bring dead objects back to life
			{
				CurrentlyDeleting = Ref;
				delete Ref;
				CurrentlyDeleting = nullptr;
			}
			else
			{
				Ref->MarkedForDelete = 0;
				FPlatformMisc::MemoryBarrier();
			}
		}
	};

	while (1)
	{
		if (PendingDeletes.IsEmpty())
		{
			break;
		}
		if (PlatformNeedsExtraDeletionLatency())
		{
			const int32 Index = DeferredDeletionQueue.AddDefaulted();
			ResourcesToDelete& ResourceBatch = DeferredDeletionQueue[Index];
			ResourceBatch.FrameDeleted = CurrentFrame;
			PendingDeletes.PopAll(ResourceBatch.Resources);
			check(ResourceBatch.Resources.Num());
		}
		else
		{
			TArray<FRHIResource*> ToDelete;
			PendingDeletes.PopAll(ToDelete);
			check(ToDelete.Num());
			Delete(ToDelete);
		}
	}

#if PLATFORM_XBOXONE
	// Adding another frame of latency on Xbox. Speculative GPU crash fix.
	const uint32 NumFramesToExpire = 4;
#else
	const uint32 NumFramesToExpire = 3;
#endif

	if (DeferredDeletionQueue.Num())
	{
		if (bFlushDeferredDeletes)
		{
			FRHICommandListExecutor::GetImmediateCommandList().BlockUntilGPUIdle();

			for (int32 Idx = 0; Idx < DeferredDeletionQueue.Num(); ++Idx)
			{
				ResourcesToDelete& ResourceBatch = DeferredDeletionQueue[Idx];
				Delete(ResourceBatch.Resources);
			}

			DeferredDeletionQueue.Empty();
		}
		else
		{
			int32 DeletedBatchCount = 0;
			while (DeletedBatchCount < DeferredDeletionQueue.Num())
			{
				ResourcesToDelete& ResourceBatch = DeferredDeletionQueue[DeletedBatchCount];
				if (((ResourceBatch.FrameDeleted + NumFramesToExpire) < CurrentFrame) || !GIsRHIInitialized)
				{
					Delete(ResourceBatch.Resources);
					++DeletedBatchCount;
				}
				else
				{
					break;
				}
			}

			if (DeletedBatchCount)
			{
				DeferredDeletionQueue.RemoveAt(0, DeletedBatchCount);
			}
		}

		++CurrentFrame;
	}
}

static_assert(ERHIZBuffer::FarPlane != ERHIZBuffer::NearPlane, "Near and Far planes must be different!");
static_assert((int32)ERHIZBuffer::NearPlane == 0 || (int32)ERHIZBuffer::NearPlane == 1, "Invalid Values for Near Plane, can only be 0 or 1!");
static_assert((int32)ERHIZBuffer::FarPlane == 0 || (int32)ERHIZBuffer::FarPlane == 1, "Invalid Values for Far Plane, can only be 0 or 1");


/**
 * RHI configuration settings.
 */

static TAutoConsoleVariable<int32> ResourceTableCachingCvar(
	TEXT("rhi.ResourceTableCaching"),
	1,
	TEXT("If 1, the RHI will cache resource table contents within a frame. Otherwise resource tables are rebuilt for every draw call.")
	);
static TAutoConsoleVariable<int32> GSaveScreenshotAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.Screenshot"),
	1,
	TEXT("Whether a screenshot should be taken when profiling the GPU. 0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> GShowProfilerAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.ShowUI"),
	1,
	TEXT("Whether the user interface profiler should be displayed after profiling the GPU.\n")
	TEXT("The results will always go to the log/console\n")
	TEXT("0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> GGPUHitchThresholdCVar(
	TEXT("RHI.GPUHitchThreshold"),
	100.0f,
	TEXT("Threshold for detecting hitches on the GPU (in milliseconds).")
	);
static TAutoConsoleVariable<int32> GCVarRHIRenderPass(
	TEXT("r.RHIRenderPasses"),
	0,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarGPUCrashDebugging(
	TEXT("r.GPUCrashDebugging"),
	0,
	TEXT("Enable vendor specific GPU crash analysis tools"),
	ECVF_ReadOnly
	);


namespace RHIConfig
{
	bool ShouldSaveScreenshotAfterProfilingGPU()
	{
		return GSaveScreenshotAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	bool ShouldShowProfilerAfterProfilingGPU()
	{
		return GShowProfilerAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	float GetGPUHitchThreshold()
	{
		return GGPUHitchThresholdCVar.GetValueOnAnyThread() * 0.001f;
	}
}

/**
 * RHI globals.
 */

bool GIsRHIInitialized = false;
int32 GMaxTextureMipCount = MAX_TEXTURE_MIP_COUNT;
bool GRHISupportsCopyToTextureMultipleMips = false;
bool GSupportsQuadBufferStereo = false;
bool GSupportsDepthFetchDuringDepthTest = true;
FString GRHIAdapterName;
FString GRHIAdapterInternalDriverVersion;
FString GRHIAdapterUserDriverVersion;
FString GRHIAdapterDriverDate;
uint32 GRHIVendorId = 0;
uint32 GRHIDeviceId = 0;
uint32 GRHIDeviceRevision = 0;
bool GRHIDeviceIsAMDPreGCNArchitecture = false;
bool GSupportsRenderDepthTargetableShaderResources = true;
TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_G8(true);
TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_FloatRGBA(true);
bool GSupportsShaderFramebufferFetch = false;
bool GSupportsShaderDepthStencilFetch = false;
bool GSupportsTimestampRenderQueries = false;
bool GRHISupportsGPUTimestampBubblesRemoval = false;
bool GRHISupportsFrameCyclesBubblesRemoval = false;
bool GHardwareHiddenSurfaceRemoval = false;
bool GRHISupportsAsyncTextureCreation = false;
bool GRHISupportsQuadTopology = false;
bool GRHISupportsRectTopology = false;
bool GSupportsParallelRenderingTasksWithSeparateRHIThread = true;
bool GRHIThreadNeedsKicking = false;
int32 GRHIMaximumReccommendedOustandingOcclusionQueries = MAX_int32;
bool GRHISupportsExactOcclusionQueries = true;
bool GSupportsVolumeTextureRendering = true;
bool GSupportsSeparateRenderTargetBlendState = false;
bool GSupportsDepthRenderTargetWithoutColorRenderTarget = true;
bool GRHINeedsUnatlasedCSMDepthsWorkaround = false;
bool GSupportsTexture3D = true;
bool GSupportsMobileMultiView = false;
bool GSupportsImageExternal = false;
bool GSupportsResourceView = true;
TRHIGlobal<bool> GSupportsMultipleRenderTargets(true);
bool GSupportsWideMRT = true;
float GMinClipZ = 0.0f;
float GProjectionSignY = 1.0f;
bool GRHINeedsExtraDeletionLatency = false;
bool GRHIForceNoDeletionLatencyForStreamingTextures = false;
TRHIGlobal<int32> GMaxComputeDispatchDimension((1 << 16) - 1);
bool GRHILazyShaderCodeLoading = false;
bool GRHISupportsLazyShaderCodeLoading = false;
TRHIGlobal<int32> GMaxShadowDepthBufferSizeX(2048);
TRHIGlobal<int32> GMaxShadowDepthBufferSizeY(2048);
TRHIGlobal<int32> GMaxTextureDimensions(2048);
TRHIGlobal<int32> GMaxCubeTextureDimensions(2048);
int32 GMaxTextureArrayLayers = 256;
int32 GMaxTextureSamplers = 16;
bool GUsingNullRHI = false;
int32 GDrawUPVertexCheckCount = MAX_int32;
int32 GDrawUPIndexCheckCount = MAX_int32;
bool GTriggerGPUProfile = false;
FString GGPUTraceFileName;
bool GRHISupportsTextureStreaming = false;
bool GSupportsDepthBoundsTest = false;
bool GSupportsEfficientAsyncCompute = false;
bool GRHISupportsBaseVertexIndex = true;
TRHIGlobal<bool> GRHISupportsInstancing(true);
bool GRHISupportsFirstInstance = false;
bool GRHISupportsDynamicResolution = false;
bool GRHISupportsRayTracing = false;
bool GRHISupportsWaveOperations = false;
bool GRHISupportsRHIThread = false;
bool GRHISupportsRHIOnTaskThread = false;
bool GRHISupportsParallelRHIExecute = false;
bool GSupportsHDR32bppEncodeModeIntrinsic = false;
bool GSupportsParallelOcclusionQueries = false;
bool GSupportsRenderTargetWriteMask = false;
bool GSupportsTransientResourceAliasing = false;
bool GRHIRequiresRenderTargetForPixelShaderUAVs = false;

bool GRHISupportsMSAADepthSampleAccess = false;
bool GRHISupportsResolveCubemapFaces = false;

bool GRHIIsHDREnabled = false;
bool GRHISupportsHDROutput = false;
EPixelFormat GRHIHDRDisplayOutputFormat = PF_FloatRGBA;

uint64 GRHIPresentCounter = 1;

/** Whether we are profiling GPU hitches. */
bool GTriggerGPUHitchProfile = false;

FVertexElementTypeSupportInfo GVertexElementTypeSupport;

RHI_API int32 volatile GCurrentTextureMemorySize = 0;
RHI_API int32 volatile GCurrentRendertargetMemorySize = 0;
RHI_API int64 GTexturePoolSize = 0 * 1024 * 1024;
RHI_API int32 GPoolSizeVRAMPercentage = 0;

RHI_API EShaderPlatform GShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num] = {SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms};

// simple stats about draw calls. GNum is the previous frame and 
// GCurrent is the current frame.
RHI_API int32 GCurrentNumDrawCallsRHI = 0;
RHI_API int32 GNumDrawCallsRHI = 0;
RHI_API int32 GCurrentNumPrimitivesDrawnRHI = 0;
RHI_API int32 GNumPrimitivesDrawnRHI = 0;

/** Called once per frame only from within an RHI. */
void RHIPrivateBeginFrame()
{
	GNumDrawCallsRHI = GCurrentNumDrawCallsRHI;
	GNumPrimitivesDrawnRHI = GCurrentNumPrimitivesDrawnRHI;
	CSV_CUSTOM_STAT(RHI, DrawCalls, GNumDrawCallsRHI, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RHI, PrimitivesDrawn, GNumPrimitivesDrawnRHI, ECsvCustomStatOp::Set);
	GCurrentNumDrawCallsRHI = GCurrentNumPrimitivesDrawnRHI = 0;
}

/** Whether to initialize 3D textures using a bulk data (or through a mip update if false). */
RHI_API bool GUseTexture3DBulkDataRHI = false;

//
// The current shader platform.
//

RHI_API EShaderPlatform GMaxRHIShaderPlatform = SP_PCD3D_SM5;

/** The maximum feature level supported on this machine */
RHI_API ERHIFeatureLevel::Type GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;

FName FeatureLevelNames[] = 
{
	FName(TEXT("ES2")),
	FName(TEXT("ES3_1")),
	FName(TEXT("SM4")),
	FName(TEXT("SM5")),
};

static_assert(ARRAY_COUNT(FeatureLevelNames) == ERHIFeatureLevel::Num, "Missing entry from feature level names.");

RHI_API bool GetFeatureLevelFromName(FName Name, ERHIFeatureLevel::Type& OutFeatureLevel)
{
	for (int32 NameIndex = 0; NameIndex < ARRAY_COUNT(FeatureLevelNames); NameIndex++)
	{
		if (FeatureLevelNames[NameIndex] == Name)
		{
			OutFeatureLevel = (ERHIFeatureLevel::Type)NameIndex;
			return true;
		}
	}

	OutFeatureLevel = ERHIFeatureLevel::Num;
	return false;
}

RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FString& OutName)
{
	check(InFeatureLevel < ARRAY_COUNT(FeatureLevelNames));
	if (InFeatureLevel < ARRAY_COUNT(FeatureLevelNames))
	{
		FeatureLevelNames[(int32)InFeatureLevel].ToString(OutName);
	}
	else
	{
		OutName = TEXT("InvalidFeatureLevel");
	}	
}

static FName InvalidFeatureLevelName(TEXT("InvalidFeatureLevel"));
RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FName& OutName)
{
	check(InFeatureLevel < ARRAY_COUNT(FeatureLevelNames));
	if (InFeatureLevel < ARRAY_COUNT(FeatureLevelNames))
	{
		OutName = FeatureLevelNames[(int32)InFeatureLevel];
	}
	else
	{
		
		OutName = InvalidFeatureLevelName;
	}
}

FName ShadingPathNames[] =
{
	FName(TEXT("Deferred")),
	FName(TEXT("Forward")),
	FName(TEXT("Mobile")),
};

static_assert(ARRAY_COUNT(ShadingPathNames) == ERHIShadingPath::Num, "Missing entry from shading path names.");

RHI_API bool GetShadingPathFromName(FName Name, ERHIShadingPath::Type& OutShadingPath)
{
	for (int32 NameIndex = 0; NameIndex < ARRAY_COUNT(ShadingPathNames); NameIndex++)
	{
		if (ShadingPathNames[NameIndex] == Name)
		{
			OutShadingPath = (ERHIShadingPath::Type)NameIndex;
			return true;
		}
	}

	OutShadingPath = ERHIShadingPath::Num;
	return false;
}

RHI_API void GetShadingPathName(ERHIShadingPath::Type InShadingPath, FString& OutName)
{
	check(InShadingPath < ARRAY_COUNT(ShadingPathNames));
	if (InShadingPath < ARRAY_COUNT(ShadingPathNames))
	{
		ShadingPathNames[(int32)InShadingPath].ToString(OutName);
	}
	else
	{
		OutName = TEXT("InvalidShadingPath");
	}
}

static FName InvalidShadingPathName(TEXT("InvalidShadingPath"));
RHI_API void GetShadingPathName(ERHIShadingPath::Type InShadingPath, FName& OutName)
{
	check(InShadingPath < ARRAY_COUNT(ShadingPathNames));
	if (InShadingPath < ARRAY_COUNT(ShadingPathNames))
	{
		OutName = ShadingPathNames[(int32)InShadingPath];
	}
	else
	{

		OutName = InvalidShadingPathName;
	}
}

static FName NAME_PLATFORM_WINDOWS(TEXT("Windows"));
static FName NAME_PLATFORM_PS4(TEXT("PS4"));
static FName NAME_PLATFORM_XBOXONE(TEXT("XboxOne"));
static FName NAME_PLATFORM_ANDROID(TEXT("Android"));
static FName NAME_PLATFORM_IOS(TEXT("IOS"));
static FName NAME_PLATFORM_MAC(TEXT("Mac"));
static FName NAME_PLATFORM_SWITCH(TEXT("Switch"));
static FName NAME_PLATFORM_TVOS(TEXT("TVOS"));

FName ShaderPlatformToPlatformName(EShaderPlatform Platform)
{
	switch(Platform)
	{
	case SP_PCD3D_SM4:
	case SP_PCD3D_SM5:
		return NAME_PLATFORM_WINDOWS;
	case SP_PS4:
		return NAME_PLATFORM_PS4;
	case SP_XBOXONE_D3D12:
		return NAME_PLATFORM_XBOXONE;
	case SP_OPENGL_ES3_1_ANDROID:
	case SP_VULKAN_ES3_1_ANDROID:
		return NAME_PLATFORM_ANDROID;
	case SP_METAL:
	case SP_METAL_MRT:
        return NAME_PLATFORM_IOS;
	case SP_METAL_TVOS:
	case SP_METAL_MRT_TVOS:
		return NAME_PLATFORM_TVOS;
	case SP_METAL_SM5:
	case SP_METAL_SM5_NOTESS:
	case SP_METAL_MACES3_1:
	case SP_METAL_MACES2:
	case SP_METAL_MRT_MAC:
		return NAME_PLATFORM_MAC;
	case SP_SWITCH:
	case SP_SWITCH_FORWARD:
		return NAME_PLATFORM_SWITCH;

	default:
		return FName();
	}
}

FName LegacyShaderPlatformToShaderFormat(EShaderPlatform Platform)
{
	return ShaderPlatformToShaderFormatName(Platform);
}

EShaderPlatform ShaderFormatToLegacyShaderPlatform(FName ShaderFormat)
{
	return ShaderFormatNameToShaderPlatform(ShaderFormat);
}

RHI_API bool IsRHIDeviceAMD()
{
	check(GRHIVendorId != 0);
	// AMD's drivers tested on July 11 2013 have hitching problems with async resource streaming, setting single threaded for now until fixed.
	return GRHIVendorId == 0x1002;
}

RHI_API bool IsRHIDeviceIntel()
{
	check(GRHIVendorId != 0);
	// Intel GPUs are integrated and use both DedicatedVideoMemory and SharedSystemMemory.
	return GRHIVendorId == 0x8086;
}

RHI_API bool IsRHIDeviceNVIDIA()
{
	check(GRHIVendorId != 0);
	// NVIDIA GPUs are discrete and use DedicatedVideoMemory only.
	return GRHIVendorId == 0x10DE;
}

RHI_API const TCHAR* RHIVendorIdToString()
{
	switch (GRHIVendorId)
	{
	case 0x1002: return TEXT("AMD");
	case 0x1010: return TEXT("ImgTec");
	case 0x10DE: return TEXT("NVIDIA");
	case 0x13B5: return TEXT("ARM");
	case 0x5143: return TEXT("Qualcomm");
	case 0x8086: return TEXT("Intel");
	default: return TEXT("Unknown");
	}
}

RHI_API uint32 RHIGetShaderLanguageVersion(const EShaderPlatform Platform)
{
	uint32 Version = 0;
	if (IsMetalPlatform(Platform))
	{
		if (IsPCPlatform(Platform))
		{
			static int32 MaxShaderVersion = -1;
			if (MaxShaderVersion < 0)
			{
				MaxShaderVersion = 2;
				int32 MinShaderVersion = 3;
				if(!GConfig->GetInt(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("MaxShaderLanguageVersion"), MaxShaderVersion, GEngineIni))
				{
					MaxShaderVersion = 3;
				}
				MaxShaderVersion = FMath::Max(MinShaderVersion, MaxShaderVersion);
			}
			Version = (uint32)MaxShaderVersion;
		}
		else
		{
			static int32 MaxShaderVersion = -1;
			if (MaxShaderVersion < 0)
			{
				MaxShaderVersion = 2;
				int32 MinShaderVersion = 2;
				if(!GConfig->GetInt(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("MaxShaderLanguageVersion"), MaxShaderVersion, GEngineIni))
				{
					MaxShaderVersion = 0;
				}
				MaxShaderVersion = FMath::Max(MinShaderVersion, MaxShaderVersion);
			}
			Version = (uint32)MaxShaderVersion;
		}
	}
	return Version;
}

RHI_API bool RHISupportsTessellation(const EShaderPlatform Platform)
{
	if (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5))
	{
		return (Platform == SP_PCD3D_SM5) || (Platform == SP_XBOXONE_D3D12) || (Platform == SP_OPENGL_SM5) || (Platform == SP_OPENGL_ES31_EXT) || (Platform == SP_METAL_SM5) || (IsVulkanSM5Platform(Platform) && CVarVulkanEnableTessellation->GetInt() != 0);
	}
	return false;
}

RHI_API bool RHISupportsPixelShaderUAVs(const EShaderPlatform Platform)
{
	if (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5))
	{
		return true;
	}
	return false;
}

RHI_API bool RHISupportsIndexBufferUAVs(const EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || IsVulkanPlatform(Platform) || IsMetalSM5Platform(Platform) || Platform == SP_XBOXONE_D3D12 || Platform == SP_PS4;
}


static ERHIFeatureLevel::Type GRHIMobilePreviewFeatureLevel = ERHIFeatureLevel::Num;
RHI_API void RHISetMobilePreviewFeatureLevel(ERHIFeatureLevel::Type MobilePreviewFeatureLevel)
{
	check(GRHIMobilePreviewFeatureLevel == ERHIFeatureLevel::Num);
	check(!GIsEditor);
	GRHIMobilePreviewFeatureLevel = MobilePreviewFeatureLevel;
}

bool RHIGetPreviewFeatureLevel(ERHIFeatureLevel::Type& PreviewFeatureLevelOUT)
{
	static bool bForceFeatureLevelES2 = !GIsEditor && FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES2"));
	static bool bForceFeatureLevelES3_1 = !GIsEditor && (FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")));

	if (bForceFeatureLevelES2)
	{
		PreviewFeatureLevelOUT = ERHIFeatureLevel::ES2;
	}
	else if (bForceFeatureLevelES3_1)
	{
		PreviewFeatureLevelOUT = ERHIFeatureLevel::ES3_1;
	}
	else if (!GIsEditor && GRHIMobilePreviewFeatureLevel != ERHIFeatureLevel::Num)
	{
		PreviewFeatureLevelOUT = GRHIMobilePreviewFeatureLevel;
	}
	else
	{
		return false;
	}
	return true;
}

void FRHIRenderPassInfo::ConvertToRenderTargetsInfo(FRHISetRenderTargetsInfo& OutRTInfo) const
{
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		if (!ColorRenderTargets[Index].RenderTarget)
		{
			break;
		}

		OutRTInfo.ColorRenderTarget[Index].Texture = ColorRenderTargets[Index].RenderTarget;
		ERenderTargetLoadAction LoadAction = GetLoadAction(ColorRenderTargets[Index].Action);
		OutRTInfo.ColorRenderTarget[Index].LoadAction = LoadAction;
		OutRTInfo.ColorRenderTarget[Index].StoreAction = GetStoreAction(ColorRenderTargets[Index].Action);
		OutRTInfo.ColorRenderTarget[Index].ArraySliceIndex = ColorRenderTargets[Index].ArraySlice;
		OutRTInfo.ColorRenderTarget[Index].MipIndex = ColorRenderTargets[Index].MipIndex;
		++OutRTInfo.NumColorRenderTargets;

		OutRTInfo.bClearColor |= (LoadAction == ERenderTargetLoadAction::EClear);
	}

	ERenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
	ERenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
	ERenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
	ERenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);
	ERenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
	ERenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);

	OutRTInfo.DepthStencilRenderTarget = FRHIDepthRenderTargetView(DepthStencilRenderTarget.DepthStencilTarget,
		DepthLoadAction,
		GetStoreAction(DepthActions),
		StencilLoadAction,
		GetStoreAction(StencilActions),
		DepthStencilRenderTarget.ExclusiveDepthStencil);
	OutRTInfo.bClearDepth = (DepthLoadAction == ERenderTargetLoadAction::EClear);
	OutRTInfo.bClearStencil = (StencilLoadAction == ERenderTargetLoadAction::EClear);

	if (NumUAVs > 0)
	{
		check(UAVIndex != -1);
		int32 StartingUAVIndex = FMath::Max(UAVIndex, OutRTInfo.NumColorRenderTargets);
		check((StartingUAVIndex+NumUAVs) <= MaxSimultaneousUAVs);
		OutRTInfo.NumColorRenderTargets = StartingUAVIndex;
		for (int32 Index = 0; Index < NumUAVs; ++Index)
		{
			OutRTInfo.UnorderedAccessView[Index] = UAVs[Index];
		}
		OutRTInfo.NumUAVs = NumUAVs;
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FRHIRenderPassInfo::Validate() const
{
	int32 NumSamples = -1;	// -1 means nothing found yet
	int32 ColorIndex = 0;
	for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
	{
		const FColorEntry& Entry = ColorRenderTargets[ColorIndex];
		if (Entry.RenderTarget)
		{
			// Ensure NumSamples matches amongst all color RTs
			if (NumSamples == -1)
			{
				NumSamples = Entry.RenderTarget->GetNumSamples();
			}
			else
			{
				ensure(Entry.RenderTarget->GetNumSamples() == NumSamples);
			}

			ERenderTargetStoreAction Store = GetStoreAction(Entry.Action);
			// Don't try to resolve a non-msaa
			ensure(Store != ERenderTargetStoreAction::EMultisampleResolve || Entry.RenderTarget->GetNumSamples() > 1);
			// Don't resolve to null
			ensure(Store != ERenderTargetStoreAction::EMultisampleResolve || Entry.ResolveTarget);

			if (Entry.ResolveTarget)
			{
				//ensure(Store == ERenderTargetStoreAction::EMultisampleResolve);
			}
		}
		else
		{
			break;
		}
	}

	int32 NumColorRenderTargets = ColorIndex;
	for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
	{
		// Gap in the sequence of valid render targets (ie RT0, null, RT2, ...)
		ensureMsgf(!ColorRenderTargets[ColorIndex].RenderTarget, TEXT("Missing color render target on slot %d"), ColorIndex - 1);
	}

	if (bGeneratingMips)
	{
		if (NumColorRenderTargets == 0)
		{
			ensureMsgf(0, TEXT("Missing color render target for which to generate mips!"));
		}

		for (int32 Index = 1; Index < NumColorRenderTargets; ++Index)
		{
			ensureMsgf(ColorRenderTargets[0].RenderTarget->GetSizeXYZ() == ColorRenderTargets[Index].RenderTarget->GetSizeXYZ(), TEXT("Color Render Targets must all have the same dimensions for generating mips!"));
		}		
	}

	if (DepthStencilRenderTarget.DepthStencilTarget)
	{
		// Ensure NumSamples matches with color RT
		if (NumSamples != -1)
		{
			ensure(DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() == NumSamples);
		}
		ERenderTargetStoreAction DepthStore = GetStoreAction(GetDepthActions(DepthStencilRenderTarget.Action));
		ERenderTargetStoreAction StencilStore = GetStoreAction(GetStencilActions(DepthStencilRenderTarget.Action));
		bool bIsMSAAResolve = (DepthStore == ERenderTargetStoreAction::EMultisampleResolve) || (StencilStore == ERenderTargetStoreAction::EMultisampleResolve);
		// Don't try to resolve a non-msaa
		ensure(!bIsMSAAResolve || DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() > 1);
		// Don't resolve to null
		//ensure(DepthStencilRenderTarget.ResolveTarget || DepthStore != ERenderTargetStoreAction::EStore);

		// Don't write to depth if read-only
		//ensure(DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite() || DepthStore != ERenderTargetStoreAction::EStore);
		// This is not true for stencil. VK and Metal specify that the DontCare store action MAY leave the attachment in an undefined state.
		/*ensure(DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite() || StencilStore != ERenderTargetStoreAction::EStore);*/

		// If we have a depthstencil target we MUST Store it or it will be undefined after rendering.
		if (DepthStencilRenderTarget.DepthStencilTarget->GetFormat() != PF_D24)
		{
			// If this is DepthStencil we must store it out unless we are absolutely sure it will never be used again.
			// it is valid to use a depthbuffer for performance and not need the results later.
			//ensure(StencilStore == ERenderTargetStoreAction::EStore);
		}

		if (DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite())
		{
			// this check is incorrect for mobile, depth/stencil is intermediate and we don't want to store it to main memory
			//ensure(DepthStore == ERenderTargetStoreAction::EStore);
		}

		if (DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite())
		{
			// this check is incorrect for mobile, depth/stencil is intermediate and we don't want to store it to main memory
			//ensure(StencilStore == ERenderTargetStoreAction::EStore);
		}
	}
	else
	{
		ensure(DepthStencilRenderTarget.Action == EDepthStencilTargetActions::DontLoad_DontStore);
		ensure(DepthStencilRenderTarget.ExclusiveDepthStencil == FExclusiveDepthStencil::DepthNop_StencilNop);
	}
}
#endif

static FRHIPanicEvent RHIPanicEvent;
FRHIPanicEvent& RHIGetPanicDelegate()
{
	return RHIPanicEvent;
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHI.cpp: Metal device RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#if PLATFORM_IOS
#include "IOS/IOSAppDelegate.h"
#elif PLATFORM_MAC
#include "Mac/MacApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#endif
#include "MetalProfiler.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "MetalShaderResources.h"
#include "MetalLLM.h"

DEFINE_LOG_CATEGORY(LogMetal)

bool GIsMetalInitialized = false;
bool GMetalSupportsHeaps = false;
bool GMetalSupportsIndirectArgumentBuffers = false;
bool GMetalSupportsTileShaders = false;
bool GMetalSupportsStoreActionOptions = false;
bool GMetalSupportsDepthClipMode = false;
bool GMetalCommandBufferHasStartEndTimeAPI = false;

FMetalBufferFormat GMetalBufferFormats[PF_MAX];

static void ValidateTargetedRHIFeatureLevelExists(EShaderPlatform Platform)
{
	bool bSupportsShaderPlatform = false;
#if PLATFORM_MAC
	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
	
	for (FString Name : TargetedShaderFormats)
	{
		FName ShaderFormatName(*Name);
		if (ShaderFormatToLegacyShaderPlatform(ShaderFormatName) == Platform)
		{
			bSupportsShaderPlatform = true;
			break;
		}
	}
#else
	if (Platform == SP_METAL || Platform == SP_METAL_TVOS)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsShaderPlatform, GEngineIni);
	}
	else if (Platform == SP_METAL_MRT || Platform == SP_METAL_MRT_TVOS)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsShaderPlatform, GEngineIni);
	}
#endif
	
	if (!bSupportsShaderPlatform && !WITH_EDITOR)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderPlatform"), FText::FromString(LegacyShaderPlatformToShaderFormat(Platform).ToString()));
		FText LocalizedMsg = FText::Format(NSLOCTEXT("MetalRHI", "ShaderPlatformUnavailable","Shader platform: {ShaderPlatform} was not cooked! Please enable this shader platform in the project's target settings."),Args);
		
		FText Title = NSLOCTEXT("MetalRHI", "ShaderPlatformUnavailableTitle","Shader Platform Unavailable");
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, &Title);
		FPlatformMisc::RequestExit(true);
		
		UE_LOG(LogMetal, Fatal, TEXT("Shader platform: %s was not cooked! Please enable this shader platform in the project's target settings."), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	}
}

bool FMetalDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FMetalDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
	LLM(MetalLLM::Initialise());
	return new FMetalDynamicRHI(RequestedFeatureLevel);
}

IMPLEMENT_MODULE(FMetalDynamicRHIModule, MetalRHI);

FMetalDynamicRHI::FMetalDynamicRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
: ImmediateContext(nullptr, FMetalDeviceContext::CreateDeviceContext())
, AsyncComputeContext(nullptr)
{
	@autoreleasepool {
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );
	
	// @todo Zebra This is now supported on all GPUs in Mac Metal, but not on iOS.
	// we cannot render to a volume texture without geometry shader support
	GSupportsVolumeTextureRendering = false;
	
	// Metal always needs a render target to render with fragment shaders!
	// GRHIRequiresRenderTargetForPixelShaderUAVs = true;

	//@todo-rco: Query name from API
	GRHIAdapterName = TEXT("Metal");
	GRHIVendorId = 1; // non-zero to avoid asserts
	
	bool const bRequestedFeatureLevel = (RequestedFeatureLevel != ERHIFeatureLevel::Num);
	bool bSupportsPointLights = false;
	bool bSupportsRHIThread = false;
	
	// get the device to ask about capabilities?
	mtlpp::Device Device = ImmediateContext.Context->GetDevice();
		
#if PLATFORM_IOS
	// A8 can use 256 bits of MRTs
#if PLATFORM_TVOS
	bool bCanUseWideMRTs = true;
	bool bCanUseASTC = true;
#else
	bool bCanUseWideMRTs = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1];
	bool bCanUseASTC = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] && !FParse::Param(FCommandLine::Get(),TEXT("noastc"));
	
	const mtlpp::FeatureSet FeatureSets[] = {
		mtlpp::FeatureSet::iOS_GPUFamily1_v1,
		mtlpp::FeatureSet::iOS_GPUFamily2_v1,
		mtlpp::FeatureSet::iOS_GPUFamily3_v1,
		mtlpp::FeatureSet::iOS_GPUFamily4_v1
	};
		
	const uint8 FeatureSetVersions[][3] = {
		{8, 0, 0},
		{8, 3, 0},
		{10, 0, 0},
		{11, 0, 0}
	};
	
	GRHIDeviceId = 0;
	for (uint32 i = 0; i < 4; i++)
	{
		if (FPlatformMisc::IOSVersionCompare(FeatureSetVersions[i][0],FeatureSetVersions[i][1],FeatureSetVersions[i][2]) >= 0 && Device.SupportsFeatureSet(FeatureSets[i]))
		{
			GRHIDeviceId++;
		}
	}
#endif

    bool bProjectSupportsMRTs = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectSupportsMRTs, GEngineIni);
	
	bool const bRequestedMetalMRT = ((RequestedFeatureLevel >= ERHIFeatureLevel::SM4) || (!bRequestedFeatureLevel && FParse::Param(FCommandLine::Get(),TEXT("metalmrt"))));
	
    // only allow GBuffers, etc on A8s (A7s are just not going to cut it)
    if (bProjectSupportsMRTs && bCanUseWideMRTs && bRequestedMetalMRT)
    {
#if PLATFORM_TVOS
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_MRT);
		GMaxRHIShaderPlatform = SP_METAL_MRT_TVOS;
#else
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_MRT);
        GMaxRHIShaderPlatform = SP_METAL_MRT;
#endif
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		
		bSupportsRHIThread = FParse::Param(FCommandLine::Get(),TEXT("rhithread"));
    }
    else
	{
		if (bRequestedMetalMRT)
		{
			UE_LOG(LogMetal, Warning, TEXT("Metal MRT support requires an iOS or tvOS device with an A8 processor or later. Falling back to Metal ES 3.1."));
		}
		
#if PLATFORM_TVOS
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_TVOS);
		GMaxRHIShaderPlatform = SP_METAL_TVOS;
#else
		ValidateTargetedRHIFeatureLevelExists(SP_METAL);
		GMaxRHIShaderPlatform = SP_METAL;
#endif
        GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
	}
		
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		
	MemoryStats.DedicatedVideoMemory = 0;
	MemoryStats.TotalGraphicsMemory = Stats.AvailablePhysical;
	MemoryStats.DedicatedSystemMemory = 0;
	MemoryStats.SharedSystemMemory = Stats.AvailablePhysical;
	
#if PLATFORM_TVOS
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL_TVOS;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL_TVOS;
#else
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL;
#endif
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) ? GMaxRHIShaderPlatform : SP_NumPlatforms;

#else // @todo zebra
	uint32 DeviceIndex = ((FMetalDeviceContext*)ImmediateContext.Context)->GetDeviceIndex();
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(DeviceIndex < GPUs.Num());
	FMacPlatformMisc::FGPUDescriptor const& GPUDesc = GPUs[DeviceIndex];
	
    // A8 can use 256 bits of MRTs
    bool bCanUseWideMRTs = true;
    bool bCanUseASTC = false;
	bool bSupportsD24S8 = false;
	bool bSupportsD16 = false;
	
	GRHIAdapterName = FString(Device.GetName());
	
	// However they don't all support other features depending on the version of the OS.
	bool bSupportsTiledReflections = false;
	bool bSupportsDistanceFields = false;
	
	// Default is SM5 on:
	// 10.11.6 for AMD/Nvidia
	// 10.12.2+ for AMD/Nvidia
	// 10.12.4+ for Intel
	bool bSupportsSM5 = true;
	bool bIsIntelHaswell = false;
	if(GRHIAdapterName.Contains("Nvidia"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x10DE;
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		bSupportsRHIThread = (FPlatformMisc::MacOSXVersionCompare(10,12,0) >= 0);
	}
	else if(GRHIAdapterName.Contains("ATi") || GRHIAdapterName.Contains("AMD"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x1002;
		if((FPlatformMisc::MacOSXVersionCompare(10,12,0) < 0) && GPUDesc.GPUVendorId == GRHIVendorId)
		{
			GRHIAdapterName = FString(GPUDesc.GPUName);
		}
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		bSupportsRHIThread = true;
	}
	else if(GRHIAdapterName.Contains("Intel"))
	{
		bSupportsTiledReflections = false;
		bSupportsPointLights = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		GRHIVendorId = 0x8086;
		bSupportsRHIThread = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,12,2) >= 0);
		bIsIntelHaswell = (GRHIAdapterName == TEXT("Intel HD Graphics 5000") || GRHIAdapterName == TEXT("Intel Iris Graphics") || GRHIAdapterName == TEXT("Intel Iris Pro Graphics"));
	}

	bool const bRequestedSM5 = (RequestedFeatureLevel == ERHIFeatureLevel::SM5 || (!bRequestedFeatureLevel && (FParse::Param(FCommandLine::Get(),TEXT("metalsm5")) || FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))));
	if(bSupportsSM5 && bRequestedSM5)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		if (!FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
		{
			GMaxRHIShaderPlatform = SP_METAL_SM5;
		}
		else
		{
			GMaxRHIShaderPlatform = SP_METAL_MRT_MAC;
		}
	}
	else
	{
		if (bRequestedSM5)
		{
			UE_LOG(LogMetal, Warning, TEXT("Metal Shader Model 5 w/tessellation support requires 10.12.6 for Nvidia, it is broken on 10.13.0+. Falling back to Metal Shader Model 5 without tessellation support."));
		}
	
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		GMaxRHIShaderPlatform = SP_METAL_SM5_NOTESS;
	}

	ERHIFeatureLevel::Type PreviewFeatureLevel;
	if (RHIGetPreviewFeatureLevel(PreviewFeatureLevel))
	{
		check(PreviewFeatureLevel == ERHIFeatureLevel::ES2 || PreviewFeatureLevel == ERHIFeatureLevel::ES3_1);

		// ES2/3.1 feature level emulation
		GMaxRHIFeatureLevel = PreviewFeatureLevel;
		if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
		{
			GMaxRHIShaderPlatform = SP_METAL_MACES2;
		}
		else if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			GMaxRHIShaderPlatform = SP_METAL_MACES3_1;
		}
	}

	ValidateTargetedRHIFeatureLevelExists(GMaxRHIShaderPlatform);
	
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL_MACES2;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::ES3_1) ? SP_METAL_MACES3_1 : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	
	// Mac GPUs support layer indexing.
	GSupportsVolumeTextureRendering = (GMaxRHIShaderPlatform != SP_METAL_MRT_MAC);
	bSupportsPointLights &= (GMaxRHIShaderPlatform != SP_METAL_MRT_MAC);
	
	// Make sure the vendors match - the assumption that order in IORegistry is the order in Metal may not hold up forever.
	if(GPUDesc.GPUVendorId == GRHIVendorId)
	{
		GRHIDeviceId = GPUDesc.GPUDeviceId;
		MemoryStats.DedicatedVideoMemory = (int64)GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.TotalGraphicsMemory = (int64)GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.DedicatedSystemMemory = 0;
		MemoryStats.SharedSystemMemory = 0;
	}
	
	// Change the support depth format if we can
	bSupportsD24S8 = Device.IsDepth24Stencil8PixelFormatSupported();
	
	// Disable tiled reflections on Mac Metal for some GPU drivers that ignore the lod-level and so render incorrectly.
	if (!bSupportsTiledReflections && !FParse::Param(FCommandLine::Get(),TEXT("metaltiledreflections")))
	{
		static auto CVarDoTiledReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DoTiledReflections"));
		if(CVarDoTiledReflections && CVarDoTiledReflections->GetInt() != 0)
		{
			CVarDoTiledReflections->Set(0);
		}
	}
	
	// Disable the distance field AO & shadowing effects on GPU drivers that don't currently execute the shaders correctly.
	if ((GMaxRHIShaderPlatform == SP_METAL_SM5 || GMaxRHIShaderPlatform == SP_METAL_SM5_NOTESS) && !bSupportsDistanceFields && !FParse::Param(FCommandLine::Get(),TEXT("metaldistancefields")))
	{
		static auto CVarDistanceFieldAO = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldAO"));
		if(CVarDistanceFieldAO && CVarDistanceFieldAO->GetInt() != 0)
		{
			CVarDistanceFieldAO->Set(0);
		}
		
		static auto CVarDistanceFieldShadowing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldShadowing"));
		if(CVarDistanceFieldShadowing && CVarDistanceFieldShadowing->GetInt() != 0)
		{
			CVarDistanceFieldShadowing->Set(0);
		}
	}
	
#endif

	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 13, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsIndirectArgumentBuffers = true;
		GMetalSupportsStoreActionOptions = true;
	}
	if (!PLATFORM_MAC && FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){0, 0, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsTileShaders = true;
	}
	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 11, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsDepthClipMode = true;
	}
	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 13, 0}, (uint32[]){10, 3, 0}, (uint32[]){10, 3, 0}))
	{
		GMetalCommandBufferHasStartEndTimeAPI = true;
	}
		
	if(
	   #if PLATFORM_MAC
	   (Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v3) && FPlatformMisc::MacOSXVersionCompare(10,13,0) >= 0)
	   #elif PLATFORM_IOS || PLATFORM_TVOS
	   FPlatformMisc::IOSVersionCompare(10,3,0)
	   #endif
	   )
	{
		GRHISupportsDynamicResolution = true;
		GRHISupportsFrameCyclesBubblesRemoval = true;
	}

	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);
	if ( GPoolSizeVRAMPercentage > 0 && MemoryStats.TotalGraphicsMemory > 0 )
	{
		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(MemoryStats.TotalGraphicsMemory);
		
		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;
		
		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
			   GTexturePoolSize / 1024 / 1024,
			   GPoolSizeVRAMPercentage,
			   MemoryStats.TotalGraphicsMemory / 1024 / 1024);
	}
	else
	{
		static const auto CVarStreamingTexturePoolSize = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.PoolSize"));
		GTexturePoolSize = (int64)CVarStreamingTexturePoolSize->GetValueOnAnyThread() * 1024 * 1024;

		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (of %llu MB total graphics mem)"),
			   GTexturePoolSize / 1024 / 1024,
			   MemoryStats.TotalGraphicsMemory / 1024 / 1024);
	}

	GRHISupportsRHIThread = false;
	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
#if METAL_SUPPORTS_PARALLEL_RHI_EXECUTE
#if WITH_EDITORONLY_DATA
		GRHISupportsRHIThread = (!GIsEditor && bSupportsRHIThread);
#else
		GRHISupportsRHIThread = bSupportsRHIThread;
#endif
		GRHISupportsParallelRHIExecute = GRHISupportsRHIThread;
#endif
		GSupportsEfficientAsyncCompute = GRHISupportsParallelRHIExecute && (IsRHIDeviceAMD() || PLATFORM_IOS); // Only AMD currently support async. compute and it requires parallel execution to be useful.
		GSupportsParallelOcclusionQueries = GRHISupportsRHIThread;
		
		// We must always use an intermediate back-buffer for the RHI thread to work properly at present.
		if(GRHISupportsRHIThread)
		{
			static auto CVarSupportsIntermediateBackBuffer = IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.Metal.SupportsIntermediateBackBuffer"));
			if(CVarSupportsIntermediateBackBuffer && CVarSupportsIntermediateBackBuffer->GetInt() != 1)
			{
				CVarSupportsIntermediateBackBuffer->Set(1);
			}
		}
	}
	else
	{
		GRHISupportsParallelRHIExecute = false;
		GSupportsEfficientAsyncCompute = false;
		GSupportsParallelOcclusionQueries = false;
	}

	if (FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG)
	{
#if PLATFORM_IOS // @todo zebra : needs a RENDER_API or whatever
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
#endif
		SetEmitDrawEvents(true);
	}
	
	// Force disable vertex-shader-layer point light rendering on GPUs that don't support it properly yet.
	if(!bSupportsPointLights && !FParse::Param(FCommandLine::Get(),TEXT("metalpointlights")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarCubemapShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowPointLightCubemapShadows"));
		if(CVarCubemapShadows && CVarCubemapShadows->GetInt() != 0)
		{
			CVarCubemapShadows->Set(0);
		}
	}
	
	if (!GSupportsVolumeTextureRendering && !FParse::Param(FCommandLine::Get(),TEXT("metaltlv")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarTranslucentLightingVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TranslucentLightingVolume"));
		if(CVarTranslucentLightingVolume && CVarTranslucentLightingVolume->GetInt() != 0)
		{
			CVarTranslucentLightingVolume->Set(0);
		}
	}

#if PLATFORM_MAC
	if (IsRHIDeviceIntel() && FPlatformMisc::MacOSXVersionCompare(10,13,5) < 0)
	{
		static auto CVarSGShadowQuality = IConsoleManager::Get().FindConsoleVariable((TEXT("sg.ShadowQuality")));
		if (CVarSGShadowQuality && CVarSGShadowQuality->GetInt() != 0)
		{
			CVarSGShadowQuality->Set(0);
		}
	}

	if (bIsIntelHaswell)
	{
		static auto CVarForceDisableVideoPlayback = IConsoleManager::Get().FindConsoleVariable((TEXT("Fort.ForceDisableVideoPlayback")));
		if (CVarForceDisableVideoPlayback && CVarForceDisableVideoPlayback->GetInt() != 1)
		{
			CVarForceDisableVideoPlayback->Set(1);
		}
	}
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	// we don't want to auto-enable draw events in Test
	SetEmitDrawEvents(GetEmitDrawEvents() | ENABLE_METAL_GPUEVENTS);
#endif

	GSupportsShaderFramebufferFetch = !PLATFORM_MAC;
	GHardwareHiddenSurfaceRemoval = true;
	GSupportsRenderTargetFormat_PF_G8 = false;
	GRHISupportsTextureStreaming = true;
	GSupportsWideMRT = bCanUseWideMRTs;

	GSupportsSeparateRenderTargetBlendState = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4);

#if PLATFORM_MAC
	check(Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v1));
	GRHISupportsBaseVertexIndex = FPlatformMisc::MacOSXVersionCompare(10,11,2) >= 0 || !IsRHIDeviceAMD(); // Supported on macOS & iOS but not tvOS - broken on AMD prior to 10.11.2
	GRHISupportsFirstInstance = true; // Supported on macOS & iOS but not tvOS.
	GMaxTextureDimensions = 16384;
	GMaxCubeTextureDimensions = 16384;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = 16384;
	GMaxShadowDepthBufferSizeY = 16384;
    bSupportsD16 = !FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v2);
    GRHISupportsHDROutput = FPlatformMisc::MacOSXVersionCompare(10,13,0) >= 0 && Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v2);
	GRHIHDRDisplayOutputFormat = (GRHISupportsHDROutput) ? PF_PLATFORM_HDR_0 : PF_B8G8R8A8;
#else
#if PLATFORM_TVOS
	GRHISupportsBaseVertexIndex = false;
	GRHISupportsFirstInstance = false; // Supported on macOS & iOS but not tvOS.
	GRHISupportsHDROutput = false;
	GRHIHDRDisplayOutputFormat = PF_B8G8R8A8; // must have a default value for non-hdr, just like mac or ios
#else
	// Only A9+ can support this, so for now we need to limit this to the desktop-forward renderer only.
	GRHISupportsBaseVertexIndex = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1] && (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	GRHISupportsFirstInstance = GRHISupportsBaseVertexIndex;
	
	// TODO: Move this into IOSPlatform
	if (@available(iOS 11.0, *))
	{
		@autoreleasepool {
			UIScreen* mainScreen = [UIScreen mainScreen];
			UIDisplayGamut gamut = mainScreen.traitCollection.displayGamut;
			GRHISupportsHDROutput = FPlatformMisc::IOSVersionCompare(10, 0, 0) && gamut == UIDisplayGamutP3;
		}
	}
	
	GRHIHDRDisplayOutputFormat = (GRHISupportsHDROutput) ? PF_PLATFORM_HDR_0 : PF_B8G8R8A8;
#endif
	GMaxTextureDimensions = 4096;
	GMaxCubeTextureDimensions = 4096;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = 4096;
	GMaxShadowDepthBufferSizeY = 4096;
#endif
	
	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );

	// Initialize the buffer format map - in such a way as to be able to validate it in non-shipping...
#if METAL_DEBUG_OPTIONS
	FMemory::Memset(GMetalBufferFormats, 255);
#endif
	GMetalBufferFormats[PF_Unknown              ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_A32B32G32R32F        ] = { mtlpp::PixelFormat::RGBA32Float, EMetalBufferFormat::RGBA32Float };
	GMetalBufferFormats[PF_B8G8R8A8             ] = { mtlpp::PixelFormat::RGBA8Unorm, EMetalBufferFormat::RGBA8Unorm }; // mtlpp::PixelFormat::BGRA8Unorm/EMetalBufferFormat::BGRA8Unorm,  < We don't support this as a vertex-format so we have code to swizzle in the shader
	GMetalBufferFormats[PF_G8                   ] = { mtlpp::PixelFormat::R8Unorm, EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_G16                  ] = { mtlpp::PixelFormat::R16Unorm, EMetalBufferFormat::R16Unorm };
	GMetalBufferFormats[PF_DXT1                 ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_DXT3                 ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_DXT5                 ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_UYVY                 ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_FloatRGB             ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::RGB16Half };
	GMetalBufferFormats[PF_FloatRGBA            ] = { mtlpp::PixelFormat::RGBA16Float, EMetalBufferFormat::RGBA16Half };
	GMetalBufferFormats[PF_DepthStencil         ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ShadowDepth          ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32_FLOAT            ] = { mtlpp::PixelFormat::R32Float, EMetalBufferFormat::R32Float };
	GMetalBufferFormats[PF_G16R16               ] = { mtlpp::PixelFormat::RG16Unorm, EMetalBufferFormat::RG16Unorm };
	GMetalBufferFormats[PF_G16R16F              ] = { mtlpp::PixelFormat::RG16Float, EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_G16R16F_FILTER       ] = { mtlpp::PixelFormat::RG16Float, EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_G32R32F              ] = { mtlpp::PixelFormat::RG32Float, EMetalBufferFormat::RG32Float };
	GMetalBufferFormats[PF_A2B10G10R10          ] = { mtlpp::PixelFormat::RGB10A2Unorm, EMetalBufferFormat::RGB10A2Unorm };
	GMetalBufferFormats[PF_A16B16G16R16         ] = { mtlpp::PixelFormat::RGBA16Unorm, EMetalBufferFormat::RGBA16Half };
	GMetalBufferFormats[PF_D24                  ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R16F                 ] = { mtlpp::PixelFormat::R16Float, EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_R16F_FILTER          ] = { mtlpp::PixelFormat::R16Float, EMetalBufferFormat::RG16Half };
	GMetalBufferFormats[PF_BC5                  ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_V8U8                 ] = { mtlpp::PixelFormat::RG8Snorm, EMetalBufferFormat::RG8Unorm };
	GMetalBufferFormats[PF_A1                   ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_FloatR11G11B10       ] = { mtlpp::PixelFormat::RG11B10Float, EMetalBufferFormat::RG11B10Half }; // < May not work on tvOS
	GMetalBufferFormats[PF_A8                   ] = { mtlpp::PixelFormat::A8Unorm, EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_R32_UINT             ] = { mtlpp::PixelFormat::R32Uint, EMetalBufferFormat::R32Uint };
	GMetalBufferFormats[PF_R32_SINT             ] = { mtlpp::PixelFormat::R32Sint, EMetalBufferFormat::R32Sint };
	GMetalBufferFormats[PF_PVRTC2               ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PVRTC4               ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R16_UINT             ] = { mtlpp::PixelFormat::R16Uint, EMetalBufferFormat::R16Uint };
	GMetalBufferFormats[PF_R16_SINT             ] = { mtlpp::PixelFormat::R16Sint, EMetalBufferFormat::R16Sint };
	GMetalBufferFormats[PF_R16G16B16A16_UINT    ] = { mtlpp::PixelFormat::RGBA16Uint, EMetalBufferFormat::RGBA16Uint };
	GMetalBufferFormats[PF_R16G16B16A16_SINT    ] = { mtlpp::PixelFormat::RGBA16Sint, EMetalBufferFormat::RGBA16Sint };
	GMetalBufferFormats[PF_R5G6B5_UNORM         ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::R5G6B5Unorm };
	GMetalBufferFormats[PF_R8G8B8A8             ] = { mtlpp::PixelFormat::RGBA8Unorm, EMetalBufferFormat::RGBA8Unorm };
	GMetalBufferFormats[PF_A8R8G8B8				] = { mtlpp::PixelFormat::RGBA8Unorm, EMetalBufferFormat::RGBA8Unorm }; // mtlpp::PixelFormat::BGRA8Unorm/EMetalBufferFormat::BGRA8Unorm,  < We don't support this as a vertex-format so we have code to swizzle in the shader
	GMetalBufferFormats[PF_BC4					] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8G8                 ] = { mtlpp::PixelFormat::RG8Unorm, EMetalBufferFormat::RG8Unorm };
	GMetalBufferFormats[PF_ATC_RGB				] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ATC_RGBA_E			] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ATC_RGBA_I			] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_X24_G8				] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC1					] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RGB				] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ETC2_RGBA			] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R32G32B32A32_UINT	] = { mtlpp::PixelFormat::RGBA32Uint, EMetalBufferFormat::RGBA32Uint };
	GMetalBufferFormats[PF_R16G16_UINT			] = { mtlpp::PixelFormat::RG16Uint, EMetalBufferFormat::RG16Uint };
	GMetalBufferFormats[PF_ASTC_4x4             ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_6x6             ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_8x8             ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_10x10           ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_ASTC_12x12           ] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_BC6H					] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_BC7					] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8_UINT				] = { mtlpp::PixelFormat::R8Uint, EMetalBufferFormat::R8Uint };
	GMetalBufferFormats[PF_L8					] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::R8Unorm };
	GMetalBufferFormats[PF_XGXR8				] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_R8G8B8A8_UINT		] = { mtlpp::PixelFormat::RGBA8Uint, EMetalBufferFormat::RGBA8Uint };
	GMetalBufferFormats[PF_R8G8B8A8_SNORM		] = { mtlpp::PixelFormat::RGBA8Snorm, EMetalBufferFormat::RGBA8Snorm };
	GMetalBufferFormats[PF_R16G16B16A16_UNORM	] = { mtlpp::PixelFormat::RGBA16Unorm, EMetalBufferFormat::RGBA16Unorm };
	GMetalBufferFormats[PF_R16G16B16A16_SNORM	] = { mtlpp::PixelFormat::RGBA16Snorm, EMetalBufferFormat::RGBA16Snorm };
	GMetalBufferFormats[PF_PLATFORM_HDR_0		] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PLATFORM_HDR_1		] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
	GMetalBufferFormats[PF_PLATFORM_HDR_2		] = { mtlpp::PixelFormat::Invalid, EMetalBufferFormat::Unknown };
		
	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA32Float;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= (uint32)mtlpp::PixelFormat::BGRA8Unorm;
	GPixelFormats[PF_G8					].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Unorm;
	GPixelFormats[PF_G16				].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Unorm;
	GPixelFormats[PF_R32G32B32A32_UINT	].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA32Uint;
	GPixelFormats[PF_R16G16_UINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Uint;
		
#if PLATFORM_IOS
    GPixelFormats[PF_DXT1				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
    GPixelFormats[PF_DXT3				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
    GPixelFormats[PF_DXT5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_PVRTC2				].PlatformFormat	= (uint32)mtlpp::PixelFormat::PVRTC_RGBA_2BPP;
	GPixelFormats[PF_PVRTC2				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= (uint32)mtlpp::PixelFormat::PVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= (uint32)mtlpp::PixelFormat::PVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_ASTC_4x4			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_4x4_LDR;
	GPixelFormats[PF_ASTC_4x4			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_6x6			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_6x6_LDR;
	GPixelFormats[PF_ASTC_6x6			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_8x8			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_8x8_LDR;
	GPixelFormats[PF_ASTC_8x8			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_10x10			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_10x10_LDR;
	GPixelFormats[PF_ASTC_10x10			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_12x12			].PlatformFormat	= (uint32)mtlpp::PixelFormat::ASTC_12x12_LDR;
	GPixelFormats[PF_ASTC_12x12			].Supported			= bCanUseASTC;
	// IOS HDR format is BGR10_XR (32bits, 3 components)
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeX		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeY		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeZ		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockBytes		= 4;
	GPixelFormats[PF_PLATFORM_HDR_0		].NumComponents		= 3;
	GPixelFormats[PF_PLATFORM_HDR_0		].PlatformFormat	= (uint32)mtlpp::PixelFormat::BGR10_XR_sRGB;
	GPixelFormats[PF_PLATFORM_HDR_0		].Supported			= GRHISupportsHDROutput;
		
#if PLATFORM_TVOS
	if (![Device supportsFeatureSet:MTLFeatureSet_tvOS_GPUFamily2_v1])
#else
	if (![Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
#endif
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat 	= (uint32)mtlpp::PixelFormat::RGBA16Float;
		GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 8;
	}
	else
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
		GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	}
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesStencilView) && FMetalCommandQueue::SupportsFeature(EMetalFeaturesCombinedDepthStencil) && !FParse::Param(FCommandLine::Get(),TEXT("metalforceseparatedepthstencil")))
	{
		GPixelFormats[PF_DepthStencil		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float_Stencil8;
		GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	}
	else
	{
		GPixelFormats[PF_DepthStencil		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
		GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	}
	GPixelFormats[PF_DepthStencil		].Supported			= true;
	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
	GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
	GPixelFormats[PF_ShadowDepth		].Supported			= true;
		
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::B5G6R5Unorm;
#else // @todo zebra : srgb?
    GPixelFormats[PF_DXT1				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC1_RGBA;
    GPixelFormats[PF_DXT3				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC2_RGBA;
    GPixelFormats[PF_DXT5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC3_RGBA;
	
	GPixelFormats[PF_FloatRGB			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
	GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG11B10Float;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	
	// Only one HDR format for OSX.
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeX		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeY		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockSizeZ		= 1;
	GPixelFormats[PF_PLATFORM_HDR_0		].BlockBytes		= 8;
	GPixelFormats[PF_PLATFORM_HDR_0		].NumComponents		= 4;
	GPixelFormats[PF_PLATFORM_HDR_0		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Float;
	GPixelFormats[PF_PLATFORM_HDR_0		].Supported			= GRHISupportsHDROutput;
		
	// Use Depth28_Stencil8 when it is available for consistency
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float_Stencil8;
	}
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	GPixelFormats[PF_DepthStencil		].Supported			= true;
	if (bSupportsD16)
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth16Unorm;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
	}
	else
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
	}
	GPixelFormats[PF_ShadowDepth		].Supported			= true;
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_D24			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_D24			].PlatformFormat	= (uint32)mtlpp::PixelFormat::Depth32Float;
	}
	GPixelFormats[PF_D24				].Supported			= true;
	GPixelFormats[PF_BC4				].Supported			= true;
	GPixelFormats[PF_BC4				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC4_RUnorm;
	GPixelFormats[PF_BC5				].Supported			= true;
	GPixelFormats[PF_BC5				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC5_RGUnorm;
	GPixelFormats[PF_BC6H				].Supported			= true;
	GPixelFormats[PF_BC6H               ].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC6H_RGBUfloat;
	GPixelFormats[PF_BC7				].Supported			= true;
	GPixelFormats[PF_BC7				].PlatformFormat	= (uint32)mtlpp::PixelFormat::BC7_RGBAUnorm;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
#endif
	GPixelFormats[PF_UYVY				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Float;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
    GPixelFormats[PF_X24_G8				].PlatformFormat	= (uint32)mtlpp::PixelFormat::Stencil8;
    GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R32Float;
	GPixelFormats[PF_G16R16				].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Unorm;
	GPixelFormats[PF_G16R16				].Supported			= true;
	GPixelFormats[PF_G16R16F			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Float;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG16Float;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG32Float;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = (uint32)mtlpp::PixelFormat::RGB10A2Unorm;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = (uint32)mtlpp::PixelFormat::RGBA16Unorm;
	GPixelFormats[PF_R16F				].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Float;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Float;
	GPixelFormats[PF_V8U8				].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG8Snorm;
	GPixelFormats[PF_A1					].PlatformFormat	= (uint32)mtlpp::PixelFormat::Invalid;
	GPixelFormats[PF_A8					].PlatformFormat	= (uint32)mtlpp::PixelFormat::A8Unorm;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R32Uint;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R32Sint;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Uint;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Sint;
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA8Unorm;
	GPixelFormats[PF_R8G8B8A8_UINT		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA8Uint;
	GPixelFormats[PF_R8G8B8A8_SNORM		].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA8Snorm;
	GPixelFormats[PF_R8G8				].PlatformFormat	= (uint32)mtlpp::PixelFormat::RG8Unorm;
	GPixelFormats[PF_R16_SINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Sint;
	GPixelFormats[PF_R16_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R16Uint;
	GPixelFormats[PF_R8_UINT			].PlatformFormat	= (uint32)mtlpp::PixelFormat::R8Uint;

	GPixelFormats[PF_R16G16B16A16_UNORM ].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Unorm;
	GPixelFormats[PF_R16G16B16A16_SNORM ].PlatformFormat	= (uint32)mtlpp::PixelFormat::RGBA16Snorm;

#if METAL_DEBUG_OPTIONS
	for (uint32 i = 0; i < PF_MAX; i++)
	{
		checkf((NSUInteger)GMetalBufferFormats[i].LinearTextureFormat != NSUIntegerMax, TEXT("Metal linear texture format for pixel-format %s (%d) is not configured!"), GPixelFormats[i].Name, i);
		checkf(GMetalBufferFormats[i].DataFormat != 255, TEXT("Metal data buffer format for pixel-format %s (%d) is not configured!"), GPixelFormats[i].Name, i);
	}
#endif
		
	// get driver version (todo: share with other RHIs)
	{
		FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
		
		GRHIAdapterUserDriverVersion = GPUDriverInfo.UserDriverVersion;
		GRHIAdapterInternalDriverVersion = GPUDriverInfo.InternalDriverVersion;
		GRHIAdapterDriverDate = GPUDriverInfo.DriverDate;
		
		UE_LOG(LogMetal, Display, TEXT("    Adapter Name: %s"), *GRHIAdapterName);
		UE_LOG(LogMetal, Display, TEXT("  Driver Version: %s (internal:%s, unified:%s)"), *GRHIAdapterUserDriverVersion, *GRHIAdapterInternalDriverVersion, *GPUDriverInfo.GetUnifiedDriverVersion());
		UE_LOG(LogMetal, Display, TEXT("     Driver Date: %s"), *GRHIAdapterDriverDate);
		UE_LOG(LogMetal, Display, TEXT("          Vendor: %s"), *GPUDriverInfo.ProviderName);
#if PLATFORM_MAC
		if(GPUDesc.GPUVendorId == GRHIVendorId)
		{
			UE_LOG(LogMetal, Display,  TEXT("      Vendor ID: %d"), GPUDesc.GPUVendorId);
			UE_LOG(LogMetal, Display,  TEXT("      Device ID: %d"), GPUDesc.GPUDeviceId);
			UE_LOG(LogMetal, Display,  TEXT("      VRAM (MB): %d"), GPUDesc.GPUMemoryMB);
		}
		else
		{
			UE_LOG(LogMetal, Warning,  TEXT("GPU descriptor (%s) from IORegistry failed to match Metal (%s)"), *FString(GPUDesc.GPUName), *GRHIAdapterName);
		}
#endif
	}

#if PLATFORM_MAC
	if(!FPlatformProcess::IsSandboxedApplication())
	{
		FString Version;
		if (GRHIAdapterUserDriverVersion.Len())	
		{
			Version = GRHIAdapterUserDriverVersion;
		}
		else
		{
			auto OSVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
			Version = FString::Printf(TEXT("%ld.%ld.%ld"), OSVersion.majorVersion, OSVersion.minorVersion, OSVersion.patchVersion);
		}

		NSString* DstPath = [NSString stringWithFormat:@"%@/BinaryPSOs/%@/com.apple.metal", FPaths::ProjectSavedDir().GetNSString(), Version.GetNSString()];
		if([[NSFileManager defaultManager] fileExistsAtPath:DstPath])
		{
			NSString* TempDir = [NSString stringWithFormat:@"%@/../C/%@/com.apple.metal", NSTemporaryDirectory(), [NSBundle mainBundle].bundleIdentifier];

			NSError* Err = nil;
			BOOL bOK = [[NSFileManager defaultManager] removeItemAtPath:TempDir
						error:&Err];

			bOK = [[NSFileManager defaultManager] copyItemAtPath:DstPath
						toPath:TempDir
						error:&Err];
		}
	}
#endif

	((FMetalDeviceContext&)ImmediateContext.GetInternalContext()).Init();
		
	GDynamicRHI = this;
	GIsMetalInitialized = true;

	ImmediateContext.Profiler = nullptr;
#if ENABLE_METAL_GPUPROFILE
	ImmediateContext.Profiler = FMetalProfiler::CreateProfiler(ImmediateContext.Context);
#endif
		
	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}
	// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	
	AsyncComputeContext = GSupportsEfficientAsyncCompute ? new FMetalRHIComputeContext(ImmediateContext.Profiler, new FMetalContext(ImmediateContext.Context->GetDevice(), ImmediateContext.Context->GetCommandQueue(), true)) : nullptr;
	}
}

FMetalDynamicRHI::~FMetalDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());
	
#if PLATFORM_MAC
	if(!FPlatformProcess::IsSandboxedApplication())
	{
		NSString* TempDir = [NSString stringWithFormat:@"%@/../C/%@/com.apple.metal", NSTemporaryDirectory(), [NSBundle mainBundle].bundleIdentifier];

		FString Version;
		if (GRHIAdapterUserDriverVersion.Len())	
		{
			Version = GRHIAdapterUserDriverVersion;
		}
		else
		{
			auto OSVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
			Version = FString::Printf(TEXT("%ld.%ld.%ld"), OSVersion.majorVersion, OSVersion.minorVersion, OSVersion.patchVersion);
		}

		NSString* DstPath = [NSString stringWithFormat:@"%@/BinaryPSOs/%@/com.apple.metal", FPaths::ProjectSavedDir().GetNSString(), Version.GetNSString()];

		NSError* Err = nil;
		BOOL bOK = [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithFormat:@"%@/BinaryPSOs", FPaths::ProjectSavedDir().GetNSString()]
					error:&Err];

		bOK = [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithFormat:@"%@/BinaryPSOs/%@", FPaths::ProjectSavedDir().GetNSString(), Version.GetNSString()]
					withIntermediateDirectories:YES
					attributes:nil
							error:&Err];

		bOK = [[NSFileManager defaultManager] copyItemAtPath:TempDir
					toPath:DstPath 
					error:&Err];
	}
#endif

	// Ask all initialized FRenderResources to release their RHI resources.
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		FRenderResource* Resource = *ResourceIt;
		check(Resource->IsInitialized());
		Resource->ReleaseRHI();
	}
	
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->ReleaseDynamicRHI();
	}
	
	GIsMetalInitialized = false;
	GIsRHIInitialized = false;
	
#if ENABLE_METAL_GPUPROFILE
	FMetalProfiler::DestroyProfiler();
#endif
}

uint64 FMetalDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	@autoreleasepool {
	OutAlign = 0;
	return CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips);
	}
}

uint64 FMetalDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	@autoreleasepool {
	OutAlign = 0;
	return CalcTextureSize3D(SizeX, SizeY, SizeZ, (EPixelFormat)Format, NumMips);
	}
}

uint64 FMetalDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	@autoreleasepool {
	OutAlign = 0;
	return CalcTextureSize(Size, Size, (EPixelFormat)Format, NumMips) * 6;
	}
}


void FMetalDynamicRHI::Init()
{
	GIsRHIInitialized = true;
}

void FMetalRHIImmediateCommandContext::RHIBeginFrame()
{
	@autoreleasepool {
        RHIPrivateBeginFrame();
#if ENABLE_METAL_GPUPROFILE
	Profiler->BeginFrame();
#endif
	((FMetalDeviceContext*)Context)->BeginFrame();
	}
}

void FMetalRHICommandContext::RHIBeginFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndFrame()
{
	@autoreleasepool {
	// @todo zebra: GPUProfilingData.EndFrame();
#if ENABLE_METAL_GPUPROFILE
	Profiler->EndFrame();
#endif
	((FMetalDeviceContext*)Context)->EndFrame();
	}
}

void FMetalRHICommandContext::RHIEndFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIBeginScene()
{
	@autoreleasepool {
	((FMetalDeviceContext*)Context)->BeginScene();
	}
}

void FMetalRHICommandContext::RHIBeginScene()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndScene()
{
	@autoreleasepool {
	((FMetalDeviceContext*)Context)->EndScene();
	}
}

void FMetalRHICommandContext::RHIEndScene()
{
	check(false);
}

void FMetalRHICommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if ENABLE_METAL_GPUEVENTS
	// @todo zebra : this was "[NSString stringWithTCHARString:Name]", an extension only on ios for some reason, it should be on Mac also
	@autoreleasepool
	{
		FPlatformMisc::BeginNamedEvent(Color, Name);
#if ENABLE_METAL_GPUPROFILE
		Profiler->PushEvent(Name, Color);
#endif
		Context->GetCurrentRenderPass().PushDebugGroup([NSString stringWithCString:TCHAR_TO_UTF8(Name) encoding:NSUTF8StringEncoding]);
	}
#endif
}

void FMetalRHICommandContext::RHIPopEvent()
{
#if ENABLE_METAL_GPUEVENTS
	@autoreleasepool {
	FPlatformMisc::EndNamedEvent();
	Context->GetCurrentRenderPass().PopDebugGroup();
#if ENABLE_METAL_GPUPROFILE
	Profiler->PopEvent();
#endif
	}
#endif
}

void FMetalDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
#if PLATFORM_MAC
	CGDisplayModeRef DisplayMode = FPlatformApplicationMisc::GetSupportedDisplayMode(kCGDirectMainDisplay, Width, Height);
	if (DisplayMode)
	{
		Width = CGDisplayModeGetWidth(DisplayMode);
		Height = CGDisplayModeGetHeight(DisplayMode);
		CGDisplayModeRelease(DisplayMode);
	}
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetSupportedResolution unimplemented!"));
#endif
}

bool FMetalDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
#if PLATFORM_MAC
	const int32 MinAllowableResolutionX = 0;
	const int32 MinAllowableResolutionY = 0;
	const int32 MaxAllowableResolutionX = 10480;
	const int32 MaxAllowableResolutionY = 10480;
	const int32 MinAllowableRefreshRate = 0;
	const int32 MaxAllowableRefreshRate = 10480;
	
	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		const int32 NumModes = CFArrayGetCount(AllModes);
		const int32 Scale = (int32)FMacApplication::GetPrimaryScreenBackingScaleFactor();
		
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			const CGDisplayModeRef Mode = (const CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			const int32 Width = (int32)CGDisplayModeGetWidth(Mode) / Scale;
			const int32 Height = (int32)CGDisplayModeGetHeight(Mode) / Scale;
			const int32 RefreshRate = (int32)CGDisplayModeGetRefreshRate(Mode);
			
			if (Width >= MinAllowableResolutionX && Width <= MaxAllowableResolutionX && Height >= MinAllowableResolutionY && Height <= MaxAllowableResolutionY)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (RefreshRate < MinAllowableRefreshRate || RefreshRate > MaxAllowableRefreshRate)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == Width) &&
							(CheckResolution.Height == Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}
				
				if (bAddIt)
				{
					// Add the mode to the list
					const int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];
					
					ScreenResolution.Width = Width;
					ScreenResolution.Height = Height;
					ScreenResolution.RefreshRate = RefreshRate;
				}
			}
		}
		
		CFRelease(AllModes);
	}
	
	return true;
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetAvailableResolutions unimplemented!"));
	return false;
#endif
}

void FMetalDynamicRHI::RHIFlushResources()
{
	@autoreleasepool {
		((FMetalDeviceContext*)ImmediateContext.Context)->DrainHeap();
		((FMetalDeviceContext*)ImmediateContext.Context)->FlushFreeList();
		ImmediateContext.Context->SubmitCommandBufferAndWait();
		((FMetalDeviceContext*)ImmediateContext.Context)->ClearFreeList();
		ImmediateContext.Context->GetCurrentState().Reset();
	}
}

void FMetalDynamicRHI::RHIAcquireThreadOwnership()
{
	SetupRecursiveResources();
}

void FMetalDynamicRHI::RHIReleaseThreadOwnership()
{
}

void* FMetalDynamicRHI::RHIGetNativeDevice()
{
	return (void*)ImmediateContext.Context->GetDevice().GetPtr();
}

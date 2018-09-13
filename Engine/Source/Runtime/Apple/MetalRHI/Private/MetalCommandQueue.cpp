// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandQueue.cpp: Metal command queue wrapper..
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandQueue.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandList.h"
#include "MetalProfiler.h"
#if METAL_STATISTICS
#include "Modules/ModuleManager.h"
#endif
#include "Misc/ConfigCacheIni.h"
#include "command_buffer.hpp"

#pragma mark - Private C++ Statics -
uint64 FMetalCommandQueue::Features = 0;

#pragma mark - Public C++ Boilerplate -

FMetalCommandQueue::FMetalCommandQueue(mtlpp::Device InDevice, uint32 const MaxNumCommandBuffers /* = 0 */)
: Device(InDevice)
#if METAL_STATISTICS
, Statistics(nullptr)
#endif
, ParallelCommandLists(0)
, RuntimeDebuggingLevel(EMetalDebugLevelOff)
{
	int32 MaxShaderVersion = 0;
#if PLATFORM_MAC
	int32 DefaultMaxShaderVersion = 2;
    const TCHAR* const Settings = TEXT("/Script/MacTargetPlatform.MacTargetSettings");
#else
	int32 DefaultMaxShaderVersion = 0;
    const TCHAR* const Settings = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
#endif
    if(!GConfig->GetInt(Settings, TEXT("MaxShaderLanguageVersion"), MaxShaderVersion, GEngineIni))
    {
        MaxShaderVersion = DefaultMaxShaderVersion;
    }
	ValidateVersion(MaxShaderVersion);

	if(MaxNumCommandBuffers == 0)
	{
		CommandQueue = Device.NewCommandQueue();
	}
	else
	{
		CommandQueue = Device.NewCommandQueue(MaxNumCommandBuffers);
	}
	check(CommandQueue);
#if PLATFORM_IOS
	NSOperatingSystemVersion Vers = [[NSProcessInfo processInfo] operatingSystemVersion];
	if(Vers.majorVersion >= 9)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions | EMetalFeaturesShaderVersions | EMetalFeaturesSetBytes;

#if PLATFORM_TVOS
        Features &= ~(EMetalFeaturesSetBytes);
		if([Device supportsFeatureSet:MTLFeatureSet_tvOS_GPUFamily1_v2])
		{
			Features |= EMetalFeaturesStencilView | EMetalFeaturesGraphicsUAVs;
		}
#else
		if ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
		{
			Features |= EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesMSAADepthResolve;
		}
		
		if([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v3] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3])
		{
			Features |= EMetalFeaturesStencilView | EMetalFeaturesFunctionConstants | EMetalFeaturesGraphicsUAVs | EMetalFeaturesMemoryLessResources /*| EMetalFeaturesHeaps | EMetalFeaturesFences*/;
		}
		
		if([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
		{
			Features |= EMetalFeaturesTessellation | EMetalFeaturesMSAAStoreAndResolve;
		}
		
		if(Vers.majorVersion > 10 || (Vers.majorVersion == 10 && Vers.minorVersion >= 3))
        {
            // This isn't currently working, with GPUStartTime and GPUStopTime usually coming back as zero.
            // The docs say this means the GPU isn't finished with the command buffer yet, but we are running
            // in the completed handler and the status is MTLCommandBufferStatusCompleted, so it has to be
            // finished. My guess is that the command buffer is empty so the GPU isn't executing it.
            //Features |= EMetalFeaturesGPUCommandBufferTimes;
            
			Features |= EMetalFeaturesLinearTextures;
			// InjectCurves() does not work with this
			//Features |= EMetalFeaturesEfficientBufferBlits;
			Features |= EMetalFeaturesPrivateBufferSubAllocation;
			
			if([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v3] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3])
			{
				Features |= EMetalFeaturesDeferredStoreActions | EMetalFeaturesCombinedDepthStencil;
			}
			
			// Turn on Linear Texture UAVs! Avoids the need to have function-constants which reduces initial runtime shader compile time
			if (MaxShaderVersion >= 3 && Vers.majorVersion >= 11)
			{
				Features |= EMetalFeaturesLinearTextureUAVs;
			}
			
			// Turn on Texture Buffers! These are faster on the GPU as we don't need to do out-of-bounds tests but require Metal 2.1 and macOS 10.14
			if (Vers.majorVersion >= 12)
			{
				Features |= EMetalFeaturesMaxThreadsPerThreadgroup;
				if (MaxShaderVersion >= 4)
				{
					Features |= EMetalFeaturesTextureBuffers;
				}
			}
        }
		
		if(Vers.majorVersion >= 11)
		{
			Features |= EMetalFeaturesPresentMinDuration | EMetalFeaturesGPUCaptureManager;
        }
#endif
	}
	else if(Vers.majorVersion == 8 && Vers.minorVersion >= 3)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset;
	}
#else // Assume that Mac & other platforms all support these from the start. They can diverge later.
	const bool bIsNVIDIA = [Device.GetName().GetPtr() rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound;
	Features = EMetalFeaturesSeparateStencil | EMetalFeaturesDepthClipMode | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions | EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesLayeredRendering | EMetalFeaturesShaderVersions | EMetalFeaturesCombinedDepthStencil | EMetalFeaturesCubemapArrays;
	if (!bIsNVIDIA)
	{
		Features |= EMetalFeaturesSetBufferOffset;
	}
	if (Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v2))
    {
        Features |= EMetalFeaturesStencilView | EMetalFeaturesDepth16 | EMetalFeaturesTessellation | EMetalFeaturesFunctionConstants | EMetalFeaturesGraphicsUAVs | EMetalFeaturesDeferredStoreActions | EMetalFeaturesMSAADepthResolve | EMetalFeaturesMSAAStoreAndResolve;
        
        // Assume that set*Bytes only works on macOS Sierra and above as no-one has tested it anywhere else.
		Features |= EMetalFeaturesSetBytes;
		
		Features |= EMetalFeaturesLinearTextures;
		
		FString DeviceName(Device.GetName());
		// On earlier OS versions Intel Broadwell couldn't suballocate properly
		if (!(DeviceName.Contains(TEXT("Intel")) && (DeviceName.Contains(TEXT("5300")) || DeviceName.Contains(TEXT("6000")) || DeviceName.Contains(TEXT("6100")))) || FPlatformMisc::MacOSXVersionCompare(10,14,0) >= 0)
		{
			// Using Private Memory & BlitEncoders for Vertex & Index data should be *much* faster.
        	Features |= EMetalFeaturesEfficientBufferBlits;
        	
			Features |= EMetalFeaturesBufferSubAllocation;
					
	        // On earlier OS versions Vega didn't like non-zero blit offsets
	        if (!DeviceName.Contains(TEXT("Vega")) || FPlatformMisc::MacOSXVersionCompare(10,13,5) >= 0)
	        {
				Features |= EMetalFeaturesPrivateBufferSubAllocation;
			}
		}

		// Turn on Linear Texture UAVs! Avoids the need to have function-constants which reduces initial runtime shader compile time
		if (MaxShaderVersion >= 3 && FPlatformMisc::MacOSXVersionCompare(10,13,5) >= 0)
		{
			Features |= EMetalFeaturesLinearTextureUAVs;
		}

		// Turn on Texture Buffers! These are faster on the GPU as we don't need to do out-of-bounds tests but require Metal 2.1 and macOS 10.14
		if (FPlatformMisc::MacOSXVersionCompare(10,14,0) >= 0)
		{
			Features |= EMetalFeaturesMaxThreadsPerThreadgroup;
			if (MaxShaderVersion >= 4)
			{
				Features |= EMetalFeaturesTextureBuffers;
			}
		}
    }
    else if ([Device.GetName().GetPtr() rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound)
    {
		// Using set*Bytes fixes bugs on Nvidia for 10.11 so we should use it...
    	Features |= EMetalFeaturesSetBytes;
    }
    
    if(Device.SupportsFeatureSet(mtlpp::FeatureSet::macOS_GPUFamily1_v3) && FPlatformMisc::MacOSXVersionCompare(10,13,0) >= 0)
    {
        Features |= EMetalFeaturesMultipleViewports | EMetalFeaturesGPUCommandBufferTimes | EMetalFeaturesGPUCaptureManager | EMetalFeaturesAbsoluteTimeQueries | EMetalFeaturesSupportsVSyncToggle /*| EMetalFeaturesHeaps | EMetalFeaturesFences*/;
    }
	else
	// Time query emulation breaks on AMD < 10.13 - disable by default until they can explain why, should work everywhere else.
	if (!FString(Device.GetName()).Contains(TEXT("AMD")) || FParse::Param(FCommandLine::Get(),TEXT("metaltimequery")))
	{
		Features |= EMetalFeaturesAbsoluteTimeQueries;
	}
#endif
	
#if !UE_BUILD_SHIPPING
	Class MTLDebugDevice = NSClassFromString(@"MTLDebugDevice");
	if ([Device isKindOfClass:MTLDebugDevice])
	{
		Features |= EMetalFeaturesValidation;
	}
#endif
    
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
	if (CVar->GetInt() == 0 || FParse::Param(FCommandLine::Get(),TEXT("metalshaderdebug")))
	{
		Features |= EMetalFeaturesGPUTrace;
	}
	
#if METAL_STATISTICS
	if (FParse::Param(FCommandLine::Get(),TEXT("metalstats")))
	{
		IMetalStatisticsModule* StatsModule = FModuleManager::Get().LoadModulePtr<IMetalStatisticsModule>(TEXT("MetalStatistics"));		
		if(StatsModule)
		{
			Statistics = StatsModule->CreateMetalStatistics(CommandQueue);
			if(Statistics->SupportsStatistics())
			{
				GSupportsTimestampRenderQueries = true;
				Features |= EMetalFeaturesStatistics;
			}
			else
			{
				delete Statistics;
				Statistics = nullptr;
			}
		}
	}
#endif
    
	PermittedOptions = 0;
	PermittedOptions |= mtlpp::ResourceOptions::CpuCacheModeDefaultCache;
	PermittedOptions |= mtlpp::ResourceOptions::CpuCacheModeWriteCombined;
	if (Features & EMetalFeaturesResourceOptions)
	{
		PermittedOptions |= mtlpp::ResourceOptions::StorageModeShared;
		PermittedOptions |= mtlpp::ResourceOptions::StorageModePrivate;
#if PLATFORM_MAC
		PermittedOptions |= mtlpp::ResourceOptions::StorageModeManaged;
#else
		if (Features & EMetalFeaturesMemoryLessResources)
		{
			PermittedOptions |= mtlpp::ResourceOptions::StorageModeMemoryless;
		}
#endif
		if (Features & EMetalFeaturesFences)
		{
			PermittedOptions |= mtlpp::ResourceOptions::HazardTrackingModeUntracked;
		}
	}
}

FMetalCommandQueue::~FMetalCommandQueue(void)
{
#if METAL_STATISTICS
	delete Statistics;
#endif
}
	
#pragma mark - Public Command Buffer Mutators -

mtlpp::CommandBuffer FMetalCommandQueue::CreateCommandBuffer(void)
{
#if PLATFORM_MAC
	static bool bUnretainedRefs = !FParse::Param(FCommandLine::Get(),TEXT("metalretainrefs"))
	&& ([Device.GetName() rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location == NSNotFound)
	&& ([Device.GetName() rangeOfString:@"Intel" options:NSCaseInsensitiveSearch].location == NSNotFound || FPlatformMisc::MacOSXVersionCompare(10,13,0) >= 0);
#else
	static bool bUnretainedRefs = !FParse::Param(FCommandLine::Get(),TEXT("metalretainrefs"));
#endif
	
	mtlpp::CommandBuffer CmdBuffer;
	@autoreleasepool
	{
		CmdBuffer = bUnretainedRefs ? MTLPP_VALIDATE(mtlpp::CommandQueue, CommandQueue, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, CommandBufferWithUnretainedReferences()) : MTLPP_VALIDATE(mtlpp::CommandQueue, CommandQueue, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, CommandBuffer());
		
		if (RuntimeDebuggingLevel > EMetalDebugLevelLogDebugGroups)
		{			
			METAL_DEBUG_ONLY(FMetalCommandBufferDebugging AddDebugging(CmdBuffer));
			MTLPP_VALIDATION(mtlpp::CommandBufferValidationTable ValidatedCommandBuffer(CmdBuffer));
		}
		else if (RuntimeDebuggingLevel == EMetalDebugLevelLogDebugGroups)
		{
			((NSObject<MTLCommandBuffer>*)CmdBuffer.GetPtr()).debugGroups = [[NSMutableArray new] autorelease];
		}
	}
	CommandBufferFences.Push(new mtlpp::CommandBufferFence(CmdBuffer.GetCompletionFence()));
	INC_DWORD_STAT(STAT_MetalCommandBufferCreatedPerFrame);
	return CmdBuffer;
}

void FMetalCommandQueue::CommitCommandBuffer(mtlpp::CommandBuffer& CommandBuffer)
{
	check(CommandBuffer);
	INC_DWORD_STAT(STAT_MetalCommandBufferCommittedPerFrame);
	
	MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Commit());
	
	// Wait for completion when debugging command-buffers.
#if METAL_DEBUG_OPTIONS
	if (RuntimeDebuggingLevel >= EMetalDebugLevelWaitForComplete)
	{
		CommandBuffer.WaitUntilCompleted();
	}
#endif
}

void FMetalCommandQueue::SubmitCommandBuffers(TArray<mtlpp::CommandBuffer> BufferList, uint32 Index, uint32 Count)
{
	CommandBuffers.SetNumZeroed(Count);
	CommandBuffers[Index] = BufferList;
	ParallelCommandLists |= (1 << Index);
	if (ParallelCommandLists == ((1 << Count) - 1))
	{
		for (uint32 i = 0; i < Count; i++)
		{
			TArray<mtlpp::CommandBuffer>& CmdBuffers = CommandBuffers[i];
			for (mtlpp::CommandBuffer Buffer : CmdBuffers)
			{
				check(Buffer);
				CommitCommandBuffer(Buffer);
			}
			CommandBuffers[i].Empty();
		}
		
		ParallelCommandLists = 0;
	}
}

mtlpp::Fence FMetalCommandQueue::CreateFence(ns::String const& Label) const
{
	mtlpp::Fence InternalFence;
	if(Features & EMetalFeaturesFences)
	{
		InternalFence = Device.NewFence();
	}
#if METAL_DEBUG_OPTIONS
	if (RuntimeDebuggingLevel >= EMetalDebugLevelValidation)
	{
		FMetalDebugFence* Fence = [[FMetalDebugFence new] autorelease];
		Fence.Inner = InternalFence;
		InternalFence = Fence;
		Fence.label = Label;
	}
	else
#endif
	if(InternalFence && Label)
	{
		InternalFence.SetLabel(Label);
	}
	return InternalFence;
}

void FMetalCommandQueue::GetCommittedCommandBufferFences(TArray<mtlpp::CommandBufferFence>& Fences)
{
	TArray<mtlpp::CommandBufferFence*> Temp;
	CommandBufferFences.PopAll(Temp);
	for (mtlpp::CommandBufferFence* Fence : Temp)
	{
		Fences.Add(*Fence);
		delete Fence;
	}
}

#pragma mark - Public Command Queue Accessors -
	
mtlpp::Device& FMetalCommandQueue::GetDevice(void)
{
	return Device;
}

mtlpp::ResourceOptions FMetalCommandQueue::GetCompatibleResourceOptions(mtlpp::ResourceOptions Options) const
{
	NSUInteger NewOptions = (Options & PermittedOptions);
#if PLATFORM_IOS // Swizzle Managed to Shared for iOS - we can do this as they are equivalent, unlike Shared -> Managed on Mac.
	if ((Features & EMetalFeaturesResourceOptions) && (Options & (1 /*mtlpp::StorageMode::Managed*/ << mtlpp::ResourceStorageModeShift)))
	{
		NewOptions |= mtlpp::ResourceOptions::StorageModeShared;
	}
#endif
	return (mtlpp::ResourceOptions)NewOptions;
}

#pragma mark - Public Debug Support -

void FMetalCommandQueue::InsertDebugCaptureBoundary(void)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	[CommandQueue insertDebugCaptureBoundary];
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FMetalCommandQueue::SetRuntimeDebuggingLevel(int32 const Level)
{
	RuntimeDebuggingLevel = Level;
}

int32 FMetalCommandQueue::GetRuntimeDebuggingLevel(void) const
{
	return RuntimeDebuggingLevel;
}

#if METAL_STATISTICS
#pragma mark - Public Statistics Extensions -

IMetalStatistics* FMetalCommandQueue::GetStatistics(void)
{
	return Statistics;
}
#endif

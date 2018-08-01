// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "EngineGlobals.h"
#include "StaticBoundShaderState.h"
#include "MetalCommandBuffer.h"
#include "HAL/FileManager.h"

// The Metal standard library extensions we need for UE4.
extern unsigned int ue4_stdlib_metal_len;
extern unsigned char ue4_stdlib_metal[];

DEFINE_STAT(STAT_MetalUniformMemAlloc);
DEFINE_STAT(STAT_MetalUniformMemFreed);
DEFINE_STAT(STAT_MetalVertexMemAlloc);
DEFINE_STAT(STAT_MetalVertexMemFreed);
DEFINE_STAT(STAT_MetalIndexMemAlloc);
DEFINE_STAT(STAT_MetalIndexMemFreed);
DEFINE_STAT(STAT_MetalTextureMemUpdate);

DEFINE_STAT(STAT_MetalDrawCallTime);
DEFINE_STAT(STAT_MetalPipelineStateTime);
DEFINE_STAT(STAT_MetalPrepareDrawTime);
DEFINE_STAT(STAT_MetalMakeDrawableTime);
DEFINE_STAT(STAT_MetalBufferPageOffTime);
DEFINE_STAT(STAT_MetalTexturePageOnTime);
DEFINE_STAT(STAT_MetalTexturePageOffTime);
DEFINE_STAT(STAT_MetalGPUWorkTime);
DEFINE_STAT(STAT_MetalGPUIdleTime);
DEFINE_STAT(STAT_MetalPresentTime);
DEFINE_STAT(STAT_MetalCommandBufferCreatedPerFrame);
DEFINE_STAT(STAT_MetalCommandBufferCommittedPerFrame);
DEFINE_STAT(STAT_MetalBufferMemory);
DEFINE_STAT(STAT_MetalTextureMemory);
DEFINE_STAT(STAT_MetalBufferUnusedMemory);
DEFINE_STAT(STAT_MetalTextureUnusedMemory);
DEFINE_STAT(STAT_MetalBufferCount);
DEFINE_STAT(STAT_MetalTextureCount);

int64 volatile GMetalTexturePageOnTime = 0;
int64 volatile GMetalGPUWorkTime = 0;
int64 volatile GMetalGPUIdleTime = 0;
int64 volatile GMetalPresentTime = 0;

void WriteString(FArchive* OutputFile, const char* String)
{
	OutputFile->Serialize((void*)String, sizeof(ANSICHAR)*FCStringAnsi::Strlen(String));
}

FMetalEventNode::~FMetalEventNode()
{
}

float FMetalEventNode::GetTiming()
{
	return FPlatformTime::ToSeconds(EndTime - StartTime);
}

void FMetalEventNode::StartTiming()
{
	StartTime = 0;
	EndTime = 0;
#if METAL_STATISTICS
	IMetalStatistics* Stats = Context->GetCommandQueue().GetStatistics();
	if (Stats)
	{
		id<IMetalStatisticsSamples> StatSample = Stats->GetLastStatisticsSample(Context->GetCurrentCommandBuffer().GetPtr());
		if (!StatSample)
		{
			Context->GetCurrentRenderPass().InsertDebugEncoder();
			StatSample = Stats->GetLastStatisticsSample(Context->GetCurrentCommandBuffer().GetPtr());
		}
		check(StatSample);
		[StatSample retain];
		
		Context->GetCurrentCommandBuffer().AddCompletedHandler(^(const mtlpp::CommandBuffer &) {
			if (StatSample.Count > 0)
			{
				StartTime = StatSample.Array[0];
			}
			[StatSample release];
		});
	}
	else
#endif
	{
		Context->StartTiming(this);
	}
}

mtlpp::CommandBufferHandler FMetalEventNode::Start(void)
{
	mtlpp::CommandBufferHandler Block = [this](mtlpp::CommandBuffer const& CompletedBuffer)
	{
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
		{
			const CFTimeInterval GpuTimeSeconds = CompletedBuffer.GetGpuStartTime();
			const double CyclesPerSecond = 1.0 / FPlatformTime::GetSecondsPerCycle();
			StartTime = GpuTimeSeconds * CyclesPerSecond;
		}
		else
		{
			StartTime = mach_absolute_time();
		}
	};
    return Block_copy(Block);
}

void FMetalEventNode::StopTiming()
{
#if METAL_STATISTICS
	IMetalStatistics* Stats = Context->GetCommandQueue().GetStatistics();
	if (Context->GetCommandQueue().GetStatistics())
	{
		id<IMetalStatisticsSamples> StatSample = Stats->GetLastStatisticsSample(Context->GetCurrentCommandBuffer().GetPtr());
		if (!StatSample)
		{
			Context->GetCurrentRenderPass().InsertDebugEncoder();
			StatSample = Stats->GetLastStatisticsSample(Context->GetCurrentCommandBuffer().GetPtr());
		}
		check(StatSample);
		[StatSample retain];
		
		Context->GetCurrentCommandBuffer().AddCompletedHandler(^(const mtlpp::CommandBuffer &) {
			if (StatSample.Count > 0)
			{
				EndTime = StatSample.Array[0];
			}
			[StatSample release];
			
			if (bRoot)
			{
				// We have a different mechanism for the overall frametime that works even with empty encoders and that doesn't report any GPU idle time between frames, we only use the fallback code below on older OSes.
				if (!FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
				{
					uint32 Time = FMath::TruncToInt( double(GetTiming()) / double(FPlatformTime::GetSecondsPerCycle()) );
					FPlatformAtomics::InterlockedExchange((int32*)&GGPUFrameTime, (int32)Time);
				}
				
				if(!bFullProfiling)
				{
					delete this;
				}
			}
		});
		
		bool const bWait = Wait();
		if (bWait)
		{
			Context->SubmitCommandBufferAndWait();
		}
	}
	else
#endif
	{
		Context->EndTiming(this);
	}
}

mtlpp::CommandBufferHandler FMetalEventNode::Stop(void)
{
	mtlpp::CommandBufferHandler Block = [this](mtlpp::CommandBuffer const& CompletedBuffer)
	{
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
		{
			// This is still used by ProfileGPU
			const CFTimeInterval GpuTimeSeconds = CompletedBuffer.GetGpuEndTime();
			const double CyclesPerSecond = 1.0 / FPlatformTime::GetSecondsPerCycle();
			EndTime = GpuTimeSeconds * CyclesPerSecond;
		}
		else
		{
			EndTime = mach_absolute_time();
		}
		
		if(bRoot)
		{
			// But we have a different mechanism for the overall frametime that works even with empty encoders and that doesn't report any GPU idle time between frames, we only use the fallback code below on older OSes.
			if (!FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
			{
				uint32 Time = FMath::TruncToInt( double(GetTiming()) / double(FPlatformTime::GetSecondsPerCycle()) );
				FPlatformAtomics::InterlockedExchange((int32*)&GGPUFrameTime, (int32)Time);
			}
			
			if(!bFullProfiling)
			{
				delete this;
			}
		}
	};
	return Block_copy(Block);
}

bool MetalGPUProfilerIsInSafeThread()
{
	return IsInRHIThread() || IsInActualRenderingThread();
}
	
/** Start this frame of per tracking */
void FMetalEventNodeFrame::StartFrame()
{
	RootNode->StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FMetalEventNodeFrame::EndFrame()
{
	RootNode->StopTiming();
}

/** Calculates root timing base frequency (if needed by this RHI) */
float FMetalEventNodeFrame::GetRootTimingResults()
{
	return RootNode->GetTiming();
}

void FMetalEventNodeFrame::LogDisjointQuery()
{
	
}

FGPUProfilerEventNode* FMetalGPUProfiler::CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent)
{
#if ENABLE_METAL_GPUPROFILE
	FMetalEventNode* EventNode = new FMetalEventNode(FMetalContext::GetCurrentContext(), InName, InParent, false, false);
	return EventNode;
#else
	return nullptr;
#endif
}

void FMetalGPUProfiler::Cleanup()
{
	
}

void FMetalGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	if(MetalGPUProfilerIsInSafeThread())
	{
		FGPUProfiler::PushEvent(Name, Color);
	}
}

void FMetalGPUProfiler::PopEvent()
{
	if(MetalGPUProfilerIsInSafeThread())
	{
		FGPUProfiler::PopEvent();
	}
}

//TGlobalResource<FVector4VertexDeclaration> GMetalVector4VertexDeclaration;
TGlobalResource<FTexture> GMetalLongTaskRT;

void FMetalGPUProfiler::BeginFrame()
{
	if(!CurrentEventNodeFrame)
	{
		// Start tracking the frame
		CurrentEventNodeFrame = new FMetalEventNodeFrame(Context, GTriggerGPUProfile);
		CurrentEventNodeFrame->StartFrame();
		
		if(GNumAlternateFrameRenderingGroups > 1)
		{
			GTriggerGPUProfile = false;
		}

		if(GTriggerGPUProfile)
		{
			bTrackingEvents = true;
			bLatchedGProfilingGPU = true;
			GTriggerGPUProfile = false;
		}
	}
	NumNestedFrames++;
}

void FMetalGPUProfiler::EndFrame()
{
	if(--NumNestedFrames == 0)
	{
#if METAL_STATISTICS
		if(Context->GetCommandQueue().GetStatistics())
		{
			Context->GetCommandQueue().GetStatistics()->UpdateDriverMonitorStatistics(GetMetalDeviceContext().GetDeviceIndex());
		}
		else
#endif
		{
#if PLATFORM_MAC
			FPlatformMisc::UpdateDriverMonitorStatistics(GetMetalDeviceContext().GetDeviceIndex());
#endif
		}
#if STATS
		SET_CYCLE_COUNTER(STAT_MetalTexturePageOnTime, GMetalTexturePageOnTime);
		GMetalTexturePageOnTime = 0;
		
		SET_CYCLE_COUNTER(STAT_MetalGPUIdleTime, GMetalGPUIdleTime);
		SET_CYCLE_COUNTER(STAT_MetalGPUWorkTime, GMetalGPUWorkTime);
		SET_CYCLE_COUNTER(STAT_MetalPresentTime, GMetalPresentTime);
#endif
		
		if(CurrentEventNodeFrame)
		{
			CurrentEventNodeFrame->EndFrame();
			
			if(bLatchedGProfilingGPU)
			{
				bTrackingEvents = false;
				bLatchedGProfilingGPU = false;
			
				UE_LOG(LogRHI, Warning, TEXT(""));
				UE_LOG(LogRHI, Warning, TEXT(""));
				CurrentEventNodeFrame->DumpEventTree();
			}
			
			delete CurrentEventNodeFrame;
			CurrentEventNodeFrame = NULL;
		}
	}
}

void FMetalGPUProfiler::IncrementFrameIndex()
{
    if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
    {
		FPlatformAtomics::InterlockedExchange(&FrameTimeGPUIndex, ((FrameTimeGPUIndex + 1) % MAX_FRAME_HISTORY));
		FPlatformAtomics::InterlockedExchange(&FrameStartGPU[FrameTimeGPUIndex], 0);
		FPlatformAtomics::InterlockedExchange(&FrameEndGPU[FrameTimeGPUIndex], 0);
		FPlatformAtomics::InterlockedExchange(&FrameGPUTime[FrameTimeGPUIndex], 0);
		FPlatformAtomics::InterlockedExchange(&FrameIdleTime[FrameTimeGPUIndex], 0);
		FPlatformAtomics::InterlockedExchange(&FramePresentTime[FrameTimeGPUIndex], 0);
    }
}

void FMetalGPUProfiler::RecordFrame(mtlpp::CommandBuffer& Buffer)
{
	RecordCommandBuffer(Buffer);
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
	{
		uint32 Existing = FrameTimeGPUIndex;
		
		Buffer.AddCompletedHandler([Existing](mtlpp::CommandBuffer const&){
			uint32 Time = FMath::TruncToInt(FPlatformTime::ToSeconds64(FrameEndGPU[Existing] - FrameStartGPU[Existing]) / FPlatformTime::GetSecondsPerCycle64());
			FPlatformAtomics::InterlockedExchange((int32*)&GGPUFrameTime, (int32)Time);
#if STATS
			FPlatformAtomics::InterlockedExchange(&GMetalGPUWorkTime, FrameGPUTime[Existing]);
			
			Time = FMath::TruncToInt(FPlatformTime::ToSeconds64(((FrameEndGPU[Existing] - FrameStartGPU[Existing]) - FrameGPUTime[Existing])) / FPlatformTime::GetSecondsPerCycle64());
			FPlatformAtomics::InterlockedExchange(&FrameIdleTime[Existing], (int32)Time);
			FPlatformAtomics::InterlockedExchange(&GMetalGPUIdleTime, Time);
#endif //STATS
		});
	}
}

void FMetalGPUProfiler::RecordPresent(mtlpp::CommandBuffer& Buffer)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
	{
		uint32 Existing = FrameTimeGPUIndex;
		Buffer.AddCompletedHandler([Existing](mtlpp::CommandBuffer const& InBuffer){
			const CFTimeInterval GpuStartTimeSeconds = InBuffer.GetGpuStartTime();
			const CFTimeInterval GpuEndTimeSeconds = InBuffer.GetGpuEndTime();
			const double CyclesPerSecond = 1.0 / FPlatformTime::GetSecondsPerCycle();
			uint64 StartTime = GpuStartTimeSeconds * CyclesPerSecond;
			uint64 EndTime = GpuEndTimeSeconds * CyclesPerSecond;
			uint32 Time = FMath::TruncToInt(FPlatformTime::ToSeconds64(EndTime - StartTime) / FPlatformTime::GetSecondsPerCycle64());
			FPlatformAtomics::InterlockedExchange(&FramePresentTime[Existing], (int32)Time);
			FPlatformAtomics::InterlockedExchange(&GMetalPresentTime, Time);
		});
	}
}

void FMetalGPUProfiler::RecordCommandBuffer(mtlpp::CommandBuffer& Buffer)
{
    if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
    {
		uint32 Index = FrameTimeGPUIndex;
		Buffer.AddCompletedHandler([Index](mtlpp::CommandBuffer const& InBuffer)
		{
            const CFTimeInterval GpuTimeSeconds = InBuffer.GetGpuEndTime();
            const double CyclesPerSecond = 1.0 / FPlatformTime::GetSecondsPerCycle();
            uint64 Time = GpuTimeSeconds * CyclesPerSecond;
            uint64 Existing, New;
			do
			{
				Existing = FrameEndGPU[Index];
				New = Existing > 0 ? FMath::Max(Existing, Time) : Time;
			} while(FPlatformAtomics::InterlockedCompareExchange(&FrameEndGPU[Index], New, Existing) != Existing);
			
			const CFTimeInterval GpuStartTimeSeconds = InBuffer.GetGpuStartTime();
			uint64 StartTime = GpuStartTimeSeconds * CyclesPerSecond;
			do
			{
				Existing = FrameStartGPU[Index];
				New = Existing > 0 ? FMath::Min(Existing, StartTime) : Time;
			} while(FPlatformAtomics::InterlockedCompareExchange(&FrameStartGPU[Index], New, Existing) != Existing);
			
			Time = FMath::TruncToInt(FPlatformTime::ToSeconds64(Time - StartTime) / FPlatformTime::GetSecondsPerCycle64());
			FPlatformAtomics::InterlockedAdd(&FrameGPUTime[Index], (int32)Time);
        });
    }
}
	
volatile int32 FMetalGPUProfiler::FrameTimeGPUIndex = 0;
volatile int64 FMetalGPUProfiler::FrameStartGPU[MAX_FRAME_HISTORY];
volatile int64 FMetalGPUProfiler::FrameEndGPU[MAX_FRAME_HISTORY];
volatile int64 FMetalGPUProfiler::FrameGPUTime[MAX_FRAME_HISTORY];
volatile int64 FMetalGPUProfiler::FrameIdleTime[MAX_FRAME_HISTORY];
volatile int64 FMetalGPUProfiler::FramePresentTime[MAX_FRAME_HISTORY];

IMetalStatsScope::~IMetalStatsScope()
{
	for (IMetalStatsScope* Stat : Children)
	{
		delete Stat;
	}
}

FString IMetalStatsScope::GetJSONRepresentation(uint32 Pid)
{
	FString JSONOutput;
	
	{
#if METAL_STATISTICS
		FMetalPipelineStats DrawStat;
		
		GetStats(DrawStat);
		
		if (GPUStartTime && GPUEndTime)
		{
			uint64 ChildStartCallTime = GPUStartTime;
			uint64 ChildDrawCallTime = GPUEndTime - GPUStartTime;
			
			if (DrawStat.PSOPerformanceStats)
			{
				FString PSOStats;
				for(id Key in DrawStat.PSOPerformanceStats)
				{
					NSString* ShaderName = (NSString*)Key;
					NSDictionary* ShaderData = [DrawStat.PSOPerformanceStats objectForKey:Key];
					if (ShaderData)
					{
						for (id StatKey in ShaderData)
						{
							PSOStats += FString::Printf(TEXT(",\"%s %s\":%s"), *FString(ShaderName), *FString((NSString*)StatKey), *FString([[ShaderData objectForKey:StatKey] description]));
						}
					}
				}
				
				JSONOutput += FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"X\", \"name\": \"%s\", \"ts\": %llu, \"dur\": %llu, \"args\":{\"num_child\":%u %s}},\n"),
											  Pid,
											  GPUThreadIndex,
											  *Name,
											  ChildStartCallTime,
											  ChildDrawCallTime,
											  Children.Num(),
											  *PSOStats
											  );
			}
			else
			{
				FString CustomCounters;
				TMap<FString, FMetalProfiler::EMTLCounterType> const& CounterTypes = FMetalProfiler::GetProfiler()->GetCounterTypes();
				for(auto const& Pair : DrawStat.Counters)
				{
					NSString* CounterName = Pair.Key;
					TPair<uint64, uint64> Vals = Pair.Value;
					const FMetalProfiler::EMTLCounterType* TypePtr = CounterTypes.Find(FString(CounterName));
					FMetalProfiler::EMTLCounterType Type = TypePtr ? *TypePtr : FMetalProfiler::EMTLCounterTypeStartEnd;
					switch(Type)
					{
						case FMetalProfiler::EMTLCounterTypeLast:
							CustomCounters += FString::Printf(TEXT(",\"%s\":%llu"), *FString(CounterName), Vals.Value);
							break;
						case FMetalProfiler::EMTLCounterTypeDifference:
							CustomCounters += FString::Printf(TEXT(",\"%s\":%llu"), *FString(CounterName), Vals.Value - Vals.Key);
							break;
						case FMetalProfiler::EMTLCounterTypeStartEnd:
						default:
							CustomCounters += FString::Printf(TEXT(",\"%s\":\"%llu:%llu\""), *FString(CounterName), Vals.Key, Vals.Value);
							break;
					}
				}
				
				JSONOutput += FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"X\", \"name\": \"%s\", \"ts\": %llu, \"dur\": %llu, \"args\":{\"num_child\":%u,\"shade_cost\":%llu,\"rhi_prims\":%llu,\"ia_prims\":%llu,\"rhi_verts\":%llu,\"ia_verts\":%llu,\"vert_invoc\":%llu,\"vert_percent\":%llu,\"clip_invoc\":%llu,\"clip_prims\":%llu,\"frag_invoc\":%llu,\"frag_percent\":%llu,\"comp_invoc\":%llu,\"comp_percent\":%llu %s}},\n"),
					 Pid,
					 GPUThreadIndex,
					 *Name,
					 ChildStartCallTime,
					 ChildDrawCallTime,
					 Children.Num(),
					 DrawStat.ShaderFunctionCost,
					 DrawStat.RHIPrimitives,
					 DrawStat.InputPrimitives,
					 DrawStat.RHIVertices,
					 DrawStat.InputVertices,
					 DrawStat.VertexFunctionInvocations,
					 DrawStat.VertexFunctionCost,
					 DrawStat.ClipperInvocations,
					 DrawStat.ClipperPrimitives,
					 DrawStat.FragmentFunctionInvocations,
					 DrawStat.FragmentFunctionCost,
					 DrawStat.ComputeFunctionInvocations,
					 DrawStat.ComputeFunctionCost,
					 *CustomCounters
				);
			}
		}
#else
		if (GPUStartTime && GPUEndTime)
		{
			uint64 ChildStartCallTime = GPUStartTime;
			uint64 ChildDrawCallTime = GPUEndTime - GPUStartTime;
			
			JSONOutput += FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"X\", \"name\": \"%s\", \"ts\": %llu, \"dur\": %llu, \"args\":{\"num_child\":%u}},\n"),
				  Pid,
				  GPUThreadIndex,
				  *Name,
				  ChildStartCallTime,
				  ChildDrawCallTime,
				  Children.Num()
			  );
		}
#endif
	}
	
	if (CPUStartTime && CPUEndTime)
	{
		uint64 ChildStartCallTime = CPUStartTime;
		uint64 ChildDrawCallTime = CPUEndTime - CPUStartTime;
		
		JSONOutput += FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"X\", \"name\": \"%s\", \"ts\": %llu, \"dur\": %llu, \"args\":{\"num_child\":%u}},\n"),
			 Pid,
			 CPUThreadIndex,
			 *Name,
			 ChildStartCallTime,
			 ChildDrawCallTime,
			 Children.Num()
		);
	}
	
	return JSONOutput;
}

#if METAL_STATISTICS
FMetalEventStats::FMetalEventStats(const TCHAR* InName, FColor Color)
{
	Name = InName;
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = 1;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
	
	StartSample = nullptr;
	EndSample = nullptr;
}

FMetalEventStats::~FMetalEventStats()
{
	[StartSample release];
	[EndSample release];
}
	
void FMetalEventStats::Start(mtlpp::CommandBuffer const& Buffer)
{
	check(!StartSample);
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();

	StartSample = [Stats->GetLastStatisticsSample(Buffer.GetPtr()) retain];
	check(StartSample);
}
void FMetalEventStats::End(mtlpp::CommandBuffer const& Buffer)
{
	check(!EndSample);
	
	CPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	
	EndSample = [Stats->GetLastStatisticsSample(Buffer.GetPtr()) retain];
	check(EndSample);
}

void FMetalEventStats::GetStats(FMetalPipelineStats& PipelineStats)
{
	check(StartSample && EndSample);
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	
	Stats->ConvertSamplesToPipelineStats(StartSample, EndSample, PipelineStats);
	
	GPUStartTime = PipelineStats.StartTime / 1000;
	GPUEndTime = GPUStartTime + (PipelineStats.DrawCallTime / 1000);
}

FMetalShaderPipelineStats::FMetalShaderPipelineStats(FMetalShaderPipeline* PipelineStat, uint64 InGPUThreadIndex)
{
	Pipeline = PipelineStat;
	check(Pipeline);
	
	CmdBufferStats = nullptr;
	
	StartSample = nullptr;
	
#if METAL_DEBUG_OPTIONS
	if (Pipeline->RenderPipelineState)
	{
		Name = Pipeline->RenderPipelineState.GetLabel().GetPtr();
		
		if (Pipeline->ComputePipelineState)
		{
			Name += TEXT("+") + FString(Pipeline->ComputePipelineState.GetLabel().GetPtr());
		}
	}
	else if (Pipeline->ComputePipelineState)
	{
		Name = Pipeline->ComputePipelineState.GetLabel().GetPtr();
	}
	else
#endif
	{
		Name = "Unknown Pipeline";
	}
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
}

FMetalShaderPipelineStats::~FMetalShaderPipelineStats()
{
	[StartSample release];
}

void FMetalShaderPipelineStats::Start(mtlpp::CommandBuffer const& Buffer)
{
	check(CmdBufferStats);
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	StartSample = [Stats->RegisterEncoderStatistics(CmdBufferStats, EMetalSamplePipelineChange) retain];
}
	
void FMetalShaderPipelineStats::End(mtlpp::CommandBuffer const& Buffer)
{
	CPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
}
	
void FMetalShaderPipelineStats::GetStats(FMetalPipelineStats& PipelineStats)
{
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	if (!GPUStartTime && !GPUEndTime)
	{
		Stats->ConvertSamplesToPipelineStats(StartSample, nullptr, PipelineStats);
		GPUStartTime = PipelineStats.StartTime / 1000;
		GPUEndTime = GPUStartTime + (PipelineStats.DrawCallTime / 1000);
	}
	else
	{
		PipelineStats.StartTime = GPUStartTime;
		PipelineStats.DrawCallTime = GPUEndTime - GPUStartTime;
	}
	
#if METAL_DEBUG_OPTIONS
	if (Pipeline->RenderPipelineReflection)
	{
		PipelineStats.PSOPerformanceStats = Stats->GetPipelinePerformanceStats(Pipeline->RenderPipelineReflection.GetPtr());
		
		if (Pipeline->ComputePipelineReflection)
		{
			NSDictionary* ComputePSO = Stats->GetPipelinePerformanceStats(Pipeline->ComputePipelineReflection.GetPtr());
			if (ComputePSO)
			{
				NSMutableDictionary* Dict = [[NSMutableDictionary new] autorelease];
				[Dict setObject:ComputePSO forKey:@"Compute Shader"];
				if (PipelineStats.PSOPerformanceStats)
				{
					[Dict addEntriesFromDictionary:PipelineStats.PSOPerformanceStats];
				}
				PipelineStats.PSOPerformanceStats = Dict;
			}
		}
	}
	else if (Pipeline->ComputePipelineReflection)
	{
		NSDictionary* Dict = Stats->GetPipelinePerformanceStats(Pipeline->ComputePipelineReflection.GetPtr());
		if (Dict)
		{
			PipelineStats.PSOPerformanceStats = [NSDictionary dictionaryWithObject:Dict forKey:@"Compute Shader"];
		}
	}
#endif
	
	FMetalProfiler::GetProfiler()->DumpPipeline(Pipeline);
}

FMetalOperationStats::FMetalOperationStats(char const* DrawCall, uint64 InGPUThreadIndex, uint32 InStartPoint, uint32 InEndPoint, uint32 InRHIPrimitives, uint32 InRHIVertices, uint32 InRHIInstances)
{
	Name = DrawCall;
	CmdBufferStats = nullptr;
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
	
	StartPoint = InStartPoint;
	EndPoint = InEndPoint;
	DrawStats = nullptr;
	RHIPrimitives = InRHIPrimitives;
	RHIVertices = InRHIVertices;
	RHIInstances = InRHIInstances;
}
	
FMetalOperationStats::FMetalOperationStats(char const* DrawCall, uint64 InGPUThreadIndex, uint32 InStartPoint, uint32 InEndPoint)
{
	Name = DrawCall;
	CmdBufferStats = nullptr;
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
	
	StartPoint = InStartPoint;
	EndPoint = InEndPoint;
	DrawStats = nullptr;
	RHIPrimitives = 0;
	RHIVertices = 0;
	RHIInstances = 0;
}

FMetalOperationStats::~FMetalOperationStats()
{
	delete DrawStats;
}

void FMetalOperationStats::Start(mtlpp::CommandBuffer const& Buffer)
{
	check(!DrawStats);
	check(CmdBufferStats);
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	DrawStats = Stats->CreateDrawStats(CmdBufferStats, (EMetalSamples)StartPoint, (EMetalSamples)EndPoint, RHIPrimitives, RHIVertices);
	check(DrawStats);
}
	
void FMetalOperationStats::End(mtlpp::CommandBuffer const& Buffer)
{
	check(DrawStats);
	CPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	DrawStats->End();
}

void FMetalOperationStats::GetStats(FMetalPipelineStats& PipelineStats)
{
	check(DrawStats);
	
	PipelineStats = DrawStats->GetResult();
	GPUStartTime = PipelineStats.StartTime / 1000;
	GPUEndTime = GPUStartTime + (PipelineStats.DrawCallTime / 1000);
}

FMetalEncoderStats::FMetalEncoderStats(mtlpp::RenderCommandEncoder const& Encoder, uint64 InGPUThreadIndex)
{
	CmdBuffer = nullptr;
	CmdBufferStats = nullptr;
	
	StartPoint = EMetalSampleRenderEncoderStart;
	EndPoint = EMetalSampleRenderEncoderEnd;
	StartSample = nullptr;
	EndSample = nullptr;
	
	Name = Encoder.GetPtr().label;
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
}

FMetalEncoderStats::FMetalEncoderStats(mtlpp::BlitCommandEncoder const& Encoder, uint64 InGPUThreadIndex)
{
	CmdBuffer = nullptr;
	CmdBufferStats = nullptr;
	
	StartPoint = EMetalSampleBlitEncoderStart;
	EndPoint = EMetalSampleBlitEncoderEnd;
	StartSample = nullptr;
	EndSample = nullptr;
	
	Name = Encoder.GetPtr().label;
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
}

FMetalEncoderStats::FMetalEncoderStats(mtlpp::ComputeCommandEncoder const& Encoder, uint64 InGPUThreadIndex)
{
	CmdBuffer = nullptr;
	CmdBufferStats = nullptr;
	
	StartPoint = EMetalSampleComputeEncoderStart;
	EndPoint = EMetalSampleComputeEncoderEnd;
	StartSample = nullptr;
	EndSample = nullptr;
	
	Name = Encoder.GetPtr().label;
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
}

FMetalEncoderStats::~FMetalEncoderStats()
{
	[StartSample release];
	[EndSample release];
}

void FMetalEncoderStats::Start(mtlpp::CommandBuffer const& Buffer)
{
	check(!StartSample);
	check(!CmdBuffer);
	check(Buffer);
	check(CmdBufferStats);
	CmdBuffer = Buffer;
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	StartSample = [Stats->RegisterEncoderStatistics(CmdBufferStats, (EMetalSamples)StartPoint) retain];
}

void FMetalEncoderStats::End(mtlpp::CommandBuffer const& Buffer)
{
	check(!EndSample);
	check(Buffer.GetPtr() == CmdBuffer.GetPtr());
	check(CmdBufferStats);
	CPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	EndSample = [Stats->RegisterEncoderStatistics(CmdBufferStats, (EMetalSamples)EndPoint) retain];
}

void FMetalEncoderStats::EncodeDraw(char const* DrawCall, uint32 RHIPrimitives, uint32 RHIVertices, uint32 RHIInstances)
{
	check(CmdBuffer);
	FMetalOperationStats* Draw = new FMetalOperationStats(DrawCall, GPUThreadIndex, EMetalSampleBeforeDraw, EMetalSampleAfterDraw, RHIPrimitives, RHIVertices, RHIInstances);
	Draw->CmdBufferStats = CmdBufferStats;
	Children.Add(Draw);
	Draw->Start(CmdBuffer);
	Draw->End(CmdBuffer);
}

void FMetalEncoderStats::EncodeBlit(char const* DrawCall)
{
	check(CmdBuffer);
	FMetalOperationStats* Draw = new FMetalOperationStats(DrawCall, GPUThreadIndex, EMetalSampleBeforeBlit, EMetalSampleAfterBlit);
	Draw->CmdBufferStats = CmdBufferStats;
	Children.Add(Draw);
	Draw->Start(CmdBuffer);
	Draw->End(CmdBuffer);
	
}

void FMetalEncoderStats::EncodeDispatch(char const* DrawCall)
{
	check(CmdBuffer);
	FMetalOperationStats* Draw = new FMetalOperationStats(DrawCall, GPUThreadIndex, EMetalSampleBeforeCompute, EMetalSampleAfterCompute);
	Draw->CmdBufferStats = CmdBufferStats;
	Children.Add(Draw);
	Draw->Start(CmdBuffer);
	Draw->End(CmdBuffer);
}

void FMetalEncoderStats::EncodePipeline(FMetalShaderPipeline* PipelineStat)
{
	check(CmdBuffer);
	FMetalShaderPipelineStats* Draw = new FMetalShaderPipelineStats(PipelineStat, GPUThreadIndex);
	Draw->CmdBufferStats = CmdBufferStats;
	Children.Add(Draw);
	Draw->Start(CmdBuffer);
	Draw->End(CmdBuffer);
}

void FMetalEncoderStats::GetStats(FMetalPipelineStats& PipelineStats)
{
	check(StartSample && EndSample);
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	
	Stats->ConvertSamplesToPipelineStats(StartSample, EndSample, PipelineStats);
	GPUStartTime = PipelineStats.StartTime / 1000;
	GPUEndTime = GPUStartTime + (PipelineStats.DrawCallTime / 1000);
}
#endif

FMetalCommandBufferStats::FMetalCommandBufferStats(mtlpp::CommandBuffer const& Buffer, uint64 InGPUThreadIndex)
{
	CmdBuffer = Buffer;
#if METAL_STATISTICS
	CmdBufferStats = nullptr;
	ActiveEncoderStats = nullptr;
	IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
	if (Stats)
	{
		CmdBufferStats = Stats->BeginCommandBufferStatistics(CmdBuffer.GetPtr());
	}
#endif
	
	Name = FString::Printf(TEXT("CommandBuffer: %s"), *FString(CmdBuffer.GetLabel().GetPtr()));
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = InGPUThreadIndex;
	
	Start(Buffer);
}

FMetalCommandBufferStats::~FMetalCommandBufferStats()
{
#if METAL_STATISTICS
	check(!ActiveEncoderStats);
	[CmdBufferStats release];
	CmdBufferStats = nil;
#endif
}

void FMetalCommandBufferStats::Start(mtlpp::CommandBuffer const& Buffer)
{
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
	
	if (!FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
	{
		CmdBuffer.AddScheduledHandler(^(const mtlpp::CommandBuffer &) {
			GPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
		});
	}
}

void FMetalCommandBufferStats::End(mtlpp::CommandBuffer const& Buffer)
{
#if METAL_STATISTICS
	check(!ActiveEncoderStats);
#endif
	check(Buffer.GetPtr() == CmdBuffer.GetPtr());
	
	bool const bTracing = FMetalProfiler::GetProfiler() && FMetalProfiler::GetProfiler()->TracingEnabled();
	CmdBuffer.AddCompletedHandler(^(const mtlpp::CommandBuffer & InnerBuffer) {
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCommandBufferTimes))
		{
			const CFTimeInterval GpuTimeSeconds = InnerBuffer.GetGpuStartTime();
			GPUStartTime = GpuTimeSeconds * 1000000.0;
			
			const CFTimeInterval GpuEndTimeSeconds = InnerBuffer.GetGpuEndTime();
			GPUEndTime = GpuEndTimeSeconds * 1000000.0;
		}
		else
		{
			GPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
		}
		if (bTracing)
		{
			FMetalProfiler::GetProfiler()->AddCommandBuffer(this);
		}
		else
		{
			delete this;
		}
	});
	
	CPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
}

#if METAL_STATISTICS
void FMetalCommandBufferStats::BeginEncoder(mtlpp::RenderCommandEncoder const& Encoder)
{
	check(!ActiveEncoderStats);
	
	ActiveEncoderStats = new FMetalEncoderStats(Encoder, GPUThreadIndex);
	ActiveEncoderStats->CmdBufferStats = CmdBufferStats;
	Children.Add(ActiveEncoderStats);
	ActiveEncoderStats->Start(CmdBuffer);
}

void FMetalCommandBufferStats::BeginEncoder(mtlpp::BlitCommandEncoder const& Encoder)
{
	check(!ActiveEncoderStats);
	
	ActiveEncoderStats = new FMetalEncoderStats(Encoder, GPUThreadIndex);
	ActiveEncoderStats->CmdBufferStats = CmdBufferStats;
	Children.Add(ActiveEncoderStats);
	ActiveEncoderStats->Start(CmdBuffer);
}

void FMetalCommandBufferStats::BeginEncoder(mtlpp::ComputeCommandEncoder const& Encoder)
{
	check(!ActiveEncoderStats);
	
	ActiveEncoderStats = new FMetalEncoderStats(Encoder, GPUThreadIndex);
	ActiveEncoderStats->CmdBufferStats = CmdBufferStats;
	Children.Add(ActiveEncoderStats);
	ActiveEncoderStats->Start(CmdBuffer);
}

void FMetalCommandBufferStats::EndEncoder(mtlpp::RenderCommandEncoder const& Encoder)
{
	check(ActiveEncoderStats);
	
	ActiveEncoderStats->End(CmdBuffer);
	ActiveEncoderStats = nullptr;
}

void FMetalCommandBufferStats::EndEncoder(mtlpp::BlitCommandEncoder const& Encoder)
{
	check(ActiveEncoderStats);
	
	ActiveEncoderStats->End(CmdBuffer);
	ActiveEncoderStats = nullptr;
}

void FMetalCommandBufferStats::EndEncoder(mtlpp::ComputeCommandEncoder const& Encoder)
{
	check(ActiveEncoderStats);
	
	ActiveEncoderStats->End(CmdBuffer);
	ActiveEncoderStats = nullptr;
}

void FMetalCommandBufferStats::GetStats(FMetalPipelineStats& PipelineStats)
{
	
}
#endif

#pragma mark -- FMetalProfiler
// ----------------------------------------------------------------


FMetalProfiler* FMetalProfiler::Self = nullptr;
static FMetalViewportPresentHandler PresentHandler = ^(uint32 DisplayID, double OutputSeconds, double OutputDuration){
	FMetalProfiler* Profiler = FMetalProfiler::GetProfiler();
	Profiler->AddDisplayVBlank(DisplayID, OutputSeconds, OutputDuration);
};

FMetalDisplayStats::FMetalDisplayStats(uint32 DisplayID, double OutputSeconds, double Duration)
{
	Name = TEXT("V-Blank");
	
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	GPUThreadIndex = DisplayID;
	
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	CPUEndTime = CPUStartTime+1;
	
	GPUStartTime = OutputSeconds * 1000000.0;
	GPUEndTime = GPUStartTime + (Duration * 1000000.0);
}
FMetalDisplayStats::~FMetalDisplayStats()
{
}

void FMetalDisplayStats::Start(mtlpp::CommandBuffer const& Buffer)
{
}
void FMetalDisplayStats::End(mtlpp::CommandBuffer const& Buffer)
{
}
#if METAL_STATISTICS
void FMetalDisplayStats::GetStats(FMetalPipelineStats& PipelineStats)
{
}
#endif

FMetalCPUStats::FMetalCPUStats(FString const& InName)
{
	Name = InName;
	
	CPUThreadIndex = 0;
	GPUThreadIndex = 0;
	
	CPUStartTime = 0;
	CPUEndTime = 0;
	
	GPUStartTime = 0;
	GPUEndTime = 0;
}
FMetalCPUStats::~FMetalCPUStats()
{
	
}

void FMetalCPUStats::Start(void)
{
	CPUThreadIndex = FPlatformTLS::GetCurrentThreadId();
	CPUStartTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
	
}
void FMetalCPUStats::End(void)
{
	CPUEndTime = FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0;
}

void FMetalCPUStats::Start(mtlpp::CommandBuffer const& Buffer)
{
	
}
void FMetalCPUStats::End(mtlpp::CommandBuffer const& Buffer)
{
	
}
#if METAL_STATISTICS
void FMetalCPUStats::GetStats(FMetalPipelineStats& PipelineStats)
{
	
}
#endif

void FMetalProfiler::AddDisplayVBlank(uint32 DisplayID, double OutputSeconds, double OutputDuration)
{
	if (GIsRHIInitialized && bEnabled)
	{
		FScopeLock Lock(&Mutex);
		DisplayStats.Add(new FMetalDisplayStats(DisplayID, OutputSeconds, OutputDuration));
	}
}

FMetalProfiler::FMetalProfiler(FMetalContext* Context)
: FMetalGPUProfiler(Context)
#if METAL_STATISTICS
, NewCounters([NSMutableArray new])
, StatisticsAPI(Context->GetCommandQueue().GetStatistics())
#endif
, bEnabled(false)
{
	NumFramesToCapture = -1;
	CaptureFrameNumber = 0;
	
	bRequestStartCapture = false;
	bRequestStopCapture = false;
	
	if (FPlatformRHIFramePacer::IsEnabled())
	{
		FPlatformRHIFramePacer::AddHandler(PresentHandler);
	}
}

FMetalProfiler::~FMetalProfiler()
{
	check(bEnabled == false);
	if (FPlatformRHIFramePacer::IsEnabled())
	{
		FPlatformRHIFramePacer::RemoveHandler(PresentHandler);
	}
	
#if METAL_STATISTICS
	[NewCounters release];
#endif
}

FMetalProfiler* FMetalProfiler::CreateProfiler(FMetalContext *InContext)
{
	if (!Self)
	{
		Self = new FMetalProfiler(InContext);
	}
	return Self;
}

FMetalProfiler* FMetalProfiler::GetProfiler()
{
	return Self;
}

#if METAL_STATISTICS
IMetalStatistics* FMetalProfiler::GetStatistics()
{
	IMetalStatistics* Stats = nullptr;
	if (Self)
	{
		Stats = Self->StatisticsAPI;
	}
	return Stats;
}
#endif

void FMetalProfiler::DestroyProfiler()
{
	delete Self;
}

void FMetalProfiler::BeginCapture(int InNumFramesToCapture)
{
	check(IsInGameThread());
	
	NumFramesToCapture = InNumFramesToCapture;
	CaptureFrameNumber = 0;
	
	bRequestStartCapture = true;
}

void FMetalProfiler::EndCapture()
{
	bRequestStopCapture = true;
}

bool FMetalProfiler::TracingEnabled() const
{
	return bEnabled;
}

void FMetalProfiler::BeginFrame()
{
	if (MetalGPUProfilerIsInSafeThread())
	{
		if (bRequestStartCapture && !bEnabled)
		{
#if METAL_STATISTICS
			if (StatisticsAPI && NewCounters)
			{
				StatisticsAPI->FinishSamplingStatistics();
				StatisticsAPI->BeginSamplingStatistics(NewCounters);
				Context->SubmitCommandBufferAndWait();
			}
#endif
			
			bEnabled = true;
			bRequestStartCapture = false;
		}
	}
	
	FMetalGPUProfiler::BeginFrame();
	
	if (MetalGPUProfilerIsInSafeThread() && GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FMetalProfiler::EndFrame()
{
	if (MetalGPUProfilerIsInSafeThread() && GetEmitDrawEvents())
	{
#if METAL_STATISTICS
		FMetalEventStats* Event = bEnabled && ActiveEvents.Num() > 0 ? ActiveEvents.Last() : nullptr;
		if (Event)
		{
#if PLATFORM_MAC
			Event->DriverStats = FPlatformMisc::GetGPUDescriptors()[GetMetalDeviceContext().GetDeviceIndex()].GetPerformanceStatistics();
#elif METAL_STATISTICS
			if(Context->GetCommandQueue().GetStatistics())
			{
				Event->DriverStats = Context->GetCommandQueue().GetStatistics()->GetDriverMonitorStatistics(GetMetalDeviceContext().GetDeviceIndex());
			}
#endif
		}
#endif
		PopEvent();
	}
	
	FMetalGPUProfiler::EndFrame();
	
	if (MetalGPUProfilerIsInSafeThread() && bEnabled)
	{
		CaptureFrameNumber++;
		if (bRequestStopCapture || (NumFramesToCapture > 0 && CaptureFrameNumber >= NumFramesToCapture))
		{
			bRequestStopCapture = false;
			NumFramesToCapture = -1;
			bEnabled = false;
			SaveTrace();
		}
	}
}

void FMetalProfiler::EncodeDraw(FMetalCommandBufferStats* CmdBufStats, char const* DrawCall, uint32 RHIPrimitives, uint32 RHIVertices, uint32 RHIInstances)
{
	if (MetalGPUProfilerIsInSafeThread())
		FMetalGPUProfiler::RegisterGPUWork(RHIPrimitives, RHIVertices);

#if METAL_STATISTICS
	if (StatisticsAPI)
		CmdBufStats->ActiveEncoderStats->EncodeDraw(DrawCall, RHIPrimitives, RHIVertices, RHIInstances);
#endif
}

void FMetalProfiler::EncodeBlit(FMetalCommandBufferStats* CmdBufStats, char const* DrawCall)
{
	if (MetalGPUProfilerIsInSafeThread())
		FMetalGPUProfiler::RegisterGPUWork(1, 1);
	
#if METAL_STATISTICS
	if (StatisticsAPI)
		CmdBufStats->ActiveEncoderStats->EncodeBlit(DrawCall);
#endif
}

void FMetalProfiler::EncodeDispatch(FMetalCommandBufferStats* CmdBufStats, char const* DrawCall)
{
	if (MetalGPUProfilerIsInSafeThread())
		FMetalGPUProfiler::RegisterGPUWork(1, 1);
	
#if METAL_STATISTICS
	if (StatisticsAPI)
		CmdBufStats->ActiveEncoderStats->EncodeDispatch(DrawCall);
#endif
}

#if METAL_STATISTICS
void FMetalProfiler::EncodePipeline(FMetalCommandBufferStats* CmdBufStats, FMetalShaderPipeline* PipelineStat)
{
	if (StatisticsAPI)
		CmdBufStats->ActiveEncoderStats->EncodePipeline(PipelineStat);
}

void FMetalProfiler::BeginEncoder(FMetalCommandBufferStats* CmdBufStats, mtlpp::RenderCommandEncoder const& Encoder)
{
	if (StatisticsAPI)
		CmdBufStats->BeginEncoder(Encoder);
}

void FMetalProfiler::BeginEncoder(FMetalCommandBufferStats* CmdBufStats, mtlpp::BlitCommandEncoder const& Encoder)
{
	if (StatisticsAPI)
		CmdBufStats->BeginEncoder(Encoder);
}

void FMetalProfiler::BeginEncoder(FMetalCommandBufferStats* CmdBufStats, mtlpp::ComputeCommandEncoder const& Encoder)
{
	if (StatisticsAPI)
		CmdBufStats->BeginEncoder(Encoder);
}

void FMetalProfiler::EndEncoder(FMetalCommandBufferStats* CmdBufStats, mtlpp::RenderCommandEncoder const& Encoder)
{
	if (StatisticsAPI)
		CmdBufStats->EndEncoder(Encoder);
}

void FMetalProfiler::EndEncoder(FMetalCommandBufferStats* CmdBufStats, mtlpp::BlitCommandEncoder const& Encoder)
{
	if (StatisticsAPI)
		CmdBufStats->EndEncoder(Encoder);
}

void FMetalProfiler::EndEncoder(FMetalCommandBufferStats* CmdBufStats, mtlpp::ComputeCommandEncoder const& Encoder)
{
	if (StatisticsAPI)
		CmdBufStats->EndEncoder(Encoder);
}

void FMetalProfiler::AddCounter(NSString* Counter, EMTLCounterType Type)
{
	check (StatisticsAPI);
	if (![NewCounters containsObject:Counter])
	{
		[NewCounters addObject:Counter];
		CounterTypes.Add(FString(Counter), Type);
	}
}

void FMetalProfiler::RemoveCounter(NSString* Counter)
{
	check (StatisticsAPI);
	[NewCounters removeObject:Counter];
	CounterTypes.Remove(FString(Counter));
}

void FMetalProfiler::DumpPipeline(FMetalShaderPipeline* PipelineStat)
{
	Pipelines.Add(PipelineStat);
}

#endif

FMetalCPUStats* FMetalProfiler::AddCPUStat(FString const& Name)
{
	if (GIsRHIInitialized && bEnabled)
	{
		FScopeLock Lock(&Mutex);
		FMetalCPUStats* Stat = new FMetalCPUStats(Name);
		CPUStats.Add(Stat);
		return Stat;
	}
	else
	{
		return nullptr;
	}
}

FMetalCommandBufferStats* FMetalProfiler::AllocateCommandBuffer(const mtlpp::CommandBuffer &Buffer, uint64 GPUThreadIndex)
{
	return new FMetalCommandBufferStats(Buffer, GPUThreadIndex);
}

void FMetalProfiler::AddCommandBuffer(FMetalCommandBufferStats *CommandBuffer)
{
	if (GIsRHIInitialized)
	{
		FScopeLock Lock(&Mutex);
		TracedBuffers.Add(CommandBuffer);
	}
	else
	{
		delete CommandBuffer;
	}
}

void FMetalProfiler::PushEvent(const TCHAR *Name, FColor Color)
{
#if METAL_STATISTICS
	if (MetalGPUProfilerIsInSafeThread() && bEnabled && StatisticsAPI)
	{
		if (!Context->GetCurrentCommandBuffer().GetPtr() || !StatisticsAPI->GetLastStatisticsSample(Context->GetCurrentCommandBuffer().GetPtr()))
		{
			Context->GetCurrentRenderPass().InsertDebugEncoder();
		}

		FMetalEventStats* Event = new FMetalEventStats(Name, Color);
		ActiveEvents.Add(Event);
		Event->Start(Context->GetCurrentCommandBuffer());
	}
#endif
	FMetalGPUProfiler::PushEvent(Name, Color);
}

void FMetalProfiler::PopEvent()
{
#if METAL_STATISTICS
	if (MetalGPUProfilerIsInSafeThread() && bEnabled && StatisticsAPI&& ActiveEvents.Num())
	{
		if (!Context->GetCurrentCommandBuffer().GetPtr() || !StatisticsAPI->GetLastStatisticsSample(Context->GetCurrentCommandBuffer().GetPtr()))
		{
			Context->GetCurrentRenderPass().InsertDebugEncoder();
		}
		
		FMetalEventStats* Event = ActiveEvents.Pop();
		Event->End(Context->GetCurrentCommandBuffer());
		FrameEvents.Add(Event);
	}
#endif
	FMetalGPUProfiler::PopEvent();
}

void FMetalProfiler::SaveTrace()
{
	Context->SubmitCommandBufferAndWait();
	{
		FScopeLock Lock(&Mutex);
		
		TSet<uint32> ThreadIDs;
		
		for (FMetalCommandBufferStats* CmdBufStats : TracedBuffers)
		{
			ThreadIDs.Add(CmdBufStats->CPUThreadIndex);
			
			for (IMetalStatsScope* ES : CmdBufStats->Children)
			{
				ThreadIDs.Add(ES->CPUThreadIndex);
				
				for (IMetalStatsScope* DS : ES->Children)
				{
					ThreadIDs.Add(DS->CPUThreadIndex);
				}
			}
		}
		
		TSet<uint32> Displays;
		for (FMetalDisplayStats* DisplayStat : DisplayStats)
		{
			ThreadIDs.Add(DisplayStat->CPUThreadIndex);
			Displays.Add(DisplayStat->GPUThreadIndex);
		}
		
		for (FMetalCPUStats* CPUStat : CPUStats)
		{
			ThreadIDs.Add(CPUStat->CPUThreadIndex);
		}
		
		FString Filename = FString::Printf(TEXT("Profile(%s)"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		FString TracingRootPath = FPaths::ProfilingDir() + TEXT("Traces/");
		FString OutputFilename = TracingRootPath + Filename + TEXT(".json");
		
		FArchive* OutputFile = IFileManager::Get().CreateFileWriter(*OutputFilename);
		
		WriteString(OutputFile, R"({"traceEvents":[)" "\n");
		
		int32 SortIndex = 0; // Lower numbers result in higher position in the visualizer.
		const uint32 Pid = FPlatformProcess::GetCurrentProcessId();
		
		for (int32 GPUIndex = 0; GPUIndex <= 0/*MaxGPUIndex*/; ++GPUIndex)
		{
			FString Output = FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_name\", \"args\":{\"name\":\"GPU %d\"}},{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_sort_index\", \"args\":{\"sort_index\": %d}},\n"),
											 Pid, GPUIndex, GPUIndex, Pid, GPUIndex, SortIndex
											 );
			
			WriteString(OutputFile, TCHAR_TO_UTF8(*Output));
			SortIndex++;
			
			Output = FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_name\", \"args\":{\"name\":\"Render Events %d\"}},{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_sort_index\", \"args\":{\"sort_index\": %d}},\n"),
									 Pid, GPUIndex+SortIndex, GPUIndex, Pid, GPUIndex+SortIndex, SortIndex
									 );
			
			WriteString(OutputFile, TCHAR_TO_UTF8(*Output));
			SortIndex++;
			
			Output = FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_name\", \"args\":{\"name\":\"Driver Stats %d\"}},{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_sort_index\", \"args\":{\"sort_index\": %d}},\n"),
									 Pid, GPUIndex+SortIndex, GPUIndex, Pid, GPUIndex+SortIndex, SortIndex
									 );
			
			WriteString(OutputFile, TCHAR_TO_UTF8(*Output));
			SortIndex++;
			
			for (uint32 Display : Displays)
			{
				Output = FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_name\", \"args\":{\"name\":\"Display %d\"}},{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_sort_index\", \"args\":{\"sort_index\": %d}},\n"),
										 Pid, Display + SortIndex, SortIndex - 3, Pid, Display + SortIndex, SortIndex
										 );
				
				WriteString(OutputFile, TCHAR_TO_UTF8(*Output));
				SortIndex++;
			}
		}
		
		static const uint32 BufferSize = 128;
		char Buffer[BufferSize];
		for (uint32 CPUIndex : ThreadIDs)
		{
			bool bThreadName = false;
			pthread_t PThread = pthread_from_mach_thread_np((mach_port_t)CPUIndex);
			if (PThread)
			{
				if (!pthread_getname_np(PThread,Buffer,BufferSize))
				{
					bThreadName = true;
				}
			}
			if (!bThreadName)
			{
				sprintf(Buffer, "Thread %d", CPUIndex);
			}
			
			FString Output = FString::Printf(TEXT("{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_name\", \"args\":{\"name\":\"%s\"}},{\"pid\":%d, \"tid\":%d, \"ph\": \"M\", \"name\": \"thread_sort_index\", \"args\":{\"sort_index\": %d}},\n"),
											 Pid, CPUIndex, UTF8_TO_TCHAR(Buffer), Pid, CPUIndex, SortIndex
											 );
			
			
			WriteString(OutputFile, TCHAR_TO_UTF8(*Output));
			SortIndex++;
		}
		
#if METAL_STATISTICS
		for (FMetalEventStats* Event : FrameEvents)
		{
			WriteString(OutputFile, TCHAR_TO_UTF8(*Event->GetJSONRepresentation(Pid)));
			
			if (Event->DriverStats.Num())
			{
				uint64 ChildStartCallTime = Event->CPUStartTime;
				uint64 ChildDrawCallTime = Event->CPUEndTime - Event->CPUStartTime;
				
				FString DriverStats;
				for(auto const& Pair : Event->DriverStats)
				{
					DriverStats += FString::Printf(TEXT(",\"%s\": %0.8f"), *Pair.Key, Pair.Value);
				}
				
				FString Output = FString::Printf(TEXT("{\"pid\":%d, \"tid\":2, \"ph\": \"X\", \"name\": \"Driver Stats\", \"ts\": %llu, \"dur\": %llu, \"args\":{\"num_child\":%d %s}},\n"),
											  Pid,
											  ChildStartCallTime,
											  ChildDrawCallTime,
											  Event->DriverStats.Num(),
											  *DriverStats
											  );
				
				WriteString(OutputFile, TCHAR_TO_UTF8(*Output));
			}
			
			delete Event;
		}
		FrameEvents.Empty();
#endif
		
		for (FMetalCommandBufferStats* CmdBufStats : TracedBuffers)
		{
			WriteString(OutputFile, TCHAR_TO_UTF8(*CmdBufStats->GetJSONRepresentation(Pid)));
			
			for (IMetalStatsScope* ES : CmdBufStats->Children)
			{
				WriteString(OutputFile, TCHAR_TO_UTF8(*ES->GetJSONRepresentation(Pid)));
				
				uint64 PrevTime = ES->GPUStartTime;
				for (IMetalStatsScope* DS : ES->Children)
				{
					WriteString(OutputFile, TCHAR_TO_UTF8(*DS->GetJSONRepresentation(Pid)));
					if (!DS->GPUStartTime)
					{
						DS->GPUStartTime = FMath::Max(PrevTime, DS->GPUStartTime);
						DS->GPUEndTime = DS->GPUStartTime + 1llu;
						WriteString(OutputFile, TCHAR_TO_UTF8(*DS->GetJSONRepresentation(Pid)));
					}
					PrevTime = DS->GPUEndTime;
				}
			}
			
			delete CmdBufStats;
		}
		TracedBuffers.Empty();
		
		for (FMetalDisplayStats* DisplayStat : DisplayStats)
		{
			DisplayStat->GPUThreadIndex += 3;
			WriteString(OutputFile, TCHAR_TO_UTF8(*DisplayStat->GetJSONRepresentation(Pid)));
			delete DisplayStat;
		}
		DisplayStats.Empty();
		
		for (FMetalCPUStats* CPUStat : CPUStats)
		{
			WriteString(OutputFile, TCHAR_TO_UTF8(*CPUStat->GetJSONRepresentation(Pid)));
			delete CPUStat;
		}
		CPUStats.Empty();
		
		// All done
		
		WriteString(OutputFile, "{}]}");
		
		OutputFile->Close();
		
#if METAL_STATISTICS && METAL_DEBUG_OPTIONS
		FString OutputDir = TracingRootPath + Filename + TEXT("/Pipelines/");
		if (Pipelines.Num())
		{
			FString FileName = OutputDir + TEXT("ue4_stdlib.metal");
			FArchive* PipelineFile = IFileManager::Get().CreateFileWriter(*FileName);
			PipelineFile->Serialize((void*)ue4_stdlib_metal, sizeof(ANSICHAR)*ue4_stdlib_metal_len);
			PipelineFile->Close();
		}
		for (auto Ptr : Pipelines)
		{
			FString PipelineName;
			if (Ptr->RenderPipelineState)
			{
				PipelineName = Ptr->RenderPipelineState.GetLabel().GetPtr();
				if (Ptr->ComputePipelineState)
				{
					PipelineName += TEXT("+") + FString(Ptr->ComputePipelineState.GetLabel().GetPtr());
				}
			}
			else if (Ptr->ComputePipelineState)
			{
				PipelineName = FString(Ptr->ComputePipelineState.GetLabel().GetPtr());
			}
			
			FString FileName = OutputDir + PipelineName + TEXT(".txt");
			FArchive* PipelineFile = IFileManager::Get().CreateFileWriter(*FileName);
			
			WriteString(PipelineFile, TCHAR_TO_UTF8(*PipelineName));
			WriteString(PipelineFile, "\n");
			
			if (Ptr->RenderDesc)
			{
				WriteString(PipelineFile, "\n\n******************* Render Pipeline Descriptor:\n");
				WriteString(PipelineFile, [[Ptr->RenderDesc.GetPtr() description] UTF8String]);
			}
			if (Ptr->VertexSource)
			{
				FString Name;
				if (Ptr->RenderDesc)
				{
					Name = Ptr->RenderDesc.GetVertexFunction().GetName().GetPtr();
					Name += TEXT(".metal");
				}
				else
				{
					Name = Ptr->RenderPipelineState.GetLabel().GetPtr();
					Name += TEXT(".vertex.metal");
				}
				
				FString ShaderName = OutputDir + Name;
				FArchive* ShaderFile = IFileManager::Get().CreateFileWriter(*ShaderName);
				WriteString(ShaderFile, [Ptr->VertexSource.GetPtr() UTF8String]);
				ShaderFile->Close();
			}
			if (Ptr->FragmentSource)
			{
				FString Name;
				if (Ptr->RenderDesc)
				{
					Name = Ptr->RenderDesc.GetFragmentFunction().GetName().GetPtr();
					Name += TEXT(".metal");
				}
				else
				{
					Name = Ptr->RenderPipelineState.GetLabel().GetPtr();
					Name += TEXT(".fragment.metal");
				}
				
				FString ShaderName = OutputDir + Name;
				FArchive* ShaderFile = IFileManager::Get().CreateFileWriter(*ShaderName);
				WriteString(ShaderFile, [Ptr->FragmentSource.GetPtr() UTF8String]);
				ShaderFile->Close();
			}
			if (Ptr->ComputeDesc)
			{
				WriteString(PipelineFile, "\n\n******************* Compute Pipeline Descriptor:\n");
				WriteString(PipelineFile, [[Ptr->ComputeDesc.GetPtr() description] UTF8String]);
			}
			if (Ptr->ComputeSource)
			{
				FString Name;
				if (Ptr->ComputeDesc)
				{
					Name = Ptr->ComputeDesc.GetComputeFunction().GetName().GetPtr();
					Name += TEXT(".metal");
				}
				else
				{
					Name = Ptr->ComputePipelineState.GetLabel().GetPtr();
					Name += TEXT(".compute.metal");
				}
				
				FString ShaderName = OutputDir + Name;
				FArchive* ShaderFile = IFileManager::Get().CreateFileWriter(*ShaderName);
				WriteString(ShaderFile, [Ptr->ComputeSource.GetPtr() UTF8String]);
				ShaderFile->Close();
			}
			
			PipelineFile->Close();
		}
		Pipelines.Empty();
#endif
	}
}

static void HandleMetalProfileCommand(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	if (Args.Num() < 1)
	{
		return;
	}
	FString Param = Args[0];
	if (Param == TEXT("START"))
	{
		FMetalProfiler::GetProfiler()->BeginCapture();
	}
	else if (Param == TEXT("STOP"))
	{
		FMetalProfiler::GetProfiler()->EndCapture();
	}
#if METAL_STATISTICS
	else if (Param == TEXT("LIST"))
	{
		IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
		if (Stats)
		{
			NSArray* Array = Stats->GetSupportedCounters();
			if (Array)
			{
				Ar.Logf(TEXT("Supported Counters:"));
				for (NSString* Str in Array)
				{
					Ar.Logf(TEXT("  %s"), *FString(Str));
				}
			}
		}
	}
	else if (Param == TEXT("LISTACTIVE"))
	{
		IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
		if (Stats)
		{
			NSArray* Array = Stats->GetActiveCounters();
			if (Array)
			{
				Ar.Logf(TEXT("Active Counters:"));
				for (NSString* Str in Array)
				{
					Ar.Logf(TEXT("  %s"), *FString(Str));
				}
			}
		}
	}
	else if (Param == TEXT("ADD"))
	{
		IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
		if (Stats)
		{
			NSArray* Array = Stats->GetActiveCounters();
			check (Array);
			
			FString NewCounter = Args[1];
			if (![Array containsObject:NewCounter.GetNSString()])
			{
				FString TypeName = Args.Num() > 2 ? Args[2] : TEXT("");
				FMetalProfiler::EMTLCounterType Type = FMetalProfiler::EMTLCounterTypeStartEnd;
				if (TypeName == TEXT("LAST"))
				{
					Type = FMetalProfiler::EMTLCounterTypeLast;
				}
				else if (TypeName == TEXT("DIFF"))
				{
					Type = FMetalProfiler::EMTLCounterTypeDifference;
				}
				FMetalProfiler::GetProfiler()->AddCounter(NewCounter.GetNSString(), Type);
			}
		}
	}
	else if (Param == TEXT("REMOVE"))
	{
		IMetalStatistics* Stats = FMetalProfiler::GetStatistics();
		if (Stats)
		{
			NSArray* Array = Stats->GetActiveCounters();
			check (Array);
			
			FString NewCounter = Args[1];
			if ([Array containsObject:NewCounter.GetNSString()])
			{
				FMetalProfiler::GetProfiler()->RemoveCounter(NewCounter.GetNSString());
			}
		}
	}
#endif
	else
	{
		int32 CaptureFrames = 0;
		if (FParse::Value(*Param, TEXT("FRAMES="), CaptureFrames))
		{
			FMetalProfiler::GetProfiler()->BeginCapture(CaptureFrames);
		}
	}
}

static FAutoConsoleCommand HandleMetalProfilerCmd(
	TEXT("MetalProfiler"),
	TEXT("Starts or stops Metal profiler"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleMetalProfileCommand)
);

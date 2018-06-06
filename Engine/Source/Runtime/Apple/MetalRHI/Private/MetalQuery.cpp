// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalQuery.cpp: Metal query RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalLLM.h"
#include "MetalCommandBuffer.h"

#if METAL_DEBUG_OPTIONS
extern int32 GMetalBufferZeroFill;
#endif

void FMetalQueryBufferPool::Allocate(FMetalQueryResult& NewQuery)
{
	FMetalQueryBuffer* QB = IsValidRef(CurrentBuffer) ? CurrentBuffer.GetReference() : GetCurrentQueryBuffer();
	
	uint32 Offset = Align(QB->WriteOffset, EQueryBufferAlignment);
	uint32 End = Align(Offset + EQueryResultMaxSize, EQueryBufferAlignment);
	
	if(Align(QB->WriteOffset, EQueryBufferAlignment) + EQueryResultMaxSize <= EQueryBufferMaxSize)
	{
		NewQuery.SourceBuffer = QB;
		NewQuery.Offset = Align(QB->WriteOffset, EQueryBufferAlignment);
		QB->WriteOffset = Align(QB->WriteOffset, EQueryBufferAlignment) + EQueryResultMaxSize;
	}
	else
	{
		UE_LOG(LogRHI, Warning, TEXT("Performance: Resetting render command encoder as query buffer offset: %d exceeds the maximum allowed: %d."), QB->WriteOffset, EQueryBufferMaxSize);
		Context->ResetRenderCommandEncoder();
		Allocate(NewQuery);
	}
}

void FMetalQueryBufferPool::ReleaseCurrentQueryBuffer()
{
	if (IsValidRef(CurrentBuffer) && CurrentBuffer->WriteOffset > 0)
	{
		CurrentBuffer.SafeRelease();
	}
}

FMetalQueryBuffer* FMetalQueryBufferPool::GetCurrentQueryBuffer()
{
	if(!IsValidRef(CurrentBuffer) || (CurrentBuffer->Buffer.GetStorageMode() != mtlpp::StorageMode::Shared && CurrentBuffer->WriteOffset > 0))
	{
		FMetalBuffer Buffer;
		if(Buffers.Num())
		{
			Buffer = Buffers.Pop();
		}
		else
		{
			LLM_SCOPE_METAL(ELLMTagMetal::Buffers);
			LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::Buffers);
#if PLATFORM_MAC
			METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), EQueryBufferMaxSize, mtlpp::ResourceOptions::StorageModeManaged)));
			Buffer = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Device, Context->GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(EQueryBufferMaxSize, GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions((mtlpp::ResourceOptions)(BUFFER_CACHE_MODE | mtlpp::ResourceOptions::HazardTrackingModeUntracked | mtlpp::ResourceOptions::StorageModeManaged)))), false);
			FMemory::Memzero((((uint8*)Buffer.GetContents())), EQueryBufferMaxSize);
			Buffer.DidModify(ns::Range(0, EQueryBufferMaxSize));
#else
			METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), EQueryBufferMaxSize, mtlpp::ResourceOptions::StorageModeShared)));
			Buffer = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Device, Context->GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(EQueryBufferMaxSize, GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions((mtlpp::ResourceOptions)(BUFFER_CACHE_MODE | mtlpp::ResourceOptions::HazardTrackingModeUntracked | mtlpp::ResourceOptions::StorageModeShared)))), false);
			FMemory::Memzero((((uint8*)Buffer.GetContents())), EQueryBufferMaxSize);
#endif
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
			MetalLLM::LogAllocBuffer(Context->GetDevice(), Buffer);
#endif
		}
		
		CurrentBuffer = new FMetalQueryBuffer(Context, MoveTemp(Buffer));
	}
	
	return CurrentBuffer.GetReference();
}

void FMetalQueryBufferPool::ReleaseQueryBuffer(FMetalBuffer& Buffer)
{
	Buffers.Add(MoveTemp(Buffer));
}

FMetalQueryBuffer::FMetalQueryBuffer(FMetalContext* InContext, FMetalBuffer InBuffer)
: Pool(InContext->GetQueryBufferPool())
, Buffer(MoveTemp(InBuffer))
, WriteOffset(0)
{
}

FMetalQueryBuffer::~FMetalQueryBuffer()
{
	if (GIsMetalInitialized)
	{
		if(Buffer)
		{
			TSharedPtr<FMetalQueryBufferPool, ESPMode::ThreadSafe> BufferPool = Pool.Pin();
			if (BufferPool.IsValid())
			{
				BufferPool->ReleaseQueryBuffer(Buffer);
			}
		}
	}
}

uint64 FMetalQueryBuffer::GetResult(uint32 Offset)
{
	uint64 Result = 0;
	@autoreleasepool
	{
		Result = *((uint64 const*)(((uint8*)Buffer.GetContents()) + Offset));
	}
    return Result;
}

bool FMetalCommandBufferFence::Wait(uint64 Millis)
{
	@autoreleasepool
	{
		if (CommandBufferFence)
		{
			bool bFinished = CommandBufferFence.Wait(Millis);
			FPlatformMisc::MemoryBarrier();
			return bFinished;
		}
		else
		{
			return true;
		}
	}
}
	
bool FMetalQueryResult::Wait(uint64 Millis)
{
	if(!bCompleted)
	{
		bCompleted = CommandBufferFence->Wait(Millis);
		
		return bCompleted;
	}
	return true;
}

uint64 FMetalQueryResult::GetResult()
{
	if(IsValidRef(SourceBuffer))
	{
		return SourceBuffer->GetResult(Offset);
	}
	return 0;
}

FMetalRenderQuery::FMetalRenderQuery(ERenderQueryType InQueryType)
: Type(InQueryType)
{
	Result = 0;
	bAvailable = false;
	Buffer.Offset = 0;
	Buffer.bCompleted = false;
	Buffer.bBatchFence = false;
}

FMetalRenderQuery::~FMetalRenderQuery()
{
	Buffer.SourceBuffer.SafeRelease();
	Buffer.Offset = 0;
}

void FMetalRenderQuery::Begin(FMetalContext* Context, TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> const& BatchFence)
{
	Buffer.CommandBufferFence.Reset();
	Buffer.SourceBuffer.SafeRelease();
	Buffer.Offset = 0;
	Buffer.bBatchFence = false;
	
	Result = 0;
	bAvailable = false;
	
	switch(Type)
	{
		case RQT_Occlusion:
		{
			// allocate our space in the current buffer
			Context->GetQueryBufferPool()->Allocate(Buffer);
			Buffer.bCompleted = false;
			
			if ((GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesCountingQueries))
			{
				Context->GetCurrentState().SetVisibilityResultMode(mtlpp::VisibilityResultMode::Counting, Buffer.Offset);
			}
			else
			{
				Context->GetCurrentState().SetVisibilityResultMode(mtlpp::VisibilityResultMode::Boolean, Buffer.Offset);
			}
			if (BatchFence.IsValid())
			{
				Buffer.CommandBufferFence = BatchFence;
				Buffer.bBatchFence = true;
			}
			else
			{
				Buffer.CommandBufferFence = MakeShareable(new FMetalCommandBufferFence);
			}
			break;
		}
		case RQT_AbsoluteTime:
		{
			break;
		}
		default:
		{
			check(0);
			break;
		}
	}
}

void FMetalRenderQuery::End(FMetalContext* Context)
{
	switch(Type)
	{
		case RQT_Occlusion:
		{
			// switch back to non-occlusion rendering
			check(Buffer.CommandBufferFence.IsValid());
			Context->GetCurrentState().SetVisibilityResultMode(mtlpp::VisibilityResultMode::Disabled, 0);
			
			// For unique, unbatched, queries insert the fence now
			if (!Buffer.bBatchFence)
			{
				Context->InsertCommandBufferFence(*(Buffer.CommandBufferFence));
			}
			break;
		}
		case RQT_AbsoluteTime:
		{
			// Reset the result availability state
			Buffer.SourceBuffer.SafeRelease();
			Buffer.Offset = 0;
			Buffer.bCompleted = false;
			Buffer.bBatchFence = false;
			Buffer.CommandBufferFence = MakeShareable(new FMetalCommandBufferFence);
			check(Buffer.CommandBufferFence.IsValid());
			
			Result = 0;
			bAvailable = false;
			
#if METAL_STATISTICS
			class IMetalStatistics* Stats = Context->GetCommandQueue().GetStatistics();
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
				
				// Insert the fence to wait on the current command buffer
				Context->InsertCommandBufferFence(*(Buffer.CommandBufferFence), [this, StatSample](mtlpp::CommandBuffer const&)
				{
					if (StatSample.Count > 0)
					{
						Result = (FPlatformTime::ToMilliseconds64(StatSample.Array[0]) * 1000.0);
					}
					[StatSample release];
				});
			}
			else
#endif
			{
				// Insert the fence to wait on the current command buffer
				Context->InsertCommandBufferFence(*(Buffer.CommandBufferFence), [this](mtlpp::CommandBuffer const&)
				{
					Result = (FPlatformTime::ToMilliseconds64(mach_absolute_time()) * 1000.0);
				});
				
				// Submit the current command buffer, marking this is as a break of a logical command buffer for render restart purposes
				// This is necessary because we use command-buffer completion to emulate timer queries as Metal has no such API
				Context->SubmitCommandsHint(EMetalSubmitFlagsCreateCommandBuffer|EMetalSubmitFlagsBreakCommandBuffer);
			}
			break;
		}
		default:
		{
			check(0);
			break;
		}
	}
}

FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery_RenderThread(class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType)
{
	@autoreleasepool {
	return GDynamicRHI->RHICreateRenderQuery(QueryType);
	}
}

FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	@autoreleasepool {
	FRenderQueryRHIRef Query;
	// AMD have subtleties to their completion handler routines that mean we don't seem able to reliably wait on command-buffers
	// until after a drawable present...
	static bool const bSupportsTimeQueries = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesAbsoluteTimeQueries);
	if (QueryType != RQT_AbsoluteTime || bSupportsTimeQueries)
	{
		Query = new FMetalRenderQuery(QueryType);
	}
	return Query;
	}
}

bool FMetalDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	@autoreleasepool {
	check(IsInRenderingThread());
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);
	
    if(!Query->bAvailable)
    {
		SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
		
		bool bOK = false;
		
		bool const bCmdBufferIncomplete = !Query->Buffer.bCompleted;
		
		// timer queries are used for Benchmarks which can stall a bit more
		uint64 WaitMS = (Query->Type == RQT_AbsoluteTime) ? 2000 : 500;
		if (bWait)
		{
			uint32 IdleStart = FPlatformTime::Cycles();
		
			bOK = Query->Buffer.Wait(WaitMS);
			
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
			
			// Never wait for a failed signal again.
			Query->bAvailable = Query->Buffer.bCompleted;
		}
		else
		{
			bOK = Query->Buffer.Wait(0);
		}
		
        if (bOK == false)
        {
			OutNumPixels = 0;
			UE_CLOG(bWait, LogMetal, Display, TEXT("Timed out while waiting for GPU to catch up. (%llu ms)"), WaitMS);
			return false;
        }
		
		if(Query->Type == RQT_Occlusion)
		{
			Query->Result = Query->Buffer.GetResult();
		}
		
		Query->Buffer.SourceBuffer.SafeRelease();
    }

	// at this point, we are ready to read the value!
	OutNumPixels = Query->Result;
    return true;
	}
}

// Occlusion/Timer queries.
void FMetalRHICommandContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	@autoreleasepool {
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);
	
	Query->Begin(Context, CommandBufferFence);
	}
}

void FMetalRHICommandContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	@autoreleasepool {
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);
	
	Query->End(Context);
	}
}

void FMetalRHICommandContext::RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch)
{
	check(!CommandBufferFence.IsValid());
	CommandBufferFence = MakeShareable(new FMetalCommandBufferFence);
}

void FMetalRHICommandContext::RHIEndOcclusionQueryBatch()
{
	check(CommandBufferFence.IsValid());
	Context->InsertCommandBufferFence(*CommandBufferFence);
	CommandBufferFence.Reset();
}

void FMetalDynamicRHI::RHICalibrateTimers()
{
	check(IsInRenderingThread());
#if METAL_STATISTICS
	FMetalContext& Context = ImmediateContext.GetInternalContext();
	if (Context.GetCommandQueue().GetStatistics())
	{
		FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());
		mtlpp::CommandBuffer Buffer = Context.GetCommandQueue().CreateCommandBuffer();
		
		id<IMetalStatisticsSamples> Samples = Context.GetCommandQueue().GetStatistics()->RegisterEncoderStatistics(Buffer.GetPtr(), EMetalSampleComputeEncoderStart);
		mtlpp::ComputeCommandEncoder Encoder = Buffer.ComputeCommandEncoder();
#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
		FMetalComputeCommandEncoderDebugging Debugging;
		if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
		{
			FMetalCommandBufferDebugging CmdDebug = FMetalCommandBufferDebugging::Get(Buffer);
			Debugging = FMetalComputeCommandEncoderDebugging(Encoder, CmdDebug);
		}
#endif
		
		Context.GetCommandQueue().GetStatistics()->RegisterEncoderStatistics(Buffer.GetPtr(), EMetalSampleComputeEncoderEnd);
		check(Samples);
		[Samples retain];
		Encoder.EndEncoding();
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, Debugging.EndEncoder());
		
		FMetalProfiler* Profiler = ImmediateContext.GetProfiler();
		Buffer.AddCompletedHandler(^(const mtlpp::CommandBuffer & theBuffer) {
			double GpuTimeSeconds = theBuffer.GetGpuStartTime();
			const double CyclesPerSecond = 1.0 / FPlatformTime::GetSecondsPerCycle();
			NSUInteger EndTime = GpuTimeSeconds * CyclesPerSecond;
			NSUInteger StatsTime = Samples.Array[0];
			Profiler->TimingSupport.SetCalibrationTimestamp(StatsTime / 1000, EndTime / 1000);
			[Samples release];
		});
		
		Context.GetCommandQueue().CommitCommandBuffer(Buffer);
		Buffer.WaitUntilCompleted();
	}
#endif
}

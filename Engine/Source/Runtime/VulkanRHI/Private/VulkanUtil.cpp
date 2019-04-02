// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUtil.cpp: Vulkan Utility implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanUtil.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"
#include "Misc/OutputDeviceRedirector.h"


extern CORE_API bool GIsGPUCrashed;


static FString		EventDeepString(TEXT("EventTooDeep"));
static const uint32	EventDeepCRC = FCrc::StrCrc32<TCHAR>(*EventDeepString);

/**
 * Initializes the static variables, if necessary.
 */
void FVulkanGPUTiming::PlatformStaticInitialize(void* UserData)
{
	GIsSupported = false;

	// Are the static variables initialized?
	check( !GAreGlobalsInitialized );

	FVulkanGPUTiming* Caller = (FVulkanGPUTiming*)UserData;
	if (Caller && Caller->Device && FVulkanPlatform::SupportsTimestampRenderQueries())
	{
		const VkPhysicalDeviceLimits& Limits = Caller->Device->GetDeviceProperties().limits;
		bool bSupportsTimestamps = (Limits.timestampComputeAndGraphics == VK_TRUE);
		if (!bSupportsTimestamps)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Timestamps not supported on Device"));
			return;
		}
		GTimingFrequency = (uint64)((1000.0 * 1000.0 * 1000.0) / Limits.timestampPeriod);
		GIsSupported = true;
	}
}

void FVulkanGPUTiming::CalibrateTimers(FVulkanCommandListContext& InCmdContext)
{
#if VULKAN_USE_NEW_QUERIES

	// TODO: Implement VULKAN_USE_NEW_QUERIES version

#else
	FVulkanDevice* Device = InCmdContext.GetDevice();
	FVulkanRenderQuery* TimestampQuery = new FVulkanRenderQuery(RQT_AbsoluteTime);

	{
		FVulkanCmdBuffer* CmdBuffer = InCmdContext.GetCommandBufferManager()->GetUploadCmdBuffer();
		InCmdContext.EndRenderQueryInternal(CmdBuffer, TimestampQuery);
		InCmdContext.GetCommandBufferManager()->SubmitUploadCmdBuffer();
	}

	uint64 CPUTimestamp = 0;
	uint64 GPUTimestampMicroseconds = 0;

	const bool bWait = true;
	if (TimestampQuery->GetResult(Device, GPUTimestampMicroseconds, bWait))
	{
		CPUTimestamp = FPlatformTime::Cycles64();

		GCalibrationTimestamp.CPUMicroseconds = uint64(FPlatformTime::ToSeconds64(CPUTimestamp) * 1e6);
		GCalibrationTimestamp.GPUMicroseconds = GPUTimestampMicroseconds;
	}

	delete TimestampQuery;

#endif
}

void FVulkanDynamicRHI::RHICalibrateTimers()
{
	check(IsInRenderingThread());

	FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());

	FVulkanGPUTiming::CalibrateTimers(GetDevice()->GetImmediateContext());
}


FVulkanStagingBuffer::~FVulkanStagingBuffer()
{
	if (StagingBuffer)
	{
		check(Device);
		Device->GetStagingManager().ReleaseBuffer(nullptr, StagingBuffer);
	}
}

void* FVulkanStagingBuffer::Lock(uint32 Offset, uint32 NumBytes)
{
	check(!bIsLocked);
	bIsLocked = true;
	uint32 QueuedEndOffset = QueuedNumBytes + QueuedOffset;
	uint32 EndOffset = Offset + NumBytes;
	check(Offset < QueuedNumBytes && EndOffset <= QueuedEndOffset);
	//#todo-rco: Apply the offset in case it doesn't match
	return (void*)((uint8*)StagingBuffer->GetMappedPointer() + Offset);
}

void FVulkanStagingBuffer::Unlock()
{
	check(bIsLocked);
	bIsLocked = false;
}

FStagingBufferRHIRef FVulkanDynamicRHI::RHICreateStagingBuffer()
{
	return new FVulkanStagingBuffer();
}

void* FVulkanDynamicRHI::RHILockStagingBuffer(FStagingBufferRHIParamRef StagingBufferRHI, uint32 Offset, uint32 NumBytes)
{
	FVulkanStagingBuffer* StagingBuffer = ResourceCast(StagingBufferRHI);
	return StagingBuffer->Lock(Offset, NumBytes);
}

void FVulkanDynamicRHI::RHIUnlockStagingBuffer(FStagingBufferRHIParamRef StagingBufferRHI)
{
	FVulkanStagingBuffer* StagingBuffer = ResourceCast(StagingBufferRHI);
	StagingBuffer->Unlock();
}

FVulkanGPUTiming::~FVulkanGPUTiming()
{
	check(!Pool);
}

/**
 * Initializes all Vulkan resources and if necessary, the static variables.
 */
void FVulkanGPUTiming::Initialize()
{
	StaticInitialize(this, PlatformStaticInitialize);

	bIsTiming = false;

	if (FVulkanPlatform::SupportsTimestampRenderQueries() && GIsSupported)
	{
		check(!Pool);
		Pool = new FVulkanTimingQueryPool(Device, 8);
		Pool->ResultsBuffer = Device->GetStagingManager().AcquireBuffer(8 * sizeof(uint64), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	}
}

/**
 * Releases all Vulkan resources.
 */
void FVulkanGPUTiming::Release()
{
	if (FVulkanPlatform::SupportsTimestampRenderQueries())
	{
		check(Pool);
		Device->GetStagingManager().ReleaseBuffer(nullptr, Pool->ResultsBuffer);
		delete Pool;
		Pool = nullptr;
	}
}

/**
 * Start a GPU timing measurement.
 */
void FVulkanGPUTiming::StartTiming(FVulkanCmdBuffer* CmdBuffer)
{
	// Issue a timestamp query for the 'start' time.
	if (GIsSupported && !bIsTiming)
	{
		if (CmdBuffer == nullptr)
		{
			CmdBuffer = CmdContext->GetCommandBufferManager()->GetActiveCmdBuffer();
		}
		Pool->CurrentTimestamp = (Pool->CurrentTimestamp + 1) % Pool->BufferSize;
		const uint32 QueryStartIndex = Pool->CurrentTimestamp * 2;
		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, Pool->GetHandle(), QueryStartIndex);
		Pool->TimestampListHandles[QueryStartIndex].CmdBuffer = CmdBuffer;
		Pool->TimestampListHandles[QueryStartIndex].FenceCounter = CmdBuffer->GetFenceSignaledCounter();
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FVulkanGPUTiming::EndTiming(FVulkanCmdBuffer* CmdBuffer)
{
	// Issue a timestamp query for the 'end' time.
	if (GIsSupported && bIsTiming)
	{
		if (CmdBuffer == nullptr)
		{
			CmdBuffer = CmdContext->GetCommandBufferManager()->GetActiveCmdBuffer();
		}
		check(Pool->CurrentTimestamp < Pool->BufferSize);
		const uint32 QueryStartIndex = Pool->CurrentTimestamp * 2;
		const uint32 QueryEndIndex = Pool->CurrentTimestamp * 2 + 1;
		check(QueryEndIndex == QueryStartIndex + 1);	// Make sure they're adjacent indices.
		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, Pool->GetHandle(), QueryEndIndex);
		//check(CmdBuffer->IsOutsideRenderPass());
		VulkanRHI::vkCmdCopyQueryPoolResults(CmdBuffer->GetHandle(), Pool->GetHandle(), QueryStartIndex, 2, Pool->ResultsBuffer->GetHandle(), sizeof(uint64) * QueryStartIndex, sizeof(uint64), VK_QUERY_RESULT_64_BIT);
		VulkanRHI::vkCmdResetQueryPool(CmdBuffer->GetHandle(), Pool->GetHandle(), QueryStartIndex, 2);
		Pool->TimestampListHandles[QueryEndIndex].CmdBuffer = CmdBuffer;
		Pool->TimestampListHandles[QueryEndIndex].FenceCounter = CmdBuffer->GetFenceSignaledCounter();
		Pool->NumIssuedTimestamps = FMath::Min<uint32>(Pool->NumIssuedTimestamps + 1, Pool->BufferSize);

		bIsTiming = false;
		bEndTimestampIssued = true;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FVulkanGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	if (GIsSupported)
	{
		check(Pool->CurrentTimestamp < Pool->BufferSize);
		uint64 StartTime, EndTime;
		int32 TimestampIndex = Pool->CurrentTimestamp;
		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for (uint32 IssueIndex = 1; IssueIndex < Pool->NumIssuedTimestamps; ++IssueIndex)
			{
				const uint32 QueryStartIndex = TimestampIndex * 2;
				const uint32 QueryEndIndex = TimestampIndex * 2 + 1;
				const FVulkanTimingQueryPool::FCmdBufferFence& StartQuerySyncPoint = Pool->TimestampListHandles[QueryStartIndex];
				const FVulkanTimingQueryPool::FCmdBufferFence& EndQuerySyncPoint = Pool->TimestampListHandles[QueryEndIndex];
				if (EndQuerySyncPoint.FenceCounter < EndQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter() && StartQuerySyncPoint.FenceCounter < StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
				{
					uint64* Data = (uint64*)Pool->ResultsBuffer->GetMappedPointer();
					StartTime = Data[QueryStartIndex];
					EndTime = Data[QueryEndIndex];

					if (EndTime > StartTime)
					{
						return EndTime - StartTime;
					}
				}

				TimestampIndex = (TimestampIndex + Pool->BufferSize - 1) % Pool->BufferSize;
			}
		}

		if (Pool->NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock)
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind

			const bool bBlocking = (Pool->NumIssuedTimestamps == Pool->BufferSize) || bGetCurrentResultsAndBlock;
			const uint32 IdleStart = FPlatformTime::Cycles();

			SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);

			const uint32 QueryStartIndex = TimestampIndex * 2;
			const uint32 QueryEndIndex = TimestampIndex * 2 + 1;

			if (bBlocking)
			{
				const FVulkanTimingQueryPool::FCmdBufferFence& StartQuerySyncPoint = Pool->TimestampListHandles[QueryStartIndex];
				const FVulkanTimingQueryPool::FCmdBufferFence& EndQuerySyncPoint = Pool->TimestampListHandles[QueryEndIndex];
				bool bWaitForStart = StartQuerySyncPoint.FenceCounter == StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter();
				bool bWaitForEnd = EndQuerySyncPoint.FenceCounter == EndQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter();
				if (bWaitForEnd || bWaitForStart)
				{
					// Need to submit the open command lists.
					Device->SubmitCommandsAndFlushGPU();
				}

				// CPU wait for query results to be ready.
				if (bWaitForStart && StartQuerySyncPoint.FenceCounter == StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
				{
					CmdContext->GetCommandBufferManager()->WaitForCmdBuffer(StartQuerySyncPoint.CmdBuffer);
				}
				if (bWaitForEnd && EndQuerySyncPoint.FenceCounter == EndQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
				{
					CmdContext->GetCommandBufferManager()->WaitForCmdBuffer(EndQuerySyncPoint.CmdBuffer);
				}
			}

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			uint64* Data = (uint64*)Pool->ResultsBuffer->GetMappedPointer();
			StartTime = Data[QueryStartIndex];
			EndTime = Data[QueryEndIndex];

			if (EndTime > StartTime)
			{
				return EndTime - StartTime;
			}
		}
	}

	return 0;
}

/** Start this frame of per tracking */
void FVulkanEventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FVulkanEventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FVulkanEventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);

		// In milliseconds
		RootResult = (double)GPUTiming / (double)RootEventTiming.GetTimingFrequency();
	}

	return (float)RootResult;
}

float FVulkanEventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		const uint64 GPUTiming = Timing.GetTiming(true);
		// In milliseconds
		Result = (double)GPUTiming / (double)Timing.GetTimingFrequency();
	}

	return Result;
}


void FVulkanGPUProfiler::BeginFrame()
{
#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GGPUCrashDebuggingEnabled && Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
	{
		static auto* CrashCollectionEnableCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.collectionenable"));
		static auto* CrashCollectionDataDepth = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.datadepth"));
		bTrackingGPUCrashData = CrashCollectionEnableCvar ? CrashCollectionEnableCvar->GetValueOnRenderThread() != 0 : false;
		GPUCrashDataDepth = CrashCollectionDataDepth ? CrashCollectionDataDepth->GetValueOnRenderThread() : -1;
	}
#endif

	bCommandlistSubmitted = false;
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GetEmitDrawEvents();
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			SetEmitDrawEvents(true);  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			CurrentEventNodeFrame = new FVulkanEventNodeFrame(CmdContext, Device);
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	if (GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FVulkanGPUProfiler::EndFrameBeforeSubmit()
{
	if (GetEmitDrawEvents())
	{
		// Finish all open nodes
		// This is necessary because timestamps must be issued before SubmitDone(), and SubmitDone() happens in RHIEndDrawingViewport instead of RHIEndFrame
		while (CurrentEventNode)
		{
			UE_LOG(LogRHI, Warning, TEXT("POPPING BEFORE SUB"));
			PopEvent();
		}

		bCommandlistSubmitted = true;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}
}

void FVulkanGPUProfiler::EndFrame()
{
	EndFrameBeforeSubmit();

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			CmdContext->GetDevice()->SubmitCommandsAndFlushGPU();

			SetEmitDrawEvents(bOriginalGEmitDrawEvents);
			UE_LOG(LogRHI, Warning, TEXT(""));
			UE_LOG(LogRHI, Warning, TEXT(""));
			check(CurrentEventNodeFrame);
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		UE_LOG(LogRHI, Warning, TEXT("GPU hitch tracking not implemented on Vulkan"));
	}
	bTrackingEvents = false;
	if (CurrentEventNodeFrame)
	{
		delete CurrentEventNodeFrame;
		CurrentEventNodeFrame = nullptr;
	}
}

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
void FVulkanGPUProfiler::PushMarkerForCrash(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TCHAR* Name)
{
	uint32 CRC = 0;
	if (GPUCrashDataDepth < 0 || PushPopStack.Num() < GPUCrashDataDepth)
	{
		CRC = FCrc::StrCrc32<TCHAR>(Name);

		if (CachedStrings.Num() > 10000)
		{
			CachedStrings.Empty(10000);
			CachedStrings.Emplace(EventDeepCRC, EventDeepString);
		}

		if (CachedStrings.Find(CRC) == nullptr)
		{
			CachedStrings.Emplace(CRC, FString(Name));
		}
	}
	else
	{
		CRC = EventDeepCRC;
	}

	PushPopStack.Push(CRC);
	FVulkanPlatform::WriteCrashMarker(Device->GetOptionalExtensions(), CmdBuffer, DestBuffer, TArrayView<uint32>(PushPopStack), true);
}

void FVulkanGPUProfiler::PopMarkerForCrash(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer)
{
	if (PushPopStack.Num() > 0)
	{
		PushPopStack.Pop(false);
		FVulkanPlatform::WriteCrashMarker(Device->GetOptionalExtensions(), CmdBuffer, DestBuffer, TArrayView<uint32>(PushPopStack), false);
	}
}

void FVulkanGPUProfiler::DumpCrashMarkers(void* BufferData)
{
#if VULKAN_SUPPORTS_AMD_BUFFER_MARKER
	if (Device->GetOptionalExtensions().HasAMDBufferMarker)
	{
		uint32* Entries = (uint32*)BufferData;
		uint32 NumCRCs = *Entries++;
		for (uint32 Index = 0; Index < NumCRCs; ++Index)
		{
			const FString* Frame = CachedStrings.Find(*Entries);
			UE_LOG(LogVulkanRHI, Error, TEXT("[VK_AMD_buffer_info] %i: %s (CRC 0x%x)"), Index, Frame ? *(*Frame) : TEXT("<undefined>"), *Entries);
			++Entries;
		}
	}
	else
#endif
	{
#if VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT
		if (Device->GetOptionalExtensions().HasNVDiagnosticCheckpoints)
		{
			struct FCheckpointDataNV : public VkCheckpointDataNV
			{
				FCheckpointDataNV()
				{
					ZeroVulkanStruct(*this, VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV);
				}
			};
			TArray<FCheckpointDataNV> Data;
			uint32 Num = 0;
			VkQueue QueueHandle = Device->GetGraphicsQueue()->GetHandle();
			VulkanDynamicAPI::vkGetQueueCheckpointDataNV(QueueHandle, &Num, nullptr);
			if (Num > 0)
			{
				Data.AddDefaulted(Num);
				VulkanDynamicAPI::vkGetQueueCheckpointDataNV(QueueHandle, &Num, &Data[0]);
				check(Num == Data.Num());
				for (uint32 Index = 0; Index < Num; ++Index)
				{
					check(Data[Index].sType == VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV);
					uint32 Value = (uint32)(size_t)Data[Index].pCheckpointMarker;
					const FString* Frame = CachedStrings.Find(Value);
					UE_LOG(LogVulkanRHI, Error, TEXT("[VK_NV_device_diagnostic_checkpoints] %i: Stage 0x%x, %s (CRC 0x%x)"), Index, Data[Index].stage, Frame ? *(*Frame) : TEXT("<undefined>"), Value);
				}
				GLog->PanicFlushThreadedLogs();
				GLog->Flush();
			}
		}
#endif
	}
}
#endif

#include "VulkanRHIBridge.h"
namespace VulkanRHIBridge
{
	uint64 GetInstance(FVulkanDynamicRHI* RHI)
	{
		return (uint64)RHI->GetInstance();
	}

	FVulkanDevice* GetDevice(FVulkanDynamicRHI* RHI)
	{
		return RHI->GetDevice();
	}

	// Returns a VkDevice
	uint64 GetLogicalDevice(FVulkanDevice* Device)
	{
		return (uint64)Device->GetInstanceHandle();
	}

	// Returns a VkDeviceVkPhysicalDevice
	uint64 GetPhysicalDevice(FVulkanDevice* Device)
	{
		return (uint64)Device->GetPhysicalHandle();
	}
}


#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "OneColorShader.h"
#include "VulkanRHI.h"
#include "StaticBoundShaderState.h"

namespace VulkanRHI
{
	VkBuffer CreateBuffer(FVulkanDevice* InDevice, VkDeviceSize Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryRequirements& OutMemoryRequirements)
	{
		VkDevice Device = InDevice->GetInstanceHandle();
		VkBuffer Buffer = VK_NULL_HANDLE;

		VkBufferCreateInfo BufferCreateInfo;
		ZeroVulkanStruct(BufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
		BufferCreateInfo.size = Size;
		BufferCreateInfo.usage = BufferUsageFlags;
		VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateBuffer(Device, &BufferCreateInfo, VULKAN_CPU_ALLOCATOR, &Buffer));

		VulkanRHI::vkGetBufferMemoryRequirements(Device, Buffer, &OutMemoryRequirements);

		return Buffer;
	}

	/**
	 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	 * @param	Result - The result code to check
	 * @param	Code - The code which yielded the result.
	 * @param	VkFunction - Tested function name.
	 * @param	Filename - The filename of the source file containing Code.
	 * @param	Line - The line number of Code within Filename.
	 */
	void VerifyVulkanResult(VkResult Result, const ANSICHAR* VkFunction, const ANSICHAR* Filename, uint32 Line)
	{
		FString ErrorString;
		switch (Result)
		{
#define VKERRORCASE(x)	case x: ErrorString = TEXT(#x)
		VKERRORCASE(VK_NOT_READY); break;
		VKERRORCASE(VK_TIMEOUT); break;
		VKERRORCASE(VK_EVENT_SET); break;
		VKERRORCASE(VK_EVENT_RESET); break;
		VKERRORCASE(VK_INCOMPLETE); break;
		VKERRORCASE(VK_ERROR_OUT_OF_HOST_MEMORY); break;
		VKERRORCASE(VK_ERROR_OUT_OF_DEVICE_MEMORY); break;
		VKERRORCASE(VK_ERROR_INITIALIZATION_FAILED); break;
		VKERRORCASE(VK_ERROR_DEVICE_LOST); GIsGPUCrashed = true; break;
		VKERRORCASE(VK_ERROR_MEMORY_MAP_FAILED); break;
		VKERRORCASE(VK_ERROR_LAYER_NOT_PRESENT); break;
		VKERRORCASE(VK_ERROR_EXTENSION_NOT_PRESENT); break;
		VKERRORCASE(VK_ERROR_FEATURE_NOT_PRESENT); break;
		VKERRORCASE(VK_ERROR_INCOMPATIBLE_DRIVER); break;
		VKERRORCASE(VK_ERROR_TOO_MANY_OBJECTS); break;
		VKERRORCASE(VK_ERROR_FORMAT_NOT_SUPPORTED); break;
		VKERRORCASE(VK_ERROR_SURFACE_LOST_KHR); break;
		VKERRORCASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR); break;
		VKERRORCASE(VK_SUBOPTIMAL_KHR); break;
		VKERRORCASE(VK_ERROR_OUT_OF_DATE_KHR); break;
		VKERRORCASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR); break;
		VKERRORCASE(VK_ERROR_VALIDATION_FAILED_EXT); break;
#if VK_HEADER_VERSION >= 13
		VKERRORCASE(VK_ERROR_INVALID_SHADER_NV); break;
#endif
#if VK_HEADER_VERSION >= 24
		VKERRORCASE(VK_ERROR_FRAGMENTED_POOL); break;
#endif
#if VK_HEADER_VERSION >= 39
		VKERRORCASE(VK_ERROR_OUT_OF_POOL_MEMORY_KHR); break;
#endif
#if VK_HEADER_VERSION >= 65
		VKERRORCASE(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR); break;
		VKERRORCASE(VK_ERROR_NOT_PERMITTED_EXT); break;
#endif
#undef VKERRORCASE
		default:
			break;
		}

		UE_LOG(LogVulkanRHI, Error, TEXT("%s failed, VkResult=%d\n at %s:%u \n with error %s"),
			ANSI_TO_TCHAR(VkFunction), (int32)Result, ANSI_TO_TCHAR(Filename), Line, *ErrorString);

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
		if (GIsGPUCrashed && GGPUCrashDebuggingEnabled)
		{
			FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;
			FVulkanDevice* Device = RHI->GetDevice();
			if (Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
			{
				Device->GetImmediateContext().GetGPUProfiler().DumpCrashMarkers(Device->GetCrashMarkerMappedPointer());
			}
		}
#endif

		UE_LOG(LogVulkanRHI, Fatal, TEXT("%s failed, VkResult=%d\n at %s:%u \n with error %s"),
			ANSI_TO_TCHAR(VkFunction), (int32)Result, ANSI_TO_TCHAR(Filename), Line, *ErrorString);
	}
}

DEFINE_STAT(STAT_VulkanDrawCallTime);
DEFINE_STAT(STAT_VulkanDispatchCallTime);
DEFINE_STAT(STAT_VulkanDrawCallPrepareTime);
DEFINE_STAT(STAT_VulkanCustomPresentTime);
DEFINE_STAT(STAT_VulkanDispatchCallPrepareTime);
DEFINE_STAT(STAT_VulkanGetOrCreatePipeline);
DEFINE_STAT(STAT_VulkanGetDescriptorSet);
DEFINE_STAT(STAT_VulkanPipelineBind);
DEFINE_STAT(STAT_VulkanNumCmdBuffers);
DEFINE_STAT(STAT_VulkanNumPSOs);
DEFINE_STAT(STAT_VulkanNumRenderPasses);
DEFINE_STAT(STAT_VulkanNumFrameBuffers);
DEFINE_STAT(STAT_VulkanNumBufferViews);
DEFINE_STAT(STAT_VulkanNumImageViews);
DEFINE_STAT(STAT_VulkanNumPhysicalMemAllocations);
DEFINE_STAT(STAT_VulkanDynamicVBSize);
DEFINE_STAT(STAT_VulkanDynamicIBSize);
DEFINE_STAT(STAT_VulkanDynamicVBLockTime);
DEFINE_STAT(STAT_VulkanDynamicIBLockTime);
DEFINE_STAT(STAT_VulkanUPPrepTime);
DEFINE_STAT(STAT_VulkanUniformBufferCreateTime);
DEFINE_STAT(STAT_VulkanApplyDSUniformBuffers);
DEFINE_STAT(STAT_VulkanApplyPackedUniformBuffers);
DEFINE_STAT(STAT_VulkanSRVUpdateTime);
DEFINE_STAT(STAT_VulkanUAVUpdateTime);
DEFINE_STAT(STAT_VulkanDeletionQueue);
DEFINE_STAT(STAT_VulkanQueueSubmit);
DEFINE_STAT(STAT_VulkanQueuePresent);
DEFINE_STAT(STAT_VulkanNumQueries);
DEFINE_STAT(STAT_VulkanNumQueryPools);
DEFINE_STAT(STAT_VulkanWaitQuery);
DEFINE_STAT(STAT_VulkanWaitFence);
DEFINE_STAT(STAT_VulkanResetQuery);
DEFINE_STAT(STAT_VulkanWaitSwapchain);
DEFINE_STAT(STAT_VulkanAcquireBackBuffer);
DEFINE_STAT(STAT_VulkanStagingBuffer);
DEFINE_STAT(STAT_VulkanVkCreateDescriptorPool);
DEFINE_STAT(STAT_VulkanNumDescPools);
#if VULKAN_ENABLE_AGGRESSIVE_STATS
DEFINE_STAT(STAT_VulkanUpdateDescriptorSets);
DEFINE_STAT(STAT_VulkanNumUpdateDescriptors);
DEFINE_STAT(STAT_VulkanNumDescSets);
DEFINE_STAT(STAT_VulkanSetUniformBufferTime);
DEFINE_STAT(STAT_VulkanVkUpdateDS);
DEFINE_STAT(STAT_VulkanBindVertexStreamsTime);
#endif
DEFINE_STAT(STAT_VulkanNumDescSetsTotal);

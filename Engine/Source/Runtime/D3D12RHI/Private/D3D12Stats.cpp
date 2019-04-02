// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Stats.cpp:RHI Stats and timing implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

void D3D12RHI::FD3DGPUProfiler::BeginFrame(FD3D12DynamicRHI* InRHI)
{
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// update the crash tracking variables
	static auto* CrashCollectionEnableCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.collectionenable"));
	static auto* CrashCollectionDataDepth = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.datadepth"));

	bTrackingGPUCrashData = CrashCollectionEnableCvar ? CrashCollectionEnableCvar->GetValueOnRenderThread() != 0 : false;
	GPUCrashDataDepth = CrashCollectionDataDepth ? CrashCollectionDataDepth->GetValueOnRenderThread() : -1;

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
			DoPreProfileGPUWork();
			CurrentEventNodeFrame = new FD3D12EventNodeFrame(GetParentAdapter());
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

	FrameTiming.StartTiming();

	if (GetEmitDrawEvents())
	{
#if NV_AFTERMATH
		// Assuming that grabbing the device 0 command list here is OK
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255), InRHI->GetAdapter().GetDevice(0)->GetCommandContext().CommandListHandle.AftermathCommandContext());
#else
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
#endif
	}
}

void D3D12RHI::FD3DGPUProfiler::EndFrame(FD3D12DynamicRHI* InRHI)
{
	if (GetEmitDrawEvents())
	{
		PopEvent();
		check(StackDepth == 0);
	}

	FrameTiming.EndTiming();

	if (FrameTiming.IsSupported())
	{
		uint64 GPUTiming = FrameTiming.GetTiming();
		uint64 GPUFreq = FrameTiming.GetTimingFrequency();
		GGPUFrameTime = FMath::TruncToInt(double(GPUTiming) / double(GPUFreq) / FPlatformTime::GetSecondsPerCycle());
	}
	else
	{
		GGPUFrameTime = 0;
	}

	double HwGpuFrameTime = 0.0;
	if (InRHI->GetHardwareGPUFrameTime(HwGpuFrameTime))
	{
		GGPUFrameTime = HwGpuFrameTime;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			SetEmitDrawEvents(bOriginalGEmitDrawEvents);
			DoPostProfileGPUWork();
			UE_LOG(LogD3D12RHI, Log, TEXT(""));
			UE_LOG(LogD3D12RHI, Log, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;

			if (RHIConfig::ShouldSaveScreenshotAfterProfilingGPU()
				&& GEngine->GameViewport)
			{
				GEngine->GameViewport->Exec(NULL, TEXT("SCREENSHOT"), *GLog);
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static double LastTime = -1.0;
		double Now = FPlatformTime::Seconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			const float HitchThreshold = RHIConfig::GetGPUHitchThreshold();
			float ThisTime = Now - LastTime;
			bool bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** Hitch detected on CPU, frametime = %6.1fms"), ThisTime * 1000.0f);
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));

				for (int32 Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					UE_LOG(LogD3D12RHI, Warning, TEXT(""));
					UE_LOG(LogD3D12RHI, Warning, TEXT(""));
					UE_LOG(LogD3D12RHI, Warning, TEXT("********** GPU Frame: Current - %d"), GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames[Frame].DumpEventTree();
				}
				UE_LOG(LogD3D12RHI, Warning, TEXT(""));
				UE_LOG(LogD3D12RHI, Warning, TEXT(""));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** End Hitch GPU Profile"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec(NULL, TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const int32 HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.RemoveAt(0);
				}
				GPUHitchEventNodeFrames.Add((FD3D12EventNodeFrame*)CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}
	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}

void D3D12RHI::FD3DGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
#if WITH_DX_PERF
	D3DPERF_BeginEvent(Color.DWColor(), Name);
#endif

	FGPUProfiler::PushEvent(Name, Color);
}

static FString EventDeepString(TEXT("EventTooDeep"));
static const uint32 EventDeepCRC = FCrc::StrCrc32<TCHAR>(*EventDeepString);
#if NV_AFTERMATH
void D3D12RHI::FD3DGPUProfiler::PushEvent(const TCHAR* Name, FColor Color, GFSDK_Aftermath_ContextHandle Context)
{

	if (GDX12NVAfterMathEnabled && bTrackingGPUCrashData)
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

		GFSDK_Aftermath_SetEventMarker(Context, &PushPopStack[0], PushPopStack.Num() * sizeof(uint32));
	}

	PushEvent(Name, Color);
}
#endif

void D3D12RHI::FD3DGPUProfiler::PopEvent()
{
#if WITH_DX_PERF
	D3DPERF_EndEvent();
#endif

#if NV_AFTERMATH
	if (GDX12NVAfterMathEnabled && bTrackingGPUCrashData)
	{
		// need to look for unbalanced push/pop
		if (PushPopStack.Num() > 0)
		{
			PushPopStack.Pop(false);
		}
	}
#endif

	FGPUProfiler::PopEvent();
}

/** Start this frame of per tracking */
void FD3D12EventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FD3D12EventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FD3D12EventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency();

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

void FD3D12EventNodeFrame::LogDisjointQuery()
{
}

float FD3D12EventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const uint64 GPUTiming = Timing.GetTiming(true);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

void UpdateBufferStats(FD3D12ResourceLocation* ResourceLocation, bool bAllocating, uint32 BufferType)
{
	uint64 RequestedSize = ResourceLocation->GetSize();

	const bool bUniformBuffer = BufferType == D3D12_BUFFER_TYPE_CONSTANT;
	const bool bIndexBuffer = BufferType == D3D12_BUFFER_TYPE_INDEX;
	const bool bVertexBuffer = BufferType == D3D12_BUFFER_TYPE_VERTEX;

	if (bAllocating)
	{
		if (bUniformBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_UniformBufferMemory, RequestedSize);
		}
		else if (bIndexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_IndexBufferMemory, RequestedSize);
		}
		else if (bVertexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_VertexBufferMemory, RequestedSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, RequestedSize);
		}

#if PLATFORM_WINDOWS
		// this is a work-around on Windows. Due to the fact that there is no way
		// to hook the actual d3d allocations it is very difficult to track memory
		// in the normal way. The problem is that some buffers are allocated from
		// the allocators and some are allocated from the device. Ideally this
		// tracking would be moved to where the actual d3d resource is created and
		// released and the tracking could be re-enabled in the buddy allocator.
		// The problem is that the releasing of resources happens in a generic way
		// (see FD3D12ResourceLocation) 
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::Meshes, RequestedSize, ELLMTracker::Default, ELLMAllocType::None);
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::GraphicsPlatform, RequestedSize, ELLMTracker::Platform, ELLMAllocType::None);
#endif
	}
	else
	{
		if (bUniformBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_UniformBufferMemory, RequestedSize);
		}
		else if (bIndexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_IndexBufferMemory, RequestedSize);
		}
		else if (bVertexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_VertexBufferMemory, RequestedSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, RequestedSize);
		}

#if PLATFORM_WINDOWS
		// this is a work-around on Windows. See comment above.
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::Meshes, -(int64)RequestedSize, ELLMTracker::Default, ELLMAllocType::None);
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::GraphicsPlatform, -(int64)RequestedSize, ELLMTracker::Platform, ELLMAllocType::None);
#endif
	}
}

#if NV_AFTERMATH
void D3D12RHI::FD3DGPUProfiler::RegisterCommandList(GFSDK_Aftermath_ContextHandle context)
{
	FScopeLock Lock(&AftermathLock);

	AftermathContexts.Push(context);
}

void D3D12RHI::FD3DGPUProfiler::UnregisterCommandList(GFSDK_Aftermath_ContextHandle context)
{
	FScopeLock Lock(&AftermathLock);

	int32 Item = AftermathContexts.Find(context);

	AftermathContexts.RemoveAt(Item);
}
#endif

extern CORE_API bool GIsGPUCrashed;
bool D3D12RHI::FD3DGPUProfiler::CheckGpuHeartbeat() const
{
#if NV_AFTERMATH
	if (GDX12NVAfterMathEnabled)
	{
		GFSDK_Aftermath_Device_Status Status;
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_GetDeviceStatus(&Status);
		if (Result == GFSDK_Aftermath_Result_Success)
		{
			if (Status != GFSDK_Aftermath_Device_Status_Active)
			{
				GIsGPUCrashed = true;
				const TCHAR* AftermathReason[] = { TEXT("Active"), TEXT("Timeout"), TEXT("OutOfMemory"), TEXT("PageFault"), TEXT("Unknown") };
				check(Status < ARRAYSIZE(AftermathReason));
				UE_LOG(LogRHI, Error, TEXT("[Aftermath] Status: %s"), AftermathReason[Status]);

				TArray<GFSDK_Aftermath_ContextData> ContextDataOut;
				ContextDataOut.AddUninitialized(AftermathContexts.Num());
				Result = GFSDK_Aftermath_GetData(AftermathContexts.Num(), AftermathContexts.GetData(), ContextDataOut.GetData());
				if (Result == GFSDK_Aftermath_Result_Success)
				{
					UE_LOG(LogRHI, Error, TEXT("[Aftermath] Scanning %d command lists for dumps"), ContextDataOut.Num());
					for (GFSDK_Aftermath_ContextData& ContextData : ContextDataOut)
					{
						if (ContextData.status == GFSDK_Aftermath_Context_Status_Executing)
						{
							UE_LOG(LogRHI, Error, TEXT("[Aftermath] GPU Stack Dump"));
							uint32 NumCRCs = ContextData.markerSize / sizeof(uint32);
							uint32* Data = (uint32*)ContextData.markerData;
							for (uint32 i = 0; i < NumCRCs; i++)
							{
								const FString* Frame = CachedStrings.Find(Data[i]);
								if (Frame != nullptr)
								{
									UE_LOG(LogRHI, Error, TEXT("[Aftermath] %i: %s"), i, *(*Frame));
								}
							}
							UE_LOG(LogRHI, Error, TEXT("[Aftermath] GPU Stack Dump"));
						}
					}
				}
				else
				{
					UE_LOG(LogRHI, Error, TEXT("[Aftermath] Failed to get Aftermath stack data"));
				}

				if (Status == GFSDK_Aftermath_Device_Status_PageFault)
				{
					GFSDK_Aftermath_PageFaultInformation FaultInformation;
					Result = GFSDK_Aftermath_GetPageFaultInformation(&FaultInformation);

					if (Result == GFSDK_Aftermath_Result_Success)
					{
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] Faulting address: 0x%016llx"), FaultInformation.faultingGpuVA);
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] Faulting resource dims: %d x %d x %d"), FaultInformation.resourceDesc.width, FaultInformation.resourceDesc.height, FaultInformation.resourceDesc.depth);
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] Faulting result size: %llu bytes"), FaultInformation.resourceDesc.size);
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] Faulting resource mips: %d"), FaultInformation.resourceDesc.mipLevels);
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] Faulting resource format: 0x%x"), FaultInformation.resourceDesc.format);
					}
					else
					{
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] No information on faulting address"));
					}
				}
				return false;
			}
		}
	}
#endif
	return true;
}

static int32 FindCmdListTimingPairIndex(const TArray<uint64>& CmdListStartTimestamps, uint64 Value)
{
	int32 Pos = Algo::UpperBound(CmdListStartTimestamps, Value) - 1;
	return FMath::Max(Pos, 0);
}

uint64 D3D12RHI::FD3DGPUProfiler::CalculateIdleTime(uint64 StartTime, uint64 EndTime)
{
	const int32 NumTimingPairs = CmdListStartTimestamps.Num();
	check(NumTimingPairs == CmdListEndTimestamps.Num() && NumTimingPairs == IdleTimeCDF.Num());
	
	if (!NumTimingPairs)
	{
		return 0;
	}

	const int32 StartIdx = FindCmdListTimingPairIndex(CmdListStartTimestamps, StartTime);
	const int32 EndIdx = FindCmdListTimingPairIndex(CmdListStartTimestamps, EndTime);
	return IdleTimeCDF[EndIdx] - IdleTimeCDF[StartIdx];
}

void D3D12RHI::FD3DGPUProfiler::DoPreProfileGPUWork()
{
	typedef typename FD3D12CommandContext::EFlushCommandsExtraAction EFlushCmdsAction;
	constexpr bool bWaitForCommands = false;
	constexpr EFlushCmdsAction FlushAction = EFlushCmdsAction::FCEA_StartProfilingGPU;

	FD3D12Adapter* Adapter = GetParentAdapter();
	for (uint32 GPUIdx : FRHIGPUMask::All())
	{
		FD3D12Device* Device = Adapter->GetDevice(GPUIdx);
		Device->GetDefaultCommandContext().FlushCommands(bWaitForCommands, FlushAction);
	}
}

void D3D12RHI::FD3DGPUProfiler::DoPostProfileGPUWork()
{
	typedef typename FD3D12CommandContext::EFlushCommandsExtraAction EFlushCmdsAction;
	constexpr bool bWaitForCommands = false;
	constexpr EFlushCmdsAction FlushAction = EFlushCmdsAction::FCEA_EndProfilingGPU;

	TArray<FResolvedCmdListExecTime> CmdListExecTimes;
	FD3D12Adapter* Adapter = GetParentAdapter();
	for (uint32 GPUIdx : FRHIGPUMask::All())
	{
		FD3D12Device* Device = Adapter->GetDevice(GPUIdx);
		Device->GetDefaultCommandContext().FlushCommands(bWaitForCommands, FlushAction);
		TArray<FResolvedCmdListExecTime> TimingPairs;
		Device->GetCommandListManager().GetCommandListTimingResults(TimingPairs);
		CmdListExecTimes.Append(MoveTemp(TimingPairs));
	}

	const int32 NumTimingPairs = CmdListExecTimes.Num();
	CmdListStartTimestamps.Empty(NumTimingPairs);
	CmdListEndTimestamps.Empty(NumTimingPairs);
	IdleTimeCDF.Empty(NumTimingPairs);

	if (NumTimingPairs > 0)
	{
		Algo::Sort(CmdListExecTimes, [](const FResolvedCmdListExecTime& A, const FResolvedCmdListExecTime& B)
		{
			return A.StartTimestamp < B.StartTimestamp;
		});
		CmdListStartTimestamps.Add(CmdListExecTimes[0].StartTimestamp);
		CmdListEndTimestamps.Add(CmdListExecTimes[0].EndTimestamp);
		IdleTimeCDF.Add(0);
		for (int32 Idx = 1; Idx < NumTimingPairs; ++Idx)
		{
			const FResolvedCmdListExecTime& Prev = CmdListExecTimes[Idx - 1];
			const FResolvedCmdListExecTime& Cur = CmdListExecTimes[Idx];
			ensure(Cur.StartTimestamp >= Prev.EndTimestamp);
			CmdListStartTimestamps.Add(Cur.StartTimestamp);
			CmdListEndTimestamps.Add(Cur.EndTimestamp);
			const uint64 Bubble = Cur.StartTimestamp >= Prev.EndTimestamp ? Cur.StartTimestamp - Prev.EndTimestamp : 0;
			IdleTimeCDF.Add(IdleTimeCDF.Last() + Bubble);
		}
	}
}

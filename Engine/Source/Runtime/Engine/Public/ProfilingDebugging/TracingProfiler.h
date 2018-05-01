// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight profiler that can output logs compatible with Google Chrome tracing visualizer.
* Captured events are written as a flat array (fixed size ring buffer), without any kind of aggregation.
* Tracing events may be added from multiple threads simultaneously. 
* Old trace events are overwritten when ring buffer wraps.
*/

#pragma once

#include "HAL/PlatformAtomics.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "CoreTypes.h"
#include "CoreMinimal.h"

#define TRACING_PROFILER (WITH_ENGINE && !UE_BUILD_SHIPPING)

#if TRACING_PROFILER

class FTracingProfiler
{
private:

	static FTracingProfiler* Instance;
	FTracingProfiler();

public:

	enum class EEventType
	{
		CPU,
		GPU,
	};

	struct FEvent
	{
		const char* Name;
		uint32 FrameNumber;
		uint32 SessionId;
		EEventType Type;
		union
		{
			struct
			{
				uint64 BeginMicroseconds;
				uint64 EndMicroseconds;
				uint64 GPUIndex;
			} GPU;

			struct
			{
				uint64 BeginCycles;
				uint64 EndCycles;
				uint64 ThreadId;
			} CPU;
		};
	};

	static ENGINE_API FTracingProfiler* Get();

	ENGINE_API void Init();

	uint32 AddCPUEvent(const char* Name,
		uint64 TimestampBeginCycles,
		uint64 TimestempEndCycles,
		uint32 ThreadId,
		uint32 FrameNumber)
	{
		FEvent Event;
		Event.Name = Name;
		Event.Type = EEventType::CPU;
		Event.FrameNumber = FrameNumber;
		Event.SessionId = SessionId;
		Event.CPU.BeginCycles = TimestampBeginCycles;
		Event.CPU.EndCycles = TimestempEndCycles;
		Event.CPU.ThreadId = ThreadId;
		return AddEvent(Event);
	}

	uint32 AddGPUEvent(const char* Name,
		uint64 TimestempBeginMicroseconds,
		uint64 TimestampEndMicroseconds,
		uint64 GPUIndex,
		uint32 FrameNumber)
	{
		FEvent Event;
		Event.Name = Name;
		Event.Type = EEventType::GPU;
		Event.FrameNumber = FrameNumber;
		Event.SessionId = SessionId;
		Event.GPU.BeginMicroseconds = TimestempBeginMicroseconds;
		Event.GPU.EndMicroseconds = TimestampEndMicroseconds;
		Event.GPU.GPUIndex = GPUIndex;
		return AddEvent(Event);
	}

	uint32 AddEvent(const FEvent& Event)
	{
		if (!bCapturing)
		{
			return ~0u;
		}

		uint32 EventId = (FPlatformAtomics::InterlockedIncrement(&EventAtomicConter)-1) % MaxNumCapturedEvents;
		CapturedEvents[EventId] = Event;
		return EventId;
	}

	ENGINE_API int32 GetCaptureFrameNumber();

	ENGINE_API void BeginCapture(int InNumFramesToCapture = -1);
	ENGINE_API void EndCapture();

	ENGINE_API bool IsCapturing() const;

private:

	void WriteCaptureToFile();

	void BeginFrame();
	void EndFrame();

	void BeginFrameRT();
	void EndFrameRT();

	TArray<FEvent> CapturedEvents;
	uint32 MaxNumCapturedEvents = 0;

	int32 NumFramesToCapture = -1;
	int32 CaptureFrameNumber = 0;

	bool bRequestStartCapture = false;
	bool bRequestStopCapture = false;
	bool bCapturing = false;
	bool bCapturingRT = false;

	uint64 GameThreadFrameBeginCycle = 0;
	uint64 GameThreadFrameEndCycle = 0;

	uint64 RenderThreadFrameBeginCycle = 0;
	uint64 RenderThreadFrameEndCycle = 0;

	uint32 SessionId = 0;

	uint32 Pad[(PLATFORM_CACHE_LINE_SIZE / 4 - 2)];
	volatile int32 EventAtomicConter = 0;
};

#endif //TRACING_PROFILER

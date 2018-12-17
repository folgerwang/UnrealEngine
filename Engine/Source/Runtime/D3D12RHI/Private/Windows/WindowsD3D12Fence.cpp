// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 fence functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"

void FD3D12Fence::InternalSignal(ED3D12CommandQueueType InQueueType, uint64 FenceToSignal)
{
	for (uint32 GPUIndex : GetGPUMask())
	{
		ID3D12CommandQueue* CommandQueue = GetParentAdapter()->GetDevice(GPUIndex)->GetD3DCommandQueue(InQueueType);
		check(CommandQueue);
		FD3D12FenceCore* FenceCore = FenceCores[GPUIndex];
		check(FenceCore);

	#if DEBUG_FENCES
		UE_LOG(LogD3D12RHI, Log, TEXT("*** GPU SIGNAL (CmdQueueType: %u) Fence: %016llX (%s), Value: %u ***"), InQueueType, FenceCore->GetFence(), *GetName().ToString(), FenceToSignal);
	#endif
		VERIFYD3D12RESULT(CommandQueue->Signal(FenceCore->GetFence(), FenceToSignal));
	}
	LastSignaledFence = FenceToSignal;
}

void FD3D12Fence::WaitForFence(uint64 FenceValue)
{
	if (!IsFenceComplete(FenceValue))
	{
		for (uint32 GPUIndex : GetGPUMask())
		{
			FD3D12FenceCore* FenceCore = FenceCores[GPUIndex];
			check(FenceCore);

			if (FenceValue > FenceCore->GetFence()->GetCompletedValue())
			{
				SCOPE_CYCLE_COUNTER(STAT_D3D12WaitForFenceTime);
#if DEBUG_FENCES
				UE_LOG(LogD3D12RHI, Log, TEXT("*** CPU WAIT Fence: %016llX (%s), Value: %u ***"), FenceCore->GetFence(), *GetName().ToString(), FenceValue);
#endif
				// Multiple threads can be using the same FD3D12Fence (texture streaming).
				FScopeLock Lock(&WaitForFenceCS);

				// We must wait.  Do so with an event handler so we don't oversleep.
				VERIFYD3D12RESULT(FenceCore->GetFence()->SetEventOnCompletion(FenceValue, FenceCore->GetCompletionEvent()));

				// Wait for the event to complete (the event is automatically reset afterwards)
				const uint32 WaitResult = WaitForSingleObject(FenceCore->GetCompletionEvent(), INFINITE);
				check(0 == WaitResult);
			}
		}

		// Refresh the completed fence value
		UpdateLastCompletedFence();
#if DEBUG_FENCES
		checkf(FenceValue <= LastCompletedFence, TEXT("Wait for fence value (%llu) failed! Last completed value is still %llu."), FenceValue, LastCompletedFence);
#endif
	}
}
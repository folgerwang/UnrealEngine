// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Stats.h: D3D12 Statistics and Timing Interfaces
=============================================================================*/
#pragma once

/**
* The D3D RHI stats.
*/

DECLARE_CYCLE_STAT_EXTERN(TEXT("Present time"), STAT_D3D12PresentTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CustomPresent time"), STAT_D3D12CustomPresentTime, STATGROUP_D3D12RHI, );

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num command allocators (3D, Compute, Copy)"), STAT_D3D12NumCommandAllocators, STATGROUP_D3D12RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num command lists (3D, Compute, Copy)"), STAT_D3D12NumCommandLists, STATGROUP_D3D12RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num pipeline state objects (PSOs)"), STAT_D3D12NumPSOs, STATGROUP_D3D12RHI, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Allocated"), STAT_D3D12TexturesAllocated, STATGROUP_D3D12RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Released"), STAT_D3D12TexturesReleased, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateTexture time"), STAT_D3D12CreateTextureTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LockTexture time"), STAT_D3D12LockTextureTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UnlockTexture time"), STAT_D3D12UnlockTextureTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateBuffer time"), STAT_D3D12CreateBufferTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LockBuffer time"), STAT_D3D12LockBufferTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UnlockBuffer time"), STAT_D3D12UnlockBufferTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Commit transient resource time"), STAT_D3D12CommitTransientResourceTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Decommit transient resource time"), STAT_D3D12DecommitTransientResourceTime, STATGROUP_D3D12RHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateBoundShaderState time"), STAT_D3D12CreateBoundShaderStateTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("New bound shader state time"), STAT_D3D12NewBoundShaderStateTime, STATGROUP_D3D12RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num bound shader states"), STAT_D3D12NumBoundShaderState, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set bound shader state"), STAT_D3D12SetBoundShaderState, STATGROUP_D3D12RHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Update uniform buffer"), STAT_D3D12UpdateUniformBufferTime, STATGROUP_D3D12RHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Commit resource tables"), STAT_D3D12CommitResourceTables, STATGROUP_D3D12RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num textures in tables"), STAT_D3D12SetTextureInTableCalls, STATGROUP_D3D12RHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear SRVs time"), STAT_D3D12ClearShaderResourceViewsTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set SRV time"), STAT_D3D12SetShaderResourceViewTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set UAV time"), STAT_D3D12SetUnorderedAccessViewTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Commit graphics constants (Set CBV time)"), STAT_D3D12CommitGraphicsConstants, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Commit compute constants (Set CBV time)"), STAT_D3D12CommitComputeConstants, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set shader uniform buffer (Set CBV time)"), STAT_D3D12SetShaderUniformBuffer, STATGROUP_D3D12RHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState time"), STAT_D3D12ApplyStateTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState: Rebuild PSO time"), STAT_D3D12ApplyStateRebuildPSOTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState: Find PSO time"), STAT_D3D12ApplyStateFindPSOTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState: Set SRV time"), STAT_D3D12ApplyStateSetSRVTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState: Set UAV time"), STAT_D3D12ApplyStateSetUAVTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState: Set Vertex Buffer time"), STAT_D3D12ApplyStateSetVertexBufferTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyState: Set CBV time"), STAT_D3D12ApplyStateSetConstantBufferTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PSO Create time"), STAT_D3D12PSOCreateTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear MRT time"), STAT_D3D12ClearMRT, STATGROUP_D3D12RHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("ExecuteCommandList time"), STAT_D3D12ExecuteCommandListTime, STATGROUP_D3D12RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("WaitForFence time"), STAT_D3D12WaitForFenceTime, STATGROUP_D3D12RHI, );

DECLARE_MEMORY_STAT_EXTERN(TEXT("Used Video Memory"), STAT_D3D12UsedVideoMemory, STATGROUP_D3D12RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Available Video Memory"), STAT_D3D12AvailableVideoMemory, STATGROUP_D3D12RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Total Video Memory"), STAT_D3D12TotalVideoMemory, STATGROUP_D3D12RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture allocator wastage"), STAT_D3D12TextureAllocatorWastage, STATGROUP_D3D12RHI, );

/**
* Detailed Descriptor heap stats
*/

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("View: Heap changed"), STAT_ViewHeapChanged, STATGROUP_D3D12DescriptorHeap, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Sampler: Heap changed"), STAT_SamplerHeapChanged, STATGROUP_D3D12DescriptorHeap, );

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("View: Num descriptor heaps"), STAT_NumViewOnlineDescriptorHeaps, STATGROUP_D3D12DescriptorHeap, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Sampler: Num descriptor heaps"), STAT_NumSamplerOnlineDescriptorHeaps, STATGROUP_D3D12DescriptorHeap, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Sampler: Num reusable unique descriptor table entries"), STAT_NumReuseableSamplerOnlineDescriptorTables, STATGROUP_D3D12DescriptorHeap, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("View: Num reserved descriptors"), STAT_NumReservedViewOnlineDescriptors, STATGROUP_D3D12DescriptorHeap, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Sampler: Num reserved descriptors"), STAT_NumReservedSamplerOnlineDescriptors, STATGROUP_D3D12DescriptorHeap, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Sampler: Num reused descriptors"), STAT_NumReusedSamplerOnlineDescriptors, STATGROUP_D3D12DescriptorHeap, );

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("View: Total descriptor heap memory (SRV, CBV, UAV)"), STAT_ViewOnlineDescriptorHeapMemory, STATGROUP_D3D12DescriptorHeap, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Sampler: Total descriptor heap memory"), STAT_SamplerOnlineDescriptorHeapMemory, STATGROUP_D3D12DescriptorHeap, FPlatformMemory::MCR_GPUSystem, );

struct FD3D12GlobalStats
{
	// in bytes, never change after RHI, needed to scale game features
	static int64 GDedicatedVideoMemory;

	// in bytes, never change after RHI, needed to scale game features
	static int64 GDedicatedSystemMemory;

	// in bytes, never change after RHI, needed to scale game features
	static int64 GSharedSystemMemory;

	// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
	static int64 GTotalGraphicsMemory;
};

// This class has multiple inheritance but really FGPUTiming is a static class
class FD3D12BufferedGPUTiming : public FRenderResource, public FGPUTiming, public FD3D12AdapterChild
{
public:
	/**
	* Constructor.
	*
	* @param InD3DRHI			RHI interface
	* @param InBufferSize		Number of buffered measurements
	*/
	FD3D12BufferedGPUTiming(class FD3D12Adapter* InParent, int32 BufferSize);

	FD3D12BufferedGPUTiming()
	{
	}

	/**
	* Start a GPU timing measurement.
	*/
	void	StartTiming();

	/**
	* End a GPU timing measurement.
	* The timing for this particular measurement will be resolved at a later time by the GPU.
	*/
	void	EndTiming();

	/**
	* Retrieves the most recently resolved timing measurement.
	* The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	*
	* @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	*/
	uint64	GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	* Initializes all D3D resources.
	*/
	virtual void InitDynamicRHI() override;

	/**
	* Releases all D3D resources.
	*/
	virtual void ReleaseDynamicRHI() override;

	struct QueryHeap : public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<QueryHeap>
	{
		QueryHeap(FD3D12Device* Parent) : FD3D12DeviceChild(Parent) {};

		void AddRef()
		{
			if (Heap)
			{
				Heap->AddRef();
			}
		}

		void Release()
		{
			if (Heap)
			{
				Heap->Release();
			}
		}

		TRefCountPtr<ID3D12QueryHeap> Heap;
		FD3D12ResidencyHandle ResidencyHandle;
	};

	static void CalibrateTimers(FD3D12Adapter* ParentAdapter);

private:
	/**
	* Initializes the static variables, if necessary.
	*/
	static void PlatformStaticInitialize(void* UserData);

	/**
	* Get the StartTimestampQueryHeapIndex.
	*/
	FORCEINLINE int32 GetStartTimestampIndex(int32 Timestamp) const
	{
		// Multiply by 2 because each timestamp has a start/end pair.
		return Timestamp * 2;
	}

	/**
	* Get the EndTimestampQueryHeapIndex.
	*/
	FORCEINLINE int32 GetEndTimestampIndex(int32 Timestamp) const
	{
		return GetStartTimestampIndex(Timestamp) + 1;
	}

	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	int32						BufferSize;
	/** Current timing being measured on the CPU. */
	int32						CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	int32						NumIssuedTimestamps;

	/** Timestamps */
	QueryHeap* TimestampQueryHeap;

	TArray<FD3D12CLSyncPoint>		TimestampListHandles;
	TRefCountPtr<FD3D12Resource>	TimestampQueryHeapBuffer;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool						bIsTiming;
	/** Whether stable power state is currently enabled */
	bool                        bStablePowerState;
};

template<>
struct TD3D12ResourceTraits<FD3D12BufferedGPUTiming::QueryHeap>
{
	typedef FD3D12BufferedGPUTiming::QueryHeap TConcreteType;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FD3D12EventNode : public FGPUProfilerEventNode, public FD3D12AdapterChild
{
public:
	FD3D12EventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, class FD3D12Adapter* InParentAdapter) :
		FGPUProfilerEventNode(InName, InParent),
		FD3D12AdapterChild(InParentAdapter),
		Timing(InParentAdapter, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitDynamicRHI();
	}

	virtual ~FD3D12EventNode()
	{
		Timing.ReleaseDynamicRHI();
	}

	/**
	* Returns the time in ms that the GPU spent in this draw event.
	* This blocks the CPU if necessary, so can cause hitching.
	*/
	virtual float GetTiming() override;

	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FD3D12BufferedGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FD3D12EventNodeFrame : public FGPUProfilerEventNodeFrame, public FD3D12AdapterChild
{
public:

	FD3D12EventNodeFrame(class FD3D12Adapter* InParent) :
		FGPUProfilerEventNodeFrame(),
		FD3D12AdapterChild(InParent),
		RootEventTiming(InParent, 1)
	{
		RootEventTiming.InitDynamicRHI();
	}

	~FD3D12EventNodeFrame()
	{
		RootEventTiming.ReleaseDynamicRHI();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual void LogDisjointQuery() override;

	/** Timer tracking inclusive time spent in the root nodes. */
	FD3D12BufferedGPUTiming RootEventTiming;
};

namespace D3D12RHI
{
	/**
	* Encapsulates GPU profiling logic and data.
	* There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
	*/
	struct FD3DGPUProfiler : public FGPUProfiler, public FD3D12AdapterChild
	{
		/** GPU hitch profile histories */
		TIndirectArray<FD3D12EventNodeFrame> GPUHitchEventNodeFrames;

		FD3DGPUProfiler(FD3D12Adapter* Parent)
			: FD3D12AdapterChild(Parent)
			, FrameTiming(Parent, 8)
		{}

		//FD3DGPUProfiler(class FD3D12Device* InParent) :
		//	FGPUProfiler(),
		//    FrameTiming(InParent, 4),
		//    FD3D12DeviceChild(InParent)
		//{
		//	// Initialize Buffered timestamp queries 
		//	FrameTiming.InitResource();
		//}

		void Init()
		{
			// Initialize Buffered timestamp queries 
			FrameTiming.InitResource();
		}

		virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
		{
			FD3D12EventNode* EventNode = new FD3D12EventNode(InName, InParent, GetParentAdapter());
			return EventNode;
		}

		virtual void PushEvent(const TCHAR* Name, FColor Color) override;
		virtual void PopEvent() override;

		void BeginFrame(class FD3D12DynamicRHI* InRHI);

		void EndFrame(class FD3D12DynamicRHI* InRHI);

		bool CheckGpuHeartbeat() const;

		/**
		 * Calculate the amount of GPU idle time between two timestamps
		 * @param StartTime - start timestamp
		 * @param EndTime - end timestamp
		 * @return number of idle GPU clock ticks between or 0 if command list execution time isn't tracked
		 */
		uint64 CalculateIdleTime(uint64 StartTime, uint64 EndTime);

#if NV_AFTERMATH
		virtual void PushEvent(const TCHAR* Name, FColor Color, GFSDK_Aftermath_ContextHandle Context);

		void RegisterCommandList(GFSDK_Aftermath_ContextHandle context);
		void UnregisterCommandList(GFSDK_Aftermath_ContextHandle context);

		TArray<GFSDK_Aftermath_ContextHandle> AftermathContexts;
		FCriticalSection AftermathLock;

		TArray<uint32> PushPopStack;
		TMap<uint32, FString> CachedStrings;
#endif

		/** Used to measure GPU time per frame. */
		FD3D12BufferedGPUTiming FrameTiming;

	private:
		/** Flush existing command lists and start command list execution time tracking */
		void DoPreProfileGPUWork();

		/** Flush existing command lists and obtain timing results of all tracked command lists */
		void DoPostProfileGPUWork();

		typedef typename FD3D12CommandListManager::FResolvedCmdListExecTime FResolvedCmdListExecTime;

		/** Timstamps marking the beginning of tracked command lists */
		TArray<uint64> CmdListStartTimestamps;
		/** Timstamps marking the end of tracked command lists */
		TArray<uint64> CmdListEndTimestamps;
		/** Accumulated idle GPU ticks before each corresponding command list */
		TArray<uint64> IdleTimeCDF;
	};
}
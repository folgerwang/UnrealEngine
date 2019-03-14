// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUtil.h: Vulkan Utility definitions.
=============================================================================*/

#pragma once

#include "GPUProfiler.h"

#include "Serialization/MemoryWriter.h"

class FVulkanCmdBuffer;
class FVulkanCommandListContext;
class FVulkanTimingQueryPool;

class FVulkanGPUTiming : public FGPUTiming
{
public:
	FVulkanGPUTiming(FVulkanCommandListContext* InCmd, FVulkanDevice* InDevice)
		: Device(InDevice)
		, bIsTiming(false)
		, bEndTimestampIssued(false)
		, CmdContext(InCmd)
	{
	}

	~FVulkanGPUTiming();

	/**
	 * Start a GPU timing measurement.
	 */
	void StartTiming(FVulkanCmdBuffer* CmdBuffer = nullptr);

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void EndTiming(FVulkanCmdBuffer* CmdBuffer = nullptr);

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64 GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	 * Initializes all Vulkan resources.
	 */
	void Initialize();

	/**
	 * Releases all Vulkan resources.
	 */
	void Release();

	bool IsComplete() const
	{
		check(bEndTimestampIssued);
		return true;
	}

	static void CalibrateTimers(FVulkanCommandListContext& InCmdContext);

private:
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	FVulkanDevice* Device;

	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool bIsTiming;
	bool bEndTimestampIssued;

	FVulkanCommandListContext* CmdContext;
	FVulkanTimingQueryPool* Pool = nullptr;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FVulkanEventNode : public FGPUProfilerEventNode
{
public:
	FVulkanEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, FVulkanCommandListContext* InCmd, FVulkanDevice* InDevice) :
		FGPUProfilerEventNode(InName, InParent),
		Timing(InCmd, InDevice)
	{
		// Initialize Buffered timestamp queries 
		Timing.Initialize();
	}

	virtual ~FVulkanEventNode()
	{
		Timing.Release(); 
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override final;


	virtual void StartTiming() override final
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override final
	{
		Timing.EndTiming();
	}

	FVulkanGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FVulkanEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:

	FVulkanEventNodeFrame(FVulkanCommandListContext* InCmd, FVulkanDevice* InDevice)
		: RootEventTiming(InCmd, InDevice)
	{
		RootEventTiming.Initialize();
	}

	~FVulkanEventNodeFrame()
	{
		RootEventTiming.Release();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override final;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override final;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override final;

	virtual bool PlatformDisablesVSync() const { return true; }

	/** Timer tracking inclusive time spent in the root nodes. */
	FVulkanGPUTiming RootEventTiming;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FVulkanGPUProfiler : public FGPUProfiler
{
	/** GPU hitch profile histories */
	TIndirectArray<FVulkanEventNodeFrame> GPUHitchEventNodeFrames;

	FVulkanGPUProfiler(FVulkanCommandListContext* InCmd, FVulkanDevice* InDevice)
		: bCommandlistSubmitted(false)
		, Device(InDevice)
		, CmdContext(InCmd)
	{
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override final
	{
		FVulkanEventNode* EventNode = new FVulkanEventNode(InName, InParent, CmdContext, Device);
		return EventNode;
	}

	void BeginFrame();

	void EndFrameBeforeSubmit();
	void EndFrame();

	bool bCommandlistSubmitted;
	FVulkanDevice* Device;
	FVulkanCommandListContext* CmdContext;

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	void PushMarkerForCrash(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TCHAR* Name);
	void PopMarkerForCrash(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer);
	void DumpCrashMarkers(void* BufferData);
#endif

	// For crash/marker tracking
	TMap<uint32, FString> CachedStrings;
	TArray<uint32> PushPopStack;
};

namespace VulkanRHI
{
	/**
	 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	 * @param	Result - The result code to check
	 * @param	Code - The code which yielded the result.
	 * @param	VkFunction - Tested function name.
	 * @param	Filename - The filename of the source file containing Code.
	 * @param	Line - The line number of Code within Filename.
	 */
	extern VULKANRHI_API void VerifyVulkanResult(VkResult Result, const ANSICHAR* VkFuntion, const ANSICHAR* Filename, uint32 Line);

	VkBuffer CreateBuffer(FVulkanDevice* InDevice, VkDeviceSize Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryRequirements& OutMemoryRequirements);
}

#define VERIFYVULKANRESULT(VkFunction)				{ const VkResult ScopedResult = VkFunction; if (ScopedResult != VK_SUCCESS) { VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); }}
#define VERIFYVULKANRESULT_EXPANDED(VkFunction)		{ const VkResult ScopedResult = VkFunction; if (ScopedResult < VK_SUCCESS) { VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); }}


template<typename T>
inline bool CopyAndReturnNotEqual(T& A, T B)
{
	const bool bOut = A != B;
	A = B;
	return bOut;
}

template <int Version>
class TDataKeyBase;

template <>
class TDataKeyBase<0>
{
protected:
	template <class DataReceiver>
	void GetData(DataReceiver&& ReceiveData)
	{
		TArray<uint8> TempData;
		ReceiveData(TempData);
	}

	void SetData(const void* InData, uint32 InSize) {}

	void CopyDataDeep(TDataKeyBase& Result) const {}
	void CopyDataShallow(TDataKeyBase& Result) const {}
	bool IsDataEquals(const TDataKeyBase& Other) const { return true; }

protected:
	uint32 Hash = 0;
};

template <>
class TDataKeyBase<1>
{
protected:
	template <class DataReceiver>
	void GetData(DataReceiver&& ReceiveData)
	{
		EnsureDataStorage();
		ReceiveData(*Data);
	}

	void SetData(const void* InData, uint32 InSize)
	{
		EnsureDataStorage();
		Data->SetNum(InSize);
		FMemory::Memcpy(Data->GetData(), InData, InSize);
	}

	void CopyDataDeep(TDataKeyBase& Result) const
	{
		check(Data);
		Result.DataStorage = MakeUnique<TArray<uint8>>(*Data);
		Result.Data = Result.DataStorage.Get();
	}

	void CopyDataShallow(TDataKeyBase& Result) const
	{
		check(Data);
		Result.Data = Data;
	}

	bool IsDataEquals(const TDataKeyBase& Other) const
	{
		check(Data && Other.Data);
		check(Data->Num() == Other.Data->Num());
		check(FMemory::Memcmp(Data->GetData(), Other.Data->GetData(), Data->Num()) == 0);
		return true;
	}

private:
	void EnsureDataStorage()
	{
		if (!DataStorage)
		{
			DataStorage = MakeUnique<TArray<uint8>>();
			Data = DataStorage.Get();
		}
	}

protected:
	uint32 Hash = 0;
	TArray<uint8> *Data = nullptr;
private:
	TUniquePtr<TArray<uint8>> DataStorage;
};

template <>
class TDataKeyBase<2> : public TDataKeyBase<1>
{
protected:
	bool IsDataEquals(const TDataKeyBase& Other) const
	{
		check(Data && Other.Data);
		return ((Data->Num() == Other.Data->Num()) &&
			(FMemory::Memcmp(Data->GetData(), Other.Data->GetData(), Data->Num()) == 0));
	}
};

template <class Derived, bool AlwaysCompareData = false>
class TDataKey : private TDataKeyBase<AlwaysCompareData ? 2 : (DO_CHECK != 0)>
{
public:
	template <class ArchiveWriter>
	void GenerateFromArchive(ArchiveWriter&& WriteToArchive, int32 DataReserve = 0)
	{
		this->GetData([&](TArray<uint8>& InData)
		{
			FMemoryWriter Ar(InData);

			InData.Reset(DataReserve);
			WriteToArchive(Ar);

			this->Hash = FCrc::MemCrc32(InData.GetData(), InData.Num());
		});
	}

	template <class ObjectType>
	void GenerateFromObject(const ObjectType& Object)
	{
		GenerateFromData(&Object, sizeof(Object));
	}

	void GenerateFromData(const void* InData, uint32 InSize)
	{
		this->SetData(InData, InSize);
		this->Hash = FCrc::MemCrc32(InData, InSize);
	}

	uint32 GetHash() const
	{
		return this->Hash;
	}

	Derived CopyDeep() const
	{
		Derived Result;
		Result.Hash = this->Hash;
		this->CopyDataDeep(Result);
		return Result;
	}

	Derived CopyShallow() const
	{
		Derived Result;
		Result.Hash = this->Hash;
		this->CopyDataShallow(Result);
		return Result;
	}

	friend uint32 GetTypeHash(const Derived& Key)
	{
		return Key.Hash;
	}

	friend bool operator==(const Derived& A, const Derived& B)
	{
		return ((A.Hash == B.Hash) && A.IsDataEquals(B));
	}
};

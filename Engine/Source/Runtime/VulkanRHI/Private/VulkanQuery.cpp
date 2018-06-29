// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQuery.cpp: Vulkan query RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "EngineGlobals.h"

struct FRHICommandWaitForFence final : public FRHICommand<FRHICommandWaitForFence>
{
	FVulkanCommandBufferManager* CmdBufferMgr;
	FVulkanCmdBuffer* CmdBuffer;
	uint64 FenceCounter;

	FORCEINLINE_DEBUGGABLE FRHICommandWaitForFence(FVulkanCommandBufferManager* InCmdBufferMgr, FVulkanCmdBuffer* InCmdBuffer, uint64 InFenceCounter)
		: CmdBufferMgr(InCmdBufferMgr)
		, CmdBuffer(InCmdBuffer)
		, FenceCounter(InFenceCounter)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		if (FenceCounter == CmdBuffer->GetFenceSignaledCounter())
		{
			uint32 IdleStart = FPlatformTime::Cycles();
			check(CmdBuffer->IsSubmitted());
			CmdBufferMgr->WaitForCmdBuffer(CmdBuffer);
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
		}
	}
};

TAutoConsoleVariable<int32> GSubmitOcclusionBatchCmdBufferCVar(
	TEXT("r.Vulkan.SubmitOcclusionBatchCmdBuffer"),
	1,
	TEXT("1 to submit the cmd buffer after end occlusion query batch (default)"),
	ECVF_RenderThreadSafe
);

FCriticalSection GQueryLock;

#if VULKAN_USE_NEW_QUERIES
const uint32 GMinNumberOfQueriesInPool = 256;
const uint32 GMaxLifetimeTimestampQueries = 1024;


inline VkResult FVulkanQueryPool::InternalGetQueryPoolResults(uint32 FirstQuery, uint32 NumQueries, VkQueryResultFlags ExtraFlags)
{
	VkResult Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, FirstQuery, NumQueries, NumQueries * sizeof(uint64), QueryOutput.GetData() + FirstQuery, sizeof(uint64), VK_QUERY_RESULT_64_BIT | ExtraFlags);
	return Result;
}

inline int32 FVulkanQueryPool::AllocateQuery()
{
	FScopeLock ScopeLock(&GQueryLock);
	int32 UsedQuery = NumUsedQueries;
	++NumUsedQueries;
	checkf((uint32)UsedQuery < MaxQueries, TEXT("Internal error allocating query! UsedQuery=%d NumUsedQueries=%d MaxQueries=%d"), UsedQuery, NumUsedQueries, MaxQueries);
	return UsedQuery;
}

bool FVulkanOcclusionQueryPool::GetAllResults(bool bWait)
{
	FScopeLock ScopeLock(&GQueryLock);
	check(!bHasResults);
	check(CmdBuffer);
	checkf(CmdBuffer->GetSubmittedFenceCounter() >= FenceCounter, TEXT("Tried to read a query that has not been submitted!"));
	VkResult Result = VK_NOT_READY;
	bool bFencePassed = false;
	if (CmdBuffer->GetFenceSignaledCounter() > FenceCounter)
	{
		bFencePassed = true;
		Result = InternalGetQueryPoolResults(0);
		if (Result == VK_SUCCESS)
		{
			bHasResults = true;
			return true;
		}
	}

	if (Result == VK_NOT_READY)
	{
		if (bWait)
		{
			uint32 IdleStart = FPlatformTime::Cycles();

			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

			// We'll do manual wait
			double StartTime = FPlatformTime::Seconds();

			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			bool bSuccess = false;
			int32 NumLoops = 0;
			while (!bSuccess)
			{
				FPlatformProcess::SleepNoStats(0);

				// pump RHIThread to make sure these queries have actually been submitted to the GPU.
				if (IsInActualRenderingThread())
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
				}

				Result = InternalGetQueryPoolResults(0);
				if (Result == VK_SUCCESS)
				{
					bSuccess = true;
					break;
				}
				else if (Result == VK_NOT_READY)
				{
					bSuccess = false;
				}
				else
				{
					bSuccess = false;
					VERIFYVULKANRESULT(Result);
				}

				// timer queries are used for Benchmarks which can stall a bit more
				const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
				// look for gpu stuck/crashed
				if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
				{
					if (QueryType == VK_QUERY_TYPE_OCCLUSION)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion results. (%.1f s)"), TimeoutValue);
					}
					else
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion/timer results. (%.1f s)"), TimeoutValue);
					}
					return false;
				}

				++NumLoops;
			}

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			bHasResults = true;
			return true;
		}
	}
	else
	{
		VERIFYVULKANRESULT(Result);
	}

	return false;
}

void FVulkanQueryPool::ResetAll(FVulkanCmdBuffer* InCmdBuffer)
{
	VulkanRHI::vkCmdResetQueryPool(InCmdBuffer->GetHandle(), QueryPool, 0, MaxQueries);
	++NumResets;
}


void FVulkanOcclusionQueryPool::SetFence(FVulkanCmdBuffer* InCmdBuffer)
{
	check(InCmdBuffer);
	CmdBuffer = InCmdBuffer;
	FenceCounter = InCmdBuffer->GetFenceSignaledCounter();
	check(!bHasResults);
}

void FVulkanOcclusionQueryPool::Reset(FVulkanCmdBuffer* InCmdBuffer)
{
	check(!CmdBuffer || CmdBuffer->GetFenceSignaledCounter() > FenceCounter);
	CmdBuffer = nullptr;
	FenceCounter = UINT64_MAX;
	NumUsedQueries = 0;
	bHasResults = false;
	FVulkanQueryPool::ResetAll(InCmdBuffer);
}

bool FVulkanTimestampQueryPool::GetResults(uint32 QueryIndex, /*uint32 Word, uint32 Bit, */bool bWait, uint64& OutResults)
{
	//check((HasResultsMask[Word] & Bit) == 0);

	VkResult Result = InternalGetQueryPoolResults(QueryIndex, 1, 0/*bWait ? 0 : VK_QUERY_RESULT_PARTIAL_BIT*/);
	if (Result == VK_SUCCESS)
	{
		OutResults = QueryOutput[QueryIndex];
		//HasResultsMask[Word] |= Bit;
		return true;
	}
	else if (Result == VK_NOT_READY)
	{
		if (bWait)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

			// We'll do manual wait
			double StartTime = FPlatformTime::Seconds();

			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			bool bSuccess = false;
			while (!bSuccess)
			{
				FPlatformProcess::SleepNoStats(0);

				// pump RHIThread to make sure these queries have actually been submitted to the GPU.
				if (IsInActualRenderingThread())
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
				}

				Result = InternalGetQueryPoolResults(QueryIndex, 1, 0);
				if (Result == VK_SUCCESS)
				{
					bSuccess = true;
					break;
				}
				else if (Result == VK_NOT_READY)
				{
					bSuccess = false;
				}
				else
				{
					bSuccess = false;
					VERIFYVULKANRESULT(Result);
				}

				// timer queries are used for Benchmarks which can stall a bit more
				const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
				// look for gpu stuck/crashed
				if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
				{
					UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on timer results. (%.1f s)"), TimeoutValue);
					return false;
				}
			}

			OutResults = QueryOutput[QueryIndex];
			//HasResultsMask[Word] |= Bit;
			return true;
		}
	}
	else
	{
		VERIFYVULKANRESULT(Result);
	}

	return false;
}

void FVulkanCommandListContext::BeginOcclusionQueryBatch(FVulkanCmdBuffer* CmdBuffer, uint32 NumQueriesInBatch)
{
	FScopeLock ScopeLock(&GQueryLock);
	ensure(IsImmediate());
	ensure(CmdBuffer->IsOutsideRenderPass());
	checkf(!CurrentOcclusionQueryPool, TEXT("BeginOcclusionQueryBatch called without corresponding EndOcclusionQueryBatch!"));
	CurrentOcclusionQueryPool = Device->AcquireOcclusionQueryPool(NumQueriesInBatch);
	CurrentOcclusionQueryPool->Reset(CmdBuffer);
}

FVulkanOcclusionQueryPool* FVulkanDevice::AcquireOcclusionQueryPool(uint32 NumQueries)
{
	if (!OcclusionQueryPools[CurrentOcclusionQueryPool] || OcclusionQueryPools[CurrentOcclusionQueryPool]->GetMaxQueries() < NumQueries)
	{
		delete OcclusionQueryPools[CurrentOcclusionQueryPool];
		OcclusionQueryPools[CurrentOcclusionQueryPool] = new FVulkanOcclusionQueryPool(this, AlignArbitrary(NumQueries, GMinNumberOfQueriesInPool));
	}

	FVulkanOcclusionQueryPool* ResultPool = OcclusionQueryPools[CurrentOcclusionQueryPool];
	CurrentOcclusionQueryPool = (CurrentOcclusionQueryPool + 1) % OcclusionQueryPools.Num();

	return ResultPool;
}

FVulkanTimestampQueryPool* FVulkanDevice::PrepareTimestampQueryPool(bool& bOutRequiresReset)
{
	bOutRequiresReset = false;
	if (!TimestampQueryPool)
	{
		//#todo-rco: Create this earlier
		TimestampQueryPool = new FVulkanTimestampQueryPool(this, GMaxLifetimeTimestampQueries);
		bOutRequiresReset = true;
	}

	// Wrap around the queries
	if (TimestampQueryPool->GetNumAllocatedQueries() >= TimestampQueryPool->GetMaxQueries())
	{
		//#todo-rco: Check overlap and grow if necessary
		TimestampQueryPool->NumUsedQueries = 0;
	}

	return TimestampQueryPool;
}

void FVulkanCommandListContext::EndOcclusionQueryBatch(FVulkanCmdBuffer* CmdBuffer)
{
	FScopeLock ScopeLock(&GQueryLock);
	ensure(IsImmediate());
	checkf(CurrentOcclusionQueryPool, TEXT("EndOcclusionQueryBatch called without corresponding BeginOcclusionQueryBatch!"));
	CurrentOcclusionQueryPool->SetFence(CmdBuffer);
	CurrentOcclusionQueryPool = nullptr;
	TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);

	// Sync point
	if (GSubmitOcclusionBatchCmdBufferCVar.GetValueOnAnyThread())
	{
		RequestSubmitCurrentCommands();
		SafePointSubmit();
	}
}
#endif

FVulkanRenderQuery::FVulkanRenderQuery(ERenderQueryType InQueryType)
#if VULKAN_USE_NEW_QUERIES
	: QueryType(InQueryType)
#else
	: CurrentQueryIdx(0)
	, QueryType(InQueryType)
	, CurrentCmdBuffer(nullptr)
#endif
{
	INC_DWORD_STAT(STAT_VulkanNumQueries);
#if VULKAN_USE_NEW_QUERIES
#else
	for (int Index = 0; Index < NumQueries; ++Index)
	{
		QueryIndices[Index] = -1;
		QueryPools[Index] = nullptr;
	}
#endif
}

FVulkanRenderQuery::~FVulkanRenderQuery()
{
	DEC_DWORD_STAT(STAT_VulkanNumQueries);
#if VULKAN_USE_NEW_QUERIES
#else
	for (int Index = 0; Index < NumQueries; ++Index)
	{
		if (QueryIndices[Index] != -1)
		{
			FScopeLock Lock(&GQueryLock);
			((FVulkanBufferedQueryPool*)QueryPools[Index])->ReleaseQuery(QueryIndices[Index]);
		}
	}
#endif
}

inline void FVulkanRenderQuery::Begin(FVulkanCmdBuffer* InCmdBuffer)
{
#if VULKAN_USE_NEW_QUERIES
	BeginCmdBuffer = InCmdBuffer;
	check(State == EState::Reset);
	if (QueryType == RQT_Occlusion)
	{
		check(Pool);
		VulkanRHI::vkCmdBeginQuery(InCmdBuffer->GetHandle(), Pool->GetHandle(), QueryIndex, VK_QUERY_CONTROL_PRECISE_BIT);
		State = EState::InBegin;
		LastPoolReset = Pool->NumResets;
	}
	else
	{
		ensureMsgf(0, TEXT("Timer queries do NOT support Begin()!"));
	}
#else
	CurrentCmdBuffer = InCmdBuffer;
	ensure(GetActiveQueryIndex() != -1);
	if (QueryType == RQT_Occlusion)
	{
		VulkanRHI::vkCmdBeginQuery(InCmdBuffer->GetHandle(), GetActiveQueryPool()->GetHandle(), GetActiveQueryIndex(), VK_QUERY_CONTROL_PRECISE_BIT);
	}
	else
	{
		ensure(0);
	}
#endif
}

inline void FVulkanRenderQuery::End(FVulkanCmdBuffer* InCmdBuffer)
{
#if VULKAN_USE_NEW_QUERIES
	ensure(QueryType != RQT_Occlusion || BeginCmdBuffer == InCmdBuffer);
	if (QueryType == RQT_Occlusion)
	{
		check(State == EState::InBegin);
		check(Pool);
		VulkanRHI::vkCmdEndQuery(InCmdBuffer->GetHandle(), Pool->GetHandle(), QueryIndex);
		check(LastPoolReset == Pool->NumResets);
	}
	else
	{
		check(State == EState::Reset);
		BeginCmdBuffer = InCmdBuffer;
		VulkanRHI::vkCmdWriteTimestamp(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Pool->GetHandle(), QueryIndex);
		LastPoolReset = Pool->NumResets;
	}
	State = EState::InEnd;
#else
	ensure(QueryType != RQT_Occlusion || CurrentCmdBuffer == InCmdBuffer);
	ensure(GetActiveQueryIndex() != -1);

	if (QueryType == RQT_Occlusion)
	{
		VulkanRHI::vkCmdEndQuery(InCmdBuffer->GetHandle(), GetActiveQueryPool()->GetHandle(), GetActiveQueryIndex());
	}
	else
	{
		VulkanRHI::vkCmdWriteTimestamp(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GetActiveQueryPool()->GetHandle(), GetActiveQueryIndex());
	}
#endif
}

bool FVulkanRenderQuery::GetResult(FVulkanDevice* Device, uint64& OutResult, bool bWait)
{
#if VULKAN_USE_NEW_QUERIES
	FScopeLock ScopeLock(&GQueryLock);
	if (State == EState::HasResults)
	{
		OutResult = Result;
		return true;
	}

	check(Pool);
	check(State == EState::InEnd);
	if (QueryType == RQT_Occlusion)
	{
		check(IsInRenderingThread());
		if (((FVulkanOcclusionQueryPool*)Pool)->GetResults(QueryIndex, bWait, Result))
		{
			check(LastPoolReset == Pool->NumResets);
			State = EState::HasResults;
			OutResult = Result;
			return true;
		}
	}
	else
	{
		check(IsInRenderingThread() || IsInRHIThread());
		if (((FVulkanTimestampQueryPool*)Pool)->GetResults(QueryIndex, bWait, Result))
		{
			check(LastPoolReset == Pool->NumResets);
			State = EState::HasResults;
			OutResult = Result;
			return true;
		}
	}
	return false;
#else
	if (GetActiveQueryIndex() != -1)
	{
		check(IsInRenderingThread() || IsInRHIThread());
		FVulkanCommandListContext& Context = Device->GetImmediateContext();
		FVulkanBufferedQueryPool* Pool = (FVulkanBufferedQueryPool*)GetActiveQueryPool();
		return Pool->GetResults(Context, this, bWait, OutResult);
	}
	return false;
#endif
}

FVulkanQueryPool::FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InMaxQueries, VkQueryType InQueryType)
	: VulkanRHI::FDeviceChild(InDevice)
	, QueryPool(VK_NULL_HANDLE)
	, MaxQueries(InMaxQueries)
	, QueryType(InQueryType)
{
	check(InDevice);

	VkQueryPoolCreateInfo PoolCreateInfo;
	ZeroVulkanStruct(PoolCreateInfo, VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
	PoolCreateInfo.queryType = QueryType;
	PoolCreateInfo.queryCount = MaxQueries;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateQueryPool(Device->GetInstanceHandle(), &PoolCreateInfo, nullptr, &QueryPool));

#if VULKAN_USE_NEW_QUERIES
	QueryOutput.AddUninitialized(MaxQueries);
#endif
}

FVulkanQueryPool::~FVulkanQueryPool()
{
#if VULKAN_USE_NEW_QUERIES
	VulkanRHI::vkDestroyQueryPool(Device->GetInstanceHandle(), QueryPool, nullptr);
	QueryPool = VK_NULL_HANDLE;
#else
	check(QueryPool == VK_NULL_HANDLE);
#endif
}

#if VULKAN_USE_NEW_QUERIES
#else
void FVulkanQueryPool::Destroy()
{
	VulkanRHI::vkDestroyQueryPool(Device->GetInstanceHandle(), QueryPool, nullptr);
	QueryPool = VK_NULL_HANDLE;
}

void FVulkanQueryPool::Reset(FVulkanCmdBuffer* InCmdBuffer)
{
	VulkanRHI::vkCmdResetQueryPool(InCmdBuffer->GetHandle(), QueryPool, 0, MaxQueries);
}

inline bool FVulkanBufferedQueryPool::GetResults(FVulkanCommandListContext& Context, FVulkanRenderQuery* Query, bool bWait, uint64& OutResult)
{
	VkQueryResultFlags Flags = VK_QUERY_RESULT_64_BIT;
#if VULKAN_USE_QUERY_WAIT
	if (bWait)
	{
		Flags |= VK_QUERY_RESULT_WAIT_BIT;
	}
#endif
	{
		uint64 Bit = (uint64)(Query->GetActiveQueryIndex() % 64);
		uint64 BitMask = (uint64)1 << Bit;

		// Try to read in bulk if possible
		const uint64 AllUsedMask = (uint64)-1;
		uint32 Word = Query->GetActiveQueryIndex() / 64;

		if ((StartedQueryBits[Word] & BitMask) == 0)
		{
			// query never started/ended so how can we get a result ?
			OutResult = 0;
			return true;
		}

#if VULKAN_USE_QUERY_WAIT
		if ((ReadResultsBits[Word] & BitMask) == 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);
			VkResult ScopedResult = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, Query->GetActiveQueryIndex(),
				1, sizeof(uint64), &QueryOutput[Query->GetActiveQueryIndex()], sizeof(uint64), Flags);
			if (ScopedResult != VK_SUCCESS)
			{
				if (bWait == false && ScopedResult == VK_NOT_READY)
				{
					OutResult = 0;
					return false;
				}
				else
				{
					VulkanRHI::VerifyVulkanResult(ScopedResult, "vkGetQueryPoolResults", __FILE__, __LINE__);
				}
			}

			ReadResultsBits[Word] = ReadResultsBits[Word] | BitMask;
		}
#else
		if ((ReadResultsBits[Word] & BitMask) == 0)
		{
			VkResult ScopedResult = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, Query->GetActiveQueryIndex(),
				1, sizeof(uint64), &QueryOutput[Query->GetActiveQueryIndex()], sizeof(uint64), Flags);
			if (ScopedResult == VK_SUCCESS)
			{
				ReadResultsBits[Word] = ReadResultsBits[Word] | BitMask;
			}
			else if (ScopedResult == VK_NOT_READY)
			{
				if (bWait)
				{
					uint32 IdleStart = FPlatformTime::Cycles();

					SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

					// We'll do manual wait
					double StartTime = FPlatformTime::Seconds();

					ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
					bool bSuccess = false;
					while (!bSuccess)
					{
						FPlatformProcess::SleepNoStats(0);

						// pump RHIThread to make sure these queries have actually been submitted to the GPU.
						if (IsInActualRenderingThread())
						{
							FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
						}

						ScopedResult = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, Query->GetActiveQueryIndex(),
							1, sizeof(uint64), &QueryOutput[Query->GetActiveQueryIndex()], sizeof(uint64), Flags);
						if (ScopedResult == VK_SUCCESS)
						{
							bSuccess = true;
							break;
						}
						else if (ScopedResult == VK_NOT_READY)
						{
							bSuccess = false;
						}
						else
						{
							bSuccess = false;
							VulkanRHI::VerifyVulkanResult(ScopedResult, "vkGetQueryPoolResults", __FILE__, __LINE__);
						}

						// timer queries are used for Benchmarks which can stall a bit more
						const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
						// look for gpu stuck/crashed
						if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
						{
							if (QueryType == VK_QUERY_TYPE_OCCLUSION)
							{
								UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion results. (%.1f s)"), TimeoutValue);
							}
							else
							{
								UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion/timer results. (%.1f s)"), TimeoutValue);
							}
							return false;
						}
					}

					GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
					GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
					check(bSuccess);
					ReadResultsBits[Word] = ReadResultsBits[Word] | BitMask;
				}
				else
				{
					OutResult = 0;
					return false;
				}
			}
			else
			{
				VulkanRHI::VerifyVulkanResult(ScopedResult, "vkGetQueryPoolResults", __FILE__, __LINE__);
			}
		}
#endif

		OutResult = QueryOutput[Query->GetActiveQueryIndex()];
		if (QueryType == VK_QUERY_TYPE_TIMESTAMP)
		{
			double NanoSecondsPerTimestamp = Device->GetDeviceProperties().limits.timestampPeriod;
			checkf(NanoSecondsPerTimestamp > 0, TEXT("Driver said it allowed timestamps but returned invalid period %f!"), NanoSecondsPerTimestamp);
			OutResult *= (NanoSecondsPerTimestamp / 1e3); // to microSec
		}
	}

	return true;
}
#endif

void FVulkanCommandListContext::ReadAndCalculateGPUFrameTime()
{
#if VULKAN_USE_NEW_QUERIES
	check(IsImmediate());

	if (FrameTiming)
	{
		const uint64 Delta = FrameTiming->GetTiming(false);
		const double SecondsPerCycle = FPlatformTime::GetSecondsPerCycle();
		const double Frequency = double(FVulkanGPUTiming::GetTimingFrequency());
		GGPUFrameTime = FMath::TruncToInt(double(Delta) / Frequency / SecondsPerCycle);
	}
	else
	{
		GGPUFrameTime = 0;
	}

	//static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Vulkan.ProfileCmdBuffers"));
	//if (CVar->GetInt() != 0)
	//{
	//	const uint64 Delta = GetCommandBufferManager()->CalculateGPUTime();
	//	GGPUFrameTime = Delta ? (Delta / 1e6) / FPlatformTime::GetSecondsPerCycle() : 0;
	//}
#else
	check(IsImmediate());

	if (FrameTiming)
	{
		const uint64 Delta = FrameTiming->GetTiming(false);
		GGPUFrameTime = Delta ? (Delta / 1e6) / FPlatformTime::GetSecondsPerCycle() : 0;
	}
	else
	{
		GGPUFrameTime = 0;
	}

	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Vulkan.ProfileCmdBuffers"));
	if (CVar->GetInt() != 0)
	{
		const uint64 Delta = GetCommandBufferManager()->CalculateGPUTime();
		GGPUFrameTime = Delta ? (Delta / 1e6) / FPlatformTime::GetSecondsPerCycle() : 0;
	}
#endif
}


FRenderQueryRHIRef FVulkanDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	FVulkanRenderQuery* Query = new FVulkanRenderQuery(QueryType);
	return Query;
}

bool FVulkanDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI, uint64& OutResult, bool bWait)
{
#if VULKAN_USE_NEW_QUERIES
	FScopeLock ScopeLock(&GQueryLock);
	check(IsInRenderingThread());

	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);
	return Query->GetResult(Device, OutResult, bWait);
#else
	check(IsInRenderingThread());

	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);
	check(Query);
	return Query->GetResult(Device, OutResult, bWait);
#endif
}

void FVulkanCommandListContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
#if VULKAN_USE_NEW_QUERIES
	FScopeLock ScopeLock(&GQueryLock);
	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);
	if (Query->QueryType == RQT_Occlusion)
	{
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		const int32 QueryIndex = CurrentOcclusionQueryPool->AllocateQuery();
		Query->Reset(CurrentOcclusionQueryPool, QueryIndex);
		Query->Begin(CmdBuffer);
	}
	else
	{
		check(0);
	}
#else
	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);
	if (Query->QueryType == RQT_Occlusion)
	{
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		AdvanceQuery(Query);
		Query->Begin(CmdBuffer);
	}
	else
	{
		check(0);
	}
#endif
}

#if VULKAN_USE_NEW_QUERIES
#else
void FVulkanCommandListContext::AdvanceQuery(FVulkanRenderQuery* Query)
{
	//reset prev query
	if (Query->GetActiveQueryIndex() != -1)
	{
		FVulkanBufferedQueryPool* OcclusionPool = (FVulkanBufferedQueryPool*)Query->GetActiveQueryPool();
		CurrentOcclusionQueryData.AddToResetList(OcclusionPool, Query->GetActiveQueryIndex());
	}

	// advance
	Query->AdvanceQueryIndex();

	// alloc if needed
	if (Query->GetActiveQueryIndex() == -1)
	{
		uint32 QueryIndex = 0;
		FVulkanBufferedQueryPool* Pool = nullptr;

		{
			FScopeLock Lock(&GQueryLock);

			if (Query->QueryType == RQT_AbsoluteTime)
			{
				Pool = &Device->FindAvailableTimestampQueryPool();
			}
			else
			{
				Pool = &Device->FindAvailableOcclusionQueryPool();
			}
			ensure(Pool);

			bool bResult = Pool->AcquireQuery(QueryIndex);
			check(bResult);
		}
		
		Query->SetActiveQueryIndex(QueryIndex);
		Query->SetActiveQueryPool(Pool);
	}

	// mark at begin
	((FVulkanBufferedQueryPool*)Query->GetActiveQueryPool())->MarkQueryAsStarted(Query->GetActiveQueryIndex());
}

void FVulkanCommandListContext::EndRenderQueryInternal(FVulkanCmdBuffer* CmdBuffer, FVulkanRenderQuery* Query)
{
	if (Query->QueryType == RQT_Occlusion)
	{
		if (Query->GetActiveQueryIndex() != -1)
		{
			Query->End(CmdBuffer);
		}
	}
	else
	{
		if (Device->GetDeviceProperties().limits.timestampComputeAndGraphics == VK_FALSE)
		{
			return;
		}

		AdvanceQuery(Query);

		// now start it
		Query->End(CmdBuffer);
	}
}
#endif

void FVulkanCommandListContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
#if VULKAN_USE_NEW_QUERIES
	FScopeLock ScopeLock(&GQueryLock);
	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (Query->QueryType == RQT_AbsoluteTime)
	{
		bool bRequiresReset = false;
		FVulkanTimestampQueryPool* Pool = Device->PrepareTimestampQueryPool(bRequiresReset);
		if (bRequiresReset)
		{
			FVulkanCmdBuffer* UploadCmdBuffer = CommandBufferManager->GetUploadCmdBuffer();
			Pool->ResetAll(UploadCmdBuffer);
			CommandBufferManager->SubmitUploadCmdBuffer(false);
		}
		const int32 QueryIndex = Pool->AllocateQuery();
		Query->Reset(Pool, QueryIndex);
	}

	Query->End(CmdBuffer);
#else
	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);
	return EndRenderQueryInternal(CommandBufferManager->GetActiveCmdBuffer(), Query);
#endif
}

#if VULKAN_USE_NEW_QUERIES
#else
void FVulkanCommandListContext::RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch)
{
	ensure(IsImmediate());

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	ensure(CmdBuffer->IsInsideRenderPass());
}

static inline void ProcessByte(VkCommandBuffer InCmdBufferHandle, FVulkanBufferedQueryPool* Pool, uint8 Bits, int32 BaseStartIndex)
{
	if (Bits)
	{
		VkQueryPool QueryPool = Pool->GetHandle();
		if (Bits == 0xff)
		{
			VulkanRHI::vkCmdResetQueryPool(InCmdBufferHandle, QueryPool, BaseStartIndex, 8);
			Pool->ResetReadResultBits(BaseStartIndex, 8);
		}
		else
		{
			while (Bits)
			{
				if (Bits & 1)
				{
					//#todo-rco: Group these
					VulkanRHI::vkCmdResetQueryPool(InCmdBufferHandle, QueryPool, BaseStartIndex, 1);
					Pool->ResetReadResultBits(BaseStartIndex, 1);
				}

				Bits >>= 1;
				++BaseStartIndex;
			}
		}
	}
}

static inline void Process16Bits(VkCommandBuffer InCmdBufferHandle, FVulkanBufferedQueryPool* Pool, uint16 Bits, int32 BaseStartIndex)
{
	if (Bits)
	{
		VkQueryPool QueryPool = Pool->GetHandle();
		if (Bits == 0xffff)
		{
			VulkanRHI::vkCmdResetQueryPool(InCmdBufferHandle, QueryPool, BaseStartIndex, 16);
			Pool->ResetReadResultBits(BaseStartIndex, 16);
		}
		else
		{
			ProcessByte(InCmdBufferHandle, Pool, (uint8)((Bits >> 0) & 0xff), BaseStartIndex + 0);
			ProcessByte(InCmdBufferHandle, Pool, (uint8)((Bits >> 8) & 0xff), BaseStartIndex + 8);
		}
	}
}

static inline void Process32Bits(VkCommandBuffer InCmdBufferHandle, FVulkanBufferedQueryPool* Pool, uint32 Bits, int32 BaseStartIndex)
{
	if (Bits)
	{
		VkQueryPool QueryPool = Pool->GetHandle();
		if (Bits == 0xffffffff)
		{
			VulkanRHI::vkCmdResetQueryPool(InCmdBufferHandle, QueryPool, BaseStartIndex, 32);
			Pool->ResetReadResultBits(BaseStartIndex, 32);
		}
		else
		{
			Process16Bits(InCmdBufferHandle, Pool, (uint16)((Bits >> 0) & 0xffff), BaseStartIndex + 0);
			Process16Bits(InCmdBufferHandle, Pool, (uint16)((Bits >> 16) & 0xffff), BaseStartIndex + 16);
		}
	}
}

void FVulkanCommandListContext::FOcclusionQueryData::ResetQueries(FVulkanCmdBuffer* InCmdBuffer)
{
	ensure(InCmdBuffer->IsOutsideRenderPass());
	const uint64 AllBitsMask = (uint64)-1;
	VkCommandBuffer CmdBufferHandle = InCmdBuffer->GetHandle();

	for (auto& Pair : ResetList)
	{
		TArray<uint64>& ListPerPool = Pair.Value;
		FVulkanBufferedQueryPool* Pool = (FVulkanBufferedQueryPool*)Pair.Key;

		// Initial bit Index
		int32 WordIndex = 0;
		while (WordIndex < ListPerPool.Num())
		{
			uint64 Bits = ListPerPool[WordIndex];
			VkQueryPool PoolHandle = Pool->GetHandle();
			if (Bits)
			{
				if (Bits == AllBitsMask)
				{
					// Quick early out
					VulkanRHI::vkCmdResetQueryPool(CmdBufferHandle, PoolHandle, WordIndex * 64, 64);
					Pool->ResetReadResultBits(WordIndex * 64, 64);
				}
				else
				{
					Process32Bits(CmdBufferHandle, Pool, (uint32)((Bits >> (uint64)0) & (uint64)0xffffffff), WordIndex * 64 + 0);
					Process32Bits(CmdBufferHandle, Pool, (uint32)((Bits >> (uint64)32) & (uint64)0xffffffff), WordIndex * 64 + 32);
				}
			}
			++WordIndex;
		}
	}
}

void FVulkanCommandListContext::RHIEndOcclusionQueryBatch()
{
	ensure(IsImmediate());

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	CurrentOcclusionQueryData.CmdBuffer = CmdBuffer;
	CurrentOcclusionQueryData.FenceCounter = CmdBuffer->GetFenceSignaledCounter();

	TransitionAndLayoutManager.EndRealRenderPass(CmdBuffer);

	// Resetting queries has to happen outside a render pass
	FVulkanCmdBuffer* UploadCmdBuffer = CommandBufferManager->GetUploadCmdBuffer();
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanResetQuery);
		CurrentOcclusionQueryData.ResetQueries(UploadCmdBuffer);
		CurrentOcclusionQueryData.ClearResetList();
	}
	CommandBufferManager->SubmitUploadCmdBuffer();

	// Sync point
	if (GSubmitOcclusionBatchCmdBufferCVar.GetValueOnAnyThread())
	{
		RequestSubmitCurrentCommands();
		SafePointSubmit();
	}
}
#endif

void FVulkanCommandListContext::WriteBeginTimestamp(FVulkanCmdBuffer* CmdBuffer)
{
	FrameTiming->StartTiming(CmdBuffer);
}

void FVulkanCommandListContext::WriteEndTimestamp(FVulkanCmdBuffer* CmdBuffer)
{
	FrameTiming->EndTiming();
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Query.cpp: D3D query RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

namespace D3D12RHI
{
	/**
	* RHI console variables used by queries.
	*/
	namespace RHIConsoleVariables
	{
		int32 bStablePowerState = 0;
		static FAutoConsoleVariableRef CVarStablePowerState(
			TEXT("D3D12.StablePowerState"),
			bStablePowerState,
			TEXT("If true, enable stable power state. This increases GPU timing measurement accuracy but may decrease overall GPU clock rate."),
			ECVF_Default
			);
	}
}
using namespace D3D12RHI;

FRenderQueryRHIRef FD3D12DynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	FD3D12Adapter* Adapter = &GetAdapter();

	check(QueryType == RQT_Occlusion || QueryType == RQT_AbsoluteTime);

	return Adapter->CreateLinkedObject<FD3D12RenderQuery>(FRHIGPUMask::All(), [QueryType](FD3D12Device* Device)
	{
		return new FD3D12RenderQuery(Device, QueryType);
	});
}

bool FD3D12DynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI, uint64& OutResult, bool bWait)
{
	check(IsInRenderingThread());
	FD3D12Adapter& Adapter = GetAdapter();

	// Multi-GPU support: might need to support per-GPU results eventually.
	// First generate the GPU node mask for of the latest queries.
	FRHIGPUMask RelevantNodeMask = FRHIGPUMask::GPU0();
	if (GNumExplicitGPUsForRendering > 1)
	{
		uint64 LatestTimestamp = 0;
		for (FD3D12RenderQuery* Query = FD3D12DynamicRHI::ResourceCast(QueryRHI); Query; Query = Query->GetNextObject())
		{
			if (Query->Timestamp > LatestTimestamp)
			{
				RelevantNodeMask = Query->GetParentDevice()->GetGPUMask();
				LatestTimestamp = Query->Timestamp;
			}
			else if (Query->Timestamp == LatestTimestamp)
			{
				RelevantNodeMask |= Query->GetParentDevice()->GetGPUMask();
			}			
		}

		if (LatestTimestamp == 0)
		{
			return false;
		}
	}

	bool bSuccess = false;
	OutResult = 0;
	for (uint32 GPUIndex : RelevantNodeMask)
	{
		FD3D12CommandContext& DefaultContext = Adapter.GetDevice(GPUIndex)->GetDefaultCommandContext();
		FD3D12RenderQuery* Query = DefaultContext.RetrieveObject<FD3D12RenderQuery>(QueryRHI);

		if (Query->HeapIndex == INDEX_NONE || !Query->bResolved)
		{
			// This query hasn't seen a begin/end before or hasn't been resolved.
			continue;
		}

		if (!Query->bResultIsCached)
		{
			SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);
			if (Query->GetParentDevice()->GetQueryData(*Query, bWait))
			{
				Query->bResultIsCached = true;
			}
			else
			{
				continue;
			}
		}

		if (Query->Type == RQT_AbsoluteTime)
		{
			// GetTimingFrequency is the number of ticks per second
			uint64 Div = FMath::Max(1llu, FGPUTiming::GetTimingFrequency() / (1000 * 1000));

			// convert from GPU specific timestamp to micro sec (1 / 1 000 000 s) which seems a reasonable resolution
			OutResult = FMath::Max<uint64>(Query->Result / Div, OutResult);
			bSuccess = true;
		}
		else
		{
			OutResult = FMath::Max<uint64>(Query->Result, OutResult);
			bSuccess = true;
		}
	}
	return bSuccess;
}

bool FD3D12Device::GetQueryData(FD3D12RenderQuery& Query, bool bWait)
{
	// Wait for the query result to be ready (if requested).
	const FD3D12CLSyncPoint& SyncPoint = Query.GetSyncPoint();
	if (!SyncPoint.IsComplete())
	{
		if (!bWait)
		{
			return false;
		}

		// It's reasonable to wait for things like occlusion query results. But waiting for timestamps should be avoided.
		UE_CLOG(Query.Type == RQT_AbsoluteTime, LogD3D12RHI, Verbose, TEXT("Waiting for a GPU timestamp query's result to be available. This should be avoided when possible."));

		const uint32 IdleStart = FPlatformTime::Cycles();

		if (SyncPoint.IsOpen())
		{
			// We should really try to avoid this!
			UE_LOG(LogD3D12RHI, Verbose, TEXT("Stalling the RHI thread and flushing GPU commands to wait for a RenderQuery that hasn't been submitted to the GPU yet."));

			// The query is on a command list that hasn't been submitted yet.
			// We need to flush, but the RHI thread may be using the default command list...so stall it first.
			check(IsInRenderingThread());
			FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());
			GetDefaultCommandContext().FlushCommands();	// Don't wait yet, since we're stalling the RHI thread.
		}

		SyncPoint.WaitForCompletion();

		GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
		GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
	}

	// Read the data from the query's result buffer.
	const uint32 BeginOffset = Query.HeapIndex * sizeof(Query.Result);
	const CD3DX12_RANGE ReadRange(BeginOffset, BeginOffset + sizeof(Query.Result));
	static const CD3DX12_RANGE EmptyRange(0, 0);

	{
		FD3D12Resource* const ResultBuffer = Query.Type == RQT_Occlusion ? OcclusionQueryHeap.GetResultBuffer() : TimestampQueryHeap.GetResultBuffer();
		const FD3D12ScopeMap<uint64> MappedData(ResultBuffer, 0, &ReadRange, &EmptyRange /* Not writing any data */);
		Query.Result = MappedData[Query.HeapIndex];
	}

	return true;
}

void FD3D12CommandContext::RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch)
{
	// Nothing to do here, we always start a new batch during RHIEndOcclusionQueryBatch().
}

void FD3D12CommandContext::RHIEndOcclusionQueryBatch()
{
	GetParentDevice()->GetOcclusionQueryHeap()->EndQueryBatchAndResolveQueryData(*this);

	// Note: We want to execute this ASAP. The Engine will call RHISubmitCommandHint after this.
	// We'll break up the command list there so that the wait on the previous frame's results don't block.
}

/*=============================================================================
* class FD3D12QueryHeap
*=============================================================================*/

FD3D12QueryHeap::FD3D12QueryHeap(FD3D12Device* InParent, const D3D12_QUERY_HEAP_TYPE &InQueryHeapType, uint32 InQueryHeapCount, uint32 InMaxActiveBatches)
	: FD3D12DeviceChild(InParent)
	, FD3D12SingleNodeGPUObject(InParent->GetGPUMask())
	, MaxActiveBatches(InMaxActiveBatches)
	, LastBatch(InMaxActiveBatches - 1)
	, ActiveAllocatedElementCount(0)
	, LastAllocatedElement(InQueryHeapCount - 1)
	, ResultSize(8)
	, pResultData(nullptr)
{
	if (InQueryHeapType == D3D12_QUERY_HEAP_TYPE_OCCLUSION)
	{
		QueryType = D3D12_QUERY_TYPE_OCCLUSION;
	}
	else if (InQueryHeapType == D3D12_QUERY_HEAP_TYPE_TIMESTAMP)
	{
		QueryType = D3D12_QUERY_TYPE_TIMESTAMP;
	}
	else
	{
		check(false);
	}

	// Setup the query heap desc
	QueryHeapDesc = {};
	QueryHeapDesc.Type = InQueryHeapType;
	QueryHeapDesc.Count = InQueryHeapCount;
	QueryHeapDesc.NodeMask = (uint32)GetGPUMask();

	CurrentQueryBatch.Clear();

	ActiveQueryBatches.Reserve(MaxActiveBatches);
	ActiveQueryBatches.AddZeroed(MaxActiveBatches);

	// Don't Init() until the RHI has created the device
}

FD3D12QueryHeap::~FD3D12QueryHeap()
{
	// Unmap the result buffer
	if (pResultData)
	{
		ResultBuffer->GetResource()->Unmap(0, nullptr);
		pResultData = nullptr;
	}
}

void FD3D12QueryHeap::Init()
{
	check(GetParentDevice());
	check(GetParentDevice()->GetDevice());

	// Create the query heap
	CreateQueryHeap();

	// Create the result buffer
	CreateResultBuffer();

	// Start out with an open query batch
	StartQueryBatch();
}

void FD3D12QueryHeap::Destroy()
{
	if (pResultData)
	{
		ResultBuffer->GetResource()->Unmap(0, nullptr);
		pResultData = nullptr;
	}

#if ENABLE_RESIDENCY_MANAGEMENT
	if (D3DX12Residency::IsInitialized(QueryHeapResidencyHandle))
	{
		D3DX12Residency::EndTrackingObject(GetParentDevice()->GetResidencyManager(), QueryHeapResidencyHandle);
		QueryHeapResidencyHandle = {};
	}
#endif

	QueryHeap = nullptr;
	ResultBuffer = nullptr;
}

uint32 FD3D12QueryHeap::GetNextElement(uint32 InElement)
{
	// Increment the provided element
	InElement++;

	// See if we need to wrap around to the begining of the heap
	if (InElement >= GetQueryHeapCount())
	{
		InElement = 0;
	}

	return InElement;
}

uint32 FD3D12QueryHeap::GetNextBatchElement(uint32 InBatchElement)
{
	// Increment the provided element
	InBatchElement++;

	// See if we need to wrap around to the begining of the heap
	if (InBatchElement >= MaxActiveBatches)
	{
		InBatchElement = 0;
	}

	return InBatchElement;
}

uint32 FD3D12QueryHeap::AllocQuery(FD3D12CommandContext& CmdContext)
{
	check(CmdContext.IsDefaultContext());
	check(CurrentQueryBatch.bOpen);

	// Get the element for this allocation
	const uint32 CurrentElement = GetNextElement(LastAllocatedElement);

	if (CurrentQueryBatch.StartElement > CurrentElement)
	{
		// We're in the middle of a batch, but we're at the end of the heap
		// We need to split the batch in two and resolve the first piece
		EndQueryBatchAndResolveQueryData(CmdContext);
		check(CurrentQueryBatch.bOpen && CurrentQueryBatch.ElementCount == 0);
	}

	// Increment the count for the current batch
	CurrentQueryBatch.ElementCount++;

	LastAllocatedElement = CurrentElement;
	check(CurrentElement < GetQueryHeapCount());
	return CurrentElement;
}

void FD3D12QueryHeap::StartQueryBatch()
{
	//#todo-rco: Use NumQueriesInBatch!
	static bool bWarned = false;
	if (!bWarned)
	{ 
		bWarned = true;
		UE_LOG(LogD3D12RHI, Warning, TEXT("NumQueriesInBatch is not used in FD3D12QueryHeap::StartQueryBatch(), this helpful warning exists to remind you about that. Remove it when this is fixed."));
	}

	if (!CurrentQueryBatch.bOpen)
	{
		// Clear the current batch
		CurrentQueryBatch.Clear();

		// Start a new batch
		CurrentQueryBatch.StartElement = GetNextElement(LastAllocatedElement);
		CurrentQueryBatch.bOpen = true;
	}
}

void FD3D12QueryHeap::EndQueryBatchAndResolveQueryData(FD3D12CommandContext& CmdContext)
{
	check(CmdContext.IsDefaultContext());
	check(CurrentQueryBatch.bOpen);

	// Discard empty batches
	if (CurrentQueryBatch.ElementCount == 0)
	{
		return;
	}

	// Close the current batch
	CurrentQueryBatch.bOpen = false;

	// Increment the active element count
	ActiveAllocatedElementCount += CurrentQueryBatch.ElementCount;
	checkf(ActiveAllocatedElementCount <= GetQueryHeapCount(), TEXT("The query heap is too small. Either increase the heap count (larger resource) or decrease MAX_ACTIVE_BATCHES."));

	// Track the current active batches (application is using the data)
	LastBatch = GetNextBatchElement(LastBatch);
	ActiveQueryBatches[LastBatch] = CurrentQueryBatch;

	// Update the head
	QueryBatch& OldestBatch = ActiveQueryBatches[GetNextBatchElement(LastBatch)];
	ActiveAllocatedElementCount -= OldestBatch.ElementCount;

	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->ResolveQueryData(
		QueryHeap, QueryType, CurrentQueryBatch.StartElement, CurrentQueryBatch.ElementCount,
		ResultBuffer->GetResource(), GetResultBufferOffsetForElement(CurrentQueryBatch.StartElement));

	CmdContext.CommandListHandle.UpdateResidency(&QueryHeapResidencyHandle);
	CmdContext.CommandListHandle.UpdateResidency(ResultBuffer);

	// For each render query used in this batch, update the command list
	// so we know what sync point to wait for. The query's data isn't ready to read until the above ResolveQueryData completes on the GPU.
	for (int32 i = 0; i < CurrentQueryBatch.RenderQueries.Num(); i++)
	{
		CurrentQueryBatch.RenderQueries[i]->MarkResolved(CmdContext.CommandListHandle);
	}

	// Start a new batch
	StartQueryBatch();
}

uint32 FD3D12QueryHeap::BeginQuery(FD3D12CommandContext& CmdContext)
{
	check(CmdContext.IsDefaultContext());
	check(CurrentQueryBatch.bOpen);
	const uint32 Element = AllocQuery(CmdContext);
	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->BeginQuery(QueryHeap, QueryType, Element);

	CmdContext.CommandListHandle.UpdateResidency(&QueryHeapResidencyHandle);

	return Element;
}

void FD3D12QueryHeap::EndQuery(FD3D12CommandContext& CmdContext, uint32 InElement, FD3D12RenderQuery* InRenderQuery)
{
	check(CmdContext.IsDefaultContext());
	check(CurrentQueryBatch.bOpen);
	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->EndQuery(QueryHeap, QueryType, InElement);

	CmdContext.CommandListHandle.UpdateResidency(&QueryHeapResidencyHandle);

	// Track which render queries are used in this batch.
	if (InRenderQuery)
	{
		CurrentQueryBatch.RenderQueries.Push(InRenderQuery);
	}
}

void FD3D12QueryHeap::CreateQueryHeap()
{
	// Create the upload heap
	VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(QueryHeap.GetInitReference())));
	SetName(QueryHeap, L"Query Heap");

#if ENABLE_RESIDENCY_MANAGEMENT
	D3DX12Residency::Initialize(QueryHeapResidencyHandle, QueryHeap.GetReference(), ResultSize * QueryHeapDesc.Count);
	D3DX12Residency::BeginTrackingObject(GetParentDevice()->GetResidencyManager(), QueryHeapResidencyHandle);
#endif
}

void FD3D12QueryHeap::CreateResultBuffer()
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

	const D3D12_HEAP_PROPERTIES ResultBufferHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, (uint32)GetGPUMask(), (uint32)GetVisibilityMask());
	const D3D12_RESOURCE_DESC ResultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ResultSize * QueryHeapDesc.Count); // Each query's result occupies ResultSize bytes.

	// Create the readback heap
	VERIFYD3D12RESULT(Adapter->CreateCommittedResource(
		ResultBufferDesc,
		ResultBufferHeapProperties,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		ResultBuffer.GetInitReference(),
		TEXT("Query Heap Result Buffer")));

	// Map the result buffer (and keep it mapped)
	VERIFYD3D12RESULT(ResultBuffer->GetResource()->Map(0, nullptr, &pResultData));
}

/*=============================================================================
* class FD3D12LinearQueryHeap
*=============================================================================*/

FD3D12LinearQueryHeap::FD3D12LinearQueryHeap(FD3D12Device* InParent, D3D12_QUERY_HEAP_TYPE InHeapType, int32 GrowCount)
	: FD3D12DeviceChild(InParent)
	, FD3D12SingleNodeGPUObject(InParent->GetGPUMask())
	, QueryHeapType(InHeapType)
	, QueryType(HeapTypeToQueryType(InHeapType))
	, GrowNumQueries(GrowCount)
	, SlotToHeapIdxShift(FPlatformMath::CountBits(GrowCount - 1))
	, HeapState(HS_Open)
	, NextFreeIdx(0)
	, CurMaxNumQueries(0)
	, NextChunkIdx(0)
{
	check(GrowCount > 0 && !(GrowCount & (GrowCount - 1)));
}

FD3D12LinearQueryHeap::~FD3D12LinearQueryHeap()
{
	ReleaseResources();
}

int32 FD3D12LinearQueryHeap::BeginQuery(FD3D12CommandListHandle CmdListHandle)
{
	const int32 SlotIdx = AllocateQueryHeapSlot();
	const int32 HeapIdx = SlotIdx >> SlotToHeapIdxShift;
	const int32 Offset = SlotIdx & (GrowNumQueries - 1);

	FChunk& Chunk = AllocatedChunks[HeapIdx];
	CmdListHandle->BeginQuery(Chunk.QueryHeap, QueryType, Offset);
	CmdListHandle.UpdateResidency(&Chunk.QueryHeapResidencyHandle);
	FD3D12CommandContext* Context = CmdListHandle.GetCurrentOwningContext();
	if (Context)
	{
		++Context->otherWorkCounter;
	}
	return SlotIdx;
}

int32 FD3D12LinearQueryHeap::EndQuery(FD3D12CommandListHandle CmdListHandle)
{
	const int32 SlotIdx = AllocateQueryHeapSlot();
	const int32 HeapIdx = SlotIdx >> SlotToHeapIdxShift;
	const int32 Offset = SlotIdx & (GrowNumQueries - 1);

	FChunk& Chunk = AllocatedChunks[HeapIdx];
	CmdListHandle->EndQuery(Chunk.QueryHeap, QueryType, Offset);
	CmdListHandle.UpdateResidency(&Chunk.QueryHeapResidencyHandle);
	FD3D12CommandContext* Context = CmdListHandle.GetCurrentOwningContext();
	if (Context)
	{
		++Context->otherWorkCounter;
	}
	return SlotIdx;
}

void FD3D12LinearQueryHeap::Reset()
{
	HeapState = HS_Open;
	NextFreeIdx = 0;
}

void FD3D12LinearQueryHeap::FlushAndGetResults(TArray<uint64>& QueryResults, bool bReleaseResources)
{
	HeapState = HS_Closed;
	
	int32 NumActiveQueries = NextFreeIdx;
	const uint64 ResultBuffSize = ResultSize * NumActiveQueries;
	TRefCountPtr<FD3D12Resource> ResultBuff;
	CreateResultBuffer(ResultBuffSize, ResultBuff.GetInitReference());

	FD3D12CommandContext& Context = GetParentDevice()->GetDefaultCommandContext();
	++Context.otherWorkCounter;
	const int32 NumHeaps = (NumActiveQueries + GrowNumQueries - 1) >> SlotToHeapIdxShift;
	for (int32 HeapIdx = 0; HeapIdx < NumHeaps; ++HeapIdx)
	{
		const int32 NumQueriesInHeap = FMath::Min(NumActiveQueries, GrowNumQueries);
		NumActiveQueries -= GrowNumQueries;
		FChunk& Chunk = AllocatedChunks[HeapIdx];
		Context.CommandListHandle->ResolveQueryData(
			Chunk.QueryHeap,
			QueryType,
			0,
			NumQueriesInHeap,
			ResultBuff->GetResource(),
			ResultSize * HeapIdx * GrowNumQueries);
		Context.CommandListHandle.UpdateResidency(&Chunk.QueryHeapResidencyHandle);
		Context.CommandListHandle.UpdateResidency(ResultBuff);
	}

	Context.FlushCommands(true);
	const int32 NumResults = NextFreeIdx;
	QueryResults.Empty(NumResults);
	QueryResults.AddUninitialized(NumResults);
	void* MappedResult;
	VERIFYD3D12RESULT(ResultBuff->GetResource()->Map(0, nullptr, &MappedResult));
	FMemory::Memcpy(QueryResults.GetData(), MappedResult, ResultBuffSize);
	ResultBuff->GetResource()->Unmap(0, nullptr);

	if (bReleaseResources)
	{
		ReleaseResources();
	}
	Reset();
}

D3D12_QUERY_TYPE FD3D12LinearQueryHeap::HeapTypeToQueryType(D3D12_QUERY_HEAP_TYPE HeapType)
{
	switch (HeapType)
	{
	case D3D12_QUERY_HEAP_TYPE_OCCLUSION:
		return D3D12_QUERY_TYPE_OCCLUSION;
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		return D3D12_QUERY_TYPE_TIMESTAMP;
	default:
		check(false);
		return (D3D12_QUERY_TYPE)-1;
	}
}

int32 FD3D12LinearQueryHeap::AllocateQueryHeapSlot()
{
	check(HeapState == HS_Open);
	const int32 SlotIdx = FPlatformAtomics::InterlockedIncrement(&NextFreeIdx) - 1;

	if (SlotIdx >= CurMaxNumQueries)
	{
		FScopeLock Lock(&CS);
		while (SlotIdx >= CurMaxNumQueries)
		{
			Grow();
		}
	}
	return SlotIdx;
}

void FD3D12LinearQueryHeap::Grow()
{
	const int32 ChunkIdx = NextChunkIdx++;
	checkf(ChunkIdx < MaxNumChunks, TEXT("Running out of chunks, consider increase MaxNumChunks or GrowNumQueries"));
	FChunk& NewChunk = AllocatedChunks[ChunkIdx];
	CreateQueryHeap(GrowNumQueries, NewChunk.QueryHeap.GetInitReference(), NewChunk.QueryHeapResidencyHandle);
	CurMaxNumQueries += GrowNumQueries;
}

void FD3D12LinearQueryHeap::CreateQueryHeap(int32 NumQueries, ID3D12QueryHeap** OutHeap, FD3D12ResidencyHandle& OutResidencyHandle)
{
	D3D12_QUERY_HEAP_DESC Desc;
	Desc.Type = QueryHeapType;
	Desc.Count = static_cast<uint32>(NumQueries);
	Desc.NodeMask = static_cast<uint32>(GetGPUMask());
	VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateQueryHeap(&Desc, IID_PPV_ARGS(OutHeap)));
	SetName(*OutHeap, TEXT("FD3D12LinearQueryHeap"));

#if ENABLE_RESIDENCY_MANAGEMENT
	D3DX12Residency::Initialize(OutResidencyHandle, *OutHeap, ResultSize * Desc.Count);
	D3DX12Residency::BeginTrackingObject(GetParentDevice()->GetResidencyManager(), OutResidencyHandle);
#endif
}

void FD3D12LinearQueryHeap::CreateResultBuffer(uint64 SizeInBytes, FD3D12Resource** OutBuffer)
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	const D3D12_HEAP_PROPERTIES ResultBufferHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, (uint32)GetGPUMask(), (uint32)GetVisibilityMask());
	const D3D12_RESOURCE_DESC ResultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(SizeInBytes);

	VERIFYD3D12RESULT(Adapter->CreateCommittedResource(
		ResultBufferDesc,
		ResultBufferHeapProperties,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		OutBuffer,
		TEXT("FD3D12LinearQueryHeap Result Buffer")));
}

void FD3D12LinearQueryHeap::ReleaseResources()
{
#if ENABLE_RESIDENCY_MANAGEMENT
	const int32 NumChunks = NextChunkIdx;
	for (int32 Idx = 0; Idx < NumChunks; ++Idx)
	{
		FChunk& Chunk = AllocatedChunks[Idx];
		if (D3DX12Residency::IsInitialized(Chunk.QueryHeapResidencyHandle))
		{
			D3DX12Residency::EndTrackingObject(GetParentDevice()->GetResidencyManager(), Chunk.QueryHeapResidencyHandle);
			Chunk.QueryHeapResidencyHandle = {};
		}
	}
#endif
	NextChunkIdx = 0;
	CurMaxNumQueries = 0;
}

/*=============================================================================
 * class FD3D12BufferedGPUTiming
 *=============================================================================*/

 /**
  * Constructor.
  *
  * @param InD3DRHI			RHI interface
  * @param InBufferSize		Number of buffered measurements
  */
FD3D12BufferedGPUTiming::FD3D12BufferedGPUTiming(FD3D12Adapter* InParent, int32 InBufferSize)
	: FD3D12AdapterChild(InParent)
	, BufferSize(InBufferSize)
	, CurrentTimestamp(-1)
	, NumIssuedTimestamps(0)
	, TimestampQueryHeap(nullptr)
	, TimestampQueryHeapBuffer(nullptr)
	, bIsTiming(false)
	, bStablePowerState(false)
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FD3D12BufferedGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	check(!GAreGlobalsInitialized);

	FD3D12Adapter* ParentAdapter = (FD3D12Adapter*)UserData;
	CalibrateTimers(ParentAdapter);
}

void FD3D12BufferedGPUTiming::CalibrateTimers(FD3D12Adapter* ParentAdapter)
{
	// Multi-GPU support : GPU timing only profile GPU0 currently.
	const uint32 GPUIndex = 0;

	GTimingFrequency = 0;
	VERIFYD3D12RESULT(ParentAdapter->GetDevice(GPUIndex)->GetCommandListManager().GetTimestampFrequency(&GTimingFrequency));
	GCalibrationTimestamp = ParentAdapter->GetDevice(GPUIndex)->GetCommandListManager().GetCalibrationTimestamp();
}

void FD3D12DynamicRHI::RHICalibrateTimers()
{
	check(IsInRenderingThread());

	FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());

	FD3D12Adapter& Adapter = GetAdapter();
	FD3D12BufferedGPUTiming::CalibrateTimers(&Adapter);
}

/**
 * Initializes all D3D resources and if necessary, the static variables.
 */
void FD3D12BufferedGPUTiming::InitDynamicRHI()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	ID3D12Device* D3DDevice = Adapter->GetD3DDevice();
	const FRHIGPUMask Node = FRHIGPUMask::All();

	StaticInitialize(Adapter, PlatformStaticInitialize);

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = false;

	// Now initialize the queries and backing buffers for this timing object.
	if (GIsSupported)
	{
		D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
		QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		QueryHeapDesc.Count = BufferSize * 2;	// Space for each Start + End pair.

		TimestampQueryHeap = Adapter->CreateLinkedObject<QueryHeap>(FRHIGPUMask::All(), [&] (FD3D12Device* Device)
		{
			QueryHeap* NewHeap = new QueryHeap(Device);
			QueryHeapDesc.NodeMask = (uint32)Device->GetGPUMask();
			VERIFYD3D12RESULT(D3DDevice->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(NewHeap->Heap.GetInitReference())));
			SetName(NewHeap->Heap, L"FD3D12BufferedGPUTiming: Timestamp Query Heap");

		#if ENABLE_RESIDENCY_MANAGEMENT
			D3DX12Residency::Initialize(NewHeap->ResidencyHandle, NewHeap->Heap.GetReference(), 8 * QueryHeapDesc.Count);
			D3DX12Residency::BeginTrackingObject(Adapter->GetDevice(0)->GetResidencyManager(), NewHeap->ResidencyHandle);
		#endif

			return NewHeap;
		});


		// Multi-GPU support : GPU timing only profile GPU0 currently.
		const uint64 Size = 8 * QueryHeapDesc.Count; // Each timestamp query occupies 8 bytes.
		Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, FRHIGPUMask::GPU0(), Node, D3D12_RESOURCE_STATE_COPY_DEST, Size, TimestampQueryHeapBuffer.GetInitReference(), TEXT("FD3D12BufferedGPUTiming: Timestamp Query Result Buffer"));

		TimestampListHandles.AddZeroed(QueryHeapDesc.Count);
	}
}

/**
 * Releases all D3D resources.
 */
void FD3D12BufferedGPUTiming::ReleaseDynamicRHI()
{
#if ENABLE_RESIDENCY_MANAGEMENT
	if (D3DX12Residency::IsInitialized(TimestampQueryHeap->ResidencyHandle))
	{
		D3DX12Residency::EndTrackingObject(GetParentAdapter()->GetDevice(0)->GetResidencyManager(), TimestampQueryHeap->ResidencyHandle);
	}
#endif

	delete(TimestampQueryHeap);
	TimestampQueryHeap = nullptr;
	TimestampQueryHeapBuffer = nullptr;
}

/**
 * Start a GPU timing measurement.
 */
void FD3D12BufferedGPUTiming::StartTiming()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	ID3D12Device* D3DDevice = Adapter->GetD3DDevice();

	// Issue a timestamp query for the 'start' time.
	if (GIsSupported && !bIsTiming)
	{
		// Check to see if stable power state cvar has changed
		const bool bStablePowerStateCVar = RHIConsoleVariables::bStablePowerState != 0;
		if (bStablePowerState != bStablePowerStateCVar)
		{
			if (SUCCEEDED(D3DDevice->SetStablePowerState(bStablePowerStateCVar)))
			{
				// Multi-GPU support : GPU timing only profile GPU0 currently.
				// SetStablePowerState succeeded. Update timing frequency.
				VERIFYD3D12RESULT(Adapter->GetDevice(0)->GetCommandListManager().GetTimestampFrequency(&GTimingFrequency));
				bStablePowerState = bStablePowerStateCVar;
			}
			else
			{
				// SetStablePowerState failed. This can occur if SDKLayers is not present on the system.
				RHIConsoleVariables::CVarStablePowerState->Set(0, ECVF_SetByConsole);
			}
		}

		CurrentTimestamp = (CurrentTimestamp + 1) % BufferSize;

		const uint32 QueryStartIndex = GetStartTimestampIndex(CurrentTimestamp);

		// Multi-GPU support : GPU timing only profile GPU0 currently.
		FD3D12CommandContext& CmdContext = Adapter->GetDevice(0)->GetDefaultCommandContext();

		CmdContext.otherWorkCounter++;

		QueryHeap* CurrentQH = CmdContext.RetrieveObject<QueryHeap>(TimestampQueryHeap);
		CmdContext.CommandListHandle->EndQuery(CurrentQH->Heap, D3D12_QUERY_TYPE_TIMESTAMP, QueryStartIndex);
		CmdContext.CommandListHandle.UpdateResidency(&CurrentQH->ResidencyHandle);

		TimestampListHandles[QueryStartIndex] = CmdContext.CommandListHandle;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FD3D12BufferedGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if (GIsSupported && bIsTiming)
	{
		check(CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize);
		const uint32 QueryStartIndex = GetStartTimestampIndex(CurrentTimestamp);
		const uint32 QueryEndIndex = GetEndTimestampIndex(CurrentTimestamp);
		check(QueryEndIndex == QueryStartIndex + 1);	// Make sure they're adjacent indices.

		// Multi-GPU support : GPU timing only profile GPU0 currently.
		FD3D12CommandContext& CmdContext = GetParentAdapter()->GetDevice(0)->GetDefaultCommandContext();

		CmdContext.otherWorkCounter += 2;

		QueryHeap* CurrentQH = CmdContext.RetrieveObject<QueryHeap>(TimestampQueryHeap);

		CmdContext.CommandListHandle->EndQuery(CurrentQH->Heap, D3D12_QUERY_TYPE_TIMESTAMP, QueryEndIndex);
		CmdContext.CommandListHandle->ResolveQueryData(CurrentQH->Heap, D3D12_QUERY_TYPE_TIMESTAMP, QueryStartIndex, 2, TimestampQueryHeapBuffer->GetResource(), 8 * QueryStartIndex);
		CmdContext.CommandListHandle.UpdateResidency(&CurrentQH->ResidencyHandle);
		CmdContext.CommandListHandle.UpdateResidency(TimestampQueryHeapBuffer.GetReference());

		TimestampListHandles[QueryEndIndex] = CmdContext.CommandListHandle;
		NumIssuedTimestamps = FMath::Min<int32>(NumIssuedTimestamps + 1, BufferSize);
		bIsTiming = false;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FD3D12BufferedGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	// Multi-GPU support : GPU timing only profile GPU0 currently.
	FD3D12Device* Device = GetParentAdapter()->GetDevice(0);

	if (GIsSupported)
	{
		check(CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize);
		uint64 StartTime, EndTime;
		static const CD3DX12_RANGE EmptyRange(0, 0);

		FD3D12CommandListManager& CommandListManager = Device->GetCommandListManager();

		int32 TimestampIndex = CurrentTimestamp;
		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for (int32 IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex)
			{
				const uint32 QueryStartIndex = GetStartTimestampIndex(TimestampIndex);
				const uint32 QueryEndIndex = GetEndTimestampIndex(TimestampIndex);
				const FD3D12CLSyncPoint& StartQuerySyncPoint = TimestampListHandles[QueryStartIndex];
				const FD3D12CLSyncPoint& EndQuerySyncPoint = TimestampListHandles[QueryEndIndex];
				if (EndQuerySyncPoint.IsComplete() && StartQuerySyncPoint.IsComplete())
				{
					// Scope map the result range for read.
					const CD3DX12_RANGE ReadRange(QueryStartIndex * sizeof(uint64), (QueryEndIndex + 1) * sizeof(uint64));
					const FD3D12ScopeMap<uint64> MappedTimestampData(TimestampQueryHeapBuffer, 0, &ReadRange, &EmptyRange /* Not writing any data */);
					StartTime = MappedTimestampData[QueryStartIndex];
					EndTime = MappedTimestampData[QueryEndIndex];
					
					if (EndTime > StartTime)
					{
						const uint64 Bubble = GetParentAdapter()->GetGPUProfiler().CalculateIdleTime(StartTime, EndTime);
						const uint64 ElapsedTime = EndTime - StartTime;
						return ElapsedTime >= Bubble ? ElapsedTime - Bubble : 0;
					}
				}

				TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
			}
		}

		if (NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock)
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind

			const bool bBlocking = (NumIssuedTimestamps == BufferSize) || bGetCurrentResultsAndBlock;
			const uint32 IdleStart = FPlatformTime::Cycles();

			SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);

			const uint32 QueryStartIndex = GetStartTimestampIndex(TimestampIndex);
			const uint32 QueryEndIndex = GetEndTimestampIndex(TimestampIndex);

			if (bBlocking)
			{
				const FD3D12CLSyncPoint& StartQuerySyncPoint = TimestampListHandles[QueryStartIndex];
				const FD3D12CLSyncPoint& EndQuerySyncPoint = TimestampListHandles[QueryEndIndex];
				if (EndQuerySyncPoint.IsOpen() || StartQuerySyncPoint.IsOpen())
				{
					// Need to submit the open command lists.
					Device->GetDefaultCommandContext().FlushCommands();
				}

				// CPU wait for query results to be ready.
				StartQuerySyncPoint.WaitForCompletion();
				EndQuerySyncPoint.WaitForCompletion();
			}

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			// Scope map the result range for read.
			const CD3DX12_RANGE ReadRange(QueryStartIndex * sizeof(uint64), (QueryEndIndex + 1) * sizeof(uint64));
			const FD3D12ScopeMap<uint64> MappedTimestampData(TimestampQueryHeapBuffer, 0, &ReadRange, &EmptyRange /* Not writing any data */);
			StartTime = MappedTimestampData[QueryStartIndex];
			EndTime = MappedTimestampData[QueryEndIndex];

			if (EndTime > StartTime)
			{
				const uint64 Bubble = GetParentAdapter()->GetGPUProfiler().CalculateIdleTime(StartTime, EndTime);
				const uint64 ElapsedTime = EndTime - StartTime;
				return ElapsedTime >= Bubble ? ElapsedTime - Bubble : 0;
			}
		}
	}

	return 0;
}
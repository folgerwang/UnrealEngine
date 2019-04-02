// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Query.h: Implementation of D3D12 Query
=============================================================================*/
#pragma once

/** D3D12 Render query */
class FD3D12RenderQuery : public FRHIRenderQuery, public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<FD3D12RenderQuery>
{
public:

	/** The query's index in its heap. */
	uint32 HeapIndex;

	/** The cached query result. */
	uint64 Result;

	/** True if the query's result is cached. */
	bool bResultIsCached : 1;

	/** True if the query has been resolved. */
	bool bResolved : 1;

	/** The query's type. */
	const ERenderQueryType Type;

	// A timestamp so that LDA query results only handle object from the most recent frames.
	uint64 Timestamp;

	/** Initialization constructor. */
	FD3D12RenderQuery(FD3D12Device* Parent, ERenderQueryType InQueryType) :
		FD3D12DeviceChild(Parent),
		Result(0),
		Type(InQueryType),
		Timestamp(0)
	{
		Reset();
	}

	inline void Reset()
	{
		HeapIndex = INDEX_NONE;
		bResultIsCached = false;
		bResolved = false;
	}

	// Indicate the command list that was used to resolve the query.
	inline void MarkResolved(FD3D12CommandListHandle& CommandList)
	{
		CLSyncPoint = CommandList;
		bResolved = true;
	}

	inline FD3D12CLSyncPoint& GetSyncPoint()
	{
		// Sync point is only valid if we've resolved the query.
		check(bResolved);
		return CLSyncPoint;
	}

private:
	// When the query result is ready on the GPU.
	FD3D12CLSyncPoint CLSyncPoint;
};

template<>
struct TD3D12ResourceTraits<FRHIRenderQuery>
{
	typedef FD3D12RenderQuery TConcreteType;
};

// This class handles query heaps
class FD3D12QueryHeap : public FD3D12DeviceChild, public FD3D12SingleNodeGPUObject
{
private:
	struct QueryBatch
	{
	public:
		uint32 StartElement;    // The first element in the batch (inclusive)
		uint32 ElementCount;    // The number of elements in the batch
		bool bOpen;             // Is the batch still open for more begin/end queries?
		
		// A list of all FD3D12RenderQuery objects used in the batch. 
		// This is used to set when each queries' result is ready to be read.
		TArray<FD3D12RenderQuery*> RenderQueries;

		QueryBatch()
		{
			RenderQueries.Reserve(256);
			Clear();
		}

		inline void Clear()
		{
			StartElement = 0;
			ElementCount = 0;
			bOpen = false;
			RenderQueries.Reset();
		}
	};

public:
	FD3D12QueryHeap(class FD3D12Device* InParent, const D3D12_QUERY_HEAP_TYPE& InQueryHeapType, uint32 InQueryHeapCount, uint32 InMaxActiveBatches);
	~FD3D12QueryHeap();

	void Init();

	void Destroy();

	// Stop tracking the current batch of begin/end query calls that will be resolved together.
	// This implicitly starts a new batch.
	void EndQueryBatchAndResolveQueryData(FD3D12CommandContext& CmdContext);  

	uint32 AllocQuery(FD3D12CommandContext& CmdContext); // Some query types don't need a BeginQuery call. Instead just alloc a slot to EndQuery with.
	uint32 BeginQuery(FD3D12CommandContext& CmdContext); // Obtain a query from the store of available queries
	void EndQuery(FD3D12CommandContext& CmdContext, uint32 InElement, FD3D12RenderQuery* InRenderQuery);

	uint32 GetQueryHeapCount() const { return QueryHeapDesc.Count; }
	uint32 GetResultSize() const { return ResultSize; }

	inline FD3D12Resource* GetResultBuffer() { return ResultBuffer.GetReference(); }

private:
	void StartQueryBatch(); // Start tracking a new batch of begin/end query calls that will be resolved together

	uint32 GetNextElement(uint32 InElement); // Get the next element, after the specified element. Handles overflow.

	uint32 GetNextBatchElement(uint32 InBatchElement);

	void CreateQueryHeap();
	void CreateResultBuffer();

	uint64 GetResultBufferOffsetForElement(uint32 InElement) const { return ResultSize * InElement; };

private:
	QueryBatch CurrentQueryBatch;                       // The current recording batch.

	TArray<QueryBatch> ActiveQueryBatches;              // List of active query batches. The data for these is in use.

	const uint32 MaxActiveBatches;                      // The max number of query batches that will be held.
	uint32 LastBatch;                                   // The index of the newest batch.

	uint32 ActiveAllocatedElementCount;         // The number of elements that are in use (Active). Between the head and the tail.

	uint32 LastAllocatedElement;                // The last element that was allocated for BeginQuery
	const uint32 ResultSize;                    // The byte size of a result for a single query
	D3D12_QUERY_HEAP_DESC QueryHeapDesc;        // The description of the current query heap
	D3D12_QUERY_TYPE QueryType;
	TRefCountPtr<ID3D12QueryHeap> QueryHeap;    // The query heap where all elements reside
	FD3D12ResidencyHandle QueryHeapResidencyHandle;
	TRefCountPtr<FD3D12Resource> ResultBuffer;  // The buffer where all query results are stored
	void* pResultData;
};

/**
 * A simple linear query allocator.
 * Never resolve or cleanup until results are explicitly requested.
 * Begin/EndQuery are thread-safe but other methods are not. Make sure no thread may
 * call Begin/EndQuery before calling FlushAndGetResults.
 * Only used in ProfileGPU to hold command list start/end timestamp queries currently
 */
class FD3D12LinearQueryHeap final : public FD3D12DeviceChild, public FD3D12SingleNodeGPUObject
{
public:
	enum EHeapState
	{
		HS_Open,
		HS_Closed
	};

	FD3D12LinearQueryHeap(class FD3D12Device* InParent, D3D12_QUERY_HEAP_TYPE InHeapType, int32 GrowCount);
	~FD3D12LinearQueryHeap();

	/**
	 * Allocate a slot on query heap and queue a BeginQuery command to the given list
	 * @param CmdListHandle - a handle to the command list where BeginQuery will be called
	 * @return index of the allocated query
	 */
	int32 BeginQuery(FD3D12CommandListHandle CmdListHandle);

	/**
	* Allocate a slot on query heap and queue an EndQuery command to the given list
	* @param CmdListHandle - a handle to the command list where EndQuery will be called
	* @return index of the allocated query
	*/
	int32 EndQuery(FD3D12CommandListHandle CmdListHandle);

	/** Get results of all allocated queries and reset */
	void FlushAndGetResults(TArray<uint64>& QueryResults, bool bReleaseResources = true);

private:
	struct FChunk
	{
		TRefCountPtr<ID3D12QueryHeap> QueryHeap;
		FD3D12ResidencyHandle QueryHeapResidencyHandle;
	};

	static D3D12_QUERY_TYPE HeapTypeToQueryType(D3D12_QUERY_HEAP_TYPE HeapType);

	/** Release all allocated query */
	void Reset();

	/** Returns an index to the allocated heap slot */
	int32 AllocateQueryHeapSlot();
	
	/** Grow the allocator's backing memory */
	void Grow();

	/** Helper to create a new query heap */
	void CreateQueryHeap(int32 NumQueries, ID3D12QueryHeap** OutHeap, FD3D12ResidencyHandle& OutResidencyHandle);

	/** Helper to create a readback buffer used to hold query results */
	void CreateResultBuffer(uint64 SizeInBytes, FD3D12Resource** OutBuffer);

	/** Release all allocated query heaps and detach them from residency manager */
	void ReleaseResources();

	/** This allocator can allocate up to (MaxNumChunks * GrowNumQueries) queries before a manual flush is needed */
	static constexpr int32 MaxNumChunks = 8;
	/** Size in bytes of a single query result */
	static constexpr SIZE_T ResultSize = sizeof(uint64);

	const D3D12_QUERY_HEAP_TYPE QueryHeapType;
	const D3D12_QUERY_TYPE QueryType;
	const int32 GrowNumQueries;
	const int32 SlotToHeapIdxShift;
	EHeapState HeapState;
	volatile int32 NextFreeIdx;
	volatile int32 CurMaxNumQueries;
	volatile int32 NextChunkIdx;
	FChunk AllocatedChunks[MaxNumChunks];
	FCriticalSection CS;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight multi-threaded CSV profiler which can be used for profiling in Test/Shipping builds
*/

#include "ProfilingDebugging/CsvProfiler.h"
#include "CoreGlobals.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreMisc.h"
#include "Containers/Map.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "HAL/Runnable.h"
#include "Misc/EngineVersion.h"
#include "Stats/Stats.h"

#if CSV_PROFILER

#define CSV_PROFILER_INLINE FORCEINLINE

#define REPAIR_MARKER_STACKS 1

const FString GCsvNamePrefix = CSV_STAT_NAME_PREFIX;

// Global CSV category (no prefix)
FCsvCategory GGlobalCsvCategory(TEXT("GLOBAL"), true, true);

// Basic high level perf category
CSV_DEFINE_CATEGORY_MODULE(CORE_API, Basic, true);
CSV_DEFINE_CATEGORY_MODULE(CORE_API, Exclusive, true);

// Other categories
CSV_DEFINE_CATEGORY(CsvProfiler, false);
CSV_DEFINE_CATEGORY(CsvTest, true);

CSV_DEFINE_STAT_GLOBAL(FrameTime);

#define RECORD_TIMESTAMPS 1 

#define LIST_VALIDATION (DO_CHECK && 0)

DEFINE_LOG_CATEGORY_STATIC(LogCsvProfiler, Log, All);

TUniquePtr<FCsvProfiler> FCsvProfiler::Instance;

static bool GCsvTestingGT = false;
static bool GCsvTestingRT = false;
static bool GCsvUseProcessingThread = true;
static int32 GCsvRepeatCount = 0;
static int32 GCsvRepeatFrameCount = 0;
static bool GCsvStatCounts = false;

static uint32 GCsvProcessingThreadId = 0;
static bool GGameThreadIsCsvProcessingThread = true;

static uint32 GCsvProfilerFrameNumber = 0;

static FCriticalSection GCsvProcessingLock;

//
// Categories
//
static const uint32 CSV_MAX_CATEGORY_COUNT = 2048;
static bool GCsvCategoriesEnabled[CSV_MAX_CATEGORY_COUNT];

static bool GCsvProfilerIsCapturing = false;
static bool GCsvProfilerIsCapturingRT = false; // Renderthread version of the above

class FCsvCategoryData
{
public:
	static FCsvCategoryData* Get()
	{
		if (!Instance)
		{
			Instance = new FCsvCategoryData;
			FMemory::Memzero(GCsvCategoriesEnabled, sizeof(GCsvCategoriesEnabled));
		}
		return Instance;
	}

	FString GetCategoryNameByIndex(int32 Index) const
	{
		FScopeLock Lock(&CS);
		return CategoryNames[Index];
	}

	int32 GetCategoryCount() const
	{
		return CategoryNames.Num();
	}

	int32 GetCategoryIndex(const FString& CategoryName) const
	{
		FScopeLock Lock(&CS);
		const int32* CategoryIndex = CategoryNameToIndex.Find(CategoryName.ToLower());
		if (CategoryIndex)
		{
			return *CategoryIndex;
		}
		return -1;
	}

	int32 RegisterCategory(const FString& CategoryName, bool bEnableByDefault, bool bIsGlobal)
	{
		int32 Index = -1;

		FScopeLock Lock(&CS);
		{
			Index = GetCategoryIndex(CategoryName);
			checkf(Index == -1, TEXT("CSV stat category already declared: %s. Note: Categories are not case sensitive"), *CategoryName);
			if (Index == -1)
			{
				if (bIsGlobal)
				{
					Index = 0;
				}
				else
				{
					Index = CategoryNames.Num();
					CategoryNames.AddDefaulted();
				}
				check(Index < CSV_MAX_CATEGORY_COUNT);
				if (Index < CSV_MAX_CATEGORY_COUNT)
				{
					GCsvCategoriesEnabled[Index] = bEnableByDefault;
					CategoryNames[Index] = CategoryName;
					CategoryNameToIndex.Add(CategoryName.ToLower(), Index);
				}
			}
		}
		return Index;
	}


private:
	FCsvCategoryData()
	{
		// Category 0 is reserved for the global category
		CategoryNames.AddDefaulted(1);
	}

	mutable FCriticalSection CS;
	TMap<FString, int32> CategoryNameToIndex;
	TArray<FString> CategoryNames;

	static FCsvCategoryData* Instance;
};
FCsvCategoryData* FCsvCategoryData::Instance = nullptr;


int32 FCsvProfiler::GetCategoryIndex(const FString& CategoryName)
{
	return FCsvCategoryData::Get()->GetCategoryIndex(CategoryName);
}

int32 FCsvProfiler::RegisterCategory(const FString& CategoryName, bool bEnableByDefault, bool bIsGlobal)
{
	return FCsvCategoryData::Get()->RegisterCategory(CategoryName, bEnableByDefault, bIsGlobal);
}


bool IsInCsvProcessingThread()
{
	uint32 ProcessingThreadId = GGameThreadIsCsvProcessingThread ? GGameThreadId : GCsvProcessingThreadId;
	return FPlatformTLS::GetCurrentThreadId() == ProcessingThreadId;
}

static void HandleCSVProfileCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return;
	}
	FString Param = Args[0];
	if (Param == TEXT("START"))
	{
		FCsvProfiler::Get()->BeginCapture();
	}
	else if (Param == TEXT("STOP"))
	{
		FCsvProfiler::Get()->EndCapture();
	}
	else
	{
		int32 CaptureFrames = 0;
		if (FParse::Value(*Param, TEXT("FRAMES="), CaptureFrames))
		{
			FCsvProfiler::Get()->BeginCapture(CaptureFrames);
		}
		int32 RepeatCount = 0;
		if (FParse::Value(*Param, TEXT("REPEAT="), RepeatCount))
		{
			GCsvRepeatCount = RepeatCount;
		}
	}
}

static void CsvProfilerBeginFrame()
{
	FCsvProfiler::Get()->BeginFrame();
}

static void CsvProfilerEndFrame()
{
	FCsvProfiler::Get()->EndFrame();
}

static void CsvProfilerBeginFrameRT()
{
	FCsvProfiler::Get()->BeginFrameRT();
}

static void CsvProfilerEndFrameRT()
{
	FCsvProfiler::Get()->EndFrameRT();
}


static FAutoConsoleCommand HandleCSVProfileCmd(
	TEXT("CsvProfile"),
	TEXT("Starts or stops Csv Profiles"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleCSVProfileCommand)
);

void CSVTest();

//-----------------------------------------------------------------------------
//	TSingleProducerSingleConsumerList : fast lock-free single producer/single 
//  consumer list implementation. 
//  Uses a linked list of blocks for allocations. Note that one block will always 
//  leak, because removing the tail cannot be done without locking
//-----------------------------------------------------------------------------
template <class T, int BlockSize>
class TSingleProducerSingleConsumerList
{
	// A block of BlockSize entries
	struct FBlock
	{
		FBlock() : Next(nullptr)
		{
		}
		T Entries[BlockSize];

#if LIST_VALIDATION
		int32 DebugIndices[BlockSize];
#endif
		FBlock* Next;
	};

	struct FCacheLineAlignedCounter
	{
#define NUM_PADDING_WORDS ( PLATFORM_CACHE_LINE_SIZE/4 - 2 )
		uint32 Pad[NUM_PADDING_WORDS];

		volatile uint64 Value;
	};

public:
	TSingleProducerSingleConsumerList()
	{
		HeadBlock = nullptr;
		TailBlock = nullptr;
#if DO_GUARD_SLOW
		bElementReserved = false;
#endif
#if LIST_VALIDATION
		LastDebugIndex = -1;
#endif
		Counter.Value = 0;
		ConsumerThreadLastReadIndex = 0;
		NumBlocks = 0;
	}

	// Reserve an element prior to writing it
	// Must be called from the Producer thread
	CSV_PROFILER_INLINE T* ReserveElement()
	{
#if DO_GUARD_SLOW
		checkSlow(!bElementReserved);
		bElementReserved = true;
#endif
		uint32 TailBlockSize = Counter.Value % BlockSize;
		if (TailBlockSize == 0)
		{
			AddTailBlock();
		}
#if LIST_VALIDATION
		TailBlock->DebugIndices[Counter.Value % BlockSize] = Counter.Value;
#endif
		return &TailBlock->Entries[TailBlockSize];
	}

	// Commit an element after writing it
	// Must be called from the Producer thread after a call to ReserveElement
	CSV_PROFILER_INLINE void CommitElement()
	{
#if DO_GUARD_SLOW
		checkSlow(bElementReserved);
		bElementReserved = false;
#endif
		FPlatformMisc::MemoryBarrier();

		// Keep track of the count of all the elements we ever committed. This value is never reset, even on a PopAll
		Counter.Value++;
	}

	// Called from the consumer thread
	bool HasNewData() const
	{
		volatile uint64 CurrentCounterValue = Counter.Value;
		FPlatformMisc::MemoryBarrier();
		return CurrentCounterValue > ConsumerThreadLastReadIndex;
	}

	// Called from the consumer thread
	void PopAll(TArray<T>& ElementsOut, bool bAppend = false)
	{
		volatile uint64 CurrentCounterValue = Counter.Value;
		FPlatformMisc::MemoryBarrier();

		uint32 Offset = 0;
		if (bAppend)
		{
			Offset = ElementsOut.Num();
		}

		uint32 ElementCount = uint32(CurrentCounterValue - ConsumerThreadLastReadIndex);
		ElementsOut.AddDefaulted(ElementCount);

		uint32 IndexInBlock = ConsumerThreadLastReadIndex % BlockSize;

		// Handle the edge-case where we just started a new block 
		if (IndexInBlock == 0 && ConsumerThreadLastReadIndex > 0)
		{
			IndexInBlock = BlockSize;
		}

		for (uint32 i = 0; i < ElementCount; i++)
		{
			// if this block is full and it's completed, retire it and move to the next block (update the head)
			if (IndexInBlock == BlockSize)
			{
				// Both threads are done with the head block now, so we can safely delete it 
				// Note that the Producer thread only reads/writes to the HeadBlock pointer on startup, so it's safe to update it at this point
				// HeadBlock->Next is also safe to read, since the producer can't be writing to it if Counter.Value has reached this block
				FBlock* PrevBlock = HeadBlock;
				PrevBlock = HeadBlock;
				HeadBlock = HeadBlock->Next;
				IndexInBlock = 0;
				NumBlocks--;
				delete PrevBlock;
			}
			check(HeadBlock != nullptr);
			check(IndexInBlock < BlockSize);
			ElementsOut[Offset + i] = HeadBlock->Entries[IndexInBlock];
#if LIST_VALIDATION
			int32 DebugIndex = HeadBlock->DebugIndices[IndexInBlock];
			ensure(DebugIndex == LastDebugIndex + 1);
			LastDebugIndex = DebugIndex;
#endif
			IndexInBlock++;
		}

		ConsumerThreadLastReadIndex = CurrentCounterValue;
	}
	uint64 GetAllocatedSize() const
	{
		return uint64(NumBlocks) * sizeof(FBlock) + sizeof(*this);
	}
private:
	void AddTailBlock()
	{
		FBlock* NewTail = new FBlock;
		NumBlocks++;
		if (TailBlock == nullptr)
		{
			// This must only happen on startup, otherwise it's not thread-safe
			checkSlow(Counter.Value == 0);
			checkSlow(HeadBlock == nullptr);
			HeadBlock = NewTail;
		}
		else
		{
			TailBlock->Next = NewTail;
		}
		TailBlock = NewTail;
	}


	FBlock* HeadBlock;
	FBlock* TailBlock;

	FCacheLineAlignedCounter Counter;

	// Used from the consumer thread
	uint64 ConsumerThreadLastReadIndex;

	// Just used for debugging/memory tracking
	uint32 NumBlocks;

#if DO_GUARD_SLOW
	bool bElementReserved;
#endif
#if LIST_VALIDATION
	int32 LastDebugIndex;
#endif

};

namespace ECsvTimeline
{
	enum Type
	{
		Gamethread,
		Renderthread,
		Count
	};
}

//-----------------------------------------------------------------------------
//	FFrameBoundaries : thread-safe class for managing thread boundary timestamps
//  These timestamps are written from the gamethread/renderthread, and consumed
//  by the CSVProfiling thread
//-----------------------------------------------------------------------------
class FFrameBoundaries
{
public:
	FFrameBoundaries() : CurrentReadFrameIndex(0)
	{}

	void Clear()
	{
		check(IsInCsvProcessingThread());
		Update();
		for (int i = 0; i < ECsvTimeline::Count; i++)
		{
			FrameBoundaryTimestamps[i].Empty();
		}
		CurrentReadFrameIndex = 0;
	}

	int32 GetFrameNumberForTimestamp(ECsvTimeline::Type Timeline, uint64 Timestamp) const
	{
		// If we have new frame data pending, grab it now
		if (FrameBoundaryTimestampsWriteBuffer[Timeline].HasNewData())
		{
			const_cast<FFrameBoundaries*>(this)->Update(Timeline);
		}

		const TArray<uint64>& ThreadTimestamps = FrameBoundaryTimestamps[Timeline];
		if (ThreadTimestamps.Num() == 0 || Timestamp < ThreadTimestamps[0])
		{
			// This timestamp is before the first frame, or there are no valid timestamps
			CurrentReadFrameIndex = 0;
			return -1;
		}

		if (CurrentReadFrameIndex >= ThreadTimestamps.Num())
		{
			CurrentReadFrameIndex = ThreadTimestamps.Num() - 1;
		}


		// Check if we need to rewind
		if (CurrentReadFrameIndex > 0 && ThreadTimestamps[CurrentReadFrameIndex - 1] > Timestamp)
		{
			// Binary search to < 4 and then resume linear searching
			int32 StartPos = 0;
			int32 EndPos = CurrentReadFrameIndex;
			while (true)
			{
				int32 Diff = (EndPos - StartPos);
				if (Diff <= 4)
				{
					CurrentReadFrameIndex = StartPos;
					break;
				}
				int32 MidPos = (EndPos + StartPos) / 2;
				if (ThreadTimestamps[MidPos] > Timestamp)
				{
					EndPos = MidPos;
				}
				else
				{
					StartPos = MidPos;
				}
			}
		}

		for (; CurrentReadFrameIndex < ThreadTimestamps.Num(); CurrentReadFrameIndex++)
		{
			if (Timestamp < ThreadTimestamps[CurrentReadFrameIndex])
			{
				// Might return -1 if this was before the first frame
				return CurrentReadFrameIndex - 1;
			}
		}
		return ThreadTimestamps.Num() - 1;
	}

	void AddBeginFrameTimestamp(ECsvTimeline::Type Timeline, const bool bDoThreadCheck = true)
	{
#if DO_CHECK
		if (bDoThreadCheck)
		{
			switch (Timeline)
			{
			case ECsvTimeline::Gamethread:
				check(IsInGameThread());
				break;
			case ECsvTimeline::Renderthread:
				check(IsInRenderingThread());
				break;
			}
		}
#endif
		uint64* Element = FrameBoundaryTimestampsWriteBuffer[Timeline].ReserveElement();
		*Element = FPlatformTime::Cycles64();
		FrameBoundaryTimestampsWriteBuffer[Timeline].CommitElement();
	}

private:
	void Update(ECsvTimeline::Type Timeline = ECsvTimeline::Count)
	{
		check(IsInCsvProcessingThread());
		if (Timeline == ECsvTimeline::Count)
		{
			for (int32 i = 0; i < int32(ECsvTimeline::Count); i++)
			{
				FrameBoundaryTimestampsWriteBuffer[i].PopAll(FrameBoundaryTimestamps[i], true);
			}
		}
		else
		{
			FrameBoundaryTimestampsWriteBuffer[Timeline].PopAll(FrameBoundaryTimestamps[Timeline], true);
		}
	}

	TSingleProducerSingleConsumerList<uint64, 16> FrameBoundaryTimestampsWriteBuffer[ECsvTimeline::Count];
	TArray<uint64> FrameBoundaryTimestamps[ECsvTimeline::Count];
	mutable int32 CurrentReadFrameIndex;
};
static FFrameBoundaries GFrameBoundaries;


static TMap<const ANSICHAR*, uint32> CharPtrToStringIndex;
static TMap<FString, uint32> UniqueNonFNameStatIDStrings;
static TArray<FString> UniqueNonFNameStatIDIndices;

struct FAnsiStringRegister
{
	static uint32 GetUniqueStringIndex(const ANSICHAR* AnsiStr)
	{
		uint32* IndexPtr = CharPtrToStringIndex.Find(AnsiStr);
		if (IndexPtr)
		{
			return *IndexPtr;
		}

		// If we haven't seen this pointer before, check the string register (this is slow!)
		FString Str = FString(StringCast<TCHAR>(AnsiStr).Get());
		uint32* Value = UniqueNonFNameStatIDStrings.Find(Str);
		if (Value)
		{
			// Cache in the index register
			CharPtrToStringIndex.Add(AnsiStr, *Value);
			return *Value;
		}
		// Otherwise, this string is totally new
		uint32 NewIndex = UniqueNonFNameStatIDIndices.Num();
		UniqueNonFNameStatIDStrings.Add(Str, NewIndex);
		UniqueNonFNameStatIDIndices.Add(Str);
		CharPtrToStringIndex.Add(AnsiStr, NewIndex);
		return NewIndex;
	}

	static FString GetString(uint32 Index)
	{
		return UniqueNonFNameStatIDIndices[Index];
	}
};


/**
* Key functions classes to allow SceNpOnlineId to be used as the key in TSet and TMap.
*/
struct FCsvStatID
{
	static const uint64 FNameOrIndexMask = 0x0007ffffffffffffull; // Lower 51 bits for fname or index

	FCsvStatID(uint64 InStatIDRaw, int32 InCategoryIndex, bool bInIsFName, bool bInIsCountStat)
	{
		// Get the unique index
		check(InCategoryIndex < CSV_MAX_CATEGORY_COUNT);
		if (bInIsFName)
		{
			check((InStatIDRaw & FNameOrIndexMask) == InStatIDRaw);
			UniqueID.Fields.IsFName = 1;
			UniqueID.Fields.FNameOrIndex = InStatIDRaw;
		}
		else
		{
			UniqueID.Fields.IsFName = 0;
			UniqueID.Fields.FNameOrIndex = FAnsiStringRegister::GetUniqueStringIndex((ANSICHAR*)InStatIDRaw);
		}
		UniqueID.Fields.CategoryIndex = InCategoryIndex;
		UniqueID.Fields.IsCountStat = bInIsCountStat ? 1 : 0;
	}

	FString GetNameString() const
	{
		if (UniqueID.Fields.IsFName)
		{
			const FNameEntry* NameEntry = FName::GetEntry(UniqueID.Fields.FNameOrIndex);
			return NameEntry->GetPlainNameString().RightChop(GCsvNamePrefix.Len());
		}
		else
		{
			return FAnsiStringRegister::GetString(UniqueID.Fields.FNameOrIndex);
		}
	}

	FString GetCategoryString() const
	{
		int32 CategoryIndex = UniqueID.Fields.CategoryIndex;
		check(CategoryIndex > 0);
		return FCsvCategoryData::Get()->GetCategoryNameByIndex(CategoryIndex);
	}

	int32 GetCategoryIndex() const
	{
		return UniqueID.Fields.CategoryIndex;
	}

	union FUniqueID
	{
		struct
		{
			uint64 IsFName : 1;
			uint64 IsCountStat : 1;
			uint64 CategoryIndex : 11;
			uint64 FNameOrIndex : 51;
		} Fields;
		uint64 Hash;
	} UniqueID;
};

//-----------------------------------------------------------------------------
//	FCsvTimingMarker : records timestamps. Uses StatName pointer as a unique ID
//-----------------------------------------------------------------------------
struct FCsvStatBase
{
	struct FFlags
	{
		static const uint8 StatIDIsFName = 0x01;
		static const uint8 TimestampBegin = 0x02;
		static const uint8 IsCustomStat = 0x04;
		static const uint8 IsInteger = 0x08;
	};

	CSV_PROFILER_INLINE void Init(uint64 InStatID, int32 InCategoryIndex, uint32 InFlags, uint64 InTimestamp)
	{
		Timestamp = InTimestamp;
		Flags = InFlags;
		StatID = InStatID;
		CategoryIndex = InCategoryIndex;
	}

	CSV_PROFILER_INLINE void Init(uint64 InStatID, int32 InCategoryIndex, uint8 InFlags, uint64 InTimestamp, uint8 InUserData)
	{
		Timestamp = InTimestamp;
		StatID = InStatID;
		CategoryIndex = InCategoryIndex;
		UserData = InUserData;
		Flags = InFlags;
	}

	CSV_PROFILER_INLINE uint32 GetUserData() const
	{
		return UserData;
	}

	CSV_PROFILER_INLINE uint64 GetTimestamp() const
	{
		return Timestamp;
	}

	CSV_PROFILER_INLINE FCsvStatID GetStatID() const
	{
		return FCsvStatID(StatID, CategoryIndex, !!(Flags & FFlags::StatIDIsFName), false);
	}

	CSV_PROFILER_INLINE FCsvStatID GetCountStatID() const
	{
		return FCsvStatID(StatID, CategoryIndex, !!(Flags & FFlags::StatIDIsFName), true);
	}

	CSV_PROFILER_INLINE bool IsCustomStat() const
	{
		return !!(Flags & FCsvStatBase::FFlags::IsCustomStat);
	}

	CSV_PROFILER_INLINE bool IsFNameStat() const
	{
		return !!(Flags & FCsvStatBase::FFlags::StatIDIsFName);
	}

	uint64 Timestamp;
	uint64 StatID;
	int32 CategoryIndex;

	uint8 UserData;
	uint8 Flags;
};

struct FCsvTimingMarker : public FCsvStatBase
{
	bool IsBeginMarker() const
	{
		return !!(Flags & FCsvStatBase::FFlags::TimestampBegin);
	}
};

struct FCsvCustomStat : public FCsvStatBase
{
	ECsvCustomStatOp GetCustomStatOp() const
	{
		return (ECsvCustomStatOp)GetUserData();
	}

	bool IsInteger() const
	{
		return !!(Flags & FCsvStatBase::FFlags::IsInteger);
	}

	double GetValueAsDouble() const
	{
		return IsInteger() ? double(Value.AsInt) : double(Value.AsFloat);
	}

	union FValue
	{
		float AsFloat;
		uint32 AsInt;
	} Value;
};

struct FCsvEvent
{
	CSV_PROFILER_INLINE uint64 GetAllocatedSize() const
	{
		return (uint64)EventText.GetAllocatedSize() + sizeof(*this);
	}

	FString EventText;
	uint64 Timestamp;
	uint32 CategoryIndex;
};


struct FCsvStatSeriesValue
{
	FCsvStatSeriesValue() { Value.AsInt = 0; }
	union
	{
		int32_t AsInt;
		float AsFloat;
	}Value;
};

//-----------------------------------------------------------------------------
//	FCsvStatSeries : Storage for intermediate stat values, after processing.
//  This is significantly more compact than the raw representation
//-----------------------------------------------------------------------------
struct FCsvStatSeries
{
	struct FFrameIndexSpan
	{
		int32 StartFrameIndex;
		int32 FrameCount;
		int32 StartValueIndex;
	};
	enum class EType : uint8
	{
		TimerData,
		CustomStatInt,
		CustomStatFloat
	};
	FCsvStatSeries(EType InSeriesType, const FCsvStatID& InStatID)
		: StatID(InStatID)
		, CurrentWriteFrameNumber(-1)
		, SeriesType(InSeriesType)
		, CurrentReadFrameSpanIndex(0)
		, bDirty(false)
	{
		CurrentValue.AsTimerCycles = 0;
	}

	void FlushIfDirty()
	{
		if (bDirty)
		{
			FCsvStatSeriesValue Value;
			switch (SeriesType)
			{
			case EType::TimerData:
				Value.Value.AsFloat = FPlatformTime::ToMilliseconds64(CurrentValue.AsTimerCycles);
				break;
			case EType::CustomStatInt:
				Value.Value.AsInt = CurrentValue.AsIntValue;
				break;
			case EType::CustomStatFloat:
				Value.Value.AsFloat = CurrentValue.AsFloatValue;
				break;
			}
			CommitFrameData(Value);
			bDirty = false;
		}
	}

	void SetTimerValue(uint32 DataFrameNumber, uint64 ElapsedCycles)
	{
		check(SeriesType == EType::TimerData);
		ensure(CurrentWriteFrameNumber <= DataFrameNumber || CurrentWriteFrameNumber == -1);

		// If we're done with the previous frame, commit it
		if (CurrentWriteFrameNumber != DataFrameNumber)
		{
			if (CurrentWriteFrameNumber != -1)
			{
				FlushIfDirty();
			}
			CurrentWriteFrameNumber = DataFrameNumber;
			bDirty = true;
		}
		CurrentValue.AsTimerCycles += ElapsedCycles;
	}

	void SetCustomStatValue_Int(uint32 DataFrameNumber, ECsvCustomStatOp Op, int32 Value)
	{
		check(SeriesType == EType::CustomStatInt);
		ensure(CurrentWriteFrameNumber <= DataFrameNumber || CurrentWriteFrameNumber == -1);

		// Is this a new frame?
		if (CurrentWriteFrameNumber != DataFrameNumber)
		{
			// If we're done with the previous frame, commit it
			if (CurrentWriteFrameNumber != -1)
			{
				FlushIfDirty();
			}

			// The first op in a frame is always a set. Otherwise min/max don't work
			Op = ECsvCustomStatOp::Set;
			CurrentWriteFrameNumber = DataFrameNumber;
			bDirty = true;
		}

		switch (Op)
		{
		case ECsvCustomStatOp::Set:
			CurrentValue.AsIntValue = Value;
			break;

		case ECsvCustomStatOp::Min:
			CurrentValue.AsIntValue = FMath::Min(Value, CurrentValue.AsIntValue);
			break;

		case ECsvCustomStatOp::Max:
			CurrentValue.AsIntValue = FMath::Max(Value, CurrentValue.AsIntValue);
			break;
		case ECsvCustomStatOp::Accumulate:
			CurrentValue.AsIntValue += Value;
			break;
		}
	}

	void SetCustomStatValue_Float(uint32 DataFrameNumber, ECsvCustomStatOp Op, float Value)
	{
		check(SeriesType == EType::CustomStatFloat);
		ensure(CurrentWriteFrameNumber <= DataFrameNumber || CurrentWriteFrameNumber == -1);

		// Is this a new frame?
		if (CurrentWriteFrameNumber != DataFrameNumber)
		{
			// If we're done with the previous frame, commit it
			if (CurrentWriteFrameNumber != -1)
			{
				FlushIfDirty();
			}

			// The first op in a frame is always a set. Otherwise min/max don't work
			Op = ECsvCustomStatOp::Set;
			CurrentWriteFrameNumber = DataFrameNumber;
			bDirty = true;
		}

		switch (Op)
		{
		case ECsvCustomStatOp::Set:
			CurrentValue.AsFloatValue = Value;
			break;

		case ECsvCustomStatOp::Min:
			CurrentValue.AsFloatValue = FMath::Min(Value, CurrentValue.AsFloatValue);
			break;

		case ECsvCustomStatOp::Max:
			CurrentValue.AsFloatValue = FMath::Max(Value, CurrentValue.AsFloatValue);
			break;

		case ECsvCustomStatOp::Accumulate:
			CurrentValue.AsFloatValue += Value;
			break;
		}
	}


	void CommitFrameData(const FCsvStatSeriesValue& Value)
	{
		Values.Add(Value);
		CurrentValue.AsTimerCycles = 0;
		if (FrameSpans.Num() > 0)
		{
			// Is this frame contiguous? If so, just add to the current span
			FFrameIndexSpan& LastFrameSpan = FrameSpans.Last();
			if (CurrentWriteFrameNumber == LastFrameSpan.StartFrameIndex + LastFrameSpan.FrameCount)
			{
				LastFrameSpan.FrameCount++;
				return;
			}
		}

		// Frame is not contiguous. Add a new span
		FrameSpans.AddUninitialized(1);
		FFrameIndexSpan& LastFrameSpan = FrameSpans.Last();
		LastFrameSpan.FrameCount = 1;
		LastFrameSpan.StartFrameIndex = CurrentWriteFrameNumber;
		LastFrameSpan.StartValueIndex = Values.Num() - 1;
	}

	double ReadValueForFrame(int32 FrameNumber, double DefaultValue)
	{
		check(IsInGameThread());

		if (FrameNumber >= (int32)GetFrameCount())
		{
			return DefaultValue;
		}

		// Check if we need to rewind (if the current framespan is ahead)
		if (CurrentReadFrameSpanIndex == FrameSpans.Num() || (CurrentReadFrameSpanIndex > 0 && FrameSpans[CurrentReadFrameSpanIndex].StartFrameIndex > FrameNumber))
		{
			CurrentReadFrameSpanIndex = 0;
		}

		for (; CurrentReadFrameSpanIndex < FrameSpans.Num(); CurrentReadFrameSpanIndex++)
		{
			FFrameIndexSpan& FrameSpan = FrameSpans[CurrentReadFrameSpanIndex];
			int32 FrameSpanOffset = FrameNumber - FrameSpan.StartFrameIndex;
			if (FrameSpanOffset < 0)
			{
				// We have no data for this framespan
				if (CurrentReadFrameSpanIndex > 0)
				{
					// Spin back to avoid an unnecessary rewind on the next read
					CurrentReadFrameSpanIndex--;
				}
				return DefaultValue;
			}
			if (FrameSpanOffset < FrameSpan.FrameCount)
			{
				// We're in this framespan
				int32 ValueIndex = FrameSpan.StartValueIndex + FrameSpanOffset;
				check(ValueIndex < Values.Num());
				return double((SeriesType == EType::CustomStatInt) ? Values[ValueIndex].Value.AsInt : Values[ValueIndex].Value.AsFloat);
			}
		}
		check(false);
		return DefaultValue;
	}

	uint32 GetFrameCount() const
	{
		if (FrameSpans.Num() == 0)
		{
			return 0;
		}
		return FrameSpans.Last().StartFrameIndex + FrameSpans.Last().FrameCount;
	}

	bool IsCustomStat() const
	{
		return (SeriesType == EType::CustomStatFloat || SeriesType == EType::CustomStatInt);
	}

	uint64 GetAllocatedSize() const
	{
		return sizeof(*this) + sizeof(FFrameIndexSpan) * FrameSpans.Num() + sizeof(FCsvStatSeriesValue) * Values.Num();
	}


	FCsvStatID StatID;
	uint32 CurrentWriteFrameNumber;
	union
	{
		int32_t AsIntValue;
		float   AsFloatValue;
		uint64  AsTimerCycles;
	}CurrentValue;
	EType SeriesType;

	int32 CurrentReadFrameSpanIndex;

	TArray<FFrameIndexSpan> FrameSpans;
	TArray<FCsvStatSeriesValue> Values;

	bool bDirty;
};


typedef TMap<uint64, FCsvStatSeries*> FStatIDToSeriesMap;

struct FCsvProcessedEvent
{
	FString GetFullName() const
	{
		if (CategoryIndex == 0)
		{
			return EventText;
		}
		return FCsvCategoryData::Get()->GetCategoryNameByIndex(CategoryIndex) + TEXT("/") + EventText;
	}
	FString EventText;
	uint32 FrameNumber;
	uint32 CategoryIndex;
};


class FCsvProfilerThreadData;

//-----------------------------------------------------------------------------
//	FCsvProcessedThreadData class : processed CSV data for a thread
//-----------------------------------------------------------------------------
struct FCsvProcessedThreadData
{
	friend class FCsvProfilerThreadData;

	FCsvProcessedThreadData()
		: ProcessedEventCount(0)
	{}

	void ReadStatNames(TArray<FString>& OutStatNames, int32 CategoryIndex) const
	{
		check(IsInGameThread());
		for (int i = 0; i < StatSeriesArray.Num(); i++)
		{
			FCsvStatSeries* Series = StatSeriesArray[i];

			if (Series->StatID.GetCategoryIndex() == CategoryIndex || CategoryIndex == -1)
			{
				FString Name = Series->StatID.GetNameString();

				bool bIsCountStat = Series->StatID.UniqueID.Fields.IsCountStat;

				if ( !Series->IsCustomStat() || bIsCountStat )
				{
					// Add a /<Threadname> prefix
					Name = ThreadName + TEXT("/") + Name;
				}

				if (Series->StatID.GetCategoryIndex() > 0)
				{
					// Categorised stats are prefixed with <CATEGORY>/
					Name = Series->StatID.GetCategoryString() + TEXT("/") + Name;
				}

				if (bIsCountStat)
				{
					// Add a counts prefix
					Name = TEXT("COUNTS/") + Name;
				}

				OutStatNames.Add(Name);
			}
		}
	}

	void FinalizeSeries()
	{
		check(IsInGameThread());
		for (int i = 0; i < StatSeriesArray.Num(); i++)
		{
			FCsvStatSeries* Series = StatSeriesArray[i];
			Series->FlushIfDirty();
		}
	}

	void ReadStatDataForFrame(uint32 FrameIndex, int32 CategoryIndex, TArray<double>& OutValues) const
	{
		check(IsInGameThread());
		for (int i = 0; i < StatSeriesArray.Num(); i++)
		{
			FCsvStatSeries* Series = StatSeriesArray[i];
			if (Series->StatID.GetCategoryIndex() == CategoryIndex || CategoryIndex == -1)
			{
				OutValues.Add(Series->ReadValueForFrame(FrameIndex, 0.0));
			}
		}
	}

	void ReadEventDataForFrame(uint32 FrameIndex, TArray<FString>& OutEvents) const
	{
		check(IsInGameThread());
		if (ProcessedFrameEvents.Num() > (int32)FrameIndex && ProcessedFrameEvents[FrameIndex] != nullptr)
		{
			TArray<FCsvProcessedEvent>& FrameEvents = *ProcessedFrameEvents[FrameIndex];
			for (int i = 0; i < FrameEvents.Num(); i++)
			{
				OutEvents.Add(FrameEvents[i].GetFullName());
			}
		}
	}

	void AddProcessedEvent(const FCsvProcessedEvent& Event)
	{
		check(IsInCsvProcessingThread());
		// Grow the array if it's not big enough
		if (ProcessedFrameEvents.Num() <= (int32)Event.FrameNumber)
		{
			int32 NumToAdd = (Event.FrameNumber + 1 - ProcessedFrameEvents.Num());
			ProcessedFrameEvents.AddZeroed(NumToAdd);
		}
		// Make sure we have an event TArray for this frame
		if (ProcessedFrameEvents[Event.FrameNumber] == nullptr)
		{
			ProcessedFrameEvents[Event.FrameNumber] = new TArray<FCsvProcessedEvent>();
		}
		ProcessedFrameEvents[Event.FrameNumber]->Add(Event);
		ProcessedEventCount++;
	}

	uint32 GetProcessedEventCount() const
	{
		return ProcessedEventCount;
	}

	void Clear()
	{
		check(IsInGameThread() || IsInCsvProcessingThread());

		// Clear event data
		for (int i = 0; i < ProcessedFrameEvents.Num(); i++)
		{
			if (ProcessedFrameEvents[i] != nullptr)
			{
				delete ProcessedFrameEvents[i];
			}
		}
		ProcessedFrameEvents.Empty();
		ProcessedEventCount = 0;

		// Clear stats
		for (int i = 0; i < StatSeriesArray.Num(); i++)
		{
			if (StatSeriesArray[i])
			{
				delete StatSeriesArray[i];
			}
		}

		StatSeriesArray.Empty();
		StatIDToSeries.Empty();
	}

	uint64 GetAllocatedSize() const
	{
		uint64 TotalSize = sizeof(*this);
		for (int i = 0; i < StatSeriesArray.Num(); i++)
		{
			if (StatSeriesArray[i])
			{
				TotalSize += StatSeriesArray[i]->GetAllocatedSize();
			}
		}

		TotalSize += ProcessedFrameEvents.GetAllocatedSize();
		for (int i = 0; i < ProcessedFrameEvents.Num(); i++)
		{
			if (ProcessedFrameEvents[i] != nullptr)
			{
				const TArray<FCsvProcessedEvent>& FrameEvents = *ProcessedFrameEvents[i];
				TotalSize += FrameEvents.GetAllocatedSize();
				for (int j = 0; j < FrameEvents.Num(); j++)
				{
					TotalSize += FrameEvents[j].EventText.GetAllocatedSize();
				}
			}
		}
		return TotalSize;
	}

	void SetThreadName(const FString& InThreadName)
	{
		ThreadName = InThreadName;
	}

private:
	FCsvStatSeries* FindOrCreateStatSeries(const FCsvStatID& StatID, FCsvStatSeries::EType SeriesType)
	{
		check(IsInCsvProcessingThread());
		FCsvStatSeries** Series = StatIDToSeries.Find(StatID.UniqueID.Hash);
		if (Series)
		{
#if DO_CHECK
			FString StatName = StatID.GetNameString();
			checkf(SeriesType == (*Series)->SeriesType, TEXT("Stat named %s was used in multiple stat types. Can't use same identifier for different stat types. Stat types are: Custom(Int), Custom(Float) and Timing"), *StatName);
#endif
			return *Series;
		}
		FCsvStatSeries* NewSeries = new FCsvStatSeries(SeriesType, StatID);
		NewSeries->StatID = StatID;
		StatSeriesArray.Add(NewSeries);
		StatIDToSeries.Add(StatID.UniqueID.Hash, NewSeries);
		return NewSeries;
	}

	TMap<uint64, FCsvStatSeries*> StatIDToSeries;
	TArray<FCsvStatSeries*> StatSeriesArray;

	TArray<TArray<FCsvProcessedEvent>*> ProcessedFrameEvents;
	FString ThreadName;
	uint32 ProcessedEventCount;
};


class FCsvProfilerThreadData
{
public:
	struct FProcessThreadDataStats
	{
		FProcessThreadDataStats()
			: TimestampCount(0)
			, CustomStatCount(0)
			, EventCount(0)
		{}

		uint32 TimestampCount;
		uint32 CustomStatCount;
		uint32 EventCount;
	};



	FCsvProfilerThreadData(uint32 InThreadId, uint32 InIndex)
		: ThreadId(InThreadId)
		, Index(InIndex)
		, LastProcessedTimestamp(0)
	{
		CurrentCaptureStartCycles = FPlatformTime::Cycles64();

		// Determine the thread name
		if (ThreadId == GGameThreadId)
		{
			ThreadName = TEXT("GameThread");
		}
		else if (ThreadId == GRenderThreadId)
		{
			ThreadName = TEXT("RenderThread");
		}
		else
		{
			ThreadName = FThreadManager::Get().GetThreadName(ThreadId);
		}
		ProcessedData.SetThreadName(ThreadName);
	}

	void FlushResults(TArray<FCsvTimingMarker>& OutMarkers, TArray<FCsvCustomStat>& OutCustomStats, TArray<FCsvEvent>& OutEvents)
	{
		check(IsInCsvProcessingThread());
		uint64 ValidTimestampStart = CurrentCaptureStartCycles;
		CurrentCaptureStartCycles = FPlatformTime::Cycles64();

		RawThreadData.TimingMarkers.PopAll(OutMarkers);
		RawThreadData.CustomStats.PopAll(OutCustomStats);
		RawThreadData.Events.PopAll(OutEvents);
	}

	CSV_PROFILER_INLINE void AddTimestampBegin(const char* StatName, int32 CategoryIndex)
	{
		RawThreadData.TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::TimestampBegin, FPlatformTime::Cycles64());
		RawThreadData.TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampEnd(const char* StatName, int32 CategoryIndex)
	{
		RawThreadData.TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, 0, FPlatformTime::Cycles64());
		RawThreadData.TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampExclusiveBegin(const char* StatName)
	{
		// TODO: Processing here is inefficient. Could be better done at processing time. This is 36% slower than normal timing stats
		uint64 Timestamp = FPlatformTime::Cycles64();
		if (ExclusiveMarkerStatIDStack.Num() > 0)
		{
			// End the previous marker on the stack at the same timestamp
			uint64 PrevStatID = ExclusiveMarkerStatIDStack.Last();
			RawThreadData.TimingMarkers.ReserveElement()->Init(PrevStatID, CSV_CATEGORY_INDEX(Exclusive), 0, Timestamp);
			RawThreadData.TimingMarkers.CommitElement();
		}
		RawThreadData.TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CSV_CATEGORY_INDEX(Exclusive), FCsvStatBase::FFlags::TimestampBegin, Timestamp+1);
		RawThreadData.TimingMarkers.CommitElement();
		ExclusiveMarkerStatIDStack.Push(GetStatID(StatName));
	}

	CSV_PROFILER_INLINE void AddTimestampExclusiveEnd(const char* StatName)
	{
		uint64 Timestamp = FPlatformTime::Cycles64();
		RawThreadData.TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CSV_CATEGORY_INDEX(Exclusive), 0, Timestamp);
		RawThreadData.TimingMarkers.CommitElement();
		ensure(ExclusiveMarkerStatIDStack.Num() > 0);
		if (ExclusiveMarkerStatIDStack.Num() > 0)
		{
			ExclusiveMarkerStatIDStack.Pop(false);
			// Resume the marker on the stack at the same timestamp
			uint64 PrevStatID = ExclusiveMarkerStatIDStack.Last();
			RawThreadData.TimingMarkers.ReserveElement()->Init(PrevStatID, CSV_CATEGORY_INDEX(Exclusive), FCsvStatBase::FFlags::TimestampBegin, Timestamp+1);
			RawThreadData.TimingMarkers.CommitElement();
		}
	}

	CSV_PROFILER_INLINE void AddTimestampBegin(const FName& StatName, int32 CategoryIndex)
	{
		RawThreadData.TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::StatIDIsFName | FCsvStatBase::FFlags::TimestampBegin, FPlatformTime::Cycles64());
		RawThreadData.TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampEnd(const FName& StatName, int32 CategoryIndex)
	{
		RawThreadData.TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::StatIDIsFName, FPlatformTime::Cycles64());
		RawThreadData.TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const char* StatName, const int32 CategoryIndex, const float Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = RawThreadData.CustomStats.ReserveElement();
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat, FPlatformTime::Cycles64(), uint8(CustomStatOp));
		CustomStat->Value.AsFloat = Value;
		RawThreadData.CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const FName& StatName, const int32 CategoryIndex, const float Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = RawThreadData.CustomStats.ReserveElement();
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat | FCsvStatBase::FFlags::StatIDIsFName, FPlatformTime::Cycles64(), uint8(CustomStatOp));
		CustomStat->Value.AsFloat = Value;
		RawThreadData.CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const char* StatName, const int32 CategoryIndex, const int32 Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = RawThreadData.CustomStats.ReserveElement();
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat | FCsvStatBase::FFlags::IsInteger, FPlatformTime::Cycles64(), uint8(CustomStatOp));
		CustomStat->Value.AsInt = Value;
		RawThreadData.CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const FName& StatName, const int32 CategoryIndex, const int32 Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = RawThreadData.CustomStats.ReserveElement();
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat | FCsvStatBase::FFlags::IsInteger | FCsvStatBase::FFlags::StatIDIsFName, FPlatformTime::Cycles64(), uint8(CustomStatOp));
		CustomStat->Value.AsInt = Value;
		RawThreadData.CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddEvent(const FString& EventText, const int32 CategoryIndex)
	{
		FCsvEvent* Event = RawThreadData.Events.ReserveElement();
		Event->EventText = EventText;
		Event->Timestamp = FPlatformTime::Cycles64();
		Event->CategoryIndex = CategoryIndex;
		RawThreadData.Events.CommitElement();
	}

	CSV_PROFILER_INLINE void AddEventWithTimestamp(const FString& EventText, const int32 CategoryIndex, const uint64 Timestamp)
	{
		FCsvEvent* Event = RawThreadData.Events.ReserveElement();
		Event->EventText = EventText;
		Event->Timestamp = Timestamp;
		Event->CategoryIndex = CategoryIndex;
		RawThreadData.Events.CommitElement();
	}


	uint64 GetAllocatedSize() const
	{
		uint64 TotalSize = RawThreadData.TimingMarkers.GetAllocatedSize() + RawThreadData.CustomStats.GetAllocatedSize() + RawThreadData.Events.GetAllocatedSize() + sizeof(*this);
		TotalSize += ProcessedData.GetAllocatedSize();
		return TotalSize;
	}

	void ProcessThreadData(FProcessThreadDataStats* StatsInOut = nullptr);

	FString GetThreadName() const
	{
		return ThreadName;
	}

	CSV_PROFILER_INLINE uint32 GetThreadID() const
	{
		return ThreadId;
	}


	const FCsvProcessedThreadData* GetProcessedData()
	{
		check(IsInGameThread());
		// Do a final process the thread data before returning it
		ProcessThreadData();
		ProcessedData.FinalizeSeries();
		return &ProcessedData;
	}

	void ClearProcessedData()
	{
		check(IsInGameThread());
		MarkerStack.Empty();
		LastProcessedTimestamp = 0;
		ProcessedData.Clear();
	}

private:
	CSV_PROFILER_INLINE uint64 GetStatID(const char* StatName)
	{
		return uint64(StatName);
	}

	CSV_PROFILER_INLINE uint64 GetStatID(const FName& StatId)
	{
		return uint64(StatId.GetComparisonIndex());
	}

	uint32 ThreadId;
	uint32 Index;
	uint64 CurrentCaptureStartCycles;
	FString ThreadName;

	uint64 LastProcessedTimestamp;

	TArray<FCsvTimingMarker> MarkerStack;
	TArray<uint64> ExclusiveMarkerStatIDStack;

	// Raw stat data (written from the thread)
	struct
	{
		TSingleProducerSingleConsumerList<FCsvTimingMarker, 256> TimingMarkers;
		TSingleProducerSingleConsumerList<FCsvCustomStat, 256> CustomStats;
		TSingleProducerSingleConsumerList<FCsvEvent, 32> Events;
	} RawThreadData;

	// Processed stat data
	FCsvProcessedThreadData ProcessedData;
};

//-----------------------------------------------------------------------------
//	FCsvProfilerThreadDataTls - manages thread-local data
//-----------------------------------------------------------------------------
class FCsvProfilerThreadDataTls
{
public:
	void GetThreadDataArray(TArray<FCsvProfilerThreadData*>& OutProfilerThreadDataArray)
	{
		FScopeLock Lock(&ProfilerThreadDataArrayLock);
		OutProfilerThreadDataArray.Empty(ProfilerThreadDataArray.Num());
		for (int i = 0; i < ProfilerThreadDataArray.Num(); i++)
		{
			OutProfilerThreadDataArray.Add(ProfilerThreadDataArray[i]);
		}
	}

	// Create the TLS profiler thread lazily
	CSV_PROFILER_INLINE FCsvProfilerThreadData* GetThreadData()
	{
		static uint32 TlsSlot = FPlatformTLS::AllocTlsSlot();
		FCsvProfilerThreadData* ProfilerThread = (FCsvProfilerThreadData*)FPlatformTLS::GetTlsValue(TlsSlot);
		if (!ProfilerThread)
		{
			ProfilerThread = new FCsvProfilerThreadData(FPlatformTLS::GetCurrentThreadId(), ProfilerThreadDataArray.Num());
			FPlatformTLS::SetTlsValue(TlsSlot, ProfilerThread);
			{
				FScopeLock Lock(&ProfilerThreadDataArrayLock);
				ProfilerThreadDataArray.Add(ProfilerThread);
			}
		}
		return ProfilerThread;
	}

private:
	// Can be written from any thread - protected by ProfilerThreadDataArrayLock
	TArray<FCsvProfilerThreadData*> ProfilerThreadDataArray;
	FCriticalSection ProfilerThreadDataArrayLock;
};
FCsvProfilerThreadDataTls GCsvProfilerThreadDataTls;


//-----------------------------------------------------------------------------
//	FCsvProfilerProcessingThread class : low priority thread to process 
//  profiling data
//-----------------------------------------------------------------------------
class FCsvProfilerProcessingThread : public FRunnable
{
	FThreadSafeCounter StopCounter;
public:
	FCsvProfilerProcessingThread(FCsvProfiler* InCsvProfiler)
		: CsvProfiler(InCsvProfiler)
	{
		Thread = FRunnableThread::Create(this, TEXT("CSVProfiler"), 0, TPri_Lowest, FPlatformAffinity::GetTaskGraphBackgroundTaskMask());
	}

	virtual ~FCsvProfilerProcessingThread()
	{
		if (Thread)
		{
			Thread->Kill(true);
			delete Thread;
			Thread = nullptr;
		}
	}

	// FRunnable interface
	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		const float TimeBetweenUpdatesMS = 50.0f;
		GCsvProcessingThreadId = FPlatformTLS::GetCurrentThreadId();
		GGameThreadIsCsvProcessingThread = false;

		while (StopCounter.GetValue() == 0)
		{
			check(CsvProfiler);
			GCsvProcessingLock.Lock();
			float ElapsedMS = CsvProfiler->ProcessStatData();
			GCsvProcessingLock.Unlock();
			float SleepTimeSeconds = FMath::Max(TimeBetweenUpdatesMS - ElapsedMS, 0.0f) / 1000.0f;
			FPlatformProcess::Sleep(SleepTimeSeconds);
		}

		return 0;
	}

	virtual void Stop() override
	{
		StopCounter.Increment();
	}

	virtual void Exit() override { }

private:
	FRunnableThread* Thread;
	FCsvProfiler* CsvProfiler;
};

void FCsvProfilerThreadData::ProcessThreadData(FCsvProfilerThreadData::FProcessThreadDataStats* StatsInOut)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfilerThreadData_ProcessThreadData);

	// We can call this from the game thread just before reading back the data, or from the CSV processing thread
	check(IsInCsvProcessingThread());

	// Read the raw CSV data
	TArray<FCsvTimingMarker> ThreadMarkers;
	TArray<FCsvCustomStat> CustomStats;
	TArray<FCsvEvent> Events;
	FlushResults(ThreadMarkers, CustomStats, Events);

	if (StatsInOut)
	{
		StatsInOut->TimestampCount += ThreadMarkers.Num();
		StatsInOut->CustomStatCount += CustomStats.Num();
		StatsInOut->EventCount += Events.Num();
	}

	// Flush the frame boundaries after the stat data. This way, we ensure the frame boundary data is up to date
	// (we do not want to encounter markers from a frame which hasn't been registered yet)
	FPlatformMisc::MemoryBarrier();
	ECsvTimeline::Type Timeline = (ThreadId == GRenderThreadId || ThreadId == GRHIThreadId) ? ECsvTimeline::Renderthread : ECsvTimeline::Gamethread;

	if (ThreadMarkers.Num() > 0)
	{
		ensure(ThreadMarkers[0].GetTimestamp() >= LastProcessedTimestamp);
		LastProcessedTimestamp = ThreadMarkers.Last().GetTimestamp();
	}

	// Process the markers
	for (int i = 0; i < ThreadMarkers.Num(); i++)
	{
		FCsvTimingMarker& Marker = ThreadMarkers[i];
		int32 FrameNumber = GFrameBoundaries.GetFrameNumberForTimestamp(Timeline, Marker.GetTimestamp());
		if (Marker.IsBeginMarker())
		{
			MarkerStack.Push(Marker);
		}
		else
		{
			// Markers might not match up if they were truncated mid-frame, so we need to be robust to that
			if (MarkerStack.Num() > 0)
			{
				// Find the start marker (might not actually be top of the stack, e.g if begin/end for two overlapping stats are independent)
				bool bFoundStart = false;
#if REPAIR_MARKER_STACKS
				FCsvTimingMarker StartMarker;
				// Prevent spurious MSVC warning about this being used uninitialized further down. Alternative is to implement a ctor, but that would add overhead
				StartMarker.Init(0, 0, 0, 0);

				for (int j = MarkerStack.Num() - 1; j >= 0; j--)
				{
					if (MarkerStack[j].StatID == Marker.StatID)
					{
						StartMarker = MarkerStack[j];
						MarkerStack.RemoveAt(j);
						bFoundStart = true;
						break; 
					}
				}
#else
				FCsvTimingMarker StartMarker = MarkerStack.Pop();
				bFoundStart = true;
#endif
				// TODO: if bFoundStart is false, this stat _never_ gets processed. Could we add it to a persistent list so it's considered next time?
				// Example where this could go wrong: staggered/overlapping exclusive stats ( e.g Abegin, Bbegin, AEnd, BEnd ), where processing ends after AEnd
				// AEnd would be missing 
				if (FrameNumber >= 0 && bFoundStart)
				{
					ensure(Marker.StatID == StartMarker.StatID);
					ensure(Marker.GetTimestamp() >= StartMarker.GetTimestamp());
					if (Marker.GetTimestamp() > StartMarker.GetTimestamp())
					{
						uint64 ElapsedCycles = Marker.GetTimestamp() - StartMarker.GetTimestamp();

						// Add the elapsed time to the table entry for this frame/stat 
						FCsvStatSeries* Series = ProcessedData.FindOrCreateStatSeries(Marker.GetStatID(), FCsvStatSeries::EType::TimerData);
						Series->SetTimerValue(FrameNumber, ElapsedCycles);

						// Add the COUNT/ series if enabled
						if (GCsvStatCounts)
						{
							FCsvStatSeries* CountSeries = ProcessedData.FindOrCreateStatSeries(Marker.GetCountStatID(), FCsvStatSeries::EType::CustomStatInt);
							CountSeries->SetCustomStatValue_Int(FrameNumber, ECsvCustomStatOp::Accumulate, 1);
						}
					}
				}
			}
		}
	}

	// Process the custom stats
	for (int i = 0; i < CustomStats.Num(); i++)
	{
		FCsvCustomStat& CustomStat = CustomStats[i];
		int32 FrameNumber = GFrameBoundaries.GetFrameNumberForTimestamp(Timeline, CustomStat.GetTimestamp());
		if (FrameNumber >= 0)
		{
			bool bIsInteger = CustomStat.IsInteger();
			FCsvStatSeries* Series = ProcessedData.FindOrCreateStatSeries(CustomStat.GetStatID(), bIsInteger ? FCsvStatSeries::EType::CustomStatInt : FCsvStatSeries::EType::CustomStatFloat);
			if (bIsInteger)
			{
				Series->SetCustomStatValue_Int(FrameNumber, CustomStat.GetCustomStatOp(), CustomStat.Value.AsInt);
			}
			else
			{
				Series->SetCustomStatValue_Float(FrameNumber, CustomStat.GetCustomStatOp(), CustomStat.Value.AsFloat);
			}

			// Add the COUNT/ series if enabled
			if (GCsvStatCounts)
			{
				FCsvStatSeries* CountSeries = ProcessedData.FindOrCreateStatSeries(CustomStat.GetCountStatID(), FCsvStatSeries::EType::CustomStatInt);
				CountSeries->SetCustomStatValue_Int(FrameNumber, ECsvCustomStatOp::Accumulate, 1);
			}
		}
	}

	// Process Events
	for (int i = 0; i < Events.Num(); i++)
	{
		FCsvEvent& Event = Events[i];
		int32 FrameNumber = GFrameBoundaries.GetFrameNumberForTimestamp(Timeline, Event.Timestamp);
		if (FrameNumber >= 0)
		{
			FCsvProcessedEvent ProcessedEvent;
			ProcessedEvent.EventText = Event.EventText;
			ProcessedEvent.FrameNumber = FrameNumber;
			ProcessedEvent.CategoryIndex = Event.CategoryIndex;
			ProcessedData.AddProcessedEvent(ProcessedEvent);
		}
	}
}


FCsvProfiler* FCsvProfiler::Get()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FCsvProfiler>();
	}
	return Instance.Get();
}

FCsvProfiler::FCsvProfiler()
	: NumFramesToCapture(-1)
	, CaptureFrameNumber(0)
	, bInsertEndFrameAtFrameStart(false)
	, LastEndFrameTimestamp(0)
	, CaptureEndFrameCount(0)
	, ProcessingThread(nullptr)
{
	check(IsInGameThread());
	GCsvProfilerThreadDataTls.GetThreadData();

	FCoreDelegates::OnBeginFrame.AddStatic(CsvProfilerBeginFrame);
	FCoreDelegates::OnEndFrame.AddStatic(CsvProfilerEndFrame);
	FCoreDelegates::OnBeginFrameRT.AddStatic(CsvProfilerBeginFrameRT);
	FCoreDelegates::OnEndFrameRT.AddStatic(CsvProfilerEndFrameRT);
}

FCsvProfiler::~FCsvProfiler()
{
	GCsvProfilerIsCapturing = false;
	IsShuttingDown.Increment();
	if (ProcessingThread)
	{
		delete ProcessingThread;
		ProcessingThread = nullptr;
	}
}

/** Per-frame update */
void FCsvProfiler::BeginFrame()
{
	check(IsInGameThread());

	if (bInsertEndFrameAtFrameStart)
	{
		bInsertEndFrameAtFrameStart = false;
		EndFrame();
	}

	// Process the command queue for start commands
	FCsvCaptureCommand CurrentCommand;
	if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Start)
	{
		CommandQueue.Dequeue(CurrentCommand);
		if (GCsvProfilerIsCapturing)
		{
			UE_LOG(LogCsvProfiler, Warning, TEXT("Capture start requested, but a capture was already running"));
		}
		else
		{
			UE_LOG(LogCsvProfiler, Display, TEXT("Capture Starting"));
			GCsvProfilerIsCapturing = true;
			NumFramesToCapture = CurrentCommand.Value;
			GCsvRepeatFrameCount = NumFramesToCapture;
			CaptureFrameNumber = 0;
			LastEndFrameTimestamp = FPlatformTime::Cycles64();

			// Determine the output path and filename based on override params
			FString DestinationFolder = CurrentCommand.DestinationFolder.IsEmpty() ? FPaths::ProfilingDir() + TEXT("CSV/") : CurrentCommand.DestinationFolder + TEXT("/");
			FString Filename = CurrentCommand.Filename.IsEmpty() ? FString::Printf(TEXT("Profile(%s).csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))) : CurrentCommand.Filename;
			OutputFilename = DestinationFolder + Filename;
			CustomMetadata = CurrentCommand.CustomMetadata;

			bWriteCompletionFile = CurrentCommand.bWriteCompletionFile;
			if (GCsvUseProcessingThread && ProcessingThread == nullptr)
			{
				// Lazily create the CSV processing thread
				ProcessingThread = new FCsvProfilerProcessingThread(this);
			}
		}
	}

	if (GCsvProfilerIsCapturing)
	{
		GFrameBoundaries.AddBeginFrameTimestamp(ECsvTimeline::Gamethread);
	}

	if (GCsvTestingGT)
	{
		CSVTest();
	}
}

void FCsvProfiler::EndFrame()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_EndFrame);

	check(IsInGameThread());
	if (GCsvProfilerIsCapturing)
	{
		if (NumFramesToCapture >= 0)
		{
			NumFramesToCapture--;
			if (NumFramesToCapture == 0)
			{
				EndCapture();
			}
		}

		// Record the frametime (measured since the last EndFrame)
		uint64 CurrentTimeStamp = FPlatformTime::Cycles64();
		uint64 ElapsedCycles = CurrentTimeStamp - LastEndFrameTimestamp;
		float ElapsedMs = FPlatformTime::ToMilliseconds64(ElapsedCycles);
		CSV_CUSTOM_STAT_DEFINED(FrameTime, ElapsedMs, ECsvCustomStatOp::Set);

		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		float PhysicalMBFree = float(MemoryStats.AvailablePhysical / 1024) / 1024.0f;
		float PhysicalMBUsed = float(MemoryStats.UsedPhysical / 1024) / 1024.0f;
		float VirtuallMBUsed = float(MemoryStats.UsedVirtual / 1024) / 1024.0f;
		CSV_CUSTOM_STAT_GLOBAL(MemoryFreeMB, PhysicalMBFree, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(PhysicalUsedMB, PhysicalMBUsed, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(VirtualUsedMB, VirtuallMBUsed, ECsvCustomStatOp::Set);

		// If we're single-threaded, process the stat data here
		if (ProcessingThread == nullptr)
		{
			ProcessStatData();
		}

		LastEndFrameTimestamp = CurrentTimeStamp;
		CaptureFrameNumber++;
	}

	// Process the command queue for stop commands
	FCsvCaptureCommand CurrentCommand;
	if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Stop)
	{
		// Delay end capture by a frame to allow RT stats to catch up
		if (CurrentCommand.FrameRequested == GCsvProfilerFrameNumber)
		{
			CaptureEndFrameCount = CaptureFrameNumber;
		}
		else
		{
			CommandQueue.Dequeue(CurrentCommand);
			if (GCsvProfilerIsCapturing)
			{
				UE_LOG(LogCsvProfiler, Display, TEXT("Capture Ending"));
				GCsvProfilerIsCapturing = false;
				WriteCaptureToFile();

				// Handle repeats
				if (GCsvRepeatCount != 0 && GCsvRepeatFrameCount > 0)
				{
					if (GCsvRepeatCount > 0)
					{
						GCsvRepeatCount--;
					}
					if (GCsvRepeatCount != 0)
					{
						// TODO: support directories
						BeginCapture(GCsvRepeatFrameCount);
					}
				}
			}
			else
			{
				UE_LOG(LogCsvProfiler, Warning, TEXT("Capture Stop requested, but no capture was running!"));
			}
		}
	}

	GCsvProfilerFrameNumber++;
}


/** Per-frame update */
void FCsvProfiler::BeginFrameRT()
{
	check(IsInRenderingThread());
	if (GCsvProfilerIsCapturing)
	{
		// Mark where the renderthread frames begin
		GFrameBoundaries.AddBeginFrameTimestamp(ECsvTimeline::Renderthread);
	}
	GCsvProfilerIsCapturingRT = GCsvProfilerIsCapturing;

	if (GCsvTestingRT)
	{
		CSVTest();
	}
}

void FCsvProfiler::EndFrameRT()
{
	check(IsInRenderingThread());
}


/** Final cleanup */
void FCsvProfiler::Release()
{

}

void FCsvProfiler::BeginCapture(int InNumFramesToCapture, 
	const FString& InDestinationFolder, 
	const FString& InFilename,
	const FString& InCustomMetadata,
	bool bInWriteCompletionFile )
{
	check(IsInGameThread());
	CommandQueue.Enqueue(FCsvCaptureCommand(ECsvCommandType::Start, GCsvProfilerFrameNumber, InNumFramesToCapture, InDestinationFolder, InFilename, InCustomMetadata, bInWriteCompletionFile));
}

void FCsvProfiler::EndCapture()
{
	check(IsInGameThread());
	CommandQueue.Enqueue(FCsvCaptureCommand(ECsvCommandType::Stop, GCsvProfilerFrameNumber));
}



class FCsvWriterHelper
{
public:
	FCsvWriterHelper(FArchive* InOutputFile) : OutputFile(InOutputFile), bIsLineStart(true)
	{}

	void WriteStringList(const TArray<FString>& Strings, FString Prefix)
	{
		for (int i = 0; i < Strings.Num(); i++)
		{
			WriteString(Prefix);
			WriteStringInternal(Strings[i]);
		}
	}
	void WriteValues(const TArray<double>& Values)
	{
		for (int i = 0; i < Values.Num(); i++)
		{
			WriteValue(Values[i]);
		}
	}

	void WriteSemicolonSeparatedStringList(const TArray<FString>& Strings)
	{
		WriteString(TEXT(""));
		for (int i = 0; i < Strings.Num(); i++)
		{
			// Remove semicolons from the event text so we can safely separate using them
			FString SanitizedText = Strings[i].Replace(TEXT(";"), TEXT("."));
			if (i > 0)
			{
				WriteChar(';');
			}
			WriteStringInternal(SanitizedText);
		}
	}

	void NewLine()
	{
		WriteChar('\n');
		bIsLineStart = true;
	}

	void WriteString(const FString& Str)
	{
		if (!bIsLineStart)
		{
			WriteChar(',');
		}
		bIsLineStart = false;
		WriteStringInternal(Str);
	}

	void WriteValue(double Value)
	{
		if (!bIsLineStart)
		{
			WriteChar(',');
		}
		bIsLineStart = false;

		ANSICHAR StringBuffer[256];
		if (FMath::Frac(Value) == 0.0)
		{
			FCStringAnsi::Snprintf(StringBuffer, 256, "%d", int(Value));
		}
		else if (FMath::Abs(Value) < 0.1)
		{
			FCStringAnsi::Snprintf(StringBuffer, 256, "%.6f", Value);
		}
		else
		{
			FCStringAnsi::Snprintf(StringBuffer, 256, "%.4f", Value);
		}
		OutputFile->Serialize((void*)StringBuffer, sizeof(ANSICHAR)*FCStringAnsi::Strlen(StringBuffer));
	}

private:
	void WriteStringInternal(const FString& Str)
	{
		auto AnsiStr = StringCast<ANSICHAR>(*Str);
		OutputFile->Serialize((void*)AnsiStr.Get(), AnsiStr.Length());
	}

	void WriteChar(ANSICHAR Char)
	{
		OutputFile->Serialize((void*)&Char, sizeof(ANSICHAR));
	}

	FArchive* OutputFile;
	bool bIsLineStart;
};


void FCsvProfiler::WriteCaptureToFile()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_WriteCaptureToFile);
	check(IsInGameThread());
	FScopeLock Lock(&GCsvProcessingLock);
	if (ProcessingThread)
	{
		GGameThreadIsCsvProcessingThread = true;
	}

	double ProcessStatsStartTime = FPlatformTime::Seconds();

	// Do a final process of the stat data
	ProcessStatData();

	// Read back the processed data for each thread
	TArray<const FCsvProcessedThreadData*> ProcessedThreadDataArray;
	TArray<FCsvProfilerThreadData*> ProfilerThreadData;
	GCsvProfilerThreadDataTls.GetThreadDataArray(ProfilerThreadData);
	for (int t = 0; t < ProfilerThreadData.Num(); t++)
	{
		ProcessedThreadDataArray.Add(ProfilerThreadData[t]->GetProcessedData());
	}

	double WriteStartTime = FPlatformTime::Seconds();

#if ALLOW_DEBUG_FILES
	FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*OutputFilename);
#else
	FArchive* OutputFile = IFileManager::Get().CreateFileWriter(*OutputFilename);
#endif

	check(OutputFile != nullptr);
	if (!OutputFile)
	{
		UE_LOG(LogCsvProfiler, Warning, TEXT("Error writing CSV file : %s"), *OutputFilename);
	}
	else
	{
		FCsvWriterHelper CsvWriter(OutputFile);

		// Write the first row (ie the header)
		bool bHasEvents = false;
		for (int32 CategoryIndex = 0; CategoryIndex < FCsvCategoryData::Get()->GetCategoryCount(); CategoryIndex++)
		{
			for (int32 t = 0; t < ProcessedThreadDataArray.Num(); t++)
			{
				const FCsvProcessedThreadData* ProcessedData = ProcessedThreadDataArray[t];
				TArray<FString> StatNames;

				// Read Custom stat names, write out with no prefix
				ProcessedData->ReadStatNames(StatNames, CategoryIndex);
				CsvWriter.WriteStringList(StatNames, TEXT(""));
				StatNames.Empty();
				if (ProcessedData->GetProcessedEventCount() > 0)
				{
					bHasEvents = true;
				}
			}
		}
		if (bHasEvents)
		{
			CsvWriter.WriteString(TEXT("EVENTS"));
		}
		CsvWriter.NewLine();

		// Write out the values
		for (int32 i = 0; i < (int32)CaptureEndFrameCount; i++)
		{
			for (int32 CategoryIndex = 0; CategoryIndex < FCsvCategoryData::Get()->GetCategoryCount(); CategoryIndex++)
			{
				TArray<FString> RowEvents;
				for (int t = 0; t < ProcessedThreadDataArray.Num(); t++)
				{
					const FCsvProcessedThreadData* ProcessedData = ProcessedThreadDataArray[t];
					TArray<double> ThreadValues;

					// Write stat values for this frame/category
					ProcessedData->ReadStatDataForFrame(i, CategoryIndex, ThreadValues);
					CsvWriter.WriteValues(ThreadValues);
				}
			}
			if (bHasEvents)
			{
				TArray<FString> RowEvents;
				for (int t = 0; t < ProcessedThreadDataArray.Num(); t++)
				{
					ProcessedThreadDataArray[t]->ReadEventDataForFrame(i, RowEvents);
				}
				CsvWriter.WriteSemicolonSeparatedStringList(RowEvents);
			}
			CsvWriter.NewLine();
		}

		// Add metadata
		FString PlatformStr = FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));
		FString BuildConfigurationStr = EBuildConfigurations::ToString(FApp::GetBuildConfiguration());
		FString CommandlineStr = FString("\"") + FCommandLine::Get() + FString("\"");
		// Strip newlines
		CommandlineStr.ReplaceInline(TEXT("\n"), TEXT(""));
		CommandlineStr.ReplaceInline(TEXT("\r"), TEXT(""));
		FString BuildVersionString = FApp::GetBuildVersion();
		FString EngineVersionString = FEngineVersion::Current().ToString();

		CsvWriter.WriteString(TEXT("[Platform]"));
		CsvWriter.WriteString(PlatformStr);
		CsvWriter.WriteString(TEXT("[Config]"));
		CsvWriter.WriteString(BuildConfigurationStr);
		CsvWriter.WriteString(TEXT("[DeviceProfile]"));
		CsvWriter.WriteString(DeviceProfileName);
		CsvWriter.WriteString(TEXT("[BuildVersion]"));
		CsvWriter.WriteString(BuildVersionString);
		CsvWriter.WriteString(TEXT("[EngineVersion]"));
		CsvWriter.WriteString(EngineVersionString);
		if (!CustomMetadata.IsEmpty())
		{
			CsvWriter.WriteString(CustomMetadata);
		}
		CsvWriter.WriteString(TEXT("[Commandline]"));
		CsvWriter.WriteString(CommandlineStr);

		OutputFile->Close();
	}

	// Clear the processed data now we're done with it
	uint64 PeakMemoryBytes = 0;
	uint64 BytesAllocatedAfterProcessing = 0;
	for (int t = 0; t < ProfilerThreadData.Num(); t++)
	{
		PeakMemoryBytes += ProfilerThreadData[t]->GetAllocatedSize();
		ProfilerThreadData[t]->ClearProcessedData();
	}

	GFrameBoundaries.Clear();
	UE_LOG(LogCsvProfiler, Display, TEXT("Capture Ended. Writing CSV to file : %s"), *OutputFilename);
	UE_LOG(LogCsvProfiler, Display, TEXT("  Frames : %d"), CaptureEndFrameCount);
	UE_LOG(LogCsvProfiler, Display, TEXT("  Peak memory usage  : %.2fMB"), float(PeakMemoryBytes) / 1024.0f / 1024.0f);

	if (bWriteCompletionFile)
	{
		FString CompletionFilename = OutputFilename + TEXT(".complete");
#if ALLOW_DEBUG_FILES
		IFileManager::Get().CreateDebugFileWriter(*CompletionFilename);
#else
		IFileManager::Get().CreateFileWriter(*CompletionFilename);
#endif
	}

	float ProcessStatsDuration = float(WriteStartTime - ProcessStatsStartTime);
	float WriteDuration = float( FPlatformTime::Seconds() - WriteStartTime );
	UE_LOG(LogCsvProfiler, Display, TEXT("  Final stat processing time : %.3f seconds"), ProcessStatsDuration );
	UE_LOG(LogCsvProfiler, Display, TEXT("  File IO time : %.3f seconds"), WriteDuration);

	if (ProcessingThread)
	{
		GGameThreadIsCsvProcessingThread = false;
	}
}

void FCsvProfiler::SetDeviceProfileName(FString InDeviceProfileName)
{
	DeviceProfileName = InDeviceProfileName;
}

/** Push/pop events */
void FCsvProfiler::BeginStat(const char * StatName, uint32 CategoryIndex)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddTimestampBegin(StatName, CategoryIndex);
	}
#endif
}

void FCsvProfiler::EndStat(const char * StatName, uint32 CategoryIndex)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddTimestampEnd(StatName, CategoryIndex);
	}
#endif
}

void FCsvProfiler::BeginExclusiveStat(const char * StatName)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddTimestampExclusiveBegin(StatName);
	}
#endif
}

void FCsvProfiler::EndExclusiveStat(const char * StatName)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddTimestampExclusiveEnd(StatName);
	}
#endif
}

void FCsvProfiler::RecordEventfInternal(int32 CategoryIndex, const TCHAR* Fmt, ...)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		TCHAR Buffer[256];
		GET_VARARGS(Buffer, ARRAY_COUNT(Buffer), ARRAY_COUNT(Buffer) - 1, Fmt, Fmt);
		Buffer[255] = '\0';
		FString Str = Buffer;
		RecordEvent(CategoryIndex, Str);
	}
}

void FCsvProfiler::RecordEvent(int32 CategoryIndex, const FString& EventText)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		UE_LOG(LogCsvProfiler, Display, TEXT("CSVEvent [Frame %d] : \"%s\""), FCsvProfiler::Get()->GetCaptureFrameNumber(), *EventText);
		GCsvProfilerThreadDataTls.GetThreadData()->AddEvent(EventText, CategoryIndex);
	}
}

void FCsvProfiler::RecordEventAtTimestamp(int32 CategoryIndex, const FString& EventText, uint64 Cycles64)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		UE_LOG(LogCsvProfiler, Display, TEXT("CSVEvent [Frame %d] : \"%s\""), FCsvProfiler::Get()->GetCaptureFrameNumber(), *EventText);
		GCsvProfilerThreadDataTls.GetThreadData()->AddEventWithTimestamp(EventText, CategoryIndex,Cycles64);
	}
}

void FCsvProfiler::RecordCustomStat(const char * StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::RecordCustomStat(const FName& StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::RecordCustomStat(const char * StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::RecordCustomStat(const FName& StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		GCsvProfilerThreadDataTls.GetThreadData()->AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::Init()
{
	int32 NumCsvFrames = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("csvGpuStats")))
	{
		IConsoleVariable* CVarGPUCsvStatsEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCsvStatsEnabled"));
		if (CVarGPUCsvStatsEnabled)
		{
			CVarGPUCsvStatsEnabled->Set(1);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvTest")))
	{
		GCsvTestingGT = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvTestMT")))
	{
		GCsvTestingGT = true;
		GCsvTestingRT = true;
	}

	FString CsvCategoriesStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("csvCategories="), CsvCategoriesStr))
	{
		TArray<FString> CsvCategories;
		CsvCategoriesStr.ParseIntoArray(CsvCategories, TEXT(","), true);
		for (int i = 0; i < CsvCategories.Num(); i++)
		{
			int32 Index = FCsvCategoryData::Get()->GetCategoryIndex(CsvCategories[i]);
			if (Index > 0)
			{
				GCsvCategoriesEnabled[Index] = true;
			}
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("csvNoProcessingThread")))
	{
		GCsvUseProcessingThread = false;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvStatCounts")))
	{
		GCsvStatCounts = true;
	}
	if (!FApp::ShouldUseThreadingForPerformance())
	{
		GCsvUseProcessingThread = false;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("csvCaptureFrames="), NumCsvFrames))
	{
		check(IsInGameThread());
		BeginCapture(NumCsvFrames);

		// Call BeginFrame() to start capturing a dummy first "frame"
		// signal bInsertEndFrameAtFrameStart to insert an EndFrame() at the start of the first _real_ frame
		// We also add a FrameBeginTimestampsRT timestamp here, to create a dummy renderthread frame, to ensure the rows match up in the CSV
		BeginFrame();
		GFrameBoundaries.AddBeginFrameTimestamp(ECsvTimeline::Renderthread, false);
		bInsertEndFrameAtFrameStart = true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("csvRepeat="), GCsvRepeatCount);
}

bool FCsvProfiler::IsCapturing()
{
	check(IsInGameThread());
	return GCsvProfilerIsCapturing;
}

int32 FCsvProfiler::GetCaptureFrameNumber()
{
	return CaptureFrameNumber;
}

bool FCsvProfiler::EnableCategoryByString(const FString& CategoryName) const
{
	int32 Category = FCsvCategoryData::Get()->GetCategoryIndex(CategoryName);
	if (Category >= 0)
	{
		GCsvCategoriesEnabled[Category] = true;
		return true;
	}
	return false;
}

bool FCsvProfiler::IsCapturing_Renderthread()
{
	check(IsInRenderingThread());
	return GCsvProfilerIsCapturingRT;
}

float FCsvProfiler::ProcessStatData()
{
	check(IsInCsvProcessingThread());

	float ElapsedMS = 0.0f;
	if (!IsShuttingDown.GetValue())
	{
		double StartTime = FPlatformTime::Seconds();

		TArray<FCsvProfilerThreadData*> ProfilerThreadData;
		GCsvProfilerThreadDataTls.GetThreadDataArray(ProfilerThreadData);

		FCsvProfilerThreadData::FProcessThreadDataStats ProcessedDataStats;
		for (int t = 0; t < ProfilerThreadData.Num(); t++)
		{
			ProfilerThreadData[t]->ProcessThreadData(&ProcessedDataStats);
		}
		ElapsedMS = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
		CSV_CUSTOM_STAT(CsvProfiler, NumTimestampsProcessed, (int32)ProcessedDataStats.TimestampCount, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(CsvProfiler, NumCustomStatsProcessed, (int32)ProcessedDataStats.CustomStatCount, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(CsvProfiler, NumEventsProcessed, (int32)ProcessedDataStats.EventCount, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(CsvProfiler, ProcessCSVStats, ElapsedMS, ECsvCustomStatOp::Accumulate);
	}
	return ElapsedMS;
}

void CSVTest()
{
	uint32 FrameNumber = FCsvProfiler::Get()->GetCaptureFrameNumber();
	CSV_SCOPED_TIMING_STAT(CsvTest, CsvTestStat);
	CSV_CUSTOM_STAT(CsvTest, CaptureFrameNumber, int32(FrameNumber), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(CsvTest, SameCustomStat, 1, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(CsvTest, SameCustomStat, 1, ECsvCustomStatOp::Accumulate);
	for (int i = 0; i < 3; i++)
	{
		CSV_SCOPED_TIMING_STAT(CsvTest, RepeatStat1MS);
		FPlatformProcess::Sleep(0.001f);
	}

	{
		CSV_SCOPED_TIMING_STAT(CsvTest, TimerStatTimer);
		for (int i = 0; i < 100; i++)
		{
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner0);
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner1);
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner2);
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner3);
		}
	}

	{
		CSV_SCOPED_TIMING_STAT(CsvTest, CustomStatTimer);
		for (int i = 0; i < 100; i++)
		{
			CSV_CUSTOM_STAT(CsvTest, SetStat_99, i, ECsvCustomStatOp::Set); // Should be 99
			CSV_CUSTOM_STAT(CsvTest, MaxStat_99, 99 - i, ECsvCustomStatOp::Max); // Should be 99
			CSV_CUSTOM_STAT(CsvTest, MinStat_0, i, ECsvCustomStatOp::Min); // Should be 0
			CSV_CUSTOM_STAT(CsvTest, AccStat_4950, i, ECsvCustomStatOp::Accumulate); // Should be 4950
		}
		if (FrameNumber > 100)
		{
			CSV_SCOPED_TIMING_STAT(CsvTest, TimerOver100);
			CSV_CUSTOM_STAT(CsvTest, CustomStatOver100, int32(FrameNumber - 100), ECsvCustomStatOp::Set);
		}
	}
	{
		CSV_SCOPED_TIMING_STAT(CsvTest, EventTimer);
		if (FrameNumber % 20 < 2)
		{
			CSV_EVENT(CsvTest, TEXT("This is frame %d"), GFrameNumber);
		}
		if (FrameNumber % 50 == 0)
		{
			for (int i = 0; i < 5; i++)
			{
				CSV_EVENT(CsvTest, TEXT("Multiple Event %d"), i);
			}
		}
	}
	//for (int i = 0; i < 2048; i++)
	//{
	//	GCsvCategoriesEnabled[i] = false;
	//}
	//GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)] = true;
	//GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(CsvTest)] = true;

	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveLevel0);
		{
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveLevel1);
			CSV_SCOPED_TIMING_STAT(CsvTest, NonExclusiveTestLevel1);
			FPlatformProcess::Sleep(0.002f);
			{
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveLevel2);
				CSV_SCOPED_TIMING_STAT(CsvTest, NonExclusiveTestLevel2);
				FPlatformProcess::Sleep(0.003f);
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	{
		CSV_SCOPED_TIMING_STAT(CsvTest, ExclusiveTimerStatTimer);
		for (int i = 0; i < 100; i++)
		{
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner0);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner1);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner2);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner3);
		}
	}

}

#endif // CSV_PROFILER

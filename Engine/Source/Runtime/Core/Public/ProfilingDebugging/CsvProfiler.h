// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight multi-threaded CSV profiler which can be used for profiling in Test/Shipping builds
*/

#pragma once

#include "CoreTypes.h"
#include "Containers/Queue.h"
#include "UObject/NameTypes.h"
#include "Templates/UniquePtr.h"
#include "Async/Future.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/EnumClassFlags.h"

// Whether to allow the CSV profiler in shipping builds.
// Enable in a .Target.cs file if required.
#ifndef CSV_PROFILER_ENABLE_IN_SHIPPING
#define CSV_PROFILER_ENABLE_IN_SHIPPING 0
#endif

// Enables command line switches and unit tests of the CSV profiler.
// The default disables these features in a shipping build, but a .Target.cs file can override this.
#ifndef CSV_PROFILER_ALLOW_DEBUG_FEATURES
#define CSV_PROFILER_ALLOW_DEBUG_FEATURES (!UE_BUILD_SHIPPING)
#endif

#if WITH_SERVER_CODE
  #define CSV_PROFILER (WITH_ENGINE && 1)
#else
  #define CSV_PROFILER (WITH_ENGINE && (!UE_BUILD_SHIPPING || CSV_PROFILER_ENABLE_IN_SHIPPING))
#endif

#if CSV_PROFILER

#define CSV_TIMING_STATS_EMIT_NAMED_EVENTS 0
#define CSV_EXCLUSIVE_TIMING_STATS_EMIT_NAMED_EVENTS 0

// Helpers
#define CSV_CATEGORY_INDEX(CategoryName)						(_GCsvCategory_##CategoryName.Index)
#define CSV_CATEGORY_INDEX_GLOBAL								(0)
#define CSV_STAT_FNAME(StatName)								(_GCsvStat_##StatName.Name)

// Inline stats (no up front definition)
#define CSV_SCOPED_TIMING_STAT(Category,StatName)				FScopedCsvStat _ScopedCsvStat_ ## StatName (#StatName, CSV_CATEGORY_INDEX(Category));
#define CSV_SCOPED_TIMING_STAT_GLOBAL(StatName)					FScopedCsvStat _ScopedCsvStat_ ## StatName (#StatName, CSV_CATEGORY_INDEX_GLOBAL);
#define CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StatName)				FScopedCsvStatExclusive _ScopedCsvStatExclusive_ ## StatName (#StatName);
#define CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(StatName,Condition) FScopedCsvStatExclusiveConditional _ScopedCsvStatExclusive_ ## StatName (#StatName,Condition);

#define CSV_CUSTOM_STAT(Category,StatName,Value,Op)				FCsvProfiler::RecordCustomStat(#StatName, CSV_CATEGORY_INDEX(Category), Value, Op)
#define CSV_CUSTOM_STAT_GLOBAL(StatName,Value,Op) 				FCsvProfiler::RecordCustomStat(#StatName, CSV_CATEGORY_INDEX_GLOBAL, Value, Op)

// Stats declared up front
#define CSV_DEFINE_STAT(Category,StatName)						FCsvDeclaredStat _GCsvStat_##StatName((TCHAR*)TEXT(#StatName), CSV_CATEGORY_INDEX(Category));
#define CSV_DEFINE_STAT_GLOBAL(StatName)						FCsvDeclaredStat _GCsvStat_##StatName((TCHAR*)TEXT(#StatName), CSV_CATEGORY_INDEX_GLOBAL);
#define CSV_DECLARE_STAT_EXTERN(Category,StatName)				extern FCsvDeclaredStat _GCsvStat_##StatName
#define CSV_CUSTOM_STAT_DEFINED(StatName,Value,Op)				FCsvProfiler::RecordCustomStat(_GCsvStat_##StatName.Name, _GCsvStat_##StatName.CategoryIndex, Value, Op);

// Categories
#define CSV_DEFINE_CATEGORY(CategoryName,bDefaultValue)			FCsvCategory _GCsvCategory_##CategoryName(TEXT(#CategoryName),bDefaultValue)
#define CSV_DECLARE_CATEGORY_EXTERN(CategoryName)				extern FCsvCategory _GCsvCategory_##CategoryName

#define CSV_DEFINE_CATEGORY_MODULE(Module_API,CategoryName,bDefaultValue)	FCsvCategory Module_API _GCsvCategory_##CategoryName(TEXT(#CategoryName),bDefaultValue)
#define CSV_DECLARE_CATEGORY_MODULE_EXTERN(Module_API,CategoryName)			extern Module_API FCsvCategory _GCsvCategory_##CategoryName

// Events
#define CSV_EVENT(Category, Format, ...) 						FCsvProfiler::RecordEventf( CSV_CATEGORY_INDEX(Category), Format, ##__VA_ARGS__ )
#define CSV_EVENT_GLOBAL(Format, ...) 							FCsvProfiler::RecordEventf( CSV_CATEGORY_INDEX_GLOBAL, Format, ##__VA_ARGS__ )

// Metadata
#define CSV_METADATA(Key,Value)									FCsvProfiler::SetMetadata( Key, Value )

#else
  #define CSV_CATEGORY_INDEX(CategoryName)						
  #define CSV_CATEGORY_INDEX_GLOBAL								
  #define CSV_STAT_FNAME(StatName)								
  #define CSV_SCOPED_TIMING_STAT(Category,StatName)				
  #define CSV_SCOPED_TIMING_STAT_GLOBAL(StatName)					
  #define CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StatName)
  #define CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(StatName,Condition)
  #define CSV_CUSTOM_STAT(Category,StatName,Value,Op)				
  #define CSV_CUSTOM_STAT_GLOBAL(StatName,Value,Op) 				
  #define CSV_DEFINE_STAT(Category,StatName)						
  #define CSV_DEFINE_STAT_GLOBAL(StatName)						
  #define CSV_DECLARE_STAT_EXTERN(Category,StatName)				
  #define CSV_CUSTOM_STAT_DEFINED(StatName,Value,Op)				
  #define CSV_DEFINE_CATEGORY(CategoryName,bDefaultValue)			
  #define CSV_DECLARE_CATEGORY_EXTERN(CategoryName)				
  #define CSV_DEFINE_CATEGORY_MODULE(Module_API,CategoryName,bDefaultValue)	
  #define CSV_DECLARE_CATEGORY_MODULE_EXTERN(Module_API,CategoryName)			
  #define CSV_EVENT(Category, Format, ...) 						
  #define CSV_EVENT_GLOBAL(Format, ...)
  #define CSV_METADATA(Key,Value)
#endif


#if CSV_PROFILER
class FCsvProfilerFrame;
class FCsvProfilerThreadData;
class FCsvProfilerProcessingThread;
class FName;

enum class ECsvCustomStatOp : uint8
{
	Set,
	Min,
	Max,
	Accumulate,
};

enum class ECsvCommandType : uint8
{
	Start,
	Stop,
	Count
};

struct FCsvCategory;

struct FCsvDeclaredStat
{
	FCsvDeclaredStat(TCHAR* InNameString, uint32 InCategoryIndex) 
		: Name(InNameString)
		, CategoryIndex(InCategoryIndex) 
	{}

	FName Name;
	uint32 CategoryIndex;
};

enum class ECsvProfilerFlags
{
	None = 0,
	WriteCompletionFile = 1,
	CompressOutput = 2
};
ENUM_CLASS_FLAGS(ECsvProfilerFlags);

struct FCsvCaptureCommand
{
	FCsvCaptureCommand()
		: CommandType(ECsvCommandType::Count)
		, FrameRequested(-1)
		, Value(-1)
	{}

	FCsvCaptureCommand(ECsvCommandType InCommandType, uint32 InFrameRequested, uint32 InValue, const FString& InDestinationFolder, const FString& InFilename, ECsvProfilerFlags InFlags)
		: CommandType(InCommandType)
		, FrameRequested(InFrameRequested)
		, Value(InValue)
		, DestinationFolder(InDestinationFolder)
		, Filename(InFilename)
		, Flags(InFlags)
	{}

	FCsvCaptureCommand(ECsvCommandType InCommandType, uint32 InFrameRequested, TPromise<FString>* InCompletion, TSharedFuture<FString> InFuture)
		: CommandType(InCommandType)
		, FrameRequested(InFrameRequested)
		, Completion(MoveTemp(InCompletion))
		, Future(InFuture)
	{}

	ECsvCommandType CommandType;
	uint32 FrameRequested;
	uint32 Value;
	FString DestinationFolder;
	FString Filename;
	ECsvProfilerFlags Flags;
	TPromise<FString>* Completion;
	TSharedFuture<FString> Future;
};

/**
* FCsvProfiler class. This manages recording and reporting all for CSV stats
*/
class FCsvProfiler
{
	friend class FCsvProfilerProcessingThread;
	friend class FCsvProfilerThreadData;
	friend struct FCsvCategory;
private:
	static TUniquePtr<FCsvProfiler> Instance;		
public:
	FCsvProfiler();
	~FCsvProfiler();
	static CORE_API FCsvProfiler* Get();

	CORE_API void Init();

	/** Begin static interface (used by macros)*/
	/** Push/pop events */
	CORE_API static void BeginStat(const char * StatName, uint32 CategoryIndex);
	CORE_API static void EndStat(const char * StatName, uint32 CategoryIndex);

	CORE_API static void BeginExclusiveStat(const char * StatName);
	CORE_API static void EndExclusiveStat(const char * StatName);

	CORE_API static void RecordCustomStat(const char * StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const FName& StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const char * StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const FName& StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp);

	CORE_API static void RecordEvent(int32 CategoryIndex, const FString& EventText);
	CORE_API static void RecordEventAtTimestamp(int32 CategoryIndex, const FString& EventText, uint64 Cycles64);

	CORE_API static void SetMetadata(const TCHAR* Key, const TCHAR* Value);

	template <typename FmtType, typename... Types>
	FORCEINLINE static void RecordEventf(int32 CategoryIndex, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FCsvProfiler::RecordEventf");
		RecordEventfInternal(CategoryIndex, Fmt, Args...);
	}

	/** Singleton interface */
	CORE_API bool IsCapturing();
	CORE_API bool IsCapturing_Renderthread();
	CORE_API bool IsWritingFile();

	CORE_API int32 GetCaptureFrameNumber();

	CORE_API bool EnableCategoryByString(const FString& CategoryName) const;

	/** Per-frame update */
	CORE_API void BeginFrame();
	CORE_API void EndFrame();

	/** Begin Capture */
	CORE_API void BeginCapture(int InNumFramesToCapture = -1,
		const FString& InDestinationFolder = FString(),
		const FString& InFilename = FString(),
		ECsvProfilerFlags InFlags = ECsvProfilerFlags::None);

	/**
	 * End Capture
	 * EventToSignal is optional. If provided, the CSV profiler will signal the event when the async file write is complete.
	 * The returned TFuture can be waited on to retrieve the filename of the csv file that was written to disk.
	 */
	CORE_API TSharedFuture<FString> EndCapture(FGraphEventRef EventToSignal = nullptr);

	/** Renderthread begin/end frame */
	CORE_API void BeginFrameRT();
	CORE_API void EndFrameRT();

	CORE_API void SetDeviceProfileName(FString InDeviceProfileName);

	CORE_API FString GetOutputFilename() const { return OutputFilename; }

private:
	CORE_API static void VARARGS RecordEventfInternal(int32 CategoryIndex, const TCHAR* Fmt, ...);

	static CORE_API int32 RegisterCategory(const FString& Name, bool bEnableByDefault, bool bIsGlobal);
	static int32 GetCategoryIndex(const FString& Name);

	void FinalizeCsvFile();

	float ProcessStatData();

	int32 NumFramesToCapture;
	int32 CaptureFrameNumber;

	bool bInsertEndFrameAtFrameStart;

	uint64 LastEndFrameTimestamp;
	uint32 CaptureEndFrameCount;

	FString OutputFilename;
	TQueue<FCsvCaptureCommand> CommandQueue;
	FCsvProfilerProcessingThread* ProcessingThread;

	FEvent* FileWriteBlockingEvent;

	FString DeviceProfileName;

	FThreadSafeCounter IsShuttingDown;

	TMap<FString, FString> MetadataMap;
	FCriticalSection MetadataCS;

	class FCsvStreamWriter* CsvWriter;

	ECsvProfilerFlags CurrentFlags;
};

class FScopedCsvStat
{
public:
	FScopedCsvStat(const char * InStatName, uint32 InCategoryIndex)
		: StatName(InStatName)
		, CategoryIndex(InCategoryIndex)
	{
		FCsvProfiler::BeginStat(StatName, CategoryIndex);
#if CSV_TIMING_STATS_EMIT_NAMED_EVENTS
		FPlatformMisc::BeginNamedEvent(FColor(255, 128, 255), StatName);
#endif
	}

	~FScopedCsvStat()
	{
#if CSV_TIMING_STATS_EMIT_NAMED_EVENTS
		FPlatformMisc::EndNamedEvent();
#endif
		FCsvProfiler::EndStat(StatName, CategoryIndex);
	}
	const char * StatName;
	uint32 CategoryIndex;
};

class FScopedCsvStatExclusive 
{
public:
	FScopedCsvStatExclusive(const char * InStatName)
		: StatName(InStatName)
	{
		FCsvProfiler::BeginExclusiveStat(StatName);
#if CSV_EXCLUSIVE_TIMING_STATS_EMIT_NAMED_EVENTS
		FPlatformMisc::BeginNamedEvent(FColor(255, 128, 128), StatName);
#endif
	}

	~FScopedCsvStatExclusive()
	{
#if CSV_EXCLUSIVE_TIMING_STATS_EMIT_NAMED_EVENTS
		FPlatformMisc::EndNamedEvent();
#endif
		FCsvProfiler::EndExclusiveStat(StatName);
	}
	const char * StatName;
};

class FScopedCsvStatExclusiveConditional
{
public:
	FScopedCsvStatExclusiveConditional(const char * InStatName, bool bInCondition)
		: StatName(InStatName)
		, bCondition(bInCondition)
	{
		if (bCondition)
		{
			FCsvProfiler::BeginExclusiveStat(StatName);
#if CSV_EXCLUSIVE_TIMING_STATS_EMIT_NAMED_EVENTS
			FPlatformMisc::BeginNamedEvent(FColor(255,128,128),StatName);
#endif
		}
	}

	~FScopedCsvStatExclusiveConditional()
	{
		if (bCondition)
		{
#if CSV_EXCLUSIVE_TIMING_STATS_EMIT_NAMED_EVENTS
			FPlatformMisc::EndNamedEvent();
#endif
			FCsvProfiler::EndExclusiveStat(StatName);
		}
	}
	const char * StatName;
	bool bCondition;
};


struct FCsvCategory
{
	FCsvCategory() : Index(-1) {}
	FCsvCategory(const TCHAR* CategoryString, bool bDefaultValue, bool bIsGlobal = false)
	{
		Name = CategoryString;
		Index = FCsvProfiler::RegisterCategory(Name, bDefaultValue, bIsGlobal);
	}

	uint32 Index;
	FString Name;
};


CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Exclusive);


#endif //CSV_PROFILER
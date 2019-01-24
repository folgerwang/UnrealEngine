// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"


// this is currently incompatible with PLATFORM_USES_FIXED_GMalloc_CLASS, because this ends up being included way too early
// and currently, PLATFORM_USES_FIXED_GMalloc_CLASS is only used in Test/Shipping builds, where we don't have STATS anyway,
// but we can't #include "Stats.h" to find out
#if PLATFORM_USES_FIXED_GMalloc_CLASS
	#define ENABLE_LOW_LEVEL_MEM_TRACKER 0
	#define DECLARE_LLM_MEMORY_STAT(CounterName,StatId,GroupId)
	#define DECLARE_LLM_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API)
#else
	#include "Stats/Stats.h"

	#define LLM_SUPPORTED_PLATFORM (PLATFORM_XBOXONE || PLATFORM_PS4 || PLATFORM_WINDOWS || PLATFORM_IOS || PLATFORM_MAC || PLATFORM_ANDROID || PLATFORM_SWITCH || PLATFORM_UNIX)

#ifndef ALLOW_LOW_LEVEL_MEM_TRACKER_IN_TEST
	#define ALLOW_LOW_LEVEL_MEM_TRACKER_IN_TEST 0
#endif

	// *** enable/disable LLM here ***
#ifndef ENABLE_LOW_LEVEL_MEM_TRACKER
	#define ENABLE_LOW_LEVEL_MEM_TRACKER (!UE_BUILD_SHIPPING && (!UE_BUILD_TEST || ALLOW_LOW_LEVEL_MEM_TRACKER_IN_TEST) && LLM_SUPPORTED_PLATFORM && WITH_ENGINE && 1)
#endif

	// using asset tagging requires a significantly higher number of per-thread tags, so make it optional
	// even if this is on, we still need to run with -llmtagsets=assets because of the shear number of stat ids it makes
	// LLM Assets can be viewed in game using 'Stat LLMAssets'
	#define LLM_ALLOW_ASSETS_TAGS 0

	#define LLM_STAT_TAGS_ENABLED (LLM_ALLOW_ASSETS_TAGS || 0)

	// this controls if the commandline is used to enable tracking, or to disable it. If LLM_COMMANDLINE_ENABLES_FUNCTIONALITY is true, 
	// then tracking will only happen through Engine::Init(), at which point it will be disabled unless the commandline tells 
	// it to keep going (with -llm). If LLM_COMMANDLINE_ENABLES_FUNCTIONALITY is false, then tracking will be on unless the commandline
	// disables it (with -nollm)
#ifndef LLM_COMMANDLINE_ENABLES_FUNCTIONALITY
	#define LLM_COMMANDLINE_ENABLES_FUNCTIONALITY 1
#endif 

	// when set to 1, forces LLM to be enabled without having to parse the command line.
#ifndef LLM_AUTO_ENABLE
	#define LLM_AUTO_ENABLE 0
#endif

	#if STATS
		#define DECLARE_LLM_MEMORY_STAT(CounterName,StatId,GroupId) \
			DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_PhysicalLLM); \
			static DEFINE_STAT(StatId)
		#define DECLARE_LLM_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API) \
			DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_PhysicalLLM); \
			extern API DEFINE_STAT(StatId);
	#else
		#define DECLARE_LLM_MEMORY_STAT(CounterName,StatId,GroupId)
		#define DECLARE_LLM_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API)
	#endif
#endif	// #if PLATFORM_USES_FIXED_GMalloc_CLASS


#if STATS
	DECLARE_STATS_GROUP(TEXT("LLM FULL"), STATGROUP_LLMFULL, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Platform"), STATGROUP_LLMPlatform, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Summary"), STATGROUP_LLM, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Overhead"), STATGROUP_LLMOverhead, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Assets"), STATGROUP_LLMAssets, STATCAT_Advanced);

	DECLARE_LLM_MEMORY_STAT_EXTERN(TEXT("Engine"), STAT_EngineSummaryLLM, STATGROUP_LLM, CORE_API);
#endif


#if ENABLE_LOW_LEVEL_MEM_TRACKER

#if DO_CHECK

	namespace LLMPrivate
	{
		static inline bool HandleAssert(bool bLog, const TCHAR* Format, ...)
		{
			if (bLog)
			{
				TCHAR DescriptionString[4096];
				GET_VARARGS(DescriptionString, ARRAY_COUNT(DescriptionString), ARRAY_COUNT(DescriptionString) - 1, Format, Format);

				FPlatformMisc::LowLevelOutputDebugString(DescriptionString);

				if (FPlatformMisc::IsDebuggerPresent())
					FPlatformMisc::PromptForRemoteDebugging(true);

				UE_DEBUG_BREAK();
			}
			return false;
		}

		// This is used by ensure to generate a bool per instance
		// by passing a lambda which will uniquely instantiate the template.
		template <typename Type>
		bool TrueOnFirstCallOnly(const Type&)
		{
			static bool bValue = true;
			bool Result = bValue;
			bValue = false;
			return Result;
		}
	}

#if !USING_CODE_ANALYSIS
	#define LLMTrueOnFirstCallOnly			LLMPrivate::TrueOnFirstCallOnly([]{})
#else
	#define LLMTrueOnFirstCallOnly			false
#endif

#define LLMCheckMessage(expr)          TEXT("LLM check failed: %s [File:%s] [Line: %d]\r\n"),             TEXT(#expr), TEXT(__FILE__), __LINE__
#define LLMCheckfMessage(expr, format) TEXT("LLM check failed: %s [File:%s] [Line: %d]\r\n") format,      TEXT(#expr), TEXT(__FILE__), __LINE__
#define LLMEnsureMessage(expr)         TEXT("LLM ensure failed: %s [File:%s] [Line: %d]\r\n"),            TEXT(#expr), TEXT(__FILE__), __LINE__

#define LLMCheck(expr)					do { if (UNLIKELY(!(expr))) { LLMPrivate::HandleAssert(true, LLMCheckMessage(expr));                         FPlatformMisc::RaiseException(1); } } while(false)
#define LLMCheckf(expr,format,...)		do { if (UNLIKELY(!(expr))) { LLMPrivate::HandleAssert(true, LLMCheckfMessage(expr, format), ##__VA_ARGS__); FPlatformMisc::RaiseException(1); } } while(false)
#define LLMEnsure(expr)					(LIKELY(!!(expr)) || LLMPrivate::HandleAssert(LLMTrueOnFirstCallOnly, LLMEnsureMessage(expr)))

#else

#define LLMCheck(expr)
#define LLMCheckf(expr,...)
#define LLMEnsure(expr)			(!!(expr))

#endif

#if LLM_STAT_TAGS_ENABLED
	#define LLM_TAG_TYPE uint64
#else
	#define LLM_TAG_TYPE uint8
#endif

// estimate the maximum amount of memory LLM will need to run on a game with around 4 million allocations.
// Make sure that you have debug memory enabled on consoles (on screen warning will show if you don't)
// (currently only used on PS4 to stop it reserving a large chunk up front. This will go away with the new memory system)
#define LLM_MEMORY_OVERHEAD (600LL*1024*1024)

/*
 * LLM Trackers
 */
enum class ELLMTracker : uint8
{
	Platform,
	Default,

	Max,
};

/*
 * optional tags that need to be enabled with -llmtagsets=x,y,z on the commandline
 */
enum class ELLMTagSet : uint8
{
	None,
	Assets,
	AssetClasses,
	
	Max,	// note: check out FLowLevelMemTracker::ShouldReduceThreads and IsAssetTagForAssets if you add any asset-style tagsets
};

#define LLM_ENUM_GENERIC_TAGS(macro) \
	macro(Untagged,								"Untagged",						NAME_None,													NAME_None,										-1)\
	macro(Paused,								"Paused",						NAME_None,													NAME_None,										-1)\
	macro(Total,								"Total",						GET_STATFNAME(STAT_TotalLLM),								GET_STATFNAME(STAT_TrackedTotalSummaryLLM),		-1)\
	macro(Untracked,							"Untracked",					GET_STATFNAME(STAT_UntrackedLLM),							GET_STATFNAME(STAT_TrackedTotalSummaryLLM),		-1)\
	macro(PlatformTotal,						"Total",						GET_STATFNAME(STAT_PlatformTotalLLM),						NAME_None,										-1)\
	macro(TrackedTotal,							"TrackedTotal",					GET_STATFNAME(STAT_TrackedTotalLLM),						GET_STATFNAME(STAT_TrackedTotalSummaryLLM),		-1)\
	macro(UntaggedTotal,						"Untagged",						GET_STATFNAME(STAT_UntaggedTotalLLM),						NAME_None,										-1)\
	macro(WorkingSetSize,						"WorkingSetSize",				GET_STATFNAME(STAT_WorkingSetSizeLLM),						GET_STATFNAME(STAT_TrackedTotalSummaryLLM),		-1)\
	macro(PagefileUsed,							"PagefileUsed",					GET_STATFNAME(STAT_PagefileUsedLLM),						GET_STATFNAME(STAT_TrackedTotalSummaryLLM),		-1)\
	macro(PlatformTrackedTotal,					"TrackedTotal",					GET_STATFNAME(STAT_PlatformTrackedTotalLLM),				NAME_None,										-1)\
	macro(PlatformUntaggedTotal,				"Untagged",						GET_STATFNAME(STAT_PlatformUntaggedTotalLLM),				NAME_None,										-1)\
	macro(PlatformUntracked,					"Untracked",					GET_STATFNAME(STAT_PlatformUntrackedLLM),					NAME_None,										-1)\
	macro(PlatformOverhead,						"LLMOverhead",					GET_STATFNAME(STAT_PlatformOverheadLLM),					NAME_None,										-1)\
	macro(FMalloc,								"FMalloc",						GET_STATFNAME(STAT_FMallocLLM),								NAME_None,										-1)\
	macro(FMallocUnused,						"FMallocUnused",				GET_STATFNAME(STAT_FMallocUnusedLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(ThreadStack,							"ThreadStack",					GET_STATFNAME(STAT_ThreadStackLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(ThreadStackPlatform,					"ThreadStack",					GET_STATFNAME(STAT_ThreadStackPlatformLLM),					NAME_None,										-1)\
	macro(ProgramSizePlatform,					"ProgramSize",					GET_STATFNAME(STAT_ProgramSizePlatformLLM),					NAME_None,										-1)\
	macro(ProgramSize,							"ProgramSize",					GET_STATFNAME(STAT_ProgramSizeLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(BackupOOMMemoryPoolPlatform,			"OOMBackupPool",				GET_STATFNAME(STAT_OOMBackupPoolPlatformLLM),				NAME_None,										-1)\
	macro(BackupOOMMemoryPool,					"OOMBackupPool",				GET_STATFNAME(STAT_OOMBackupPoolLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(GenericPlatformMallocCrash,			"GenericPlatformMallocCrash",	GET_STATFNAME(STAT_GenericPlatformMallocCrashLLM),			GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(GenericPlatformMallocCrashPlatform,	"GenericPlatformMallocCrash",	GET_STATFNAME(STAT_GenericPlatformMallocCrashPlatformLLM),	GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(EngineMisc,							"EngineMisc",					GET_STATFNAME(STAT_EngineMiscLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(TaskGraphTasksMisc,					"TaskGraphMiscTasks",			GET_STATFNAME(STAT_TaskGraphTasksMiscLLM),					GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(Audio,								"Audio",						GET_STATFNAME(STAT_AudioLLM),								GET_STATFNAME(STAT_AudioSummaryLLM),			-1)\
	macro(AudioMixer,							"AudioMixer",					GET_STATFNAME(STAT_AudioMixerLLM),							GET_STATFNAME(STAT_AudioSummaryLLM),			-1)\
	macro(AudioPrecache,						"AudioPrecache",				GET_STATFNAME(STAT_AudioPrecacheLLM),						GET_STATFNAME(STAT_AudioSummaryLLM),			-1)\
	macro(AudioDecompress,						"AudioDecompress",				GET_STATFNAME(STAT_AudioDecompressLLM),						GET_STATFNAME(STAT_AudioSummaryLLM),			-1)\
	macro(AudioRealtimePrecache,				"AudioRealtimePrecache",		GET_STATFNAME(STAT_AudioRealtimePrecacheLLM),				GET_STATFNAME(STAT_AudioSummaryLLM),			-1)\
	macro(AudioFullDecompress,					"AudioFullDecompress",			GET_STATFNAME(STAT_AudioFullDecompressLLM),					GET_STATFNAME(STAT_AudioSummaryLLM),			-1)\
	macro(FName,								"FName",						GET_STATFNAME(STAT_FNameLLM),								GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(Networking,							"Networking",					GET_STATFNAME(STAT_NetworkingLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(Meshes,								"Meshes",						GET_STATFNAME(STAT_MeshesLLM),								GET_STATFNAME(STAT_MeshesSummaryLLM),			-1)\
	macro(Stats,								"Stats",						GET_STATFNAME(STAT_StatsLLM),								GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(Shaders,								"Shaders",						GET_STATFNAME(STAT_ShadersLLM),								GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(PSO,									"PSO",							GET_STATFNAME(STAT_PSOLLM),									GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(Textures,								"Textures",						GET_STATFNAME(STAT_TexturesLLM),							GET_STATFNAME(STAT_TexturesSummaryLLM),			-1)\
	macro(RenderTargets,						"RenderTargets",				GET_STATFNAME(STAT_RenderTargetsLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(RHIMisc,								"RHIMisc",						GET_STATFNAME(STAT_RHIMiscLLM),								GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(AsyncLoading,							"AsyncLoading",					GET_STATFNAME(STAT_AsyncLoadingLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(UObject,								"UObject",						GET_STATFNAME(STAT_UObjectLLM),								GET_STATFNAME(STAT_UObjectSummaryLLM),			-1)\
	macro(Animation,							"Animation",					GET_STATFNAME(STAT_AnimationLLM),							GET_STATFNAME(STAT_AnimationSummaryLLM),		-1)\
	macro(StaticMesh,							"StaticMesh",					GET_STATFNAME(STAT_StaticMeshLLM),							GET_STATFNAME(STAT_StaticMeshSummaryLLM),		-1)\
	macro(Materials,							"Materials",					GET_STATFNAME(STAT_MaterialsLLM),							GET_STATFNAME(STAT_MaterialsSummaryLLM),		-1)\
	macro(Particles,							"Particles",					GET_STATFNAME(STAT_ParticlesLLM),							GET_STATFNAME(STAT_ParticlesSummaryLLM),		-1)\
	macro(GC,									"GC",							GET_STATFNAME(STAT_GCLLM),									GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(UI,									"UI",							GET_STATFNAME(STAT_UILLM),									GET_STATFNAME(STAT_UISummaryLLM),				-1)\
	macro(PhysX,								"PhysX",						GET_STATFNAME(STAT_PhysXLLM),								GET_STATFNAME(STAT_PhysXSummaryLLM),			-1)\
	macro(EnginePreInitMemory,					"EnginePreInit",				GET_STATFNAME(STAT_EnginePreInitLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(EngineInitMemory,						"EngineInit",					GET_STATFNAME(STAT_EngineInitLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(RenderingThreadMemory,				"RenderingThread",				GET_STATFNAME(STAT_RenderingThreadLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(LoadMapMisc,							"LoadMapMisc",					GET_STATFNAME(STAT_LoadMapMiscLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(StreamingManager,						"StreamingManager",				GET_STATFNAME(STAT_StreamingManagerLLM),					GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(GraphicsPlatform,						"Graphics",						GET_STATFNAME(STAT_GraphicsPlatformLLM),					NAME_None,										-1)\
	macro(FileSystem,							"FileSystem",					GET_STATFNAME(STAT_FileSystemLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(Localization,							"Localization",					GET_STATFNAME(STAT_LocalizationLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(VertexBuffer,							"VertexBuffer",					GET_STATFNAME(STAT_VertexBufferLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(IndexBuffer,							"IndexBuffer",					GET_STATFNAME(STAT_IndexBufferLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(UniformBuffer,						"UniformBuffer",				GET_STATFNAME(STAT_UniformBufferLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(AssetRegistry,						"AssetRegistry",				GET_STATFNAME(STAT_AssetRegistryLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(ConfigSystem,							"ConfigSystem",					GET_STATFNAME(STAT_ConfigSystemLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(InitUObject,							"InitUObject",					GET_STATFNAME(STAT_InitUObjectLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(VideoRecording,						"VideoRecording",				GET_STATFNAME(STAT_VideoRecordingLLM),						GET_STATFNAME(STAT_EngineSummaryLLM),			-1)\
	macro(CsvProfiler,							"CsvProfiler",					GET_STATFNAME(STAT_CsvProfilerLLM),							GET_STATFNAME(STAT_EngineSummaryLLM),			-1)

/*
 * Enum values to be passed in to LLM_SCOPE() macro
 */
enum class ELLMTag : LLM_TAG_TYPE
{
#define LLM_ENUM(Enum,Str,Stat,Group,Parent) Enum,
	LLM_ENUM_GENERIC_TAGS(LLM_ENUM)
#undef LLM_ENUM

	GenericTagCount,

	//------------------------------
	// Platform tags
	PlatformTagStart = 100,
	PlatformTagEnd = 0xff,

	// anything above this value is treated as an FName for a stat section
};

static const uint32 LLM_TAG_COUNT = 256;

/**
 * Passed in to OnLowLevelAlloc to specify the type of allocation. Used to track FMalloc total
 * and pausing for a specific allocation type.
 */
enum class ELLMAllocType
{
	None = 0,
	FMalloc,
	System,

	Count
};

extern const ANSICHAR* LLMGetTagNameANSI(ELLMTag Tag);
extern const TCHAR* LLMGetTagName(ELLMTag Tag);
extern FName LLMGetTagStatGroup(ELLMTag Tag);
extern FName LLMGetTagStat(ELLMTag Tag);

/*
 * LLM utility macros
 */
#define LLM(x) x
#define LLM_IF_ENABLED(x) if (!FLowLevelMemTracker::bIsDisabled) { x; }
#define SCOPE_NAME PREPROCESSOR_JOIN(LLMScope,__LINE__)

///////////////////////////////////////////////////////////////////////////////////////
// These are the main macros to use externally when tracking memory
///////////////////////////////////////////////////////////////////////////////////////

/**
 * LLM scope macros
 */
#define LLM_SCOPE(Tag) FLLMScope SCOPE_NAME(Tag, ELLMTagSet::None, ELLMTracker::Default);
#define LLM_PLATFORM_SCOPE(Tag) FLLMScope SCOPE_NAME(Tag, ELLMTagSet::None, ELLMTracker::Platform);

 /**
 * LLM Pause scope macros
 */
#define LLM_SCOPED_PAUSE_TRACKING(AllocType) FLLMPauseScope SCOPE_NAME(NAME_None, 0, ELLMTracker::Max, AllocType);
#define LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(Tracker, AllocType) FLLMPauseScope SCOPE_NAME(NAME_None, 0, Tracker, AllocType);
#define LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(Tag, Amount, Tracker, AllocType) FLLMPauseScope SCOPE_NAME(Tag, Amount, Tracker, AllocType);

/**
 * LLM Stat scope macros (if stats system is enabled)
 */
#if LLM_STAT_TAGS_ENABLED
	#define LLM_SCOPED_TAG_WITH_STAT(Stat, Tracker) FLLMScope SCOPE_NAME(GET_STATFNAME(Stat), ELLMTagSet::None, Tracker);
	#define LLM_SCOPED_TAG_WITH_STAT_IN_SET(Stat, Set, Tracker) FLLMScope SCOPE_NAME(GET_STATFNAME(Stat), Set, Tracker);
	#define LLM_SCOPED_TAG_WITH_STAT_NAME(StatName, Tracker) FLLMScope SCOPE_NAME(StatName, ELLMTagSet::None, Tracker);
	#define LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(StatName, Set, Tracker) FLLMScope SCOPE_NAME(StatName, Set, Tracker);
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG(Stat) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMPlatform); LLM_SCOPED_TAG_WITH_STAT(Stat, ELLMTracker::Platform);
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG_IN_SET(Stat, Set) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMPlatform); LLM_SCOPED_TAG_WITH_STAT_IN_SET(Stat, Set, ELLMTracker::Platform);
	#define LLM_SCOPED_SINGLE_STAT_TAG(Stat) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMFULL); LLM_SCOPED_TAG_WITH_STAT(Stat, ELLMTracker::Default);
	#define LLM_SCOPED_SINGLE_STAT_TAG_IN_SET(Stat, Set) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMFULL); LLM_SCOPED_TAG_WITH_STAT_IN_SET(Stat, Set, ELLMTracker::Default);
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_STAT_AND_AMOUNT(Stat, Amount, Tracker) FLLMPauseScope SCOPE_NAME(GET_STATFNAME(Stat), Amount, Tracker, ELLMAllocType::None);
	#define LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(Object, Set) LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(FLowLevelMemTracker::Get().IsTagSetActive(Set) ? (FDynamicStats::CreateMemoryStatId<FStatGroup_STATGROUP_LLMAssets>(FName(*(Object)->GetFullName())).GetName()) : NAME_None, Set, ELLMTracker::Default);

	// special stat pushing for when asset tracking is on, which abuses the poor thread tracking
	#define LLM_PUSH_STATS_FOR_ASSET_TAGS() if (FLowLevelMemTracker::Get().IsTagSetActive(ELLMTagSet::Assets)) FLowLevelMemTracker::Get().UpdateStatsPerFrame();
#else
	#define LLM_SCOPED_TAG_WITH_STAT(...)
	#define LLM_SCOPED_TAG_WITH_STAT_IN_SET(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_STAT_AND_AMOUNT(...)
	#define LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(...)
	#define LLM_PUSH_STATS_FOR_ASSET_TAGS()
#endif

typedef void*(*LLMAllocFunction)(size_t);
typedef void(*LLMFreeFunction)(void*, size_t);

/**
 * The allocator LLM uses to allocate internal memory. Uses platform defined
 * allocation functions to grab memory directly from the OS.
 */
class FLLMAllocator
{
public:
	FLLMAllocator()
	: PlatformAlloc(NULL)
	, PlatformFree(NULL)
	, Alignment(0)
	{
	}

	void Initialise(LLMAllocFunction InAlloc, LLMFreeFunction InFree, int32 InAlignment)
	{
		PlatformAlloc = InAlloc;
		PlatformFree = InFree;
		Alignment = InAlignment;
	}

	void* Alloc(size_t Size)
	{
		Size = Align(Size, Alignment);
		FScopeLock Lock(&CriticalSection);
		void* Ptr = PlatformAlloc(Size);
		Total += Size;
		LLMCheck(Ptr);
		return Ptr;
	}

	void Free(void* Ptr, size_t Size)
	{
		Size = Align(Size, Alignment);
		FScopeLock Lock(&CriticalSection);
		PlatformFree(Ptr, Size);
		Total -= Size;
	}

	int64 GetTotal() const
	{
		FScopeLock Lock((FCriticalSection*)&CriticalSection);
		return Total;
	}

private:
	FCriticalSection CriticalSection;
	LLMAllocFunction PlatformAlloc;
	LLMFreeFunction PlatformFree;
	int64 Total;
	int32 Alignment;
};

struct FLLMPlatformTag
{
	int32 Tag;
	const TCHAR* Name;
	FName StatName;
	FName SummaryStatName;
};

/*
 * The main LLM tracker class
 */
class CORE_API FLowLevelMemTracker
{
public:

	// get the singleton, which makes sure that we always have a valid object
	inline static FLowLevelMemTracker& Get()
	{
		if (TrackerInstance)
			return *TrackerInstance;
		else
			return Construct();
	}

	static FLowLevelMemTracker& Construct();

	static bool IsEnabled();

	// we always start up running, but if the commandline disables us, we will do it later after main
	// (can't get the commandline early enough in a cross-platform way)
	void ProcessCommandLine(const TCHAR* CmdLine);
	
	// Return the total amount of memory being tracked
	uint64 GetTotalTrackedMemory(ELLMTracker Tracker);

	// this is the main entry point for the class - used to track any pointer that was allocated or freed 
	void OnLowLevelAlloc(ELLMTracker Tracker, const void* Ptr, uint64 Size, ELLMTag DefaultTag = ELLMTag::Untagged, ELLMAllocType AllocType = ELLMAllocType::None);		// DefaultTag is used it no other tag is set
	void OnLowLevelFree(ELLMTracker Tracker, const void* Ptr, ELLMAllocType AllocType = ELLMAllocType::None);

	// call if an allocation is moved in memory, such as in a defragger
	void OnLowLevelAllocMoved(ELLMTracker Tracker, const void* Dest, const void* Source);

	// expected to be called once a frame, from game thread or similar - updates memory stats 
	void UpdateStatsPerFrame(const TCHAR* LogName=nullptr);

	// Optionally set the amount of memory taken up before the game starts for executable and data segments
	void InitialiseProgramSize();
	void SetProgramSize(uint64 InProgramSize);

	// console command handler
	bool Exec(const TCHAR* Cmd, FOutputDevice& Ar);

	// are we in the more intensive asset tracking mode, and is it active
	bool IsTagSetActive(ELLMTagSet Set);

	// for some tag sets, it's really useful to reduce threads, to attribute allocations to assets, for instance
	bool ShouldReduceThreads();

    // get the top active tag for the given tracker
    int64 GetActiveTag(ELLMTracker Tracker);

	void RegisterPlatformTag(int32 Tag, const TCHAR* Name, FName StatName, FName SummaryStatName, int32 ParentTag = -1);
    
	// look up the tag associated with the given name
	bool FindTagByName( const TCHAR* Name, uint64& OutTag ) const;

	// get the name for the given tag
	const TCHAR* FindTagName(uint64 Tag) const;

	// Get the amount of memory for a tag from the given tracker
	int64 GetTagAmountForTracker(ELLMTracker Tracker, ELLMTag Tag);

private:
	FLowLevelMemTracker();

	~FLowLevelMemTracker();

	void InitialiseTrackers();

	class FLLMTracker* GetTracker(ELLMTracker Tracker);

	friend class FLLMScope;
	friend class FLLMPauseScope;

	FLLMAllocator Allocator;
	
	bool bFirstTimeUpdating;

	uint64 ProgramSize;

	bool ActiveSets[(int32)ELLMTagSet::Max];

	bool bCanEnable;

	bool bCsvWriterEnabled;

	bool bInitialisedTrackers;

	FLLMPlatformTag PlatformTags[(int32)ELLMTag::PlatformTagEnd + 1 - (int32)ELLMTag::PlatformTagStart];

	FLLMTracker* Trackers[(int32)ELLMTracker::Max];

	int32 ParentTags[(int32)ELLMTag::PlatformTagEnd];

	static FLowLevelMemTracker* TrackerInstance;

public: // really internal but needs to be visible for LLM_IF_ENABLED macro
	static bool bIsDisabled;
};

/*
 * LLM scope for tracking memory
 */
class CORE_API FLLMScope
{
public:
	FLLMScope(FName StatIDName, ELLMTagSet Set, ELLMTracker Tracker);
	FLLMScope(ELLMTag Tag, ELLMTagSet Set, ELLMTracker Tracker);
	~FLLMScope();
protected:
	void Init(int64 Tag, ELLMTagSet Set, ELLMTracker Tracker);
	ELLMTagSet TagSet;
	ELLMTracker TrackerSet;
	bool Enabled;
};

/*
* LLM scope for pausing LLM (disables the allocation hooks)
*/
class CORE_API FLLMPauseScope
{
public:
	FLLMPauseScope(FName StatIDName, int64 Amount, ELLMTracker TrackerToPause, ELLMAllocType InAllocType);
	FLLMPauseScope(ELLMTag Tag, int64 Amount, ELLMTracker TrackerToPause, ELLMAllocType InAllocType);
	~FLLMPauseScope();
protected:
	void Init(int64 Tag, int64 Amount, ELLMTracker TrackerToPause, ELLMAllocType InAllocType);
	ELLMTracker PausedTracker;
	ELLMAllocType AllocType;
};

#else
	#define LLM(...)
	#define LLM_IF_ENABLED(...)
	#define LLM_SCOPE(...)
	#define LLM_PLATFORM_SCOPE(...)
	#define LLM_SCOPED_TAG_WITH_STAT(...)
	#define LLM_SCOPED_TAG_WITH_STAT_IN_SET(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_SINGLE_RHI_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_RHI_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(...)
	#define LLM_SCOPED_PAUSE_TRACKING(...)
	#define LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(...)
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(...)
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_STAT_AND_AMOUNT(...)
	#define LLM_PUSH_STATS_FOR_ASSET_TAGS()
#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

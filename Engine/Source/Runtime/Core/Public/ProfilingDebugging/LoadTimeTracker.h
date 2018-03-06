// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
* Declarations for LoadTimer which helps get load times for various parts of the game.
*/

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "ProfilingDebugging/ScopedTimers.h"

#ifndef ENABLE_LOADTIME_TRACKING
	#define ENABLE_LOADTIME_TRACKING 0
#endif

#define ENABLE_LOADTIME_RAW_TIMINGS 0

/** High level load time tracker utility (such as initial engine startup or game specific timings) */
class CORE_API FLoadTimeTracker
{
public:
	static FLoadTimeTracker& Get()
	{
		static FLoadTimeTracker Singleton;
		return Singleton;
	}

	/** Adds a scoped time for a given label.  Records each instance individually */
	void ReportScopeTime(double ScopeTime, const FName ScopeLabel);

	/** Gets/adds a scoped time for a given label and instance. Records each instance individually */
	double& GetScopeTimeAccumulator(const FName& ScopeLabel, const FName& ScopeInstance);

	/** Prints out total time and individual times */
	void DumpHighLevelLoadTimes() const;

	static void DumpHighLevelLoadTimesStatic()
	{
		Get().DumpHighLevelLoadTimes();
	}

	const TMap<FName, TArray<double>>& GetData() const
	{
		return TimeInfo;
	}

	void ResetHighLevelLoadTimes();

	/** Prints out raw load times for individual timers */
	void DumpRawLoadTimes() const;

	static void DumpRawLoadTimesStatic()
	{
		Get().DumpRawLoadTimes();
	}

	void ResetRawLoadTimes();

	static void ResetRawLoadTimesStatic()
	{
		Get().ResetRawLoadTimes();
	}

	void StartAccumulatedLoadTimes();

	static void StartAccumulatedLoadTimesStatic()
	{
		Get().StartAccumulatedLoadTimes();
	}

	void StopAccumulatedLoadTimes();

	static void StopAccumulatedLoadTimesStatic()
	{
		Get().StopAccumulatedLoadTimes();
	}

	bool IsAccumulating() { return bAccumulating; }

#if ENABLE_LOADTIME_RAW_TIMINGS

	/** Raw Timers */
	double CreateAsyncPackagesFromQueueTime;
	double ProcessAsyncLoadingTime;
	double ProcessLoadedPackagesTime;
	double SerializeTaggedPropertiesTime;
	double CreateLinkerTime;
	double FinishLinkerTime;
	double CreateImportsTime;
	double CreateExportsTime;
	double PreLoadObjectsTime;
	double PostLoadObjectsTime;
	double PostLoadDeferredObjectsTime;
	double FinishObjectsTime;
	double MaterialPostLoad;
	double MaterialInstancePostLoad;
	double SerializeInlineShaderMaps;
	double MaterialSerializeTime;
	double MaterialInstanceSerializeTime;
	double AsyncLoadingTime;
	double CreateMetaDataTime;

	double LinkerLoad_CreateLoader;
	double LinkerLoad_SerializePackageFileSummary;
	double LinkerLoad_SerializeNameMap;
	double LinkerLoad_SerializeGatherableTextDataMap;
	double LinkerLoad_SerializeImportMap;
	double LinkerLoad_SerializeExportMap;
	double LinkerLoad_FixupImportMap;
	double LinkerLoad_FixupExportMap;
	double LinkerLoad_SerializeDependsMap;
	double LinkerLoad_SerializePreloadDependencies;
	double LinkerLoad_CreateExportHash;
	double LinkerLoad_FindExistingExports;
	double LinkerLoad_FinalizeCreation;

	double Package_FinishLinker;
	double Package_LoadImports;
	double Package_CreateImports;
	double Package_CreateLinker;
	double Package_CreateExports;
	double Package_PreLoadObjects;
	double Package_ExternalReadDependencies;
	double Package_PostLoadObjects;
	double Package_Tick;
	double Package_CreateAsyncPackagesFromQueue;
	double Package_CreateMetaData;
	double Package_EventIOWait;

	double Package_Temp1;
	double Package_Temp2;
	double Package_Temp3;
	double Package_Temp4;

	double Graph_AddNode;
	uint32 Graph_AddNodeCnt;

	double Graph_AddArc;
	uint32 Graph_AddArcCnt;

	double Graph_RemoveNode;
	uint32 Graph_RemoveNodeCnt;

	double Graph_RemoveNodeFire;
	uint32 Graph_RemoveNodeFireCnt;

	double Graph_DoneAddingPrerequistesFireIfNone;
	uint32 Graph_DoneAddingPrerequistesFireIfNoneCnt;

	double Graph_DoneAddingPrerequistesFireIfNoneFire;
	uint32 Graph_DoneAddingPrerequistesFireIfNoneFireCnt;

	double Graph_Misc;
	uint32 Graph_MiscCnt;

	double TickAsyncLoading_ProcessLoadedPackages;


	double LinkerLoad_SerializeNameMap_ProcessingEntries;
#endif

private:
	TMap<FName, TArray<double>> TimeInfo;

	/** Track a time and count for a stat */
	struct FTimeAndCount
	{
		double Time;
		uint64 Count;
	};

	/** An accumulated stat group, with time and count for each instance */
	struct FAccumulatorTracker
	{
		TMap<FName, FTimeAndCount> TimeInfo;
	};

	/** Map to track accumulated timings */
	TMap<FName, FAccumulatorTracker> AccumulatedTimeInfo;

	/** We dont normally track accumulated load time info, only when this flag is true */
	bool bAccumulating;
private:
	FLoadTimeTracker();
};

/** Scoped helper class for tracking accumulated object times */
struct CORE_API FScopedLoadTimeAccumulatorTimer : public FScopedDurationTimer
{
	static double DummyTimer;

	FScopedLoadTimeAccumulatorTimer(const FName& InTimerName, const FName& InInstanceName);
};

#if ENABLE_LOADTIME_TRACKING
#define ACCUM_LOADTIME(TimerName, Time) FLoadTimeTracker::Get().ReportScopeTime(Time, FName(TimerName));
#else
#define ACCUM_LOADTIME(TimerName, Time)
#endif

#if ENABLE_LOADTIME_TRACKING
#define SCOPED_ACCUM_LOADTIME(TimerName, InstanceName) FScopedLoadTimeAccumulatorTimer AccumulatorTimer_##TimerName(FName(#TimerName), FName(InstanceName));
#else
#define SCOPED_ACCUM_LOADTIME(TimerName, InstanceName)
#endif

#if ENABLE_LOADTIME_RAW_TIMINGS
#define SCOPED_LOADTIMER(TimerName) FScopedDurationTimer DurationTimer_##TimerName(FLoadTimeTracker::Get().TimerName);
#define SCOPED_LOADTIMER_CNT(TimerName) FScopedDurationTimer DurationTimer_##TimerName(FLoadTimeTracker::Get().TimerName); FLoadTimeTracker::Get().TimerName##Cnt++;
#else
#define SCOPED_LOADTIMER(TimerName)
#define SCOPED_LOADTIMER_CNT(TimerName)
#endif

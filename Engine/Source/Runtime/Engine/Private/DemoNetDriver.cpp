// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UDemoNetDriver.cpp: Simulated network driver for recording and playing back game sessions.
=============================================================================*/


// @todo: LowLevelSend now includes the packet size in bits, but this is ignored locally.
//			Tracking of this must be added, if demos are to support PacketHandler's in the future (not presently needed).


#include "Engine/DemoNetDriver.h"
#include "EngineGlobals.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/DemoPendingNetGame.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetworkObjectList.h"
#include "Net/RepLayout.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/LevelStreamingDynamic.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "Net/UnrealNetwork.h"
#include "UnrealEngine.h"
#include "Net/NetworkProfiler.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/LowLevelMemTracker.h"
#include "Stats/StatsMisc.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Misc/EngineVersion.h"
#include "Stats/Stats2.h"

DEFINE_LOG_CATEGORY( LogDemo );

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);

static TAutoConsoleVariable<float> CVarDemoRecordHz( TEXT( "demo.RecordHz" ), 8, TEXT( "Maximum number of demo frames recorded per second" ) );
static TAutoConsoleVariable<float> CVarDemoMinRecordHz(TEXT("demo.MinRecordHz"), 0, TEXT("Minimum number of demo frames recorded per second (use with care)"));
static TAutoConsoleVariable<float> CVarDemoTimeDilation( TEXT( "demo.TimeDilation" ), -1.0f, TEXT( "Override time dilation during demo playback (-1 = don't override)" ) );
static TAutoConsoleVariable<float> CVarDemoSkipTime( TEXT( "demo.SkipTime" ), 0, TEXT( "Skip fixed amount of network replay time (in seconds)" ) );
static TAutoConsoleVariable<int32> CVarEnableCheckpoints( TEXT( "demo.EnableCheckpoints" ), 1, TEXT( "Whether or not checkpoints save on the server" ) );
static TAutoConsoleVariable<float> CVarGotoTimeInSeconds( TEXT( "demo.GotoTimeInSeconds" ), -1, TEXT( "For testing only, jump to a particular time" ) );
static TAutoConsoleVariable<int32> CVarDemoFastForwardDestroyTearOffActors( TEXT( "demo.FastForwardDestroyTearOffActors" ), 1, TEXT( "If true, the driver will destroy any torn-off actors immediately while fast-forwarding a replay." ) );
static TAutoConsoleVariable<int32> CVarDemoFastForwardSkipRepNotifies( TEXT( "demo.FastForwardSkipRepNotifies" ), 1, TEXT( "If true, the driver will optimize fast-forwarding by deferring calls to RepNotify functions until the fast-forward is complete. " ) );
static TAutoConsoleVariable<int32> CVarDemoQueueCheckpointChannels( TEXT( "demo.QueueCheckpointChannels" ), 1, TEXT( "If true, the driver will put all channels created during checkpoint loading into queuing mode, to amortize the cost of spawning new actors across multiple frames." ) );
static TAutoConsoleVariable<int32> CVarUseAdaptiveReplayUpdateFrequency( TEXT( "demo.UseAdaptiveReplayUpdateFrequency" ), 1, TEXT( "If 1, NetUpdateFrequency will be calculated based on how often actors actually write something when recording to a replay" ) );
static TAutoConsoleVariable<int32> CVarDemoAsyncLoadWorld( TEXT( "demo.AsyncLoadWorld" ), 0, TEXT( "If 1, we will use seamless server travel to load the replay world asynchronously" ) );
static TAutoConsoleVariable<float> CVarCheckpointUploadDelayInSeconds( TEXT( "demo.CheckpointUploadDelayInSeconds" ), 30.0f, TEXT( "" ) );
static TAutoConsoleVariable<int32> CVarDemoLoadCheckpointGarbageCollect( TEXT( "demo.LoadCheckpointGarbageCollect" ), 1, TEXT("If nonzero, CollectGarbage will be called during LoadCheckpoint after the old actors and connection are cleaned up." ) );
static TAutoConsoleVariable<float> CVarCheckpointSaveMaxMSPerFrameOverride( TEXT( "demo.CheckpointSaveMaxMSPerFrameOverride" ), -1.0f, TEXT( "If >= 0, this value will override the CheckpointSaveMaxMSPerFrame member variable, which is the maximum time allowed each frame to spend on saving a checkpoint. If 0, it will save the checkpoint in a single frame, regardless of how long it takes." ) );
static TAutoConsoleVariable<int32> CVarDemoClientRecordAsyncEndOfFrame( TEXT( "demo.ClientRecordAsyncEndOfFrame" ), 0, TEXT( "If true, TickFlush will be called on a thread in parallel with Slate." ) );
static TAutoConsoleVariable<int32> CVarForceDisableAsyncPackageMapLoading( TEXT( "demo.ForceDisableAsyncPackageMapLoading" ), 0, TEXT( "If true, async package map loading of network assets will be disabled." ) );
static TAutoConsoleVariable<int32> CVarDemoUseNetRelevancy( TEXT( "demo.UseNetRelevancy" ), 0, TEXT( "If 1, will enable relevancy checks and distance culling, using all connected clients as reference." ) );
static TAutoConsoleVariable<float> CVarDemoCullDistanceOverride( TEXT( "demo.CullDistanceOverride" ), 0.0f, TEXT( "If > 0, will represent distance from any viewer where actors will stop being recorded." ) );
static TAutoConsoleVariable<float> CVarDemoRecordHzWhenNotRelevant( TEXT( "demo.RecordHzWhenNotRelevant" ), 2.0f, TEXT( "Record at this frequency when actor is not relevant." ) );
static TAutoConsoleVariable<int32> CVarLoopDemo(TEXT("demo.Loop"), 0, TEXT("<1> : play replay from beginning once it reaches the end / <0> : stop replay at the end"));
static TAutoConsoleVariable<int32> CVarDemoFastForwardIgnoreRPCs( TEXT( "demo.FastForwardIgnoreRPCs" ), 1, TEXT( "If true, RPCs will be discarded during playback fast forward." ) );
static TAutoConsoleVariable<int32> CVarDemoLateActorDormancyCheck(TEXT("demo.LateActorDormancyCheck"), 1, TEXT("If true, check if an actor should become dormant as late as possible- when serializing it to the demo archive."));

static int32 GDemoSaveRollbackActorState = 1;
static FAutoConsoleVariableRef CVarDemoSaveRollbackActorState( TEXT( "demo.SaveRollbackActorState" ), GDemoSaveRollbackActorState, TEXT( "If true, rollback actors will save some replicated state to apply when respawned." ) );

static TAutoConsoleVariable<int32> CVarWithLevelStreamingFixes(TEXT("demo.WithLevelStreamingFixes"), 0, TEXT("If 1, provides fixes for level streaming (but breaks backwards compatibility)."));
static TAutoConsoleVariable<int32> CVarWithDemoTimeBurnIn(TEXT("demo.WithTimeBurnIn"), 0, TEXT("If true, adds an on screen message with the current DemoTime and Changelist."));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarDemoForceFailure( TEXT( "demo.ForceFailure" ), 0, TEXT( "" ) );
#endif

static TAutoConsoleVariable<float> CVarDemoIncreaseRepPrioritizeThreshold(TEXT("demo.IncreaseRepPrioritizeThreshold"), 0.9, TEXT("The % of Replicated to Prioritized actors at which prioritize time will be decreased."));
static TAutoConsoleVariable<float> CVarDemoDecreaseRepPrioritizeThreshold(TEXT("demo.DecreaseRepPrioritizeThreshold"), 0.7, TEXT("The % of Replicated to Prioritized actors at which prioritize time will be increased."));
static TAutoConsoleVariable<float> CVarDemoMinimumRepPrioritizeTime(TEXT("demo.MinimumRepPrioritizePercent"), 0.3, TEXT("Minimum percent of time that must be spent prioritizing actors, regardless of throttling."));
static TAutoConsoleVariable<float> CVarDemoMaximumRepPrioritizeTime(TEXT("demo.MaximumRepPrioritizePercent"), 0.8, TEXT("Maximum percent of time that may be spent prioritizing actors, regardless of throttling."));

static const int32 MAX_DEMO_READ_WRITE_BUFFER = 1024 * 2;

namespace ReplayTaskNames
{
	static FName SkipTimeInSecondsTask(TEXT("SkipTimeInSecondsTask"));
	static FName JumpToLiveReplayTask(TEXT("JumpToLiveReplayTask"));
	static FName GotoTimeInSecondsTask(TEXT("GotoTimeInSecondsTask"));
	static FName FastForwardLevelsTask(TEXT("FastForwardLevelsTask"));
};

#define DEMO_CHECKSUMS 0		// When setting this to 1, this will invalidate all demos, you will need to re-record and playback

// static delegates
FOnDemoStartedDelegate UDemoNetDriver::OnDemoStarted;
FOnDemoFailedToStartDelegate UDemoNetDriver::OnDemoFailedToStart;

// This is only intended for testing purposes
// A "better" way might be to throw together a GameplayDebuggerComponent or Category, so we could populate
// more than just the DemoTime.
static void ConditionallyDisplayBurnInTime(uint32 RecordedCL, float CurrentDemoTime)
{
	if (CVarWithDemoTimeBurnIn.GetValueOnAnyThread() != 0)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, FColor::Red, FString::Printf(TEXT("Current CL: %lu | Recorded CL: %lu | Time: %f"), FEngineVersion::Current().GetChangelist(), RecordedCL, CurrentDemoTime), true, FVector2D(3.f, 3.f));
	}
}

static void FlushNetChecked(UNetConnection& NetConnection)
{
	NetConnection.FlushNet();
	check(NetConnection.SendBuffer.GetNumBits() == 0);
}

static bool ShouldActorGoDormantForDemo(const AActor* Actor, const UActorChannel* Channel)
{
	if ( Actor->NetDormancy <= DORM_Awake || !Channel || Channel->bPendingDormancy || Channel->Dormant )
{
		// Either shouldn't go dormant, or is already dormant
		return false;
	}

	return true;
}

namespace DemoNetDriverRecordingPrivate
{
	static constexpr float WarningTimeInterval = 1.f;
	static double LastWarningTime = 0.f;

	template<size_t N, typename... T>
	FORCEINLINE static void LogDemoRecordTimeElapsed(TCHAR const (&Format)[N], T... Args)
	{
		if (UE_LOG_ACTIVE(LogDemo, Log))
		{
			const double Time = FPlatformTime::Seconds();
			if ((Time - LastWarningTime) > WarningTimeInterval)
			{
				UE_LOG(LogDemo, Log, Format, Args...);
				LastWarningTime = Time;
			}
		}
	}
}

// Helps manage packets, and any associations with streaming levels or exported GUIDs / fields.
class FScopedPacketManager
{
public:
	FScopedPacketManager(UDemoNetConnection& InConnection, const uint32 InSeenLevelIndex):
		Connection(InConnection),
		Packets(Connection.bResendAllDataSinceOpen ? InConnection.QueuedCheckpointPackets : InConnection.QueuedDemoPackets),
		SeenLevelIndex(InSeenLevelIndex)
	{
		FlushNetChecked(Connection);
		StartPacketCount = Packets.Num();
	}

	~FScopedPacketManager()
	{
		FlushNetChecked(Connection);
		AssociatePacketsWithLevel();
	}

private:

	void AssociatePacketsWithLevel()
	{
		for (int32 i = StartPacketCount; i < Packets.Num(); i++)
		{
			Packets[i].SeenLevelIndex = SeenLevelIndex;
		}
	}

	UDemoNetConnection& Connection;
	TArray<FQueuedDemoPacket>& Packets;
	const uint32 SeenLevelIndex;
	int32 StartPacketCount;
};

class FPendingTaskHelper
{
// TODO: Consider making these private, and adding explicit friend access for the tasks that need them.
public:

	static bool LoadCheckpoint(UDemoNetDriver* DemoNetDriver, const FGotoResult& GotoResult)
	{
		return DemoNetDriver->LoadCheckpoint(GotoResult);
	}

	static bool FastForwardLevels(UDemoNetDriver* DemoNetDriver, const FGotoResult& GotoResult)
	{
		return DemoNetDriver->FastForwardLevels(GotoResult);
	}

	static float GetLastProcessedPacketTime(UDemoNetDriver* DemoNetDriver)
	{
		return DemoNetDriver->LastProcessedPacketTime;
	}
};

/**
 * Helps track Offsets in an Archive before the actual size of the offset is known.
 * This relies on serialization always used a fixed number of bytes for primitive types,
 * and Sane implementations of Seek and Tell.
 */
struct FScopedStoreArchiveOffset
{
	typedef UDemoNetDriver::FArchivePos FArchivePos;

	FScopedStoreArchiveOffset(FArchive& InAr):
		Ar(InAr),
		StartPosition(Ar.Tell())
	{
		// Save room for the offset here.
		FArchivePos TempOffset = 0;
		Ar << TempOffset;
	}

	~FScopedStoreArchiveOffset()
	{
		const FArchivePos CurrentPosition = Ar.Tell();
		FArchivePos Offset = CurrentPosition - (StartPosition + sizeof(FArchivePos));
		Ar.Seek(StartPosition);
		Ar << Offset;
		Ar.Seek(CurrentPosition);
	}

private:

	FArchive& Ar;
	const FArchivePos StartPosition;
};

class FJumpToLiveReplayTask : public FQueuedReplayTask
{
public:
	FJumpToLiveReplayTask( UDemoNetDriver* InDriver ) : FQueuedReplayTask( InDriver )
	{
		if (Driver.IsValid())
		{
		InitialTotalDemoTime	= Driver->ReplayStreamer->GetTotalDemoTime();
		TaskStartTime			= FPlatformTime::Seconds();
	}
	}

	virtual void StartTask()
	{
	}

	virtual bool Tick() override
	{
		if (!Driver.IsValid())
		{
			return true;
		}

		if ( !Driver->ReplayStreamer->IsLive() )
		{
			// The replay is no longer live, so don't try to jump to end
			return true;
		}

		// Wait for the most recent live time
		const bool bHasNewReplayTime = ( Driver->ReplayStreamer->GetTotalDemoTime() != InitialTotalDemoTime );

		// If we haven't gotten a new time from the demo by now, assume it might not be live, and just jump to the end now so we don't hang forever
		const bool bTimeExpired = ( FPlatformTime::Seconds() - TaskStartTime >= 15 );

		if ( bHasNewReplayTime || bTimeExpired )
		{
			if ( bTimeExpired )
			{
				UE_LOG( LogDemo, Warning, TEXT( "FJumpToLiveReplayTask::Tick: Too much time since last live update." ) );
			}

			// We're ready to jump to the end now
			Driver->JumpToEndOfLiveReplay();
			return true;
		}

		// Waiting to get the latest update
		return false;
	}

	virtual FName GetName() const override
	{
		return ReplayTaskNames::JumpToLiveReplayTask;
	}

	uint32	InitialTotalDemoTime;		// Initial total demo time. This is used to wait until we get a more updated time so we jump to the most recent end time
	double	TaskStartTime;				// This is the time the task started. If too much real-time passes, we'll just jump to the current end
};

class FGotoTimeInSecondsTask : public FQueuedReplayTask
{
public:
	FGotoTimeInSecondsTask(UDemoNetDriver* InDriver, const float InTimeInSeconds) : FQueuedReplayTask(InDriver), TimeInSeconds(InTimeInSeconds)
	{
	}

	virtual void StartTask() override
	{		
		if (!Driver.IsValid())
		{
			return;
		}

		check(!GotoResult.IsSet());
		check(!Driver->IsFastForwarding());

		OldTimeInSeconds = Driver->DemoCurrentTime;	// Rember current time, so we can restore on failure
		Driver->DemoCurrentTime = TimeInSeconds;	// Also, update current time so HUD reflects desired scrub time now

		// Clamp time
		Driver->DemoCurrentTime = FMath::Clamp(Driver->DemoCurrentTime, 0.0f, Driver->DemoTotalTime - 0.01f);

		// Tell the streamer to start going to this time
		Driver->ReplayStreamer->GotoTimeInMS(Driver->DemoCurrentTime * 1000, FGotoCallback::CreateSP(this, &FGotoTimeInSecondsTask::CheckpointReady));

		// Pause channels while we wait (so the world is paused while we wait for the new stream location to load)
		Driver->PauseChannels( true );
	}

	virtual bool Tick() override
	{
		if (!Driver.IsValid())
		{
			// Detect failure case
			return true;
		}
		else if (GotoResult.IsSet())
		{
			if (!GotoResult->WasSuccessful())
			{
				return true;
			}
			else if (GotoResult->ExtraTimeMS > 0 && !Driver->ReplayStreamer->IsDataAvailable())
			{
				// Wait for rest of stream before loading checkpoint
				// We do this so we can load the checkpoint and fastforward the stream all at once
				// We do this so that the OnReps don't stay queued up outside of this frame
				return false;
			}

			// We're done
			return FPendingTaskHelper::LoadCheckpoint(Driver.Get(), GotoResult.GetValue());
		}

		return false;
	}

	virtual FName GetName() const override
	{
		return ReplayTaskNames::GotoTimeInSecondsTask;
	}

	void CheckpointReady(const FGotoResult& Result)
	{
		check(!GotoResult.IsSet());
		GotoResult = Result;

		if (!Driver.IsValid())
	{
			return;
		}

		if (!Result.WasSuccessful())
		{
			UE_LOG(LogDemo, Warning, TEXT("FGotoTimeInSecondsTask::CheckpointReady: Failed to go to checkpoint."));

			// Restore old demo time
			Driver->DemoCurrentTime = OldTimeInSeconds;

			// Call delegate if any
			Driver->NotifyGotoTimeFinished(false);
		}
	}

	// So we can restore on failure
	float OldTimeInSeconds;		
	float TimeInSeconds;
	TOptional<FGotoResult> GotoResult;
};

class FSkipTimeInSecondsTask : public FQueuedReplayTask
{
public:
	FSkipTimeInSecondsTask( UDemoNetDriver* InDriver, const float InSecondsToSkip ) : FQueuedReplayTask( InDriver ), SecondsToSkip( InSecondsToSkip )
	{
	}

	virtual void StartTask() override
	{
		if (!Driver.IsValid())
		{
			return;
		}

		check( !Driver->IsFastForwarding() );

		const uint32 TimeInMSToCheck = FMath::Clamp( Driver->GetDemoCurrentTimeInMS() + ( uint32 )( SecondsToSkip * 1000 ), ( uint32 )0, Driver->ReplayStreamer->GetTotalDemoTime() );

		Driver->ReplayStreamer->SetHighPriorityTimeRange( Driver->GetDemoCurrentTimeInMS(), TimeInMSToCheck );

		Driver->SkipTimeInternal( SecondsToSkip, true, false );
	}

	virtual bool Tick() override
	{
		// The real work was done in StartTask, so we're done
		return true;
	}

	virtual FName GetName() const override
	{
		return ReplayTaskNames::SkipTimeInSecondsTask;
	}

	float SecondsToSkip;
};

class FFastForwardLevelsTask : public FQueuedReplayTask
{
public:

	FFastForwardLevelsTask( UDemoNetDriver* InDriver ) : FQueuedReplayTask( InDriver ), GotoTime(0), bSkipWork(false)
	{
	}

	virtual void StartTask() override
	{
		if (!Driver.IsValid())
		{
			return;
		}

		check(!Driver->IsFastForwarding());

		// If there's a GotoTimeInSeconds task pending, we don't need to do any work.
		// That task should trigger a full checkpoint load.
		// Only check the next task, to avoid issues with SkipTime / JumpToLive not having updated levels.
		if (Driver->GetNextQueuedTaskName() == ReplayTaskNames::GotoTimeInSecondsTask)
		{
			bSkipWork = true;
		}
		else
		{
			// Make sure we request all the data we need so we don't end up doing a "partial" fast forward which
			// could cause the level to miss network updates.
			const float LastProcessedPacketTime = FPendingTaskHelper::GetLastProcessedPacketTime(Driver.Get());
			GotoTime = LastProcessedPacketTime * 1000;

			Driver->ReplayStreamer->GotoTimeInMS(GotoTime, FGotoCallback::CreateSP(this, &FFastForwardLevelsTask::CheckpointReady));

			// Pause channels while we wait (so the world is paused while we wait for the new stream location to load)
			Driver->PauseChannels(true);
		}
	}

	virtual bool Tick() override
	{
		if (bSkipWork)
		{
			return true;
		}
		else if (!Driver.IsValid())
		{
			return true;
		}
		else if (GotoResult.IsSet())
		{
			if (!GotoResult->WasSuccessful())
			{
				return true;
			}
		
			// If not all data is available, we could end only partially fast forwarding the levels.
			// Note, IsDataAvailable may return false even if IsDataAvailableForTimeRange is true.
			// So, check both to ensure that we don't end up skipping data in FastForwardLevels.
			else if (GotoResult->ExtraTimeMS > 0 && !(Driver->ReplayStreamer->IsDataAvailable() && Driver->ReplayStreamer->IsDataAvailableForTimeRange(GotoTime - GotoResult->ExtraTimeMS, GotoTime)))
			{
				return false;
			}

			return FPendingTaskHelper::FastForwardLevels(Driver.Get(), GotoResult.GetValue());
		}

		return false;
	}

	virtual FName GetName() const override
	{
		return ReplayTaskNames::FastForwardLevelsTask;
	}

	void CheckpointReady(const FGotoResult& Result)
	{
		check(!GotoResult.IsSet());

		GotoResult = Result;

		if (!Result.WasSuccessful())
		{
			UE_LOG( LogDemo, Warning, TEXT( "FFastForwardLevelsTask::CheckpointReady: Faled to get checkpoint." ) );
		}
	}

private:

	uint32 GotoTime;
	bool bSkipWork;
	TOptional<FGotoResult> GotoResult;
};

class FScopedForceUnicodeInArchive
{
public:
	FScopedForceUnicodeInArchive(FArchive& InArchive)
		: Archive(InArchive)
		, bWasUnicode(InArchive.IsForcingUnicode())
	{
		EnableFastStringSerialization();
	}

	~FScopedForceUnicodeInArchive()
	{
		RestoreStringSerialization();
	}

private:
	void EnableFastStringSerialization()
	{
		if (FPlatformString::TAreEncodingsCompatible<WIDECHAR, TCHAR>::Value)
		{
			Archive.SetForceUnicode(true);
		}
	}

	void RestoreStringSerialization()
	{
		if (FPlatformString::TAreEncodingsCompatible<WIDECHAR, TCHAR>::Value)
		{
			Archive.SetForceUnicode(bWasUnicode);
		}
	}

	FArchive& Archive;
	bool bWasUnicode;
};

/*-----------------------------------------------------------------------------
	UDemoNetDriver.
-----------------------------------------------------------------------------*/

UDemoNetDriver::UDemoNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DemoSessionID(FGuid::NewGuid().ToString().ToLower())
{
	CurrentLevelIndex = 0;
	bRecordMapChanges = false;
	bIsWaitingForHeaderDownload = false;
	bIsWaitingForStream = false;

	LevelIntervals.Reserve(512);

	RecordBuildConsiderAndPrioritizeTimeSlice = CVarDemoMaximumRepPrioritizeTime.GetValueOnGameThread();
}

FString UDemoNetDriver::GetLevelPackageName(const ULevel& InLevel)
{
	FString PathName = InLevel.GetOutermost()->GetFName().ToString();
	return UWorld::RemovePIEPrefix(PathName);
}

void UDemoNetDriver::AddReplayTask( FQueuedReplayTask* NewTask )
{
	UE_LOG( LogDemo, Verbose, TEXT( "UDemoNetDriver::AddReplayTask. Name: %s" ), *NewTask->GetName().ToString() );

	QueuedReplayTasks.Add( TSharedPtr< FQueuedReplayTask >( NewTask ) );

	// Give this task a chance to immediately start if nothing else is happening
	if ( !IsAnyTaskPending() )
	{
		ProcessReplayTasks();	
	}
}

bool UDemoNetDriver::IsAnyTaskPending() const
{
	return ( QueuedReplayTasks.Num() > 0 ) || ActiveReplayTask.IsValid();
}

void UDemoNetDriver::ClearReplayTasks()
{
	QueuedReplayTasks.Empty();

	ActiveReplayTask = nullptr;
}

bool UDemoNetDriver::ProcessReplayTasks()
{
	// Store a shared pointer to the current task in a local variable so that if
	// the task itself causes tasks to be cleared (for example, if it calls StopDemo()
	// in StartTask() or Tick()), the current task won't be destroyed immediately.
	TSharedPtr<FQueuedReplayTask> LocalActiveTask;

	if ( !ActiveReplayTask.IsValid() && QueuedReplayTasks.Num() > 0 )
	{
		// If we don't have an active task, pull one off now
		ActiveReplayTask = QueuedReplayTasks[0];
		LocalActiveTask = ActiveReplayTask;
		QueuedReplayTasks.RemoveAt( 0 );

		UE_LOG( LogDemo, Verbose, TEXT( "UDemoNetDriver::ProcessReplayTasks. Name: %s" ), *ActiveReplayTask->GetName().ToString() );

		// Start the task
		ActiveReplayTask->StartTask();
	}

	// Tick the currently active task
	if ( ActiveReplayTask.IsValid() )
	{
		if ( !ActiveReplayTask->Tick() )
		{
			// Task isn't done, we can return
			return false;
		}

		// This task is now done
		ActiveReplayTask = nullptr;
	}

	return true;	// No tasks to process
}

bool UDemoNetDriver::IsNamedTaskInQueue( const FName& Name ) const
{
	if ( ActiveReplayTask.IsValid() && ActiveReplayTask->GetName() == Name )
	{
		return true;
	}

	for ( int32 i = 0; i < QueuedReplayTasks.Num(); i++ )
	{
		if ( QueuedReplayTasks[i]->GetName() == Name )
		{
			return true;
		}
	}

	return false;
}

FName UDemoNetDriver::GetNextQueuedTaskName() const
{
	return QueuedReplayTasks.Num() > 0 ? QueuedReplayTasks[0]->GetName() : NAME_None;
}

bool UDemoNetDriver::InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error )
{
	if ( Super::InitBase( bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error ) )
	{
		DemoURL							= URL;
		Time							= 0;
		bDemoPlaybackDone				= false;
		bChannelsArePaused				= false;
		bIsFastForwarding				= false;
		bIsFastForwardingForCheckpoint	= false;
		bWasStartStreamingSuccessful	= true;
		SavedReplicatedWorldTimeSeconds	= 0.0f;
		SavedSecondsToSkip				= 0.0f;
		bIsLoadingCheckpoint			= false;
		MaxDesiredRecordTimeMS			= -1.0f;
		ViewerOverride					= nullptr;
		bPrioritizeActors				= false;
		bPauseRecording					= false;
		PlaybackPacketIndex				= 0;
		CheckpointSaveMaxMSPerFrame		= -1.0f;
		RecordBuildConsiderAndPrioritizeTimeSlice = CVarDemoMaximumRepPrioritizeTime.GetValueOnAnyThread();

		if ( RelevantTimeout == 0.0f )
		{
			RelevantTimeout = 5.0f;
		}

		ResetDemoState();

		const TCHAR* const StreamerOverride = URL.GetOption(TEXT("ReplayStreamerOverride="), nullptr);
		ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory(StreamerOverride).CreateReplayStreamer();

		const TCHAR* const DemoPath = URL.GetOption(TEXT("ReplayStreamerDemoPath="), nullptr);
		if (DemoPath != nullptr && ReplayStreamer.IsValid())
		{
			ReplayStreamer->SetDemoPath(DemoPath);
		}

		return true;
	}

	return false;
}

void UDemoNetDriver::FinishDestroy()
{
	if ( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		// Make sure we stop any recording/playing that might be going on
		if ( IsRecording() || IsPlaying() )
		{
			StopDemo();
		}
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::FinishDestroy();
}

FString UDemoNetDriver::LowLevelGetNetworkNumber()
{
	return FString( TEXT( "" ) );
}

void UDemoNetDriver::ResetDemoState()
{
	DemoFrameNum			= 0;
	LastCheckpointTime		= 0.0f;
	DemoTotalTime			= 0;
	DemoCurrentTime			= 0;
	DemoTotalFrames			= 0;
	LatestReadFrameTime		= 0.0f;
	LastProcessedPacketTime	= 0.0f;
	PlaybackPacketIndex		= 0;

	bIsFastForwarding = false;
	bIsFastForwardingForCheckpoint = false;
	bWasStartStreamingSuccessful = false;
	bIsLoadingCheckpoint = false;
	bIsWaitingForHeaderDownload = false;
	bIsWaitingForStream = false;

	ExternalDataToObjectMap.Empty();
	PlaybackPackets.Empty();
	ClearLevelStreamingState();
}

bool UDemoNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	if ( GetWorld() == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "GetWorld() == nullptr" ) );
		return false;
	}

	if ( GetWorld()->GetGameInstance() == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "GetWorld()->GetGameInstance() == nullptr" ) );
		return false;
	}

	// handle default initialization
	if ( !InitBase( true, InNotify, ConnectURL, false, Error ) )
	{
		GetWorld()->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::InitBase, FString( TEXT( "InitBase FAILED" ) ) );
		return false;
	}

	GuidCache->SetNetworkChecksumMode( FNetGUIDCache::ENetworkChecksumMode::SaveButIgnore );

	if ( CVarForceDisableAsyncPackageMapLoading.GetValueOnGameThread() > 0 )
	{
		GuidCache->SetAsyncLoadMode( FNetGUIDCache::EAsyncLoadMode::ForceDisable );
	}
	else
	{
		GuidCache->SetAsyncLoadMode( FNetGUIDCache::EAsyncLoadMode::UseCVar );
	}

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), UDemoNetConnection::StaticClass());
	ServerConnection->InitConnection( this, USOCK_Pending, ConnectURL, 1000000 );

	TArray< FString > UserNames;

	if ( GetWorld()->GetGameInstance()->GetFirstGamePlayer() != nullptr )
	{
		FUniqueNetIdRepl ViewerId = GetWorld()->GetGameInstance()->GetFirstGamePlayer()->GetPreferredUniqueNetId();

		if ( ViewerId.IsValid() )
		{ 
			UserNames.Add( ViewerId.ToString() );
		}
	}

	const TCHAR* const LevelPrefixOverrideOption = DemoURL.GetOption(TEXT("LevelPrefixOverride="), nullptr);
	if (LevelPrefixOverrideOption)
	{
		SetDuplicateLevelID(FCString::Atoi(LevelPrefixOverrideOption));
	}

	if ( GetDuplicateLevelID() == -1 )
	{
		// Set this driver as the demo net driver for the source level collection.
		FLevelCollection* const SourceCollection = GetWorld()->FindCollectionByType( ELevelCollectionType::DynamicSourceLevels );
		if ( SourceCollection )
		{
			SourceCollection->SetDemoNetDriver( this );
		}
	}
	else
	{
		// Set this driver as the demo net driver for the duplicate level collection.
		FLevelCollection* const DuplicateCollection = GetWorld()->FindCollectionByType( ELevelCollectionType::DynamicDuplicatedLevels );
		if ( DuplicateCollection )
		{
			DuplicateCollection->SetDemoNetDriver( this );
		}
	}

	bIsWaitingForStream = true;
	bWasStartStreamingSuccessful = true;

	ActiveReplayName = DemoURL.Map;
	ReplayStreamer->StartStreaming( 
		DemoURL.Map, 
		FString(),		// Friendly name isn't important for loading an existing replay.
		UserNames, 
		false, 
		FNetworkVersion::GetReplayVersion(), 
		FStartStreamingCallback::CreateUObject( this, &UDemoNetDriver::ReplayStreamingReady ) );

	return bWasStartStreamingSuccessful;
}

bool UDemoNetDriver::ReadPlaybackDemoHeader(FString& Error)
{
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();

	PlaybackDemoHeader = FNetworkDemoHeader();

	FArchive* FileAr = ReplayStreamer->GetHeaderArchive();

	if ( !FileAr )
	{
		Error = FString::Printf( TEXT( "Couldn't open demo file %s for reading" ), *DemoURL.Map );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPlaybackDemoHeader: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::DemoNotFound, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::DemoNotFound ) ) );
		return false;
	}

	(*FileAr) << PlaybackDemoHeader;

	if ( FileAr->IsError() )
	{
		Error = FString( TEXT( "Demo file is corrupt" ) );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPlaybackDemoHeader: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Corrupt, Error );
		return false;
	}
	
	// Check whether or not we need to process streaming level fixes.
	bHasLevelStreamingFixes = !!(PlaybackDemoHeader.HeaderFlags & EReplayHeaderFlags::HasStreamingFixes);

	// Set network version on connection
	ServerConnection->EngineNetworkProtocolVersion = PlaybackDemoHeader.EngineNetworkProtocolVersion;
	ServerConnection->GameNetworkProtocolVersion = PlaybackDemoHeader.GameNetworkProtocolVersion;

	if (!ProcessGameSpecificDemoHeader(PlaybackDemoHeader.GameSpecificData, Error))
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::InitConnect: (Game Specific) %s"), *Error);
		GameInstance->HandleDemoPlaybackFailure(EDemoPlayFailure::GameSpecificHeader, Error);
		return false;
	}

	return true;
}

bool UDemoNetDriver::InitConnectInternal( FString& Error )
{
	ResetDemoState();

	if (!ReadPlaybackDemoHeader(Error))
	{
		return false;
	}

	// Create fake control channel
	CreateInitialClientChannels();
	
	// Default async world loading to the cvar value...
	bool bAsyncLoadWorld = CVarDemoAsyncLoadWorld.GetValueOnGameThread() > 0;

	// ...but allow it to be overridden via a command-line option.
	const TCHAR* const AsyncLoadWorldOverrideOption = DemoURL.GetOption(TEXT("AsyncLoadWorldOverride="), nullptr);
	if (AsyncLoadWorldOverrideOption)
	{
		bAsyncLoadWorld = FCString::ToBool(AsyncLoadWorldOverrideOption);
	}

	// Hook up to get notifications so we know when a travel is complete (LoadMap or Seamless).
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnPostLoadMapWithWorld);

	if (GetDuplicateLevelID() == -1)
	{
		if ( bAsyncLoadWorld && GetWorld()->WorldType != EWorldType::PIE ) // Editor doesn't support async map travel
		{
			LevelNamesAndTimes = PlaybackDemoHeader.LevelNamesAndTimes;

			// FIXME: Test for failure!!!
			ProcessSeamlessTravel(0);
		}
		else
		{
			// Bypass UDemoPendingNetLevel
			FString LoadMapError;

			FURL LocalDemoURL;
			LocalDemoURL.Map = PlaybackDemoHeader.LevelNamesAndTimes[0].LevelName;

			FWorldContext * WorldContext = GEngine->GetWorldContextFromWorld( GetWorld() );

			if ( WorldContext == nullptr )
			{
				UGameInstance* GameInstance = GetWorld()->GetGameInstance();

				Error = FString::Printf( TEXT( "No world context" ) );
				UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
				GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "No world context" ) ) );
				return false;
			}

			GetWorld()->DemoNetDriver = nullptr;
			SetWorld( nullptr );

			auto NewPendingNetGame = NewObject<UDemoPendingNetGame>();

			// Set up the pending net game so that the engine can call LoadMap on the next tick.
			NewPendingNetGame->DemoNetDriver = this;
			NewPendingNetGame->URL = LocalDemoURL;
			NewPendingNetGame->bSuccessfullyConnected = true;

			WorldContext->PendingNetGame = NewPendingNetGame;
		}
	}
	else
	{
		ResetLevelStatuses();
	}

	return true;
}

bool UDemoNetDriver::InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error )
{
	if ( !InitBase( false, InNotify, ListenURL, bReuseAddressAndPort, Error ) )
	{
		return false;
	}

	GuidCache->SetNetworkChecksumMode( FNetGUIDCache::ENetworkChecksumMode::SaveButIgnore );

	check( World != nullptr );

	class AWorldSettings * WorldSettings = World->GetWorldSettings(); 

	if ( !WorldSettings )
	{
		Error = TEXT( "No WorldSettings!!" );
		return false;
	}

	// We'll only check these CVars here, because we don't want to break the replay if they change part way through recording.
	// During playback the CVars won't be used. Instead, we'll rely on the DemoPacketHeader value.
	bHasLevelStreamingFixes = !!CVarWithLevelStreamingFixes.GetValueOnAnyThread();

	// Recording, local machine is server, demo stream acts "as if" it's a client.
	UDemoNetConnection* Connection = NewObject<UDemoNetConnection>();
	Connection->InitConnection( this, USOCK_Open, ListenURL, 1000000 );
	Connection->InitSendBuffer();

	AddClientConnection( Connection );

	const TCHAR* FriendlyNameOption = ListenURL.GetOption( TEXT("DemoFriendlyName="), nullptr );

	bRecordMapChanges = ListenURL.GetOption(TEXT("RecordMapChanges"), nullptr) != nullptr;

	TArray< FString > UserNames;
	AGameStateBase* GameState = GetWorld()->GetGameState();

	// If a client is recording a replay, GameState may not have replicated yet
	if (GameState != nullptr )
	{
		for ( int32 i = 0; i < GameState->PlayerArray.Num(); i++ )
		{
			APlayerState* PlayerState = GameState->PlayerArray[i];
			if ( PlayerState && !PlayerState->bIsABot && !PlayerState->bIsSpectator )
			{
				UserNames.Add( PlayerState->UniqueId.ToString() );
			}
		}
	}

	bIsWaitingForStream = true;

	ActiveReplayName = DemoURL.Map;
	ReplayStreamer->StartStreaming(
		DemoURL.Map,
		FriendlyNameOption != nullptr ? FString(FriendlyNameOption) : World->GetMapName(),
		UserNames,
		true,
		FNetworkVersion::GetReplayVersion(),
		FStartStreamingCallback::CreateUObject( this, &UDemoNetDriver::ReplayStreamingReady ) );

	AddNewLevel(World->GetOuter()->GetName());

	bool Result = WriteNetworkDemoHeader(Error);

	// Spawn the demo recording spectator.
	SpawnDemoRecSpectator( Connection, ListenURL );

	return Result;
}

void UDemoNetDriver::OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
	Super::OnLevelAddedToWorld(InLevel, InWorld);

	if (InLevel && !InLevel->bClientOnlyVisible && GetWorld() == InWorld && HasLevelStreamingFixes() && IsPlaying())
	{
		if (!NewStreamingLevelsThisFrame.Contains(InLevel) && !LevelsPendingFastForward.Contains(InLevel))
		{
			FLevelStatus& LevelStatus = FindOrAddLevelStatus(*InLevel);

			// If we haven't processed any packets for this level yet, immediately mark it as ready.
			if (!LevelStatus.bHasBeenSeen)
			{
				LevelStatus.bIsReady = true;
			}

			// If the level isn't ready, go ahead and queue it up to get fast-forwarded.
			// Note, we explicitly check the visible flag in case same the level gets notified multiple times.
			else if (!LevelStatus.bIsReady)
			{
				NewStreamingLevelsThisFrame.Add(InLevel);
			}
		}
	}
}

void UDemoNetDriver::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);

	if (InLevel && !InLevel->bClientOnlyVisible && GetWorld() == InWorld && HasLevelStreamingFixes() && IsPlaying())
	{
		const FString LevelPackageName = GetLevelPackageName(*InLevel);
		if (LevelStatusesByName.Contains(LevelPackageName))
		{
			FLevelStatus& LevelStatus = GetLevelStatus(LevelPackageName);
			LevelStatus.bIsReady = false;

			// Make sure we don't try to fast-forward this level later.
			LevelsPendingFastForward.Remove(InLevel);
			NewStreamingLevelsThisFrame.Remove(InLevel);
		}
	}

	// always invalidate cache since it uses pointers
	LevelStatusIndexByLevel.Remove(InLevel);
}

void UDemoNetDriver::NotifyStreamingLevelUnload( ULevel* InLevel )
{
	if (InLevel && !InLevel->bClientOnlyVisible && HasLevelStreamingFixes() && IsPlaying())
	{
			// We can't just iterate over the levels actors, because the ones in the queue will already have been destroyed.
			for (TMap<FString, FRollbackNetStartupActorInfo>::TIterator It = RollbackNetStartupActors.CreateIterator(); It; ++It)
			{
				if (It.Value().Level == InLevel)
				{
					It.RemoveCurrent();
				}
			}
		}

	Super::NotifyStreamingLevelUnload(InLevel);
}

void UDemoNetDriver::OnPostLoadMapWithWorld(UWorld* InWorld)
{
	if (InWorld != nullptr && InWorld == World && HasLevelStreamingFixes())
	{
		if (IsPlaying())
		{
			ResetLevelStatuses();
		}
		else
		{
			ClearLevelStreamingState();
		}
	}
}

TUniquePtr<FScopedPacketManager> UDemoNetDriver::ConditionallyCreatePacketManager(ULevel& Level)
{
	if(IsRecording() && HasLevelStreamingFixes())
	{
		// Indices need to be 1 based, so +1.
		return MakeUnique<FScopedPacketManager>(*(UDemoNetConnection*)ClientConnections[0], FindOrAddLevelStatus(Level).LevelIndex + 1);
	}

	return nullptr;
}

TUniquePtr<FScopedPacketManager> UDemoNetDriver::ConditionallyCreatePacketManager(int32 LevelIndex)
{
	if(IsRecording() && HasLevelStreamingFixes())
	{
		// Indices need to be 1 based, so +1.
		return MakeUnique<FScopedPacketManager>(*(UDemoNetConnection*)ClientConnections[0], LevelIndex);
	}

	return nullptr;
}

void UDemoNetDriver::ResetLevelStatuses()
{
	ClearLevelStreamingState();

	// There are times (e.g., during travel) when we may not have a valid level.
	// This **should never** be called during those times.
	check(World);

	// ResetLevelStatuses should only ever be called before receiving *any* data from the Replay stream,
	// immediately before processing checkpoint data, or after a level transistion (in which case no data
	// will be relevant to the new sublevels).
	// In any case, we can just flag these sublevels as ready immediately.
	FindOrAddLevelStatus(*(World->PersistentLevel)).bIsReady = true;
	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		if (LevelStreaming && LevelStreaming->IsLevelVisible())
		{
			FindOrAddLevelStatus(*LevelStreaming->GetLoadedLevel()).bIsReady = true;
		}
	}
}

bool UDemoNetDriver::ContinueListen(FURL& ListenURL)
{
	if (IsRecording() && ensure(IsRecordingPaused()))
	{
		++CurrentLevelIndex;

		PauseRecording(false);

		// Delete the old player controller, we're going to create a new one (and we can't leave this one hanging around)
		if (SpectatorController != nullptr)
		{
			SpectatorController->Player = nullptr;		// Force APlayerController::DestroyNetworkActorHandled to return false
			World->DestroyActor(SpectatorController, true);
			SpectatorController = nullptr;
		}

		SpawnDemoRecSpectator(ClientConnections[0], ListenURL);

		// Force a checkpoint to be created in the next tick - We need a checkpoint right after travelling so that scrubbing
		// from a different level will have essentially an "empty" checkpoint to work from.
		LastCheckpointTime = -1 * CVarCheckpointUploadDelayInSeconds.GetValueOnGameThread();
		return true;
	}
	return false;
}

bool UDemoNetDriver::WriteNetworkDemoHeader(FString& Error)
{
	FArchive* FileAr = ReplayStreamer->GetHeaderArchive();

	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for writing"), *DemoURL.Map );//@todo demorec: localize
		return false;
	}

	FNetworkDemoHeader DemoHeader;

	DemoHeader.LevelNamesAndTimes = LevelNamesAndTimes;

	WriteGameSpecificDemoHeader(DemoHeader.GameSpecificData);

	if (World && World->IsRecordingClientReplay())
	{
		DemoHeader.HeaderFlags |= EReplayHeaderFlags::ClientRecorded;
	}
	if (HasLevelStreamingFixes())
	{
		DemoHeader.HeaderFlags |= EReplayHeaderFlags::HasStreamingFixes;
	}

	DemoHeader.Guid = FGuid::NewGuid();

	// Write the header
	(*FileAr) << DemoHeader;
	FileAr->Flush();

	return true;
}

void UDemoNetDriver::WriteGameSpecificDemoHeader(TArray<FString>& GameSpecificData)
{
	FNetworkReplayDelegates::OnWriteGameSpecificDemoHeader.Broadcast(GameSpecificData);
}

bool UDemoNetDriver::ProcessGameSpecificDemoHeader(const TArray<FString>& GameSpecificData, FString& Error)
{
	FNetworkReplayDelegates::OnProcessGameSpecificDemoHeader.Broadcast(GameSpecificData, Error);
	return Error.Len() == 0;
}

bool UDemoNetDriver::IsRecording() const
{
	return ClientConnections.Num() > 0 && ClientConnections[0] != nullptr && ClientConnections[0]->State != USOCK_Closed;
}

bool UDemoNetDriver::IsPlaying() const
{
	// ServerConnection may be deleted / recreated during checkpoint loading.
	return IsLoadingCheckpoint() || (ServerConnection != nullptr && ServerConnection->State != USOCK_Closed);
}

bool UDemoNetDriver::IsServer() const
{
	return (ServerConnection == nullptr) || IsRecording();
}

bool UDemoNetDriver::ShouldTickFlushAsyncEndOfFrame() const
{
	return	GEngine && GEngine->ShouldDoAsyncEndOfFrameTasks() && CVarDemoClientRecordAsyncEndOfFrame.GetValueOnAnyThread() != 0 && World && World->IsRecordingClientReplay();
}

void UDemoNetDriver::TickFlush(float DeltaSeconds)
{
	if (!ShouldTickFlushAsyncEndOfFrame())
	{
		TickFlushInternal(DeltaSeconds);
	}
}

void UDemoNetDriver::TickFlushAsyncEndOfFrame(float DeltaSeconds)
{
	if (ShouldTickFlushAsyncEndOfFrame())
	{
		TickFlushInternal(DeltaSeconds);
	}
}

/** Accounts for the network time we spent in the demo driver. */
double GTickFlushDemoDriverTimeSeconds = 0.0;

void UDemoNetDriver::TickFlushInternal( float DeltaSeconds )
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(DemoRecording);

	GTickFlushDemoDriverTimeSeconds = 0.0;
	FSimpleScopeSecondsCounter ScopedTimer(GTickFlushDemoDriverTimeSeconds);

	// Set the context on the world for this driver's level collection.
	const int32 FoundCollectionIndex = World ? World->GetLevelCollections().IndexOfByPredicate([this](const FLevelCollection& Collection)
	{
		return Collection.GetDemoNetDriver() == this;
	}) : INDEX_NONE;

	FScopedLevelCollectionContextSwitch LCSwitch(FoundCollectionIndex, GetWorld());

	Super::TickFlush( DeltaSeconds );

	if (!IsRecording() || bIsWaitingForStream)
	{
		// Nothing to do
		return;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickFlush: ReplayStreamer ERROR: %s" ), ENetworkReplayError::ToString( ReplayStreamer->GetLastError() ) );
		StopDemo();
		return;
	}

	if (bPauseRecording)
	{
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickFlush: FileAr == nullptr" ) );
		StopDemo();
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "Net replay record time" ), STAT_ReplayRecordTime, STATGROUP_Net );

	const double StartTime = FPlatformTime::Seconds();

	TickDemoRecord( DeltaSeconds );

	const double EndTime = FPlatformTime::Seconds();

	const double RecordTotalTime = ( EndTime - StartTime );

	// While recording, the CurrentCL is the same as the recording CL.
	ConditionallyDisplayBurnInTime(FEngineVersion::Current().GetChangelist(), DemoCurrentTime);

	MaxRecordTime = FMath::Max( MaxRecordTime, RecordTotalTime );

	AccumulatedRecordTime += RecordTotalTime;

	RecordCountSinceFlush++;

	const double ElapsedTime = EndTime - LastRecordAvgFlush;

	const double AVG_FLUSH_TIME_IN_SECONDS = 2;

	if ( ElapsedTime > AVG_FLUSH_TIME_IN_SECONDS && RecordCountSinceFlush > 0 )
	{
		const float AvgTimeMS = ( AccumulatedRecordTime / RecordCountSinceFlush ) * 1000;
		const float MaxRecordTimeMS = MaxRecordTime * 1000;

		if ( AvgTimeMS > 8.0f )//|| MaxRecordTimeMS > 6.0f )
		{
			UE_LOG( LogDemo, Verbose, TEXT( "UDemoNetDriver::TickFlush: SLOW FRAME. Avg: %2.2f, Max: %2.2f, Actors: %i" ), AvgTimeMS, MaxRecordTimeMS, GetNetworkObjectList().GetActiveObjects().Num() );
		}

		LastRecordAvgFlush		= EndTime;
		AccumulatedRecordTime	= 0;
		MaxRecordTime			= 0;
		RecordCountSinceFlush	= 0;
	}
}

static float GetClampedDeltaSeconds( UWorld* World, const float DeltaSeconds )
{
	check( World != nullptr );

	const float RealDeltaSeconds = DeltaSeconds;

	// Clamp delta seconds
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	const float ClampedDeltaSeconds = WorldSettings->FixupDeltaSeconds( DeltaSeconds * WorldSettings->GetEffectiveTimeDilation(), RealDeltaSeconds );
	check( ClampedDeltaSeconds >= 0.0f );

	return ClampedDeltaSeconds;
}

void UDemoNetDriver::TickDispatch( float DeltaSeconds )
{
	LLM_SCOPE(ELLMTag::Networking);

	// Set the context on the world for this driver's level collection.
		const int32 FoundCollectionIndex = World ? World->GetLevelCollections().IndexOfByPredicate([this](const FLevelCollection& Collection)
	{
		return Collection.GetDemoNetDriver() == this;
	}) : INDEX_NONE;

	FScopedLevelCollectionContextSwitch LCSwitch(FoundCollectionIndex, GetWorld());

	Super::TickDispatch( DeltaSeconds );

	if ( !IsPlaying() || bIsWaitingForStream )
	{
		// Nothing to do
		return;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::TickDispatch: ReplayStreamer ERROR: %s"), ENetworkReplayError::ToString(ReplayStreamer->GetLastError()));
		NotifyDemoPlaybackFailure(EDemoPlayFailure::ReplayStreamerInternal);
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickDispatch: FileAr == nullptr" ) );
		NotifyDemoPlaybackFailure(EDemoPlayFailure::ReplayStreamerInternal);
		return;
	}

	if (!HasLevelStreamingFixes())
	{
		// Wait until all levels are streamed in
		for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if ( StreamingLevel && StreamingLevel->ShouldBeLoaded() && (!StreamingLevel->IsLevelLoaded() || !StreamingLevel->GetLoadedLevel()->GetOutermost()->IsFullyLoaded() || !StreamingLevel->IsLevelVisible() ) )
			{
				// Abort, we have more streaming levels to load
				return;
			}
		}
	}	

	if ( CVarDemoTimeDilation.GetValueOnGameThread() >= 0.0f )
	{
		World->GetWorldSettings()->DemoPlayTimeDilation = CVarDemoTimeDilation.GetValueOnGameThread();
	}

	// DeltaSeconds that is padded in, is unclampped and not time dilated
	DeltaSeconds = GetClampedDeltaSeconds( World, DeltaSeconds );

	// Update time dilation on spectator pawn to compensate for any demo dilation 
	//	(we want to continue to fly around in real-time)
	if ( SpectatorController != nullptr )
	{
		if ( World->GetWorldSettings()->DemoPlayTimeDilation > KINDA_SMALL_NUMBER )
		{
			SpectatorController->CustomTimeDilation = 1.0f / World->GetWorldSettings()->DemoPlayTimeDilation;
		}
		else
		{
			SpectatorController->CustomTimeDilation = 1.0f;
		}

		if ( SpectatorController->GetSpectatorPawn() != nullptr )
		{
			SpectatorController->GetSpectatorPawn()->CustomTimeDilation = SpectatorController->CustomTimeDilation;
					
			SpectatorController->GetSpectatorPawn()->PrimaryActorTick.bTickEvenWhenPaused = true;

			USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(SpectatorController->GetSpectatorPawn()->GetMovementComponent());

			if ( SpectatorMovement )
			{
				//SpectatorMovement->bIgnoreTimeDilation = true;
				SpectatorMovement->PrimaryComponentTick.bTickEvenWhenPaused = true;
			}
		}
	}

	TickDemoPlayback( DeltaSeconds );

	// Used LastProcessedPacketTime because it will correlate better with recorded frame time.
	ConditionallyDisplayBurnInTime(PlaybackDemoHeader.EngineVersion.GetChangelist(), LastProcessedPacketTime);
}

void UDemoNetDriver::ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject )
{
#if !UE_BUILD_SHIPPING
	bool bBlockSendRPC = false;

	SendRPCDel.ExecuteIfBound(Actor, Function, Parameters, OutParms, Stack, SubObject, bBlockSendRPC);

	if (!bBlockSendRPC)
#endif
	{
		if ( IsRecording() )
		{
			TUniquePtr<FScopedPacketManager> PacketManager(ConditionallyCreatePacketManager(*Actor->GetLevel()));

			if ((Function->FunctionFlags & FUNC_NetMulticast))
			{
				// Handle role swapping if this is a client-recorded replay.
				FScopedActorRoleSwap RoleSwap(Actor);
			
				InternalProcessRemoteFunction(Actor, SubObject, ClientConnections[0], Function, Parameters, OutParms, Stack, IsServer());
			}
		}
	}
}

bool UDemoNetDriver::ShouldClientDestroyTearOffActors() const
{
	if ( CVarDemoFastForwardDestroyTearOffActors.GetValueOnGameThread() != 0 )
	{
		return bIsFastForwarding;
	}

	return false;
}

bool UDemoNetDriver::ShouldSkipRepNotifies() const
{
	if ( CVarDemoFastForwardSkipRepNotifies.GetValueOnAnyThread() != 0 )
	{
		return bIsFastForwarding;
	}

	return false;
}

void UDemoNetDriver::StopDemo()
{
	if ( !IsRecording() && !IsPlaying() )
	{
		UE_LOG( LogDemo, Log, TEXT( "StopDemo: No demo is playing" ) );
		return;
	}
	OnDemoFinishRecordingDelegate.Broadcast();
	UE_LOG( LogDemo, Log, TEXT( "StopDemo: Demo %s stopped at frame %d" ), *DemoURL.Map, DemoFrameNum );

	if ( !ServerConnection )
	{
		// let GC cleanup the object
		if ( ClientConnections.Num() > 0 && ClientConnections[0] != nullptr )
		{
			ClientConnections[0]->Close();
		}
	}
	else
	{
		// flush out any pending network traffic
		ServerConnection->FlushNet();

		ServerConnection->State = USOCK_Closed;
		ServerConnection->Close();
	}

	ReplayStreamer->StopStreaming();
	ClearReplayTasks();
	ActiveReplayName = FString();
	ResetDemoState();

	check( !IsRecording() && !IsPlaying() );
}

/*-----------------------------------------------------------------------------
Demo Recording tick.
-----------------------------------------------------------------------------*/

bool UDemoNetDriver::DemoReplicateActor(AActor* Actor, UNetConnection* Connection, bool bMustReplicate)
{
	if ( Actor->NetDormancy == DORM_Initial && Actor->IsNetStartupActor() )
	{
		return false;
	}

	const int32 OriginalOutBunches = Connection->Driver->OutBunches;
	
	bool bDidReplicateActor = false;

	// Handle role swapping if this is a client-recorded replay.
	FScopedActorRoleSwap RoleSwap(Actor);

	if ((Actor->GetRemoteRole() != ROLE_None || Actor->GetTearOff()) && (Actor == Connection->PlayerController || Cast< APlayerController >(Actor) == nullptr))
	{
		const bool bShouldHaveChannel =
			Actor->bRelevantForNetworkReplays &&
			!Actor->GetTearOff() &&
			(!Actor->IsNetStartupActor() || Connection->ClientHasInitializedLevelFor(Actor));

		UActorChannel* Channel = Connection->FindActorChannelRef(Actor);

		if (bShouldHaveChannel && Channel == nullptr)
		{
			// Create a new channel for this actor.
			Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally);
			if (Channel != nullptr)
			{
				Channel->SetChannelActor(Actor);
			}
		}

		if (Channel != nullptr && !Channel->Closing)
		{
			// Send it out!
			bDidReplicateActor = (Channel->ReplicateActor() > 0);

			// Close the channel if this actor shouldn't have one
			if (!bShouldHaveChannel)
			{
				if (!Connection->bResendAllDataSinceOpen)		// Don't close the channel if we're forcing them to re-open for checkpoints
				{
					Channel->Close(EChannelCloseReason::Destroyed);
				}
			}
		}
	}

	if ( bMustReplicate && Connection->Driver->OutBunches == OriginalOutBunches )
	{
		UE_LOG( LogDemo, Error, TEXT( "DemoReplicateActor: bMustReplicate is true but nothing was sent: %s" ), Actor ? *Actor->GetName() : TEXT( "NULL" ) );
	}

	return bDidReplicateActor;
}

void UDemoNetDriver::SerializeGuidCache(TSharedPtr<FNetGUIDCache> InGuidCache, FArchive* CheckpointArchive)
{
	int32 NumValues = 0;
	int32 UnloadedValues = 0;

	int64 CountPos = CheckpointArchive->Tell();

	*CheckpointArchive << NumValues;

	for ( auto It = InGuidCache->ObjectLookup.CreateIterator(); It; ++It )
	{
		FNetworkGUID& NetworkGUID = It.Key();
		FNetGuidCacheObject& CacheObject = It.Value();

		if (NetworkGUID.IsValid())
		{
			const UObject* Object = CacheObject.Object.Get();

			if (NetworkGUID.IsStatic() || (Object && Object->IsNameStableForNetworking()))
			{		
				// if we know the guid was specifically deleted, do not serialize it
				if (DeletedNetStartupActorGUIDs.Contains(NetworkGUID))
				{
					continue;
				}

				FString PathName = Object ? Object->GetName() : CacheObject.PathName.ToString();

				GEngine->NetworkRemapPath(this, PathName, false);

				*CheckpointArchive << NetworkGUID;
				*CheckpointArchive << CacheObject.OuterGUID;
				*CheckpointArchive << PathName;
				*CheckpointArchive << CacheObject.NetworkChecksum;

				uint8 Flags = 0;
				Flags |= CacheObject.bNoLoad ? (1 << 0) : 0;
				Flags |= CacheObject.bIgnoreWhenMissing ? (1 << 1) : 0;

				*CheckpointArchive << Flags;

				++NumValues;

				const bool bUnloaded = Object == nullptr || !Object->IsNameStableForNetworking();
				if (bUnloaded)
				{
					++UnloadedValues;
				}
			}
		}
	}

	int64 Pos = CheckpointArchive->Tell();
	CheckpointArchive->Seek(CountPos);
	*CheckpointArchive << NumValues;
	CheckpointArchive->Seek(Pos);

	UE_LOG( LogDemo, Verbose, TEXT( "Checkpoint. SerializeGuidCache: %i Unloaded: %i" ), NumValues, UnloadedValues );
}

float UDemoNetDriver::GetCheckpointSaveMaxMSPerFrame() const
{
	const float CVarValue = CVarCheckpointSaveMaxMSPerFrameOverride.GetValueOnAnyThread();
	if (CVarValue >= 0.0f)
	{
		return CVarValue;
	}

	return CheckpointSaveMaxMSPerFrame;
}

void UDemoNetDriver::AddNewLevel(const FString& NewLevelName)
{
	LevelNamesAndTimes.Add(FLevelNameAndTime(UWorld::RemovePIEPrefix(NewLevelName), ReplayStreamer->GetTotalDemoTime()));
}

void UDemoNetDriver::SaveCheckpoint()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SaveCheckpoint time"), STAT_ReplayCheckpointSaveTime, STATGROUP_Net);

	FArchive* CheckpointArchive = ReplayStreamer->GetCheckpointArchive();

	if ( CheckpointArchive == nullptr )
	{
		// This doesn't mean error, it means the streamer isn't ready to save checkpoints
		return;
	}

	check( CheckpointArchive->TotalSize() == 0 );

	check( ClientConnections[0]->SendBuffer.GetNumBits() == 0 );

	check( CheckpointSaveContext.CheckpointSaveState == ECheckpointSaveState_Idle );
	
	if (HasLevelStreamingFixes())
	{
		SCOPED_NAMED_EVENT(UDemoNetDriver_ReplayLevelSortAndAssign, FColor::Purple);

		struct StrippedActorInfo
		{
			TWeakObjectPtr<AActor> Actor;
			const UObject* Level;
		};

		const UNetConnection* Connection = ClientConnections[0];
		const FActorChannelMap& ActorChannelMap = Connection->ActorChannelMap();
		const FNetworkObjectList::FNetworkObjectSet& AllObjectsSet = GetNetworkObjectList().GetAllObjects();

		TArray<StrippedActorInfo> ActorArray;
		ActorArray.Reserve(FPlatformMath::Min(GetNetworkObjectList().GetAllObjects().Num(), ActorChannelMap.Num()));

		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Replay actor level sorting time."), STAT_ReplayLevelSorting, STATGROUP_Net);

			// Add all actors that has a channel and also exists in the AllObjectsSet
			for (auto& ChannelPair : ActorChannelMap)
			{
				if (ChannelPair.Value != nullptr)
				{
					AActor* Actor = ChannelPair.Value->GetActor();
					if (Actor)
					{
						// Validate that we do not pickup any extra actors
						if (AllObjectsSet.Find(Actor) != nullptr)
						{
							ActorArray.Add( { ChannelPair.Key, Actor->GetOuter() } );
						}
					}
				}
			}

			// Sort by level			
			ActorArray.Sort([](const StrippedActorInfo& A, const StrippedActorInfo& B) { return (B.Level < A.Level); });
		}

		CheckpointSaveContext.PendingCheckpointActors.Reserve(ActorArray.Num());

		uint32 LevelIt = 0;
		for (int32 CurrentIt = 0, EndIt = ActorArray.Num(); CurrentIt != EndIt; ++LevelIt)
		{
			const UObject* CurrentLevelToIndex = ActorArray[CurrentIt].Level;
			const FLevelStatus& LevelStatus = FindOrAddLevelStatus(*Cast<const ULevel>(CurrentLevelToIndex));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Validate that we get the correct level
			check(Cast<const ULevel>(CurrentLevelToIndex) == ActorArray[CurrentIt].Actor->GetLevel());
#endif
			while (CurrentIt < EndIt && (CurrentLevelToIndex == ActorArray[CurrentIt].Level))
			{
				CheckpointSaveContext.PendingCheckpointActors.Add( { ActorArray[CurrentIt].Actor, LevelStatus.LevelIndex } );
				++CurrentIt;
			};
		}
	}
	else
	{
		// Add any actor with a valid channel to the PendingCheckpointActors list
		for (const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetAllObjects())
		{
			AActor* Actor = ObjectInfo.Get()->Actor;

			if ( ClientConnections[0]->FindActorChannelRef( Actor ) )
			{
				CheckpointSaveContext.PendingCheckpointActors.Add( { Actor, -1 } );
			}
		}
	}

	if ( CheckpointSaveContext.PendingCheckpointActors.Num() == 0 )
	{
		return;
	}

	UPackageMapClient* PackageMapClient = ( ( UPackageMapClient* )ClientConnections[0]->PackageMap );

	PackageMapClient->SavePackageMapExportAckStatus( CheckpointSaveContext.CheckpointAckState );

	// We are now processing checkpoint actors	
	CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_ProcessCheckpointActors;
	CheckpointSaveContext.TotalCheckpointSaveTimeSeconds = 0;
	CheckpointSaveContext.TotalCheckpointReplicationTimeSeconds = 0;
	CheckpointSaveContext.TotalCheckpointSaveFrames = 0;
	LastCheckpointTime = DemoCurrentTime;

	UE_LOG( LogDemo, Log, TEXT( "Starting checkpoint. Actors: %i" ), GetNetworkObjectList().GetActiveObjects().Num() );

	// Do the first checkpoint tick now if we're not amortizing
	if (GetCheckpointSaveMaxMSPerFrame() <= 0.0f)
	{
		TickCheckpoint();
	}
}

class FRepActorsCheckpointParams
{
public:

	const double StartCheckpointTime;
	const double CheckpointMaxUploadTimePerFrame;
};

// Only start execution if a certain percentage remains of the 
static bool inline ShouldExecuteState(const FRepActorsCheckpointParams& Params, double CurrentTime, double RequiredRatioToStart)
{
	const double CheckpointMaxUploadTimePerFrame = Params.CheckpointMaxUploadTimePerFrame;
	if (CheckpointMaxUploadTimePerFrame <= 0.0)
	{
		return true;
	}

	return (1.0 - ((CurrentTime - Params.StartCheckpointTime) / Params.CheckpointMaxUploadTimePerFrame)) > RequiredRatioToStart;
}

void UDemoNetDriver::TickCheckpoint()
{
	if ( CheckpointSaveContext.CheckpointSaveState == ECheckpointSaveState_Idle )
	{
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "SaveCheckpoint time" ), STAT_ReplayCheckpointSaveTime, STATGROUP_Net );

	FArchive* CheckpointArchive = ReplayStreamer->GetCheckpointArchive();

	if ( !ensure( CheckpointArchive != nullptr ) )
	{
		return;
	}

	FRepActorsCheckpointParams Params
	{
		FPlatformTime::Seconds(),
		(double)GetCheckpointSaveMaxMSPerFrame() / 1000
	};

	bool bExecuteNextState = true;
	double CurrentTime = Params.StartCheckpointTime;

	{
		FScopedForceUnicodeInArchive ScopedUnicodeSerialization(*CheckpointArchive);

		UDemoNetConnection* ClientConnection = CastChecked<UDemoNetConnection>(ClientConnections[0]);

		CheckpointSaveContext.TotalCheckpointSaveFrames++;

		FlushNetChecked(*ClientConnection);

		UPackageMapClient* PackageMapClient = ( ( UPackageMapClient* )ClientConnection->PackageMap );

		// Save package map ack status in case we export stuff during the checkpoint (so we can restore the connection back to what it was before we saved the checkpoint)
		PackageMapClient->OverridePackageMapExportAckStatus( &CheckpointSaveContext.CheckpointAckState );

		while (bExecuteNextState && (CheckpointSaveContext.CheckpointSaveState != ECheckpointSaveState_Finalize) && !(Params.CheckpointMaxUploadTimePerFrame > 0 && CurrentTime - Params.StartCheckpointTime > Params.CheckpointMaxUploadTimePerFrame))
		{
			switch (CheckpointSaveContext.CheckpointSaveState)
			{
				case ECheckpointSaveState_ProcessCheckpointActors:
				{
					SCOPED_NAMED_EVENT(UDemoNetDriver_ProcessCheckpointActors, FColor::Green);

					// Save the replicated server time so we can restore it after the checkpoint has been serialized.
					// This preserves the existing behavior and prevents clients from receiving updated server time
					// more often than the normal update rate.
					AGameStateBase* const GameState = World != nullptr ? World->GetGameState() : nullptr;

					const float SavedReplicatedServerTimeSeconds = GameState ? GameState->ReplicatedWorldTimeSeconds : -1.0f;

					// Normally AGameStateBase::ReplicatedWorldTimeSeconds is only updated periodically,
					// but we want to make sure it's accurate for the checkpoint.
					if ( GameState )
					{
						GameState->UpdateServerTimeSeconds();
					}

					{
						// Re-use the existing connection to record all properties that have changed since channels were first opened
						// Set bResendAllDataSinceOpen to true to signify that we want to do this
						TGuardValue<bool> ResendAllData(ClientConnection->bResendAllDataSinceOpen, true);

						// Can't use conditionally create here, because NumActorsToProcess will be empty when HasLevelStreamingFixes is false.
						TUniquePtr<FScopedPacketManager> PacketManager;

						int32 ProcessedLevelIndex = -1;
						const int32 UseScopedPacketManager = HasLevelStreamingFixes() ? 1 : 0;

						bool bContinue = true;
						int32 NumActorsToReplicate = CheckpointSaveContext.PendingCheckpointActors.Num();

						do
						{
							const FPendingCheckPointActor Current = CheckpointSaveContext.PendingCheckpointActors.Pop();
							AActor* Actor = Current.Actor.Get();

							if (UseScopedPacketManager & (Current.LevelIndex != ProcessedLevelIndex))
							{
								PacketManager.Reset(new FScopedPacketManager(*ClientConnection, Current.LevelIndex + 1));
								ProcessedLevelIndex = Current.LevelIndex;
							}

							bContinue = ReplicateCheckpointActor(Actor, ClientConnection, Params);				
						}
						while (--NumActorsToReplicate && bContinue);

						if ( GameState )
						{
							// Restore the game state's replicated world time
							GameState->ReplicatedWorldTimeSeconds = SavedReplicatedServerTimeSeconds;
						}

						FlushNetChecked(*ClientConnection);

						PackageMapClient->OverridePackageMapExportAckStatus( nullptr );
					}

					// We are done processing for this frame so  store the TotalCheckpointSave time here to be true to the old behavior which did not account for the	actual saving time of the check point
					CheckpointSaveContext.TotalCheckpointReplicationTimeSeconds += ( FPlatformTime::Seconds() - Params.StartCheckpointTime );

					// if we have replicated all checkpointactors, move on to the next state
					if ( CheckpointSaveContext.PendingCheckpointActors.Num() == 0 )
					{
						CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_SerializeDeletedStartupActors;
					}
				}
				break;

				case ECheckpointSaveState_SerializeDeletedStartupActors:
				{
					// Postpone execution of this state if we have used to much of our alloted time, this value can be tweaked based on profiling
					const double RequiredRatioFor_SerializeDeletedStartupActors = 0.6;
					if ((bExecuteNextState = ShouldExecuteState(Params, CurrentTime, RequiredRatioFor_SerializeDeletedStartupActors)) == true)
					{
						SCOPED_NAMED_EVENT(UDemoNetDriver_SerializeDeletedStartupActors, FColor::Green);

						//
						// We're done saving this checkpoint, now we need to write out all data for it.
						//

						CheckpointSaveContext.bWriteCheckpointOffset = HasLevelStreamingFixes();
						if (HasLevelStreamingFixes())
						{
							CheckpointSaveContext.CheckpointOffset = CheckpointArchive->Tell();
							// We will rewrite this offset when we are done saving the checkpoint
							*CheckpointArchive << CheckpointSaveContext.CheckpointOffset;
						}

						*CheckpointArchive << CurrentLevelIndex;

						// Save deleted startup actors	
						*CheckpointArchive << DeletedNetStartupActors;

						CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_SerializeGuidCache;
					}
				}
				break;

				case ECheckpointSaveState_SerializeGuidCache:
				{
					// Postpone execution of this state if we have used to much of our alloted time, this value can be tweaked based on profiling
					const double RequiredRatioFor_SerializeGuidCache = 0.8;
					if ((bExecuteNextState = ShouldExecuteState(Params, CurrentTime, RequiredRatioFor_SerializeGuidCache)) == true)
					{
						SCOPED_NAMED_EVENT(UDemoNetDriver_SerializeGuidCache, FColor::Green);

						// Save the current guid cache
						SerializeGuidCache( GuidCache, CheckpointArchive );

						CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_SerializeNetFieldExportGroupMap;
					}
				}
				break;

				case ECheckpointSaveState_SerializeNetFieldExportGroupMap:
				{
					// Postpone execution of this state if we have used to much of our alloted time, this value can be tweaked based on profiling
					const double RequiredRatioFor_SerializeNetFieldExportGroupMap = 0.6;
					if ((bExecuteNextState = ShouldExecuteState(Params, CurrentTime, RequiredRatioFor_SerializeNetFieldExportGroupMap)) == true)
					{
						SCOPED_NAMED_EVENT(UDemoNetDriver_SerializeNetFieldExportGroupMap, FColor::Green);

						// Save the compatible rep layout map
						PackageMapClient->SerializeNetFieldExportGroupMap( *CheckpointArchive );

						CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_SerializeDemoFrameFromQueuedDemoPackets;
					}
				}
				break;

				case ECheckpointSaveState_SerializeDemoFrameFromQueuedDemoPackets:
				{
					// Postpone execution of this state if we have used to much of our alloted time, this value can be tweaked based on profiling
					const double RequiredRatioFor_SerializeDemoFrameFromQueuedDemoPackets = 0.8;
					if ((bExecuteNextState = ShouldExecuteState(Params, CurrentTime, RequiredRatioFor_SerializeDemoFrameFromQueuedDemoPackets)) == true)
					{
						SCOPED_NAMED_EVENT(UDemoNetDriver_SerializeDemoFrameFromQueuedDemoPackets, FColor::Green);

						// Write offset
						if (CheckpointSaveContext.bWriteCheckpointOffset)
						{
							const FArchivePos CurrentPosition = CheckpointArchive->Tell();
							FArchivePos Offset = CurrentPosition - (CheckpointSaveContext.CheckpointOffset + sizeof(FArchivePos));
							CheckpointArchive->Seek(CheckpointSaveContext.CheckpointOffset);
							*CheckpointArchive << Offset;
							CheckpointArchive->Seek(CurrentPosition);	
						}

						// Get the size of the guid data saved
						CheckpointSaveContext.GuidCacheSize = CheckpointArchive->TotalSize();

						// This will cause the entire name list to be written out again.
						// Note, WriteDemoFrameFromQueuedDemoPackets will set this to 0 so we guard the value.
						// This is because when checkpoint amortization is enabled, it's possible for new levels to stream
						// in while recording a checkpoint, and we want to make sure those get written out to the normal
						// streaming archive next frame.
						TGuardValue<uint32> NumLevelsAddedThisFrameGuard(NumLevelsAddedThisFrame, AllLevelStatuses.Num());

						// Write out all of the queued up packets generated while saving the checkpoint
						WriteDemoFrameFromQueuedDemoPackets( *CheckpointArchive, ClientConnection->QueuedCheckpointPackets, static_cast<float>(LastCheckpointTime) );

						CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_Finalize;
					}
				}
				break;

				default:
					break;
			}

			CurrentTime = FPlatformTime::Seconds();
		}
	}

	// accumulate time spent over all checkpoint ticks
	CheckpointSaveContext.TotalCheckpointSaveTimeSeconds += ( CurrentTime - Params.StartCheckpointTime );

	if (CheckpointSaveContext.CheckpointSaveState == ECheckpointSaveState_Finalize)
	{
		SCOPED_NAMED_EVENT(UDemoNetDriver_Finalize, FColor::Green);

		// Get the total checkpoint size
		const int32 TotalCheckpointSize = CheckpointArchive->TotalSize();

		if ( CheckpointArchive->TotalSize() > 0 )
		{
			ReplayStreamer->FlushCheckpoint( GetLastCheckpointTimeInMS() );
		}

		const float TotalCheckpointTimeInMS = CheckpointSaveContext.TotalCheckpointReplicationTimeSeconds * 1000.0f;
		const float TotalCheckpointTimeWithOverheadInMS = CheckpointSaveContext.TotalCheckpointSaveTimeSeconds * 1000.0f;

		UE_LOG( LogDemo, Log, TEXT( "Finished checkpoint. Actors: %i, GuidCacheSize: %i, TotalSize: %i, TotalCheckpointSaveFrames: %i, TotalCheckpointTimeInMS: %2.2f, TotalCheckpointTimeWithOverheadInMS: %2.2f" ), GetNetworkObjectList().GetActiveObjects().Num(), CheckpointSaveContext.GuidCacheSize, TotalCheckpointSize, CheckpointSaveContext.TotalCheckpointSaveFrames, TotalCheckpointTimeInMS, TotalCheckpointTimeWithOverheadInMS);

		// we are done, out
		CheckpointSaveContext.CheckpointSaveState = ECheckpointSaveState_Idle;
	}
}

bool UDemoNetDriver::ReplicateCheckpointActor(AActor* ToReplicate, UDemoNetConnection* ClientConnection, FRepActorsCheckpointParams& Params)
{
	// Early out if the actor has been destroyed or the world is streamed out.
	if (ToReplicate == nullptr || ToReplicate->GetWorld() == nullptr)
		return true;

	if (UActorChannel* ActorChannel = ClientConnection->FindActorChannelRef(ToReplicate))
	{
		ToReplicate->CallPreReplication(this);
		DemoReplicateActor(ToReplicate, ClientConnection, true);

		UpdateExternalDataForActor(ToReplicate);
		
		const double CheckpointTime = FPlatformTime::Seconds();

		if (Params.CheckpointMaxUploadTimePerFrame > 0 && CheckpointTime - Params.StartCheckpointTime > Params.CheckpointMaxUploadTimePerFrame)
		{
			return false;
		}
	}

	return true;
}

void UDemoNetDriver::SaveExternalData( FArchive& Ar )
{
	SCOPED_NAMED_EVENT(UDemoNetDriver_SaveExternalData, FColor::Blue);
	for ( auto It = ObjectsWithExternalData.CreateIterator(); It; ++It )
	{		
		FReplayExternalOutData& Element = *It;
		if (UObject* Object = Element.Object.Get())
		{
			FRepChangedPropertyTracker* PropertyTracker = RepChangedPropertyTrackerMap.FindChecked(Object).Get();

			uint32 ExternalDataNumBits = PropertyTracker->ExternalDataNumBits;
			if (ExternalDataNumBits > 0)
			{
				// Save payload size (in bits)
				Ar.SerializeIntPacked(ExternalDataNumBits);

				// Save GUID
				Ar << Element.GUID;

				// Save payload
				Ar.Serialize(PropertyTracker->ExternalData.GetData(), PropertyTracker->ExternalData.Num());

				PropertyTracker->ExternalData.Empty();
				PropertyTracker->ExternalDataNumBits = 0;
			}
		}
	}

	// Reset external out datas
	ObjectsWithExternalData.Reset();

	uint32 StopCount = 0;
	Ar.SerializeIntPacked( StopCount );
}

void UDemoNetDriver::LoadExternalData( FArchive& Ar, const float TimeSeconds )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Demo_LoadExternalData"), Demo_LoadExternalData, STATGROUP_Net);

	while ( true )
	{
		uint32 ExternalDataNumBits;

		// Read payload into payload/guid map
		Ar.SerializeIntPacked( ExternalDataNumBits );

		if ( ExternalDataNumBits == 0 )
		{
			return;
		}

		FNetworkGUID NetGUID;

		// Read net guid this payload belongs to
		Ar << NetGUID;

		int32 ExternalDataNumBytes = ( ExternalDataNumBits + 7 ) >> 3;

		FBitReader Reader(nullptr, ExternalDataNumBits);

		Ar.Serialize(Reader.GetData(), ExternalDataNumBytes);

		Reader.SetEngineNetVer( ServerConnection->EngineNetworkProtocolVersion );
		Reader.SetGameNetVer( ServerConnection->GameNetworkProtocolVersion );

		FReplayExternalDataArray& ExternalDataArray = ExternalDataToObjectMap.FindOrAdd( NetGUID );

		ExternalDataArray.Add( new FReplayExternalData( MoveTemp(Reader), TimeSeconds ) );
	}
}

void UDemoNetDriver::AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data)
{
	AddOrUpdateEvent(FString(), Group, Meta, Data);
}

void UDemoNetDriver::AddOrUpdateEvent(const FString& Name, const FString& Group, const FString& Meta, const TArray<uint8>& Data)
{
	uint32 SavedTimeMS = GetDemoCurrentTimeInMS();
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->AddOrUpdateEvent(Name, SavedTimeMS, Group, Meta, Data);
	}
	UE_LOG(LogDemo, Verbose, TEXT("Custom Event %s.%s. Total: %i, Time: %2.2f"), *Group, *Name, Data.Num(), SavedTimeMS);
}

void UDemoNetDriver::EnumerateEvents(const FString& Group, const FEnumerateEventsCallback& Delegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->EnumerateEvents(Group, Delegate);
	}
}

void UDemoNetDriver::RequestEventData(const FString& EventID, const FRequestEventDataCallback& Delegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->RequestEventData(EventID, Delegate);
	}
}

void UDemoNetDriver::EnumerateEventsForActiveReplay(const FString& Group, const FEnumerateEventsCallback& Delegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->EnumerateEvents(ActiveReplayName, Group, Delegate);
	}
}

void UDemoNetDriver::EnumerateEventsForActiveReplay(const FString& Group, const int32 UserIndex, const FEnumerateEventsCallback& Delegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->EnumerateEvents(ActiveReplayName, Group, UserIndex, Delegate);
	}
}

void UDemoNetDriver::RequestEventDataForActiveReplay(const FString& EventID, const FRequestEventDataCallback& Delegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->RequestEventData(ActiveReplayName, EventID, Delegate);
	}
}

void UDemoNetDriver::RequestEventDataForActiveReplay(const FString& EventID, const int32 UserIndex, const FRequestEventDataCallback& Delegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->RequestEventData(ActiveReplayName, EventID, UserIndex, Delegate);
	}
}

/**
* FReplayViewer
* Used when demo.UseNetRelevancy enabled
* Tracks all of the possible viewers of a replay that we use to determine relevancy
*/
class FReplayViewer
{
public:
	FReplayViewer( const UNetConnection* Connection ) :
		Viewer( Connection->PlayerController ? Connection->PlayerController : Connection->OwningActor ), 
		ViewTarget( Connection->PlayerController ? Connection->PlayerController->GetViewTarget() : Connection->OwningActor )
	{
		Location = ViewTarget ? ViewTarget->GetActorLocation() : FVector::ZeroVector;
	}

	AActor*		Viewer;
	AActor*		ViewTarget;
	FVector		Location;
};

class FRepActorsParams : public FNoncopyable
{
public:

	FRepActorsParams(const bool bInUseAdaptiveNetFrequency, const bool bInDoFindActorChannel, const bool bInDoCheckDormancy,
					const float InMinRecordHz, const float InMaxRecordHz, const float InServerTickTime,
					const double InReplicationStartTimeSeconds, const double InTimeLimitSeconds):
		bUseAdapativeNetFrequency(bInUseAdaptiveNetFrequency),
		bDoFindActorChannel(bInDoFindActorChannel),
		bDoCheckDormancy(bInDoCheckDormancy),
		NumActorsReplicated(0),
		MinRecordHz(InMinRecordHz),
		MaxRecordHz(InMaxRecordHz),
		ServerTickTime(InServerTickTime),
		ReplicationStartTimeSeconds(InReplicationStartTimeSeconds),
		TimeLimitSeconds(InTimeLimitSeconds)
	{
	}

	const bool bUseAdapativeNetFrequency;
	const bool bDoFindActorChannel;
	const bool bDoCheckDormancy;
	int32 NumActorsReplicated;
	const float MinRecordHz;
	const float MaxRecordHz;
	const float ServerTickTime;
	const double ReplicationStartTimeSeconds;
	const double TimeLimitSeconds;
};

void UDemoNetDriver::TickDemoRecord( float DeltaSeconds )
{
	if ( !IsRecording() || bPauseRecording )
	{
		return;
	}

	CSV_SCOPED_TIMING_STAT(Basic, DemoRecordTime);

	// DeltaSeconds that is padded in, is unclampped and not time dilated
	DemoCurrentTime += GetClampedDeltaSeconds( World, DeltaSeconds );

	ReplayStreamer->UpdateTotalDemoTime( GetDemoCurrentTimeInMS() );

	if ( CheckpointSaveContext.CheckpointSaveState != ECheckpointSaveState_Idle )
	{
		// If we're in the middle of saving a checkpoint, then update that now and return
		TickCheckpoint();
		return;
	}
	else
	{
		TickDemoRecordFrame( DeltaSeconds );

		// Save a checkpoint if it's time
		if (CVarEnableCheckpoints.GetValueOnAnyThread() == 1)
		{
			check( CheckpointSaveContext.CheckpointSaveState == ECheckpointSaveState_Idle );		// We early out above, so this shouldn't be possible

			if (ShouldSaveCheckpoint())
			{
				SaveCheckpoint();
			}
		}
	}
}

void UDemoNetDriver::BuildSortedLevelPriorityOnLevels(const TArray<FDemoActorPriority>& PrioritizedActorList, TArray<FLevelnterval>& OutLevelIntervals)
{
	OutLevelIntervals.Reset();

	// Find level intervals
	const int32 Count = PrioritizedActorList.Num();
	const FDemoActorPriority* Priorities = PrioritizedActorList.GetData();

	for (int32 It = 0; It < Count;)
	{
		const UObject* CurrentLevel = Priorities[It].Level;

		FLevelnterval Interval;
		Interval.StartIndex = It;
		Interval.Priority = Priorities[It].ActorPriority.Priority;
		Interval.LevelIndex = (CurrentLevel != nullptr ? FindOrAddLevelStatus(*Cast<ULevel>(CurrentLevel)).LevelIndex + 1 : 0);

		while (It < Count && Priorities[It].Level == CurrentLevel)
		{
			++It;
		}

		Interval.Count = It - Interval.StartIndex;

		OutLevelIntervals.Add(Interval);
	}

	// Sort intervals on priority
	OutLevelIntervals.Sort([](const FLevelnterval& A, const FLevelnterval& B) { return (B.Priority < A.Priority) || ((A.Priority == B.Priority) && (A.LevelIndex < B.LevelIndex)); });
}


void UDemoNetDriver::TickDemoRecordFrame( float DeltaSeconds )
{
	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == nullptr )
	{
		return;
	}

	const double RecordFrameStartTime = FPlatformTime::Seconds();
	const double RecordTimeLimit = (MaxDesiredRecordTimeMS * 1000.f);

	// Mark any new streaming levels, so that they are saved out this frame
	if (!HasLevelStreamingFixes())
	{
		for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if ( StreamingLevel == nullptr || !StreamingLevel->ShouldBeLoaded() || StreamingLevel->ShouldBeAlwaysLoaded() )
			{
				continue;
			}

			TWeakObjectPtr<UObject> WeakStreamingLevel;
			WeakStreamingLevel = StreamingLevel;
			if ( !UniqueStreamingLevels.Contains( WeakStreamingLevel ) )
			{
				UniqueStreamingLevels.Add( WeakStreamingLevel );
				NewStreamingLevelsThisFrame.Add( WeakStreamingLevel );
			}
		}
	}

	// Save out a frame
	DemoFrameNum++;
	ReplicationFrame++;

	UDemoNetConnection* ClientConnection = CastChecked<UDemoNetConnection>(ClientConnections[0]);

	// flush out any pending network traffic
	FlushNetChecked(*ClientConnection);

	float ServerTickTime = GEngine->GetMaxTickRate( DeltaSeconds );
	if ( ServerTickTime == 0.0 )
	{
		ServerTickTime = DeltaSeconds;
	}
	else
	{
		ServerTickTime	= 1.0 / ServerTickTime;
	}

	// Build priority list
	FNetworkObjectList& NetObjectList = GetNetworkObjectList();
	const FNetworkObjectList::FNetworkObjectSet& ActiveObjectSet = NetObjectList.GetActiveObjects();
	const int32 NumActiveObjects = ActiveObjectSet.Num();

	PrioritizedActors.Reset(NumActiveObjects);

	// Set the location of the connection's viewtarget for prioritization.
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	APlayerController* CachedViewerOverride = ViewerOverride.Get();
	APlayerController* Viewer = CachedViewerOverride ? CachedViewerOverride : ClientConnection->GetPlayerController(World);
	AActor* ViewTarget = Viewer ? Viewer->GetViewTarget() : nullptr;
	
	if (ViewTarget)
	{
		ViewLocation = ViewTarget->GetActorLocation();
		ViewDirection = ViewTarget->GetActorRotation().Vector();
	}

	const bool bDoCheckDormancyEarly = CVarDemoLateActorDormancyCheck.GetValueOnAnyThread() == 0;
	const bool bDoPrioritizeActors = bPrioritizeActors;
	const bool bDoFindActorChannelEarly = bDoPrioritizeActors || bDoCheckDormancyEarly;

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Replay prioritize time"), STAT_ReplayPrioritizeTime, STATGROUP_Net);

		const double ConsiderTimeLimit = RecordTimeLimit * RecordBuildConsiderAndPrioritizeTimeSlice;
		auto HasConsiderTimeBeenExhausted = [ConsiderTimeLimit, RecordFrameStartTime, RecordTimeLimit]()
		{
			return RecordTimeLimit > 0.f && (FPlatformTime::Seconds() - RecordFrameStartTime) > ConsiderTimeLimit;
		};

		{
			SCOPED_NAMED_EVENT(UDemoNetDriver_PrioritizeDestroyedOrDormantActors, FColor::Green);

			// Add destroyed actors that the client may not have a channel for
			// We add these first so they get more of the prioritize time slice.
			// This is because they are marked top priority anyway, and won't need to be prioritized
			// which should decrease overall time spent next frame.
			FDemoActorPriority DestroyedActorPriority;
			DestroyedActorPriority.ActorPriority.Priority = 0x7FFFFFFF;
			for (auto It = ClientConnection->GetDestroyedStartupOrDormantActorGUIDs().CreateIterator(); It; ++It)
			{
				TUniquePtr<FActorDestructionInfo>& DInfo = DestroyedStartupOrDormantActors.FindChecked(*It);
				DestroyedActorPriority.ActorPriority.DestructionInfo = DInfo.Get();
				DestroyedActorPriority.Level = bHasLevelStreamingFixes ? DestroyedActorPriority.ActorPriority.DestructionInfo->Level.Get() : nullptr;
				PrioritizedActors.Add(DestroyedActorPriority);

				if (HasConsiderTimeBeenExhausted())
				{
					break;
				}
			}
		}

		if (!HasConsiderTimeBeenExhausted())
		{
			TArray< FReplayViewer, TInlineAllocator<16> > ReplayViewers;

			const bool bUseNetRelevancy = CVarDemoUseNetRelevancy.GetValueOnAnyThread() > 0 && World->NetDriver != nullptr && World->NetDriver->IsServer();

			// If we're using relevancy, consider all connections as possible viewing sources
			if (bUseNetRelevancy)
			{
				for (UNetConnection* Connection : World->NetDriver->ClientConnections)
				{
					FReplayViewer ReplayViewer(Connection);

					if (ReplayViewer.ViewTarget)
					{
						ReplayViewers.Add(FReplayViewer(Connection));
					}
				}
			}

			const float CullDistanceOverride = CVarDemoCullDistanceOverride.GetValueOnAnyThread();
			const float CullDistanceOverrideSq = CullDistanceOverride > 0.0f ? FMath::Square(CullDistanceOverride) : 0.0f;

			const float RecordHzWhenNotRelevant = CVarDemoRecordHzWhenNotRelevant.GetValueOnAnyThread();
			const float UpdateDelayWhenNotRelevant = RecordHzWhenNotRelevant > 0.0f ? 1.0f / RecordHzWhenNotRelevant : 0.5f;

			TArray<AActor*, TInlineAllocator<128>> ActorsToRemove;

			FDemoActorPriority DemoActorPriority;
			FActorPriority& ActorPriority = DemoActorPriority.ActorPriority;

			for (const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : ActiveObjectSet)
			{
				FNetworkObjectInfo* ActorInfo = ObjectInfo.Get();

				if (DemoCurrentTime > ActorInfo->NextUpdateTime)
				{
					AActor* Actor = ActorInfo->Actor;

					if (Actor->IsPendingKill())
					{
						ActorsToRemove.Add(Actor);
						continue;
					}

					// During client recording, a torn-off actor will already have its remote role set to None, but
					// we still need to replicate it one more time so that the recorded replay knows it's been torn-off as well.
					if (Actor->GetRemoteRole() == ROLE_None && !Actor->GetTearOff())
					{
						ActorsToRemove.Add(Actor);
						continue;
					}

					if (Actor->NetDormancy == DORM_Initial && Actor->IsNetStartupActor())
					{
						ActorsToRemove.Add(Actor);
						continue;
					}

					// We check ActorInfo->LastNetUpdateTime < KINDA_SMALL_NUMBER to force at least one update for each actor
					const bool bWasRecentlyRelevant = (ActorInfo->LastNetUpdateTime < KINDA_SMALL_NUMBER) || ((Time - ActorInfo->LastNetUpdateTime) < RelevantTimeout);

					bool bIsRelevant = !bUseNetRelevancy || Actor->bAlwaysRelevant || Actor == ClientConnection->PlayerController || ActorInfo->bForceRelevantNextUpdate;

					ActorInfo->bForceRelevantNextUpdate = false;

					if (!bIsRelevant)
					{
						// Assume this actor is relevant as long as *any* viewer says so
						for (const FReplayViewer& ReplayViewer : ReplayViewers)
						{
							if (Actor->IsReplayRelevantFor(ReplayViewer.Viewer, ReplayViewer.ViewTarget, ReplayViewer.Location, CullDistanceOverrideSq))
							{
								bIsRelevant = true;
								break;
							}
						}
					}

					if (!bIsRelevant && !bWasRecentlyRelevant)
					{
						// Actor is not relevant (or previously relevant), so skip and set next update time based on demo.RecordHzWhenNotRelevant
						ActorInfo->NextUpdateTime = DemoCurrentTime + UpdateDelayWhenNotRelevant;
						continue;
					}

					UActorChannel* Channel = nullptr;
					if (bDoFindActorChannelEarly)
					{
						Channel = ClientConnection->FindActorChannelRef(Actor);

						// Check dormancy
						if (bDoCheckDormancyEarly && Channel && ShouldActorGoDormantForDemo(Actor, Channel))
						{
							// Either shouldn't go dormant, or is already dormant
							Channel->StartBecomingDormant();
						}
					}

					ActorPriority.ActorInfo = ActorInfo;
					ActorPriority.Channel = Channel;
					DemoActorPriority.Level = Actor->GetOuter();

					if (bDoPrioritizeActors) // implies bDoFindActorChannelEarly is true
					{
						const float LastReplicationTime = Channel ? (Time - Channel->LastUpdateTime) : SpawnPrioritySeconds;
						ActorPriority.Priority = FMath::RoundToInt(65536.0f * Actor->GetReplayPriority(ViewLocation, ViewDirection, Viewer, ViewTarget, Channel, LastReplicationTime));
					}

					PrioritizedActors.Add(DemoActorPriority);

					if (bIsRelevant)
					{
						ActorInfo->LastNetUpdateTime = Time;
					}
				}

				if (HasConsiderTimeBeenExhausted())
				{
					break;
				}
			}

			{
				SCOPED_NAMED_EVENT(UDemoNetDriver_PrioritizeRemoveActors, FColor::Green);

				// Always remove necessary actors, don't time slice this.
				for (AActor* Actor : ActorsToRemove)
				{
					RemoveNetworkActor(Actor);
				}
			}
		}
	}

	if ( HasLevelStreamingFixes() )
	{
		SCOPED_NAMED_EVENT(UDemoNetDriver_PrioritizeLevelSort, FColor::Green);
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Replay actor level sorting time."), STAT_ReplayLevelSorting, STATGROUP_Net);

		if (bPrioritizeActors)
		{
			UE_LOG(LogDemo, Verbose, TEXT("bPrioritizeActors and HasLevelStreamingFixes are both enabled. This will undo some prioritization work."));
		}

		// Sort by Level and priority, If the order of levels are relevant we need a second pass on the array to find the intervals of the levels and sort those on "level with netobject with highest priority"
		// but since prioritization is disabled the order is arbitrary so there is really no use to do the extra work 
		PrioritizedActors.Sort([](const FDemoActorPriority& A, const FDemoActorPriority& B) { return (B.Level < A.Level) || ((B.Level == A.Level) && (B.ActorPriority.Priority < A.ActorPriority.Priority)); });

		// Find intervals in sorted priority lists with the same level and sort the intervals based on priority of first Object in each interval.
		// Intervals are then used to determine the order we write out the replicated objects as we write one packet per level.
		BuildSortedLevelPriorityOnLevels(PrioritizedActors, LevelIntervals);
	}
	else if ( bPrioritizeActors )
	{
		// Sort on priority
		PrioritizedActors.Sort([](const FDemoActorPriority& A, const FDemoActorPriority& B) { return B.ActorPriority.Priority < A.ActorPriority.Priority; });
	}

	const double PrioritizeEndTime = FPlatformTime::Seconds();
	const double TotalPrioritizeActorsTime = (PrioritizeEndTime - RecordFrameStartTime);
	const float TotalPrioritizeActorsTimeMS = TotalPrioritizeActorsTime * 1000.f;
	const int32 NumPrioritizedActors = PrioritizedActors.Num();

	CSV_CUSTOM_STAT(Basic, DemoRecPrioritizeTime, TotalPrioritizeActorsTimeMS, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(Basic, DemoRecPriotizedActors, NumPrioritizedActors, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(Basic, DemoNumActiveObjects, NumActiveObjects, ECsvCustomStatOp::Set);

	// Make sure we're under the desired recording time quota, if any.
	// See ReplicatePriorizeActor.
	if (RecordTimeLimit > 0.0f && TotalPrioritizeActorsTime > RecordTimeLimit)
	{
		DemoNetDriverRecordingPrivate::LogDemoRecordTimeElapsed(TEXT("Exceeded maximum desired recording time (during Prioritization).  Max: %.3fms, TimeSpent: %.3fms, Active Actors: %d, Prioritized Actors: %d"),
			MaxDesiredRecordTimeMS, TotalPrioritizeActorsTimeMS, NumActiveObjects, NumPrioritizedActors);
	}

	float MinRecordHz = CVarDemoMinRecordHz.GetValueOnAnyThread();
	float MaxRecordHz = CVarDemoRecordHz.GetValueOnAnyThread();

	if (MaxRecordHz < MinRecordHz)
	{
		Swap(MinRecordHz, MaxRecordHz);
	}

	FRepActorsParams Params
	(
		CVarUseAdaptiveReplayUpdateFrequency.GetValueOnAnyThread() > 0,
		!bDoFindActorChannelEarly,
		!bDoCheckDormancyEarly,
		MinRecordHz,
		MaxRecordHz,
		ServerTickTime,
		RecordFrameStartTime,
		RecordTimeLimit
	);

	if (HasLevelStreamingFixes())
	{
		const FDemoActorPriority* Priorities = PrioritizedActors.GetData();

		// Split per level		
		for (const FLevelnterval& Interval : LevelIntervals)
		{
			FScopedPacketManager PacketManager(*(UDemoNetConnection*)ClientConnections[0], Interval.LevelIndex);
			bool bContinue = ReplicatePrioritizedActors(&Priorities[Interval.StartIndex], Interval.Count, Params);
			if (!bContinue)
				break;
		}
	}
	else
	{
		ReplicatePrioritizedActors(PrioritizedActors.GetData(), PrioritizedActors.Num(), Params);
	}

	
	CSV_CUSTOM_STAT(Basic, DemoNumReplicatedActors, Params.NumActorsReplicated, ECsvCustomStatOp::Set);

	FlushNetChecked(*ClientConnection);

	WriteDemoFrameFromQueuedDemoPackets(*FileAr, ClientConnection->QueuedDemoPackets, DemoCurrentTime);
	
	AdjustConsiderTime((float)Params.NumActorsReplicated / (float)NumPrioritizedActors);
}

bool UDemoNetDriver::ReplicatePrioritizedActor(const FActorPriority& ActorPriority, const class FRepActorsParams& Params)
{
	FNetworkObjectInfo* ActorInfo = ActorPriority.ActorInfo;
	FActorDestructionInfo* DestructionInfo = ActorPriority.DestructionInfo;

	const double RecordStartTimeSeconds = FPlatformTime::Seconds();

	const bool bDoFindActorChannel = Params.bDoFindActorChannel;
	const bool bDoCheckDormancy = Params.bDoCheckDormancy;

	// Deletion entry
	if (ActorInfo == nullptr && DestructionInfo != nullptr)
	{
		UActorChannel* Channel = (UActorChannel*)ClientConnections[0]->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally);
		if (Channel)
		{
			UE_LOG(LogDemo, Verbose, TEXT("TickDemoRecord creating destroy channel for NetGUID <%s,%s> Priority: %d"), *DestructionInfo->NetGUID.ToString(), *DestructionInfo->PathName, ActorPriority.Priority);

			// Send a close bunch on the new channel
			Channel->SetChannelActorForDestroy(DestructionInfo);

			// Remove from connection's to-be-destroyed list (close bunch is reliable, so it will make it there)
			ClientConnections[0]->GetDestroyedStartupOrDormantActorGUIDs().Remove(DestructionInfo->NetGUID);
		}
	}
	else if (ActorInfo != nullptr && DestructionInfo == nullptr)
	{
		AActor* Actor = ActorInfo->Actor;
		
		if (bDoCheckDormancy)
		{
			UActorChannel* Channel = (bDoFindActorChannel ? ClientConnections[0]->FindActorChannelRef(Actor) : ActorPriority.Channel);
			if (Channel && ShouldActorGoDormantForDemo(Actor, Channel))
			{
				// Either shouldn't go dormant, or is already dormant
				Channel->StartBecomingDormant();
			}
		}

		// Use NetUpdateFrequency for this actor, but clamp it to RECORD_HZ.
		const float ClampedNetUpdateFrequency = FMath::Clamp(Actor->NetUpdateFrequency, Params.MinRecordHz, Params.MaxRecordHz);
		const double NetUpdateDelay = 1.0 / ClampedNetUpdateFrequency;

		// Set defaults if this actor is replicating for first time
		if (ActorInfo->LastNetReplicateTime == 0)
		{
			ActorInfo->LastNetReplicateTime = DemoCurrentTime;
			ActorInfo->OptimalNetUpdateDelta = NetUpdateDelay;
		}

		const float LastReplicateDelta = static_cast<float>(DemoCurrentTime - ActorInfo->LastNetReplicateTime);

		if (Actor->MinNetUpdateFrequency == 0.0f)
		{
			Actor->MinNetUpdateFrequency = 2.0f;
		}

		// Calculate min delta (max rate actor will update), and max delta (slowest rate actor will update)
		const float MinOptimalDelta = NetUpdateDelay;										// Don't go faster than NetUpdateFrequency
		const float MaxOptimalDelta = FMath::Max(1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta);	// Don't go slower than MinNetUpdateFrequency (or NetUpdateFrequency if it's slower)

		const float ScaleDownStartTime = 2.0f;
		const float ScaleDownTimeRange = 5.0f;

		if (LastReplicateDelta > ScaleDownStartTime)
		{
			// Interpolate between MinOptimalDelta/MaxOptimalDelta based on how long it's been since this actor actually sent anything
			const float Alpha = FMath::Clamp((LastReplicateDelta - ScaleDownStartTime) / ScaleDownTimeRange, 0.0f, 1.0f);
			ActorInfo->OptimalNetUpdateDelta = FMath::Lerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}

		const double NextUpdateDelta = Params.bUseAdapativeNetFrequency ? ActorInfo->OptimalNetUpdateDelta : NetUpdateDelay;

		// Account for being fractionally into the next frame
		// But don't be more than a fraction of a frame behind either (we don't want to do catch-up frames when there is a long delay)
		const double ExtraTime = DemoCurrentTime - ActorInfo->NextUpdateTime;
		const double ClampedExtraTime = FMath::Clamp(ExtraTime, 0.0, NetUpdateDelay);

		// Try to spread the updates across multiple frames to smooth out spikes.
		ActorInfo->NextUpdateTime = (DemoCurrentTime + NextUpdateDelta - ClampedExtraTime + ((FMath::SRand() - 0.5) * Params.ServerTickTime));

		Actor->CallPreReplication(this);

		const bool bDidReplicateActor = DemoReplicateActor(Actor, ClientConnections[0], false);

		const bool bUpdatedExternalData = UpdateExternalDataForActor(Actor);

		if (bDidReplicateActor || bUpdatedExternalData)
		{
			// Choose an optimal time, we choose 70% of the actual rate to allow frequency to go up if needed
			ActorInfo->OptimalNetUpdateDelta = FMath::Clamp(LastReplicateDelta * 0.7f, MinOptimalDelta, MaxOptimalDelta);
			ActorInfo->LastNetReplicateTime = DemoCurrentTime;
		}
	}
	else
	{
		UE_LOG(LogDemo, Warning, TEXT("TickDemoRecord: prioritized actor entry should have either an actor or a destruction info"));
	}

	// Make sure we're under the desired recording time quota, if any.
	if (Params.TimeLimitSeconds > 0.f)
	{
		const double RecordEndTimeSeconds = FPlatformTime::Seconds();
		const double RecordTimeSeconds = RecordEndTimeSeconds - RecordStartTimeSeconds;

		if ((ActorInfo && ActorInfo->Actor) && (RecordTimeSeconds > (Params.TimeLimitSeconds * 0.95f)))
		{
			UE_LOG(LogDemo, Verbose, TEXT("Actor %s took more than 95%% of maximum desired recording time. Actor: %.3fms. Max: %.3fms."),
				*ActorInfo->Actor->GetName(), RecordTimeSeconds * 1000.f, MaxDesiredRecordTimeMS);
		}

		const double TotalRecordTimeSeconds = (RecordEndTimeSeconds - Params.ReplicationStartTimeSeconds);

		if (TotalRecordTimeSeconds > Params.TimeLimitSeconds)
		{
			DemoNetDriverRecordingPrivate::LogDemoRecordTimeElapsed(TEXT("Exceeded maximum desired recording time (during Actor Replication).  Max: %.3fms."), MaxDesiredRecordTimeMS);
			return false;
		}
	}

	return true;
}

bool UDemoNetDriver::ReplicatePrioritizedActors(const FDemoActorPriority* ActorsToReplicate, uint32 Count, FRepActorsParams& Params)
{
	bool bTimeRemaining = true;
	uint32 It = 0;
	for (; It < Count; ++It)
	{
		const FActorPriority& ActorPriority = ActorsToReplicate[It].ActorPriority;
		bTimeRemaining = ReplicatePrioritizedActor(ActorPriority, Params);
		if (!bTimeRemaining)
		{
			++It;
			break;
		}
	}

	Params.NumActorsReplicated += It;
	return bTimeRemaining;
}

bool UDemoNetDriver::ShouldSaveCheckpoint()
{
	const double CHECKPOINT_DELAY = CVarCheckpointUploadDelayInSeconds.GetValueOnAnyThread();

	if (DemoCurrentTime - LastCheckpointTime > CHECKPOINT_DELAY)
	{
		return true;
	}

	return false;
}

void UDemoNetDriver::PauseChannels( const bool bPause )
{
	if ( bPause == bChannelsArePaused )
	{
		return;
	}

	// Pause all non player controller actors
	// FIXME: Would love a more elegant way of handling this at a more global level
	for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
	{
		UChannel* OpenChannel = ServerConnection->OpenChannels[i];

		UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );

		if ( ActorChannel == nullptr )
		{
			continue;
		}

		ActorChannel->CustomTimeDilation = bPause ? 0.0f : 1.0f;

		if ( ActorChannel->GetActor() == SpectatorController )
		{
			continue;
		}

		if ( ActorChannel->GetActor() == nullptr )
		{
			continue;
		}

		// Better way to pause each actor?
		ActorChannel->GetActor()->CustomTimeDilation = ActorChannel->CustomTimeDilation;
	}

	bChannelsArePaused = bPause;
}

bool UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets( FArchive& Ar, TArray<FPlaybackPacket>& InPlaybackPackets, const bool bForLevelFastForward, float* OutTime )
{
	SCOPED_NAMED_EVENT(UDemoNetDriver_ReadDemoFrameIntoPlaybackPackets, FColor::Purple);

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ReadDemoFrameIntoPlaybackPackets"), ReadDemoFrameIntoPlaybackPackets, STATGROUP_Net);

	check(!bForLevelFastForward || HasLevelStreamingFixes());

	if ( Ar.IsError() )
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: Archive Error"));
		NotifyDemoPlaybackFailure(EDemoPlayFailure::Serialization);
		return false;
	}

	if ( Ar.AtEnd() )
	{
		return false;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: ReplayStreamer ERROR: %s"), ENetworkReplayError::ToString(ReplayStreamer->GetLastError()));
		NotifyDemoPlaybackFailure(EDemoPlayFailure::ReplayStreamerInternal);
		return false;
	}

	// Above checks guarantee the Archive is in a valid state, but it's entirely possible that
	// the ReplayStreamer doesn't have more stream data available (i.e., if we only have checkpoint data).
	// Therefore, skip this if we know we're only reading in checkpoint data.
	if ( !bIsLoadingCheckpoint && !ReplayStreamer->IsDataAvailable() )
	{
		return false;
	}

	int32 ReadCurrentLevelIndex = 0;

	if (PlaybackDemoHeader.Version >= HISTORY_MULTIPLE_LEVELS)
	{
		Ar << ReadCurrentLevelIndex;
	}

	float TimeSeconds = 0.0f;

	Ar << TimeSeconds;

	if (OutTime)
	{
		*OutTime = TimeSeconds;
	}

	if (PlaybackDemoHeader.Version >= HISTORY_LEVEL_STREAMING_FIXES)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Demo_ReceiveExports"), Demo_ReceiveExports, STATGROUP_Net);
		((UPackageMapClient*)ServerConnection->PackageMap)->ReceiveExportData(Ar);
	}

	// Check to see if we can skip adding these packets.
	// This may happen if the archive isn't set to a proper position due to level fast forwarding.
	const bool bAppendPackets = bIsLoadingCheckpoint || bForLevelFastForward || LatestReadFrameTime < TimeSeconds;
	LatestReadFrameTime = FMath::Max(LatestReadFrameTime, TimeSeconds);

	if (HasLevelStreamingFixes())
	{
		uint32 NumStreamingLevels = 0;
		Ar.SerializeIntPacked(NumStreamingLevels);

		// We want to avoid adding the same levels to the Seen list multiple times.
		// This can occur if the Archive is "double read" due to a level fast forward.
		const bool bAddToSeenList = bAppendPackets && !bForLevelFastForward;

		FString NameTemp;
		for (uint32 i = 0; i < NumStreamingLevels; i++)
		{
			Ar << NameTemp;

			if (bAddToSeenList)
			{
				// Add this level to the seen list, but don't actually mark it as being seen.
				// It will be marked when we have processed packets for it.
				const FLevelStatus& LevelStatus = FindOrAddLevelStatus(NameTemp);
				SeenLevelStatuses.Add(LevelStatus.LevelIndex);
			}
		}
	}
	else
	{
		// Read any new streaming levels this frame
		uint32 NumStreamingLevels = 0;
		Ar.SerializeIntPacked(NumStreamingLevels);

		for (uint32 i = 0; i < NumStreamingLevels; ++i)
		{
			FString PackageName;
			FString PackageNameToLoad;
			FTransform LevelTransform;

			Ar << PackageName;
			Ar << PackageNameToLoad;
			Ar << LevelTransform;

			// Don't add if already exists
			bool bFound = false;

			for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
			{
				FString SrcPackageName = StreamingLevel->GetWorldAssetPackageName();
				FString SrcPackageNameToLoad = StreamingLevel->PackageNameToLoad.ToString();

				if (SrcPackageName == PackageName && SrcPackageNameToLoad == PackageNameToLoad)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				continue;
			}

			ULevelStreamingDynamic* StreamingLevel = NewObject<ULevelStreamingDynamic>(World, NAME_None, RF_NoFlags, nullptr);

			StreamingLevel->SetShouldBeLoaded(true);
			StreamingLevel->SetShouldBeVisible(true);
			StreamingLevel->bShouldBlockOnLoad = false;
			StreamingLevel->bInitiallyLoaded = true;
			StreamingLevel->bInitiallyVisible = true;
			StreamingLevel->LevelTransform = LevelTransform;

			StreamingLevel->PackageNameToLoad = FName(*PackageNameToLoad);
			StreamingLevel->SetWorldAssetByPackageName(FName(*PackageName));

			World->AddStreamingLevel(StreamingLevel);

			UE_LOG(LogDemo, Log, TEXT("ReadDemoFrameIntoPlaybackPackets: Loading streamingLevel: %s, %s"), *PackageName, *PackageNameToLoad);
		}
	}
	

#if DEMO_CHECKSUMS == 1
	{
		uint32 ServerDeltaTimeCheksum = 0;
		Ar << ServerDeltaTimeCheksum;

		const uint32 DeltaTimeChecksum = FCrc::MemCrc32(&TimeSeconds, sizeof(TimeSeconds), 0);

		if (DeltaTimeChecksum != ServerDeltaTimeCheksum)
		{
			UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: DeltaTimeChecksum != ServerDeltaTimeCheksum"));
			NotifyDemoPlaybackFailure(EDemoPlayFailure::Generic);
			return false;
		}
	}
#endif

	if (Ar.IsError())
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: Failed to read demo ServerDeltaTime"));
		NotifyDemoPlaybackFailure(EDemoPlayFailure::Serialization);
		return false;
	}

	FArchivePos SkipExternalOffset = 0;
	if (HasLevelStreamingFixes())
	{
		Ar << SkipExternalOffset;
	}

	if (!bForLevelFastForward)
	{
		// Load any custom external data in this frame
		LoadExternalData(Ar, TimeSeconds);
	}
	else
	{
		Ar.Seek(Ar.Tell() + SkipExternalOffset);
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Demo_ReadPackets"), Demo_ReadPackets, STATGROUP_Net);

		FPlaybackPacket ScratchPacket;
		ScratchPacket.TimeSeconds = TimeSeconds;
		ScratchPacket.LevelIndex = ReadCurrentLevelIndex;
		ScratchPacket.SeenLevelIndex = INDEX_NONE;

		const EReadPacketMode ReadPacketMode = bAppendPackets ? EReadPacketMode::Default : EReadPacketMode::SkipData;

		while (true)
		{
			if (HasLevelStreamingFixes())
			{
				Ar.SerializeIntPacked(ScratchPacket.SeenLevelIndex);
			}

			switch (ReadPacket(Ar, ScratchPacket.Data, ReadPacketMode))
			{
				case EReadPacketState::Error:
				{
					UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: ReadPacket failed."));
					NotifyDemoPlaybackFailure(EDemoPlayFailure::Serialization);
					return false;
				}

				case EReadPacketState::Success:
				{
					if (EReadPacketMode::SkipData == ReadPacketMode)
					{
						continue;
					}

					InPlaybackPackets.Emplace(MoveTemp(ScratchPacket));
					ScratchPacket.Data = TArray<uint8>();
					break;
				}

				case EReadPacketState::End:
				{
					return true;
				}

				default:
				{
					check(false);
					return false;
				}
			}
		}
	}

	// We should never hit this, as the while loop above should return on error or success.
	check(false);
	return false;
}

void UDemoNetDriver::ProcessSeamlessTravel(int32 LevelIndex)
{
	// Destroy all player controllers since FSeamlessTravelHandler will not destroy them.
	TArray<AController*> Controllers;
	for (FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator)
	{
		Controllers.Add(Iterator->Get());
	}

	for (int i = 0; i < Controllers.Num(); i++)
	{
		if (Controllers[i])
		{
			// bNetForce is true so that the replicated spectator player controller will
			// be destroyed as well.
			Controllers[i]->Destroy(true);
		}
	}

	// Set this to nullptr since we just destroyed it.
	SpectatorController = nullptr;

	if (PlaybackDemoHeader.LevelNamesAndTimes.IsValidIndex(LevelIndex))
	{
		World->SeamlessTravel(PlaybackDemoHeader.LevelNamesAndTimes[LevelIndex].LevelName, true);
	}
	else
	{
		// If we're watching a live replay, it's probable that the header has been updated with the level added,
		// so we need to download it again before proceeding.
		bIsWaitingForHeaderDownload = true;
		ReplayStreamer->DownloadHeader(FDownloadHeaderCallback::CreateUObject(this, &UDemoNetDriver::OnDownloadHeaderCompletePrivate, LevelIndex));
	}
}

void UDemoNetDriver::OnDownloadHeaderCompletePrivate(const FDownloadHeaderResult& Result, int32 LevelIndex)
{
	bIsWaitingForHeaderDownload = false;

	if (Result.WasSuccessful())
	{
		FString Error;
		if (ReadPlaybackDemoHeader(Error))
		{
			if (PlaybackDemoHeader.LevelNamesAndTimes.IsValidIndex(LevelIndex))
			{
				ProcessSeamlessTravel(LevelIndex);
			}
			else
			{
				World->GetGameInstance()->HandleDemoPlaybackFailure(EDemoPlayFailure::Corrupt, FString::Printf(TEXT("UDemoNetDriver::OnDownloadHeaderComplete: LevelIndex %d not in range of level names of size: %d"), LevelIndex, PlaybackDemoHeader.LevelNamesAndTimes.Num()));
			}
		}
		else
		{
			World->GetGameInstance()->HandleDemoPlaybackFailure(EDemoPlayFailure::Corrupt, FString::Printf(TEXT("UDemoNetDriver::OnDownloadHeaderComplete: ReadPlaybackDemoHeader header failed with error %s."), *Error));
		}
	}
	else
	{
		World->GetGameInstance()->HandleDemoPlaybackFailure(EDemoPlayFailure::Corrupt, FString::Printf(TEXT("UDemoNetDriver::OnDownloadHeaderComplete: Downloading header failed.")));
	}
}

bool UDemoNetDriver::ConditionallyReadDemoFrameIntoPlaybackPackets( FArchive& Ar )
{
	if ( PlaybackPackets.Num() > 0 )
	{
		const float MAX_PLAYBACK_BUFFER_SECONDS = 5.0f;

		const FPlaybackPacket& LastPacket = PlaybackPackets.Last();
		if ( LastPacket.TimeSeconds > DemoCurrentTime && LastPacket.TimeSeconds - DemoCurrentTime > MAX_PLAYBACK_BUFFER_SECONDS )
		{
			return false;	// Don't buffer more than MAX_PLAYBACK_BUFFER_SECONDS worth of frames
		}
	}

	if ( !ReadDemoFrameIntoPlaybackPackets( Ar ) )
	{
		return false;
	}

	return true;
}

// Deprecated, DO NOT USE.
bool UDemoNetDriver::ReadPacket( FArchive& Archive, uint8* OutReadBuffer, int32& OutBufferSize, const int32 MaxBufferSize )
{
	OutBufferSize = 0;

	Archive << OutBufferSize;

	if ( Archive.IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: Failed to read demo OutBufferSize" ) );
		return false;
	}

	if ( OutBufferSize == 0 )
	{
		return true;		// Done
	}

	if ( OutBufferSize > MaxBufferSize )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: OutBufferSize > sizeof( ReadBuffer )" ) );
		return false;
	}

	// Read data from file.
	Archive.Serialize( OutReadBuffer, OutBufferSize );

	if ( Archive.IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: Failed to read demo file packet" ) );
		return false;
	}

#if DEMO_CHECKSUMS == 1
	{
		uint32 ServerChecksum = 0;
		Archive << ServerChecksum;

		const uint32 Checksum = FCrc::MemCrc32( OutReadBuffer, OutBufferSize, 0 );

		if ( Checksum != ServerChecksum )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: Checksum != ServerChecksum" ) );
			return false;
		}
	}
#endif

	return true;
}

const UDemoNetDriver::EReadPacketState UDemoNetDriver::ReadPacket(FArchive& Archive, TArray<uint8>& OutBuffer, const EReadPacketMode Mode)
{
	const bool bSkipData = (EReadPacketMode::SkipData == Mode);

	int32 BufferSize = 0;
	Archive << BufferSize;

	if (UNLIKELY(Archive.IsError()))
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadPacket: Failed to read demo OutBufferSize"));
		return EReadPacketState::Error;
	}

	if (BufferSize == 0)
	{
		return EReadPacketState::End;
	}

	else if (UNLIKELY(BufferSize > MAX_DEMO_READ_WRITE_BUFFER))
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadPacket: OutBufferSize > MAX_DEMO_READ_WRITE_BUFFER"));
		return EReadPacketState::Error;
	}
	else if (UNLIKELY(BufferSize < 0))
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadPacket: OutBufferSize < 0"));
		return EReadPacketState::Error;
	}

	if (bSkipData)
	{
		Archive.Seek(Archive.Tell() + static_cast<int64>(BufferSize));
	}
	else
	{
		OutBuffer.SetNumUninitialized(BufferSize, false);
		Archive.Serialize(OutBuffer.GetData(), BufferSize);
	}

	if (UNLIKELY(Archive.IsError()))
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadPacket: Failed to read demo file packet"));
		return EReadPacketState::Error;
	}

#if DEMO_CHECKSUMS == 1
	// When skipping data, skip checksums too.
	// It implies the data was read elsewhere.
	if (bSkipData)
	{
		Archive.Seek(Archive.Tell() + static_cast<int64>(sizeof(uint32)));
	}
	else
	{
		uint32 ServerChecksum = 0;
		Archive << ServerChecksum;

		const uint32 Checksum = FCrc::MemCrc32(OutReadBuffer, OutBufferSize, 0);

		if (Checksum != ServerChecksum)
		{
			UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::ReadPacket: Checksum != ServerChecksum"));
			return EReadPacketState::Error;
		}
	}
#endif

	return EReadPacketState::Success;
}

bool UDemoNetDriver::ShouldSkipPlaybackPacket(const FPlaybackPacket& Packet)
{
	if (HasLevelStreamingFixes() && Packet.SeenLevelIndex != 0)
	{
		if (SeenLevelStatuses.IsValidIndex(Packet.SeenLevelIndex - 1))
		{
			// Flag the status as being seen, since we're potentially going to process it.
			// We need to skip processing if it's not ready (in that case, we'll do a fast-forward).
			FLevelStatus& LevelStatus = GetLevelStatus(Packet.SeenLevelIndex);
			LevelStatus.bHasBeenSeen = true;
			return !LevelStatus.bIsReady;
		}
		else
		{
			UE_LOG(LogDemo, Warning, TEXT("ShouldSkipPlaybackPacket encountered a packet with an invalid seen level index."));
		}
	}

	return false;
}

bool UDemoNetDriver::ConditionallyProcessPlaybackPackets()
{
	if ( !PlaybackPackets.IsValidIndex(PlaybackPacketIndex) )
	{
		PauseChannels( true );
		return false;
	}

	const FPlaybackPacket& CurPacket = PlaybackPackets[PlaybackPacketIndex];
	if ( DemoCurrentTime < CurPacket.TimeSeconds )
	{
		// Not enough time has passed to read another frame
		return false;
	}

	if (CurPacket.LevelIndex != CurrentLevelIndex)
	{
		GetWorld()->GetGameInstance()->OnSeamlessTravelDuringReplay();
		CurrentLevelIndex = CurPacket.LevelIndex;
		ProcessSeamlessTravel(CurrentLevelIndex);
		return false;
	}

	++PlaybackPacketIndex;
	return ProcessPacket(CurPacket);
}

void UDemoNetDriver::ProcessAllPlaybackPackets()
{
	if (PlaybackPackets.Num() > 0)
	{
		for (const FPlaybackPacket& PlaybackPacket : PlaybackPackets)
		{
			ProcessPacket(PlaybackPacket);
		}

		LastProcessedPacketTime = PlaybackPackets.Last().TimeSeconds;
		PlaybackPackets.Empty();
	}
}

bool UDemoNetDriver::ProcessPacket( const uint8* Data, int32 Count )
{
	PauseChannels( false );

	if ( ServerConnection != nullptr )
	{
		// Process incoming packet.
		// ReceivedRawPacket shouldn't change any data, so const_cast should be safe.
		ServerConnection->ReceivedRawPacket( const_cast<uint8*>(Data), Count );
	}

	if ( ServerConnection == nullptr || ServerConnection->State == USOCK_Closed )
	{
		// Something we received resulted in the demo being stopped
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ProcessPacket: ReceivedRawPacket closed connection" ) );
		NotifyDemoPlaybackFailure(EDemoPlayFailure::Generic);
		return false;
	}

	return true;
}

void UDemoNetDriver::WriteDemoFrameFromQueuedDemoPackets( FArchive& Ar, TArray<FQueuedDemoPacket>& QueuedPackets, float FrameTime )
{
	Ar << CurrentLevelIndex;
	
	// Save total absolute demo time in seconds
	Ar << FrameTime;

	((UPackageMapClient*)ClientConnections[0]->PackageMap)->AppendExportData(Ar);

	if (HasLevelStreamingFixes())
	{
		uint32 NumStreamingLevels = AllLevelStatuses.Num();
		Ar.SerializeIntPacked(NumLevelsAddedThisFrame);

		for (uint32 i = NumStreamingLevels - NumLevelsAddedThisFrame; i < NumStreamingLevels; i++)
		{
			Ar << AllLevelStatuses[i].LevelName;
		}

		NumLevelsAddedThisFrame = 0;
	}
	else
	{
		// Save any new streaming levels
		uint32 NumStreamingLevels = NewStreamingLevelsThisFrame.Num();
		Ar.SerializeIntPacked(NumStreamingLevels);

		for (uint32 i = 0; i < NumStreamingLevels; i++)
		{
			ULevelStreaming* StreamingLevel = World->GetStreamingLevels()[i];

			// TODO: StreamingLevel could be null, but since we've already written out the integer count, skipping entries could cause an issue, so leaving as is for now
			FString PackageName = StreamingLevel->GetWorldAssetPackageName();
			FString PackageNameToLoad = StreamingLevel->PackageNameToLoad.ToString();

			Ar << PackageName;
			Ar << PackageNameToLoad;
			Ar << StreamingLevel->LevelTransform;

			UE_LOG(LogDemo, Log, TEXT("WriteDemoFrameFromQueuedDemoPackets: StreamingLevel: %s, %s"), *PackageName, *PackageNameToLoad);
		}

		NewStreamingLevelsThisFrame.Empty();
	}


	{
		TUniquePtr<FScopedStoreArchiveOffset> ScopedOffset(HasLevelStreamingFixes() ? new FScopedStoreArchiveOffset(Ar) : nullptr);

		// Save external data
		SaveExternalData( Ar );
	}

	for (FQueuedDemoPacket& DemoPacket : QueuedPackets)
	{
		if (HasLevelStreamingFixes())
		{
			Ar.SerializeIntPacked(DemoPacket.SeenLevelIndex);
		}

		WritePacket(Ar, DemoPacket.Data.GetData(), DemoPacket.Data.Num());
	}

	QueuedPackets.Empty();

	if (HasLevelStreamingFixes())
	{
		uint32 EndCountUnsigned = 0;
		Ar.SerializeIntPacked(EndCountUnsigned);
	}

	// Write a count of 0 to signal the end of the frame
	int32 EndCount = 0;
	Ar << EndCount;
}

void UDemoNetDriver::WritePacket( FArchive& Ar, uint8* Data, int32 Count )
{
	Ar << Count;
	Ar.Serialize( Data, Count );

#if DEMO_CHECKSUMS == 1
	uint32 Checksum = FCrc::MemCrc32( Data, Count, 0 );
	Ar << Checksum;
#endif
}

void UDemoNetDriver::SkipTime(const float InTimeToSkip)
{
	if ( IsNamedTaskInQueue( ReplayTaskNames::SkipTimeInSecondsTask ) )
	{
		return;		// Don't allow time skipping if we already are
	}

	AddReplayTask( new FSkipTimeInSecondsTask( this, InTimeToSkip ) );
}

void UDemoNetDriver::SkipTimeInternal( const float SecondsToSkip, const bool InFastForward, const bool InIsForCheckpoint )
{
	check( !bIsFastForwarding );				// Can only do one of these at a time (use tasks to gate this)
	check( !bIsFastForwardingForCheckpoint );	// Can only do one of these at a time (use tasks to gate this)

	SavedSecondsToSkip = SecondsToSkip;
	DemoCurrentTime += SecondsToSkip;

	DemoCurrentTime = FMath::Clamp( DemoCurrentTime, 0.0f, DemoTotalTime - 0.01f );

	bIsFastForwarding				= InFastForward;
	bIsFastForwardingForCheckpoint	= InIsForCheckpoint;
}

void UDemoNetDriver::GotoTimeInSeconds(const float TimeInSeconds, const FOnGotoTimeDelegate& InOnGotoTimeDelegate)
{
	OnGotoTimeDelegate_Transient = InOnGotoTimeDelegate;

	if ( IsNamedTaskInQueue( ReplayTaskNames::GotoTimeInSecondsTask ) || bIsFastForwarding )
	{
		NotifyGotoTimeFinished(false);
		return;		// Don't allow scrubbing if we already are
	}

	UE_LOG(LogDemo, Log, TEXT("GotoTimeInSeconds: %2.2f"), TimeInSeconds);

	AddReplayTask( new FGotoTimeInSecondsTask( this, TimeInSeconds ) );
}

void UDemoNetDriver::JumpToEndOfLiveReplay()
{
	UE_LOG( LogDemo, Log, TEXT( "UDemoNetConnection::JumpToEndOfLiveReplay." ) );

	const uint32 TotalDemoTimeInMS = ReplayStreamer->GetTotalDemoTime();

	DemoTotalTime = (float)TotalDemoTimeInMS / 1000.0f;

	const uint32 BufferInMS = 5 * 1000;

	const uint32 JoinTimeInMS = FMath::Max( (uint32)0, ReplayStreamer->GetTotalDemoTime() - BufferInMS );

	if ( JoinTimeInMS > 0 )
	{
		GotoTimeInSeconds( (float)JoinTimeInMS / 1000.0f );
	}
}

void UDemoNetDriver::AddUserToReplay( const FString& UserString )
{
	if ( ReplayStreamer.IsValid() )
	{
		ReplayStreamer->AddUserToReplay( UserString );
	}
}

#if (CSV_PROFILER && (!UE_BUILD_SHIPPING))
struct FCsvDemoSettings
{
	bool bCaptureCsv;
	int32 StartTime;
	int32 EndTime;
	int32 FrameCount;
};

static FCsvDemoSettings GetCsvDemoSettings()
{
	FCsvDemoSettings Settings = {};
	Settings.bCaptureCsv = FParse::Value(FCommandLine::Get(), TEXT("-csvdemostarttime="), Settings.StartTime);
	if (Settings.bCaptureCsv)
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("-csvdemoendtime="), Settings.EndTime))
			Settings.EndTime = -1.0;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-csvdemoframecount="), Settings.FrameCount))
			Settings.FrameCount = -1;
	}

	return Settings;
}
#endif // (CSV_PROFILER && (!UE_BUILD_SHIPPING))

void UDemoNetDriver::TickDemoPlayback( float DeltaSeconds )
{
	SCOPED_NAMED_EVENT(UDemoNetDriver_TickDemoPlayback, FColor::Purple);
	if ( World && World->IsInSeamlessTravel() )
	{
		return;
	}

#if (CSV_PROFILER && (!UE_BUILD_SHIPPING))
	{
		static FCsvDemoSettings CsvDemoSettings = GetCsvDemoSettings();
		if (CsvDemoSettings.bCaptureCsv)
		{
			bool bDoCapture = IsPlaying()
				&& DemoCurrentTime >= CsvDemoSettings.StartTime
				&& ((DemoCurrentTime <= CsvDemoSettings.EndTime) || (CsvDemoSettings.EndTime < 0));

			static bool bStartedCsvRecording = false;
			if (!bStartedCsvRecording && bDoCapture)
			{
				FCsvProfiler::Get()->BeginCapture(CsvDemoSettings.FrameCount);
				bStartedCsvRecording = true;
			}
			else if (bStartedCsvRecording && !bDoCapture)
			{
				FCsvProfiler::Get()->EndCapture();
				bStartedCsvRecording = false;
			}
		}
	}
#endif // (CSV_PROFILER && (!UE_BUILD_SHIPPING))

	if ( !IsPlaying() )
	{
		return;
	}
	
	// This will be true when watching a live replay and we're grabbing an up to date header.
	// In that case, we want to pause playback until we can actually travel.
	if ( bIsWaitingForHeaderDownload )
	{
		return;
	}

	if ( CVarForceDisableAsyncPackageMapLoading.GetValueOnGameThread() > 0 )
	{
		GuidCache->SetAsyncLoadMode( FNetGUIDCache::EAsyncLoadMode::ForceDisable );
	}
	else
	{
		GuidCache->SetAsyncLoadMode( FNetGUIDCache::EAsyncLoadMode::UseCVar );
	}

	if ( CVarGotoTimeInSeconds.GetValueOnGameThread() >= 0.0f )
	{
		GotoTimeInSeconds( CVarGotoTimeInSeconds.GetValueOnGameThread() );
		CVarGotoTimeInSeconds.AsVariable()->Set( TEXT( "-1" ), ECVF_SetByConsole );
	}

	if (FMath::Abs(CVarDemoSkipTime.GetValueOnGameThread()) > 0.0f)
	{
		// Just overwrite existing value, cvar wins in this case
		GotoTimeInSeconds(DemoCurrentTime + CVarDemoSkipTime.GetValueOnGameThread());
		CVarDemoSkipTime.AsVariable()->Set(TEXT("0"), ECVF_SetByConsole);
	}

	// Before we update tasks or move the demo time forward, see if there are any new sublevels that
	// need to be fast forwarded.
	PrepFastForwardLevels();

	// Update total demo time
	if ( ReplayStreamer->GetTotalDemoTime() > 0 )
	{
		DemoTotalTime = ( float )ReplayStreamer->GetTotalDemoTime() / 1000.0f;
	}

	if ( !ProcessReplayTasks() )
	{
		// We're busy processing tasks, return
		return;
	}
	
	// If the ExitAfterReplay option is set, automatically shut down at the end of the replay.
	// Use AtEnd() of the archive instead of checking DemoCurrentTime/DemoTotalTime, because the DemoCurrentTime may never catch up to DemoTotalTime.
	if (FArchive* const StreamingArchive = ReplayStreamer->GetStreamingArchive())
	{
		const bool bIsAtEnd = StreamingArchive->AtEnd() && (PlaybackPackets.Num() == 0 || (DemoCurrentTime + DeltaSeconds >= DemoTotalTime));
		if (!ReplayStreamer->IsLive() && bIsAtEnd)
		{
			OnDemoFinishPlaybackDelegate.Broadcast();

			if (FParse::Param(FCommandLine::Get(), TEXT("ExitAfterReplay")))
			{
				FPlatformMisc::RequestExit(false);
			}

			if (CVarLoopDemo.GetValueOnGameThread() > 0)
			{
				GotoTimeInSeconds(0.0f);
			}
		}
	}

	// Advance demo time by seconds passed if we're not paused
	if ( World->GetWorldSettings()->Pauser == nullptr )
	{
		DemoCurrentTime += DeltaSeconds;
	}

	// Clamp time
	DemoCurrentTime = FMath::Clamp( DemoCurrentTime, 0.0f, DemoTotalTime - 0.01f );

	// Make sure there is data available to read
	// If we're at the end of the demo, just pause channels and return
	if (bDemoPlaybackDone || (!PlaybackPackets.Num() && !ReplayStreamer->IsDataAvailable()))
	{
		PauseChannels(true);
		return;
	}

	// Speculatively grab seconds now in case we need it to get the time it took to fast forward
	const double FastForwardStartSeconds = FPlatformTime::Seconds();

	if (FArchive* const StreamingArchive = ReplayStreamer->GetStreamingArchive())
	{
		StreamingArchive->SetEngineNetVer(PlaybackDemoHeader.EngineNetworkProtocolVersion);
		StreamingArchive->SetGameNetVer(PlaybackDemoHeader.GameNetworkProtocolVersion);
	}

	// Buffer up demo frames until we have enough time built-up
	while ( ConditionallyReadDemoFrameIntoPlaybackPackets( *ReplayStreamer->GetStreamingArchive() ) )
	{
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TickDemoPlayback_ProcessPackets"), TickDemoPlayback_ProcessPackets, STATGROUP_Net);

	// Process packets until we are caught up (this implicitly handles fast forward if DemoCurrentTime past many frames)
	while ( ConditionallyProcessPlaybackPackets() )
	{
		DemoFrameNum++;
	}

		if (PlaybackPacketIndex > 0)
		{
			// Remove all packets that were processed
			// At this point, PlaybackPacketIndex will actually be the number of packets we've processed,
			// as it points to the "next" index we would otherwise have processed.
			LastProcessedPacketTime = PlaybackPackets[PlaybackPacketIndex - 1].TimeSeconds;

			PlaybackPackets.RemoveAt(0, PlaybackPacketIndex);
			PlaybackPacketIndex = 0;
		}
	}

	// Finalize any fast forward stuff that needs to happen
	if ( bIsFastForwarding )
	{
		FinalizeFastForward( FastForwardStartSeconds );
	}
}

void UDemoNetDriver::FinalizeFastForward( const double StartTime )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Demo_FinalizeFastForward"), Demo_FinalizeFastForward, STATGROUP_Net);

	// This must be set before we CallRepNotifies or they might be skipped again
	bIsFastForwarding = false;

	AGameStateBase* const GameState = World != nullptr ? World->GetGameState() : nullptr;

	// Make sure that we delete any Rewind actors that aren't valid anymore.
	if (bIsFastForwardingForCheckpoint)
	{
		CleanupOutstandingRewindActors();
	}	

	// Correct server world time for fast-forwarding after a checkpoint
	if (GameState != nullptr)
	{
		if (bIsFastForwardingForCheckpoint)
		{
			const float PostCheckpointServerTime = SavedReplicatedWorldTimeSeconds + SavedSecondsToSkip;
			GameState->ReplicatedWorldTimeSeconds = PostCheckpointServerTime;
		}

		// Correct the ServerWorldTimeSecondsDelta
		GameState->OnRep_ReplicatedWorldTimeSeconds();
	}

	if ( ServerConnection != nullptr && bIsFastForwardingForCheckpoint )
	{
		// Make a pass at OnReps for startup actors, since they were skipped during checkpoint loading.
		// At this point the shadow state of these actors should be the actual state from before the checkpoint,
		// and the current state is the CDO state evolved by any changes that occurred during checkpoint loading and fast-forwarding.
		for (UChannel* Channel : ServerConnection->OpenChannels)
		{
			UActorChannel* const ActorChannel = Cast<UActorChannel>(Channel);
			if (ActorChannel == nullptr)
			{
				continue;
			}

			const AActor* const Actor = ActorChannel->GetActor();
			if (Actor == nullptr)
			{
				continue;
			}

			const FObjectReplicator* const ActorReplicator = ActorChannel->ActorReplicator;
			if (Actor->IsNetStartupActor() && ActorReplicator)
			{
				FRepShadowDataBuffer ShadowData(ActorReplicator->RepState->StaticBuffer.GetData());
				FConstRepObjectDataBuffer ActorData(Actor);

				ActorReplicator->RepLayout->DiffProperties(&(ActorReplicator->RepState->RepNotifies), ShadowData, ActorData, EDiffPropertiesFlags::Sync);
			}
		}
	}

	// Flush all pending RepNotifies that were built up during the fast-forward.
	if ( ServerConnection != nullptr )
	{
		for ( auto& ChannelPair : ServerConnection->ActorChannelMap() )
		{
			if ( ChannelPair.Value != nullptr )
			{
				for ( auto& ReplicatorPair : ChannelPair.Value->ReplicationMap )
				{
					ReplicatorPair.Value->CallRepNotifies( true );
				}
			}
		}

		for (auto& DormantPair : ServerConnection->DormantReplicatorMap)
		{
			DormantPair.Value->CallRepNotifies( true );
		}
	}

	// We may have been fast-forwarding immediately after loading a checkpoint
	// for fine-grained scrubbing. If so, at this point we are no longer loading a checkpoint.
	bIsFastForwardingForCheckpoint = false;

	// Reset the never-queue GUID list, we'll rebuild it
	NonQueuedGUIDsForScrubbing.Reset();

	const double FastForwardTotalSeconds = FPlatformTime::Seconds() - StartTime;

	NotifyGotoTimeFinished(true);

	UE_LOG( LogDemo, Log, TEXT( "Fast forward took %.2f seconds." ), FastForwardTotalSeconds );
}

void UDemoNetDriver::SpawnDemoRecSpectator( UNetConnection* Connection, const FURL& ListenURL )
{
	// Optionally skip spawning the demo spectator if requested via the URL option
	if (ListenURL.HasOption(TEXT("SkipSpawnSpectatorController")))
	{
		return;
	}

	check( Connection != nullptr );

	// Get the replay spectator controller class from the default game mode object,
	// since the game mode instance isn't replicated to clients of live games.
	AGameStateBase* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	TSubclassOf<AGameModeBase> DefaultGameModeClass = GameState != nullptr ? GameState->GameModeClass : nullptr;
	
	// If we don't have a game mode class from the world, try to get it from the URL option.
	// This may be true on clients who are recording a replay before the game mode class was replicated to them.
	if (DefaultGameModeClass == nullptr)
	{
		const TCHAR* URLGameModeClass = ListenURL.GetOption(TEXT("game="), nullptr);
		if (URLGameModeClass != nullptr)
		{
			UClass* GameModeFromURL = StaticLoadClass(AGameModeBase::StaticClass(), nullptr, URLGameModeClass);
			DefaultGameModeClass = GameModeFromURL;
		}
	}

	AGameModeBase* DefaultGameMode = DefaultGameModeClass.GetDefaultObject();
	UClass* C = DefaultGameMode != nullptr ? DefaultGameMode->ReplaySpectatorPlayerControllerClass : nullptr;

	if ( C == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::SpawnDemoRecSpectator: Failed to load demo spectator class." ) );
		return;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want these to save into a map
	SpectatorController = World->SpawnActor<APlayerController>( C, SpawnInfo );

	if ( SpectatorController == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::SpawnDemoRecSpectator: Failed to spawn demo spectator." ) );
		return;
	}

	// Streaming volumes logic must not be affected by replay spectator camera
	SpectatorController->bIsUsingStreamingVolumes = false;

	// Make sure SpectatorController->GetNetDriver returns this driver. Ensures functions that depend on it,
	// such as IsLocalController, work as expected.
	SpectatorController->SetNetDriverName(NetDriverName);

	// If the controller doesn't have a player state, we are probably recording on a client.
	// Spawn one manually.
	if ( SpectatorController->PlayerState == nullptr && GetWorld() != nullptr && GetWorld()->IsRecordingClientReplay())
	{
		SpectatorController->InitPlayerState();
	}

	// Tell the game that we're spectator and not a normal player
	if (SpectatorController->PlayerState)
	{
		SpectatorController->PlayerState->bOnlySpectator = true;
	}

	for ( FActorIterator It( World ); It; ++It)
	{
		if ( It->IsA( APlayerStart::StaticClass() ) )
		{
			SpectatorController->SetInitialLocationAndRotation( It->GetActorLocation(), It->GetActorRotation() );
			break;
		}
	}
	
	SpectatorController->SetReplicates( true );
	SpectatorController->SetAutonomousProxy( true );

	SpectatorController->SetPlayer( Connection );
}

void UDemoNetDriver::ReplayStreamingReady( const FStartStreamingResult& Result )
{
	bIsWaitingForStream = false;
	bWasStartStreamingSuccessful = Result.WasSuccessful();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( CVarDemoForceFailure.GetValueOnGameThread() == 1 )
	{
		bWasStartStreamingSuccessful = false;
	}
#endif

	if ( !bWasStartStreamingSuccessful )
	{
		UE_LOG(LogDemo, Warning, TEXT("UDemoNetConnection::ReplayStreamingReady: Failed. %s"), Result.bRecording ? TEXT("") : EDemoPlayFailure::ToString(EDemoPlayFailure::DemoNotFound));

		if (Result.bRecording)
		{
			StopDemo();
		}
		else
		{
			NotifyDemoPlaybackFailure(EDemoPlayFailure::DemoNotFound);
		}
		return;
	}



	if ( !Result.bRecording )
	{
		FString Error;
		
		const double StartTime = FPlatformTime::Seconds();

		if ( !InitConnectInternal( Error ) )
		{
			return;
		}

		// InitConnectInternal calls ResetDemoState which will reset this, so restore the value
		bWasStartStreamingSuccessful = Result.WasSuccessful();

		const TCHAR* const SkipToLevelIndexOption = DemoURL.GetOption(TEXT("SkipToLevelIndex="), nullptr);
		if (SkipToLevelIndexOption)
		{
			int32 Index = FCString::Atoi(SkipToLevelIndexOption);
			if (Index < LevelNamesAndTimes.Num())
			{
				AddReplayTask(new FGotoTimeInSecondsTask(this, (float)LevelNamesAndTimes[Index].LevelChangeTimeInMS / 1000.0f));
			}
		}

		if ( ReplayStreamer->IsLive() && ReplayStreamer->GetTotalDemoTime() > 15 * 1000 )
		{
			// If the load time wasn't very long, jump to end now
			// Otherwise, defer it until we have a more recent replay time
			if ( FPlatformTime::Seconds() - StartTime < 10 )
			{
				JumpToEndOfLiveReplay();
			}
			else
			{
				UE_LOG( LogDemo, Log, TEXT( "UDemoNetConnection::ReplayStreamingReady: Deferring checkpoint until next available time." ) );
				AddReplayTask( new FJumpToLiveReplayTask( this ) );
			}
		}

		UE_LOG(LogDemo, Log, TEXT("ReplayStreamingReady: playing back replay [%s] %s, which was recorded on engine version %s"),
			*PlaybackDemoHeader.Guid.ToString(EGuidFormats::Digits), *DemoURL.Map, *PlaybackDemoHeader.EngineVersion.ToString());

		// Notify all listeners that a demo is starting
		OnDemoStarted.Broadcast(this);
	}
}

FReplayExternalDataArray* UDemoNetDriver::GetExternalDataArrayForObject( UObject* Object )
{
	FNetworkGUID NetworkGUID = GuidCache->NetGUIDLookup.FindRef( Object );

	if ( !NetworkGUID.IsValid() )
	{
		return nullptr;
	}

	return ExternalDataToObjectMap.Find( NetworkGUID );
}

void UDemoNetDriver::RespawnNecessaryNetStartupActors(TArray<AActor*>& SpawnedActors, ULevel* Level /* = nullptr */)
{
	for ( auto It = RollbackNetStartupActors.CreateIterator(); It; ++It )
	{
		if ( DeletedNetStartupActors.Contains( It.Key() ) )
		{
			// We don't want to re-create these since they should no longer exist after the current checkpoint
			continue;
		}

		FRollbackNetStartupActorInfo& RollbackActor = It.Value();

		// filter to a specific level
		if ((Level != nullptr) && (RollbackActor.Level != Level))
		{
			continue;
		}

		if (HasLevelStreamingFixes())
		{
			// skip rollback actors in streamed out levels (pending gc)
			if (!LevelStatusesByName.Contains(GetLevelPackageName(*RollbackActor.Level)))
			{
				continue;
			}
		}

		AActor* ExistingActor = FindObjectFast<AActor>(RollbackActor.Level, RollbackActor.Name);
		if (ExistingActor)
		{
			check(ExistingActor->IsPendingKillOrUnreachable());
			ExistingActor->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		}

		FActorSpawnParameters SpawnInfo;

		SpawnInfo.Template							= CastChecked<AActor>( RollbackActor.Archetype );
		SpawnInfo.SpawnCollisionHandlingOverride	= ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.bNoFail							= true;
		SpawnInfo.Name								= RollbackActor.Name;
		SpawnInfo.OverrideLevel						= RollbackActor.Level;
		SpawnInfo.bDeferConstruction				= true;

		const FTransform SpawnTransform = FTransform( RollbackActor.Rotation, RollbackActor.Location );

		AActor* Actor = GetWorld()->SpawnActorAbsolute( RollbackActor.Archetype->GetClass(), SpawnTransform, SpawnInfo );
		if (Actor)
		{
			if ( !ensure( Actor->GetFullName() == It.Key() ) )
			{
				UE_LOG( LogDemo, Log, TEXT( "RespawnNecessaryNetStartupActors: NetStartupRollbackActor name doesn't match original: %s, %s" ), *Actor->GetFullName(), *It.Key() );
			}

			bool bSanityCheckReferences = true;

			for(UObject* ObjRef : RollbackActor.ObjReferences)
			{
				if (ObjRef == nullptr)
				{
					bSanityCheckReferences = false;
					UE_LOG(LogDemo, Warning, TEXT("RespawnNecessaryNetStartupActors: Rollback actor reference was gc'd, skipping state restore: %s"), *GetFullNameSafe(Actor));
					break;
				}
			}

			TSharedPtr<FRepLayout> RepLayout = GetObjectClassRepLayout(Actor->GetClass());
			if (RepLayout.IsValid())
			{
				if (RepLayout.IsValid() && RollbackActor.RepState.IsValid() && bSanityCheckReferences)
				{
					const ENetRole SavedRole = Actor->Role;

					FRepObjectDataBuffer ActorData(Actor);
					FConstRepShadowDataBuffer ShadowData(RollbackActor.RepState->StaticBuffer.GetData());

					RepLayout->DiffStableProperties(&RollbackActor.RepState->RepNotifies, nullptr, ActorData, ShadowData);

					Actor->Role = SavedRole;
				}
			}

			check(Actor->GetRemoteRole() != ROLE_Authority);

			Actor->bNetStartup = true;

			UGameplayStatics::FinishSpawningActor(Actor, SpawnTransform);

			if (Actor->Role == ROLE_Authority)
			{
				Actor->SwapRoles();
			}

			if (RepLayout.IsValid() && RollbackActor.RepState.IsValid())
			{
				if (RollbackActor.RepState->RepNotifies.Num() > 0)
				{
					RepLayout->CallRepNotifies(RollbackActor.RepState.Get(), Actor);

					Actor->PostRepNotifies();
				}
			}

			for (UActorComponent* ActorComp : Actor->GetComponents())
			{
				if (ActorComp)
				{
					TSharedPtr<FRepLayout> SubObjLayout = GetObjectClassRepLayout(ActorComp->GetClass());
					if (SubObjLayout.IsValid())
					{
						TSharedPtr<FRepState> RepState = RollbackActor.SubObjRepState.FindRef(ActorComp->GetFullName());

						if (SubObjLayout.IsValid() && RepState.IsValid() && bSanityCheckReferences)
						{
							FRepObjectDataBuffer ActorCompData(ActorComp);
							FConstRepShadowDataBuffer ShadowData(RepState->StaticBuffer.GetData());

							SubObjLayout->DiffStableProperties(&RepState->RepNotifies, nullptr, ActorCompData, ShadowData);

							if (RepState->RepNotifies.Num() > 0)
							{
								SubObjLayout->CallRepNotifies(RepState.Get(), ActorComp);

								ActorComp->PostRepNotifies();
							}
						}
					}
				}
			}

			check( Actor->GetRemoteRole() == ROLE_Authority );

			SpawnedActors.Add(Actor);
		}

		It.RemoveCurrent();
	}
}

void UDemoNetDriver::PrepFastForwardLevels()
{
	if (!HasLevelStreamingFixes() || NewStreamingLevelsThisFrame.Num() == 0)
	{
		return;
	}

	check(!bIsFastForwarding);
	check(!bIsLoadingCheckpoint);

	// Do a quick pass to double check everything is still valid, and that we have data for the levels.
	UWorld* LocalWorld = GetWorld();
	for (TWeakObjectPtr<UObject> &WeakLevel : NewStreamingLevelsThisFrame)
	{
		// For playback, we should only ever see ULevels in this list.
		if (ULevel* Level = CastChecked<ULevel>(WeakLevel.Get()))
		{
			if (!ensure(!LevelsPendingFastForward.Contains(Level)))
			{
				UE_LOG(LogDemo, Warning, TEXT("FastForwardLevels - NewStreamingLevel found in Pending list! %s"), *GetFullName(Level));
				continue;
			}

			TSet<TWeakObjectPtr<AActor>> LevelActors;
			for (AActor* Actor : Level->Actors)
			{
				if (Actor == nullptr || !Actor->IsNetStartupActor())
				{
					continue;
				}
				else if (DeletedNetStartupActors.Contains(Actor->GetFullName()))
				{
					// Put this actor on the rollback list so we can undelete it during future scrubbing,
					// then delete it.
					QueueNetStartupActorForRollbackViaDeletion(Actor);
					LocalWorld->DestroyActor(Actor, true);
				}
				else
				{
					if (RollbackNetStartupActors.Contains(Actor->GetFullName()))
					{
						LocalWorld->DestroyActor(Actor, true);
					}
					else
					{
					LevelActors.Add(Actor);
				}
			}
			}

			TArray<AActor*> SpawnedActors;
			RespawnNecessaryNetStartupActors(SpawnedActors, Level);

			for(AActor* Actor : SpawnedActors)
			{
				LevelActors.Add(Actor);
			}

			if (LevelActors.Num() > 0)
			{
				LevelsPendingFastForward.Emplace(Level, MoveTemp(LevelActors));
			}
		}
	}

	NewStreamingLevelsThisFrame.Empty();

	if (LevelsPendingFastForward.Num() == 0 ||
		LastProcessedPacketTime == 0.f ||
		// If there's already a FastForwardLevelsTask or GotoTimeTask, then we don't need
		// to add another (as the levels will get picked up by either of those).
		IsNamedTaskInQueue(ReplayTaskNames::GotoTimeInSecondsTask) ||
		IsNamedTaskInQueue(ReplayTaskNames::FastForwardLevelsTask))
	{
		return;
	}

	AddReplayTask(new FFastForwardLevelsTask(this));
}

bool UDemoNetDriver::FastForwardLevels(const FGotoResult& GotoResult)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FastForwardLevels time"), STAT_FastForwardLevelTime, STATGROUP_Net);

	FArchive* CheckpointArchive = ReplayStreamer->GetCheckpointArchive();

	PauseChannels(false);

	// We can skip processing the checkpoint here, because Goto will load one up for us later.
	// We only want to check the very next task, though. Otherwise, we could end processing other
	// tasks in an invalid state.
	if (GetNextQueuedTaskName() == ReplayTaskNames::GotoTimeInSecondsTask)
	{
		// This is a bit hacky, but we don't want to do *any* processing this frame.
		// Therefore, we'll reset the ActiveReplayTask and return false.
		// This will cause us to early out, and then handle the Goto task next frame.
		ActiveReplayTask.Reset();
		return false;
	}

	// Generate the list of level names, and an uber list of the startup actors.
	// We manually track whenever a level is added and removed from the world, so these should always be valid.
	TSet<int32> LevelIndices;
	TSet<TWeakObjectPtr<AActor>> StartupActors;
	TSet<ULevel*> LocalLevels;

	// Reserve some default space, and just assume a minimum of at least 4 actors per level (super low estimate).
	LevelIndices.Reserve(LevelsPendingFastForward.Num());
	StartupActors.Reserve(LevelsPendingFastForward.Num() * 4);

	for (auto It = LevelsPendingFastForward.CreateIterator(); It; ++It)
	{
		// Track the appropriate level, and mark it as ready.
		FLevelStatus& LevelStatus = GetLevelStatus(GetLevelPackageName(*It.Key()));
		LevelIndices.Add(LevelStatus.LevelIndex);
		LevelStatus.bIsReady = true;

		// Quick sanity check to make sure the actors are still valid
		// NOTE: The only way any of these should not be valid is if the level was unloaded,
		//			or something in the demo caused the actor to be destroyed *before*
		//			the level was ready. Either case seems bad if we've made it this far.
		TSet<TWeakObjectPtr<AActor>>& LevelStartupActors = It.Value();
		for (auto ActorIt = LevelStartupActors.CreateIterator(); ActorIt; ++ActorIt)
		{
			if (!ensure((*ActorIt).IsValid()))
			{
				ActorIt.RemoveCurrent();
			}
		}

		LocalLevels.Add(It.Key());
		StartupActors.Append(MoveTemp(LevelStartupActors));
	}

	LevelsPendingFastForward.Reset();

	struct FLocalReadPacketsHelper
	{
		FLocalReadPacketsHelper(UDemoNetDriver& InDriver, const float InLastPacketTime):
			Driver(InDriver),
			LastPacketTime(InLastPacketTime)
		{
		}

		// @return True if another read can be attempted, false otherwise.
		bool ReadPackets(FArchive& Ar)
		{
			// Grab the packets, and make sure the stream is OK.
			PreFramePos = Ar.Tell();
			NumPackets = Packets.Num();
			if (!Driver.ReadDemoFrameIntoPlaybackPackets(Ar, Packets, true, &LastReadTime))
			{
				bErrorOccurred = true;
				return false;
			}

			// In case the archive had more data than we needed, we'll try to leave it where we left off
			// before the level fast forward.
			else if (LastReadTime > LastPacketTime)
			{
				Ar.Seek(PreFramePos);
				if (ensure(NumPackets != 0))
	{
					Packets.RemoveAt(NumPackets, Packets.Num() - NumPackets);
	}
				return false;
			}

			return true;
		}

		bool IsError() const
	{
			return bErrorOccurred;
	}

		TArray<FPlaybackPacket> Packets;

	private:

		UDemoNetDriver& Driver;
		const float LastPacketTime;

	// We only want to process packets that are before anything we've currently processed.
	// Further, we want to make sure that we leave the archive in a good state for later use.
	int32 NumPackets = 0;
	float LastReadTime = 0;
	FArchivePos PreFramePos = 0;

		bool bErrorOccurred = false;

	} ReadPacketsHelper(*this, LastProcessedPacketTime);

	{
		auto IgnoreReceivedExportGUIDs = ((UPackageMapClient*)ServerConnection->PackageMap)->ScopedIgnoreReceivedExportGUIDs();

		// First, read in the checkpoint data (if any is available);
		if (CheckpointArchive->TotalSize() != 0)
		{
			CheckpointArchive->SetEngineNetVer(PlaybackDemoHeader.EngineNetworkProtocolVersion);
			CheckpointArchive->SetGameNetVer(PlaybackDemoHeader.GameNetworkProtocolVersion);

			TGuardValue<bool> LoadingCheckpointGuard(bIsLoadingCheckpoint, true);

			FArchivePos PacketOffset = 0;
			*CheckpointArchive << PacketOffset;
			CheckpointArchive->Seek(PacketOffset + CheckpointArchive->Tell());

			if (!ReadPacketsHelper.ReadPackets(*CheckpointArchive) && ReadPacketsHelper.IsError())
			{
				UE_LOG(LogDemo, Warning, TEXT("UDemoNetDriver::FastForwardLevels: Failed to read packets from Checkpoint."));
				NotifyDemoPlaybackFailure(EDemoPlayFailure::Serialization);
				return false;
			}
		}

		// Next, read in streaming data (if any is available)
		FArchive* StreamingAr = ReplayStreamer->GetStreamingArchive();

		StreamingAr->SetEngineNetVer(PlaybackDemoHeader.EngineNetworkProtocolVersion);
		StreamingAr->SetGameNetVer(PlaybackDemoHeader.GameNetworkProtocolVersion);
		
		while (!StreamingAr->AtEnd() && ReplayStreamer->IsDataAvailable() && ReadPacketsHelper.ReadPackets(*StreamingAr));

		if (ReadPacketsHelper.IsError())
		{
			UE_LOG(LogDemo, Warning, TEXT("UDemoNetDriver::FastForwardLevels: Failed to read packets from Stream."));
			NotifyDemoPlaybackFailure(EDemoPlayFailure::Serialization);
			return false;
		}
	}

	// If we've gotten this far, it means we should have something to process.
	check(ReadPacketsHelper.Packets.Num() > 0);

	// It's possible that the level we're streaming in may spawn Dynamic Actors.
	// In that case, we want to make sure we track them so we can process them below.
	// We only care about the actors if they're outered to the Level.
	struct FDynamicActorTracker
	{
		FDynamicActorTracker(UWorld* InTrackWorld, TSet<ULevel*>& InCareAboutLevels, TSet<TWeakObjectPtr<AActor>>& InActorSet) :
			TrackWorld(InTrackWorld),
			CareAboutLevels(InCareAboutLevels),
			ActorSet(InActorSet)
		{
			FOnActorSpawned::FDelegate TrackActorDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FDynamicActorTracker::TrackActor);
			TrackActorHandle = TrackWorld->AddOnActorSpawnedHandler(TrackActorDelegate);
		}

		~FDynamicActorTracker()
		{
			TrackWorld->RemoveOnActorSpawnedHandler(TrackActorHandle);
		}

	private:

		void TrackActor(AActor* Actor)
		{
			if (Actor && CareAboutLevels.Contains(Actor->GetLevel()))
			{
				UE_LOG(LogDemo, Verbose, TEXT("FastForwardLevels - Sublevel spawned dynamic actor."));
				ActorSet.Add(Actor);
			}
		}

		UWorld* TrackWorld;
		TSet<ULevel*> CareAboutLevels;
		TSet<TWeakObjectPtr<AActor>>& ActorSet;
		FDelegateHandle TrackActorHandle;

	} ActorTracker(World, LocalLevels, StartupActors);

	{
		TGuardValue<bool> FastForward(bIsFastForwarding, true);
		struct FScopedIgnoreChannels
		{
			FScopedIgnoreChannels(UNetConnection* InConnection):
				Connection(InConnection)
			{
				if (Connection.IsValid())
				{
					Connection->SetIgnoreAlreadyOpenedChannels(true);
				}	
			}

			~FScopedIgnoreChannels()
			{
				if (Connection.IsValid())
				{
					Connection->SetIgnoreAlreadyOpenedChannels(false);
				}	
			}

		private:
			TWeakObjectPtr<UNetConnection> Connection;
		} ScopedIgnoreChannels(ServerConnection);

		// Process all the packets we need.
		for (FPlaybackPacket& Packet : ReadPacketsHelper.Packets)
		{
			// Skip packets that aren't associated with levels.
			if (Packet.SeenLevelIndex == 0)
			{
				continue;
			}

			// Don't attempt to go beyond the current demo time.
			// These packets should have been already been filtered out while reading.
			if (!ensureMsgf(Packet.TimeSeconds <= DemoCurrentTime, TEXT("UDemoNetDriver::FastForwardLevels: Read packet beyond DemoCurrentTime DemoTime = %f PacketTime = %f"), DemoCurrentTime, Packet.TimeSeconds))
			{
				break;
			}

			if (SeenLevelStatuses.IsValidIndex(Packet.SeenLevelIndex - 1))
			{
				const FLevelStatus& LevelStatus = GetLevelStatus(Packet.SeenLevelIndex);
				const bool bCareAboutLevel = LevelIndices.Contains(LevelStatus.LevelIndex);

				if (bCareAboutLevel)
				{
					// If we tried to process the packet, but failed, then the replay will be in a broken state.
					// ProcessPacket will have called StopDemo.
					if (!ProcessPacket(Packet.Data.GetData(), Packet.Data.Num()))
					{
						UE_LOG(LogDemo, Warning, TEXT("FastForwardLevel failed to process packet"));
						return false;
					}
				}
			}
			else
			{
				UE_LOG(LogDemo, Warning, TEXT("FastForwardLevel could not process packet with invalid seen level index"));
			}
		}
	}

	if (ensure(ServerConnection != nullptr))
	{
		// Make a pass at OnReps for startup actors, since they were skipped during checkpoint loading.
		// At this point the shadow state of these actors should be the actual state from before the checkpoint,
		// and the current state is the CDO state evolved by any changes that occurred during checkpoint loading and fast-forwarding.

		TArray<UActorChannel*> ChannelsToUpdate;
		ChannelsToUpdate.Reserve(StartupActors.Num());

		for (UChannel* Channel : ServerConnection->OpenChannels)
		{
			// Skip non-actor channels.
			if (Channel == nullptr || Channel->ChName != NAME_Actor)
			{
				continue;
			}

			// Since we know this is an actor channel, should be safe to do a static_cast.
			UActorChannel* const ActorChannel = static_cast<UActorChannel*>(Channel);
			AActor* Actor = ActorChannel->GetActor();

			// We only need to consider startup actors, or dynamic that were spawned and outered
			// to one of our sublevels.
			if (Actor == nullptr || !StartupActors.Contains(Actor))
			{
				continue;
			}

			ChannelsToUpdate.Add(ActorChannel);
			if (const FObjectReplicator* const ActorReplicator = ActorChannel->ActorReplicator)
			{
				FRepShadowDataBuffer ShadowData(ActorReplicator->RepState->StaticBuffer.GetData());
				FConstRepObjectDataBuffer ActorData(Actor);

				ActorReplicator->RepLayout->DiffProperties(&(ActorReplicator->RepState->RepNotifies), ShadowData, ActorData, EDiffPropertiesFlags::Sync);
			}
		}

		for (UActorChannel* Channel : ChannelsToUpdate)
		{
			for (auto& ReplicatorPair : Channel->ReplicationMap)
			{
				ReplicatorPair.Value->CallRepNotifies(true);
			}
		}

		for (auto& DormantPair : ServerConnection->DormantReplicatorMap)
		{
			DormantPair.Value->CallRepNotifies( true );
		}
	}

	return true;
}

bool UDemoNetDriver::LoadCheckpoint(const FGotoResult& GotoResult)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("LoadCheckpoint time"), STAT_ReplayCheckpointLoadTime, STATGROUP_Net);

	FArchive* GotoCheckpointArchive = ReplayStreamer->GetCheckpointArchive();

	check( GotoCheckpointArchive != nullptr );
	check( !bIsFastForwardingForCheckpoint );
	check( !bIsFastForwarding );

	GotoCheckpointArchive->SetEngineNetVer(PlaybackDemoHeader.EngineNetworkProtocolVersion);
	GotoCheckpointArchive->SetGameNetVer(PlaybackDemoHeader.GameNetworkProtocolVersion);

	int32 LevelForCheckpoint = 0;

	if (HasLevelStreamingFixes())
	{
		// Make sure to read the packet offset, even though we won't use it here.
		if (GotoCheckpointArchive->TotalSize() > 0)
		{
			FArchivePos PacketOffset = 0;
			*GotoCheckpointArchive << PacketOffset;
		}

		ResetLevelStatuses();
	}

	LastProcessedPacketTime = 0.f;
	LatestReadFrameTime = 0.f;

	if (PlaybackDemoHeader.Version >= HISTORY_MULTIPLE_LEVELS)
	{
		if (GotoCheckpointArchive->TotalSize() > 0)
		{
			*GotoCheckpointArchive << LevelForCheckpoint;
		}
	}

	if (LevelForCheckpoint != CurrentLevelIndex)
	{
		GetWorld()->GetGameInstance()->OnSeamlessTravelDuringReplay();

		for (FActorIterator It(GetWorld()); It; ++It)
		{
			GetWorld()->DestroyActor(*It, true);
		}

		// Clean package map to prepare to restore it to the checkpoint state
		GuidCache->ResetCacheForDemo();

		SpectatorController = nullptr;

		ServerConnection->Close();
		ServerConnection->CleanUp();

		// Recreate the server connection - this is done so that when we execute the code the below again when we read in the
		// checkpoint again after the server travel is finished, we'll have a clean server connection to work with.
		ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), UDemoNetConnection::StaticClass());

		FURL ConnectURL;
		ConnectURL.Map = DemoURL.Map;
		ServerConnection->InitConnection(this, USOCK_Pending, ConnectURL, 1000000);

		GEngine->ForceGarbageCollection(true);

		ProcessSeamlessTravel(LevelForCheckpoint);
		CurrentLevelIndex = LevelForCheckpoint;

		if (GotoCheckpointArchive->TotalSize() != 0 && GotoCheckpointArchive->TotalSize() != INDEX_NONE)
		{
			GotoCheckpointArchive->Seek(0);
		}

		return false;
	}

	// Save off the current spectator position
	// Check for nullptr, which can be the case if we haven't played any of the demo yet but want to fast forward (joining live game for example)
	if ( SpectatorController != nullptr )
	{
		// Save off the SpectatorController's GUID so that we know not to queue his bunches
		AddNonQueuedActorForScrubbing( SpectatorController );
	}

	// Remember the spectator controller's view target so we can restore it
	FNetworkGUID ViewTargetGUID;

	if ( SpectatorController && SpectatorController->GetViewTarget() )
	{
		ViewTargetGUID = GuidCache->NetGUIDLookup.FindRef( SpectatorController->GetViewTarget() );

		if ( ViewTargetGUID.IsValid() )
		{
			AddNonQueuedActorForScrubbing( SpectatorController->GetViewTarget() );
		}
	}

	PauseChannels( false );

	FNetworkReplayDelegates::OnPreScrub.Broadcast(GetWorld());

	bIsLoadingCheckpoint = true;

	struct FPreservedNetworkGUIDEntry
	{
		FPreservedNetworkGUIDEntry( const FNetworkGUID InNetGUID, const AActor* const InActor )
			: NetGUID( InNetGUID ), Actor( InActor ) {}

		FNetworkGUID NetGUID;
		const AActor* Actor;
	};

	// Store GUIDs for the spectator controller and any of its owned actors, so we can find them when we process the checkpoint.
	// For the spectator controller, this allows the state and position to persist.
	TArray<FPreservedNetworkGUIDEntry> NetGUIDsToPreserve;

	if (!ensureMsgf(TrackedRewindActorsByGUID.Num() == 0, TEXT("LoadCheckpoint: TrackedRewindAcotrsByGUID list not empty!")))
	{
		TrackedRewindActorsByGUID.Empty();
	}

#if 1
	TSet<const AActor*> KeepAliveActors;

	// Destroy all non startup actors. They will get restored with the checkpoint
	for ( FActorIterator It( GetWorld() ); It; ++It )
	{
		// If there are any existing actors that are bAlwaysRelevant, don't queue their bunches.
		// Actors that do queue their bunches might not appear immediately after the checkpoint is loaded,
		// and missing bAlwaysRelevant actors are more likely to cause noticeable artifacts.
		// NOTE - We are adding the actor guid here, under the assumption that the actor will reclaim the same guid when we load the checkpoint
		// This is normally the case, but could break if actors get destroyed and re-created with different guids during recording
		if ( It->bAlwaysRelevant )
		{
			AddNonQueuedActorForScrubbing(*It);
		}
		
		const bool bShouldPreserveForPlayerController = (SpectatorController != nullptr && (*It == SpectatorController || *It == SpectatorController->GetSpectatorPawn() || It->GetOwner() == SpectatorController));
		const bool bShouldPreserveForRewindability = (It->bReplayRewindable && !It->IsNetStartupActor());										

		if (bShouldPreserveForPlayerController || bShouldPreserveForRewindability)
		{
			// If an non-startup actor that we don't destroy has an entry in the GuidCache, preserve that entry so
			// that the object will be re-used after loading the checkpoint. Otherwise, a new copy
			// of the object will be created each time a checkpoint is loaded, causing a leak.
			const FNetworkGUID FoundGUID = GuidCache->NetGUIDLookup.FindRef( *It );
				
			if (FoundGUID.IsValid())
			{
				NetGUIDsToPreserve.Emplace(FoundGUID, *It);
				
				if (bShouldPreserveForRewindability)
				{
					TrackedRewindActorsByGUID.Add(FoundGUID);
				}
			}

			KeepAliveActors.Add(*It);
			continue;
		}

		// Prevent NetStartupActors from being destroyed.
		// NetStartupActors that can't have properties directly re-applied should use QueueNetStartupActorForRollbackViaDeletion.
		if ( It->IsNetStartupActor() )
		{
			// Go ahead and rewind this now, since we won't be destroying it later.
			if (It->bReplayRewindable)
			{
				It->RewindForReplay();
			}
			KeepAliveActors.Add(*It);
			continue;
		}

		GetWorld()->DestroyActor( *It, true );
	}

	// Destroy all particle FX attached to the WorldSettings (the WorldSettings actor persists but the particle FX spawned at runtime shouldn't)
	GetWorld()->HandleTimelineScrubbed();

	// Remove references to our KeepAlive actors so that cleaning up the channels won't destroy them.
	for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
	{
		UChannel* OpenChannel = ServerConnection->OpenChannels[i];
		if ( OpenChannel != nullptr )
		{
			UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );
			if ( ActorChannel != nullptr && KeepAliveActors.Contains(ActorChannel->Actor))
			{
				ActorChannel->Actor = nullptr;
			}
		}
	}

	if ( ServerConnection->OwningActor == SpectatorController )
	{
		ServerConnection->OwningActor = nullptr;
	}
#else
	for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
	{
		UChannel* OpenChannel = ServerConnection->OpenChannels[i];
		if ( OpenChannel != nullptr )
		{
			UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );
			if ( ActorChannel != nullptr && ActorChannel->GetActor() != nullptr && !ActorChannel->GetActor()->IsNetStartupActor() )
			{
				GetWorld()->DestroyActor( ActorChannel->GetActor(), true );
			}
		}
	}
#endif

	ExternalDataToObjectMap.Empty();
	PlaybackPackets.Empty();

	ServerConnection->Close();
	ServerConnection->CleanUp();

	// Destroy startup actors that need to rollback via being destroyed and re-created
	for ( FActorIterator It( GetWorld() ); It; ++It )
	{
		if ( RollbackNetStartupActors.Contains( It->GetFullName() ) )
		{
			GetWorld()->DestroyActor( *It, true );
		}
	}

	// Optionally collect garbage after the old actors and connection are cleaned up - there could be a lot of pending-kill objects at this point.
	if (CVarDemoLoadCheckpointGarbageCollect.GetValueOnGameThread() != 0)
	{
		GEngine->ForceGarbageCollection(true);
	}

	FURL ConnectURL;
	ConnectURL.Map = DemoURL.Map;

	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), UDemoNetConnection::StaticClass());
	ServerConnection->InitConnection( this, USOCK_Pending, ConnectURL, 1000000 );

	// Set network version on connection
	ServerConnection->EngineNetworkProtocolVersion = PlaybackDemoHeader.EngineNetworkProtocolVersion;
	ServerConnection->GameNetworkProtocolVersion = PlaybackDemoHeader.GameNetworkProtocolVersion;

	// Create fake control channel
	CreateInitialClientChannels();

	// Catch a rare case where the spectator controller is null, but a valid GUID is
	// found on the GuidCache. The weak pointers in the NetGUIDLookup map are probably
	// going null, and we want catch these cases and investigate further.
	if (!ensure( GuidCache->NetGUIDLookup.FindRef( SpectatorController ).IsValid() == ( SpectatorController != nullptr ) ))
	{
		UE_LOG(LogDemo, Log, TEXT("LoadCheckpoint: SpectatorController is null and a valid GUID for null was found in the GuidCache. SpectatorController = %s"),
			*GetFullNameSafe(SpectatorController));
	}
	
	// Clean package map to prepare to restore it to the checkpoint state
	FlushAsyncLoading();
	GuidCache->ResetCacheForDemo();

	// Restore preserved packagemap entries
	for ( const FPreservedNetworkGUIDEntry& PreservedEntry : NetGUIDsToPreserve )
	{
		check( PreservedEntry.NetGUID.IsValid() );
		
		FNetGuidCacheObject& CacheObject = GuidCache->ObjectLookup.FindOrAdd( PreservedEntry.NetGUID );

		CacheObject.Object = MakeWeakObjectPtr(const_cast<AActor*>(PreservedEntry.Actor));
		check( CacheObject.Object != nullptr );
		CacheObject.bNoLoad = true;
		GuidCache->NetGUIDLookup.Add( CacheObject.Object, PreservedEntry.NetGUID );
	}

	if ( GotoCheckpointArchive->TotalSize() == 0 || GotoCheckpointArchive->TotalSize() == INDEX_NONE )
	{
		// Make sure this is empty so that RespawnNecessaryNetStartupActors will respawn them
		DeletedNetStartupActors.Empty();

		// Re-create all startup actors that were destroyed but should exist beyond this point
		TArray<AActor*> SpawnedActors;
		RespawnNecessaryNetStartupActors(SpawnedActors);

		// This is the very first checkpoint, we'll read the stream from the very beginning in this case
		DemoCurrentTime			= 0;
		bDemoPlaybackDone		= false;
		bIsLoadingCheckpoint	= false;

		if ( GotoResult.ExtraTimeMS != -1 )
		{
			SkipTimeInternal( ( float )GotoResult.ExtraTimeMS / 1000.0f, true, true );
		}
		else
		{
			// Make sure that we delete any Rewind actors that aren't valid anymore.
			// If there's more data to stream in, we will handle this in FinalizeFastForward.
			CleanupOutstandingRewindActors();
		}

		return true;
	}

	// Load net startup actors that need to be destroyed
	if ( PlaybackDemoHeader.Version >= HISTORY_DELETED_STARTUP_ACTORS )
	{
		*GotoCheckpointArchive << DeletedNetStartupActors;
	}

	// Destroy startup actors that shouldn't exist past this checkpoint
	for ( FActorIterator It( GetWorld() ); It; ++It )
	{
		const FString FullName = It->GetFullName();
		if ( DeletedNetStartupActors.Contains( FullName ) )
		{
			if (It->bReplayRewindable)
			{
				// Log and skip. We can't queue Rewindable actors and we can't destroy them.
				// This actor may still get destroyed during cleanup.
				UE_LOG(LogDemo, Warning, TEXT("Replay Rewindable Actor found in the DeletedNetStartupActors. Replay may show artifacts (%s)"), *FullName);
				continue;
			}

			// Put this actor on the rollback list so we can undelete it during future scrubbing
			QueueNetStartupActorForRollbackViaDeletion( *It );

			UE_LOG(LogDemo, Verbose, TEXT("LoadCheckpoint: deleting startup actor %s"), *FullName);

			// Delete the actor
			GetWorld()->DestroyActor( *It, true );
		}
	}

	// Re-create all startup actors that were destroyed but should exist beyond this point
	TArray<AActor*> SpawnedActors;
	RespawnNecessaryNetStartupActors(SpawnedActors);

	int32 NumValues = 0;
	*GotoCheckpointArchive << NumValues;

	for ( int32 i = 0; i < NumValues; i++ )
	{
		FNetworkGUID Guid;
		
		*GotoCheckpointArchive << Guid;
		
		FNetGuidCacheObject CacheObject;

		FString PathName;

		*GotoCheckpointArchive << CacheObject.OuterGUID;
		*GotoCheckpointArchive << PathName;
		*GotoCheckpointArchive << CacheObject.NetworkChecksum;

		// Remap the pathname to handle client-recorded replays
		GEngine->NetworkRemapPath(this, PathName, true);

		CacheObject.PathName = FName( *PathName );

		uint8 Flags = 0;
		*GotoCheckpointArchive << Flags;

		CacheObject.bNoLoad = ( Flags & ( 1 << 0 ) ) ? true : false;
		CacheObject.bIgnoreWhenMissing = ( Flags & ( 1 << 1 ) ) ? true : false;		

		GuidCache->ObjectLookup.Add( Guid, CacheObject );
	}

	// Read in the compatible rep layouts in this checkpoint
	( ( UPackageMapClient* )ServerConnection->PackageMap )->SerializeNetFieldExportGroupMap( *GotoCheckpointArchive );

	ReadDemoFrameIntoPlaybackPackets( *GotoCheckpointArchive );

	if ( PlaybackPackets.Num() > 0 )
	{
		DemoCurrentTime = PlaybackPackets.Last().TimeSeconds;
	}
	else
	{
		DemoCurrentTime = 0.0f;
	}

	if (GotoResult.ExtraTimeMS != -1 )
	{
		// If we need to skip more time for fine scrubbing, set that up now
		SkipTimeInternal( ( float )GotoResult.ExtraTimeMS / 1000.0f, true, true );
	}
	else
	{
		// Make sure that we delete any Rewind actors that aren't valid anymore.
		// If there's more data to stream in, we will handle this in FinalizeFastForward.
		CleanupOutstandingRewindActors();
	}

	ProcessAllPlaybackPackets();

	bDemoPlaybackDone = false;
	bIsLoadingCheckpoint = false;

	// Save the replicated server time here
	if (World != nullptr)
	{
		const AGameStateBase* const GameState = World->GetGameState();
		if (GameState != nullptr)
		{
			SavedReplicatedWorldTimeSeconds = GameState->ReplicatedWorldTimeSeconds;
		}
	}

	if ( SpectatorController && ViewTargetGUID.IsValid() )
	{
		AActor* ViewTarget = Cast< AActor >( GuidCache->GetObjectFromNetGUID( ViewTargetGUID, false ) );

		if ( ViewTarget )
		{
			SpectatorController->SetViewTarget( ViewTarget );
		}
	}

	return true;
}

bool UDemoNetDriver::IsSavingCheckpoint() const
{
	if (ClientConnections.Num() > 0)
	{
		UNetConnection* const NetConnection = ClientConnections[0];
		if (NetConnection)
		{
			return NetConnection->bResendAllDataSinceOpen;
		}
	}

	return false;
}

bool UDemoNetDriver::ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const
{
	if ( CVarDemoQueueCheckpointChannels.GetValueOnGameThread() == 0)
	{
		return false;
	}

	// While loading a checkpoint, queue most bunches so that we don't process them all on one frame.
	if ( bIsFastForwardingForCheckpoint )
	{
		return !NonQueuedGUIDsForScrubbing.Contains(InGUID);
	}

	return false;
}

bool UDemoNetDriver::ShouldIgnoreRPCs() const
{
	return (CVarDemoFastForwardIgnoreRPCs.GetValueOnAnyThread() && (bIsLoadingCheckpoint || bIsFastForwarding));
}

FNetworkGUID UDemoNetDriver::GetGUIDForActor(const AActor* InActor) const
{
	UNetConnection* Connection = ServerConnection;
	
	if ( ClientConnections.Num() > 0)
	{
		Connection = ClientConnections[0];
	}

	if ( !Connection )
	{
		return FNetworkGUID();
	}

	FNetworkGUID Guid = Connection->PackageMap->GetNetGUIDFromObject(InActor);
	return Guid;
}

AActor* UDemoNetDriver::GetActorForGUID(FNetworkGUID InGUID) const
{
	UNetConnection* Connection = ServerConnection;
	
	if ( ClientConnections.Num() > 0)
	{
		Connection = ClientConnections[0];
	}

	if ( !Connection )
	{
		return nullptr;
	}

	UObject* FoundObject = Connection->PackageMap->GetObjectFromNetGUID(InGUID, true);
	return Cast<AActor>(FoundObject);

}

bool UDemoNetDriver::ShouldReceiveRepNotifiesForObject(UObject* Object) const
{
	// Return false for startup actors during checkpoint loading, since they are
	// not destroyed and re-created like dynamic actors. Startup actors will
	// have their properties diffed and RepNotifies called after the checkpoint is loaded.

	if (!bIsLoadingCheckpoint && !bIsFastForwardingForCheckpoint)
	{
		return true;
	}

	const AActor* const Actor = Cast<AActor>(Object);
	const bool bIsStartupActor = Actor != nullptr && Actor->IsNetStartupActor();

	return !bIsStartupActor;
}

void UDemoNetDriver::AddNonQueuedActorForScrubbing(AActor const* Actor)
{
	UActorChannel const* const* const FoundChannel = ServerConnection->FindActorChannel(MakeWeakObjectPtr(const_cast<AActor*>(Actor)));
	if (FoundChannel != nullptr && *FoundChannel != nullptr)
	{
		FNetworkGUID const ActorGUID = (*FoundChannel)->ActorNetGUID;
		NonQueuedGUIDsForScrubbing.Add(ActorGUID);
	}
}

void UDemoNetDriver::AddNonQueuedGUIDForScrubbing(FNetworkGUID InGUID)
{
	if (InGUID.IsValid())
	{
		NonQueuedGUIDsForScrubbing.Add(InGUID);
	}
}

FDemoSavedPropertyState UDemoNetDriver::SavePropertyState() const
{
	FDemoSavedPropertyState State;

	if (IsRecording())
	{
		const UNetConnection* const RecordingConnection = ClientConnections[0];
		for (auto ChannelPair = RecordingConnection->ActorChannelConstIterator(); ChannelPair; ++ChannelPair)
		{
			const UActorChannel* const Channel = ChannelPair.Value();
			if (Channel)
			{
				for (const auto& ReplicatorPair : Channel->ReplicationMap)
				{
					TWeakObjectPtr<UObject> WeakObjectPtr = ReplicatorPair.Value->GetWeakObjectPtr();
					if (const UObject* const RepObject = WeakObjectPtr.Get())
					{
						FDemoSavedRepObjectState& SavedObject = State.Emplace_GetRef();
						SavedObject.Object = WeakObjectPtr;
						SavedObject.RepLayout = ReplicatorPair.Value->RepLayout;

						SavedObject.RepLayout->InitShadowData(SavedObject.PropertyData, RepObject->GetClass(), reinterpret_cast<const uint8* const>(RepObject));

						// Store the properties in the new RepState
						FRepShadowDataBuffer ShadowData(SavedObject.PropertyData.GetData());
						FConstRepObjectDataBuffer RepObjectData(RepObject);

						SavedObject.RepLayout->DiffProperties(nullptr, ShadowData, RepObjectData, EDiffPropertiesFlags::Sync | EDiffPropertiesFlags::IncludeConditionalProperties);
					}
				}
			}
		}
	}

	return State;
}

bool UDemoNetDriver::ComparePropertyState(const FDemoSavedPropertyState& State) const
{
	bool bWasDifferent = false;

	if (IsRecording())
	{
		for (const FDemoSavedRepObjectState& ObjectState : State)
		{
			const UObject* const RepObject = ObjectState.Object.Get();
			if (RepObject)
			{
				FRepObjectDataBuffer RepObjectData(const_cast<UObject* const>(RepObject));
				FConstRepShadowDataBuffer ShadowData(ObjectState.PropertyData.GetData());

				if (ObjectState.RepLayout->DiffProperties(nullptr, RepObjectData, ShadowData, EDiffPropertiesFlags::IncludeConditionalProperties))
				{
					bWasDifferent = true;
				}
			}
			else
			{
				UE_LOG(LogDemo, Warning, TEXT("A replicated object was destroyed or marked pending kill since its state was saved!"));
				bWasDifferent = true;
			}
		}
	}

	return bWasDifferent;
}

/*-----------------------------------------------------------------------------
	UDemoNetConnection.
-----------------------------------------------------------------------------*/

UDemoNetConnection::UDemoNetConnection( const FObjectInitializer& ObjectInitializer ) : Super( ObjectInitializer )
{
	MaxPacket = MAX_DEMO_READ_WRITE_BUFFER;
	InternalAck = true;
}

void UDemoNetConnection::InitConnection( UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed, int32 InMaxPacket)
{
	// default implementation
	Super::InitConnection( InDriver, InState, InURL, InConnectionSpeed );

	MaxPacket = (InMaxPacket == 0 || InMaxPacket > MAX_DEMO_READ_WRITE_BUFFER) ? MAX_DEMO_READ_WRITE_BUFFER : InMaxPacket;
	InternalAck = true;

	InitSendBuffer();

	// the driver must be a DemoRecording driver (GetDriver makes assumptions to avoid Cast'ing each time)
	check( InDriver->IsA( UDemoNetDriver::StaticClass() ) );
}

FString UDemoNetConnection::LowLevelGetRemoteAddress( bool bAppendPort )
{
	return TEXT( "UDemoNetConnection" );
}

void UDemoNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	uint32 CountBytes = FMath::DivideAndRoundUp(CountBits, 8);

	if ( CountBytes == 0 )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::LowLevelSend: Ignoring empty packet." ) );
		return;
	}

	if ( CountBytes > MAX_DEMO_READ_WRITE_BUFFER )
	{
		UE_LOG( LogDemo, Fatal, TEXT( "UDemoNetConnection::LowLevelSend: CountBytes > MAX_DEMO_READ_WRITE_BUFFER." ) );
	}

	TrackSendForProfiler( Data, CountBytes );

	if ( bResendAllDataSinceOpen )
	{
		// This path is only active for a checkpoint saving out, we need to queue in separate list
		new( QueuedCheckpointPackets )FQueuedDemoPacket( ( uint8* )Data, CountBits, Traits );
		return;
	}

	new(QueuedDemoPackets)FQueuedDemoPacket((uint8*)Data, CountBits, Traits);
}

void UDemoNetConnection::TrackSendForProfiler(const void* Data, int32 NumBytes)
{
	NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));

	// Track "socket send" even though we're not technically sending to a socket, to get more accurate information in the profiler.
	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendToCore(TEXT("Unreal"), Data, NumBytes, NumPacketIdBits, NumBunchBits, NumAckBits, NumPaddingBits, this));
}

FString UDemoNetConnection::LowLevelDescribe()
{
	return TEXT( "Demo recording/playback driver connection" );
}

int32 UDemoNetConnection::IsNetReady( bool Saturate )
{
	return 1;
}

void UDemoNetConnection::FlushNet( bool bIgnoreSimulation )
{
	// in playback, there is no data to send except
	// channel closing if an error occurs.
	if ( GetDriver()->ServerConnection != nullptr )
	{
		InitSendBuffer();
	}
	else
	{
		Super::FlushNet( bIgnoreSimulation );
	}
}

void UDemoNetConnection::HandleClientPlayer( APlayerController* PC, UNetConnection* NetConnection )
{
	// If the spectator is the same, assume this is for scrubbing, and we are keeping the old one
	// (so don't set the position, since we want to persist all that)
	if ( GetDriver()->SpectatorController == PC )
	{
		PC->Role			= ROLE_AutonomousProxy;
		PC->NetConnection	= NetConnection;
		LastReceiveTime		= Driver->Time;
		LastReceiveRealtime = FPlatformTime::Seconds();
		LastGoodPacketRealtime = FPlatformTime::Seconds();
		State				= USOCK_Open;
		PlayerController	= PC;
		OwningActor			= PC;
		return;
	}

	ULocalPlayer* LocalPlayer = nullptr;
	for (FLocalPlayerIterator It(GEngine, Driver->GetWorld()); It; ++It)
	{
		LocalPlayer = *It;
		break;
	}
	int32 SavedNetSpeed = LocalPlayer ? LocalPlayer->CurrentNetSpeed : 0;

	Super::HandleClientPlayer( PC, NetConnection );
	
	// Restore the netspeed if we're a local replay
	if (GetDriver()->bIsLocalReplay && LocalPlayer)
	{
		LocalPlayer->CurrentNetSpeed = SavedNetSpeed;
	}

	// Assume this is our special spectator controller
	GetDriver()->SpectatorController = PC;

	for ( FActorIterator It( Driver->World ); It; ++It)
	{
		if ( It->IsA( APlayerStart::StaticClass() ) )
		{
			PC->SetInitialLocationAndRotation( It->GetActorLocation(), It->GetActorRotation() );
			break;
		}
	}
}

TSharedPtr<FInternetAddr> UDemoNetConnection::GetInternetAddr()
{
	// Does not use MappedClientConnections
	return TSharedPtr<FInternetAddr>();
}

bool UDemoNetConnection::ClientHasInitializedLevelFor(const AActor* TestActor) const
{
	// We save all currently streamed levels into the demo stream so we can force the demo playback client
	// to stay in sync with the recording server
	// This may need to be tweaked or re-evaluated when we start recording demos on the client
	return ( GetDriver()->DemoFrameNum > 2 || Super::ClientHasInitializedLevelFor( TestActor ) );
}

TSharedPtr<FObjectReplicator> UDemoNetConnection::CreateReplicatorForNewActorChannel(UObject* Object)
{
	TSharedPtr<FObjectReplicator> NewReplicator = MakeShareable(new FObjectReplicator());

	// To handle rewinding net startup actors in replays properly, we need to
	// initialize the shadow state with the object's current state.
	// Afterwards, we will copy the CDO state to object's current state with repnotifies disabled.
	UDemoNetDriver* NetDriver = GetDriver();
	AActor* Actor = Cast<AActor>(Object);

	const bool bIsCheckpointStartupActor = NetDriver && NetDriver->IsLoadingCheckpoint() && Actor && Actor->IsNetStartupActor();
	const bool bUseDefaultState = !bIsCheckpointStartupActor;

	NewReplicator->InitWithObject(Object, this, bUseDefaultState);

	// Now that the shadow state is initialized, copy the CDO state into the actor state.
	if (bIsCheckpointStartupActor && NewReplicator->RepLayout.IsValid() && Object->GetClass())
	{
		FRepObjectDataBuffer ObjectData(Object);
		FConstRepObjectDataBuffer ShadowData(Object->GetClass()->GetDefaultObject());

		NewReplicator->RepLayout->DiffProperties(nullptr, ObjectData, ShadowData, EDiffPropertiesFlags::Sync);

		// Need to swap roles for the startup actor since in the CDO they aren't swapped, and the CDO just
		// overwrote the actor state.
		if (Actor && (Actor->Role == ROLE_Authority))
		{
			Actor->SwapRoles();
		}
	}

	QueueNetStartupActorForRewind(Actor);

	return NewReplicator;
}

void UDemoNetConnection::DestroyIgnoredActor(AActor* Actor)
	{
	QueueNetStartupActorForRewind(Actor);

	Super::DestroyIgnoredActor(Actor);
}

void UDemoNetConnection::QueueInitialDormantStartupActorForRewind(AActor* Actor)
{
	if (Actor && Actor->NetDormancy == DORM_Initial)
	{
		QueueNetStartupActorForRewind(Actor);
	}
}

void UDemoNetConnection::QueueNetStartupActorForRewind(AActor* Actor)
{
	UDemoNetDriver* NetDriver = GetDriver();

	// Handle rewinding initially dormant startup actors that were changed on the client
	const bool bIsStartupActor = NetDriver && Actor && Actor->IsNetStartupActor() && !Actor->bReplayRewindable;
	if (bIsStartupActor)
	{
		NetDriver->QueueNetStartupActorForRollbackViaDeletion(Actor);
	}
}

bool UDemoNetDriver::IsLevelInitializedForActor( const AActor* InActor, const UNetConnection* InConnection ) const
{
	return ( DemoFrameNum > 2 || Super::IsLevelInitializedForActor( InActor, InConnection ) );
}

bool UDemoNetDriver::IsPlayingClientReplay() const
{
	return IsPlaying() && ((PlaybackDemoHeader.HeaderFlags & EReplayHeaderFlags::ClientRecorded) != EReplayHeaderFlags::None);
}

void UDemoNetDriver::NotifyGotoTimeFinished(bool bWasSuccessful)
{
	// execute and clear the transient delegate
	OnGotoTimeDelegate_Transient.ExecuteIfBound(bWasSuccessful);
	OnGotoTimeDelegate_Transient.Unbind();

	// execute and keep the permanent delegate
	// call only when successful
	if (bWasSuccessful)
	{
		OnGotoTimeDelegate.Broadcast();
	}
}

void UDemoNetDriver::PendingNetGameLoadMapCompleted()
{
}

void UDemoNetDriver::OnSeamlessTravelStartDuringRecording(const FString& LevelName)
{
	PauseRecording(true);

	AddNewLevel(LevelName);

	FString Error;
	WriteNetworkDemoHeader(Error);

	ReplayStreamer->RefreshHeader();
}

void UDemoNetDriver::NotifyActorDestroyed( AActor* Actor, bool IsSeamlessTravel )
{
	check( Actor != nullptr );

	const bool bIsRecording = IsRecording();
	const bool bNetStartup = Actor->IsNetStartupActor();
	const bool bActorRewindable = Actor->bReplayRewindable;

	if (bActorRewindable && !IsSeamlessTravel && !bIsRecording)
	{
		if (bNetStartup || !TrackedRewindActorsByGUID.Contains(GuidCache->NetGUIDLookup.FindRef(Actor)))
		{
			// This may happen during playback due to new versions of code playing captures with old versions.
			// but this should never happen during recording (otherwise it's likely a game code bug). 
			// We catch that case below.
			UE_LOG(LogDemo, Warning, TEXT("Replay Rewindable Actor destroyed during playback. Replay may show artifacts (%s)"), *Actor->GetFullName());
		}
	}

	if (bIsRecording && bNetStartup)
	{
		// We don't want to send any destruction info in this case, because the actor should stick around.
		// The Replay will manage deleting this when it performs streaming or travel behavior.
		if (IsSeamlessTravel)
		{
			// This is a stripped down version of UNetDriver::NotifyActorDestroy and UActorChannel::Close
			// combined, and should be kept up to date with those methods.
		
			// Remove the actor from the property tracker map
			RepChangedPropertyTrackerMap.Remove(Actor);

			if (UNetConnection* Connection = ClientConnections[0])
			{
				if (Actor->bNetTemporary)
				{
					Connection->SentTemporaries.Remove(Actor);
				}

				if (UActorChannel* Channel = Connection->FindActorChannelRef(Actor))
				{
					check(Channel->OpenedLocally);
					Channel->bClearRecentActorRefs = false;
					Channel->SetClosingFlag();
					Channel->Actor = nullptr;
					Channel->CleanupReplicators(false);
				}
	
				Connection->DormantReplicatorMap.Remove(Actor);
			}
	
			GetNetworkObjectList().Remove(Actor);
			RenamedStartupActors.Remove(Actor->GetFName());
			return;
		}
		else
		{
			// This was deleted due to a game interaction, which isn't supported for Rewindable actors (while recording).
			// However, since the actor is going to be deleted imminently, we need to track it.
			UE_CLOG(bActorRewindable, LogDemo, Warning, TEXT("Replay Rewindable Actor destroyed during recording. Replay may show artifacts (%s)"), *Actor->GetFullName());

			UE_LOG(LogDemo, VeryVerbose, TEXT("NotifyActyorDestroyed: adding actor to deleted startup list: %s"), *Actor->GetFullName());
			DeletedNetStartupActors.Add( Actor->GetFullName() );

			FNetworkGUID NetGUID = GuidCache->NetGUIDLookup.FindRef(Actor);
			if (NetGUID.IsValid())
			{
				DeletedNetStartupActorGUIDs.Add(NetGUID);
			}
		}
	}

	TUniquePtr<FScopedPacketManager> PacketManager(ConditionallyCreatePacketManager(*Actor->GetLevel()));
	Super::NotifyActorDestroyed( Actor, IsSeamlessTravel );
}

void UDemoNetDriver::CleanupOutstandingRewindActors()
{
	UWorld* LocalWorld = GetWorld();

	for (const FNetworkGUID& NetGUID : TrackedRewindActorsByGUID)
	{
		if (FNetGuidCacheObject* CacheObject = GuidCache->ObjectLookup.Find(NetGUID))
		{
			if (AActor* Actor = Cast<AActor>(CacheObject->Object))
			{
				// Destroy the actor before removing entries from the GuidCache so its entries are still valid in NotifyActorDestroyed.
				LocalWorld->DestroyActor(Actor);

				ensureMsgf(GuidCache->NetGUIDLookup.Remove(CacheObject->Object) > 0, TEXT("CleanupOutstandingRewindActors: No entry found for %d in NetGUIDLookup"), NetGUID.Value);
				GuidCache->ObjectLookup.Remove(NetGUID);
				CacheObject->bNoLoad = false;
			}
			else
			{
				UE_LOG(LogDemo, Warning, TEXT("CleanupOutstandingRewindActors - Invalid object for %d, skipping."), NetGUID.Value);
				continue;
			}
		}	
		else
		{
			UE_LOG(LogDemo, Warning, TEXT("CleanupOutstandingRewindActors - CacheObject not found for %s"), NetGUID.Value);
		}
	}

	TrackedRewindActorsByGUID.Empty();
}

void UDemoNetDriver::NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor)
{
	const bool bValidChannel = ensureMsgf(Channel, TEXT("NotifyActorChannelOpen called with invalid channel"));
	const bool bValidActor = ensureMsgf(Actor, TEXT("NotifyActorChannelOpen called with invalid actor"));
	
	// Rewind the actor if necessary.
	// This should be called before any other notifications / data reach the Actor.
	if (bValidChannel && bValidActor && TrackedRewindActorsByGUID.Remove(Channel->ActorNetGUID) > 0)
	{
		Actor->RewindForReplay();
	}	
}

void UDemoNetDriver::NotifyActorLevelUnloaded(AActor* Actor)
{
	if (ServerConnection != nullptr)
	{
		// This is a combination of the Client and Server logic for destroying a channel,
		// since we won't actually be sending data back and forth.
		if (UActorChannel* ActorChannel = ServerConnection->FindActorChannelRef(Actor))
		{
			ServerConnection->RemoveActorChannel(Actor);
			ActorChannel->Actor = nullptr;
			ActorChannel->ConditionalCleanUp(false, EChannelCloseReason::LevelUnloaded);
		}
	}

	Super::NotifyActorLevelUnloaded(Actor);
}

void UDemoNetDriver::QueueNetStartupActorForRollbackViaDeletion( AActor* Actor )
{
	check( Actor != nullptr );

	if ( !Actor->IsNetStartupActor() )
	{
		return;		// We only want startup actors
	}

	if ( !IsPlaying() )
	{
		return;		// We should only be doing this at runtime while playing a replay
	}

	const FString ActorFullName = Actor->GetFullName();
	if ( RollbackNetStartupActors.Contains( ActorFullName ) )
	{
		return;		// This actor is already queued up
	}

	if ( Actor->bReplayRewindable )
	{
		UE_LOG(LogDemo, Warning, TEXT("Attempted to queue a Replay Rewindable Actor for Rollback Via Deletion. Replay may have artifacts (%s)"), *ActorFullName);
		return;
	}

	FRollbackNetStartupActorInfo& RollbackActor = RollbackNetStartupActors.Add(ActorFullName);

	RollbackActor.Name		= Actor->GetFName();
	RollbackActor.Archetype	= Actor->GetArchetype();
	RollbackActor.Location	= Actor->GetActorLocation();
	RollbackActor.Rotation	= Actor->GetActorRotation();
	RollbackActor.Level		= Actor->GetLevel();

	if (GDemoSaveRollbackActorState != 0)
	{
		TSharedPtr<FObjectReplicator> NewReplicator = MakeShared<FObjectReplicator>();
		if (NewReplicator.IsValid())
		{
			NewReplicator->InitWithObject(Actor->GetArchetype(), ServerConnection, false);

			if (NewReplicator->RepLayout.IsValid() && NewReplicator->RepState.IsValid())
			{
				FRepShadowDataBuffer ShadowData(NewReplicator->RepState->StaticBuffer.GetData());
				FConstRepObjectDataBuffer ActorData(Actor);

				if (NewReplicator->RepLayout->DiffStableProperties(nullptr, &RollbackActor.ObjReferences, ShadowData, ActorData))
				{
					RollbackActor.RepState = MakeShareable(NewReplicator->RepState.Release());
				}
			}
		}

		for (UActorComponent* ActorComp : Actor->GetComponents())
		{
			if (ActorComp)
			{
				TSharedPtr<FObjectReplicator> SubObjReplicator = MakeShared<FObjectReplicator>();
				if (SubObjReplicator.IsValid())
				{
					SubObjReplicator->InitWithObject(ActorComp->GetArchetype(), ServerConnection, false);

					if (SubObjReplicator->RepLayout.IsValid() && SubObjReplicator->RepState.IsValid())
					{
						FRepShadowDataBuffer ShadowData(SubObjReplicator->RepState->StaticBuffer.GetData());
						FConstRepObjectDataBuffer ActorCompData(ActorComp);

						if (SubObjReplicator->RepLayout->DiffStableProperties(nullptr, &RollbackActor.ObjReferences, ShadowData, ActorCompData))
						{
							RollbackActor.SubObjRepState.Add(ActorComp->GetFullName(), MakeShareable(SubObjReplicator->RepState.Release()));
						}
					}
				}
			}
		}
	}
}

void UDemoNetDriver::ForceNetUpdate(AActor* Actor)
{
	UReplicationDriver* RepDriver = GetReplicationDriver();
	if (RepDriver)
	{
		RepDriver->ForceNetUpdate(Actor);
	}
	else
	{
		if ( FNetworkObjectInfo* NetActor = FindNetworkObjectInfo(Actor) )
		{
			// replays use update times relative to DemoCurrentTime and not World->TimeSeconds
			NetActor->NextUpdateTime = DemoCurrentTime - 0.01f;
		}
	}
	}

UChannel* UDemoNetDriver::InternalCreateChannelByName(const FName& ChName)
{
	// In case of recording off the game thread with CVarDemoClientRecordAsyncEndOfFrame,
	// we need to clear the async flag on the channel so that it will get cleaned up by GC.
	// This should be safe since channel objects don't interact with async loading, and
	// async recording happens in a very controlled manner.
	UChannel* NewChannel = Super::InternalCreateChannelByName(ChName);
	if (NewChannel)
	{
		NewChannel->ClearInternalFlags(EInternalObjectFlags::Async);
	}
	return NewChannel;
}

void UDemoNetDriver::NotifyDemoPlaybackFailure(EDemoPlayFailure::Type FailureType)
{
	UE_LOG(LogDemo, Warning, TEXT("Demo playback failure: '%s'"), EDemoPlayFailure::ToString(FailureType));

	const bool bIsPlaying = IsPlaying();

	// fire delegate
	OnDemoFailedToStart.Broadcast(this, FailureType);

	StopDemo();

	if (bIsPlaying)
	{
		if (UWorld* LocalWorld = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				GameInstance->HandleDemoPlaybackFailure(FailureType, FString(EDemoPlayFailure::ToString(FailureType)));
			}
		}
	}
}

FString UDemoNetDriver::GetDemoPath() const
{
	if (ReplayStreamer.IsValid())
	{
		FString DemoPath;
		if (ReplayStreamer->GetDemoPath(DemoPath) == EStreamingOperationResult::Success)
		{
			return DemoPath;
		}
	}

	return FString();
}

bool UDemoNetDriver::UpdateExternalDataForActor(AActor* Actor)
{
	FRepChangedPropertyTracker* PropertyTracker = RepChangedPropertyTrackerMap.FindChecked(Actor).Get();	

	if (PropertyTracker->ExternalData.Num() == 0)
	{
		return false;
	}

	if (FNetworkGUID* NetworkGUID = GuidCache->NetGUIDLookup.Find(Actor))
	{
		ObjectsWithExternalData.Add({ Actor, *NetworkGUID });

		return true;
	}
	else
	{
		// Clear external data if the actor has never replicated yet (and doesn't have a net guid)
		PropertyTracker->ExternalData.Reset();
		PropertyTracker->ExternalDataNumBits = 0;

		return false;
	}
}

bool UDemoNetDriver::ShouldReplicateFunction(AActor* Actor, UFunction* Function) const
{
	const bool bRecordingMulticast = (Function && Function->FunctionFlags & FUNC_NetMulticast) && IsRecording();
	return bRecordingMulticast || Super::ShouldReplicateFunction(Actor, Function);
}

bool UDemoNetDriver::ShouldReplicateActor(AActor* Actor) const
{
	// replicate actors that share the demo net driver name, or actors belonging to the game net driver
	return Super::ShouldReplicateActor(Actor) || (Actor && Actor->GetNetDriverName() == NAME_GameNetDriver);
}

/*
* If a large number of Actors makes it onto the NetworkObjectList, and Demo Recording is limited,
* then we can easily hit cases where building the Consider List and Sorting it can take up the
* entire time slice. In that case, we'll have spent a lot of time setting up for replication,
* but never actually doing it.
* Further, if dormancy is used, dormant actors need to replicate once before they're removed from
* the NetworkObjectList. That means in the worst case, we can have a large number of dormant actors
* artificially driving up consider / sort times.
*
* To prevent that, we'll throttle the amount of time we spend prioritize next frame based
* on how much time it took this frame.
*
* @param TimeSlicePercent	The current percent of time allocated to building consider lists / prioritizing.
* @param ReplicatedPercet	The percent of actors that were replicated this last frame.
*/
void UDemoNetDriver::AdjustConsiderTime(const float ReplicatedPercent)
{
	if (MaxDesiredRecordTimeMS > 0.f)
	{
		auto ConditionallySwap = [](float& Less, float& More)
		{
			if (More < Less)
			{
				Swap(Less, More);
			}
		};

		float DecreaseThreshold = CVarDemoDecreaseRepPrioritizeThreshold.GetValueOnAnyThread();
		float IncreaseThreshold = CVarDemoIncreaseRepPrioritizeThreshold.GetValueOnAnyThread();
		ConditionallySwap(DecreaseThreshold, IncreaseThreshold);

		float MinRepTime = CVarDemoMinimumRepPrioritizeTime.GetValueOnAnyThread();
		float MaxRepTime = CVarDemoMaximumRepPrioritizeTime.GetValueOnAnyThread();
		ConditionallySwap(MinRepTime, MaxRepTime);
		MinRepTime = FMath::Clamp<float>(MinRepTime, 0.1, 1.0);
		MaxRepTime = FMath::Clamp<float>(MaxRepTime, 0.1, 1.0);

		if (ReplicatedPercent > IncreaseThreshold)
		{
			RecordBuildConsiderAndPrioritizeTimeSlice += 0.1f;
		}
		else if (ReplicatedPercent < DecreaseThreshold)
		{
			RecordBuildConsiderAndPrioritizeTimeSlice *= (1.f - ReplicatedPercent) * 0.5f;
		}

		RecordBuildConsiderAndPrioritizeTimeSlice = FMath::Clamp<float>(RecordBuildConsiderAndPrioritizeTimeSlice, MinRepTime, MaxRepTime);
	}
}

/*-----------------------------------------------------------------------------
	UDemoPendingNetGame.
-----------------------------------------------------------------------------*/

UDemoPendingNetGame::UDemoPendingNetGame( const FObjectInitializer& ObjectInitializer ) : Super( ObjectInitializer )
{
}

void UDemoPendingNetGame::Tick( float DeltaTime )
{
	// Replays don't need to do anything here
}

void UDemoPendingNetGame::SendJoin()
{
	// Don't send a join request to a replay
}

void UDemoPendingNetGame::LoadMapCompleted(UEngine* Engine, FWorldContext& Context, bool bLoadedMapSuccessfully, const FString& LoadMapError)
{
#if !( UE_BUILD_SHIPPING || UE_BUILD_TEST )
	if ( CVarDemoForceFailure.GetValueOnGameThread() == 2 )
	{
		bLoadedMapSuccessfully = false;
	}
#endif

	// If we have a demo pending net game we should have a demo net driver
	check(DemoNetDriver);

	if ( !bLoadedMapSuccessfully )
	{
		DemoNetDriver->StopDemo();

		// If we don't have a world that means we failed loading the new world.
		// Since there is no world, we must free the net driver ourselves
		// Technically the pending net game should handle it, but things aren't quite setup properly to handle that either
		if ( Context.World() == nullptr )
		{
			GEngine->DestroyNamedNetDriver( Context.PendingNetGame, DemoNetDriver->NetDriverName );
		}

		Context.PendingNetGame = nullptr;

		GEngine->BrowseToDefaultMap( Context );

		UE_LOG( LogDemo, Error, TEXT( "UDemoPendingNetGame::HandlePostLoadMap: LoadMap failed: %s" ), *LoadMapError );
		if ( Context.OwningGameInstance )
		{
			Context.OwningGameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::LoadMap, FString( TEXT( "LoadMap failed" ) ) );
		}
		return;
	}
	
	DemoNetDriver->PendingNetGameLoadMapCompleted();
}

void UDemoNetDriver::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsCountingMemory())
	{
		// TODO: We don't currently track:
		//		Replay Streamers
		//		Dynamic Delegate Data
		//		QueuedReplayTasks.
		//		DemoURL

		DeletedNetStartupActors.CountBytes(Ar);

		for (FString& ActorString : DeletedNetStartupActors)
		{
			Ar << ActorString;
		}

		DeletedNetStartupActorGUIDs.CountBytes(Ar);

		// The map for RollbackNetStartupActors may have already been serialized,
		// However, that won't capture non-property members or properly count them.
		for (const auto& RollbackNetStartupActorPair : RollbackNetStartupActors)
		{
			RollbackNetStartupActorPair.Value.CountBytes(Ar);
		}


		ExternalDataToObjectMap.CountBytes(Ar);

		for (const auto& ExternalDataToObjectPair : ExternalDataToObjectMap)
		{
			ExternalDataToObjectPair.Value.CountBytes(Ar);
		}

		PlaybackPackets.CountBytes(Ar);

		for (const FPlaybackPacket& Packet : PlaybackPackets)
		{
			Packet.CountBytes(Ar);
		}

		UniqueStreamingLevels.CountBytes(Ar);
		NewStreamingLevelsThisFrame.CountBytes(Ar);
		NonQueuedGUIDsForScrubbing.CountBytes(Ar);
		QueuedReplayTasks.CountBytes(Ar);
		
		Ar << DemoSessionID;

		PlaybackDemoHeader.CountBytes(Ar);

		PrioritizedActors.CountBytes(Ar);

		LevelNamesAndTimes.CountBytes(Ar);
		for (const FLevelNameAndTime& LevelNameAndTime : LevelNamesAndTimes)
		{
			LevelNameAndTime.CountBytes(Ar);
		}

		LevelIntervals.CountBytes(Ar);
		TrackedRewindActorsByGUID.CountBytes(Ar);
		AllLevelStatuses.CountBytes(Ar);
		for (const FLevelStatus& LevelStatus : AllLevelStatuses)
		{
			LevelStatus.CountBytes(Ar);
		}

		LevelStatusesByName.CountBytes(Ar);
		for (const auto& LevelStatusNamePair : LevelStatusesByName)
		{
			LevelStatusNamePair.Key.CountBytes(Ar);
		}

		LevelStatusIndexByLevel.CountBytes(Ar);
		SeenLevelStatuses.CountBytes(Ar);
		LevelsPendingFastForward.CountBytes(Ar);
		ObjectsWithExternalData.CountBytes(Ar);
		CheckpointSaveContext.CountBytes(Ar);
		QueuedPacketsBeforeTravel.CountBytes(Ar);
		for (const FQueuedDemoPacket& QueuedPacket : QueuedPacketsBeforeTravel)
		{
			QueuedPacket.CountBytes(Ar);
		}
	}
}

void UDemoNetConnection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsCountingMemory())
	{
		QueuedDemoPackets.CountBytes(Ar);
		for (const FQueuedDemoPacket& QueuedPacket : QueuedDemoPackets)
		{
			QueuedPacket.CountBytes(Ar);
		}

		QueuedCheckpointPackets.CountBytes(Ar);
		for (const FQueuedDemoPacket& QueuedPacket : QueuedCheckpointPackets)
		{
			QueuedPacket.CountBytes(Ar);
		}
	}
}
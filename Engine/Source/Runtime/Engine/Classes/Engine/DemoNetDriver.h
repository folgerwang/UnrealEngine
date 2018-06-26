// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/BitReader.h"
#include "Misc/NetworkGuid.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "Misc/EngineVersion.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkVersion.h"
#include "NetworkReplayStreaming.h"
#include "Engine/DemoNetConnection.h"
#include "Net/RepLayout.h"
#include "DemoNetDriver.generated.h"

class Error;
class FNetworkNotify;
class UDemoNetDriver;
class UDemoNetConnection;
class FRepState;

DECLARE_LOG_CATEGORY_EXTERN( LogDemo, Log, All );

DECLARE_MULTICAST_DELEGATE(FOnGotoTimeMCDelegate);
DECLARE_DELEGATE_OneParam(FOnGotoTimeDelegate, const bool /* bWasSuccessful */);

DECLARE_MULTICAST_DELEGATE(FOnDemoFinishPlaybackDelegate);

class UDemoNetDriver;
class UDemoNetConnection;

DECLARE_MULTICAST_DELEGATE(FOnDemoFinishRecordingDelegate);

class FQueuedReplayTask : public TSharedFromThis<FQueuedReplayTask>
{
public:
	FQueuedReplayTask( UDemoNetDriver* InDriver ) : Driver( InDriver )
	{
	}

	virtual ~FQueuedReplayTask()
	{
	}

	virtual void	StartTask() = 0;
	virtual bool	Tick() = 0;
	virtual FName	GetName() const = 0;

	TWeakObjectPtr<UDemoNetDriver> Driver;
};

class FReplayExternalData
{
public:
	FReplayExternalData() : TimeSeconds( 0.0f )
	{
	}

	FReplayExternalData( const FBitReader& InReader, const float InTimeSeconds ) : Reader( InReader ), TimeSeconds( InTimeSeconds )
	{
	}

	FBitReader	Reader;
	float		TimeSeconds;
};

// Using an indirect array here since FReplayExternalData stores an FBitReader, and it's not safe to store an FArchive directly in a TArray.
typedef TIndirectArray< FReplayExternalData > FReplayExternalDataArray;

struct FPlaybackPacket
{
	TArray< uint8 >		Data;
	float				TimeSeconds;
	int32				LevelIndex;
	uint32				SeenLevelIndex;
};

enum ENetworkVersionHistory
{
	HISTORY_REPLAY_INITIAL					= 1,
	HISTORY_SAVE_ABS_TIME_MS				= 2,			// We now save the abs demo time in ms for each frame (solves accumulation errors)
	HISTORY_INCREASE_BUFFER					= 3,			// Increased buffer size of packets, which invalidates old replays
	HISTORY_SAVE_ENGINE_VERSION				= 4,			// Now saving engine net version + InternalProtocolVersion
	HISTORY_EXTRA_VERSION					= 5,			// We now save engine/game protocol version, checksum, and changelist
	HISTORY_MULTIPLE_LEVELS					= 6,			// Replays support seamless travel between levels
	HISTORY_MULTIPLE_LEVELS_TIME_CHANGES	= 7,			// Save out the time that level changes happen
	HISTORY_DELETED_STARTUP_ACTORS			= 8,			// Save DeletedNetStartupActors inside checkpoints
	HISTORY_HEADER_FLAGS					= 9,			// Save out enum flags with demo header
	HISTORY_LEVEL_STREAMING_FIXES			= 10,			// Optional level streaming fixes.
	HISTORY_SAVE_FULL_ENGINE_VERSION		= 11,			// Now saving the entire FEngineVersion including branch name
	
	// -----<new versions can be added before this line>-------------------------------------------------
	HISTORY_PLUS_ONE,
	HISTORY_LATEST 							= HISTORY_PLUS_ONE - 1
};

static const uint32 MIN_SUPPORTED_VERSION = HISTORY_EXTRA_VERSION;

static const uint32 NETWORK_DEMO_MAGIC				= 0x2CF5A13D;
static const uint32 NETWORK_DEMO_VERSION			= HISTORY_LATEST;
static const uint32 MIN_NETWORK_DEMO_VERSION		= HISTORY_EXTRA_VERSION;

static const uint32 NETWORK_DEMO_METADATA_MAGIC		= 0x3D06B24E;
static const uint32 NETWORK_DEMO_METADATA_VERSION	= 0;

USTRUCT()
struct FLevelNameAndTime
{
	GENERATED_USTRUCT_BODY()

	FLevelNameAndTime()
		: LevelChangeTimeInMS(0)
	{}

	FLevelNameAndTime(const FString& InLevelName, uint32 InLevelChangeTimeInMS)
		: LevelName(InLevelName)
		, LevelChangeTimeInMS(InLevelChangeTimeInMS)
	{}

	UPROPERTY()
	FString LevelName;

	UPROPERTY()
	uint32 LevelChangeTimeInMS;

	friend FArchive& operator<<(FArchive& Ar, FLevelNameAndTime& LevelNameAndTime)
	{
		Ar << LevelNameAndTime.LevelName;
		Ar << LevelNameAndTime.LevelChangeTimeInMS;
		return Ar;
	}
};

enum class EReplayHeaderFlags : uint32
{
	None				= 0,
	ClientRecorded		= ( 1 << 0 ),
	HasStreamingFixes	= ( 1 << 1 ),
};

ENUM_CLASS_FLAGS(EReplayHeaderFlags);

struct FNetworkDemoHeader
{
	uint32	Magic;									// Magic to ensure we're opening the right file.
	uint32	Version;								// Version number to detect version mismatches.
	uint32	NetworkChecksum;						// Network checksum
	uint32	EngineNetworkProtocolVersion;			// Version of the engine internal network format
	uint32	GameNetworkProtocolVersion;				// Version of the game internal network format

	DEPRECATED(4.20, "Changelist is deprecated, use EngineVersion.GetChangelist() instead.")
	uint32	Changelist;								// Engine changelist built from

	FEngineVersion EngineVersion;					// Full engine version on which the replay was recorded
	EReplayHeaderFlags HeaderFlags;					// Replay flags
	TArray<FLevelNameAndTime> LevelNamesAndTimes;	// Name and time changes of levels loaded for demo
	TArray<FString> GameSpecificData;				// Area for subclasses to write stuff

	FNetworkDemoHeader() :
		Magic( NETWORK_DEMO_MAGIC ),
		Version( NETWORK_DEMO_VERSION ),
		NetworkChecksum( FNetworkVersion::GetLocalNetworkVersion() ),
		EngineNetworkProtocolVersion( FNetworkVersion::GetEngineNetworkProtocolVersion() ),
		GameNetworkProtocolVersion( FNetworkVersion::GetGameNetworkProtocolVersion() ),
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Changelist( FEngineVersion::Current().GetChangelist() ),
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		EngineVersion( FEngineVersion::Current() ),
		HeaderFlags( EReplayHeaderFlags::None )
	{
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FNetworkDemoHeader(const FNetworkDemoHeader& Other) = default;
	FNetworkDemoHeader(FNetworkDemoHeader&& Other) = default;
	FNetworkDemoHeader& operator=(const FNetworkDemoHeader& Other) = default;
	FNetworkDemoHeader& operator=(FNetworkDemoHeader&& Other) = default;
	~FNetworkDemoHeader() = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	friend FArchive& operator << ( FArchive& Ar, FNetworkDemoHeader& Header )
	{
		Ar << Header.Magic;

		// Check magic value
		if ( Header.Magic != NETWORK_DEMO_MAGIC )
		{
			UE_LOG( LogDemo, Error, TEXT( "Header.Magic != NETWORK_DEMO_MAGIC" ) );
			Ar.SetError();
			return Ar;
		}

		Ar << Header.Version;

		// Check version
		if ( Header.Version < MIN_NETWORK_DEMO_VERSION )
		{
			UE_LOG( LogDemo, Error, TEXT( "Header.Version < MIN_NETWORK_DEMO_VERSION. Header.Version: %i, MIN_NETWORK_DEMO_VERSION: %i" ), Header.Version, MIN_NETWORK_DEMO_VERSION );
			Ar.SetError();
			return Ar;
		}

		Ar << Header.NetworkChecksum;
		Ar << Header.EngineNetworkProtocolVersion;
		Ar << Header.GameNetworkProtocolVersion;

		if (Header.Version >= HISTORY_SAVE_FULL_ENGINE_VERSION)
		{
			Ar << Header.EngineVersion;
		}
		else
		{
			// Previous versions only stored the changelist
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Ar << Header.Changelist;
			
			if (Ar.IsLoading())
			{
				// We don't have any valid information except the changelist.
				Header.EngineVersion.Set(0, 0, 0, Header.Changelist, FString());
			}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		if (Header.Version < HISTORY_MULTIPLE_LEVELS)
		{
			FString LevelName;
			Ar << LevelName;
			Header.LevelNamesAndTimes.Add(FLevelNameAndTime(LevelName, 0));
		}
		else if (Header.Version == HISTORY_MULTIPLE_LEVELS)
		{
			TArray<FString> LevelNames;
			Ar << LevelNames;

			for (const FString& LevelName : LevelNames)
			{
				Header.LevelNamesAndTimes.Add(FLevelNameAndTime(LevelName, 0));
			}
		}
		else
		{
			Ar << Header.LevelNamesAndTimes;
		}

		if (Header.Version >= HISTORY_HEADER_FLAGS)
		{
			Ar << Header.HeaderFlags;
		}

		Ar << Header.GameSpecificData;

		return Ar;
	}
};

/** Information about net startup actors that need to be rolled back by being destroyed and re-created */
USTRUCT()
struct FRollbackNetStartupActorInfo
{
	GENERATED_BODY()

	FName		Name;
	UPROPERTY()
	UObject*	Archetype;
	FVector		Location;
	FRotator	Rotation;
	UPROPERTY()
	ULevel*		Level;

	TSharedPtr<FRepState> RepState;
	TMap<FString, TSharedPtr<FRepState>> SubObjRepState;

	UPROPERTY()
	TArray<UObject*> ObjReferences;
};

struct FDemoSavedRepObjectState
{
	TWeakObjectPtr<const UObject> Object;
	TSharedPtr<FRepLayout> RepLayout;
	FRepStateStaticBuffer PropertyData;
};

typedef TArray<struct FDemoSavedRepObjectState> FDemoSavedPropertyState;

/**
 * Simulated network driver for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class ENGINE_API UDemoNetDriver : public UNetDriver
{
	GENERATED_UCLASS_BODY()

	/** Current record/playback frame number */
	int32 DemoFrameNum;

	/** Total time of demo in seconds */
	float DemoTotalTime;

	/** Current record/playback position in seconds */
	float DemoCurrentTime;

	/** Old current record/playback position in seconds (so we can restore on checkpoint failure) */
	float OldDemoCurrentTime;

	/** Total number of frames in the demo */
	int32 DemoTotalFrames;

	/** True if we are at the end of playing a demo */
	bool bDemoPlaybackDone;

	/** True if as have paused all of the channels */
	bool bChannelsArePaused;

	/** Index of LevelNames that is currently loaded */
	int32 CurrentLevelIndex;

	/** This is our spectator controller that is used to view the demo world from */
	APlayerController* SpectatorController;

	/** Our network replay streamer */
	TSharedPtr< class INetworkReplayStreamer >	ReplayStreamer;

	uint32 GetDemoCurrentTimeInMS() { return (uint32)( (double)DemoCurrentTime * 1000 ); }

	/** Internal debug timing/tracking */
	double		AccumulatedRecordTime;
	double		LastRecordAvgFlush;
	double		MaxRecordTime;
	int32		RecordCountSinceFlush;

	/** When we save a checkpoint, we remember all of the actors that need a checkpoint saved out by adding them to this list */
	TArray< TWeakObjectPtr< AActor > > PendingCheckpointActors;

	/** Net startup actors that need to be destroyed after checkpoints are loaded */
	TSet< FString >									DeletedNetStartupActors;

	/** 
	 * Net startup actors that need to be rolled back during scrubbing by being destroyed and re-spawned 
	 * NOTE - DeletedNetStartupActors will take precedence here, and destroy the actor instead
	 */
	UPROPERTY(transient)
	TMap< FString, FRollbackNetStartupActorInfo >	RollbackNetStartupActors;

	/** Checkpoint state */
	FPackageMapAckState CheckpointAckState;					// Current ack state of packagemap for the current checkpoint being saved
	double				TotalCheckpointSaveTimeSeconds;		// Total time it took to save checkpoint across all frames
	int32				TotalCheckpointSaveFrames;			// Total number of frames used to save a checkpoint
	double				LastCheckpointTime;					// Last time a checkpoint was saved

	void		RespawnNecessaryNetStartupActors(TArray<AActor*>& SpawnedActors, ULevel* Level = nullptr);

	virtual bool ShouldSaveCheckpoint();

	void		SaveCheckpoint();
	void		TickCheckpoint();
	bool		LoadCheckpoint( FArchive* GotoCheckpointArchive, int64 GotoCheckpointSkipExtraTimeInMS );

	/** Returns true if we're in the process of saving a checkpoint. */
	bool		IsSavingCheckpoint() const;

	void		SaveExternalData( FArchive& Ar );
	void		LoadExternalData( FArchive& Ar, const float TimeSeconds );

	/** Public delegate for external systems to be notified when scrubbing is complete. Only called for successful scrub. */
	FOnGotoTimeMCDelegate OnGotoTimeDelegate;

	/** Delegate for external systems to be notified when demo playback ends */
	FOnDemoFinishPlaybackDelegate OnDemoFinishPlaybackDelegate;

	/** Public Delegate for external systems to be notified when replay recording is about to finish. */
	FOnDemoFinishRecordingDelegate OnDemoFinishRecordingDelegate;

	bool		IsLoadingCheckpoint() const { return bIsLoadingCheckpoint; }
	
	bool		IsPlayingClientReplay() const;

	/** ExternalDataToObjectMap is used to map a FNetworkGUID to the proper FReplayExternalDataArray */
	TMap< FNetworkGUID, FReplayExternalDataArray > ExternalDataToObjectMap;
		
	/** PlaybackPackets are used to buffer packets up when we read a demo frame, which we can then process when the time is right */
	TArray< FPlaybackPacket > PlaybackPackets;

	/**
	 * During recording, all unique streaming levels since recording started.
	 * During playback, all streaming level instances we've created.
	 */
	TSet< TWeakObjectPtr< UObject > >	UniqueStreamingLevels;

	/**
	 * During recording, streaming levels waiting to be saved next frame.
	 * During playback, streaming levels that have recently become visible.
	 */
	TSet< TWeakObjectPtr< UObject > >	NewStreamingLevelsThisFrame;

	bool bRecordMapChanges;

private:
	bool		bIsFastForwarding;
	bool		bIsFastForwardingForCheckpoint;
	bool		bWasStartStreamingSuccessful;
	bool		bIsLoadingCheckpoint;

	TArray<FNetworkGUID> NonQueuedGUIDsForScrubbing;

	// Replay tasks
	TArray< TSharedPtr< FQueuedReplayTask > >	QueuedReplayTasks;
	TSharedPtr< FQueuedReplayTask >				ActiveReplayTask;
	TSharedPtr< FQueuedReplayTask >				ActiveScrubReplayTask;

	/** Set via GotoTimeInSeconds, only fired once (at most). Called for successful or failed scrub. */
	FOnGotoTimeDelegate OnGotoTimeDelegate_Transient;
	
	/** Saved server time after loading a checkpoint, so that we can set the server time as accurately as possible after the fast-forward */
	float SavedReplicatedWorldTimeSeconds;

	/** Saved fast-forward time, used for correcting world time after the fast-forward is complete */
	float SavedSecondsToSkip;

	/** Cached replay URL, so that the driver can access the map name and any options later */
	FURL DemoURL;

	/** The unique identifier for the lifetime of this object. */
	FString DemoSessionID;

	/** This header is valid during playback (so we know what version to pass into serializers, etc */
	FNetworkDemoHeader PlaybackDemoHeader;

	/** Optional time quota for actor replication during recording. Going over this limit effectively lowers the net update frequency of the remaining actors. Negative values are considered unlimited. */
	float MaxDesiredRecordTimeMS;

	/**
	 * Maximum time allowed each frame to spend on saving a checkpoint. If 0, it will save the checkpoint in a single frame, regardless of how long it takes.
	 * See also demo.CheckpointSaveMaxMSPerFrameOverride.
	 */
	UPROPERTY(Config)
	float CheckpointSaveMaxMSPerFrame;

	/** A player controller that this driver should consider its viewpoint for actor prioritization purposes. */
	TWeakObjectPtr<APlayerController> ViewerOverride;

	/** Array of prioritized actors, used in TickDemoRecord. Stored as a member so that its storage doesn't have to be re-allocated each frame. */
	TArray<FActorPriority> PrioritizedActors;

	/** If true, recording will prioritize replicating actors based on the value that AActor::GetReplayPriority returns. */
	bool bPrioritizeActors;

	/** If true, will skip recording, but leaves the replay open so that recording can be resumed again. */
	bool bPauseRecording;

	/** List of levels used in the current replay */
	TArray<FLevelNameAndTime> LevelNamesAndTimes;

	/** Does the actual work of TickFlush, either on the main thread or in a task thread in parallel with Slate. */
	void TickFlushInternal(float DeltaSeconds);

	/** Returns either CheckpointSaveMaxMSPerFrame or the value of demo.CheckpointSaveMaxMSPerFrameOverride if it's >= 0. */
	float GetCheckpointSaveMaxMSPerFrame() const;

	/** Returns the last checkpoint time in integer milliseconds. */
	uint32 GetLastCheckpointTimeInMS() const { return (uint32)( (double)LastCheckpointTime * 1000 ); }

	/** Adds a new level to the level list */
	void AddNewLevel(const FString& NewLevelName);

public:

	// UNetDriver interface.

	virtual bool InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void FinishDestroy() override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual bool InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) override;
	virtual bool InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void TickFlush( float DeltaSeconds ) override;
	virtual void TickDispatch( float DeltaSeconds ) override;
	virtual void ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = nullptr ) override;
	virtual bool IsAvailable() const override { return true; }
	void SkipTime(const float InTimeToSkip);
	void SkipTimeInternal( const float SecondsToSkip, const bool InFastForward, const bool InIsForCheckpoint );
	bool InitConnectInternal(FString& Error);
	virtual bool ShouldClientDestroyTearOffActors() const override;
	virtual bool ShouldSkipRepNotifies() const override;
	virtual bool ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const override;
	virtual bool ShouldIgnoreRPCs() const override;
	virtual FNetworkGUID GetGUIDForActor(const AActor* InActor) const override;
	virtual AActor* GetActorForGUID(FNetworkGUID InGUID) const override;
	virtual bool ShouldReceiveRepNotifiesForObject(UObject* Object) const override;
	virtual void ForceNetUpdate(AActor* Actor) override;
	virtual bool IsServer() const override;

	/** Called when we are already recording but have traveled to a new map to start recording again */
	bool ContinueListen(FURL& ListenURL);

	/** 
	 * Scrubs playback to the given time. 
	 * 
	 * @param TimeInSeconds
	 * @param InOnGotoTimeDelegate		Delegate to call when finished. Will be called only once at most.
	*/
	void GotoTimeInSeconds(const float TimeInSeconds, const FOnGotoTimeDelegate& InOnGotoTimeDelegate = FOnGotoTimeDelegate());

	bool IsRecording() const;
	bool IsPlaying() const;

	FString GetDemoURL() const { return DemoURL.ToString(); }

	/** Sets the desired maximum recording time in milliseconds. */
	void SetMaxDesiredRecordTimeMS(const float InMaxDesiredRecordTimeMS) { MaxDesiredRecordTimeMS = InMaxDesiredRecordTimeMS; }

	/** Sets the controller to use as the viewpoint for recording prioritization purposes. */
	void SetViewerOverride(APlayerController* const InViewerOverride ) { ViewerOverride = InViewerOverride; }

	/** Enable or disable prioritization of actors for recording. */
	void SetActorPrioritizationEnabled(const bool bInPrioritizeActors) { bPrioritizeActors = bInPrioritizeActors; }

	/** Sets CheckpointSaveMaxMSPerFrame. */
	void SetCheckpointSaveMaxMSPerFrame(const float InCheckpointSaveMaxMSPerFrame) { CheckpointSaveMaxMSPerFrame = InCheckpointSaveMaxMSPerFrame; }

	/** Called by a task thread if the engine is doing async end of frame tasks in parallel with Slate. */
	void TickFlushAsyncEndOfFrame(float DeltaSeconds);

	const TArray<FLevelNameAndTime>& GetLevelNameAndTimeList() { return LevelNamesAndTimes; }

	/** Returns the replicated state of every object on a current actor channel. Use the result to compare in DiffReplicatedProperties. */
	FDemoSavedPropertyState SavePropertyState() const;

	/** Compares the values of replicated properties stored in State with the current values of the object replicators. Logs and returns true if there were any differences. */
	bool ComparePropertyState(const FDemoSavedPropertyState& State) const;

public:

	UPROPERTY()
	bool bIsLocalReplay;

	/** @todo document */
	bool UpdateDemoTime( float* DeltaTime, float TimeDilation );

	/** Called when demo playback finishes, either because we reached the end of the file or because the demo spectator was destroyed */
	void DemoPlaybackEnded();

	/** @return true if the net resource is valid or false if it should not be used */
	virtual bool IsNetResourceValid(void) override { return true; }

	void TickDemoRecord( float DeltaSeconds );
	void PauseChannels( const bool bPause );
	void PauseRecording( const bool bInPauseRecording ) { bPauseRecording = bInPauseRecording; }
	bool IsRecordingPaused() const { return bPauseRecording; }

	bool ConditionallyProcessPlaybackPackets();
	void ProcessAllPlaybackPackets();
	bool ReadPacket( FArchive& Archive, uint8* OutReadBuffer, int32& OutBufferSize, const int32 MaxBufferSize );
	bool ConditionallyReadDemoFrameIntoPlaybackPackets( FArchive& Ar );

	bool ProcessPacket( const uint8* Data, int32 Count );
	bool ProcessPacket( const FPlaybackPacket& PlaybackPacket )
	{
		bool Result = true;
		if (!ShouldSkipPlaybackPacket(PlaybackPacket))
		{
			ProcessPacket(PlaybackPacket.Data.GetData(), PlaybackPacket.Data.Num());
			LastProcessedPacketTime = PlaybackPacket.TimeSeconds;
		}

		return Result;
	}

	void WriteDemoFrameFromQueuedDemoPackets( FArchive& Ar, TArray<FQueuedDemoPacket>& QueuedPackets, float FrameTime );
	void WritePacket( FArchive& Ar, uint8* Data, int32 Count );

	void TickDemoPlayback( float DeltaSeconds );
	
	DEPRECATED(4.20, "Please use the FinalizeFastForward that takes a double.")
	void FinalizeFastForward( const float StartTime ) { FinalizeFastForward(static_cast<double>(StartTime)); }
	void FinalizeFastForward( const double StartTime );
	
	void SpawnDemoRecSpectator( UNetConnection* Connection, const FURL& ListenURL );
	void ResetDemoState();
	void JumpToEndOfLiveReplay();
	void AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data);
	void AddOrUpdateEvent(const FString& EventName, const FString& Group, const FString& Meta, const TArray<uint8>& Data);

	DEPRECATED(4.20, "Please use a version of EnumerateEvents that accepts a FEnumerateEventsCallback delegate.")
	void EnumerateEvents(const FString& Group, FEnumerateEventsCompleteDelegate& Delegate) { EnumerateEvents(Group, UpgradeEnumerateEventsDelegate(Delegate)); }
	void EnumerateEvents(const FString& Group, const FEnumerateEventsCallback& Delegate);

	// In most cases, this is desirable over EnumerateEvents because it will explicitly use ActiveReplayName
	// instead of letting the streamer decide.
	void EnumerateEventsForActiveReplay(const FString& Group, const FEnumerateEventsCallback& Delegate);

	DEPRECATED(4.20, "Please use a version of RequestEventData that accepts a FRequestEventDataCallback delegate.")
	void RequestEventData(const FString& EventID, FOnRequestEventDataComplete& Delegate) { RequestEventData(EventID, UpgradeRequestEventDelegate(Delegate)); }
	void RequestEventData(const FString& EventID, const FRequestEventDataCallback& Delegate);

	// In most cases, this is desirable over EnumerateEvents because it will explicitly use ActiveReplayName
	// instead of letting the streamer decide.
	void RequestEventDataForActiveReplay(const FString& EventID, const FRequestEventDataCallback& Delegate);

	bool IsFastForwarding() const { return bIsFastForwarding; }

	FReplayExternalDataArray* GetExternalDataArrayForObject( UObject* Object );

	bool ReadDemoFrameIntoPlaybackPackets(FArchive& Ar, TArray<FPlaybackPacket>& Packets, const bool bForLevelFastForward, float* OutTime);
	bool ReadDemoFrameIntoPlaybackPackets(FArchive& Ar) { return ReadDemoFrameIntoPlaybackPackets(Ar, PlaybackPackets, false, nullptr); }

	/**
	 * Adds a join-in-progress user to the set of users associated with the currently recording replay (if any)
	 *
	 * @param UserString a string that uniquely identifies the user, usually his or her FUniqueNetId
	 */
	void AddUserToReplay(const FString& UserString);

	void StopDemo();

	DEPRECATED(4.20, "Please use the version of ReplayStreamingReady that accepts a FStartStreamingResult.")
	void ReplayStreamingReady(bool bSuccess, bool bRecord)
	{
		FStartStreamingResult Result;
		if (bSuccess)
		{
			Result.Result = EStreamingOperationResult::Success;
		}

		Result.bRecording = bRecord;
		ReplayStreamingReady(Result);
	}
	void ReplayStreamingReady(const FStartStreamingResult& Result);

	void AddReplayTask( FQueuedReplayTask* NewTask );
	bool IsAnyTaskPending() const;
	void ClearReplayTasks();
	bool ProcessReplayTasks();
	bool IsNamedTaskInQueue( const FName& Name ) const;
	FName GetNextQueuedTaskName() const;

	/** If a channel is associated with Actor, adds the channel's GUID to the list of GUIDs excluded from queuing bunches during scrubbing. */
	void AddNonQueuedActorForScrubbing(AActor const* Actor);
	/** Adds the channel's GUID to the list of GUIDs excluded from queuing bunches during scrubbing. */
	void AddNonQueuedGUIDForScrubbing(FNetworkGUID InGUID);

	virtual bool IsLevelInitializedForActor( const AActor* InActor, const UNetConnection* InConnection ) const override;

	/** Called when a "go to time" operation is completed. */
	void NotifyGotoTimeFinished(bool bWasSuccessful);

	/** Read the streaming level information from the metadata after the level is loaded */
	void PendingNetGameLoadMapCompleted();
	
	virtual void NotifyActorDestroyed( AActor* ThisActor, bool IsSeamlessTravel=false ) override;
	virtual void NotifyActorLevelUnloaded( AActor* Actor ) override;
	virtual void NotifyStreamingLevelUnload( ULevel* InLevel ) override;

	/** Call this function during playback to track net startup actors that need a hard reset when scrubbing, which is done by destroying and then re-spawning */
	virtual void QueueNetStartupActorForRollbackViaDeletion( AActor* Actor );

	/** Called when seamless travel begins when recording a replay. */
	void OnSeamlessTravelStartDuringRecording(const FString& LevelName);
	/** @return the unique identifier for the lifetime of this object. */
	const FString& GetDemoSessionID() const { return DemoSessionID; }

	DEPRECATED(4.20, "OnDownloadHeaderComplete will be made private. Please remove any calls to it.")
	void OnDownloadHeaderComplete(const FDownloadHeaderResult& Result, int32 LevelIndex)
	{
	}

	/** Returns true if TickFlush can be called in parallel with the Slate tick. */
	bool ShouldTickFlushAsyncEndOfFrame() const;

	/** Returns whether or not this replay was recorded / is playing with Level Streaming fixes. */
	bool HasLevelStreamingFixes() const
	{
		return bHasLevelStreamingFixes;
	}

	/**
	 * Called when a new ActorChannel is opened, before the Actor is notified.
	 *
	 * @param Channel	The channel associated with the actor.
	 * @param Actor		The actor that was recently serialized.
	 */
	void PreNotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor);

	/**
	 * Gets the actively recording or playback replay (stream) name.
	 * Note, this will be empty when not recording or playing back.
	 */
	const FString& GetActiveReplayName() const
	{
		return ActiveReplayName;
	}

private:

	/** Called when the downloading header request from the replay streamer completes. */
	void OnDownloadHeaderCompletePrivate(const FDownloadHeaderResult& Result, int32 LevelIndex);

	void CleanupOutstandingRewindActors();

	// Tracks actors that will need to be rewound during scrubbing.
	// This list should always be empty outside of scrubbing.
	TSet<FNetworkGUID> TrackedRewindActorsByGUID;

	/**
	 * Helps keeps tabs on what levels are Ready, Have Seen data, Level Name, and Index into the main status list.
	 *
	 * A Level is not considered ready until the following criteria are met:
	 *	- UWorld::AddToWorld has been called, signifying the level is both Loaded and Visible (in the streaming sense).
	 *	- Either:
	 *		No packets of data have been processed for the level (yet),
	 *		OR The level has been fully fast-forwarded.
	 *
	 * A level is marked as Seen once the replay has seen a packet marked for the level.
	 */
	struct FLevelStatus
	{
		FLevelStatus(const FString& LevelPackageName) :
			LevelName(LevelPackageName),
			LevelIndex(INDEX_NONE),
			bIsReady(false),
			bHasBeenSeen(false)
		{
		}

		// Level name.
		FString LevelName;

		// Level index (in AllLevelStatuses).
		int32 LevelIndex;

		// Whether or not the level is ready to receive streaming data.
		bool bIsReady;

		// Whether or not we've seen replicated data for the level. Only set during playback.
		bool bHasBeenSeen;
	};

	// Tracks all available level statuses.
	// When Recording, this will be in order of replication, and all statuses will be assumed Seen and Visible (even if unmarked).
	// During Playback, there's no guaranteed order. Levels will be added either when they are added to the world, or when we handle the first
	// frame containing replicated data.
	// Use SeenLevelStatuses and LevelStatusesByName for querying.
	TArray<FLevelStatus> AllLevelStatuses;

	// Since Arrays are dynamically allocated, we can't just hold onto pointers.
	// If we tried, the underlying memory could be moved without us knowing.
	// Therefore, we track the Index into the array which should be independent of allocation.

	// Index of level status (in AllLevelStatuses list).
	TMap<FString, int32> LevelStatusesByName;

	// List of seen level statuses indices (in ALlLevelStatuses).
	TArray<int32> SeenLevelStatuses;

	// Time of the last packet we've processed (in seconds).
	float LastProcessedPacketTime;

	// Time of the last frame we've read (in seconds).
	float LatestReadFrameTime;

	// Whether or not the Streaming Level Fixes are enabled for capture or playback.
	bool bHasLevelStreamingFixes;

	// Levels that are currently pending for fast forward.
	// Using raw pointers, because we manually keep when levels are added and removed.
	TMap<class ULevel*, TSet<TWeakObjectPtr<class AActor>>> LevelsPendingFastForward;

	// Pairs of Level Indices to the remaining number of actors that need to be processed for a given Demo Frame.
	// Only used during recording.
	TArray<TPair<int32, int32>> NumActorsToProcessForLevel;

	// Only used during recording.
	uint32 NumLevelsAddedThisFrame;

	// Index into PlaybackPackets array. Used so we can process many packets in one frame and avoid removing them individually.
	int32 PlaybackPacketIndex;

	FLevelStatus& FindOrAddLevelStatus(const FString& LevelPackageName)
	{
		if (int32* LevelStatusIndex = LevelStatusesByName.Find(LevelPackageName))
		{
			return AllLevelStatuses[*LevelStatusIndex];
		}

		const int32 Index = AllLevelStatuses.Emplace(LevelPackageName);
		AllLevelStatuses[Index].LevelIndex = Index;

		LevelStatusesByName.Add(LevelPackageName, Index);
		NumLevelsAddedThisFrame++;

		return AllLevelStatuses[Index];
	}

	FLevelStatus& GetLevelStatus(const int32 SeenLevelIndex)
	{
		return AllLevelStatuses[SeenLevelStatuses[SeenLevelIndex - 1]];
	}

	FLevelStatus& GetLevelStatus(const FString& LevelPackageName)
	{
		return AllLevelStatuses[LevelStatusesByName[LevelPackageName]];
	}

	// Determines whether or not a packet should be skipped, based on it's level association.
	bool ShouldSkipPlaybackPacket(const FPlaybackPacket& Packet);

	void ResetLevelStatuses();
	void ClearLevelStreamingState()
	{
		AllLevelStatuses.Empty();
		LevelStatusesByName.Empty();
		SeenLevelStatuses.Empty();
		LevelsPendingFastForward.Empty();
		NumActorsToProcessForLevel.Empty();
		NumLevelsAddedThisFrame = 0;
	}

	/**
	 * Replicates the given prioritized actors, so their packets can be captured for recording.
	 * This should be used for normal frame recording.
	 * @see ReplicateCheckpointActor for recording during checkpoints.
	 *
	 * @param ToReplicate	The actors to replicate.
	 * @param Params		Implementation specific params necessary to replicate the actor.
	 *
	 * @return True if there is time remaining to replicate more actors. False otherwise.
	 */
	bool ReplicatePrioritizedActors(const TArray<FActorPriority>& ToReplicate, const class FRepActorsParams& Params);
	bool ReplicatePrioritizedActors(const TArray<FActorPriority*>& ToReplicate, const class FRepActorsParams& Params);
	bool ReplicatePrioritizedActor(const FActorPriority& ActorPriority, const class FRepActorsParams& Params);

	/**
	* Replicates the given prioritized actors, so their packets can be captured for recording.
	* This should be used for normal frame recording.
	* @see ReplicateCheckpointActor for recording during checkpoints.
	*
	* @param ToReplicate	The actors to replicate.
	* @param RepStartTime	The start time for replication.
	*
	* @return True if there is time remaining to replicate more Actors, False otherwise.
	*/
	bool ReplicateCheckpointActor(AActor* ToReplicate, UDemoNetConnection* ClientConnection, class FRepActorsCheckpointParams& Params);

	friend class FPendingTaskHelper;

	// Manages basic setup of newly visible levels, and queuing a FastForward task if necessary.
	void PrepFastForwardLevels();

	// Performs the logic for actually fast-forwarding a level.
	bool FastForwardLevels(FArchive* CheckpointArchive, int64 ExtraTime);

	// Hooks used to determine when levels are streamed in, streamed out, or if there's a map change.
	virtual void OnLevelAddedToWorld(ULevel* Level, UWorld* World) override;
	virtual void OnLevelRemovedFromWorld(ULevel* Level, UWorld* World) override;
	void OnPostLoadMapWithWorld(UWorld* World);

	// These should only ever be called when recording.
	TUniquePtr<class FScopedPacketManager> ConditionallyCreatePacketManager(ULevel& Level);
	TUniquePtr<class FScopedPacketManager> ConditionallyCreatePacketManager(int32 LevelIndex);

	FString GetLevelPackageName(const ULevel& InLevel);

protected:
	/** allows subclasses to write game specific data to demo header which is then handled by ProcessGameSpecificDemoHeader */
	virtual void WriteGameSpecificDemoHeader(TArray<FString>& GameSpecificData)
	{}
	/** allows subclasses to read game specific data from demo
	 * return false to cancel playback
	 */
	virtual bool ProcessGameSpecificDemoHeader(const TArray<FString>& GameSpecificData, FString& Error)
	{
		return true;
	}

	void ProcessClientTravelFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject);

	bool WriteNetworkDemoHeader(FString& Error);

	void ProcessSeamlessTravel(int32 LevelIndex);

	bool ReadPlaybackDemoHeader(FString& Error);

	bool DemoReplicateActor(AActor* Actor, UNetConnection* Connection, bool bMustReplicate);

	void SerializeGuidCache(TSharedPtr<class FNetGUIDCache> GuidCache, FArchive* CheckpointArchive);

	void NotifyDemoPlaybackFailure(EDemoPlayFailure::Type FailureType);

	TArray<FQueuedDemoPacket> QueuedPacketsBeforeTravel;

	bool bIsWaitingForHeaderDownload;
	bool bIsWaitingForStream;

private:

	FString ActiveReplayName;
};

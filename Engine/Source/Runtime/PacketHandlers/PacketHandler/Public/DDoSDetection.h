// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// Includes
#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"


// Defines

/** The limit for potential DDoS logspam, before log restrictions are enabled */
#define DDOS_LOGSPAM_LIMIT 64


// Delegates

/**
 * Delegate for allowing analytics to receive notification of detected DDoS attacks
 */
DECLARE_DELEGATE_OneParam(FDDoSSeverityEscalation, FString /*SeverityCategory*/);



/**
 * Struct containing the per-second packet counters
 */
struct PACKETHANDLER_API FDDoSPacketCounters
{
	/** Counter for non-NetConnection packets received, since the last per second quota period began */
	int32 NonConnPacketCounter;

	/** Counter for NetConnection packets received, since the last per second quota period began */
	int32 NetConnPacketCounter;

	/** Counter for bad non-NetConnection packets received, since the last per second quota period began */
	int32 BadPacketCounter;

	/** Counter for non-DDoS packet errors received */
	int32 ErrorPacketCounter;

	/** Counter for the number of packets (of any type) dropped, since the last per second quota period began */
	int32 DroppedPacketCounter;

	/** The worst per-frame packet receive time in milliseconds, over the ~1 second this packet counter history represents */
	int32 WorstFrameReceiveTimeMS;


	FDDoSPacketCounters()
		: NonConnPacketCounter(0)
		, NetConnPacketCounter(0)
		, BadPacketCounter(0)
		, ErrorPacketCounter(0)
		, DroppedPacketCounter(0)
		, WorstFrameReceiveTimeMS(0)
	{
	}
};


/**
 * Stores the DDoS detection state (either settings from the config file, or the active DDoS detection state)
 */
struct PACKETHANDLER_API FDDoSState
{
	/** The number of packets/sec before the next stage of DDoS detection is triggered */
	int32 EscalateQuotaPacketsPerSec;

	/** The number of bad (failed to process correctly) packets/sec, before the next stage of DDoS detection is triggered */
	int32 EscalateQuotaBadPacketsPerSec;

	/** The amount of time spent processing packets, before the next stage of DDoS detection is triggered */
	int16 EscalateTimeQuotaMSPerFrame;

	/** The limit for the number of non-NetConnection packets to process, each frame */
	int32 PacketLimitPerFrame;

	/** The limit for time spent processing non-NetConnection packets, each frame (counts all packets time, non-NetConn and NetConn) */
	int32 PacketTimeLimitMSPerFrame;

	/** The limit for time spent processing NetConnection packets, each frame (counts all packets time, non-NetConn and NetConn) */
	int32 NetConnPacketTimeLimitMSPerFrame;

	/** The amount of time, in seconds, before the current DDoS severity category cools off and de-escalates */
	int32 CooloffTime;


	FDDoSState()
		: EscalateQuotaPacketsPerSec(-1)
		, EscalateQuotaBadPacketsPerSec(-1)
		, EscalateTimeQuotaMSPerFrame(-1)
		, PacketLimitPerFrame(-1)
		, PacketTimeLimitMSPerFrame(-1)
		, NetConnPacketTimeLimitMSPerFrame(-1)
		, CooloffTime(-1)
	{
	}

	/**
	 * Whether or not the specified counters and time passed has hit any of the quota's
	 *
	 * @param InCounter		The counters to check against
	 * @param TimePassedMS	The amount of time passed in milliseconds, since the beginning of this state
	 * @return				Whether or not the quota was hit
	 */
	FORCEINLINE bool HasHitQuota(FDDoSPacketCounters& InCounter, int32 TimePassedMS) const
	{
		const bool bAtQuota = EscalateQuotaPacketsPerSec > 0 && InCounter.NonConnPacketCounter >= EscalateQuotaPacketsPerSec;
		const bool bAtBadQuota = EscalateQuotaBadPacketsPerSec > 0 && InCounter.BadPacketCounter >= EscalateQuotaBadPacketsPerSec;
		const bool bAtTimeQuota = EscalateTimeQuotaMSPerFrame > 0 && TimePassedMS > EscalateTimeQuotaMSPerFrame;

		return bAtQuota || bAtBadQuota || bAtTimeQuota;
	}
};

/**
 * DDoS detection state, with functions for applying the state to active DDoS detection
 */
struct PACKETHANDLER_API FDDoSStateConfig : public FDDoSState
{
	/** The name of the DDoS severity level this config section represents */
	FString SeverityCategory;


	FDDoSStateConfig()
		: SeverityCategory(TEXT(""))
	{
	}

	FORCEINLINE void ApplyState(FDDoSState& Target)
	{
		Target.EscalateQuotaPacketsPerSec		= EscalateQuotaPacketsPerSec;
		Target.EscalateQuotaBadPacketsPerSec	= EscalateQuotaBadPacketsPerSec;
		Target.EscalateTimeQuotaMSPerFrame		= EscalateTimeQuotaMSPerFrame;
		Target.PacketLimitPerFrame				= PacketLimitPerFrame;
		Target.PacketTimeLimitMSPerFrame		= PacketTimeLimitMSPerFrame;
		Target.NetConnPacketTimeLimitMSPerFrame	= NetConnPacketTimeLimitMSPerFrame;
		Target.CooloffTime						= CooloffTime;
	}

	/**
	 * Applies only the per-frame adjusted state (based on expected vs actual framerate), to active DDoS protection.
	 * ApplyState should be called, first.
	 */
	FORCEINLINE void ApplyAdjustedState(FDDoSState& Target, float FrameAdjustment)
	{
		// Exclude escalation triggers from this
		//Target.EscalateTimeQuotaMSPerFrame		= EscalateTimeQuotaMSPerFrame * FrameAdjustment;
		Target.PacketLimitPerFrame				= PacketLimitPerFrame * FrameAdjustment;
		Target.PacketTimeLimitMSPerFrame		= PacketTimeLimitMSPerFrame * FrameAdjustment;
		Target.NetConnPacketTimeLimitMSPerFrame	= NetConnPacketTimeLimitMSPerFrame * FrameAdjustment;
	}
};


/**
 * The main DDoS detection tracking class, for counting packets and applying restrictions.
 * Implemented separate to the NetDriver, to allow wider use e.g. potentially at socket level, if useful.
 */
class PACKETHANDLER_API FDDoSDetection : protected FDDoSPacketCounters, protected FDDoSState
{
public:
	FDDoSDetection()
		: bDDoSDetection(false)
		, bDDoSAnalytics(false)
		, bHitFrameNonConnLimit(false)
		, bHitFrameNetConnLimit(false)
		, NotifySeverityEscalation()
		, DetectionSeverity()
		, ActiveState(0)
		, WorstActiveState(0)
		, LastMetEscalationConditions(0.0)
		, bMetEscalationConditionsThisFrame(false)
		, bDDoSLogRestrictions(false)
		, LogHitCounter(0)
		, HitchTimeQuotaMS(-1)
		, HitchFrameTolerance(-1)
		, HitchFrameCount(0)
		, LastPerSecQuotaBegin(0.0)
		, CounterPerSecHistory()
		, LastCounterPerSecHistoryIdx(0)
		, StartFrameRecvTimestamp(0.0)
		, EndFrameRecvTimestamp(0.0)
		, StartFramePacketCount(0)
		, ExpectedFrameTime(0.0)
		, FrameAdjustment(0.f)
	{
	}

	/**
	 * Initializes the DDoS detection settings
	 *
	 * @param MaxTickRate	The maximum tick rate of the server
	 */
	void Init(int32 MaxTickRate);

	/**
	 * Updates the current DDoS detection severity state
	 *
	 * @param bEscalate		Whether or not we are escalating or de-escalating the severity state
	 */
	void UpdateSeverity(bool bEscalate);


	/**
	 * Triggered before packet receive begins, during the current frame
	 */
	void PreFrameReceive(float DeltaTime);

	/**
	 * Triggered after packet receive ends, during the current frame
	 */
	FORCEINLINE void PostFrameReceive()
	{
		EndFrameRecvTimestamp = FPlatformTime::Seconds();

		int32 FrameReceiveTimeMS = (int32)((EndFrameRecvTimestamp - StartFrameRecvTimestamp) * 1000.0);

		WorstFrameReceiveTimeMS = FMath::Max(FrameReceiveTimeMS, WorstFrameReceiveTimeMS);
	}


	/**
	 * Rate limited call to CheckNonConnQuotasAndLimits
	 */
	FORCEINLINE void CondCheckNonConnQuotasAndLimits()
	{
		// Limit checks to once every 128 packets
		if ((NonConnPacketCounter & 0x7F) == 0)
		{
			bHitFrameNonConnLimit = CheckNonConnQuotasAndLimits();
		}
	}

	/**
	 * Rate limited call to CheckNetConnLimits
	 */
	FORCEINLINE void CondCheckNetConnLimits()
	{
		// Limit checks to once every 128 packets
		if ((NetConnPacketCounter & 0x7F) == 0)
		{
			bHitFrameNetConnLimit = CheckNetConnLimits();
		}
	}


	/**
	 * Accessor for bDDoSLogRestrictions - doubles as a per-frame logspam counter, automatically disabling logs after a quota
	 */
	FORCEINLINE bool CheckLogRestrictions()
	{
		return bDDoSLogRestrictions || (bDDoSDetection && ++LogHitCounter > DDOS_LOGSPAM_LIMIT);
	}


	// Brief accessors

	FORCEINLINE void IncNonConnPacketCounter()			{ ++NonConnPacketCounter; }
	FORCEINLINE int32 GetNonConnPacketCounter() const	{ return NonConnPacketCounter; }
	FORCEINLINE void IncNetConnPacketCounter()			{ ++NetConnPacketCounter; }
	FORCEINLINE int32 GetNetConnPacketCounter() const	{ return NetConnPacketCounter; }
	FORCEINLINE void IncBadPacketCounter()				{ ++BadPacketCounter; }
	FORCEINLINE int32 GetBadPacketCounter() const		{ return BadPacketCounter; }
	FORCEINLINE void IncErrorPacketCounter()			{ ++ErrorPacketCounter; }
	FORCEINLINE int32 GetErrorPacketCounter() const		{ return ErrorPacketCounter; }
	FORCEINLINE void IncDroppedPacketCounter()			{ ++DroppedPacketCounter; }
	FORCEINLINE int32 GetDroppedPacketCounter() const	{ return DroppedPacketCounter; }

protected:
	/**
	 * Performs periodic checks on trigger quota's and packet limits, for non-NetConnection packets
	 *
	 * @return	Whether or not non-NetConnection packet limits have been reached
	 */
	bool CheckNonConnQuotasAndLimits();

	/**
	 * Performs periodic checks on NetConnection packet limits
	 *
	 * @return	Whether or not NetColnnection packet limits have been reached
	 */
	FORCEINLINE bool CheckNetConnLimits()
	{
		return NetConnPacketTimeLimitMSPerFrame > 0 &&
				(int32)((FPlatformTime::Seconds() - StartFrameRecvTimestamp) * 1000.0) > NetConnPacketTimeLimitMSPerFrame;
	}



public:
	/** Whether or not DDoS detection is presently enabled */
	bool bDDoSDetection;

	/** Whether or not analytics for DDoS detection is enabled */
	bool bDDoSAnalytics;

	/** Whether or not the current frame has reached non-NetConnection packet limits, and should block non-NetConnection packets */
	bool bHitFrameNonConnLimit;

	/** Whether or not the current frame has reached NetConnection packet limits, and should block ALL further packets */
	bool bHitFrameNetConnLimit;

	/** Analytics delegate for notifying of severity state escalations */
	FDDoSSeverityEscalation NotifySeverityEscalation;


protected:
	/** The different DDoS detection states, of escalating severity, depending on the strength of the DDoS */
	TArray<FDDoSStateConfig> DetectionSeverity;

	/** The currently active DDoS severity state settings */
	int8 ActiveState;

	/** The worst DDoS severity state that has been active - used for limiting analytics events */
	int8 WorstActiveState;

	/** The last time the previous severity states escalation conditions were met (to prevent bouncing up/down between states) */
	double LastMetEscalationConditions;

	/** Limit checking previous states escalation conditions to once per frame */
	bool bMetEscalationConditionsThisFrame;


	/** Whether or not restriction of log messages from non-NetConnection packets is enabled */
	bool bDDoSLogRestrictions;

	/** Counter for log restriction hits, in the current frame */
	int32 LogHitCounter;


	/** The amount of time since the previous frame, for detecting frame hitches, to prevent DDoS detection false positives */
	int32 HitchTimeQuotaMS;

	/** The number of frames spent hitching, before disabling false positive detection, and treating packet buildup as potential DDoS */
	int8 HitchFrameTolerance;

	/** The number of consecutive frames spent hitching */
	int32 HitchFrameCount;


	/** Timestamp for the last time per-second quota counting began */
	double LastPerSecQuotaBegin;

	/** Stores enough per second quota history, to allow all DetectionSeverity states to recalculate if their CooloffTime is reached */
	TArray<FDDoSPacketCounters> CounterPerSecHistory;

	/** The last written index of CounterPerSecHistory */
	int32 LastCounterPerSecHistoryIdx;


	/** The timestamp for the start of the current frames receive */
	double StartFrameRecvTimestamp;

	/** Timestamp for the end of the last frames receive loop */
	double EndFrameRecvTimestamp;

	/** Counts the packets from the start of the current frame */
	int32 StartFramePacketCount;

	/** The expected time between frames (1.0 / MaxTickRate) - used for adjusting limits/quota's based on DeltaTime */
	double ExpectedFrameTime;

	/** The current frames adjustment/deviation, from ExpectedFrameTime */
	float FrameAdjustment;
};

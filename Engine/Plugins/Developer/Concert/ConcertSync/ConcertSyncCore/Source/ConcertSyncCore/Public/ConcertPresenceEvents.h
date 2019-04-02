// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertPresenceEvents.generated.h"

USTRUCT()
struct FConcertClientPresenceEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 TransactionUpdateIndex;
};

USTRUCT()
struct FConcertClientPresenceVisibilityUpdateEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ModifiedEndpointId;

	UPROPERTY()
	bool bVisibility;
};

USTRUCT()
struct FConcertClientPresenceInVREvent
{
	GENERATED_BODY()

	UPROPERTY()
	FName VRDevice;
};

USTRUCT()
struct FConcertClientPresenceDataUpdateEvent : public FConcertClientPresenceEventBase
{
	GENERATED_BODY()

	FConcertClientPresenceDataUpdateEvent()
		: WorldPath()
		, Position(FVector::ZeroVector)
		, Orientation(FQuat::Identity)
	{
	}

	UPROPERTY()
	FName WorldPath;

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FQuat Orientation;
};

USTRUCT()
struct FConcertClientDesktopPresenceUpdateEvent : public FConcertClientPresenceEventBase
{
	GENERATED_BODY()

	FConcertClientDesktopPresenceUpdateEvent()
		: TraceStart(FVector::ZeroVector)
		, TraceEnd(FVector::ZeroVector)
		, bMovingCamera(false)
	{
	}

	UPROPERTY()
	FVector TraceStart;

	UPROPERTY()
	FVector TraceEnd;

	UPROPERTY()
	bool bMovingCamera;
};

USTRUCT()
struct FConcertClientVRPresenceUpdateEvent : public FConcertClientPresenceEventBase
{
	GENERATED_BODY()

	FConcertClientVRPresenceUpdateEvent()
		: LeftMotionControllerPosition(FVector::ZeroVector)
		, LeftMotionControllerOrientation(FQuat::Identity)
		, RightMotionControllerPosition(FVector::ZeroVector)
		, RightMotionControllerOrientation(FQuat::Identity)
		, LaserStart(FVector::ZeroVector)
		, LaserEnd(FVector::ZeroVector)
		, bHasLaser(false)
	{
	}

	UPROPERTY()
	FVector LeftMotionControllerPosition;

	UPROPERTY()
	FQuat LeftMotionControllerOrientation;

	UPROPERTY()
	FVector RightMotionControllerPosition;

	UPROPERTY()
	FQuat RightMotionControllerOrientation;

	UPROPERTY()
	FVector LaserStart;

	UPROPERTY()
	FVector LaserEnd;

	UPROPERTY()
	bool bHasLaser;
};



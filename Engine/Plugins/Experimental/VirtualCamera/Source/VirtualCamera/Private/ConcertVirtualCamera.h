// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraComponent.h"
#include "VirtualCameraPlayerControllerBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "ConcertVirtualCamera.generated.h"

struct FConcertSessionContext;
class IConcertClientSession;
class UCineCameraComponent;

/**
 * Can't use FCameraFocusSettings since it use a reference to an actor
 * The camera will always be in Manual, and will transfer the CurrentFocusDistance.
 */
USTRUCT()
struct FConcertVirtualCameraCameraFocusData
{
public:
	GENERATED_BODY()

	FConcertVirtualCameraCameraFocusData();
	FConcertVirtualCameraCameraFocusData(const UCineCameraComponent* InCineCameraComponent);

	FCameraFocusSettings ToCameraFocusSettings() const;

	UPROPERTY()
	float ManualFocusDistance;
	UPROPERTY()
	float FocusSmoothingInterpSpeed;
	UPROPERTY()
	uint8 bSmoothFocusChanges : 1;
};


/**
 *
 */
USTRUCT()
struct FConcertVirtualCameraCameraEvent
{
public:
	GENERATED_BODY()

	FConcertVirtualCameraCameraEvent();

	/** Controller settings */
	UPROPERTY()
	ETrackerInputSource InputSource;

	/** Camera transform */
	UPROPERTY()
	FVector CameraActorLocation;
	UPROPERTY()
	FRotator CameraActorRotation;
	UPROPERTY()
	FVector CameraComponentLocation;
	UPROPERTY()
	FRotator CameraComponentRotation;

	/** Camera settings */
	UPROPERTY()
	float CurrentAperture;
	UPROPERTY()
	float CurrentFocalLength; 
	UPROPERTY()
	FConcertVirtualCameraCameraFocusData FocusSettings;
	UPROPERTY()
	FCameraLensSettings LensSettings;
	UPROPERTY()
	FCameraFilmbackSettings FilmbackSettings;
};



#if VIRTUALCAMERA_WITH_CONCERT // Concert is only available in development mode

/**
 *
 */
class FConcertVirtualCameraManager
{
public:
	FConcertVirtualCameraManager();
	FConcertVirtualCameraManager(const FConcertVirtualCameraManager&) = delete;
	FConcertVirtualCameraManager& operator=(const FConcertVirtualCameraManager&) = delete;
	~FConcertVirtualCameraManager();

public:
	bool GetLatestCameraEventData(FConcertVirtualCameraCameraEvent& OutCameraEvent) const;
	void SendCameraEventData(const FConcertVirtualCameraCameraEvent& InCameraEvent);

private:
	void RegisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession);
	void UnregisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession);

	void HandleCameraEventData(const FConcertSessionContext& InEventContext, const FConcertVirtualCameraCameraEvent& InEvent);

private:
	/** Latest event data */
	FConcertVirtualCameraCameraEvent LatestCameraEventData;
	bool bIsLatestCameraEventDataValid;

	/** Delegate handle for a the callback when a session starts up */
	FDelegateHandle OnSessionStartupHandle;

	/** Delegate handle for a the callback when a session shuts down */
	FDelegateHandle OnSessionShutdownHandle;

	/** Weak pointer to the client session with which to send events. May be null or stale. */
	TWeakPtr<IConcertClientSession> WeakSession;
};

#else //VIRTUALCAMERA_WITH_CONCERT

class FConcertVirtualCameraManager
{
public:
	bool GetLatestCameraEventData(FConcertVirtualCameraCameraEvent& OutCameraEvent) const;
	void SendCameraEventData(const FConcertVirtualCameraCameraEvent& InCameraEvent);
};

#endif //VIRTUALCAMERA_WITH_CONCERT

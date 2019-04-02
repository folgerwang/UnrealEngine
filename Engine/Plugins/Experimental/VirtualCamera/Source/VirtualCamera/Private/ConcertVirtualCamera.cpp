// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertVirtualCamera.h"

#if VIRTUALCAMERA_WITH_CONCERT
#include "IConcertClient.h"
#include "IConcertModule.h"
#include "IConcertSession.h"
#endif


/**
 *
 */
FConcertVirtualCameraCameraFocusData::FConcertVirtualCameraCameraFocusData()
	: ManualFocusDistance(100000.f)
	, FocusSmoothingInterpSpeed(8.f)
	, bSmoothFocusChanges(false)
{}


FConcertVirtualCameraCameraFocusData::FConcertVirtualCameraCameraFocusData(const UCineCameraComponent* CineCamera)
	: ManualFocusDistance(CineCamera->CurrentFocusDistance)
	, FocusSmoothingInterpSpeed(CineCamera->FocusSettings.FocusSmoothingInterpSpeed)
	, bSmoothFocusChanges(CineCamera->FocusSettings.bSmoothFocusChanges)
{}


FCameraFocusSettings FConcertVirtualCameraCameraFocusData::ToCameraFocusSettings() const
{
	FCameraFocusSettings Result;
	Result.FocusMethod = ECameraFocusMethod::Manual;
	Result.ManualFocusDistance = ManualFocusDistance;
	Result.bSmoothFocusChanges = bSmoothFocusChanges;
	Result.FocusSmoothingInterpSpeed = FocusSmoothingInterpSpeed;
	Result.FocusOffset = 0.f;
	return Result;
}


/**
 *
 */
FConcertVirtualCameraCameraEvent::FConcertVirtualCameraCameraEvent()
	: InputSource(ETrackerInputSource::ARKit)
	, CameraActorLocation(FVector::ZeroVector)
	, CameraActorRotation(FRotator::ZeroRotator)
	, CameraComponentLocation(FVector::ZeroVector)
	, CameraComponentRotation(FRotator::ZeroRotator)
	, CurrentAperture(0.f)
	, CurrentFocalLength(0.f)
{}


#if VIRTUALCAMERA_WITH_CONCERT

/**
 *
 */
FConcertVirtualCameraManager::FConcertVirtualCameraManager()
	: bIsLatestCameraEventDataValid(false)
{
	IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
	if (ConcertClient.IsValid())
	{
		OnSessionStartupHandle = ConcertClient->OnSessionStartup().AddRaw(this, &FConcertVirtualCameraManager::RegisterConcertSyncHandlers);
		OnSessionShutdownHandle = ConcertClient->OnSessionShutdown().AddRaw(this, &FConcertVirtualCameraManager::UnregisterConcertSyncHandlers);

		TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
		if (ConcertClientSession.IsValid())
		{
			RegisterConcertSyncHandlers(ConcertClientSession.ToSharedRef());
		}
	}
}


FConcertVirtualCameraManager::~FConcertVirtualCameraManager()
{
	IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
	if (ConcertClient.IsValid())
	{
		TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
		if (ConcertClientSession.IsValid())
		{
			UnregisterConcertSyncHandlers(ConcertClientSession.ToSharedRef());
		}

		ConcertClient->OnSessionStartup().Remove(OnSessionStartupHandle);
		OnSessionStartupHandle.Reset();

		ConcertClient->OnSessionShutdown().Remove(OnSessionShutdownHandle);
		OnSessionShutdownHandle.Reset();
	}
}


bool FConcertVirtualCameraManager::GetLatestCameraEventData(FConcertVirtualCameraCameraEvent& OutCameraEvent) const
{
	OutCameraEvent = LatestCameraEventData;
	return bIsLatestCameraEventDataValid;
}


void FConcertVirtualCameraManager::SendCameraEventData(const FConcertVirtualCameraCameraEvent& InCameraEvent)
{
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		TArray<FGuid> ClientIds = Session->GetSessionClientEndpointIds();
		Session->SendCustomEvent(InCameraEvent, ClientIds, EConcertMessageFlags::None);
	}
}


void FConcertVirtualCameraManager::RegisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession)
{
	// Hold onto the session so we can trigger events
	WeakSession = InSession;

	// Register our events
	InSession->RegisterCustomEventHandler<FConcertVirtualCameraCameraEvent>(this, &FConcertVirtualCameraManager::HandleCameraEventData);
}


void FConcertVirtualCameraManager::UnregisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession)
{
	// Unregister our events and explicitly reset the session ptr
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		Session->UnregisterCustomEventHandler<FConcertVirtualCameraCameraEvent>();
	}

	WeakSession.Reset();
}


void FConcertVirtualCameraManager::HandleCameraEventData(const FConcertSessionContext& InEventContext, const FConcertVirtualCameraCameraEvent& InEvent)
{
	LatestCameraEventData = InEvent;
	bIsLatestCameraEventDataValid = true;
}


#else //#if VIRTUALCAMERA_WITH_CONCERT


bool FConcertVirtualCameraManager::GetLatestCameraEventData(FConcertVirtualCameraCameraEvent& OutCameraEvent) const
{
	return false;
}


void FConcertVirtualCameraManager::SendCameraEventData(const FConcertVirtualCameraCameraEvent& InCameraEvent)
{

}


#endif //#if VIRTUALCAMERA_WITH_CONCERT

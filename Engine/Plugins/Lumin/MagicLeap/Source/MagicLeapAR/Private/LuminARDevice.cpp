// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LuminARDevice.h"
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeCounter64.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h" // for FWorldDelegates
#include "Engine/Engine.h"
#include "GeneralProjectSettings.h"
#include "LuminARTrackingSystem.h"

namespace
{
	ELuminARFunctionStatus ToLuminARFunctionStatus(ELuminARAPIStatus Status)
	{
		switch (Status)
		{
		case ELuminARAPIStatus::AR_SUCCESS:
			return ELuminARFunctionStatus::Success;
		default:
			ensureMsgf(false, TEXT("Unknown conversion from ELuminARAPIStatus %d to ELuminARFunctionStatus."), static_cast<int>(Status));
			return ELuminARFunctionStatus::Fatal;
		}
	}
}

FLuminARDevice* FLuminARDevice::GetInstance()
{
	static FLuminARDevice Inst;
	return &Inst;
}

FLuminARDevice::FLuminARDevice()
	: bIsLuminARSessionRunning(false)
	, bForceLateUpdateEnabled(false)
	, bSessionConfigChanged(false)
	, bStartSessionRequested(false)
	, bShouldSessionRestart(false)
	, WorldToMeterScale(100.0f)
	, CurrentSessionStatus(EARSessionStatus::NotStarted)
{
}


bool FLuminARDevice::GetIsTrackingTypeSupported(EARSessionType SessionType)
{
	if (SessionType == EARSessionType::World)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void FLuminARDevice::OnModuleLoaded()
{
	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FLuminARDevice::OnWorldTickStart);
}

void FLuminARDevice::OnModuleUnloaded()
{
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
	// clear the unique ptr.
	LuminARSession.Reset();
}

bool FLuminARDevice::GetIsLuminARSessionRunning()
{
	return bIsLuminARSessionRunning;
}

EARSessionStatus FLuminARDevice::GetSessionStatus()
{
	return CurrentSessionStatus;
}

float FLuminARDevice::GetWorldToMetersScale()
{
	return WorldToMeterScale;
}

// This function will be called by public function to start Lumin AR session request.
void FLuminARDevice::StartLuminARSessionRequest(UARSessionConfig* SessionConfig)
{
	UE_LOG(LogLuminAR, Log, TEXT("Start LuminAR session requested"));

	if (bIsLuminARSessionRunning)
	{
		if (SessionConfig == AccessSessionConfig())
		{
			UE_LOG(LogLuminAR, Warning, TEXT("LuminAR session is already running with the requested LuminAR config. Request aborted."));
			bStartSessionRequested = false;
			return;
		}

		PauseLuminARSession();
	}

	if (bStartSessionRequested)
	{
		UE_LOG(LogLuminAR, Warning, TEXT("LuminAR session is already starting. This will overriding the previous session config with the new one."))
	}

	bStartSessionRequested = true;

	// Try recreating the LuminARSession to fix the fatal error.
	if (CurrentSessionStatus == EARSessionStatus::FatalError)
	{
		UE_LOG(LogLuminAR, Warning, TEXT("Reset LuminAR session due to fatal error detected."));
		ResetLuminARSession();
	}
}

bool FLuminARDevice::GetStartSessionRequestFinished()
{
	return !bStartSessionRequested;
}

// Note that this function will only be registered when LuminAR is supported.
void FLuminARDevice::OnWorldTickStart(ELevelTick TickType, float DeltaTime)
{
	WorldToMeterScale = GWorld->GetWorldSettings()->WorldToMeters;
	TFunction<void()> Func;
	while (RunOnGameThreadQueue.Dequeue(Func))
	{
		Func();
	}

	if (!bIsLuminARSessionRunning && bStartSessionRequested)
	{
		StartSessionWithRequestedConfig();
	}

	if (bIsLuminARSessionRunning)
	{
		ELuminARAPIStatus Status = LuminARSession->Update(WorldToMeterScale);
		if (Status == ELuminARAPIStatus::AR_ERROR_FATAL)
		{
			LuminARSession->Pause();
			bIsLuminARSessionRunning = false;
			CurrentSessionStatus = EARSessionStatus::FatalError;
		}
		else
		{
		}
	}
}

void FLuminARDevice::HandleRuntimePermissionsGranted(const TArray<FString>& RuntimePermissions, const TArray<bool>& Granted)
{
	bool bGranted = true;
	for (int32 i = 0; i < RuntimePermissions.Num(); i++)
	{
		if (!Granted[i])
		{
			bGranted = false;
			UE_LOG(LogLuminAR, Warning, TEXT("Android runtime permission denied: %s"), *RuntimePermissions[i]);
		}
		else
		{
			UE_LOG(LogLuminAR, Log, TEXT("Android runtime permission granted: %s"), *RuntimePermissions[i]);
		}
	}
}

void FLuminARDevice::StartSessionWithRequestedConfig()
{
	bStartSessionRequested = false;

	if (!LuminARSession.IsValid())
	{
		LuminARSession = FLuminARSession::CreateLuminARSession();
		/*ELuminARAPIStatus SessionCreateStatus = LuminARSession->GetSessionCreateStatus();
		if (SessionCreateStatus != ELuminARAPIStatus::AR_SUCCESS)
		{
			ensureMsgf(false, TEXT("Failed to create LuminAR session with error status: %d"), (int)SessionCreateStatus);
			if (SessionCreateStatus != ELuminARAPIStatus::AR_ERROR_FATAL)
			{
				CurrentSessionStatus = EARSessionStatus::NotSupported;
			}
			else
			{
				CurrentSessionStatus = EARSessionStatus::FatalError;
			}
			LuminARSession.Reset();
			return;
		}
		*/
		LuminARSession->SetARSystem(ARSystem.ToSharedRef());
	}

	StartSession();
}

void FLuminARDevice::StartSession()
{
	UARSessionConfig* RequestedConfig = AccessSessionConfig();

	if (RequestedConfig->GetSessionType() != EARSessionType::World)
	{
		UE_LOG(LogLuminAR, Warning, TEXT("Start AR failed: Unsupported AR tracking type %d for LuminAR"), static_cast<int>(RequestedConfig->GetSessionType()));
		CurrentSessionStatus = EARSessionStatus::UnsupportedConfiguration;
		return;
	}

	ELuminARAPIStatus Status = ELuminARAPIStatus::AR_SUCCESS;
	/*
	ELuminARAPIStatus Status = LuminARSession->ConfigSession(*RequestedConfig);

	if (Status != ELuminARAPIStatus::AR_SUCCESS)
	{
	UE_LOG(LogLuminAR, Error, TEXT("LuminAR Session start failed with error status %d"), static_cast<int>(Status));
	CurrentSessionStatus = EARSessionStatus::UnsupportedConfiguration;
	return;
	}
	*/

	Status = LuminARSession->Resume();

	if (Status != ELuminARAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogLuminAR, Error, TEXT("LuminAR Session start failed with error status %d"), static_cast<int>(Status));
		check(Status == ELuminARAPIStatus::AR_ERROR_FATAL);
		// If we failed here, the only reason would be fatal error.
		CurrentSessionStatus = EARSessionStatus::FatalError;
		return;
	}
	/*
	if (GEngine->XRSystem.IsValid())
	{
		FMagicLeapHMD* MagicLeapHMD = static_cast<FMagicLeapHMD*>(GEngine->XRSystem.Get());
		if (MagicLeapHMD && LuminARImplementation.IsValid())
		{
			const bool bMatchFOV = RequestedConfig->ShouldRenderCameraOverlay();
			LuminARImplementation->ConfigLuminARCamera(bMatchFOV, RequestedConfig->ShouldRenderCameraOverlay());
		}
		else
		{
			UE_LOG(LogLuminAR, Error, TEXT("ERROR: LuminARImplementation is not available."));
		}
	}
	*/

	bIsLuminARSessionRunning = true;
	CurrentSessionStatus = EARSessionStatus::Running;

	ARSystem->OnARSessionStarted.Broadcast();
}

void* FLuminARDevice::GetARSessionRawPointer()
{
#if PLATFORM_LUMIN
	check(false); // never used and unsupported
	return nullptr;
#endif
	return nullptr;
}

void* FLuminARDevice::GetGameThreadARFrameRawPointer()
{
#if PLATFORM_LUMIN
	return reinterpret_cast<void*>(LuminARSession->GetLatestFrameRawPointer());
#endif
	return nullptr;
}

TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> FLuminARDevice::GetARSystem()
{
	return ARSystem;
}


void FLuminARDevice::SetARSystem(TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> InARSystem)
{
	check(InARSystem.IsValid());
	ARSystem = InARSystem;
}

TSharedPtr<FLuminARImplementation, ESPMode::ThreadSafe> FLuminARDevice::GetLuminARImplementation()
{
	return LuminARImplementation;
}

void FLuminARDevice::SetLuminARImplementation(TSharedPtr<FLuminARImplementation, ESPMode::ThreadSafe> InARImplementation)
{
	LuminARImplementation = InARImplementation;
}

void FLuminARDevice::PauseLuminARSession()
{
	UE_LOG(LogLuminAR, Log, TEXT("Pausing LuminAR session."));
	if (!bIsLuminARSessionRunning)
	{
		if(bStartSessionRequested)
		{
			bStartSessionRequested = false;
		}
		else
		{
			UE_LOG(LogLuminAR, Log, TEXT("Could not stop LuminAR tracking session because there is no running tracking session!"));
		}
		return;
	}

	ELuminARAPIStatus Status = LuminARSession->Pause();

	if (Status == ELuminARAPIStatus::AR_ERROR_FATAL)
	{
		CurrentSessionStatus = EARSessionStatus::FatalError;
	}

	bIsLuminARSessionRunning = false;
	CurrentSessionStatus = EARSessionStatus::NotStarted;
}

void FLuminARDevice::ResetLuminARSession()
{
	LuminARSession.Reset();
	CurrentSessionStatus = EARSessionStatus::NotStarted;
}


FMatrix FLuminARDevice::GetPassthroughCameraProjectionMatrix(FIntPoint ViewRectSize) const
{
	if (!bIsLuminARSessionRunning)
	{
		return FMatrix::Identity;
	}
	return LuminARSession->GetLatestFrame()->GetProjectionMatrix();
}

void FLuminARDevice::GetPassthroughCameraImageUVs(const TArray<float>& InUvs, TArray<float>& OutUVs) const
{
	if (!bIsLuminARSessionRunning)
	{
		return;
	}
	LuminARSession->GetLatestFrame()->TransformDisplayUvCoords(InUvs, OutUVs);
}

ELuminARTrackingState FLuminARDevice::GetTrackingState() const
{
	if (!bIsLuminARSessionRunning)
	{
		return ELuminARTrackingState::StoppedTracking;
	}
	return LuminARSession->GetLatestFrame()->GetCameraTrackingState();
}

//FTransform FLuminARDevice::GetLatestPose() const
//{
//	if (!bIsLuminARSessionRunning)
//	{
//		return FTransform::Identity;
//	}
//	return LuminARSession->GetLatestFrame()->GetCameraPose();
//}

FLuminARLightEstimate FLuminARDevice::GetLatestLightEstimate() const
{
	if (!bIsLuminARSessionRunning)
	{
		return FLuminARLightEstimate();
	}

	return LuminARSession->GetLatestFrame()->GetLightEstimate();
}

void FLuminARDevice::ARLineTrace(const FVector2D& ScreenPosition, ELuminARLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	if (!bIsLuminARSessionRunning)
	{
		return;
	}
	OutHitResults.Empty();
	LuminARSession->GetLatestFrame()->ARLineTrace(ScreenPosition, TraceChannels, OutHitResults);
}

void FLuminARDevice::ARLineTrace(const FVector& Start, const FVector& End, ELuminARLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	if (!bIsLuminARSessionRunning)
	{
		return;
	}
	OutHitResults.Empty();
	LuminARSession->GetLatestFrame()->ARLineTrace(Start, End, TraceChannels, OutHitResults);
}

ELuminARFunctionStatus FLuminARDevice::CreateARPin(const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, USceneComponent* ComponentToPin, const FName DebugName, UARPin*& OutARAnchorObject)
{
	if (!bIsLuminARSessionRunning)
	{
		return ELuminARFunctionStatus::SessionPaused;
	}

	const FTransform& TrackingToAlignedTracking = ARSystem->GetAlignmentTransform();
	const FTransform PinToTrackingTransform = PinToWorldTransform.GetRelativeTransform(ARSystem->GetXRTrackingSystem()->GetTrackingToWorldTransform()).GetRelativeTransform(TrackingToAlignedTracking);

	ELuminARFunctionStatus Status = ToLuminARFunctionStatus(LuminARSession->CreateARAnchor(PinToTrackingTransform, TrackedGeometry, ComponentToPin, DebugName, OutARAnchorObject));

	return Status;
}

void FLuminARDevice::RemoveARPin(UARPin* ARAnchorObject)
{
	if (!bIsLuminARSessionRunning)
	{
		return;
	}

	LuminARSession->DetachAnchor(ARAnchorObject);
}

void FLuminARDevice::GetAllARPins(TArray<UARPin*>& LuminARAnchorList)
{
	if (!bIsLuminARSessionRunning)
	{
		return;
	}
	LuminARSession->GetAllAnchors(LuminARAnchorList);
}

UARSessionConfig* FLuminARDevice::AccessSessionConfig() const
{
	return (ARSystem.IsValid())
		? &ARSystem->AccessSessionConfig()
		: nullptr;
}

ELuminARFunctionStatus FLuminARDevice::AcquireCameraImage(ULuminARCameraImage *&OutLatestCameraImage)
{
	if (!bIsLuminARSessionRunning)
	{
		return ELuminARFunctionStatus::SessionPaused;
	}

	return ToLuminARFunctionStatus(LuminARSession->AcquireCameraImage(OutLatestCameraImage));
}

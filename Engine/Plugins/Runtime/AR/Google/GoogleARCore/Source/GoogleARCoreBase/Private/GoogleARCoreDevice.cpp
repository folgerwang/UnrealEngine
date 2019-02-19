// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreDevice.h"
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeCounter64.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h" // for FWorldDelegates
#include "Engine/Engine.h"
#include "GeneralProjectSettings.h"
#include "GoogleARCoreXRTrackingSystem.h"
#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreBaseLogCategory.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#endif

#include "GoogleARCorePermissionHandler.h"

namespace
{
	EGoogleARCoreFunctionStatus ToARCoreFunctionStatus(EGoogleARCoreAPIStatus Status)
	{
		switch (Status)
		{
		case EGoogleARCoreAPIStatus::AR_SUCCESS:
			return EGoogleARCoreFunctionStatus::Success;
		case EGoogleARCoreAPIStatus::AR_ERROR_NOT_TRACKING:
			return EGoogleARCoreFunctionStatus::NotTracking;
		case EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED:
			return EGoogleARCoreFunctionStatus::SessionPaused;
		case EGoogleARCoreAPIStatus::AR_ERROR_RESOURCE_EXHAUSTED:
			return EGoogleARCoreFunctionStatus::ResourceExhausted;
		case EGoogleARCoreAPIStatus::AR_ERROR_NOT_YET_AVAILABLE:
			return EGoogleARCoreFunctionStatus::NotAvailable;
		case EGoogleARCoreAPIStatus::AR_ERROR_ILLEGAL_STATE:
			return EGoogleARCoreFunctionStatus::IllegalState;
		default:
			ensureMsgf(false, TEXT("Unknown conversion from EGoogleARCoreAPIStatus %d to EGoogleARCoreFunctionStatus."), static_cast<int>(Status));
			return EGoogleARCoreFunctionStatus::Unknown;
		}
	}
}

FGoogleARCoreDelegates::FGoogleARCoreOnConfigCameraDelegate FGoogleARCoreDelegates::OnCameraConfig;

FGoogleARCoreDevice* FGoogleARCoreDevice::GetInstance()
{
	static FGoogleARCoreDevice Inst;
	return &Inst;
}

FGoogleARCoreDevice::FGoogleARCoreDevice()
	: PassthroughCameraTexture(nullptr)
	, PassthroughCameraTextureId(-1)
	, bIsARCoreSessionRunning(false)
	, bForceLateUpdateEnabled(false)
	, bSessionConfigChanged(false)
	, bAndroidRuntimePermissionsRequested(false)
	, bAndroidRuntimePermissionsGranted(false)
	, bPermissionDeniedByUser(false)
	, bStartSessionRequested(false)
	, bShouldSessionRestart(false)
	, bARCoreInstallRequested(false)
	, bARCoreInstalled(false)
	, WorldToMeterScale(100.0f)
	, PermissionHandler(nullptr)
	, bDisplayOrientationChanged(false)
	, CurrentSessionStatus(EARSessionStatus::NotStarted, TEXT("ARCore Session is uninitialized."))
{
}

EGoogleARCoreAvailability FGoogleARCoreDevice::CheckARCoreAPKAvailability()
{
	return FGoogleARCoreAPKManager::CheckARCoreAPKAvailability();
}

EGoogleARCoreAPIStatus FGoogleARCoreDevice::RequestInstall(bool bUserRequestedInstall, EGoogleARCoreInstallStatus& OutInstallStatus)
{
	return FGoogleARCoreAPKManager::RequestInstall(bUserRequestedInstall, OutInstallStatus);
}

bool FGoogleARCoreDevice::GetIsTrackingTypeSupported(EARSessionType SessionType)
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

void FGoogleARCoreDevice::OnModuleLoaded()
{
	// Init display orientation.
	OnDisplayOrientationChanged();

	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FGoogleARCoreDevice::OnWorldTickStart);
}

void FGoogleARCoreDevice::OnModuleUnloaded()
{
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
	// clear the unique ptr.
	ARCoreSession.Reset();
}

bool FGoogleARCoreDevice::GetIsARCoreSessionRunning()
{
	return bIsARCoreSessionRunning;
}

FARSessionStatus FGoogleARCoreDevice::GetSessionStatus()
{
	return CurrentSessionStatus;
}

float FGoogleARCoreDevice::GetWorldToMetersScale()
{
	return WorldToMeterScale;
}

// This function will be called by public function to start AR core session request.
void FGoogleARCoreDevice::StartARCoreSessionRequest(UARSessionConfig* SessionConfig)
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Start ARCore session requested."));

	if (bIsARCoreSessionRunning)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session is already running, set it to use the new session config."));
		EGoogleARCoreAPIStatus Status = ARCoreSession->ConfigSession(*SessionConfig);
		ensureMsgf(Status == EGoogleARCoreAPIStatus::AR_SUCCESS, TEXT("Failed to set ARCore session to new configuration while it is running."));

		return;
	}

	if (bStartSessionRequested)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("ARCore session is already starting. This will overriding the previous session config with the new one."))
	}

	bStartSessionRequested = true;
	// Re-request permission if necessary
	bPermissionDeniedByUser = false;
	bARCoreInstallRequested = false;

	// Try recreating the ARCoreSession to fix the fatal error.
	if (CurrentSessionStatus.Status == EARSessionStatus::FatalError)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Reset ARCore session due to fatal error detected."));
		ResetARCoreSession();
	}
}

bool FGoogleARCoreDevice::SetARCameraConfig(FGoogleARCoreCameraConfig CameraConfig)
{
	EGoogleARCoreAPIStatus APIStatus = ARCoreSession->SetCameraConfig(CameraConfig);
	if (APIStatus == EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		return true;
	}
	else if (APIStatus == EGoogleARCoreAPIStatus::AR_ERROR_SESSION_NOT_PAUSED)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to set ARCamera configuration due to the arcore session isn't paused."));
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to set ARCamera configuration with provided CameraConfig."));
	}

	UE_LOG(LogGoogleARCore, Error, TEXT("You should only call the ConfigARCoreCamera function when the OnConfigCamera delegate get called, and the provided CameraConfig must be from the array that is passed by the delegate."));
	return false;
}

bool FGoogleARCoreDevice::GetARCameraConfig(FGoogleARCoreCameraConfig& OutCurrentCameraConfig)
{
	if (ARCoreSession.IsValid())
	{
		ARCoreSession->GetARCameraConfig(OutCurrentCameraConfig);
		return true;
	}
	else
	{
		return false;
	}
}

int FGoogleARCoreDevice::AddRuntimeAugmentedImage(UGoogleARCoreAugmentedImageDatabase* TargetImageDatabase, const TArray<uint8>& ImageGrayscalePixels,
	int ImageWidth, int ImageHeight, FString ImageName, float ImageWidthInMeter)
{
	if (!ARCoreSession.IsValid())
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Failed to add runtime augmented image: No valid session!"));
		return -1;
	}

	return ARCoreSession->AddRuntimeAugmentedImage(TargetImageDatabase, ImageGrayscalePixels, ImageWidth, ImageHeight, ImageName, ImageWidthInMeter);
}

bool FGoogleARCoreDevice::AddRuntimeCandidateImage(UARSessionConfig* SessionConfig, const TArray<uint8>& ImageGrayscalePixels, int ImageWidth, int ImageHeight,
	FString FriendlyName, float PhysicalWidth)
{
	if (!ARCoreSession.IsValid())
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Failed to add runtime candidate image: No valid session!"));
		return false;
	}

	return ARCoreSession->AddRuntimeCandidateImage(SessionConfig, ImageGrayscalePixels, ImageWidth, ImageHeight, FriendlyName, PhysicalWidth);
}

bool FGoogleARCoreDevice::GetStartSessionRequestFinished()
{
	return !bStartSessionRequested;
}

// Note that this function will only be registered when ARCore is supported.
void FGoogleARCoreDevice::OnWorldTickStart(ELevelTick TickType, float DeltaTime)
{
	WorldToMeterScale = GWorld->GetWorldSettings()->WorldToMeters;
	TFunction<void()> Func;
	while (RunOnGameThreadQueue.Dequeue(Func))
	{
		Func();
	}

	if (!bIsARCoreSessionRunning && bStartSessionRequested)
	{
		if (!bARCoreInstalled)
		{
			EGoogleARCoreInstallStatus InstallStatus = EGoogleARCoreInstallStatus::Installed;
			EGoogleARCoreAPIStatus Status = FGoogleARCoreAPKManager::RequestInstall(!bARCoreInstallRequested, InstallStatus);

			if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
			{
				bStartSessionRequested = false;
				CurrentSessionStatus.Status = EARSessionStatus::NotSupported;
				CurrentSessionStatus.AdditionalInfo = TEXT("ARCore APK installation failed on this device.");
			}
			else if (InstallStatus == EGoogleARCoreInstallStatus::Installed)
			{
				bARCoreInstalled = true;
			}
			else
			{
				bARCoreInstallRequested = true;
			}
		}

		else if (bPermissionDeniedByUser)
		{
			CurrentSessionStatus.Status = EARSessionStatus::PermissionNotGranted;
			CurrentSessionStatus.AdditionalInfo = TEXT("Camera permission has been denied by the user.");
			bStartSessionRequested = false;
		}
		else
		{
			CheckAndRequrestPermission(*AccessSessionConfig());
			// Either we don't need to request permission or the permission request is done.
			// Queue the session start task on UiThread
			if (!bAndroidRuntimePermissionsRequested)
			{
				StartSessionWithRequestedConfig();
			}
		}
	}

	if (bIsARCoreSessionRunning)
	{
		// Update ARFrame
		FVector2D ViewportSize(1, 1);
		if (GEngine && GEngine->GameViewport)
		{
			ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		}
		ARCoreSession->SetDisplayGeometry(static_cast<int>(FGoogleARCoreAndroidHelper::GetDisplayRotation()), ViewportSize.X, ViewportSize.Y);
		EGoogleARCoreAPIStatus Status = ARCoreSession->Update(WorldToMeterScale);
		if (Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
		{
			ARCoreSession->Pause();
			bIsARCoreSessionRunning = false;
			CurrentSessionStatus.Status = EARSessionStatus::FatalError;
			CurrentSessionStatus.AdditionalInfo = TEXT("Fatal error occurred when updating ARCore Session. Stopping and restarting ARCore Session may fix the issue.");
		}
		else
		{
			// Copy the camera GPU texture to a normal Texture2D so that we can get around the multi-threaded rendering issue.
			CameraBlitter.DoBlit(PassthroughCameraTextureId, SessionCameraConfig.CameraTextureResolution);
		}
	}
}

void FGoogleARCoreDevice::CheckAndRequrestPermission(const UARSessionConfig& ConfigurationData)
{
	if (!bAndroidRuntimePermissionsRequested)
	{
		TArray<FString> RuntimePermissions;
		TArray<FString> NeededPermissions;
		GetRequiredRuntimePermissionsForConfiguration(ConfigurationData, RuntimePermissions);
		if (RuntimePermissions.Num() > 0)
		{
			for (int32 i = 0; i < RuntimePermissions.Num(); i++)
			{
				if (!UARCoreAndroidPermissionHandler::CheckRuntimePermission(RuntimePermissions[i]))
				{
					NeededPermissions.Add(RuntimePermissions[i]);
				}
			}
		}
		if (NeededPermissions.Num() > 0)
		{
			bAndroidRuntimePermissionsGranted = false;
			bAndroidRuntimePermissionsRequested = true;
			if (PermissionHandler == nullptr)
			{
				PermissionHandler = NewObject<UARCoreAndroidPermissionHandler>();
				PermissionHandler->AddToRoot();
			}
			PermissionHandler->RequestRuntimePermissions(NeededPermissions);
		}
		else
		{
			bAndroidRuntimePermissionsGranted = true;
		}
	}
}

void FGoogleARCoreDevice::HandleRuntimePermissionsGranted(const TArray<FString>& RuntimePermissions, const TArray<bool>& Granted)
{
	bool bGranted = true;
	for (int32 i = 0; i < RuntimePermissions.Num(); i++)
	{
		if (!Granted[i])
		{
			bGranted = false;
			UE_LOG(LogGoogleARCore, Warning, TEXT("Android runtime permission denied: %s"), *RuntimePermissions[i]);
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Android runtime permission granted: %s"), *RuntimePermissions[i]);
		}
	}
	bAndroidRuntimePermissionsRequested = false;
	bAndroidRuntimePermissionsGranted = bGranted;

	if (!bGranted)
	{
		bPermissionDeniedByUser = true;
	}
}

void FGoogleARCoreDevice::StartSessionWithRequestedConfig()
{
	bStartSessionRequested = false;

	// Allocate passthrough camera texture if necessary.
	if (PassthroughCameraTexture == nullptr)
	{
		ENQUEUE_RENDER_COMMAND(UpdateCameraImageUV)(
			[ARCoreDevicePtr = this](FRHICommandListImmediate& RHICmdList)
			{
				ARCoreDevicePtr->AllocatePassthroughCameraTexture_RenderThread();
			}
		);
		FlushRenderingCommands();
	}

	if (!ARCoreSession.IsValid())
	{
		ARCoreSession = FGoogleARCoreSession::CreateARCoreSession();
		EGoogleARCoreAPIStatus SessionCreateStatus = ARCoreSession->GetSessionCreateStatus();
		if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_SUCCESS)
		{
			ensureMsgf(false, TEXT("Failed to create ARCore session with error status: %d"), (int)SessionCreateStatus);
			CurrentSessionStatus.AdditionalInfo =
			    FString::Printf(TEXT("Failed to create ARCore session with error status: %d"), (int)SessionCreateStatus);

			if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
			{
				CurrentSessionStatus.Status = EARSessionStatus::NotSupported;
			}
			else
			{
				CurrentSessionStatus.Status = EARSessionStatus::FatalError;
			}

			ARCoreSession.Reset();
			return;
		}
		ARCoreSession->SetARSystem(ARSystem.ToSharedRef());
	}

	StartSession();
}

void FGoogleARCoreDevice::StartSession()
{
	UARSessionConfig* RequestedConfig = AccessSessionConfig();

	if (RequestedConfig->GetSessionType() != EARSessionType::World)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Start AR failed: Unsupported AR tracking type %d for GoogleARCore"), static_cast<int>(RequestedConfig->GetSessionType()));
		CurrentSessionStatus.AdditionalInfo = TEXT("Unsupported AR tracking type. Only EARSessionType::World is supported by ARCore.");
		CurrentSessionStatus.Status = EARSessionStatus::UnsupportedConfiguration;
		return;
	}

	EGoogleARCoreAPIStatus Status = ARCoreSession->ConfigSession(*RequestedConfig);

	if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ARCore Session start failed with error status %d"), static_cast<int>(Status));
		CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session start failed due to unsupported ARSessionConfig.");
		CurrentSessionStatus.Status = EARSessionStatus::UnsupportedConfiguration;
		return;
	}

	check(PassthroughCameraTextureId != -1);
	ARCoreSession->SetCameraTextureId(PassthroughCameraTextureId);

	FGoogleARCoreDelegates::OnCameraConfig.Broadcast(ARCoreSession->GetSupportedCameraConfig());

	Status = ARCoreSession->Resume();

	if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ARCore Session start failed with error status %d"), static_cast<int>(Status));

		if (Status == EGoogleARCoreAPIStatus::AR_ERROR_ILLEGAL_STATE)
		{
			CurrentSessionStatus.AdditionalInfo = TEXT("Failed to start ARCore Session due to illegal state: All camera images previously acquired must be released before resuming the session with a different camera configuration.");
			CurrentSessionStatus.Status = EARSessionStatus::Other;
		}
		else
		{
			// If we failed here, the only reason would be fatal error.
			CurrentSessionStatus.AdditionalInfo = TEXT("Fatal error occurred when starting ARCore Session. Stopping and restarting ARCore Session may fix the issue.");
			CurrentSessionStatus.Status = EARSessionStatus::FatalError;
		}

		return;
	}

	if (GEngine->XRSystem.IsValid())
	{
		FGoogleARCoreXRTrackingSystem* ARCoreTrackingSystem = static_cast<FGoogleARCoreXRTrackingSystem*>(GEngine->XRSystem.Get());
		if (ARCoreTrackingSystem)
		{
			const bool bMatchFOV = RequestedConfig->ShouldRenderCameraOverlay();
			ARCoreTrackingSystem->ConfigARCoreXRCamera(bMatchFOV, RequestedConfig->ShouldRenderCameraOverlay());
		}
		else
		{
			UE_LOG(LogGoogleARCore, Error, TEXT("ERROR: GoogleARCoreXRTrackingSystem is not available."));
		}
	}

	ARCoreSession->GetARCameraConfig(SessionCameraConfig);

	bIsARCoreSessionRunning = true;
	CurrentSessionStatus.Status = EARSessionStatus::Running;
	CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session is running.");
	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session started successfully."));

	ARSystem->OnARSessionStarted.Broadcast();
}

void FGoogleARCoreDevice::SetARSystem(TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> InARSystem)
{
	check(InARSystem.IsValid());
	ARSystem = InARSystem;
}

void* FGoogleARCoreDevice::GetARSessionRawPointer()
{
#if PLATFORM_ANDROID
	return reinterpret_cast<void*>(ARCoreSession->GetHandle());
#endif
	return nullptr;
}

void* FGoogleARCoreDevice::GetGameThreadARFrameRawPointer()
{
#if PLATFORM_ANDROID
	return reinterpret_cast<void*>(ARCoreSession->GetLatestFrameRawPointer());
#endif
	return nullptr;
}

TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> FGoogleARCoreDevice::GetARSystem()
{
	return ARSystem;
}

void FGoogleARCoreDevice::PauseARCoreSession()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Pausing ARCore session."));
	if (!bIsARCoreSessionRunning)
	{
		if(bStartSessionRequested)
		{
			bStartSessionRequested = false;
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Could not stop ARCore tracking session because there is no running tracking session!"));
		}
		return;
	}

	EGoogleARCoreAPIStatus Status = ARCoreSession->Pause();

	if (Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
	{
		CurrentSessionStatus.Status = EARSessionStatus::FatalError;
		CurrentSessionStatus.AdditionalInfo = TEXT("Fatal error occurred when starting ARCore Session. Stopping and restarting ARCore Session may fix the issue.");
	}
	else
	{
		CurrentSessionStatus.Status = EARSessionStatus::NotStarted;
		CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session is paused.");
	}
	bIsARCoreSessionRunning = false;
	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session paused"));
}

void FGoogleARCoreDevice::ResetARCoreSession()
{
	ARCoreSession.Reset();
	CurrentSessionStatus.Status = EARSessionStatus::NotStarted;
	CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session is uninitialized.");
}

void FGoogleARCoreDevice::AllocatePassthroughCameraTexture_RenderThread()
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRHIResourceCreateInfo CreateInfo;

	PassthroughCameraTexture = RHICmdList.CreateTextureExternal2D(1, 1, PF_R8G8B8A8, 1, 1, 0, CreateInfo);

	void* NativeResource = PassthroughCameraTexture->GetNativeResource();
	check(NativeResource);
	PassthroughCameraTextureId = *reinterpret_cast<uint32*>(NativeResource);
}

FTextureRHIRef FGoogleARCoreDevice::GetPassthroughCameraTexture()
{
	return PassthroughCameraTexture;
}

FMatrix FGoogleARCoreDevice::GetPassthroughCameraProjectionMatrix(FIntPoint ViewRectSize) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return FMatrix::Identity;
	}
	return ARCoreSession->GetLatestFrame()->GetProjectionMatrix();
}

void FGoogleARCoreDevice::GetPassthroughCameraImageUVs(const TArray<float>& InUvs, TArray<float>& OutUVs) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	ARCoreSession->GetLatestFrame()->TransformDisplayUvCoords(InUvs, OutUVs);
}

EGoogleARCoreTrackingState FGoogleARCoreDevice::GetTrackingState() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreTrackingState::StoppedTracking;
	}
	else if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreTrackingState::NotTracking;
	}

	return ARCoreSession->GetLatestFrame()->GetCameraTrackingState();
}

FTransform FGoogleARCoreDevice::GetLatestPose() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return FTransform::Identity;
	}
	return ARCoreSession->GetLatestFrame()->GetCameraPose();
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetPointCloud(OutLatestPointCloud));
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::AcquireLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->AcquirePointCloud(OutLatestPointCloud));
}

#if PLATFORM_ANDROID
EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetLatestCameraMetadata(const ACameraMetadata*& OutCameraMetadata) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraMetadata(OutCameraMetadata));
}
#endif

UTexture* FGoogleARCoreDevice::GetCameraTexture()
{
	return CameraBlitter.GetLastCameraImageTexture();
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::AcquireCameraImage(UGoogleARCoreCameraImage *&OutLatestCameraImage)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->AcquireCameraImage(OutLatestCameraImage));
}

FGoogleARCoreLightEstimate FGoogleARCoreDevice::GetLatestLightEstimate() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return FGoogleARCoreLightEstimate();
	}

	return ARCoreSession->GetLatestFrame()->GetLightEstimate();
}

void FGoogleARCoreDevice::ARLineTrace(const FVector2D& ScreenPosition, EGoogleARCoreLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	OutHitResults.Empty();
	ARCoreSession->GetLatestFrame()->ARLineTrace(ScreenPosition, TraceChannels, OutHitResults);
}

void FGoogleARCoreDevice::ARLineTrace(const FVector& Start, const FVector& End, EGoogleARCoreLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	OutHitResults.Empty();
	ARCoreSession->GetLatestFrame()->ARLineTrace(Start, End, TraceChannels, OutHitResults);
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::CreateARPin(const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, USceneComponent* ComponentToPin, const FName DebugName, UARPin*& OutARAnchorObject)
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	const FTransform& TrackingToAlignedTracking = ARSystem->GetAlignmentTransform();
	const FTransform PinToTrackingTransform = PinToWorldTransform.GetRelativeTransform(ARSystem->GetXRTrackingSystem()->GetTrackingToWorldTransform()).GetRelativeTransform(TrackingToAlignedTracking);

	EGoogleARCoreFunctionStatus Status = ToARCoreFunctionStatus(ARCoreSession->CreateARAnchor(PinToTrackingTransform, TrackedGeometry, ComponentToPin, DebugName, OutARAnchorObject));

	return Status;
}

void FGoogleARCoreDevice::RemoveARPin(UARPin* ARAnchorObject)
{
	if (!ARCoreSession.IsValid())
	{
		return;
	}

	ARCoreSession->DetachAnchor(ARAnchorObject);
}

void FGoogleARCoreDevice::GetAllARPins(TArray<UARPin*>& ARCoreAnchorList)
{
	if (!ARCoreSession.IsValid())
	{
		return;
	}
	ARCoreSession->GetAllAnchors(ARCoreAnchorList);
}

void FGoogleARCoreDevice::GetUpdatedARPins(TArray<UARPin*>& ARCoreAnchorList)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	ARCoreSession->GetLatestFrame()->GetUpdatedAnchors(ARCoreAnchorList);
}

// Functions that are called on Android lifecycle events.
void FGoogleARCoreDevice::OnApplicationCreated()
{
}

void FGoogleARCoreDevice::OnApplicationDestroyed()
{
}

void FGoogleARCoreDevice::OnApplicationPause()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("OnPause Called: %d"), bIsARCoreSessionRunning);
	bShouldSessionRestart = bIsARCoreSessionRunning;
	if (bIsARCoreSessionRunning)
	{
		PauseARCoreSession();
	}
}

void FGoogleARCoreDevice::OnApplicationResume()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("OnResume Called: %d"), bShouldSessionRestart);
	// Try to ask for permission if it is denied by user.
	if (bShouldSessionRestart)
	{
		bShouldSessionRestart = false;
		StartSession();
	}
}

void FGoogleARCoreDevice::OnApplicationStop()
{
}

void FGoogleARCoreDevice::OnApplicationStart()
{
}

// TODO: we probably don't need this.
void FGoogleARCoreDevice::OnDisplayOrientationChanged()
{
	FGoogleARCoreAndroidHelper::UpdateDisplayRotation();
	bDisplayOrientationChanged = true;
}

UARSessionConfig* FGoogleARCoreDevice::AccessSessionConfig() const
{
	return (ARSystem.IsValid())
		? &ARSystem->AccessSessionConfig()
		: nullptr;
}


EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetCameraImageIntrinsics(UGoogleARCoreCameraIntrinsics *&OutCameraIntrinsics)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraImageIntrinsics(OutCameraIntrinsics));
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetCameraTextureIntrinsics(UGoogleARCoreCameraIntrinsics *&OutCameraIntrinsics)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraTextureIntrinsics(OutCameraIntrinsics));
}

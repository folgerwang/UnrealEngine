// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraPlayerControllerBase.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "IXRTrackingSystem.h"
#include "RemoteSession/RemoteSession.h"
#include "RemoteSession/Channels/RemoteSessionInputChannel.h"
#include "RemoteSession/Channels/RemoteSessionFrameBufferChannel.h"
#include "RemoteSession/Channels/RemoteSessionXRTrackingChannel.h"
#include "SteamVRFunctionLibrary.h"

AVirtualCameraPlayerControllerBase::AVirtualCameraPlayerControllerBase(const FObjectInitializer& ObjectInitializer)
{
	// Default tracker input source
	InputSource = ETrackerInputSource::ARKit;

	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	}

	// Default autofocus to center of screen
	AutoFocusScreenPosition = FVector2D(.5f, .5f);

	// Default touch input values
	TouchInputState = ETouchInputState::BlueprintDefined;
	CurrentFocusMethod = EVirtualCameraFocusMethod::Manual;
}

void AVirtualCameraPlayerControllerBase::BeginPlay()
{
	// Make a default sequence playback controller
	LevelSequencePlaybackController = NewObject<ULevelSequencePlaybackController>(this);

	if (LevelSequencePlaybackController)
	{
		// Bind to record enabled state change delegate
		LevelSequencePlaybackController->OnRecordEnabledStateChanged.BindUObject(this, &AVirtualCameraPlayerControllerBase::HandleRecordEnabledStateChange);

		// Bind to stop delegate
		OnStop.BindUFunction(this, FName("OnStopped"));
		LevelSequencePlaybackController->OnStop.Add(OnStop);
	}

	if (IRemoteSessionModule* RemoteSession = FModuleManager::LoadModulePtr<IRemoteSessionModule>("RemoteSession"))
	{
        TMap<FString, ERemoteSessionChannelMode> RequiredChannels;
        RequiredChannels.Add(FRemoteSessionFrameBufferChannel::StaticType(),ERemoteSessionChannelMode::Write);
        RequiredChannels.Add(FRemoteSessionInputChannel::StaticType(),ERemoteSessionChannelMode::Read);
        RequiredChannels.Add(FRemoteSessionXRTrackingChannel::StaticType(),ERemoteSessionChannelMode::Read);
        
        RemoteSession->SetSupportedChannels(RequiredChannels);
		RemoteSession->InitHost();
	}
	
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		// Need to make sure we don't let ARKit control camera completely
		CineCamera->bLockToHmd = false;
		if (LevelSequencePlaybackController)
		{
			LevelSequencePlaybackController->SetCameraComponentToFollow(CineCamera);
		}
	}

	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->OnOffsetReset.AddDynamic(this, &AVirtualCameraPlayerControllerBase::BroadcastOffsetReset);
	}

	// Setup Sequencer Recorder
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->SetupSequenceRecorderSettings(RequiredSequencerRecorderCameraSettings);
	}

	// Initialize the view of the camera with offsets taken into account
	UpdatePawnWithTrackerData();
	ResetCameraOffsetsToTracker();
}

void AVirtualCameraPlayerControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		UpdatePawnWithTrackerData();

		if (VCPawn->IsAutoFocusEnabled())
		{
			// Auto focus is just setting screen focus through the Auto Focus position.
			SetFocusDistanceThroughPoint(AutoFocusScreenPosition);
		}
	}

	UVirtualCameraCineCameraComponent* VCCamera = GetVirtualCameraCineCameraComponent();
	if (VCCamera)
	{
		if (VCCamera->GetCurrentFocusMethod() == EVirtualCameraFocusMethod::Auto)
		{
			SetFocusDistanceThroughPoint(AutoFocusScreenPosition);
		}

		VCCamera->UpdateCameraView();
	}

	if (LevelSequencePlaybackController)
	{
		if (LevelSequencePlaybackController->bIsRecording)
		{
			LevelSequencePlaybackController->PilotTargetedCamera(VCCamera ? &VCCamera->DesiredFilmbackSettings : nullptr);
		}

		if (LevelSequencePlaybackController->GetSequence())
		{
			LevelSequencePlaybackController->Update(DeltaSeconds);
		}
	}
}

void AVirtualCameraPlayerControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindTouch(IE_Released, this, &AVirtualCameraPlayerControllerBase::OnTouchInput);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &AVirtualCameraPlayerControllerBase::OnMoveForward);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &AVirtualCameraPlayerControllerBase::OnMoveRight);
	InputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &AVirtualCameraPlayerControllerBase::OnMoveUp);
}

void AVirtualCameraPlayerControllerBase::InitializeAutoFocusPoint()
{
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(AutoFocusScreenPosition);
		AutoFocusScreenPosition.X *= 0.5f;
		AutoFocusScreenPosition.Y *= 0.5f;
	}
	UpdateFocusReticle(FVector(AutoFocusScreenPosition, 0.f));
}

FString AVirtualCameraPlayerControllerBase::GetDistanceInDesiredUnits(const float InputDistance, const EUnit ConversionUnit) const
{
	// Checks if the specified conversion unit is a unit of distance, since this function assumes conversion from Unreal Units
	if (!FUnitConversion::IsUnitOfType(ConversionUnit, EUnitType::Distance))
	{
		UE_LOG(LogActor, Warning, TEXT("Conversion unit selected is not a unit of distance."))
		return FString();
	}

	// ToDo: Add support for different project settings if default is changed
	float ConvertedDistance = FUnitConversion::Convert<float>(InputDistance, EUnit::Centimeters, ConversionUnit);

	FString ReturnString;

	if (ConversionUnit == EUnit::Feet)
	{
		int32 Feet = FMath::FloorToInt(ConvertedDistance);
		int32 Inches = FMath::RoundToInt(FMath::Frac(ConvertedDistance) * 12.f);

		// Handle when inches rounds to 12
		if (Inches == 12)
		{
			// Feet should increase by one instead of having 12 inches displayed
			Feet++;
			Inches = 0;
		}

		// Use ' and " for feet an inches unit labels
		ReturnString = FString::FromInt(Feet);
		ReturnString += "'";
		
		// Only display inches if there is enough room
		if (Feet < 10000)
		{
			ReturnString += FString::FromInt(Inches);
			ReturnString += "\"";
		}
		
	}
	else
	{
		// Meters
		FNumberFormattingOptions NumberFormat;
		// Only show one degree of precision for decimal values
		NumberFormat.MinimumFractionalDigits = 0;
		NumberFormat.MaximumFractionalDigits = 1;
		ReturnString = FText::AsNumber(ConvertedDistance, &NumberFormat).ToString();
		if (ConvertedDistance >= 1000) 
		{
			// Don't show decimal place if not enough room
			ReturnString = FString::FromInt(FMath::RoundToInt(ConvertedDistance));
		}
		ReturnString += FUnitConversion::GetUnitDisplayString(ConversionUnit);
	}

	return ReturnString;
}

bool AVirtualCameraPlayerControllerBase::GetCurrentTrackerLocationAndRotation(FVector& OutTrackerLocation, FRotator& OutTrackerRotation)
{
	TArray<int32> TrackedDeviceIDs;
	FQuat ARKitQuaternion;

	switch (InputSource)
	{
		case ETrackerInputSource::ARKit:
			if (GEngine && GEngine->XRSystem.IsValid())
			{
				GEngine->XRSystem->GetCurrentPose(0, ARKitQuaternion, OutTrackerLocation);
				OutTrackerRotation = ARKitQuaternion.Rotator();
				return true;
			}
			break;

		case ETrackerInputSource::LiveLink:
			if (LiveLinkClient)
			{
				const FLiveLinkSubjectFrame* CurrentFrame = LiveLinkClient->GetSubjectData(LiveLinkTargetName);
				if (CurrentFrame && CurrentFrame->Transforms.Num() > 0)
				{
					OutTrackerLocation = CurrentFrame->Transforms[0].GetLocation();
					OutTrackerRotation = CurrentFrame->Transforms[0].GetRotation().Rotator();
				}
				return true;
			}
			break;

		case ETrackerInputSource::Vive:
			USteamVRFunctionLibrary::GetValidTrackedDeviceIds(ESteamVRTrackedDeviceType::Other, TrackedDeviceIDs);

			// ToDo: Add filtering in event that there are multiple valid trackers available
			if (TrackedDeviceIDs.Num() > 0)
			{
				USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(TrackedDeviceIDs[0], OutTrackerLocation, OutTrackerRotation);
				return true;
			}
			break;

		case ETrackerInputSource::Custom:
			GetCustomTrackerLocationAndRotation(OutTrackerLocation, OutTrackerRotation);
			return true;

		default:
			UE_LOG(LogActor, Warning, TEXT("Selected tracker source is not yet supported"))
			break;
	}

	// Return failure status if we couldn't find device to track or device isn't supported
	return false;
}

bool AVirtualCameraPlayerControllerBase::IsTouchInputInFocusMode()
{
	return TouchInputState == ETouchInputState::ActorFocusTargeting || TouchInputState == ETouchInputState::AutoFocusTargeting || TouchInputState == ETouchInputState::ManualTouchFocus;
}

void AVirtualCameraPlayerControllerBase::SetFocusDistanceToActor(const ETouchIndex::Type TouchIndex, const FVector& Location)
{
	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();

	// Don't try to execute if not currently possessing a virtual Camera pawn
	if (!VCPawn)
	{
		return;
	}

	FVector TraceDirection;
	FVector CameraWorldLocation;

	if (!DeprojectScreenPositionToWorld(Location.X, Location.Y, CameraWorldLocation, TraceDirection))
	{
		// If projection fails, return now
		return;
	}

	float FocusTraceDist = 1000000.f;

	// Trace to get depth under auto focus position
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(UpdateAutoFocus), true);
	FHitResult Hit;

	FVector const TraceEnd = CameraWorldLocation + TraceDirection * FocusTraceDist;
	bool const bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraWorldLocation, TraceEnd, ECC_Visibility, TraceParams);
	
	if (bHit)
	{
		VCPawn->SetFocusDistance(Hit.Distance);

		// Set this actor as the tracked actor, and move the exact focus tracked point to where the touch occurred
		if (AActor* HitActor = Hit.GetActor())
		{
			FVector TrackingPointOffset = HitActor->GetActorRotation().UnrotateVector(Hit.ImpactPoint - HitActor->GetActorLocation());
			TrackingPointOffset /= HitActor->GetActorScale(); // Adjust for non-standard scales when we rotate the vector

			VCPawn->SetTrackedActorForFocus(HitActor, TrackingPointOffset);
			if (!LevelSequencePlaybackController || !LevelSequencePlaybackController->bIsRecording)
			{	
				VCPawn->TriggerFocusPlaneTimer();
				VCPawn->HighlightTappedActor(HitActor);
			}
		}
	}
}

AVirtualCameraPawnBase* AVirtualCameraPlayerControllerBase::GetVirtualCameraPawn() const 
{
	return Cast<AVirtualCameraPawnBase>(GetPawn());
}

void AVirtualCameraPlayerControllerBase::HandleRecordEnabledStateChange(const bool bIsRecordEnabled)
{
	OnRecordEnabledStateChanged(bIsRecordEnabled);
}

void AVirtualCameraPlayerControllerBase::SetFocusDistanceThroughPoint(const FVector2D ScreenPosition)
{
	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();

	FVector TraceDirection;
	FVector CameraWorldLocation;

	if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, CameraWorldLocation, TraceDirection))
	{
		// If projection fails, return now
		return;
	}

	float FocusTraceDist = 1000000.f;

	// Trace to get depth under auto focus position
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(UpdateAutoFocus), true);
	FHitResult Hit;

	FVector const TraceEnd = CameraWorldLocation + TraceDirection * FocusTraceDist;
	bool const bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraWorldLocation, TraceEnd, ECC_Visibility, TraceParams);

	if (bHit && Hit.GetActor())
	{
		VCPawn->SetFocusDistance(Hit.Distance);
	}	
}

UVirtualCameraCineCameraComponent* AVirtualCameraPlayerControllerBase::GetVirtualCameraCineCameraComponent() const 
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->CineCamera;
	}

	return nullptr;
}

UVirtualCameraMovementComponent* AVirtualCameraPlayerControllerBase::GetVirtualCameraMovementComponent() const 
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->MovementComponent;
	}

	return nullptr;
}

bool AVirtualCameraPlayerControllerBase::InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D & TouchLocation, float Force, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	// Don't allow for input touch events if not within matte
	if (!IsLocationWithinMatte(FVector(TouchLocation, 0.0f)))
	{
		return Super::InputTouch(Handle, Type, TouchLocation, Force, DeviceTimestamp, TouchpadIndex);;
	}

	// Allow touch and drag of the auto focus reticle when in auto focus targeting mode
	if (Type == ETouchType::Moved && TouchInputState == ETouchInputState::AutoFocusTargeting)
	{
		UpdateScreenFocus(ETouchIndex::Touch1, FVector(TouchLocation, 0.f));
	}

	return Super::InputTouch(Handle, Type, TouchLocation, Force, DeviceTimestamp, TouchpadIndex);
}

void AVirtualCameraPlayerControllerBase::OnTouchInput(const ETouchIndex::Type TouchIndex, const FVector Location)
{
	if (!IsLocationWithinMatte(Location))
	{
		return;
	}

	switch (TouchInputState)
	{
		// Mode for attaching focus to an actor.
		case ETouchInputState::ActorFocusTargeting:
			SetFocusDistanceToActor(TouchIndex, Location);
			UpdateScreenFocus(TouchIndex, Location);
			break;

		// Mode for changing the point on the screen used for auto focus targeting.
		case ETouchInputState::AutoFocusTargeting:
			UpdateScreenFocus(TouchIndex, Location);
			ShowFocusPlaneFromTouch();
			break;

		case ETouchInputState::ManualTouchFocus:
			SetFocusDistanceThroughPoint(FVector2D(Location.X, Location.Y));
			UpdateScreenFocus(TouchIndex, Location);
			ShowFocusPlaneFromTouch();
			break;

		// Allows for user defined behavior in blueprint
		case ETouchInputState::BlueprintDefined:
			break;

		default:
			break;
	}
	
}

void AVirtualCameraPlayerControllerBase::UpdateScreenFocus(const ETouchIndex::Type TouchIndex, const FVector Location)
{
	// Set New Screen Location for auto focus to trace through
	AutoFocusScreenPosition.X = Location.X;
	AutoFocusScreenPosition.Y = Location.Y;
	UpdateFocusReticle(Location);
}

void AVirtualCameraPlayerControllerBase::OnMoveForward(const float InValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->OnMoveForward(InValue);
	}
}

void AVirtualCameraPlayerControllerBase::OnMoveRight(const float InValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->OnMoveRight(InValue);
	}
}

void AVirtualCameraPlayerControllerBase::OnMoveUp(const float InValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		// ToDo: Figure out why this needs to be negated; maybe inverted look?
		MovementComponent->OnMoveUp(-InValue);
	}
}

void AVirtualCameraPlayerControllerBase::ShowFocusPlaneFromTouch()
{
	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();

	if (!VCPawn)
	{
		return;
	}

	if (!LevelSequencePlaybackController || !LevelSequencePlaybackController->bIsRecording)
	{
		VCPawn->TriggerFocusPlaneTimer();
	}
}

void AVirtualCameraPlayerControllerBase::UpdatePawnWithTrackerData()
{
	// Initialize the virtual camera view
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		FVector TrackerLocation = FVector::ZeroVector;
		FRotator TrackerRotation = FRotator::ZeroRotator;
		GetCurrentTrackerLocationAndRotation(TrackerLocation, TrackerRotation);

		// Apply tracker offset to tracker; convert everything to transforms to make sure motions are calculated in the right order
		FTransform TrackerRaw = FTransform(TrackerRotation, TrackerLocation);
		FTransform AdjustedTracker = TrackerPostOffset.AsTransform() * TrackerRaw * TrackerPreOffset.AsTransform();
		TrackerRotation = AdjustedTracker.Rotator();
		TrackerLocation = AdjustedTracker.GetLocation();

		VCPawn->ProcessMovementInput(TrackerLocation, TrackerRotation);
	}
}

bool AVirtualCameraPlayerControllerBase::IsLocationWithinMatte(const FVector Location) const 
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		FVector2D ViewportSize; 
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		FVector2D MatteResolution;
		FVector2D LowerBound;
		FVector2D UpperBound;
		
		// Calculate the matte resolution based on viewport and ratio
		MatteResolution.X = ViewportSize.X * CineCamera->ViewSizeRatio.X;
		MatteResolution.Y = ViewportSize.Y * CineCamera->ViewSizeRatio.Y;

		// Lower bound is half the difference between the two resolutions
		LowerBound.X = (ViewportSize.X - MatteResolution.X) / 2;
		LowerBound.Y = (ViewportSize.Y - MatteResolution.Y) / 2;

		// Upper bound is the viewport size minus the lower bound
		UpperBound.X = ViewportSize.X - LowerBound.X;
		UpperBound.Y = ViewportSize.Y - LowerBound.Y;

		// The touch location needs to be within the bounds
		if (Location.X >= LowerBound.X && Location.X <= UpperBound.X && Location.Y >= LowerBound.Y && Location.Y <= UpperBound.Y)
		{
			return true;
		}

	}
	
	return false;
}

/***** UI Interface Functions *****/

void AVirtualCameraPlayerControllerBase::AddBlendableToCamera(TScriptInterface<IBlendableInterface> BlendableToAdd, float Weight)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->AddBlendableToCamera(BlendableToAdd, Weight);
	}
}

float AVirtualCameraPlayerControllerBase::ChangeAperturePreset(const bool bShiftUp)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->ChangeAperturePreset(bShiftUp);
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::ChangeFocalLengthPreset(const bool bShiftUp)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->ChangeFocalLengthPreset(bShiftUp);
	}

	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::ClearActiveLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->ClearActiveLevelSequence();
	}
}

int32 AVirtualCameraPlayerControllerBase::DeletePreset(FString PresetName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->DeletePreset(PresetName);
	}
	return -1;
}

int32 AVirtualCameraPlayerControllerBase::DeleteScreenshot(FString ScreenshotName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->DeleteScreenshot(ScreenshotName);
	}
	return -1;
}

int32 AVirtualCameraPlayerControllerBase::DeleteWaypoint(FString WaypointName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->DeleteWaypoint(WaypointName);
	}
	return -1;
}

FString AVirtualCameraPlayerControllerBase::GetActiveLevelSequenceName()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetActiveLevelSequenceName();
	}

	return FString();
}

float AVirtualCameraPlayerControllerBase::GetAxisStabilizationScale(EVirtualCameraAxis AxisToRetrieve)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->GetAxisStabilizationScale(AxisToRetrieve);
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetAxisMovementScale(EVirtualCameraAxis AxisToRetrieve)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->GetAxisMovementScale(AxisToRetrieve);
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentAperture()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentAperture();
	}

	return 0.0f;
}

FString AVirtualCameraPlayerControllerBase::GetCurrentFilmbackName()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentFilmbackName();
	}

	return FString();
}

float AVirtualCameraPlayerControllerBase::GetCurrentFocalLength()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentFocalLength();
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentFocusDistance()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentFocusDistance();
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentRecordingFrameRate()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentRecordingFrameRate();
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentRecordingLength()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentRecordingLength();
	}

	return 0.0f;
}

FString AVirtualCameraPlayerControllerBase::GetCurrentRecordingSceneName()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentRecordingSceneName();
	}

	return FString();
}

FString AVirtualCameraPlayerControllerBase::GetCurrentRecordingTakeName()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentRecordingTakeName();
	}

	return FString();
}

float AVirtualCameraPlayerControllerBase::GetCurrentSequencePlaybackEnd()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetEndTime().AsSeconds();
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentSequencePlaybackStart()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetStartTime().AsSeconds();
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentSequenceFrameRate()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetFrameRate().AsDecimal();
	}

	return 0.0f;
}

EUnit AVirtualCameraPlayerControllerBase::GetDesiredDistanceUnits()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->GetDesiredDistanceUnits();
	}
	return EUnit();
}



FColor AVirtualCameraPlayerControllerBase::GetFocusPlaneColor()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->FocusSettings.DebugFocusPlaneColor;
	}

	return FColor();
}

bool AVirtualCameraPlayerControllerBase::GetFilmbackPresetOptions(TArray<FString>& OutFilmbackPresetsArray)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetFilmbackPresetOptions(OutFilmbackPresetsArray);
	}

	return false;
}

float AVirtualCameraPlayerControllerBase::GetLevelSequenceLength()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetDuration().AsSeconds();
	}

	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::GetLevelSequences(TArray<FLevelSequenceData>& OutLevelSequenceNames)
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->GetLevelSequences(OutLevelSequenceNames);
	}
}

float AVirtualCameraPlayerControllerBase::GetMatteAspectRatio()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetMatteAspectRatio();
	}
	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetMatteOpacity()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->MatteOpacity;
	}
	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::GetMatteValues(TArray<float>& OutMatteValues)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->GetMatteValues(OutMatteValues);
		OutMatteValues.Sort();
	}
}

float AVirtualCameraPlayerControllerBase::GetPlaybackPosition()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentTime().AsSeconds();
	}

	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::GetScreenshotInfo(FString ScreenshotName, FVirtualCameraScreenshot &OutScreenshotInfo)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetScreenshotInfo(ScreenshotName, OutScreenshotInfo);
	}
}

void AVirtualCameraPlayerControllerBase::GetScreenshotNames(TArray<FString>& OutArray)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetScreenshotNames(OutArray);
		// Sort OutArray reversed by name
		OutArray.Sort([](const FString A, const FString B) {
			return A > B;
		});
	}
}

TMap<FString, FVirtualCameraSettingsPreset> AVirtualCameraPlayerControllerBase::GetSettingsPresets()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->GetSettingsPresets();
	}

	return TMap<FString, FVirtualCameraSettingsPreset>();
}

void AVirtualCameraPlayerControllerBase::GetWaypointInfo(FString WaypointName, FVirtualCameraWaypoint &OutWaypointInfo)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetWaypointInfo(WaypointName, OutWaypointInfo);
	}
}

void AVirtualCameraPlayerControllerBase::GetWaypointNames(TArray<FString>& OutArray)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetWaypointNames(OutArray);
		// Sort OutArray reversed by name
		OutArray.Sort([] (const FString A, const FString B) {
			return A > B;
		});
	}
}

bool AVirtualCameraPlayerControllerBase::IsAxisLocked(EVirtualCameraAxis AxisToCheck)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->IsAxisLocked(AxisToCheck);
	}
	return false;
}

bool AVirtualCameraPlayerControllerBase::IsFocusVisualizationAllowed()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->IsFocusVisualizationAllowed();
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::IsPlaying()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->IsPlaying();
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::IsUsingGlobalBoom()
{
	if (UVirtualCameraMovementComponent* VCMovementComponent = GetVirtualCameraMovementComponent())
	{
		return VCMovementComponent->IsUsingGlobalBoom();
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::JumpToLevelSequenceEnd()
{
	if (LevelSequencePlaybackController && LevelSequencePlaybackController->GetSequence())
	{
		LevelSequencePlaybackController->ScrubToSeconds(LevelSequencePlaybackController->GetEndTime().AsSeconds());
		LevelSequencePlaybackController->Pause();
	}
}

void AVirtualCameraPlayerControllerBase::JumpToLevelSequenceStart()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->ScrubToSeconds(0.0f);
		LevelSequencePlaybackController->Pause();
	}
}

void AVirtualCameraPlayerControllerBase::JumpToPlaybackPositionSeconds(float NewPlaybackPosition)
{
	if (LevelSequencePlaybackController && LevelSequencePlaybackController->GetSequence())
	{
		LevelSequencePlaybackController->JumpToSeconds(NewPlaybackPosition);
	}
}

bool AVirtualCameraPlayerControllerBase::LoadPreset(FString PresetName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->LoadPreset(PresetName);
	}
	return false;
}

bool AVirtualCameraPlayerControllerBase::LoadScreenshotView(const FString& ScreenshotName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->LoadScreenshotView(ScreenshotName);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::PauseLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->Pause();
	}
}

void AVirtualCameraPlayerControllerBase::PlayLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->Play();
	}
}

void AVirtualCameraPlayerControllerBase::PlayLevelSequenceInReverse()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->PlayReverse();
	}
}

void AVirtualCameraPlayerControllerBase::ResetCameraOffsetsToTracker()
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->ResetCameraOffsetsToTracker();
	}
}

void AVirtualCameraPlayerControllerBase::ResumeLevelSequencePlay()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->ResumeLevelSequencePlay();
	}
}

void AVirtualCameraPlayerControllerBase::SaveHomeWaypoint(const FString& NewHomeWaypointName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SaveHomeWaypoint(NewHomeWaypointName);
	}
}

FString AVirtualCameraPlayerControllerBase::SavePreset(bool bSaveCameraSettings, bool bSaveStabilization, bool bSaveAxisLocking, bool bSaveMotionScale)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->SavePreset(bSaveCameraSettings, bSaveStabilization, bSaveAxisLocking, bSaveMotionScale);
	}
	return FString();
}

FString AVirtualCameraPlayerControllerBase::SaveWaypoint()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->SaveWaypoint();
	}

	return FString();
}

bool AVirtualCameraPlayerControllerBase::SetActiveLevelSequence(const FString & LevelSequenceName)
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->SetActiveLevelSequence(LevelSequenceName);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::SetAllowFocusPlaneVisualization(bool bShouldAllowFocusVisualization)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetAllowFocusPlaneVisualization(bShouldAllowFocusVisualization);
	}
}

float AVirtualCameraPlayerControllerBase::SetAxisStabilizationScale(EVirtualCameraAxis AxisToAdjust, float NewStabilizationAmount)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->SetAxisStabilizationScale(AxisToAdjust, NewStabilizationAmount);
	}

	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::SetCurrentAperture(float NewAperture)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->SetCurrentAperture(NewAperture);
	}
}

void AVirtualCameraPlayerControllerBase::SetCurrentFocalLength(const float NewFocalLength)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->SetCurrentFocalLength(NewFocalLength);
	}
}

void AVirtualCameraPlayerControllerBase::SetCurrentFocusDistance(const float NewFocusDistance)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->SetCurrentFocusDistance(NewFocusDistance);
	}
}

void AVirtualCameraPlayerControllerBase::SetDesiredDistanceUnits(const EUnit DesiredUnits)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetDesiredDistanceUnits(DesiredUnits);
	}
}

bool AVirtualCameraPlayerControllerBase::SetFilmbackPresetOption(const FString & NewFilmbackPreset)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->SetFilmbackPresetOption(NewFilmbackPreset);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::SetFocusMethod(const EVirtualCameraFocusMethod NewFocusMethod)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CurrentFocusMethod = NewFocusMethod;
		FocusMethodChanged(NewFocusMethod);
		return CineCamera->SetFocusMethod(NewFocusMethod);
	}
}

void AVirtualCameraPlayerControllerBase::SetFocusPlaneColor(const FColor NewFocusPlaneColor)
{
	if (GetVirtualCameraCineCameraComponent())
	{
		GetVirtualCameraCineCameraComponent()->FocusSettings.DebugFocusPlaneColor = NewFocusPlaneColor;
	}
}

void AVirtualCameraPlayerControllerBase::SetFocusVisualization(bool bShowFocusVisualization)
{
	if (GetVirtualCameraCineCameraComponent())
	{
		GetVirtualCameraCineCameraComponent()->SetFocusVisualization(bShowFocusVisualization);
	}
}

bool AVirtualCameraPlayerControllerBase::SetMatteAspectRatio(const float NewMatteAspectRatio)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->SetMatteAspectRatio(NewMatteAspectRatio);
	}
	return false;
}

void AVirtualCameraPlayerControllerBase::SetMatteOpacity(const float NewMatteOpacity)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->MatteOpacity = NewMatteOpacity;
	}
}

void AVirtualCameraPlayerControllerBase::SetMovementScale(const EVirtualCameraAxis AxisToAdjust, const float NewMovementScale)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->SetMovementScale(AxisToAdjust, NewMovementScale);
	}
}

void AVirtualCameraPlayerControllerBase::SetPresetFavoriteStatus(const FString& PresetName, const bool bIsFavorite)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetPresetFavoriteStatus(PresetName, bIsFavorite);
	}
}

void AVirtualCameraPlayerControllerBase::SetSaveSettingsWhenClosing(bool bShouldSettingsSave)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetSaveSettingsWhenClosing(bShouldSettingsSave);
	}
}

void AVirtualCameraPlayerControllerBase::SetScreenshotFavoriteStatus(const FString& ScreenshotName, const bool bIsFavorite)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetScreenshotFavoriteStatus(ScreenshotName, bIsFavorite);
	}
}

void AVirtualCameraPlayerControllerBase::SetUseGlobalBoom(bool bShouldUseGlobalBoom)
{
	if (UVirtualCameraMovementComponent* VCMovementComponent = GetVirtualCameraMovementComponent())
	{
		VCMovementComponent->SetUseGlobalBoom(bShouldUseGlobalBoom);
	}
}

void AVirtualCameraPlayerControllerBase::SetWaypointFavoriteStatus(const FString& WaypointName, const bool bIsFavorite)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetWaypointFavoriteStatus(WaypointName, bIsFavorite);
	}
}

void AVirtualCameraPlayerControllerBase::SetZeroDutchOnLock(const bool bInValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->SetZeroDutchOnLock(bInValue);
	}
}

bool AVirtualCameraPlayerControllerBase::ShouldSaveSettingsWhenClosing()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->GetSaveSettingsWhenClosing();
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::StartRecording()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->StartRecording();
	}
}

void AVirtualCameraPlayerControllerBase::StopLevelSequencePlay()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->Stop();
	}
}

void AVirtualCameraPlayerControllerBase::StopRecording()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->StopRecording();
	}
}

FString AVirtualCameraPlayerControllerBase::TakeScreenshot()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->TakeScreenshot();
	}

	return FString();
}

void AVirtualCameraPlayerControllerBase::TeleportToHomeWaypoint()
{
	if (GetVirtualCameraPawn() && GetVirtualCameraPawn()->TeleportToHomeWaypoint())
	{
		// Do nothing since we teleport in the above call
	}
	else 
	{
		ResetCameraOffsetsToTracker();
	}
}

bool AVirtualCameraPlayerControllerBase::TeleportToWaypoint(const FString& WaypointName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->TeleportToWaypoint(WaypointName);
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::ToggleAxisFreeze(const EVirtualCameraAxis AxisToToggle)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->ToggleAxisFreeze(AxisToToggle);
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::ToggleAxisLock(const EVirtualCameraAxis AxisToToggle)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->ToggleAxisLock(AxisToToggle);
	}

	return false;
}

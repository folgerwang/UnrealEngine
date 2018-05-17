// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "MagicLeapVREyeTracker.h"
#include "IEyeTrackerModule.h"
#include "EyeTrackerTypes.h"
#include "IEyeTracker.h"
#include "IMagicLeapVREyeTracker.h"
#include "MagicLeapEyeTrackerTypes.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "AppFramework.h"
#include "MagicLeapHMD.h"
#include "UnrealEngine.h"
#include "MagicLeapSDKDetection.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_eye_tracking.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

FMagicLeapVREyeTracker::FMagicLeapVREyeTracker()
: EyeTrackingStatus(EMagicLeapEyeTrackingStatus::NotConnected)
, bReadyToInit(false)
, bInitialized(false)
, bIsCalibrated(false)
#if WITH_MLSDK
, EyeTrackingHandle(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
{
	SetDefaultDataValues();
}

FMagicLeapVREyeTracker::~FMagicLeapVREyeTracker()
{
#if WITH_MLSDK
	if (EyeTrackingHandle != ML_INVALID_HANDLE)
	{
		MLEyeTrackingDestroy(EyeTrackingHandle);
		EyeTrackingHandle = ML_INVALID_HANDLE;
	}
#endif //WITH_MLSDK
}


void FMagicLeapVREyeTracker::SetDefaultDataValues()
{
	FMemory::Memzero(UnfilteredEyeTrackingData);
#if WITH_MLSDK
	FMemory::Memzero(EyeTrackingStaticData);
#endif //WITH_MLSDK
}


void FMagicLeapVREyeTracker::SetActivePlayerController(APlayerController* NewActivePlayerController)
{
	if (NewActivePlayerController && NewActivePlayerController->IsValidLowLevel() && NewActivePlayerController != ActivePlayerController)
	{
		ActivePlayerController = NewActivePlayerController;
	}
}

bool FMagicLeapVREyeTracker::Tick(float DeltaTime)
{
	bool bSuccess = true;
#if WITH_MLSDK && PLATFORM_LUMIN
	//assume we're in a bad state
	UnfilteredEyeTrackingData.bIsStable = false;

	if (EyeTrackingHandle != ML_INVALID_HANDLE)
	{
		//check stat first to make sure everything is valid
		MLEyeTrackingState TempTrackingState;
		FMemory::Memzero(TempTrackingState);
		bSuccess = bSuccess && MLEyeTrackingGetState(EyeTrackingHandle, &TempTrackingState);

		//make sure this is valid eye tracking data!
		if (bSuccess
			&& (TempTrackingState.error == MLEyeTrackingError_None) 
			&& (TempTrackingState.fixation_confidence > 0.0f)
			&& (TempTrackingState.left_center_confidence > 0.0f)
			&& (TempTrackingState.right_center_confidence > 0.0f))
		{
			EyeTrackingStatus = EMagicLeapEyeTrackingStatus::UserPresentAndWatchingWindow;

			bIsCalibrated = TempTrackingState.calibration_complete;

			UnfilteredEyeTrackingData.bIsStable = true;
			FDateTime Now = FDateTime::UtcNow();
			UnfilteredEyeTrackingData.TimeStamp = Now;

			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFramework();
			if (AppFramework.IsInitialized())
			{
				FTransform PoseTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);

				//harvest the 3 transforms
				EFailReason FailReason;
				FTransform FixationTransform;
				AppFramework.GetTransform(EyeTrackingStaticData.fixation, FixationTransform, FailReason);
				FixationTransform.AddToTranslation(PoseTransform.GetLocation());
				FixationTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());

				FTransform LeftCenterTransform;
				AppFramework.GetTransform(EyeTrackingStaticData.left_center, LeftCenterTransform, FailReason);
				LeftCenterTransform.AddToTranslation(PoseTransform.GetLocation());
				LeftCenterTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());

				FTransform RightCenterTransform;
				AppFramework.GetTransform(EyeTrackingStaticData.right_center, RightCenterTransform, FailReason);
				RightCenterTransform.AddToTranslation(PoseTransform.GetLocation());
				RightCenterTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());

				//average the left and right eye
				UnfilteredEyeTrackingData.AverageGazeOrigin = (LeftCenterTransform.GetLocation() + RightCenterTransform.GetLocation()) * .5f;
				//get focal point
				UnfilteredEyeTrackingData.WorldAverageGazeConvergencePoint = FixationTransform.GetTranslation();
				//get the gaze vector (Point-Eye)
				UnfilteredEyeTrackingData.AverageGazeRay = (UnfilteredEyeTrackingData.WorldAverageGazeConvergencePoint - UnfilteredEyeTrackingData.AverageGazeOrigin);
				UnfilteredEyeTrackingData.AverageGazeRay.Normalize();

				UnfilteredEyeTrackingData.LeftOriginPoint = LeftCenterTransform.GetLocation();
				UnfilteredEyeTrackingData.RightOriginPoint = RightCenterTransform.GetLocation();

				UnfilteredEyeTrackingData.Confidence = TempTrackingState.fixation_confidence;

				UnfilteredEyeTrackingData.bLeftBlink = TempTrackingState.left_blink;
				UnfilteredEyeTrackingData.bRightBlink = TempTrackingState.right_blink;

				//UE_LOG(LogCore, Warning, TEXT("   SUCCESS -> "));
				//UE_LOG(LogCore, Warning, TEXT("        Gaze Origin %f, %f, %f"), UnfilteredEyeTrackingData.AverageGazeOrigin.X, UnfilteredEyeTrackingData.AverageGazeOrigin.Y, UnfilteredEyeTrackingData.AverageGazeOrigin.Z);
				//UE_LOG(LogCore, Warning, TEXT("        Converge    %f, %f, %f"), UnfilteredEyeTrackingData.WorldAverageGazeConvergencePoint.X, UnfilteredEyeTrackingData.WorldAverageGazeConvergencePoint.Y, UnfilteredEyeTrackingData.WorldAverageGazeConvergencePoint.Z);
				//UE_LOG(LogCore, Warning, TEXT("        Gaze Ray    %f, %f, %f"), UnfilteredEyeTrackingData.AverageGazeRay.X, UnfilteredEyeTrackingData.AverageGazeRay.Y, UnfilteredEyeTrackingData.AverageGazeRay.Z);
			}
		}
		else
		{
			EyeTrackingStatus = EMagicLeapEyeTrackingStatus::UserNotPresent;
		}
	}
	else
	{
		if (!bInitialized && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->IsPerceptionEnabled())
		{
#if !PLATFORM_MAC
			//keep trying until we are successful in creating one
			ML_FUNCTION_WRAPPER(EyeTrackingHandle = MLEyeTrackingCreate());
			bInitialized = MLHandleIsValid(EyeTrackingHandle);
			if (bInitialized)
			{
				UE_LOG(LogCore, Warning, TEXT("   VR Eye Tracker Created"));
				// Needs to be called only once.
				MLEyeTrackingGetStaticData(EyeTrackingHandle, &EyeTrackingStaticData);
			}
			else
			{
				UE_LOG(LogCore, Warning, TEXT("   Unable to create VR Eye Tracker"));
			}
#endif
		}
	}
#endif //WITH_MLSDK && PLATFORM_LUMIN
	return true;
}

void FMagicLeapVREyeTracker::DrawDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	DrawDebugSphere(HUD->GetWorld(), UnfilteredEyeTrackingData.WorldAverageGazeConvergencePoint, 20.0f, 16, UnfilteredEyeTrackingData.bIsStable ? FColor::Green : FColor::Red);
}

const FMagicLeapVREyeTrackingData& FMagicLeapVREyeTracker::GetVREyeTrackingData()
{
	bReadyToInit = true;
	return UnfilteredEyeTrackingData;
}

EMagicLeapEyeTrackingStatus FMagicLeapVREyeTracker::GetEyeTrackingStatus()
{
	bReadyToInit = true;
	return EyeTrackingStatus;
}

bool FMagicLeapVREyeTracker::IsEyeTrackerCalibrated() const
{
	return bIsCalibrated;
}


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

#include "MagicLeapEyeTrackerModule.h"
#include "GameFramework/HUD.h"
#include "MagicLeapVREyeTracker.h"
#include "Engine/Engine.h"

IMPLEMENT_MODULE(FMagicLeapEyeTrackerModule, MagicLeapEyeTracker)

static TAutoConsoleVariable<float> CVarFovealRegionAngleDegrees(TEXT("MagicLeap.FovealRegionAngleDegrees"), 1.5f, TEXT("A larger value here will lead to the GTOM system considering a larger area around the gaze point. Refer to this link to see what values are reasonable: https://en.wikipedia.org/wiki/Fovea_centralis#/media/File:Macula.svg"));
static TAutoConsoleVariable<int32> CVarEnableEyetrackingDebug(TEXT("MagicLeap.debug.EnableEyetrackingDebug"), 1, TEXT("0 - Eyetracking debug visualizations are disabled. 1 - Eyetracking debug visualizations are enabled."));



FMagicLeapEyeTracker::FMagicLeapEyeTracker()
{
	VREyeTracker = new FMagicLeapVREyeTracker();
}

FMagicLeapEyeTracker::~FMagicLeapEyeTracker()
{
	if (VREyeTracker)
	{
		delete VREyeTracker;
		VREyeTracker = nullptr;
	}
}


bool FMagicLeapEyeTracker::GetEyeTrackerGazeData(FEyeTrackerGazeData& OutGazeData) const
{
	if (VREyeTracker)
	{
		// get data from the eyetracker
		const FMagicLeapVREyeTrackingData& VRGazeData = VREyeTracker->GetVREyeTrackingData();
		if (EEyeTrackerStatus::Tracking == GetEyeTrackerStatus())
		{
			OutGazeData.GazeDirection = VRGazeData.AverageGazeRay;
			OutGazeData.GazeOrigin = VRGazeData.AverageGazeOrigin;
			OutGazeData.FixationPoint = VRGazeData.WorldAverageGazeConvergencePoint;
			OutGazeData.ConfidenceValue = VRGazeData.Confidence;
			return true;
		}
	}

	// default
	OutGazeData = FEyeTrackerGazeData();
	return false;
}

bool FMagicLeapEyeTracker::GetEyeTrackerStereoGazeData(FEyeTrackerStereoGazeData& OutStereoGazeData) const
{
	if (VREyeTracker)
	{
		// get data from the VR eyetracker
		const FMagicLeapVREyeTrackingData& VRGazeData = VREyeTracker->GetVREyeTrackingData();
		if (EEyeTrackerStatus::Tracking == GetEyeTrackerStatus())
		{
			OutStereoGazeData.LeftEyeOrigin = VRGazeData.LeftOriginPoint;
			OutStereoGazeData.LeftEyeDirection = VRGazeData.WorldAverageGazeConvergencePoint - VRGazeData.LeftOriginPoint;
			OutStereoGazeData.LeftEyeDirection.Normalize();

			OutStereoGazeData.RightEyeOrigin = VRGazeData.RightOriginPoint;
			OutStereoGazeData.RightEyeDirection = VRGazeData.WorldAverageGazeConvergencePoint - VRGazeData.RightOriginPoint;
			OutStereoGazeData.RightEyeDirection.Normalize();

			OutStereoGazeData.FixationPoint = VRGazeData.WorldAverageGazeConvergencePoint;
			OutStereoGazeData.ConfidenceValue = VRGazeData.Confidence;
			return true;
		}
	}

	// default
	OutStereoGazeData = FEyeTrackerStereoGazeData();
	return false;
}

EEyeTrackerStatus FMagicLeapEyeTracker::GetEyeTrackerStatus() const
{
	EMagicLeapEyeTrackingStatus const TrackerStatus = VREyeTracker->GetEyeTrackingStatus();

	// translate to UE4 enum
	switch (TrackerStatus)
	{
	case EMagicLeapEyeTrackingStatus::NotConnected:
	case EMagicLeapEyeTrackingStatus::Disabled:
		return EEyeTrackerStatus::NotConnected;

	case EMagicLeapEyeTrackingStatus::UserNotPresent:
	case EMagicLeapEyeTrackingStatus::UserPresent:
		return EEyeTrackerStatus::NotTracking;

	case EMagicLeapEyeTrackingStatus::UserPresentAndWatchingWindow:
		return EEyeTrackerStatus::Tracking;
	}

	// should not get here
	UE_LOG(LogCore, Warning, TEXT("   GET STATUS FAIL! %d"), (uint32)TrackerStatus);
	return EEyeTrackerStatus::NotConnected;
}

bool FMagicLeapEyeTracker::IsStereoGazeDataAvailable() const
{
	if (VREyeTracker)
	{
		return true;
	}

	return false;
}

void FMagicLeapEyeTracker::SetEyeTrackedPlayer(APlayerController* PlayerController)
{
	if (VREyeTracker)
	{
		VREyeTracker->SetActivePlayerController(PlayerController);
	}
}

bool FMagicLeapEyeTracker::IsEyeTrackerCalibrated() const
{
	if (VREyeTracker)
	{
		return VREyeTracker->IsEyeTrackerCalibrated();
	}

	return false;
}

bool FMagicLeapEyeTracker::GetEyeBlinkState(FMagicLeapEyeBlinkState& BlinkState) const
{
	if (VREyeTracker)
	{
		const FMagicLeapVREyeTrackingData& VRGazeData = VREyeTracker->GetVREyeTrackingData();
		if (EEyeTrackerStatus::Tracking == GetEyeTrackerStatus())
		{
			BlinkState.LeftEyeBlinked = VRGazeData.bLeftBlink;
			BlinkState.RightEyeBlinked = VRGazeData.bRightBlink;

			return true;
		}
	}

	return false;
}


/************************************************************************/
/* FMagicLeapEyeTrackerModule                                                     */
/************************************************************************/
void FMagicLeapEyeTrackerModule::StartupModule()
{
	IEyeTrackerModule::StartupModule();

	MagicLeapEyeTracker = TSharedPtr<FMagicLeapEyeTracker, ESPMode::ThreadSafe>(new FMagicLeapEyeTracker());

	OnDrawDebugHandle = AHUD::OnShowDebugInfo.AddRaw(this, &FMagicLeapEyeTrackerModule::OnDrawDebug);
}

void FMagicLeapEyeTrackerModule::ShutdownModule()
{
	AHUD::OnShowDebugInfo.Remove(OnDrawDebugHandle);
	FCoreUObjectDelegates::PreLoadMap.Remove(OnPreLoadMapHandle);
}

TSharedPtr<class IEyeTracker, ESPMode::ThreadSafe> FMagicLeapEyeTrackerModule::CreateEyeTracker()
{
	return MagicLeapEyeTracker;
}

void FMagicLeapEyeTrackerModule::OnDrawDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	if (CVarEnableEyetrackingDebug.GetValueOnGameThread())
	{
		if (MagicLeapEyeTracker.IsValid() && MagicLeapEyeTracker->VREyeTracker)
		{
			MagicLeapEyeTracker->VREyeTracker->DrawDebug(HUD, Canvas, DisplayInfo, YL, YPos);
		}
	}
}


bool FMagicLeapEyeTrackerModule::IsEyeTrackerConnected() const
{
	IEyeTracker* EyeTracker = MagicLeapEyeTracker.Get();
	if (EyeTracker)
	{
		EEyeTrackerStatus Status = EyeTracker->GetEyeTrackerStatus();
		if ((Status != EEyeTrackerStatus::NotTracking) && (Status != EEyeTrackerStatus::NotConnected))
		{
			return true;
		}
	}

	return false;
}

bool UMagicLeapEyeTrackerFunctionLibrary::IsEyeTrackerCalibrated()
{
	// TODO: Don't do this. Use StaticCastSharedPtr().
	FMagicLeapEyeTracker* ET = GEngine ? static_cast<FMagicLeapEyeTracker*>(GEngine->EyeTrackingDevice.Get()) : nullptr;
	if (ET)
	{
		return ET->IsEyeTrackerCalibrated();
	}

	return false;
}

bool UMagicLeapEyeTrackerFunctionLibrary::GetEyeBlinkState(FMagicLeapEyeBlinkState& BlinkState)
{
	// TODO: Don't do this. Use StaticCastSharedPtr().
	FMagicLeapEyeTracker* ET = GEngine ? static_cast<FMagicLeapEyeTracker*>(GEngine->EyeTrackingDevice.Get()) : nullptr;
	if (ET)
	{
		return ET->GetEyeBlinkState(BlinkState);
	}

	return false;
}

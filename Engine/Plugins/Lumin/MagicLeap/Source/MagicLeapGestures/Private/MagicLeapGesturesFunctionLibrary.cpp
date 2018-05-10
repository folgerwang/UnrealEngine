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

#include "MagicLeapGesturesFunctionLibrary.h"
#include "IMagicLeapGesturesPlugin.h"
#include "MagicLeapGestures.h"
#include "IMagicLeapPlugin.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"

bool UMagicLeapGesturesFunctionLibrary::GetHandCenter(EControllerHand Hand, FTransform& HandCenter)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapGestures::FGestureData& GestureData = (Hand == EControllerHand::Right) ? gestures->CurrentRightGestureData() : gestures->CurrentLeftGestureData();
			HandCenter = GestureData.HandCenter;
			return GestureData.HandCenterValid;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapGesturesFunctionLibrary::GetHandPointer(EControllerHand Hand, EGestureTransformSpace TransformSpace, FTransform& Pointer)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapGestures::FGestureData& GestureData = (Hand == EControllerHand::Right) ? gestures->CurrentRightGestureData() : gestures->CurrentLeftGestureData();
			switch (TransformSpace)
			{
			case EGestureTransformSpace::Tracking:
			{
				Pointer = GestureData.HandPointer;
				break;
			}
			case EGestureTransformSpace::World:
			{
				FTransform TrackingToWorldTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
				Pointer = GestureData.HandPointer * TrackingToWorldTransform;
				break;
			}
			case EGestureTransformSpace::Hand:
			{
				Pointer = GestureData.HandPointer * GestureData.HandCenter.Inverse();
				break;
			}
			default:
				check(false);
				return false;
			}
			return GestureData.HandCenterValid;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapGesturesFunctionLibrary::GetHandSecondary(EControllerHand Hand, EGestureTransformSpace TransformSpace, FTransform& Secondary)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapGestures::FGestureData& GestureData = (Hand == EControllerHand::Right) ? gestures->CurrentRightGestureData() : gestures->CurrentLeftGestureData();
			switch (TransformSpace)
			{
			case EGestureTransformSpace::Tracking:
			{
				Secondary = GestureData.HandSecondary;
				break;
			}
			case EGestureTransformSpace::World:
			{
				FTransform TrackingToWorldTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
				Secondary = GestureData.HandSecondary * TrackingToWorldTransform;
				break;
			}
			case EGestureTransformSpace::Hand:
			{
				Secondary = GestureData.HandSecondary * GestureData.HandCenter.Inverse();
				break;
			}
			default:
				check(false);
				return false;
			}
			return GestureData.HandCenterValid;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapGesturesFunctionLibrary::GetHandCenterNormalized(EControllerHand Hand, FVector& HandCenterNormalized)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			if (gestures->CurrentLeftGestureData().HandCenterValid)
			{
				HandCenterNormalized = gestures->CurrentLeftGestureData().HandCenterNormalized;
			}
			return gestures->CurrentLeftGestureData().HandCenterValid;
		}
		else if (Hand == EControllerHand::Right)
		{
			if (gestures->CurrentRightGestureData().HandCenterValid)
			{
				HandCenterNormalized = gestures->CurrentRightGestureData().HandCenterNormalized;
			}
			return gestures->CurrentRightGestureData().HandCenterValid;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapGesturesFunctionLibrary::GetGestureKeypoints(EControllerHand Hand, TArray<FTransform>& Keypoints)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			Keypoints = gestures->CurrentLeftGestureData().Keypoints;
			return true;
		}
		else if (Hand == EControllerHand::Right)
		{
			Keypoints = gestures->CurrentRightGestureData().Keypoints;
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapGesturesFunctionLibrary::SetConfiguration(const TArray<EStaticGestures>& StaticGesturesToActivate, EGestureKeypointsFilterLevel KeypointsFilterLevel, EGestureRecognitionFilterLevel GestureFilterLevel, EGestureRecognitionFilterLevel HandSwitchingFilterLevel)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());
	return gestures.IsValid() && gestures->SetConfiguration(StaticGesturesToActivate, KeypointsFilterLevel, GestureFilterLevel, HandSwitchingFilterLevel);
}

bool UMagicLeapGesturesFunctionLibrary::GetConfiguration(TArray<EStaticGestures>& ActiveStaticGestures, EGestureKeypointsFilterLevel& KeypointsFilterLevel, EGestureRecognitionFilterLevel& GestureFilterLevel, EGestureRecognitionFilterLevel& HandSwitchingFilterLevel)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());
	return gestures.IsValid() && gestures->GetConfiguration(ActiveStaticGestures, KeypointsFilterLevel, GestureFilterLevel, HandSwitchingFilterLevel);
}

void UMagicLeapGesturesFunctionLibrary::SetStaticGestureConfidenceThreshold(EStaticGestures Gesture, float Confidence)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());
	if (gestures.IsValid())
	{
		gestures->SetStaticGestureConfidenceThreshold(Gesture, Confidence);
	}
}

float UMagicLeapGesturesFunctionLibrary::GetStaticGestureConfidenceThreshold(EStaticGestures Gesture)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());
	return (gestures.IsValid()) ? gestures->GetStaticGestureConfidenceThreshold(Gesture) : 0.0f;
}

bool UMagicLeapGesturesFunctionLibrary::GetHandGestureConfidence(EControllerHand Hand, float& Confidence)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			Confidence = gestures->CurrentLeftGestureData().GestureConfidence;
			return true;
		}
		else if (Hand == EControllerHand::Right)
		{
			Confidence = gestures->CurrentRightGestureData().GestureConfidence;
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapGesturesFunctionLibrary::GetCurrentGesture(EControllerHand Hand, EStaticGestures& Gesture)
{
	TSharedPtr<FMagicLeapGestures> gestures = StaticCastSharedPtr<FMagicLeapGestures>(IMagicLeapGesturesPlugin::Get().GetInputDevice());

	if (gestures.IsValid() && gestures->IsGestureStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			Gesture = gestures->CurrentLeftGestureData().Gesture;
			return true;
		}
		else if (Hand == EControllerHand::Right)
		{
			Gesture = gestures->CurrentRightGestureData().Gesture;
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			Gesture = EStaticGestures::NoHand;
			return false;
		}
	}

	Gesture = EStaticGestures::NoHand;
	return false;
}

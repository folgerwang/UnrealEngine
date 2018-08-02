// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapHandTrackingFunctionLibrary.h"
#include "IMagicLeapHandTrackingPlugin.h"
#include "MagicLeapHandTracking.h"
#include "IMagicLeapPlugin.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkSource.h"

bool UMagicLeapHandTrackingFunctionLibrary::GetHandCenter(EControllerHand Hand, FTransform& HandCenter)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapHandTracking::FHandState& HandTrackingData = (Hand == EControllerHand::Right) ? HandTracking->GetRightHandState() : HandTracking->GetLeftHandState();
			HandCenter = HandTrackingData.HandCenter.Transform;
			return HandTrackingData.IsValid();
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetHandIndexFingerTip(EControllerHand Hand, EGestureTransformSpace TransformSpace, FTransform& Pointer)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapHandTracking::FHandState& HandTrackingData = (Hand == EControllerHand::Right) ? HandTracking->GetRightHandState() : HandTracking->GetLeftHandState();
			switch (TransformSpace)
			{
			case EGestureTransformSpace::Tracking:
			{
				Pointer = HandTrackingData.IndexFinger.Tip.Transform;
				break;
			}
			case EGestureTransformSpace::World:
			{
				FTransform TrackingToWorldTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
				Pointer = HandTrackingData.IndexFinger.Tip.Transform * TrackingToWorldTransform;
				break;
			}
			case EGestureTransformSpace::Hand:
			{
				Pointer = HandTrackingData.IndexFinger.Tip.Transform * HandTrackingData.HandCenter.Transform.Inverse();
				break;
			}
			default:
				check(false);
				return false;
			}
			return HandTrackingData.IsValid();
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetHandThumbTip(EControllerHand Hand, EGestureTransformSpace TransformSpace, FTransform& Secondary)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapHandTracking::FHandState& HandTrackingData = (Hand == EControllerHand::Right) ? HandTracking->GetRightHandState() : HandTracking->GetLeftHandState();
			switch (TransformSpace)
			{
			case EGestureTransformSpace::Tracking:
			{
				Secondary = HandTrackingData.Thumb.Tip.Transform;
				break;
			}
			case EGestureTransformSpace::World:
			{
				FTransform TrackingToWorldTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
				Secondary = HandTrackingData.Thumb.Tip.Transform * TrackingToWorldTransform;
				break;
			}
			case EGestureTransformSpace::Hand:
			{
				Secondary = HandTrackingData.Thumb.Tip.Transform * HandTrackingData.HandCenter.Transform.Inverse();
				break;
			}
			default:
				check(false);
				return false;
			}
			return HandTrackingData.IsValid();
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetHandCenterNormalized(EControllerHand Hand, FVector& HandCenterNormalized)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			if (HandTracking->GetLeftHandState().IsValid())
			{
				HandCenterNormalized = HandTracking->GetLeftHandState().HandCenterNormalized;
			}
			return HandTracking->GetLeftHandState().IsValid();
		}
		else if (Hand == EControllerHand::Right)
		{
			if (HandTracking->GetRightHandState().IsValid())
			{
				HandCenterNormalized = HandTracking->GetRightHandState().HandCenterNormalized;
			}
			return HandTracking->GetRightHandState().IsValid();
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetGestureKeypoints(EControllerHand Hand, TArray<FTransform>& Keypoints)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left || Hand == EControllerHand::Right)
		{
			const FMagicLeapHandTracking::FHandState& HandState = (Hand == EControllerHand::Left) ? HandTracking->GetLeftHandState() : HandTracking->GetRightHandState();
			Keypoints.SetNum(3);
			Keypoints[0] = HandState.HandCenter.Transform;
			Keypoints[1] = HandState.IndexFinger.Tip.Transform;
			Keypoints[2] = HandState.Thumb.Tip.Transform;
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetGestureKeypointTransform(EControllerHand Hand, EHandTrackingKeypoint Keypoint, EGestureTransformSpace TransformSpace, FTransform& OutTransform)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		FTransform KeyPointTransform;
		const bool bSuccess = HandTracking->GetKeypointTransform(Hand, Keypoint, KeyPointTransform);

		if (bSuccess)
		{
			switch (TransformSpace)
			{
			case EGestureTransformSpace::Tracking:
			{
				OutTransform = KeyPointTransform;
				return true;
			}
			case EGestureTransformSpace::World:
			{
				FTransform TrackingToWorldTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
				OutTransform = KeyPointTransform * TrackingToWorldTransform;
				return true;
			}
			case EGestureTransformSpace::Hand:
			{
				FTransform HandTransform;
				const bool bSuccess2 = HandTracking->GetKeypointTransform(Hand, EHandTrackingKeypoint::Hand_Center, HandTransform);
				if (bSuccess2)
				{
					OutTransform = KeyPointTransform * HandTransform.Inverse();
					return true;
				}
				break;
			}
			default:
				check(false);
				return false;
			}
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::SetConfiguration(const TArray<EHandTrackingGesture>& StaticGesturesToActivate, EHandTrackingKeypointFilterLevel KeypointsFilterLevel, EHandTrackingGestureFilterLevel GestureFilterLevel, EHandTrackingGestureFilterLevel HandSwitchingFilterLevel, bool bEnabled)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());
	return HandTracking.IsValid() && HandTracking->SetConfiguration(bEnabled, StaticGesturesToActivate, KeypointsFilterLevel, GestureFilterLevel);
}

bool UMagicLeapHandTrackingFunctionLibrary::GetConfiguration(TArray<EHandTrackingGesture>& ActiveStaticGestures, EHandTrackingKeypointFilterLevel& KeypointsFilterLevel, EHandTrackingGestureFilterLevel& GestureFilterLevel, EHandTrackingGestureFilterLevel& HandSwitchingFilterLevel, bool& bEnabled)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());
	return HandTracking.IsValid() && HandTracking->GetConfiguration(bEnabled, ActiveStaticGestures, KeypointsFilterLevel, GestureFilterLevel);
}

void UMagicLeapHandTrackingFunctionLibrary::SetStaticGestureConfidenceThreshold(EHandTrackingGesture Gesture, float Confidence)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());
	if (HandTracking.IsValid())
	{
		HandTracking->SetGestureConfidenceThreshold(Gesture, Confidence);
	}
}

float UMagicLeapHandTrackingFunctionLibrary::GetStaticGestureConfidenceThreshold(EHandTrackingGesture Gesture)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());
	return (HandTracking.IsValid()) ? HandTracking->GetGestureConfidenceThreshold(Gesture) : 0.0f;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetCurrentGestureConfidence(EControllerHand Hand, float& Confidence)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			Confidence = HandTracking->GetLeftHandState().GestureConfidence;
			return true;
		}
		else if (Hand == EControllerHand::Right)
		{
			Confidence = HandTracking->GetRightHandState().GestureConfidence;
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			return false;
		}
	}

	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetCurrentGesture(EControllerHand Hand, EHandTrackingGesture& Gesture)
{
	TSharedPtr<FMagicLeapHandTracking> HandTracking = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetInputDevice());

	if (HandTracking.IsValid() && HandTracking->IsHandTrackingStateValid())
	{
		if (Hand == EControllerHand::Left)
		{
			Gesture = HandTracking->GetLeftHandState().Gesture;
			return true;
		}
		else if (Hand == EControllerHand::Right)
		{
			Gesture = HandTracking->GetRightHandState().Gesture;
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapHandTracking, Error, TEXT("Hand %d is not supported"), static_cast<int32>(Hand));
			Gesture = EHandTrackingGesture::NoHand;
			return false;
		}
	}

	Gesture = EHandTrackingGesture::NoHand;
	return false;
}

bool UMagicLeapHandTrackingFunctionLibrary::GetMagicLeapHandTrackingLiveLinkSource(FLiveLinkSourceHandle& SourceHandle)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		TSharedPtr<ILiveLinkSource> HandTrackingSource = StaticCastSharedPtr<FMagicLeapHandTracking>(IMagicLeapHandTrackingPlugin::Get().GetLiveLinkSource());
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient->AddSource(HandTrackingSource);
		SourceHandle.SetSourcePointer(HandTrackingSource);
		return true;
	}
	else
	{
		SourceHandle.SetSourcePointer(nullptr);
		return false;
	}
};

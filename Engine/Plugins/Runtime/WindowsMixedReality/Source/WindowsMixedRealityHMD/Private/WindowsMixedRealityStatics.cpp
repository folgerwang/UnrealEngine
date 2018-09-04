// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealityStatics.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "WindowsMixedRealityHMD.h"

namespace WindowsMixedReality
{
	FWindowsMixedRealityHMD* GetWindowsMixedRealityHMD() noexcept
	{
		if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FName("WindowsMixedRealityHMD")))
		{
			return static_cast<FWindowsMixedRealityHMD*>(GEngine->XRSystem.Get());
		}

		return nullptr;
	}

	bool FWindowsMixedRealityStatics::SupportsSpatialInput()
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			return hmd->SupportsSpatialInput();
		}

		return false;
	}

	MixedRealityInterop::HMDTrackingStatus FWindowsMixedRealityStatics::GetControllerTrackingStatus(MixedRealityInterop::HMDHand hand)
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			return hmd->GetControllerTrackingStatus(hand);
		}

		return MixedRealityInterop::HMDTrackingStatus::NotTracked;
	}

	bool FWindowsMixedRealityStatics::GetControllerOrientationAndPosition(MixedRealityInterop::HMDHand hand, FRotator & OutOrientation, FVector & OutPosition)
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			return hmd->GetControllerOrientationAndPosition(hand, OutOrientation, OutPosition);
		}

		return false;
	}

	void FWindowsMixedRealityStatics::PollInput()
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			hmd->PollInput();
		}
	}

	MixedRealityInterop::HMDInputPressState FWindowsMixedRealityStatics::GetPressState(
		MixedRealityInterop::HMDHand hand,
		MixedRealityInterop::HMDInputControllerButtons button)
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			return hmd->GetPressState(hand, button);
		}

		return MixedRealityInterop::HMDInputPressState::NotApplicable;
	}

	float FWindowsMixedRealityStatics::GetAxisPosition(MixedRealityInterop::HMDHand hand, MixedRealityInterop::HMDInputControllerAxes axis)
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			return hmd->GetAxisPosition(hand, axis);
		}

		return 0.0f;
	}

	void FWindowsMixedRealityStatics::SubmitHapticValue(MixedRealityInterop::HMDHand hand, float value)
	{
		FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();

		if (hmd != nullptr)
		{
			hmd->SubmitHapticValue(hand, value);
		}
	}
}

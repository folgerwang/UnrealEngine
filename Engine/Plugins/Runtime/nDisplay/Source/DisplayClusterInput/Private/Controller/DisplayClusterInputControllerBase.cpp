// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputControllerBase.h"
#include "XRMotionControllerBase.h"


TArray<FKey> FControllerDeviceHelper::AllDefinedKeys;

void FControllerDeviceHelper::InitializeAllDefinedFKey()
{
	if (AllDefinedKeys.Num() == 0)
	{
		EKeys::GetAllKeys(AllDefinedKeys);
	}
}

bool FControllerDeviceHelper::FindUnrealEngineKeyByName(EDisplayClusterInputDeviceType DevType, const FString& TargetName, FName& TargetKey)
{
	InitializeAllDefinedFKey();

	for (auto& itKey : AllDefinedKeys)
	{
		// Check by target type (Float Axis)
		if (DevType == EDisplayClusterInputDeviceType::VrpnAnalog)
		{
			if (!itKey.IsFloatAxis())
			{
				// Skip all not buttons keys
				continue;
			}
		}

		// Check by target type (Button)
		if (DevType == EDisplayClusterInputDeviceType::VrpnButton ||
			DevType == EDisplayClusterInputDeviceType::VrpnKeyboard)
		{
			if (itKey.IsFloatAxis() || itKey.IsTouch() || itKey.IsVectorAxis())
			{
				// Skip all not buttons keys
				continue;
			}
		}

		// Now check by user target name
		FString ShortName = itKey.GetDisplayName().ToString();
		if (TargetName.Compare(ShortName, ESearchCase::IgnoreCase) == 0)
		{
			TargetKey = itKey.GetFName();
			return true;
		}

		FString LongName = itKey.GetDisplayName(true).ToString();
		if (TargetName.Compare(LongName, ESearchCase::IgnoreCase) == 0)
		{
			TargetKey = itKey.GetFName();
			return true;
		}
	}

	return false;
}

bool FControllerDeviceHelper::FindTrackerByName(const FString& TargetName, EControllerHand& TargetTracker)
{
	FName Source(*TargetName);
	if (FXRMotionControllerBase::GetHandEnumForSourceName(Source, TargetTracker))
	{
		return true;
	}

	return false;
}

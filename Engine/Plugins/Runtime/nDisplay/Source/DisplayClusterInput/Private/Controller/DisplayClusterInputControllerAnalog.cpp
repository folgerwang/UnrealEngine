// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputControllerAnalog.h"
#include "IDisplayClusterInputModule.h"
#include "Misc/DisplayClusterInputLog.h"

#include "IDisplayCluster.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Input/IDisplayClusterInputManager.h"


#define LOCTEXT_NAMESPACE "DisplayClusterInput"


// Add vrpn analog to UE4 global name-space
void FAnalogController::Initialize()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputAnalog);

	static const FName nDisplayClusterInputCategoryName(TEXT("nDisplayAnalogs"));
	EKeys::AddMenuCategoryDisplayInfo(nDisplayClusterInputCategoryName, LOCTEXT("nDisplayInputSubCateogry", "nDisplay"), TEXT("GraphEditor.KeyEvent_16x"));

	uint8 Flags = FKeyDetails::GamepadKey | FKeyDetails::FloatAxis;

	for (int32 idx = 0; idx < FAnalogKey::TotalCount; ++idx)
	{
		FText AnalogLocaleText = FText::Format(LOCTEXT("nDisplayAnalogHintFmt", "nDisplay Analog {0}"), idx);
		UE_LOG(LogDisplayClusterInputAnalog, Verbose, TEXT("Registering %s%d..."), *nDisplayClusterInputCategoryName.ToString(), idx);
		EKeys::AddKey(FKeyDetails(*FAnalogKey::AnalogKeys[idx], AnalogLocaleText, Flags, nDisplayClusterInputCategoryName));
	}

	UE_LOG(LogDisplayClusterInputAnalog, Log, TEXT("nDisplay input controller has been initialized <Analog>"));
}

void FAnalogController::ProcessStartSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputAnalog);

	ResetAllBindings();

	IDisplayClusterInputManager&  InputMgr  = *IDisplayCluster::Get().GetInputMgr();
	IDisplayClusterConfigManager& ConfigMgr = *IDisplayCluster::Get().GetConfigMgr();

	TArray<FString> DeviceNames;
	InputMgr.GetAxisDeviceIds(DeviceNames);
	for (const FString& DeviceName : DeviceNames)
	{
		AddDevice(DeviceName);

		TArray<FDisplayClusterConfigInputSetup> Records = ConfigMgr.GetInputSetupRecords();
		for (const FDisplayClusterConfigInputSetup& Record : Records)
		{
			if (DeviceName.Compare(Record.Id, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterInputAnalog, Verbose, TEXT("Binding %s:%d to %s..."), *DeviceName, Record.Channel, *Record.BindName);
				BindChannel(DeviceName, Record.Channel, Record.BindName);
			}
		}
	}
}

void FAnalogController::ProcessEndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputAnalog);

	UE_LOG(LogDisplayClusterInputAnalog, Verbose, TEXT("Removing all analog bindings..."));

	ResetAllBindings();
}

void FAnalogController::ProcessPreTick()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputAnalog);

	IDisplayClusterInputManager& InputMgr = *IDisplayCluster::Get().GetInputMgr();
	for (auto& DeviceIt : BindMap)
	{
		for (auto& ChannelIt : DeviceIt.Value)
		{
			float AxisValue;
			if (InputMgr.GetAxis(DeviceIt.Key, ChannelIt.Key, AxisValue))
			{
				UE_LOG(LogDisplayClusterInputAnalog, Verbose, TEXT("Obtained analog data %s:%d => %f"), *DeviceIt.Key, ChannelIt.Key, AxisValue);
				ChannelIt.Value.SetData(AxisValue);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

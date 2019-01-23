// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInput.h"

#include "Misc/CoreDelegates.h"
#include "Features/IModularFeatures.h"
#include "IDisplayCluster.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DisplayClusterInputLog.h"

#include "DisplayClusterInputModule.h"


FDisplayClusterInput::FDisplayClusterInput( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, FDisplayClusterInputModule& InputModuleAPIRef)
	: InputModuleAPI(InputModuleAPIRef)
	, MessageHandler( InMessageHandler )
	, UnrealControllerIndex(0)
{
	IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );
	UE_LOG(LogDisplayClusterInputModule, Log, TEXT("DisplayClusterInput device has been initialized"));
}

FDisplayClusterInput::~FDisplayClusterInput()
{
	IModularFeatures::Get().UnregisterModularFeature( GetModularFeatureName(), this );
}

void FDisplayClusterInput::Tick(float DeltaTime)
{
	if (InputModuleAPI.IsSessionStarted())
	{
		InputModuleAPI.UpdateVrpnBindings();
	}
}

void FDisplayClusterInput::SendControllerEvents()
{
	// Do this code only after StartSession event
	if (InputModuleAPI.IsSessionStarted())
	{
		if (MessageHandler.IsValid())
		{
			InputModuleAPI.SendControllerEvents(MessageHandler, UnrealControllerIndex);
		}
	}
}

void FDisplayClusterInput::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}


bool FDisplayClusterInput::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// No exec commands supported, for now.
	return false;
}

//!
void FDisplayClusterInput::SetChannelValue( int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value )
{
	//dummy
}

void FDisplayClusterInput::SetChannelValues( int32 ControllerId, const FForceFeedbackValues& Values )
{
	//dummy
}

FName FDisplayClusterInput::GetMotionControllerDeviceTypeName() const
{
	const static FName DefaultName(IDisplayClusterInputModule::ModuleName);
	return DefaultName;
}


bool FDisplayClusterInput::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	if (InputModuleAPI.IsSessionStarted() && UnrealControllerIndex == ControllerIndex)
	{

		FRotator trRot;
		FVector  trPos;
		if (InputModuleAPI.GetTrackerController().GetTrackerOrientationAndPosition(DeviceHand, trRot, trPos))
		{
			OutOrientation = trRot;
			OutPosition = trPos;

			return true;
		}
	}
	// This tracker is not binded, ignore
	return false;
}

ETrackingStatus FDisplayClusterInput::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	if (InputModuleAPI.IsSessionStarted() && UnrealControllerIndex == ControllerIndex) // Support multilayer
	{
		if (InputModuleAPI.GetTrackerController().IsTrackerConnected(DeviceHand))
		{
			return ETrackingStatus::Tracked;
		}
	}

	return ETrackingStatus::NotTracked;
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "GenericPlatform/IInputInterface.h"
#include "XRMotionControllerBase.h"

class FDisplayClusterInputModule;


class FDisplayClusterInput :
	public IInputDevice,
	public FXRMotionControllerBase
{
public:
	// Constructor that takes an initial message handler that will receive motion controller events
	FDisplayClusterInput( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, FDisplayClusterInputModule& InputModuleAPI);
	virtual ~FDisplayClusterInput();

public:
	// IInputDevice overrides
	virtual void Tick(float DeltaTime ) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

public:
	// IMotionController overrides
	virtual FName GetMotionControllerDeviceTypeName() const override;
	virtual bool  GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;
	
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override;

private:
	FDisplayClusterInputModule&                   InputModuleAPI;
	TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;
	
	int32 UnrealControllerIndex;  // Local UE4 player index (multi player purpose)
};

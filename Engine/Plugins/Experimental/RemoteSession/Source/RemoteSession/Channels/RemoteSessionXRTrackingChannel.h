// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"
#include "XRTrackingSystemBase.h"

class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;

class REMOTESESSION_API FXRTrackingProxy :
	public FXRTrackingSystemBase
{
public:
	FXRTrackingProxy()
		: FXRTrackingSystemBase(nullptr)
	{}
		
	virtual bool IsTracking(int32 DeviceId) override { return true; }
	virtual bool DoesSupportPositionalTracking() const override { return true; }
	virtual bool IsHeadTrackingAllowed() const override { return true; }
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override { }
	virtual float GetWorldToMetersScale() const override { return 100.f; }

	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual FName GetSystemName() const override;
};

class REMOTESESSION_API FRemoteSessionXRTrackingChannel :
	public IRemoteSessionChannel
{
public:

	FRemoteSessionXRTrackingChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection);

	~FRemoteSessionXRTrackingChannel();

	virtual void Tick(const float InDeltaTime) override;

	/** Sends the current location and rotation for the XRTracking system to the remote */
	void SendXRTracking();

	/** Handles data coming from the client */
	void ReceiveXRTracking(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch);

	/* Begin IRemoteSessionChannel implementation */
	static const TCHAR* StaticType() { return TEXT("FRemoteSessionXRTrackingChannel"); }
	virtual const TCHAR* GetType() const override { return StaticType(); }
	/* End IRemoteSessionChannel implementation */

protected:
	TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> Connection;
	ERemoteSessionChannelMode Role;

	/** If we're sending, this is GEngine->XRSystem. If we are receiving, this is the previous GEngine->XRSystem */
	TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> XRSystem;
	/** Used to set the values from the remote client as the XRTracking's pose */
	TSharedPtr<FXRTrackingProxy, ESPMode::ThreadSafe> ProxyXRSystem;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle MessageCallbackHandle;
};

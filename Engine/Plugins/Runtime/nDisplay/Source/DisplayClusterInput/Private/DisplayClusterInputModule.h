// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "IInputDevice.h"

#include "Controller/DisplayClusterInputControllerButton.h"
#include "Controller/DisplayClusterInputControllerAnalog.h"
#include "Controller/DisplayClusterInputControllerTracker.h"
#include "Controller/DisplayClusterInputControllerKeyboard.h"

class FDisplayClusterInput;

class FDisplayClusterInputModule : public IDisplayClusterInputModule
{
public:
	FDisplayClusterInputModule();
	virtual ~FDisplayClusterInputModule();

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	virtual TSharedPtr< class IInputDevice > CreateInputDevice( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;

public:
	// Add new VRPN device input bind from channel to UE4 target (type auto detected by device ID)
	virtual bool BindVrpnChannel(const FString& VrpnDeviceId, uint32 VrpnChannel, const FString& BindTargetName) override;
	// Bind all keyboard keys to ue4 (default keyboard and|or nDisplay second keyboard namespaces)
	virtual bool SetVrpnKeyboardReflectionMode(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode) override;
	// Apply all delayed vrpn bindings
	void UpdateVrpnBindings();
	// Send vrpn data to UE
	void SendControllerEvents(const TSharedPtr<FGenericApplicationMessageHandler>& MessageHandler, int UnrealControllerIndex);

	bool IsSessionStarted() const
	{ return bIsSessionStarted; }

	const FTrackerController& GetTrackerController() const
	{ return TrackerController; }

protected:
	// Delegated events to nDisplayCluster vrpn input
	void OnDisplayClusterStartSession();
	void OnDisplayClusterEndSession();
	void OnDisplayClusterPreTick();

private:
	// VRPN device controllers
	FButtonController   ButtonController;
	FAnalogController   AnalogController;
	FTrackerController  TrackerController;
	FKeyboardController KeyboardController;
	
	// For easy access
	IDisplayClusterInputController* Controllers[4] = 
	{
		&ButtonController,
		&AnalogController,
		&TrackerController,
		&KeyboardController
	};

	// Runtime variable, true when DisplayCluster session is running
	bool bIsSessionStarted : 1;

private:
	// Run-time binding (delayed execute on tick event)
	struct VrpnChannelBind
	{
		FString VrpnDeviceId;
		uint32  VrpnChannel;
		FString BindTargetName;
		
		VrpnChannelBind(const FString& DelayedVrpnDeviceId, uint32 DelayedVrpnChannel, const FString& DelayedBindTargetName) :
			VrpnDeviceId(DelayedVrpnDeviceId),
			VrpnChannel(DelayedVrpnChannel),
			BindTargetName(DelayedBindTargetName)
		{ }
	};
	// A buffer for binding requests to be processed on tick
	TArray<VrpnChannelBind> DelayedBinds;

	// Run-time keyboard reflection (delayed execute on tick event)
	struct VrpnKeyboardReflect
	{
		FString VrpnDeviceId;
		EDisplayClusterInputKeyboardReflectMode ReflectMode;

		VrpnKeyboardReflect(const FString& DelayedVrpnDeviceId, EDisplayClusterInputKeyboardReflectMode DelayedReflectMode) :
			VrpnDeviceId(DelayedVrpnDeviceId),
			ReflectMode(DelayedReflectMode)
		{ }
	};
	// A buffer for keyboard reflection settings to be processed on tick
	TArray<VrpnKeyboardReflect> DelayedKeyboardReflects;

private:
	TWeakPtr<FDisplayClusterInput> DisplayClusterInputDevice;
};

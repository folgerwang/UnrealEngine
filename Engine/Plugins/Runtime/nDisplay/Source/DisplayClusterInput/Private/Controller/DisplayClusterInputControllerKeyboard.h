// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterInputControllerBase.h"
#include "DisplayClusterInputTypes.h"
#include "IDisplayClusterInputModule.h"

class FGenericApplicationMessageHandler;


class FKeyboardController : public FControllerDeviceBase<EDisplayClusterInputDeviceType::VrpnKeyboard>
{
public:
	virtual ~FKeyboardController()
	{ }

public:
	// Register nDisplay second keyboard keys in UE4 FKey namespace
	virtual void Initialize() override;

	// Delegated events for DisplayClusterInput vrpn connect
	virtual void ProcessStartSession() override;
	virtual void ProcessEndSession() override;
	virtual void ProcessPreTick() override;

	// Reflect vrpn keyboard to UE4
	void ReflectKeyboard(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode);

private:
	// Runtime. Add bind for key by key name (reflect option purpose)
	void ConnectKey(FChannelBinds& KeyboardData, uint32 VrpnChannel, const TCHAR* KeyName);

	// Parse reflection type
	EDisplayClusterInputKeyboardReflectMode ParseReflectionType(const FString& Text, EDisplayClusterInputKeyboardReflectMode DefaultValue) const;

	//  Run-time flags for init
	bool bReflectToUE4             : 1;  // Bind vrpn keyboard to UE4 at OnDisplayClusterStartSession pass
	bool bReflectToNDisplayCluster : 1;  // Bind vrpn keyboard to nDisplay keyboard at OnDisplayClusterStartSession pass
};

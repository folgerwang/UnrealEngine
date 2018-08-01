// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

class IMagicLeapInputDevice;

class IMagicLeapHMD
{
public:
	virtual void RegisterMagicLeapInputDevice(IMagicLeapInputDevice* InputDevice) = 0;
	virtual void UnregisterMagicLeapInputDevice(IMagicLeapInputDevice* InputDevice) = 0;
	virtual bool IsInitialized() const = 0;
};

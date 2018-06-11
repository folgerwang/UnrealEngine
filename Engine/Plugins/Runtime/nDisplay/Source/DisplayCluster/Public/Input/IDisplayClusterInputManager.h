// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Public input manager interface
 */
struct IDisplayClusterInputManager
{
	virtual ~IDisplayClusterInputManager()
	{ }

	//////////////////////////////////////////////////////////////////////////
	// Device amount
	virtual uint32 GetAxisDeviceAmount()    const = 0;
	virtual uint32 GetButtonDeviceAmount()  const = 0;
	virtual uint32 GetTrackerDeviceAmount() const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Device IDs
	virtual bool GetAxisDeviceIds   (TArray<FString>& ids) const = 0;
	virtual bool GetButtonDeviceIds (TArray<FString>& ids) const = 0;
	virtual bool GetTrackerDeviceIds(TArray<FString>& ids) const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Button data access
	virtual bool GetButtonState    (const FString& devId, const uint8 btn, bool& curState)    const = 0;
	virtual bool IsButtonPressed   (const FString& devId, const uint8 btn, bool& curPressed)  const = 0;
	virtual bool IsButtonReleased  (const FString& devId, const uint8 btn, bool& curReleased) const = 0;
	virtual bool WasButtonPressed  (const FString& devId, const uint8 btn, bool& wasPressed)  const = 0;
	virtual bool WasButtonReleased (const FString& devId, const uint8 btn, bool& wasReleased) const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Axes data access
	virtual bool GetAxis(const FString& devId, const uint8 axis, float& value) const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Tracking data access
	virtual bool GetTrackerLocation(const FString& devId, const uint8 tr, FVector& location) const = 0;
	virtual bool GetTrackerQuat(const FString& devId, const uint8 tr, FQuat& rotation) const = 0;
};

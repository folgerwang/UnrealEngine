// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterInputDeviceTraits.h"
#include "IDisplayClusterStringSerializable.h"

#include "Config/DisplayClusterConfigTypes.h"


/**
 * Interface for input devices
 */
struct IDisplayClusterInputDevice
	: public IDisplayClusterStringSerializable
{
	virtual ~IDisplayClusterInputDevice() = 0
	{ }

	virtual FString GetId() const = 0;
	virtual FString GetType() const = 0;
	virtual EDisplayClusterInputDevice GetTypeId() const = 0;
	virtual FDisplayClusterConfigInput GetConfig() const = 0;

	virtual bool Initialize() = 0;
	virtual void PreUpdate() = 0;
	virtual void Update() = 0;
	virtual void PostUpdate() = 0;

	virtual FString ToString() const = 0;
};


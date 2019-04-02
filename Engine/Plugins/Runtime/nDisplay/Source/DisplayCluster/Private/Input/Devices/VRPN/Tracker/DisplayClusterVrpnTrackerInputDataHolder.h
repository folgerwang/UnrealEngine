// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Devices/DisplayClusterInputDeviceTraits.h"
#include "Input/Devices/DisplayClusterInputDeviceBase.h"

#include "CoreMinimal.h"


/**
 * VRPN tracker device data holder. Responsible for data serialization and deserialization.
 */
class FDisplayClusterVrpnTrackerInputDataHolder
	: public FDisplayClusterInputDeviceBase<EDisplayClusterInputDeviceType::VrpnTracker>
{
public:
	FDisplayClusterVrpnTrackerInputDataHolder(const FDisplayClusterConfigInput& config);
	virtual ~FDisplayClusterVrpnTrackerInputDataHolder();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterInputDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Initialize() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterStringSerializable
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString SerializeToString() const override final;
	virtual bool    DeserializeFromString(const FString& data) override final;

private:
	// Serialization constants
	static constexpr auto SerializationDelimiter = TEXT("@");
	static constexpr auto SerializationItems = 3;
};

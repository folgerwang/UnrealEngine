// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Devices/DisplayClusterInputDeviceBase.h"
#include "Input/Devices/DisplayClusterInputDeviceTraits.h"

struct FDisplayClusterConfigInput;


/**
 * VRPN analog device data holder. Responsible for data serialization and deserialization.
 */
class FDisplayClusterVrpnAnalogInputDataHolder
	: public FDisplayClusterInputDeviceBase<EDisplayClusterInputDeviceType::VrpnAnalog>
{
public:
	FDisplayClusterVrpnAnalogInputDataHolder(const FDisplayClusterConfigInput& config);
	virtual ~FDisplayClusterVrpnAnalogInputDataHolder();

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
	static constexpr auto SerializationItems = 2;
};

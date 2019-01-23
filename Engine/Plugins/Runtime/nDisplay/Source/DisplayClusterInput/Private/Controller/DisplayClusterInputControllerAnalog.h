// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Controller/DisplayClusterInputControllerBase.h"


class FAnalogController : public FControllerDeviceBase<EDisplayClusterInputDeviceType::VrpnAnalog>
{
public:
	virtual ~FAnalogController()
	{ }

public:
	virtual void Initialize() override;

	virtual void ProcessStartSession() override;
	virtual void ProcessEndSession() override;
	virtual void ProcessPreTick() override;
};

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "DisplayClusterInputControllerBase.h"


class FButtonController : public FControllerDeviceBase<EDisplayClusterInputDeviceType::VrpnButton>
{
public:
	~FButtonController()
	{ }

public:
	virtual void Initialize() override;

	virtual void ProcessStartSession() override;
	virtual void ProcessEndSession() override;
	virtual void ProcessPreTick() override;
};

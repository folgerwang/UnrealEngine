// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaIOCoreHardwareSync.h"
#include "BlackmagicLib.h"

/**
 * Implementation of HardwareSync for Blackmagic.
 */

class FBlackmagicHardwareSync : public IMediaIOCoreHardwareSync
{
public:
	FBlackmagicHardwareSync(BlackmagicDevice::IPortShared* InPort)
	: Port(InPort)
	{
	}
	virtual bool IsValid() const override
	{
		return Port != nullptr;
	}
	virtual bool WaitVSync() override
	{
		if (Port)
		{
			return Port->WaitVSync();
		}
		return false;
	}
protected:
	BlackmagicDevice::IPortShared* Port;
};

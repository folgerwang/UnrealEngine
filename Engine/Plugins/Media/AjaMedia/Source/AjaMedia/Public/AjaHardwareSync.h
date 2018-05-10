// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaIOCoreHardwareSync.h"
#include "AJALib.h"

/**
 * Implementation of HardwareSync for Aja.
 */

class FAjaHardwareSync : public IMediaIOCoreHardwareSync
{
public:
	FAjaHardwareSync(AJA::AJASyncChannel* InSyncChannel)
	: SyncChannel(InSyncChannel)
	{
	}
	virtual bool IsValid() const override
	{
		return SyncChannel != nullptr;
	}
	virtual bool WaitVSync() override
	{
		if (SyncChannel)
		{
			AJA::FTimecode NewTimecode;
			return SyncChannel->WaitForSync(NewTimecode);
		}
		return false;
	}
protected:
	AJA::AJASyncChannel* SyncChannel;
};

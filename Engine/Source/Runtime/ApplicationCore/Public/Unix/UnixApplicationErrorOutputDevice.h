// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Unix/UnixErrorOutputDevice.h"

struct FUnixApplicationErrorOutputDevice : public FUnixErrorOutputDevice
{
protected:
	virtual void HandleErrorRestoreUI() override;
};

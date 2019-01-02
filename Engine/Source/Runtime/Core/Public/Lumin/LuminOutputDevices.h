// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// Copyright 2017 Magic Leap, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FLuminOutputDevices : public FGenericPlatformOutputDevices
{
	static void	SetupOutputDevices();
};

typedef FLuminOutputDevices FPlatformOutputDevices;

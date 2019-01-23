// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Auxiliary class with different helpers
 */
class DisplayClusterInputHelpers
{
public:
	/** 
	 * Converts keyboard button name (defined inside UE4 core) to a scancode (equals to a keyboard vrpn channel)
	 */
	static bool KeyNameToVrpnScancode(const FString& KeyName, int32& VrpnScanCode);
};

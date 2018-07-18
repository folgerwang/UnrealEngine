// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Current state of rig
*	What  state Control Rig currently is
*/
UENUM()
enum class EControlRigState : uint8
{
	Init,
	Update,
	Invalid,
};

/** Execution context that rig units use */
struct FRigUnitContext
{
	/** The current delta time */
	float DeltaTime;

	/** Current execution context */
	EControlRigState State;
};


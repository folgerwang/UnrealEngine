// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AngularLimit.h: Angular limit features
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "CommonAnimTypes.h"

namespace AnimationCore
{
	ANIMATIONCORE_API bool ConstrainAngularRangeUsingEuler(FQuat& InOutQuatRotation, const FQuat& InRefRotation, const FVector& InLimitMinDegrees, const FVector& InLimitMaxDegrees);
}
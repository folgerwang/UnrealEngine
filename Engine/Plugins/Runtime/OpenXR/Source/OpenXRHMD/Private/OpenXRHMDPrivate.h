// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Windows/D3D11RHI/Private/D3D11RHIPrivate.h"

#if PLATFORM_WINDOWS
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define XR_ENSURE(x) ensure(XR_SUCCEEDED(x))

FORCEINLINE FQuat ToFQuat(XrQuaternionf Quat)
{
	return FQuat(-Quat.z, Quat.x, Quat.y, -Quat.w);
}

FORCEINLINE XrQuaternionf ToXrQuat(FQuat Quat)
{
	return XrQuaternionf{ Quat.Y, Quat.Z, -Quat.X, -Quat.W };
}

FORCEINLINE FVector ToFVector(XrVector3f Vector, float Scale = 1.0f)
{
	return FVector(-Vector.z * Scale, Vector.x * Scale, Vector.y * Scale);
}

FORCEINLINE XrVector3f ToXrVector(FVector Vector, float Scale = 1.0f)
{
	if (Vector.IsZero())
		return XrVector3f{ 0.0f, 0.0f, 0.0f };

	return XrVector3f{ Vector.Y / Scale, Vector.Z / Scale, -Vector.X / Scale };
}

FORCEINLINE FTransform ToFTransform(XrPosef Transform, float Scale = 1.0f)
{
	return FTransform(ToFQuat(Transform.orientation), ToFVector(Transform.position, Scale));
}

FORCEINLINE XrPosef ToXrPose(FTransform Transform, float Scale = 1.0f)
{
	return XrPosef{ ToXrQuat(Transform.GetRotation()), ToXrVector(Transform.GetTranslation(), Scale) };
}

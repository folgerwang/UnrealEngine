// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/KismetMathLibrary.h"
#include "CoreMinimal.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_types.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

namespace MagicLeap
{
	static const float kIdentityMatColMajor[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
#if WITH_MLSDK
	static const MLTransform kIdentityTransform = { { { { 0, 0, 0, 1 } } },{ { { 0, 0, 0 } } } };

	FORCEINLINE FVector ToFVector(const MLVec3f& InVec3f, float WorldToMetersScale)
	{
		return FVector(-InVec3f.xyz.z * WorldToMetersScale, InVec3f.xyz.x * WorldToMetersScale, InVec3f.xyz.y * WorldToMetersScale);
	}

	FORCEINLINE MLVec3f ToMLVector(const FVector& InVector, float WorldToMetersScale)
	{
		float InverseScale = 1.0f / WorldToMetersScale;
		MLVec3f vec;
		vec.xyz.x = InVector.Y * InverseScale;
		vec.xyz.y = InVector.Z * InverseScale;
		vec.xyz.z = -InVector.X * InverseScale;
		return vec;
	}

	FORCEINLINE MLVec3f ToMLVectorNoScale(const FVector& InVector)
	{
		return ToMLVector(InVector, 1.0f);
	}

	FORCEINLINE FQuat ToFQuat(const MLQuaternionf& InQuat)
	{
		return FQuat(-InQuat.z, InQuat.x, InQuat.y, -InQuat.w);
	}

	FORCEINLINE MLQuaternionf ToMLQuat(const FQuat& InQuat)
	{
		MLQuaternionf quat;
		quat.x = InQuat.Y;
		quat.y = InQuat.Z;
		quat.z = -InQuat.X;
		quat.w = -InQuat.W;
		return quat;
	}

	FORCEINLINE FTransform ToFTransform(const MLTransform& InTransform, float WorldToMetersScale)
	{
		return FTransform(ToFQuat(InTransform.rotation), ToFVector(InTransform.position, WorldToMetersScale), FVector(1.0f, 1.0f, 1.0f));
	}

	FORCEINLINE MLTransform ToMLTransform(const FTransform& InTransform, float WorldToMetersScale)
	{
		MLTransform transform = { { { { 0, 0, 0, 1 } } }, { { { 0, 0, 0 } } } };
		transform.position = ToMLVector(InTransform.GetLocation(), WorldToMetersScale);
		transform.rotation = ToMLQuat(InTransform.GetRotation());
		return transform;
	}

	FORCEINLINE FMatrix ToFMatrix(const MLMat4f& InMat4f)
	{
		// FTransform and FMatrix have a reversed multiplication order as opposed to column major.
		// Note that unreal is left handed and graphics is right handed so the conversion is applied here to the whole column.
		return FMatrix(
			FPlane(InMat4f.matrix_colmajor[0], InMat4f.matrix_colmajor[1], InMat4f.matrix_colmajor[2], InMat4f.matrix_colmajor[3]),
			FPlane(InMat4f.matrix_colmajor[4], InMat4f.matrix_colmajor[5], InMat4f.matrix_colmajor[6], InMat4f.matrix_colmajor[7]),
			FPlane(-InMat4f.matrix_colmajor[8], -InMat4f.matrix_colmajor[9], -InMat4f.matrix_colmajor[10], -InMat4f.matrix_colmajor[11]),
			FPlane(InMat4f.matrix_colmajor[12], InMat4f.matrix_colmajor[13], InMat4f.matrix_colmajor[14], InMat4f.matrix_colmajor[15])
		);
	}
#endif //WITH_MLSDK

	FORCEINLINE FQuat ToUERotator(const FQuat& InMLRotation)
	{
		return FQuat(UKismetMathLibrary::GetRightVector(InMLRotation.Rotator()), PI) * InMLRotation;
	}
}

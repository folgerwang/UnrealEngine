// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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

// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 201x Magic Leap, Inc. (COMPANY) All Rights Reserved.
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

#include "MagicLeapUtils.h"
#include "MagicLeapMath.h"
#include "HAL/UnrealMemory.h"

namespace MagicLeap
{
#if WITH_MLSDK
	void ResetClipExtentsInfoArray(MLGraphicsClipExtentsInfoArray& UpdateInfoArray)
	{
		UpdateInfoArray.num_virtual_cameras = 0;
		for (MLGraphicsClipExtentsInfo &ViewportInfo : UpdateInfoArray.virtual_camera_extents)
		{
			FMemory::Memcpy(ViewportInfo.projection.matrix_colmajor, MagicLeap::kIdentityMatColMajor, sizeof(MagicLeap::kIdentityMatColMajor));
			FMemory::Memcpy(&ViewportInfo.transform, &MagicLeap::kIdentityTransform, sizeof(MagicLeap::kIdentityTransform));
		}

		FMemory::Memcpy(UpdateInfoArray.full_extents.projection.matrix_colmajor, MagicLeap::kIdentityMatColMajor, sizeof(MagicLeap::kIdentityMatColMajor));
		FMemory::Memcpy(&UpdateInfoArray.full_extents.transform, &MagicLeap::kIdentityTransform, sizeof(MagicLeap::kIdentityTransform));
	}

	void ResetVirtualCameraInfoArray(MLGraphicsVirtualCameraInfoArray& RenderInfoArray)
	{
		RenderInfoArray.num_virtual_cameras = 0;
		RenderInfoArray.color_id = 0;
		RenderInfoArray.depth_id = 0;
		RenderInfoArray.viewport.x = 0;
		RenderInfoArray.viewport.y = 0;
		RenderInfoArray.viewport.w = 0;
		RenderInfoArray.viewport.h = 0;
		for (MLGraphicsVirtualCameraInfo &ViewportInfo : RenderInfoArray.virtual_cameras)
		{
			FMemory::Memcpy(ViewportInfo.projection.matrix_colmajor, MagicLeap::kIdentityMatColMajor, sizeof(MagicLeap::kIdentityMatColMajor));
			FMemory::Memcpy(&ViewportInfo.transform, &MagicLeap::kIdentityTransform, sizeof(MagicLeap::kIdentityTransform));
		}
	}
#endif //WITH_MLSDK
}

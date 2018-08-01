// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

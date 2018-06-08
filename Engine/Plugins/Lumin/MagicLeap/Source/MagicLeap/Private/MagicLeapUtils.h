// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_graphics.h>
ML_INCLUDES_END

namespace MagicLeap
{
	// Clears the extents info and puts it into a safe initial state.
	void ResetClipExtentsInfoArray(MLGraphicsClipExtentsInfoArray& UpdateInfoArray);

	// Clears the virtual camera info and puts it into a safe initial state.
	void ResetVirtualCameraInfoArray(MLGraphicsVirtualCameraInfoArray& RenderInfoArray);
}
#endif //WITH_MLSDK

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_MLSDK

#include "MagicLeapGraphics.h"

namespace MagicLeap
{
	// Clears the extents info and puts it into a safe initial state.
	void ResetClipExtentsInfoArray(MLGraphicsClipExtentsInfoArray& UpdateInfoArray);

	// Clears the virtual camera info and puts it into a safe initial state.
	void ResetVirtualCameraInfoArray(MLGraphicsVirtualCameraInfoArray& RenderInfoArray);
}
#endif //WITH_MLSDK

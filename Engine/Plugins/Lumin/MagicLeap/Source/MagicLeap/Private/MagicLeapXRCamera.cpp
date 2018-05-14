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

#include "MagicLeapXRCamera.h"
#include "SceneView.h"
#include "MagicLeapCustomPresent.h"
#include "MagicLeapHMD.h"
#include "IMagicLeapPlugin.h"

FMagicLeapXRCamera::FMagicLeapXRCamera(const FAutoRegister& AutoRegister, FMagicLeapHMD& InMagicLeapSystem, int32 InDeviceID)
	: FDefaultXRCamera(AutoRegister, &InMagicLeapSystem, InDeviceID)
	, MagicLeapSystem(InMagicLeapSystem)
{
}

void FMagicLeapXRCamera::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
#if WITH_MLSDK
	// this needs to happen before the FDefaultXRCamera call, because UpdateProjectionMatrix is somewhat destructive. 
	if (View.StereoPass != eSSP_FULL)
	{
		const int EyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
		FTrackingFrame* const Frame = MagicLeapSystem.GetCurrentFrame();

		// update to use render projection matrix
		// #todo: Roll UpdateProjectionMatrix into UpdateViewMatrix?
		FMatrix RenderInfoProjectionMatrix = MagicLeap::ToFMatrix(Frame->RenderInfoArray.virtual_cameras[EyeIdx].projection);

		// Set the near clipping plane to GNearClippingPlane which is clamped to the minimum value allowed for the device. (ref: MLGraphicsGetRenderTargets())
		RenderInfoProjectionMatrix.M[3][2] = GNearClippingPlane;

		View.UpdateProjectionMatrix(RenderInfoProjectionMatrix);
	}

	FDefaultXRCamera::PreRenderView_RenderThread(RHICmdList, View);
#endif //WITH_MLSDK
}

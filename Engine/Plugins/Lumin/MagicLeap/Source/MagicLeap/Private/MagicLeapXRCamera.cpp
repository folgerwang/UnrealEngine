// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
		const FTrackingFrame& Frame = MagicLeapSystem.GetCurrentFrame();

		// update to use render projection matrix
		// #todo: Roll UpdateProjectionMatrix into UpdateViewMatrix?
		FMatrix RenderInfoProjectionMatrix = MagicLeap::ToFMatrix(Frame.RenderInfoArray.virtual_cameras[EyeIdx].projection);

		// Set the near clipping plane to GNearClippingPlane which is clamped to the minimum value allowed for the device. (ref: MLGraphicsGetRenderTargets())
		RenderInfoProjectionMatrix.M[3][2] = GNearClippingPlane;

		View.UpdateProjectionMatrix(RenderInfoProjectionMatrix);
	}

	FDefaultXRCamera::PreRenderView_RenderThread(RHICmdList, View);
#endif //WITH_MLSDK
}

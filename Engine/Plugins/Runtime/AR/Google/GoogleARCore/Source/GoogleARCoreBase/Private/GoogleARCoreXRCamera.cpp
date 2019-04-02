// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreXRCamera.h"
#include "GoogleARCoreXRTrackingSystem.h"
#include "SceneView.h"
#include "GoogleARCorePassthroughCameraRenderer.h"
#include "GoogleARCoreAndroidHelper.h"

FGoogleARCoreXRCamera::FGoogleARCoreXRCamera(const FAutoRegister& AutoRegister, FGoogleARCoreXRTrackingSystem& InARCoreSystem, int32 InDeviceID)
	: FDefaultXRCamera(AutoRegister, &InARCoreSystem, InDeviceID)
	, GoogleARCoreTrackingSystem(InARCoreSystem)
	, bMatchDeviceCameraFOV(false)
	, bEnablePassthroughCameraRendering_RT(false)
{
	PassthroughRenderer = new FGoogleARCorePassthroughCameraRenderer();
}

void FGoogleARCoreXRCamera::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	TrackingSystem->GetCurrentPose(DeviceId, InView.BaseHmdOrientation, InView.BaseHmdLocation);
}

void FGoogleARCoreXRCamera::SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData)
{
	if (GoogleARCoreTrackingSystem.ARCoreDeviceInstance->GetIsARCoreSessionRunning() && bMatchDeviceCameraFOV)
	{
		FIntRect ViewRect = InOutProjectionData.GetViewRect();
		InOutProjectionData.ProjectionMatrix = GoogleARCoreTrackingSystem.ARCoreDeviceInstance->GetPassthroughCameraProjectionMatrix(ViewRect.Size());
	}
}

void FGoogleARCoreXRCamera::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	PassthroughRenderer->InitializeOverlayMaterial();
}

void FGoogleARCoreXRCamera::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
}

void FGoogleARCoreXRCamera::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	FGoogleARCoreXRTrackingSystem& TS = GoogleARCoreTrackingSystem;

	if (TS.ARCoreDeviceInstance->GetIsARCoreSessionRunning() && bEnablePassthroughCameraRendering_RT)
	{
		PassthroughRenderer->InitializeRenderer_RenderThread(TS.ARCoreDeviceInstance->GetPassthroughCameraTexture());
	}
}

void FGoogleARCoreXRCamera::PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	TArray<FVector2D> Tmp;
	if (GetPassthroughCameraUVs_RenderThread(Tmp))
	{
		PassthroughRenderer->RenderVideoOverlay_RenderThread(RHICmdList, InView);
	}
}

bool FGoogleARCoreXRCamera::GetPassthroughCameraUVs_RenderThread(TArray<FVector2D>& OutUVs)
{
	if (GoogleARCoreTrackingSystem.ARCoreDeviceInstance->GetIsARCoreSessionRunning() && bEnablePassthroughCameraRendering_RT)
	{
		TArray<float> TransformedUVs;
		// TODO save the transformed UVs and only calculate if uninitialized or FGoogleARCoreFrame::IsDisplayRotationChanged() returns true
		GoogleARCoreTrackingSystem.ARCoreDeviceInstance->GetPassthroughCameraImageUVs(PassthroughRenderer->OverlayQuadUVs, TransformedUVs);
		PassthroughRenderer->UpdateOverlayUVCoordinate_RenderThread(TransformedUVs, FGoogleARCoreAndroidHelper::GetDisplayRotation());
		OutUVs.SetNumUninitialized(4);
		OutUVs[0] = FVector2D(TransformedUVs[0], TransformedUVs[1]);
		OutUVs[1] = FVector2D(TransformedUVs[2], TransformedUVs[3]);
		OutUVs[2] = FVector2D(TransformedUVs[4], TransformedUVs[5]);
		OutUVs[3] = FVector2D(TransformedUVs[6], TransformedUVs[7]);
		return true;
	}
	else
	{
		return false;
	}
}

bool FGoogleARCoreXRCamera::IsActiveThisFrame(class FViewport* InViewport) const
{
	return GoogleARCoreTrackingSystem.IsHeadTrackingAllowed();
}

void FGoogleARCoreXRCamera::ConfigXRCamera(bool bInMatchDeviceCameraFOV, bool bInEnablePassthroughCameraRendering)
{
	bMatchDeviceCameraFOV = bInMatchDeviceCameraFOV;
	FGoogleARCoreXRCamera* ARCoreXRCamera = this;
	ENQUEUE_RENDER_COMMAND(ConfigXRCamera)(
		[ARCoreXRCamera, bInEnablePassthroughCameraRendering](FRHICommandListImmediate& RHICmdList)
		{
			ARCoreXRCamera->bEnablePassthroughCameraRendering_RT = bInEnablePassthroughCameraRendering;
		}
	);
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultXRCamera.h"
#include "MagicLeapHMD.h"

/**
  * MagicLeap CR Camera
  */
class MAGICLEAP_API FMagicLeapXRCamera : public FDefaultXRCamera
{
public:
	FMagicLeapXRCamera(const FAutoRegister&, FMagicLeapHMD& InMagicLeapSystem, int32 InDeviceID);

	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) override;

private:
	FMagicLeapHMD& MagicLeapSystem;
};

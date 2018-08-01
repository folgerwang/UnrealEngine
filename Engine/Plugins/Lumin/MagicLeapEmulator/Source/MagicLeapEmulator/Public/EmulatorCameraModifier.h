// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraModifier.h"
#include "EmulatorCameraModifier.generated.h"

class UTextureRenderTarget2D;

/** CameraModifier used to inject postprocess blending for the ML emulator. */
UCLASS(ClassGroup = MagicLeapEmulator)
class MAGICLEAPEMULATOR_API UEmulatorCameraModifier : public UCameraModifier
{
  GENERATED_BODY()

public:
	UEmulatorCameraModifier();

	void InitForEmulation(UTextureRenderTarget2D* BGRenderTarget_LeftOrFull, UTextureRenderTarget2D* BGRenderTarget_Right);

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;

	UPROPERTY()
	UMaterialInstanceDynamic* CompositingMatInst;

private:
	FPostProcessSettings EmulatorPPSettings;
};


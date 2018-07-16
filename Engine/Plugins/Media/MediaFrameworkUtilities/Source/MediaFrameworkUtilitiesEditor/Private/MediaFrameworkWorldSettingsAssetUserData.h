// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"

#include "MediaFrameworkWorldSettingsAssetUserData.generated.h"

class ACameraActor;
class UMediaOutput;
class UTextureRenderTarget2D;

/**
 * FMediaFrameworkCaptureCameraViewportCameraOutputInfo
 */
USTRUCT()
struct FMediaFrameworkCaptureCameraViewportCameraOutputInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	TArray<ACameraActor*> LockedCameraActors;

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	UMediaOutput* MediaOutput;
};

/**
 * FMediaFrameworkCaptureRenderTargetCameraOutputInfo
 */
USTRUCT()
struct FMediaFrameworkCaptureRenderTargetCameraOutputInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=MediaRenderTargetCapture)
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(EditAnywhere, Category=MediaRenderTargetCapture)
	UMediaOutput* MediaOutput;
};

/**
 * UMediaFrameworkCaptureCameraViewportAssetUserData
 */
UCLASS(MinimalAPI)
class UMediaFrameworkWorldSettingsAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="MediaRenderTargetCapture", meta=(ShowOnlyInnerProperties))
	TArray<FMediaFrameworkCaptureRenderTargetCameraOutputInfo> RenderTargetCaptures;

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture", meta=(ShowOnlyInnerProperties))
	TArray<FMediaFrameworkCaptureCameraViewportCameraOutputInfo> ViewportCaptures;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "UObject/LazyObjectPtr.h"

#include "MediaFrameworkWorldSettingsAssetUserData.generated.h"

class UMediaFrameworkWorldSettingsAssetUserData;
class UMediaOutput;
class UTextureRenderTarget2D;

/**
 * FMediaFrameworkCaptureCameraViewportCameraOutputInfo
 */
USTRUCT()
struct FMediaFrameworkCaptureCameraViewportCameraOutputInfo
{
	GENERATED_BODY()

	FMediaFrameworkCaptureCameraViewportCameraOutputInfo();

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture", meta=(DisplayName="Cameras"))
	TArray<TLazyObjectPtr<AActor>> LockedActors;

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	UMediaOutput* MediaOutput;

	UPROPERTY()
	TEnumAsByte<EViewModeIndex> ViewMode;

private:
	//DEPRECATED 4.21 The type of LockedCameraActors has changed and will be removed from the code base in a future release. Use LockedActors.
	UPROPERTY()
	TArray<AActor*> LockedCameraActors_DEPRECATED;
	friend UMediaFrameworkWorldSettingsAssetUserData;
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

public:
	virtual void Serialize(FArchive& Ar) override;
};

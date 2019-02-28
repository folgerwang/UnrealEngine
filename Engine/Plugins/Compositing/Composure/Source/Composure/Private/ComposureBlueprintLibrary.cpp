// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ComposureBlueprintLibrary.h"

#include "UObject/Package.h"
#include "Public/Slate/SceneViewport.h"
#include "Classes/Components/SceneCaptureComponent2D.h"
#include "Classes/Camera/PlayerCameraManager.h"
#include "Classes/GameFramework/PlayerController.h"
#include "Classes/Engine/LocalPlayer.h"

#include "ComposurePlayerCompositingTarget.h"
#include "ComposureUtils.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"


UComposureBlueprintLibrary::UComposureBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


// static
UComposurePlayerCompositingTarget* UComposureBlueprintLibrary::CreatePlayerCompositingTarget(UObject* WorldContextObject)
{
	UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
	return NewObject< UComposurePlayerCompositingTarget>(Outer);
}

// static
void UComposureBlueprintLibrary::GetProjectionMatrixFromPostMoveSettings(
	const FComposurePostMoveSettings& PostMoveSettings, float HorizontalFOVAngle, float AspectRatio, FMatrix& ProjectionMatrix)
{
	ProjectionMatrix = PostMoveSettings.GetProjectionMatrix(HorizontalFOVAngle, AspectRatio);
}

// static
void UComposureBlueprintLibrary::GetCroppingUVTransformationMatrixFromPostMoveSettings(
	const FComposurePostMoveSettings& PostMoveSettings, float AspectRatio,
	FMatrix& CropingUVTransformationMatrix, FMatrix& UncropingUVTransformationMatrix)
{
	PostMoveSettings.GetCroppingUVTransformationMatrix(AspectRatio, &CropingUVTransformationMatrix, &UncropingUVTransformationMatrix);
}

// static
void UComposureBlueprintLibrary::GetRedGreenUVFactorsFromChromaticAberration(
	float ChromaticAberrationAmount, FVector2D& RedGreenUVFactors)
{
	RedGreenUVFactors = FComposureUtils::GetRedGreenUVFactorsFromChromaticAberration(
		FMath::Clamp(ChromaticAberrationAmount, 0.f, 1.f));
}

//static
void UComposureBlueprintLibrary::GetPlayerDisplayGamma(const APlayerCameraManager* PlayerCameraManager, float& DisplayGamma)
{
	DisplayGamma = 0;
	if (!PlayerCameraManager)
	{
		return;
	}

	UGameViewportClient* ViewportClient = PlayerCameraManager->PCOwner->GetLocalPlayer()->ViewportClient;
	if (!ViewportClient)
	{
		return;
	}

	FSceneViewport* SceneViewport = ViewportClient->GetGameViewport();

	DisplayGamma = SceneViewport ? SceneViewport->GetDisplayGamma() : 0.0;
}

void UComposureBlueprintLibrary::CopyCameraSettingsToSceneCapture(UCameraComponent* Src, USceneCaptureComponent2D* Dst)
{
	if (Src && Dst)
	{
		Dst->SetWorldLocationAndRotation(Src->GetComponentLocation(), Src->GetComponentRotation());
		Dst->FOVAngle = Src->FieldOfView;

		FMinimalViewInfo CameraViewInfo;
		Src->GetCameraView(/*DeltaTime =*/0.0f, CameraViewInfo);

		const FPostProcessSettings& SrcPPSettings = CameraViewInfo.PostProcessSettings;
		FPostProcessSettings& DstPPSettings = Dst->PostProcessSettings;

#define COPY_PPSETTING(FieldName, ToggleName) \
		DstPPSettings.FieldName  = SrcPPSettings.FieldName; \
		DstPPSettings.ToggleName = true;

		COPY_PPSETTING(WhiteTemp, bOverride_WhiteTemp);
		
		COPY_PPSETTING(ColorSaturation, bOverride_ColorSaturation);
		COPY_PPSETTING(ColorContrast, bOverride_ColorContrast);
		COPY_PPSETTING(ColorGamma, bOverride_ColorGamma);
		COPY_PPSETTING(ColorGain, bOverride_ColorGain);
		COPY_PPSETTING(ColorOffset, bOverride_ColorOffset);

		COPY_PPSETTING(ColorSaturationShadows, bOverride_ColorSaturationShadows);
		COPY_PPSETTING(ColorContrastShadows, bOverride_ColorContrastShadows);
		COPY_PPSETTING(ColorGammaShadows, bOverride_ColorGammaShadows);
		COPY_PPSETTING(ColorGainShadows, bOverride_ColorGainShadows);
		COPY_PPSETTING(ColorOffsetShadows, bOverride_ColorOffsetShadows);

		COPY_PPSETTING(ColorCorrectionShadowsMax, bOverride_ColorCorrectionShadowsMax);
		
		COPY_PPSETTING(ColorSaturationMidtones, bOverride_ColorSaturationMidtones);
		COPY_PPSETTING(ColorContrastMidtones, bOverride_ColorContrastMidtones);
		COPY_PPSETTING(ColorGammaMidtones, bOverride_ColorGammaMidtones);
		COPY_PPSETTING(ColorGainMidtones, bOverride_ColorGainMidtones);
		COPY_PPSETTING(ColorOffsetMidtones, bOverride_ColorOffsetMidtones);
		
		COPY_PPSETTING(ColorSaturationHighlights, bOverride_ColorSaturationHighlights);
		COPY_PPSETTING(ColorContrastHighlights, bOverride_ColorContrastHighlights);
		COPY_PPSETTING(ColorGammaHighlights, bOverride_ColorGammaHighlights);
		COPY_PPSETTING(ColorGainHighlights, bOverride_ColorGainHighlights);
		COPY_PPSETTING(ColorOffsetHighlights, bOverride_ColorOffsetHighlights);

		COPY_PPSETTING(ColorCorrectionHighlightsMin, bOverride_ColorCorrectionHighlightsMin);
			
		COPY_PPSETTING(DepthOfFieldFstop, bOverride_DepthOfFieldFstop);
		COPY_PPSETTING(DepthOfFieldMinFstop, bOverride_DepthOfFieldMinFstop);
		COPY_PPSETTING(DepthOfFieldBladeCount, bOverride_DepthOfFieldBladeCount);
		COPY_PPSETTING(AutoExposureBias, bOverride_AutoExposureBias);
		COPY_PPSETTING(AutoExposureBiasCurve, bOverride_AutoExposureBiasCurve);
			
		COPY_PPSETTING(DepthOfFieldSensorWidth, bOverride_DepthOfFieldSensorWidth);
		COPY_PPSETTING(DepthOfFieldFocalDistance, bOverride_DepthOfFieldFocalDistance);
		COPY_PPSETTING(DepthOfFieldDepthBlurAmount, bOverride_DepthOfFieldDepthBlurAmount);
		COPY_PPSETTING(DepthOfFieldDepthBlurRadius, bOverride_DepthOfFieldDepthBlurRadius);

#undef COPY_PPSETTING
	}
}

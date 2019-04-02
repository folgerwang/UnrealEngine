// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "MovieSceneCaptureProtocolBase.h"
#include "CompositionGraphCaptureProtocol.generated.h"

class FSceneViewport;
class UMaterialInterface;

struct FMovieSceneCaptureSettings;
struct FFrameCaptureViewExtension;

/** Used by UCompositionGraphCaptureSettings. Matches gamut oreder in TonemapCommon.usf OuputGamutMappingMatrix()*/
UENUM(BlueprintType)
enum EHDRCaptureGamut
{
	HCGM_Rec709 UMETA(DisplayName = "Rec.709 / sRGB"),
	HCGM_P3DCI UMETA(DisplayName = "P3 D65"),
	HCGM_Rec2020 UMETA(DisplayName = "Rec.2020"),
	HCGM_ACES UMETA(DisplayName = "ACES"),
	HCGM_ACEScg UMETA(DisplayName = "ACEScg"),
	HCGM_Linear UMETA(DisplayName = "Linear"),
	HCGM_MAX,
};

USTRUCT(BlueprintType)
struct MOVIESCENECAPTURE_API FCompositionGraphCapturePasses
{
	GENERATED_BODY()

	/** List of passes to record by name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composition Graph Settings")
	TArray<FString> Value;
};

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Custom Render Passes", CommandLineID="CustomRenderPasses"))
class MOVIESCENECAPTURE_API UCompositionGraphCaptureProtocol : public UMovieSceneImageCaptureProtocolBase
{
public:
	GENERATED_BODY()

	UCompositionGraphCaptureProtocol(const FObjectInitializer& Init)
		: Super(Init)
		, bDisableScreenPercentage(true)
	{}

	/** A list of render passes to include in the capture. Leave empty to export all available passes. */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Composition Graph Options")
	FCompositionGraphCapturePasses IncludeRenderPasses;

	/** Whether to capture the frames as HDR textures (*.exr format) */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Composition Graph Options")
	bool bCaptureFramesInHDR;

	/** Compression Quality for HDR Frames (0 for no compression, 1 for default compression which can be slow) */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Composition Graph Options", meta = (EditCondition = "bCaptureFramesInHDR", UIMin=0, ClampMin=0, UIMax=1, ClampMax=1))
	int32 HDRCompressionQuality;

	/** The color gamut to use when storing HDR captured data. The gamut depends on whether the bCaptureFramesInHDR option is enabled. */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Composition Graph Options", meta = (EditCondition = "bCaptureFramesInHDR"))
	TEnumAsByte<enum EHDRCaptureGamut> CaptureGamut;

	/** Custom post processing material to use for rendering */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Composition Graph Options", meta=(AllowedClasses=""))
	FSoftObjectPath PostProcessingMaterial;

	/** Whether to disable screen percentage */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Composition Graph Options")
	bool bDisableScreenPercentage;

public:

	/**~ UMovieSceneCaptureProtocolBase implementation */
	virtual bool SetupImpl();
	virtual void CaptureFrameImpl(const FFrameMetrics& FrameMetrics);
	virtual void TickImpl() override;
	virtual void FinalizeImpl() override;
	virtual bool HasFinishedProcessingImpl() const override;
	virtual void OnReleaseConfigImpl(FMovieSceneCaptureSettings& InSettings) override;
	virtual void OnLoadConfigImpl(FMovieSceneCaptureSettings& InSettings) override;
	/**~ End UMovieSceneCaptureProtocolBase implementation */

private:

	UPROPERTY(transient)
	UMaterialInterface* PostProcessingMaterialPtr;

	/** The viewport we are capturing from */
	TWeakPtr<FSceneViewport> SceneViewport;

	/** A view extension that we use to ensure we dump out the composition graph frames with the correct settings */
	TSharedPtr<FFrameCaptureViewExtension, ESPMode::ThreadSafe> ViewExtension;
};

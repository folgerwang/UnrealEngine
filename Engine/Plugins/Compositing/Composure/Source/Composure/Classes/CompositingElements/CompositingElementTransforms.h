// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CompositingElements/CompositingElementPasses.h"
#include "CompositingElements/CompositingMaterialPass.h"
#include "Engine/Scene.h" // for FColorGradingSettings, FFilmStockSettings
#include "OpenColorIOColorSpace.h"

#include "CompositingElementTransforms.generated.h"

/* UCompositingPostProcessPass
 *****************************************************************************/

class FCompositingTargetSwapChain;
class UComposurePostProcessPassPolicy;

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Post Process Pass Set"))
class COMPOSURE_API UCompositingPostProcessPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	float RenderScale = 1.f;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Compositing Pass", meta = (DisplayAfter = "RenderScale"/*, EditCondition = "bEnabled"*/))
	TArray<UComposurePostProcessPassPolicy*> PostProcessPasses;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

protected:
	void RenderPostPassesToSwapChain(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, FCompositingTargetSwapChain& TargetSwapChain);
};

/* UCompositingElementMaterialPass
 *****************************************************************************/

UCLASS(BlueprintType, Blueprintable, editinlinenew, meta = (DisplayName = "Custom Material Pass"))
class COMPOSURE_API UCompositingElementMaterialPass : public UCompositingPostProcessPass
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (ShowOnlyInnerProperties, DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FCompositingMaterial Material;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;
	//~ End UCompositingElementTransform interface

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void ApplyMaterialParams(UMaterialInstanceDynamic* MID);
};

/* UCompositingTonemapPass
 *****************************************************************************/

class UComposureTonemapperPassPolicy;

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Tonemap"))
class COMPOSURE_API UCompositingTonemapPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	/** Color grading settings. */
	UPROPERTY(Interp, Category = "Compositing Pass", meta = (ShowOnlyInnerProperties, DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FColorGradingSettings ColorGradingSettings;
	
	/** Film stock settings. */
	UPROPERTY(Interp, Category = "Compositing Pass", meta = (ShowOnlyInnerProperties, DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FFilmStockSettings FilmStockSettings;

	/** in percent, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(Interp, Category = "Compositing Pass", meta = (UIMin = "0.0", UIMax = "5.0", DisplayAfter = "PassName", EditCondition = "bEnabled"))
	float ChromaticAberration = 0.0f;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

private:
	UPROPERTY(Transient, DuplicateTransient, SkipSerialization)
	UComposureTonemapperPassPolicy* TonemapPolicy;
};

/* UMultiPassChromaKeyer
 *****************************************************************************/

class UMediaBundle;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Multi Pass Chroma Keyer"))
class UMultiPassChromaKeyer : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UMultiPassChromaKeyer();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TArray<FLinearColor> KeyColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compositing Pass", meta = (ShowOnlyInnerProperties, DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FCompositingMaterial KeyerMaterial;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

private:
	UPROPERTY(Transient)
	UTexture* DefaultWhiteTexture = nullptr;
};


/* UMultiPassDespill
 *****************************************************************************/

class UMediaBundle;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Multi Pass Despill"))
class UMultiPassDespill : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UMultiPassDespill();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TArray<FLinearColor> KeyColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compositing Pass", meta = (ShowOnlyInnerProperties, DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FCompositingMaterial KeyerMaterial;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

private:
	UPROPERTY(Transient)
	UTexture* DefaultWhiteTexture = nullptr;
};

/* UAlphaTransformPass
 *****************************************************************************/

class UMaterialInstanceDynamic;

UCLASS(noteditinlinenew, hidedropdown)
class UAlphaTransformPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UAlphaTransformPass();

	UPROPERTY(EditAnywhere, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	float AlphaScale = 1.f;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;
	//~ End UCompositingElementTransform interface

private:
	UPROPERTY(Transient)
	UMaterialInterface* DefaultMaterial;
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* AlphaTransformMID;
};


/* UCompositingOpenColorIOPass
*****************************************************************************/

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "OpenColorIO"))
class COMPOSURE_API UCompositingOpenColorIOPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	/** Color grading settings. */
	UPROPERTY(Interp, Category = "OpenColorIO Settings", meta = (ShowOnlyInnerProperties, DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FOpenColorIOColorConversionSettings ColorConversionSettings;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;
};
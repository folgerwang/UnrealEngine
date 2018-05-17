// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "Components/LocalLightComponent.h"
#include "RectLightComponent.generated.h"

class FLightSceneProxy;

/**
 * A light component which emits light from a rectangle.
 */
UCLASS(Blueprintable, ClassGroup=(Lights,Common), hidecategories=(Object, LightShafts), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API URectLightComponent : public ULocalLightComponent
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Width of light source rect.
	 * Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	float SourceWidth;

	/** 
	 * Height of light source rect.
	 * Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	float SourceHeight;

	/** Texture mapped to the light source rectangle */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Light, AdvancedDisplay)
	//class UTexture* SourceTexture;

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetSourceWidth(float bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetSourceHeight(float NewValue);

public:

	virtual float ComputeLightBrightness() const override;

	//~ Begin ULightComponent Interface.
	virtual ELightComponentType GetLightType() const override;
	virtual float GetUniformPenumbraSize() const override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface
};
// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraCommon.h"
#include "NiagaraLightRendererProperties.generated.h"

UCLASS(editinlinenew)
class UNiagaraLightRendererProperties : public UNiagaraRendererProperties
{
public:
	GENERATED_BODY()

	UNiagaraLightRendererProperties();

	virtual void PostInitProperties() override;

	static void InitCDOPropertiesAfterModuleStartup();

	//~ UNiagaraRendererProperties interface
	virtual NiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const override { return (InSimTarget == ENiagaraSimTarget::CPUSim); };
#if WITH_EDITORONLY_DATA
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) override;
	virtual void FixMaterial(UMaterial* Material) override;
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif // WITH_EDITORONLY_DATA

	/** Whether to use physically based inverse squared falloff from the light.  If unchecked, the value from the LightExponent binding will be used instead. */
	UPROPERTY(EditAnywhere, Category = "Light Rendering")
	uint32 bUseInverseSquaredFalloff : 1;

	/**
	 * Whether lights from this renderer should affect translucency.
	 * Use with caution - if enabled, create only a few particle lights at most, and the smaller they are, the less they will cost.
	 */
	UPROPERTY(EditAnywhere, Category = "Light Rendering")
	uint32 bAffectsTranslucency : 1;

	/** By default, a light is spawned for each particle. Enable this to control the spawn-rate on a per-particle basis. */
	UPROPERTY(EditAnywhere, Category = "Light Rendering", meta = (InlineEditConditionToggle))
	uint32 bOverrideRenderingEnabled : 1;

	/** A factor used to scale each particle light radius */
	UPROPERTY(EditAnywhere, Category = "Light Rendering", meta = (UIMin = "0"))
	float RadiusScale;

	/** A static color shift applied to each rendered light */
	UPROPERTY(EditAnywhere, Category = "Light Rendering")
	FVector ColorAdd;

	/** Which attribute should we use to check if light rendering should be enabled for a particle? */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Bindings", meta = (EditCondition = "bOverrideRenderingEnabled"))
	FNiagaraVariableAttributeBinding LightRenderingEnabledBinding;

	/** Which attribute should we use for the light's exponent when inverse squared falloff is disabled? */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Bindings", meta = (EditCondition = "!bUseInverseSquaredFalloff"))
	FNiagaraVariableAttributeBinding LightExponentBinding;

	/** Which attribute should we use for position when generating lights?*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Bindings")
	FNiagaraVariableAttributeBinding PositionBinding;

	/** Which attribute should we use for light color when generating lights?*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Bindings")
	FNiagaraVariableAttributeBinding ColorBinding;

	/** Which attribute should we use for light radius when generating lights?*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Bindings")
	FNiagaraVariableAttributeBinding RadiusBinding;

	/** Which attribute should we use for the intensity of the volumetric scattering from this light? This scales the light's intensity and color. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Bindings")
	FNiagaraVariableAttributeBinding VolumetricScatteringBinding;
};

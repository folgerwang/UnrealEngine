// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

	UPROPERTY(EditAnywhere, Category = "Light Rendering")
	float RadiusScale;

	UPROPERTY(EditAnywhere, Category = "Light Rendering")
	FVector ColorAdd;

	/** Which attribute should we use for position when generating lights?*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Light Rendering")
	FNiagaraVariableAttributeBinding PositionBinding;

	/** Which attribute should we use for light color when generating lights?*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Light Rendering")
	FNiagaraVariableAttributeBinding ColorBinding;

	/** Which attribute should we use for light radius when generating lights?*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Light Rendering")
	FNiagaraVariableAttributeBinding RadiusBinding;
};

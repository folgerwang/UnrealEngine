// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialExpressionScalarParameter.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionScalarParameter : public UMaterialExpressionParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float DefaultValue;

#if WITH_EDITORONLY_DATA
	/** 
	 * Sets the lower bound for the slider on this parameter in the material instance editor. 
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float SliderMin;

	/** 
	 * Sets the upper bound for the slider on this parameter in the material instance editor. 
	 * The slider will be disabled if SliderMax <= SliderMin.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionScalarParameter)
	float SliderMax;
#endif

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(const FMaterialParameterInfo& ParameterInfo, float& OutValue) const;

#if WITH_EDITOR
	bool SetParameterValue(FName InParameterName, float InValue);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ValidateParameterName(const bool bAllowDuplicateName) override;
	virtual bool HasClassAndNameCollision(UMaterialExpression* OtherExpression) const override;
	virtual void SetValueToMatchingExpression(UMaterialExpression* OtherExpression) override;
#endif

	virtual bool IsUsedAsAtlasPosition() const { return false; }
};





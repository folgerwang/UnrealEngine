// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MaterialExpressionCurveAtlasRowParameter.generated.h"



UCLASS(collapsecategories, hidecategories=(Object, MaterialExpressionScalarParameter), MinimalAPI)
class UMaterialExpressionCurveAtlasRowParameter : public UMaterialExpressionScalarParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCurveAtlasRowParameter)
	class UCurveLinearColor* Curve;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionCurveAtlasRowParameter)
	class UCurveLinearColorAtlas* Atlas;

	UPROPERTY()
	FExpressionInput InputTime;

	virtual bool IsUsedAsAtlasPosition() const override { return true; }
	virtual UTexture* GetReferencedTexture() override;
	virtual bool CanReferenceTexture() const { return true; }

#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override 
	{
		return MCT_Float;
	}

	virtual bool IsInputConnectionRequired(int32 InputIndex) const override { return true; }
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetTexturesForceMaterialRecompile(TArray<UTexture *> &Textures) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override 
	{
		OutCaptions.Empty();
		OutCaptions.Add(TEXT(""));
	};
//~ Begin UMaterialExpression Interface
	virtual FName GetInputName(int32 InputIndex) const override
	{
		return TEXT("CurveTime");
	}

#endif
};


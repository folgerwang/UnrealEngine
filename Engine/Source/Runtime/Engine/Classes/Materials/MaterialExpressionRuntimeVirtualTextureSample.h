// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "UObject/ObjectMacros.h"
#include "VT/RuntimeVirtualTexture.h"
#include "MaterialExpressionRuntimeVirtualTextureSample.generated.h"

/** Material expression for sampling from a runtime virtual texture. */
UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionRuntimeVirtualTextureSample : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** UV coordinates input if we want to override standard world position based coordinates. */
	UPROPERTY()
	FExpressionInput Coordinates;

	/** The virtual texture object to sample. */
	UPROPERTY(EditAnywhere, Category = VirtualTexture)
	class URuntimeVirtualTexture* VirtualTexture;

	/** How to interpret the virtual texture contents. Note that the bound Virtual Texture should have the same setting for sampling to work correctly. */
	UPROPERTY(EditAnywhere, Category = VirtualTexture, meta = (DisplayName = "Virtual texture content"))
	ERuntimeVirtualTextureMaterialType MaterialType = ERuntimeVirtualTextureMaterialType::BaseColor;

protected:
	/** Init MaterialType based on the current VirtualTexture */
	void InitMaterialType();

	/** Initialize the output pins. */
	void InitOutputs();

	//~ Begin UMaterialExpression Interface
	virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const { return true; }

#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};

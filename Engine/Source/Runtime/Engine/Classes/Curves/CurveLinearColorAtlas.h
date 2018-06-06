// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "CurveLinearColorAtlas.generated.h"


static FName NAME_GradientTexture = FName(TEXT("GradientTexture"));
static FName NAME_GradientBias = FName(TEXT("GradientBias"));
static FName NAME_GradientScale = FName(TEXT("GradientScale"));
static FName NAME_GradientCount = FName(TEXT("GradientCount"));

class UCurveLinearColor;



/**
*  Manages gradient LUT textures for registered actors and assigns them to the corresponding materials on the actor
*/
UCLASS()
class ENGINE_API UCurveLinearColorAtlas : public UTexture2D
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostLoad() override;
	// How many slots are available per texture
	FORCEINLINE uint32 MaxSlotsPerTexture()
	{
		return TextureSize / GradientPixelSize;
	}

	// Immediately render a new material to the specified slot index(SlotIndex must be within this section's range)
	void UpdateGradientSlot(UCurveLinearColor* Gradient);

	// Re-render all texture groups
	void UpdateTextures();
#endif

	UFUNCTION(BlueprintCallable, Category = "Math|Curves")
	bool GetCurveIndex(UCurveLinearColor* InCurve, int32& Index);

	UFUNCTION(BlueprintCallable, Category = "Math|Curves")
	bool GetCurvePosition(UCurveLinearColor* InCurve, float& Position);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint32 bIsDirty : 1;

	uint32	bHasAnyDirtyTextures : 1;
	uint32	bShowDebugColorsForNullGradients : 1;	// Renders alternate blue/yellow lines for empty gradients. Good for debugging, but turns off optimization for selective updates to gradients.

	TArray<FColor> SrcData;
#endif
	UPROPERTY(EditAnywhere, Category = "Curves")
	uint32	TextureSize;						// Size of the lookup textures

	UPROPERTY(VisibleAnywhere, Category = "Curves")
	uint32	GradientPixelSize;					// How many pixels tall is any gradient slot

	UPROPERTY(EditAnywhere, Category = "Curves")
	TArray<UCurveLinearColor*> GradientCurves;

protected:
#if WITH_EDITORONLY_DATA
	FVector2D SizeXY;
#endif
};

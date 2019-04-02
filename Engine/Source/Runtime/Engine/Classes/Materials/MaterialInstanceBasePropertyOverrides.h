// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "MaterialInstanceBasePropertyOverrides.generated.h"

/** Properties from the base material that can be overridden in material instances. */
USTRUCT()
struct ENGINE_API FMaterialInstanceBasePropertyOverrides
{
	GENERATED_USTRUCT_BODY()

	/** Enables override of the opacity mask clip value. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 bOverride_OpacityMaskClipValue : 1;

	/** Enables override of the blend mode. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 bOverride_BlendMode : 1;

	/** Enables override of the shading model. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 bOverride_ShadingModel : 1;

	/** Enables override of the dithered LOD transition property. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 bOverride_DitheredLODTransition : 1;

	/** Enables override of whether to shadow using masked opacity on translucent materials. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 bOverride_CastDynamicShadowAsMasked : 1;

	/** Enables override of the two sided property. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 bOverride_TwoSided : 1;

	/** Indicates that the material should be rendered without backface culling and the normal should be flipped for backfaces. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_TwoSided"))
	uint8 TwoSided : 1;

	/** Whether the material should support a dithered LOD transition when used with the foliage system. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_DitheredLODTransition"))
	uint8 DitheredLODTransition : 1;

	/** Whether the material should cast shadows as masked even though it has a translucent blend mode. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_CastShadowAsMasked", NoSpinbox = true))
	uint8 bCastDynamicShadowAsMasked:1;

	/** The blend mode */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_BlendMode"))
	TEnumAsByte<EBlendMode> BlendMode;

	/** The shading model */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_ShadingModel"))
	TEnumAsByte<EMaterialShadingModel> ShadingModel;

	/** If BlendMode is BLEND_Masked, the surface is not rendered where OpacityMask < OpacityMaskClipValue. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_OpacityMaskClipValue", NoSpinbox = true))
	float OpacityMaskClipValue;

	FMaterialInstanceBasePropertyOverrides();

	bool operator==(const FMaterialInstanceBasePropertyOverrides& Other)const;
	bool operator!=(const FMaterialInstanceBasePropertyOverrides& Other)const;
};

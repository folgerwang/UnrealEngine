// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "TextureDefines.generated.h"

/**
 * @warning: if this is changed:
 *     update BaseEngine.ini [SystemSettings]
 *     you might have to update the update Game's DefaultEngine.ini [SystemSettings]
 *     order and actual name can never change (order is important!)
 *
 * TEXTUREGROUP_Cinematic: should be used for Cinematics which will be baked out
 *                         and want to have the highest settings
 */
UENUM()
enum TextureGroup
{
	TEXTUREGROUP_World UMETA(DisplayName="ini:World"),
	TEXTUREGROUP_WorldNormalMap UMETA(DisplayName="ini:WorldNormalMap"),
	TEXTUREGROUP_WorldSpecular UMETA(DisplayName="ini:WorldSpecular"),
	TEXTUREGROUP_Character UMETA(DisplayName="ini:Character"),
	TEXTUREGROUP_CharacterNormalMap UMETA(DisplayName="ini:CharacterNormalMap"),
	TEXTUREGROUP_CharacterSpecular UMETA(DisplayName="ini:CharacterSpecular"),
	TEXTUREGROUP_Weapon UMETA(DisplayName="ini:Weapon"),
	TEXTUREGROUP_WeaponNormalMap UMETA(DisplayName="ini:WeaponNormalMap"),
	TEXTUREGROUP_WeaponSpecular UMETA(DisplayName="ini:WeaponSpecular"),
	TEXTUREGROUP_Vehicle UMETA(DisplayName="ini:Vehicle"),
	TEXTUREGROUP_VehicleNormalMap UMETA(DisplayName="ini:VehicleNormalMap"),
	TEXTUREGROUP_VehicleSpecular UMETA(DisplayName="ini:VehicleSpecular"),
	TEXTUREGROUP_Cinematic UMETA(DisplayName="ini:Cinematic"),
	TEXTUREGROUP_Effects UMETA(DisplayName="ini:Effects"),
	TEXTUREGROUP_EffectsNotFiltered UMETA(DisplayName="ini:EffectsNotFiltered"),
	TEXTUREGROUP_Skybox UMETA(DisplayName="ini:Skybox"),
	TEXTUREGROUP_UI UMETA(DisplayName="ini:UI"),
	TEXTUREGROUP_Lightmap UMETA(DisplayName="ini:Lightmap"),
	TEXTUREGROUP_RenderTarget UMETA(DisplayName="ini:RenderTarget"),
	TEXTUREGROUP_MobileFlattened UMETA(DisplayName="ini:MobileFlattened"),
	/** Obsolete - kept for backwards compatibility. */
	TEXTUREGROUP_ProcBuilding_Face UMETA(DisplayName="ini:ProcBuilding_Face"),
	/** Obsolete - kept for backwards compatibility. */
	TEXTUREGROUP_ProcBuilding_LightMap UMETA(DisplayName="ini:ProcBuilding_LightMap"),
	TEXTUREGROUP_Shadowmap UMETA(DisplayName="ini:Shadowmap"),
	/** No compression, no mips. */
	TEXTUREGROUP_ColorLookupTable UMETA(DisplayName="ini:ColorLookupTable"),
	TEXTUREGROUP_Terrain_Heightmap UMETA(DisplayName="ini:Terrain_Heightmap"),
	TEXTUREGROUP_Terrain_Weightmap UMETA(DisplayName="ini:Terrain_Weightmap"),
	/** Using this TextureGroup triggers special mip map generation code only useful for the BokehDOF post process. */
	TEXTUREGROUP_Bokeh UMETA(DisplayName="ini:Bokeh"),
	/** No compression, created on import of a .IES file. */
	TEXTUREGROUP_IESLightProfile UMETA(DisplayName="ini:IESLightProfile"),
	/** Non-filtered, useful for 2D rendering. */
	TEXTUREGROUP_Pixels2D UMETA(DisplayName="ini:2D Pixels (unfiltered)"),
	/** Hierarchical LOD generated textures*/
	TEXTUREGROUP_HierarchicalLOD UMETA(DisplayName="ini:Hierarchical LOD"),
	/** Impostor Color Textures*/
	TEXTUREGROUP_Impostor UMETA(DisplayName="ini:Impostor Color"),
	/** Impostor Normal and Depth, use default compression*/
	TEXTUREGROUP_ImpostorNormalDepth UMETA(DisplayName="ini:Impostor Normal and Depth"),
	/** Project specific group, rename in Engine.ini, [EnumRemap] TEXTUREGROUP_Project**.DisplayName=My Fun Group */
	TEXTUREGROUP_Project01 UMETA(DisplayName="ini:Project Group 01"),
	TEXTUREGROUP_Project02 UMETA(DisplayName="ini:Project Group 02"),
	TEXTUREGROUP_Project03 UMETA(DisplayName="ini:Project Group 03"),
	TEXTUREGROUP_Project04 UMETA(DisplayName="ini:Project Group 04"),
	TEXTUREGROUP_Project05 UMETA(DisplayName="ini:Project Group 05"),
	TEXTUREGROUP_Project06 UMETA(DisplayName="ini:Project Group 06"),
	TEXTUREGROUP_Project07 UMETA(DisplayName="ini:Project Group 07"),
	TEXTUREGROUP_Project08 UMETA(DisplayName="ini:Project Group 08"),
	TEXTUREGROUP_Project09 UMETA(DisplayName="ini:Project Group 09"),
	TEXTUREGROUP_Project10 UMETA(DisplayName="ini:Project Group 10"),
	TEXTUREGROUP_MAX,
};

UENUM()
enum TextureMipGenSettings
{
	/** Default for the "texture". */
	TMGS_FromTextureGroup UMETA(DisplayName="FromTextureGroup"),
	/** 2x2 average, default for the "texture group". */
	TMGS_SimpleAverage UMETA(DisplayName="SimpleAverage"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen0 UMETA(DisplayName="Sharpen0"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen1 UMETA(DisplayName = "Sharpen1"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen2 UMETA(DisplayName = "Sharpen2"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen3 UMETA(DisplayName = "Sharpen3"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen4 UMETA(DisplayName = "Sharpen4"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen5 UMETA(DisplayName = "Sharpen5"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen6 UMETA(DisplayName = "Sharpen6"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen7 UMETA(DisplayName = "Sharpen7"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen8 UMETA(DisplayName = "Sharpen8"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen9 UMETA(DisplayName = "Sharpen9"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen10 UMETA(DisplayName = "Sharpen10"),
	TMGS_NoMipmaps UMETA(DisplayName="NoMipmaps"),
	/** Do not touch existing mip chain as it contains generated data. */
	TMGS_LeaveExistingMips UMETA(DisplayName="LeaveExistingMips"),
	/** Blur further (useful for image based reflections). */
	TMGS_Blur1 UMETA(DisplayName="Blur1"),
	TMGS_Blur2 UMETA(DisplayName="Blur2"),
	TMGS_Blur3 UMETA(DisplayName="Blur3"),
	TMGS_Blur4 UMETA(DisplayName="Blur4"),
	TMGS_Blur5 UMETA(DisplayName="Blur5"),
	TMGS_MAX,

	// Note: These are serialized as as raw values in the texture DDC key, so additional entries
	// should be added at the bottom; reordering or removing entries will require changing the GUID
	// in the texture compressor DDC key
};

/** Options for texture padding mode. */
UENUM()
namespace ETexturePowerOfTwoSetting
{
	enum Type
	{
		/** Do not modify the texture dimensions. */
		None,

		/** Pad the texture to the nearest power of two size. */
		PadToPowerOfTwo,

		/** Pad the texture to the nearest square power of two size. */
		PadToSquarePowerOfTwo

		// Note: These are serialized as as raw values in the texture DDC key, so additional entries
		// should be added at the bottom; reordering or removing entries will require changing the GUID
		// in the texture compressor DDC key
	};
}

// Must match enum ESamplerFilter in RHIDefinitions.h
UENUM()
enum class ETextureSamplerFilter : uint8
{
	Point,
	Bilinear,
	Trilinear,
	AnisotropicPoint,
	AnisotropicLinear,
};

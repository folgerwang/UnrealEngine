// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Engine/TextureLODSettings.h"
#include "Engine/Texture2D.h"

void FTextureLODGroup::SetupGroup()
{
	// editor would never want to use smaller mips based on memory (could affect cooking, etc!)
	if (!GIsEditor)
	{
		// setup 
		switch (FPlatformMemory::GetMemorySizeBucket())
		{
			case EPlatformMemorySizeBucket::Smallest:
				// use Smallest values, if they exist, or Smaller, if not
				if (LODBias_Smallest > 0)
				{
					LODBias = LODBias_Smallest;
				}
				else if (LODBias_Smaller > 0)
				{
					LODBias = LODBias_Smaller;
				}
				if (MaxLODSize_Smallest > 0)
				{
					MaxLODSize = MaxLODSize_Smallest;
				}
				else if (MaxLODSize_Smaller > 0)
				{
					MaxLODSize = MaxLODSize_Smaller;
				}
				break;
			case EPlatformMemorySizeBucket::Smaller:
				// use Smaller values if they exist
				if (LODBias_Smaller > 0)
				{
					LODBias = LODBias_Smaller;
				}
				if (MaxLODSize_Smaller > 0)
				{
					MaxLODSize = MaxLODSize_Smaller;
				}
				break;
			default:
				break;
		}
	}

	MinLODMipCount = FMath::CeilLogTwo(MinLODSize);
	MaxLODMipCount = FMath::CeilLogTwo(MaxLODSize);

	OptionalMaxLODMipCount = FMath::CeilLogTwo(OptionalMaxLODSize);

	// Linear filtering
	if (MinMagFilter == NAME_Linear)
	{
		if (MipFilter == NAME_Point)
		{
			Filter = ETextureSamplerFilter::Bilinear;
		}
		else
		{
			Filter = ETextureSamplerFilter::Trilinear;
		}
	}
	// Point. Don't even care about mip filter.
	else if (MinMagFilter == NAME_Point)
	{
		Filter = ETextureSamplerFilter::Point;
	}
	// Aniso or unknown.
	else
	{
		if (MipFilter == NAME_Point)
		{
			Filter = ETextureSamplerFilter::AnisotropicPoint;
		}
		else
		{
			Filter = ETextureSamplerFilter::AnisotropicLinear;
		}
	}
}

UTextureLODSettings::UTextureLODSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 * Returns the texture group names, sorted like enum.
 *
 * @return array of texture group names
 */
TArray<FString> UTextureLODSettings::GetTextureGroupNames()
{
	TArray<FString> TextureGroupNames;

#define GROUPNAMES(g) new(TextureGroupNames) FString(TEXT(#g));
	FOREACH_ENUM_TEXTUREGROUP(GROUPNAMES)
#undef GROUPNAMES

	return TextureGroupNames;
}

void UTextureLODSettings::SetupLODGroup(int32 GroupId)
{
	TextureLODGroups[GroupId].SetupGroup();
}

int32 UTextureLODSettings::CalculateLODBias(const UTexture* Texture, bool bIncCinematicMips) const
{	
	check( Texture );
	TextureMipGenSettings MipGenSetting = TMGS_MAX;
	int32 TextureMaxSize = 0;

#if WITH_EDITORONLY_DATA
	MipGenSetting = Texture->MipGenSettings;
	TextureMaxSize = Texture->MaxTextureSize;
#endif // #if WITH_EDITORONLY_DATA

	return CalculateLODBias(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight(), TextureMaxSize, Texture->LODGroup, Texture->LODBias, bIncCinematicMips ? Texture->NumCinematicMipLevels : 0, MipGenSetting);
}

int32 UTextureLODSettings::CalculateLODBias(int32 Width, int32 Height, int32 MaxSize, int32 LODGroup, int32 LODBias, int32 NumCinematicMipLevels, TextureMipGenSettings InMipGenSetting) const
{	
	// Find LOD group.
	const FTextureLODGroup& LODGroupInfo = TextureLODGroups[LODGroup];

	// Test to see if we have no mip generation as in which case the LOD bias will be ignored
	const TextureMipGenSettings FinalMipGenSetting = (InMipGenSetting == TMGS_FromTextureGroup) ? (TextureMipGenSettings)LODGroupInfo.MipGenSettings : InMipGenSetting;
	if ( FinalMipGenSetting == TMGS_NoMipmaps )
	{
		return 0;
	}

	// Calculate maximum number of miplevels.
	if (MaxSize > 0)
	{
		Width = FMath::Min(Width, MaxSize);
		Height = FMath::Min(Height, MaxSize);
	}
	int32 TextureMaxLOD	= FMath::CeilLogTwo( FMath::Max( Width, Height ) );

	// Calculate LOD bias.
	int32 UsedLODBias	= NumCinematicMipLevels;
	if (!FPlatformProperties::RequiresCookedData())
	{
		// When cooking, LODBias and LODGroupInfo.LODBias are taken into account to strip the top mips.
		// Considering them again here would apply them twice.
		UsedLODBias	+= LODBias + LODGroupInfo.LODBias;
	}

	int32 MinLOD		= LODGroupInfo.MinLODMipCount;
	int32 MaxLOD		= LODGroupInfo.MaxLODMipCount;
	int32 WantedMaxLOD	= FMath::Clamp( TextureMaxLOD - UsedLODBias, MinLOD, MaxLOD );
	WantedMaxLOD		= FMath::Clamp( WantedMaxLOD, 0, TextureMaxLOD );
	UsedLODBias			= TextureMaxLOD - WantedMaxLOD;

	return UsedLODBias;
}

int32 UTextureLODSettings::CalculateNumOptionalMips(int32 LODGroup, const int32 Width, const int32 Height, const int32 NumMips, const int32 MinMipToInline, TextureMipGenSettings InMipGenSetting) const
{
	// shouldn't need to call this client side, this is calculated at save texture time
	check( FPlatformProperties::RequiresCookedData() == false);

	const FTextureLODGroup& LODGroupInfo = TextureLODGroups[LODGroup];

	const TextureMipGenSettings& FinalMipGenSetting = (InMipGenSetting == TMGS_FromTextureGroup) ? (TextureMipGenSettings)LODGroupInfo.MipGenSettings : InMipGenSetting;
	if ( FinalMipGenSetting == TMGS_NoMipmaps)
	{
		return 0;
	}

	int32 OptionalLOD = FMath::Min(LODGroupInfo.OptionalMaxLODMipCount+1, NumMips);

	int32 NumOptionalMips = FMath::Min(NumMips - (OptionalLOD - LODGroupInfo.OptionalLODBias), MinMipToInline);
	return NumOptionalMips;
}

/**
* TextureLODGroups access with bounds check
*
* @param   GroupIndex      usually from Texture.LODGroup
* @return                  A handle to the indexed LOD group. 
*/
FTextureLODGroup& UTextureLODSettings::GetTextureLODGroup(TextureGroup GroupIndex)
{
	check(GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX);
	return TextureLODGroups[GroupIndex];
}

/**
* TextureLODGroups access with bounds check
*
* @param   GroupIndex      usually from Texture.LODGroup
* @return                  A handle to the indexed LOD group.
*/
const FTextureLODGroup& UTextureLODSettings::GetTextureLODGroup(TextureGroup GroupIndex) const
{
	check(GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX);
	return TextureLODGroups[GroupIndex];
}

#if WITH_EDITORONLY_DATA
void UTextureLODSettings::GetMipGenSettings(const UTexture& Texture, TextureMipGenSettings& OutMipGenSettings, float& OutSharpen, uint32& OutKernelSize, bool& bOutDownsampleWithAverage, bool& bOutSharpenWithoutColorShift, bool &bOutBorderColorBlack) const
{
	TextureMipGenSettings Setting = (TextureMipGenSettings)Texture.MipGenSettings;

	bOutBorderColorBlack = false;

	// avoiding the color shift assumes we deal with colors which is not true for normalmaps
	// or we blur where it's good to blur the color as well
	bOutSharpenWithoutColorShift = !Texture.IsNormalMap();

	bOutDownsampleWithAverage = true;

	// inherit from texture group
	if(Setting == TMGS_FromTextureGroup)
	{
		const FTextureLODGroup& LODGroup = TextureLODGroups[Texture.LODGroup];

		Setting = LODGroup.MipGenSettings;
	}
	OutMipGenSettings = Setting;

	// ------------

	// default:
	OutSharpen = 0;
	OutKernelSize = 2;

	if(Setting >= TMGS_Sharpen0 && Setting <= TMGS_Sharpen10)
	{
		// 0 .. 2.0f
		OutSharpen = ((int32)Setting - (int32)TMGS_Sharpen0) * 0.2f;
		OutKernelSize = 8;
	}
	else if(Setting >= TMGS_Blur1 && Setting <= TMGS_Blur5)
	{
		int32 BlurFactor = ((int32)Setting + 1 - (int32)TMGS_Blur1);
		OutSharpen = -BlurFactor * 2;
		OutKernelSize = 2 + 2 * BlurFactor;
		bOutDownsampleWithAverage = false;
		bOutSharpenWithoutColorShift = false;
		bOutBorderColorBlack = true;
	}
}
#endif // #if WITH_EDITORONLY_DATA



/**
 * Returns the LODGroup mip gen settings
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	TextureMipGenSettings for lod group
 */
const TextureMipGenSettings UTextureLODSettings::GetTextureMipGenSettings(int32 InLODGroup) const
{
	return TextureLODGroups[InLODGroup].MipGenSettings; 
}



/**
 * Returns the filter state that should be used for the passed in texture, taking
 * into account other system settings.
 *
 * @param	Texture		Texture to retrieve filter state for, must not be 0
 * @return	Filter sampler state for passed in texture
 */
ETextureSamplerFilter UTextureLODSettings::GetSamplerFilter(const UTexture* Texture) const
{
	// Default to point filtering.
	ETextureSamplerFilter Filter = ETextureSamplerFilter::Point;

	switch(Texture->Filter)
	{
		case TF_Nearest: Filter = ETextureSamplerFilter::Point; break;
		case TF_Bilinear: Filter = ETextureSamplerFilter::Bilinear; break;
		case TF_Trilinear: Filter = ETextureSamplerFilter::Trilinear; break;

		// TF_Default
		default:
			// Use LOD group value to find proper filter setting.
			Filter = TextureLODGroups[Texture->LODGroup].Filter;
	}

	return Filter;
}

ETextureSamplerFilter UTextureLODSettings::GetSamplerFilter(int32 InLODGroup) const
{
	return TextureLODGroups[InLODGroup].Filter;
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpace.h"

#include "OpenColorIOConfiguration.h"

/*
 * FOpenColorIOColorSpace implementation
 */

FOpenColorIOColorSpace::FOpenColorIOColorSpace()
	: ColorSpaceIndex(INDEX_NONE)
{ }

FOpenColorIOColorSpace::FOpenColorIOColorSpace(const FString& InColorSpaceName, int32 InColorSpaceIndex, const FString& InFamilyName)
	: ColorSpaceName(InColorSpaceName)
	, ColorSpaceIndex(InColorSpaceIndex)
	, FamilyName(InFamilyName)
{ }

FString FOpenColorIOColorSpace::ToString() const
{
	if (IsValid())
	{
		return FString::Printf(TEXT("[%s] %s"), *FamilyName, *ColorSpaceName);
	}
	return TEXT("<Invalid>");
}

bool FOpenColorIOColorSpace::IsValid() const
{
	return ColorSpaceIndex != INDEX_NONE && !ColorSpaceName.IsEmpty();
}


/*
 * FOpenColorIOColorConversionSettings implementation
 */

FOpenColorIOColorConversionSettings::FOpenColorIOColorConversionSettings()
	: ConfigurationSource(nullptr)
{

}

FString FOpenColorIOColorConversionSettings::ToString() const
{
	if (ConfigurationSource)
	{
		return FString::Printf(TEXT("%s config - %s to %s"), *ConfigurationSource->GetName(), *SourceColorSpace.ToString(), *DestinationColorSpace.ToString());
	}
	return TEXT("<Invalid Conversion>");
}

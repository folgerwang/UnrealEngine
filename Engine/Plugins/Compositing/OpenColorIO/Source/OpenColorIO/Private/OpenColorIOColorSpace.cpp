// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpace.h"

#include "OpenColorIOConfiguration.h"

/*
 * FOpenColorIOColorSpace implementation
 */

const TCHAR* FOpenColorIOColorSpace::FamilyDelimiter = TEXT("/");

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
		return FString::Printf(TEXT("%s"), *ColorSpaceName);
	}
	return TEXT("<Invalid>");
}

bool FOpenColorIOColorSpace::IsValid() const
{
	return ColorSpaceIndex != INDEX_NONE && !ColorSpaceName.IsEmpty();
}

FString FOpenColorIOColorSpace::GetFamilyNameAtDepth(int32 InDepth) const
{
	FString ReturnName;

	TArray<FString> Families;
	FamilyName.ParseIntoArray(Families, FamilyDelimiter);
	if (Families.IsValidIndex(InDepth))
	{
		ReturnName = Families[InDepth];
	}
	else
	{
		//No separator found, does it want the first family?
		if (InDepth == 0 && !FamilyName.IsEmpty())
		{
			ReturnName = FamilyName;
		}
	}

	return ReturnName;
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

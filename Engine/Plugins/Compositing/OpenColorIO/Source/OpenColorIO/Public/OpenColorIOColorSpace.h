// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "OpenColorIOColorSpace.generated.h"


class UOpenColorIOConfiguration;

/**
 * Structure to identify a ColorSpace as described in an OCIO configuration file. 
 * Members are populated by data coming from a config file.
 */
USTRUCT(BlueprintType)
struct OPENCOLORIO_API FOpenColorIOColorSpace
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	FOpenColorIOColorSpace();

	/**
	 * Create and initialize a new instance.
	 */
	FOpenColorIOColorSpace(const FString& InColorSpaceName, int32 InColorSpaceIndex, const FString& InFamilyName);

	/** The ColorSpace name. */
	UPROPERTY(VisibleAnywhere, Category=ColorSpace)
	FString ColorSpaceName;

	/** The index of the ColorSpace in the config */
	UPROPERTY()
	int32 ColorSpaceIndex;

	/** 
	 * The family of this ColorSpace as specified in the configuration file. 
	 * When you have lots of colorspaces, you can regroup them by family to facilitate browsing them. 
	 */
	UPROPERTY(VisibleAnywhere, Category=ColorSpace)
	FString FamilyName;

	/** Delimiter used in the OpenColorIO library to make family hierarchies */
	static const TCHAR* FamilyDelimiter;

public:
	bool operator==(const FOpenColorIOColorSpace& Other) const { return Other.ColorSpaceIndex == ColorSpaceIndex && Other.ColorSpaceName == ColorSpaceName; }

	/**
	 * Get the string representation of this color space.
	 * @return ColorSpace name. 
	 */
	FString ToString() const;

	/** Return true if the index and name have been set properly */
	bool IsValid() const;

	/** 
	 * Return the family name at the desired depth level 
	 * @param InDepth Desired depth in the family string. 0 == First layer. 
	 * @return FamilyName at the desired depth. Empty string if depth level doesn't exist.
	 */
	FString GetFamilyNameAtDepth(int32 InDepth) const;
};

/**
* Identifies a OCIO ColorSpace.
*/
USTRUCT(BlueprintType)
struct OPENCOLORIO_API FOpenColorIOColorConversionSettings
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	FOpenColorIOColorConversionSettings();

	/** The source color space name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	UOpenColorIOConfiguration* ConfigurationSource;

	/** The source color space name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	FOpenColorIOColorSpace SourceColorSpace;

	/** The destination color space name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	FOpenColorIOColorSpace DestinationColorSpace;

public:

	/**
	 * Get a string representation of this conversion.
	 * @return String representation, i.e. "ConfigurationAssetName - SourceColorSpace to DestinationColorSpace".
	 */
	FString ToString() const;
};


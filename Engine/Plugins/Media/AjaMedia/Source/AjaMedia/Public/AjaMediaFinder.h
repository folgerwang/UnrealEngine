// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "UObject/ObjectMacros.h"
#include "AjaMediaFinder.generated.h"

/**
 * Available type of link the AJA support.
 */
UENUM()
enum class EAjaLinkType
{
	SingleLink = 0,
	DualLink,
	QuadLink,
};

/**
 * Available quad link the AJA support.
 */
UENUM()
enum class EAjaQuadLinkType
{
	Square,
	TSI,
};

/**
 * Available timecode formats that AJA support.
 */
UENUM()
enum class EAjaMediaTimecodeFormat : uint8
{
	None,
	LTC,
	VITC,
};

/**
* Identifies an AJA media source.
*/
USTRUCT(BlueprintType)
struct AJAMEDIA_API FAjaMediaDevice
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	FAjaMediaDevice();

	/** The retail name of the AJA Device, i.e. "IoExpress". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA)
	FString DeviceName;

	/** The index of the AJA Device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA, meta=(ClampMin="0"))
	int32 DeviceIndex;
};

/**
 * Identifies an AJA media source.
 */
USTRUCT(BlueprintType)
struct AJAMEDIA_API FAjaMediaPort
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	FAjaMediaPort();

	/**
	 * Create and initialize a new instance.
	 */
	FAjaMediaPort(FName InDeviceName, int32 InDeviceIndex, int32 InPortIndex);

	/** The retail name of the AJA Device, i.e. "IoExpress". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA)
	FName DeviceName;

	/** The index of the AJA Device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA, meta=(ClampMin="0"))
	int32 DeviceIndex;

	/** The type of cable link used for that configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA)
	EAjaLinkType LinkType;

	/** The type of cable link used for that configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AJA)
	EAjaQuadLinkType QuadLinkType;

	/** The index of the video channel on the Device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA, meta=(ClampMin="0"))
	int32 PortIndex;

public:
	bool operator==(const FAjaMediaPort& Other) const
	{
		return Other.DeviceIndex == DeviceIndex
			&& Other.LinkType == LinkType
			&& Other.PortIndex == PortIndex;
	}

	/**
	 * Get a string representation of this source.
	 * @return String representation, i.e. "IoExpress [device0/single1]".
	 */
	FString ToString() const;

	/**
	 * Get a url used by the Media framework
	 * @return Url representation, "aja://device0/single1"
	 */
	FString ToUrl() const;

	/** Return true if the device & port index have been set properly */
	bool IsValid() const;
};

/**
 * Identifies a media mode.
 */
USTRUCT(BlueprintType)
struct AJAMEDIA_API FAjaMediaMode
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	FAjaMediaMode();

public:
	/** The frame rate of the mode  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AJA)
	FFrameRate FrameRate;

	/** The image resolution of the mode  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AJA, meta=(DisplayName="Resolution"))
	FIntPoint TargetSize;

	/** If that configuration is in progressive transport */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AJA)
	bool bIsProgressiveStandard; //NTV2_IS_PROGRESSIVE_STANDARD

	/** If that configuration is in PSF */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AJA)
	bool bIsInterlacedStandard;

	/** If that configuration is in PSF */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AJA)
	bool bIsPsfStandard;

	/** The video format index for AJA */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=AJA)
	int32 VideoFormatIndex;

public:
	bool operator==(const FAjaMediaMode& Other) const
	{
		return Other.VideoFormatIndex == VideoFormatIndex;
	}

	/** Return true if the MediaMode has been set properly */
	bool IsValid() const;

	/**
	 * Get a string representation of this source.
	 * @return String representation, i.e. "IoExpress [device0/single1]".
	 */
	FString ToString() const;
};

/**
 * Configuration of an AJA input/output.
 */
USTRUCT()
struct AJAMEDIA_API FAjaMediaConfiguration
{
	GENERATED_BODY()
	
	FAjaMediaConfiguration();

public:
	/** Configured as an input or output. */
	UPROPERTY()
	bool bInput;

	/** The device and port and link type of the configuration. */
	UPROPERTY()
	FAjaMediaPort MediaPort;

	/** The video format of the configuration. */
	UPROPERTY()
	FAjaMediaMode MediaMode;

public:
	/** Return true if the device & port index have been set properly */
	bool IsValid() const;

	bool operator== (const FAjaMediaConfiguration& Other) const
	{
		return MediaPort == Other.MediaPort
			&& MediaMode == Other.MediaMode
			&& bInput == Other.bInput;
	}

	bool operator!= (const FAjaMediaConfiguration& Other) const
	{
		return !(operator==(Other));
	}

public:
	/**
	 * Get a string representation of this source.
	 * @return String representation, i.e. "IoExpress [SingleLink1][1080p60]".
	 */
	FText ToText() const;
};

/*
 * Find all of the AJA Inputs
 */
class AJAMEDIA_API FAjaMediaFinder
{
public:
	/** Link in a text format. */
	static FText LinkTypeToPrettyText(EAjaLinkType InLinkType, int32 InChannel, bool bShortVersion);
	/** Quad link in a text format. */
	static FText QuadLinkTypeToPrettyText(EAjaQuadLinkType InLinkType);
	/** Resolution in a text format */
	static FText ResolutionToPrettyText(FIntPoint InResolution);

	/** Get the list of AJA different configuration permutations available for that machine. */
	static bool	GetInputConfigurations(TArray<FAjaMediaConfiguration>& OutConfigurations);
	static bool	GetOutputConfigurations(TArray<FAjaMediaConfiguration>& OutConfigurations);

	/** Get the list of AJA device available for that machine. */
	static bool GetDevices(TArray<FAjaMediaDevice>& OutDevices);

	/** Get the list of AJA available sources available for that machine. */
	static bool GetSources(TArray<FAjaMediaPort>& OutSources);

	/** Get the list of Supported AJA video modes. */
	static bool GetModes(int32 DeviceIndex, bool bInOutput, TArray<FAjaMediaMode>& OutModes);

	/** Return true if the device & port index have been set properly */
	static bool IsValid(const FAjaMediaPort& InPort, const FAjaMediaMode& InMode, FString& OutFailureReason);
};

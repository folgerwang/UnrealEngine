// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/FrameRate.h"

#include "AjaMediaFinder.generated.h"


/**
 * Available timecode formats for Aja sources.
 */
UENUM()
enum class EAjaMediaTimecodeFormat : uint8
{
	None,
	LTC,
	VITC,
};

/**
 * Identifies an Aja media source.
 */
USTRUCT(BlueprintType)
struct AJAMEDIA_API FAjaMediaPort
{
	GENERATED_BODY()

public:
	static const TCHAR* Protocol;

	/** Default constructor. */
	FAjaMediaPort();

	/**
	 * Create and initialize a new instance.
	 */
	FAjaMediaPort(const FString& InDeviceName, int32 InDeviceIndex, int32 InPortIndex);

	/** The retail name of the Aja Device, i.e. "IoExpress". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA)
	FString DeviceName;

	/** The index of the Aja Device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA, meta=(ClampMin="0"))
	int32 DeviceIndex;

	/** The index of the video input/ouput port on that Device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AJA, meta=(ClampMin="0"))
	int32 PortIndex;

public:
	bool operator==(const FAjaMediaPort& Other) const { return Other.DeviceIndex == DeviceIndex && Other.PortIndex == PortIndex; }

	/**
	 * Get a string representation of this source.
	 * @return String representation, i.e. "IoExpress [device0/port1]".
	 */
	FString ToString() const;

	/**
	 * Get a url used by the Media framework
	 * @return Url representation, "aja://device0/port1"
	 */
	FString ToUrl() const;

	/** Return true if the device & port index have been set properly */
	bool IsValid() const;

	/**
	 * Build a AjaMediaSourceId from a Url representation.
	 * @param Url					A Url representation, i.e. "IoExpress [device0/port1]".
	 * @param bDiscoverDeviceName	Ask the AjaDevice the name of the Device. If false, the name will be empty.
	 * @return true on success
	 */
	bool FromUrl(const FString& Url, bool bDiscoverDeviceName);
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
	/** The index of the Aja Device */
	UPROPERTY()
	int32 DeviceIndex;

	/** The name of the mode, i.e. "1080p 60". */
	UPROPERTY()
	FString ModeName;

	/** The frame rate of the mode  */
	UPROPERTY()
	FFrameRate FrameRate;

	/** The target size of the mode  */
	UPROPERTY()
	FIntPoint TargetSize;

	/** The video format index for AJA */
	UPROPERTY()
	int32 VideoFormatIndex;

public:
	bool operator==(const FAjaMediaMode& Other) const
	{
		return Other.DeviceIndex == DeviceIndex && Other.FrameRate == FrameRate && Other.VideoFormatIndex == VideoFormatIndex;
	}

	/**
	 * Get a string representation of this mode.
	 * @return i.e. "1080p 60".
	 */
	FString ToString() const;

	/** Return true if the MediaMode has been set properly */
	bool IsValid() const;
};

/*
 * Find all of the AJA Inputs
 */
class AJAMEDIA_API FAjaMediaFinder
{
public:

	/** Get the list of AJA device installed in the machine. */
	static bool GetSources(TArray<FAjaMediaPort>& OutSources);

	/** Get the list of Supported AJA video modes. */
	static bool GetModes(int32 DeviceIndex, bool bInOutput, TArray<FAjaMediaMode>& OutModes);

	/** Return true if the device & port index have been set properly */
	static bool IsValid(const FAjaMediaPort& InPort, const FAjaMediaMode& InMode, FString& OutFailureReason);
};

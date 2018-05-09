// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AjaMediaFinder.generated.h"


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

	/**
	* Create and initialize a new instance.
	*/
	FAjaMediaMode(const FString& InModeName, int32 inMode);

	/** The retail name of the Device, i.e. "IoExpress". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AJA)
	FString ModeName;

	/** The index of the Device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AJA, meta = (ClampMin = "0"))
	int32 Mode;

public:

	/**
	 * Get a string representation of this mode.
	 * @return String representation, i.e. "".
	 */
	FString ToString() const;

	/**
	 * Get a url used by the Media framework
	 * @return Url representation, "aja://device0/port1"
	 */
	FString ToUrl() const;

	/** Return true if the device & port index have been set properly */
	bool IsValid() const;
};

/** Used to manage input modes. */
USTRUCT(BlueprintType)
struct AJAMEDIA_API FAjaMediaModeInput : public FAjaMediaMode
{
	GENERATED_BODY()
};

/** Used to manage output modes. */
USTRUCT(BlueprintType)
struct AJAMEDIA_API FAjaMediaModeOutput : public FAjaMediaMode
{
	GENERATED_BODY()
};

/*
 * Find all of the AJA Inputs
 */
UCLASS()
class AJAMEDIA_API UAjaMediaFinder : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Get the list of AJA media sources installed in the machine.
	 * @param OutSources Will contain the collection of found NDI source names and their URLs.
	 * @return true on success, false if the finder wasn't initialized.
	 */
	UFUNCTION(BlueprintCallable, Category=AJA)
	static bool GetSources(TArray<FAjaMediaPort>& OutSources);

	/**
	 * Get the list of Supported AJA video modes.
	 * @param OutModes Will contain the collection of found modes.
	 * @return true on success, false if the finder wasn't initialized.
	 */
	UFUNCTION(BlueprintCallable, Category = AJA)
	static bool GetModes(TArray<FAjaMediaMode>& OutModes, bool bInOutput);
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BlackmagicMediaFinder.generated.h"


/**
 * Identifies an media source.
 */
USTRUCT(BlueprintType)
struct BLACKMAGICMEDIA_API FBlackmagicMediaPort
{
	GENERATED_BODY()

public:
	static const TCHAR* Protocol;

	/** Default constructor. */
	FBlackmagicMediaPort();

	/**
	 * Create and initialize a new instance.
	 */
	FBlackmagicMediaPort(const FString& InDeviceName, int32 InDeviceIndex, int32 InPortIndex);

	/** The retail name of the Device, i.e. "IoExpress". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BLACKMAGIC)
	FString DeviceName;

	/** The index of the Device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BLACKMAGIC, meta=(ClampMin="0"))
	int32 DeviceIndex;

	/** The index of the video input/ouput port on that Device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BLACKMAGIC, meta=(ClampMin="0"))
	int32 PortIndex;

public:

	/**
	 * Get a string representation of this source.
	 * @return String representation, i.e. "IoExpress [device0/port1]".
	 */
	FString ToString() const;

	/**
	 * Get a url used by the Media framework
	 * @return Url representation, "blackmagic://device0/port1"
	 */
	FString ToUrl() const;

	/** Return true if the device & port index have been set properly */
	bool IsValid() const;

	/**
	 * Build a BlackmagicMediaSourceId from a Url representation.
	 * @param Url					A Url representation, i.e. "IoExpress [device0/port1]".
	 * @param bDiscoverDeviceName	Ask the BlackmagicDevice the name of the Device. If false, the name will be empty.
	 * @return true on success
	 */
	bool FromUrl(const FString& Url, bool bDiscoverDeviceName);
};

/**
 * Identifies a media mode.
 */
USTRUCT(BlueprintType)
struct BLACKMAGICMEDIA_API FBlackmagicMediaMode
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	FBlackmagicMediaMode();

	/**
	 * Create and initialize a new instance.
	 */
	FBlackmagicMediaMode(const FString& InModeName, int32 inMode);

	/** The retail name of the Device, i.e. "IoExpress". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BLACKMAGIC)
	FString ModeName;

	/** The index of the Device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BLACKMAGIC, meta=(ClampMin="0"))
	int32 Mode;

public:

	/**
	 * Get a string representation of this source.
	 * @return String representation, i.e. "IoExpress [device0/port1]".
	 */
	FString ToString() const;

	/**
	 * Get a url used by the Media framework
	 * @return Url representation, "blackmagic://device0/port1"
	 */
	FString ToUrl() const;

	/** Return true if the device & port index have been set properly */
	bool IsValid() const;
};

/** Used to manage input modes. */
USTRUCT(BlueprintType)
struct BLACKMAGICMEDIA_API FBlackmagicMediaModeInput : public FBlackmagicMediaMode
{
	GENERATED_BODY()
};

/** Used to manage output modes. */
USTRUCT(BlueprintType)
struct BLACKMAGICMEDIA_API FBlackmagicMediaModeOutput : public FBlackmagicMediaMode
{
	GENERATED_BODY()
};

/*
 * Find all of the Inputs
 */
UCLASS()
class BLACKMAGICMEDIA_API UBlackmagicMediaFinder : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Get the list of media sources installed in the machine.
	 * @param OutSources Will contain the collection of found NDI source names and their URLs.
	 * @return true on success, false if the finder wasn't initialized.
	 */
	UFUNCTION(BlueprintCallable, Category=Blackmagic)
	static bool GetSources(TArray<FBlackmagicMediaPort>& OutSources);

	UFUNCTION(BlueprintCallable, Category=Blackmagic)
	static bool GetModes(TArray<FBlackmagicMediaMode>& OutSources, bool bInOutput);
};

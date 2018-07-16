// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AjaMediaFinder.h"

#include "AjaMediaSettings.generated.h"


USTRUCT()
struct FAjaInputPortSettings
{
	GENERATED_BODY()

public:
	/**
	 * The input name of the AJA source to be played".
	 * This combines the device ID, and the input.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "AJA")
	FAjaMediaPort MediaPort;

	/** The expected signal input from the MediaPort. */
	UPROPERTY(Config, EditAnywhere, Category = "AJA", meta = (MediaPort = "MediaPort"))
	FAjaMediaMode MediaMode;
};

USTRUCT()
struct FAjaOutputPortSettings
{
	GENERATED_BODY()

public:
	/**
	 * The AJA Device and port to output to.
	 * This combines the device ID, and the output port.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "AJA")
	FAjaMediaPort MediaPort;

	/** The signal output mode. */
	UPROPERTY(Config, EditAnywhere, Category = AJA, meta = (CustomizeAsInput = "false", MediaPort = "MediaPort"))
	FAjaMediaMode MediaMode;
};



/**
 * Settings for the AjaMedia plug-in.
 */
UCLASS(config=AjaMedia)
class AJAMEDIA_API UAjaMediaSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category = PortSettings)
	TArray<FAjaInputPortSettings> InputPortSettings;

	UPROPERTY(config, EditAnywhere, Category = PortSettings)
	TArray<FAjaOutputPortSettings> OutputPortSettings;

public:
	/** Returns MediaMode associated to MediaPort argument */
	FAjaMediaMode GetInputMediaMode(const FAjaMediaPort& InMediaPort) const;
	FAjaMediaMode GetOutputMediaMode(const FAjaMediaPort& InMediaPort) const;
};

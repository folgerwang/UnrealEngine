// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AjaMediaFinder.h"

#include "AjaMediaOutput.generated.h"

/**
 * Option for Aja output formats.
 */
UENUM(BlueprintType)
enum class EAjaMediaOutputType : uint8
{
	FillOnly		UMETA(Tooltip="Fill will be on the provided FillPort"),
	FillAndKey		UMETA(Tooltip="Fill will be on provided FillPort and Key will be on KeyPort"),
};

UENUM()
enum class EAjaMediaOutputReferenceType
{
	FreeRun,
	External,
	Input
};

/**
 * Output Media for Aja streams.
 * The output format is ARGB8. 
 */
UCLASS(BlueprintType)
class AJAMEDIA_API UAjaMediaOutput : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Whether to output the fill or the fill and key.
	 * If the key is selected, the pin need to be FillPort.Port + 1.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA")
	EAjaMediaOutputType OutputType;

	/** The signal output mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AJA, AssetRegistrySearchable, meta=(CustomizeAsInput="false", MediaPort="FillPort"))
	FAjaMediaMode MediaMode;

	/**
	 * The AJA Device and port to output to.
	 * This combines the device ID, and the output port.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA", AssetRegistrySearchable)
	FAjaMediaPort FillPort;

	/**
	 * The AJA Device and port to output the key to.
	 * Need to be the same Device as the FillPort.
	 * This combines the device ID, and the output port.
	 * @note	'Frame Buffer Pixel Format' must be set to at least 8 bits of alpha.
	 * @note	'Enable alpha channel support in post-processing' must be set to 'Allow through tonemapper'.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA", meta=(EditCondition="IN_CPP"))
	FAjaMediaPort KeyPort;


	/** The AJA Device output sync with either its internal clock, an external reference, or an other input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA")
	EAjaMediaOutputReferenceType OutputReference;

	/**
	 * The AJA Device and port to sync with.
	 * Need to be the same Device as the FillPort.
	 * This combines the device ID, and the output port.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA", meta=(EditCondition="IN_CPP"))
	FAjaMediaPort SyncPort;

public:
	/**
	 * The output of the Audio, Ancillary and/or video will be perform at the same time.
	 * This may decrease transfer performance but each the data will be sync in relation with each other.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Output")
	bool bOutputWithAutoCirculating;

	/** Whether to embed the timecode to the output frame (if enabled by the Engine). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Output")
	bool bOutputTimecode;

	/*
	 * Copy of the "game" frame buffer on the Render Thread or the Game Thread.
	 * The copy may take some time and can lock the thread.
	 * If the copy is on the Render Thread, it will guarantee that the output will be available.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Output")
	bool bCopyVideoOnRenderThread;

	/**
	 * Try to maintain a the engine "Genlock" with the VSync signal.
	 * This is not necessary if you are waiting for the Output frame. You will be "Genlock" once the card output buffer are filled.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Synchronization", meta=(EditCondition="bOutputWithAutoCirculating"))
	bool bWaitForSyncEvent;

public:
	/**
	 * Clear the buffer before filling the output
	 * Will only clear if the game output frame buffer is smaller than the AJA output.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bClearBuffer;

	/**
	 * Color to fill when clearing the buffer
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug", meta=(EditCondition=bClearBuffer))
	FColor ClearBufferColor;

	/**
	 * Encode Timecode in the output
	 * Current value will be white. The format will be encoded in hh:mm::ss::ff. Each value, will be on a different line.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bEncodeTimecodeInTexel;

public:
	bool Validate(FString& FailureReason) const;

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

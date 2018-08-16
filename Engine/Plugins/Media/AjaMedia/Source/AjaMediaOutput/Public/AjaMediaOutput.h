// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"
#include "AjaMediaFinder.h"

#include "AjaMediaOutput.generated.h"

/**
 * Option for Aja output formats.
 */
UENUM()
enum class EAjaMediaOutputType : uint8
{
	FillOnly		UMETA(Tooltip="Fill will be on the provided FillPort"),
	FillAndKey		UMETA(Tooltip="Fill will be on provided FillPort and Key will be on KeyPort"),
};

/**
 * Option for Aja output formats.
 */
UENUM()
enum class EAjaMediaOutputPixelFormat : uint8
{
	PF_8BIT_ARGB UMETA(DisplayName = "8bit RGBA"),
	PF_10BIT_RGB UMETA(DisplayName = "10bit RGB"),
};

UENUM()
enum class EAjaMediaOutputReferenceType
{
	FreeRun,
	External,
	Input
};

/**
 * Output Media for AJA streams.
 */
UCLASS(BlueprintType)
class AJAMEDIAOUTPUT_API UAjaMediaOutput : public UMediaOutput
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Whether to output the fill or the fill and key.
	 * If the key is selected, the pin need to be FillPort.Port + 1.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA")
	EAjaMediaOutputType OutputType;

public:
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


private:
	/** Override project setting's media mode. */
	UPROPERTY()
	bool bIsDefaultModeOverriden;

	/** The signal output format. Uses project settings by default. */
	UPROPERTY(EditAnywhere, Category="AJA", AssetRegistrySearchable, meta=(EditCondition="bIsDefaultModeOverriden", CustomizeAsInput="false", MediaPort="FillPort"))
	FAjaMediaMode MediaMode;

public:
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

	/** Whether to embed the Engine's timecode to the output frame. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Output")
	EAjaMediaTimecodeFormat TimecodeFormat;

	/** Pixel format we wish to capture from and format to send to AJA. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Output")
	EAjaMediaOutputPixelFormat PixelFormat;

	/**
	 * Number of frame used to transfer from the system memory to the AJA card.
	 * A smaller number is most likely miss frame will occur.
	 * A bigger number is most likely to increase latency.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Output", meta=(ClampMin=1, ClampMax=4))
	int32 NumberOfAJABuffers;

	/**
	 * Only make sense in interlaced mode.
	 * When creating a new Frame the 2 fields need to have the same timecode value.
	 * The Engine's need a TimecodeProvider (or the default system clock) that is in sync with the generated fields.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Output", meta=(ClampMin=1, ClampMax=4))
	bool bInterlacedFieldsTimecodeNeedToMatch;

	/** Try to maintain a the engine "Genlock" with the VSync signal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Synchronization")
	bool bWaitForSyncEvent;

public:
	/**
	 * Encode Timecode in the output
	 * Current value will be white. The format will be encoded in hh:mm::ss::ff. Each value, will be on a different line.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug")
	bool bEncodeTimecodeInTexel;

public:
	bool Validate(FString& FailureReason) const;
	FAjaMediaMode GetMediaMode() const;

	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;

	//~ UObject interface
#if WITH_EDITOR
public:
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif
	//~ End UObject interface
};

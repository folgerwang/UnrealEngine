// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseMediaSource.h"

#include "BlackmagicMediaFinder.h"

#include "BlackmagicMediaOutput.generated.h"

/**
 * Option for output formats.
 */
UENUM(BlueprintType)
enum class EBlackmagicMediaOutputType : uint8
{
	FillOnly		UMETA(Tooltip="Fill will be on the provided FillPort"),
	FillAndKey		UMETA(Tooltip="Fill will be on provided FillPort pin and Key will be on FillPort + 1"),
};

/**
 * Output Media for a stream.
 * The output format is ARGB8. 
 */
UCLASS(BlueprintType)
class BLACKMAGICMEDIA_API UBlackmagicMediaOutput : public UObject
{
	GENERATED_BODY()

	UBlackmagicMediaOutput();

public:
	/**
	 * The Device and port to output to".
	 * This combines the device ID, and the output port.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Blackmagic, AssetRegistrySearchable)
	FBlackmagicMediaPort FillPort;

	/** Which mode to use for Output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Blackmagic, AssetRegistrySearchable)
	FBlackmagicMediaModeOutput MediaMode;

	/**
	 * Whether to output the fill or the fill and key.
	 * If the key is selected, the pin need to be FillPort.Port + 1.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Blackmagic)
	EBlackmagicMediaOutputType OutputType;

	/**
	 * Clear the buffer before filling the output
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Buffer)
	bool bClearBuffer;

	/**
	 * Color to fill when clearing the buffer
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Buffer, meta = (EditCondition = bClearBuffer))
	FColor ClearBufferColor;

	/** Whether to embed the timecode to the output frame (if enabled by the Engine). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Synchronization)
	bool bOutputTimecode;

	/*
	 * Copy of the "game" frame buffer on the Render Thread or the Game Thread.
	 * The copy may take some time and can lock the thread.
	 * If the copy is on the Render Thread, it will guarantee that the output will be available.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Synchronization)
	bool bCopyOnRenderThread;

	/**
	 * Wait for an Output Frame to be available on the card.
	 * The card output at a "Genlock" rate.
	 * If you do not wait and the Output Frame is not available, the "Game" frame will be discarded.
	 * If you wait and the Output Frame is not available, the thread is wait (freeze). This can be used as a "Genlock" solution.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Synchronization)
	bool bWaitForOutputFrame;

	/**
	 * Try to maintain a the engine "Genlock" with the VSync signal.
	 * This is not necessary if you are waiting for the Output frame. You will be "Genlock" once the card output buffer are filled.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Synchronization)
	bool bWaitForVSyncEvent;

	/*
	 * The Engine will try to detect when it took to much time and missed the VSync signal.
	 * To do so, it creates another thread.
	 * If false and you missed the VSync opportunity, the engine will stall for 1 VSync.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Synchronization, meta = (EditCondition = bWaitForVSyncEvent))
	bool bVSyncEventOnAnotherThread;

	/**
	 * Encode Timecode in the output
	 * Current value will be white. The format will be encoded in hh:mm::ss::ff. Each value, will be on a different line.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug", meta = (EditCondition = "IN_CPP"))
	bool bEncodeTimecodeInTexel;

};
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "PixelFormat.h"

#include "MediaOutput.generated.h"


class UMediaCapture;

/**
 * Abstract base class for media output.
 *
 * Media output describe the location and/or settings of media objects that can
 * be used to output the content of UE4 to a target device via a MediaCapture.
 */
UCLASS(Abstract, editinlinenew, BlueprintType, hidecategories = (Object))
class MEDIAIOCORE_API UMediaOutput : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Number of texture used to transfer the texture from the GPU to the system memory.
	 * A smaller number is most likely to block the GPU (wait for the transfer to complete).
	 * A bigger number is most likely to increase latency.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Output", meta=(ClampMin=1, ClampMax=4))
	int32 NumberOfTextureBuffers;

	/** Creates the specific implementation of the MediaCapture for the MediaOutput. */
	UFUNCTION(BlueprintCallable, Category="Media|Output")
	UMediaCapture* CreateMediaCapture();

	/**
	 * Validate the media output settings (must be implemented in child classes).
	 *
	 * @return true if validation passed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	virtual bool Validate(FString& OutFailureReason) const;

public:
	virtual FIntPoint GetRequestedSize() const PURE_VIRTUAL(UMediaOutput::GetRequestedSize, return FIntPoint(0, 0); );
	virtual EPixelFormat GetRequestedPixelFormat() const PURE_VIRTUAL(UMediaOutput::GetRequestedPixelFormat, return EPixelFormat::PF_Unknown; );

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() { return nullptr; }
};

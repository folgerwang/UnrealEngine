// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"

#include "ProxyMediaOutput.generated.h"


/**
 * A media output that redirect to another media output.
 */
UCLASS(BlueprintType)
class MEDIAFRAMEWORKUTILITIES_API UProxyMediaOutput
	: public UMediaOutput
{
	GENERATED_BODY()

	UProxyMediaOutput();

private:

	/** Dynamic media output proxy. */
	UPROPERTY(Transient, DuplicateTransient)
	UMediaOutput* DynamicProxy;

	/** Media output proxy. */
	UPROPERTY(EditAnywhere, Category = "Output")
	UMediaOutput* Proxy;

public:

	/**
	 * Get the media output proxy.
	 *
	 * @return The media output, or nullptr if not set.
	 */
	UMediaOutput* GetMediaOutput() const;

	/**
	 * Set the dynamic media output proxy.
	 *
	 * @param InProxy The proxy to use.
	 */
	void SetDynamicMediaOutput(UMediaOutput* InProxy);

public:

	//~ UMediaOutput interface
	virtual bool Validate(FString& OutFailureReason) const override;
	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;

protected:

	virtual UMediaCapture* CreateMediaCaptureImpl() override;

private:

	mutable bool bValidateGuard;
	mutable bool bRequestedSizeGuard;
	mutable bool bRequestedPixelFormatGuard;
	mutable bool bCreateMediaCaptureImplGuard;
};

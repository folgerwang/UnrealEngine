// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaSource.h"

#include "ProxyMediaSource.generated.h"


/**
 * A media source that reditect to another media source.
 */
UCLASS(BlueprintType)
class MEDIAFRAMEWORKUTILITIES_API UProxyMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

	UProxyMediaSource();

private:

	/** Cached media source proxy. */
	UPROPERTY(Transient, DuplicateTransient)
	UMediaSource* DynamicProxy;

	/** Media source proxy. */
	UPROPERTY(EditAnywhere, Category="Source")
	UMediaSource* Proxy;

public:

	/**
	 * Get the media source proxy.
	 *
	 * @return The media source, or nullptr if not set.
	 */
	UMediaSource* GetMediaSource() const;

	/**
	 * Set the dynamic media source proxy.
	 * 
	 * @param InProxy The proxy to use.
	 */
	void SetDynamicMediaSource(UMediaSource* InProxy);

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

public:

	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

private:

	mutable bool bUrlGuard;
	mutable bool bValidateGuard;
	mutable bool bMediaOptionGuard;

};

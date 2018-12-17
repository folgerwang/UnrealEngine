// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpRequest.h"

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class HTTP_API FHttpRequestImpl : public IHttpRequest
{
public:
	// IHttpRequest
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FHttpRequestProgressDelegate& OnRequestProgress() override;
	virtual FHttpRequestHeaderReceivedDelegate& OnHeaderReceived() override;

protected:
	/** 
	 * Broadcast all of our response's headers as having been received
	 * Used when we don't know when we receive headers in our HTTP implementation
	 */
	void BroadcastResponseHeadersReceived();

protected:
	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;

	/** Delegate that will get called for each new header received */
	FHttpRequestHeaderReceivedDelegate HeaderReceivedDelegate;
};

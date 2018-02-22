// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpManager.h"

class FHttpThread;

#if WITH_LIBCURL

typedef void CURLSH;
typedef void CURLM;

class FCurlHttpManager : public FHttpManager
{
public:
	static void InitCurl();
	static void ShutdownCurl();
	static CURLSH* GShareHandle;
	static CURLM * GMultiHandle;

	static struct FCurlRequestOptions
	{
		FCurlRequestOptions()
			:	bVerifyPeer(true)
			,	bDontReuseConnections(false)
			,	CertBundlePath(nullptr)
			,	BufferSize(64*1024)
		{}

		/** Prints out the options to the log */
		void Log();

		/** Whether or not should verify peer certificate (disable to allow self-signed certs) */
		bool bVerifyPeer;

		/** Forbid reuse connections (for debugging purposes, since normally it's faster to reuse) */
		bool bDontReuseConnections;

		/** A path to certificate bundle */
		const char * CertBundlePath;

		/** Receive buffer size */
		int32 BufferSize;
	}
	CurlRequestOptions;

	//~ Begin HttpManager Interface
public:
	virtual bool SupportsDynamicProxy() const override;
protected:
	virtual FHttpThread* CreateHttpThread() override;
	//~ End HttpManager Interface
};

#endif //WITH_LIBCURL

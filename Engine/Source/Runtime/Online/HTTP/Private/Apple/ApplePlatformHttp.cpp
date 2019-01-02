// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Apple/ApplePlatformHttp.h"
#include "AppleHTTP.h"


void FApplePlatformHttp::Init()
{
}


void FApplePlatformHttp::Shutdown()
{
}


IHttpRequest* FApplePlatformHttp::ConstructRequest()
{
	return new FAppleHttpRequest();
}

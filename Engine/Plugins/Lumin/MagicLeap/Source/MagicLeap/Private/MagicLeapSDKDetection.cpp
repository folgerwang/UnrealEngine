// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapSDKDetection.h"
#include "MagicLeapPluginUtil.h"

bool FMagicLeapSDKDetection::bMLSDKPresent = false;

void FMagicLeapSDKDetection::DetectSDK()
{
#if WITH_MLSDK
	FMagicLeapAPISetup APISetup;
	APISetup.Startup();
	bMLSDKPresent = APISetup.LoadDLL(TEXT("ml_perception_client"));
	APISetup.Shutdown();
#endif
}

bool FMagicLeapSDKDetection::IsSDKDetected()
{
	return bMLSDKPresent;
}

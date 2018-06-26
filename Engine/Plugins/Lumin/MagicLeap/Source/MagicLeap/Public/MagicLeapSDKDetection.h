// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_MLSDK

#if PLATFORM_LUMIN
#define ML_FUNCTION_WRAPPER(CODE) \
		CODE;
#else
#define ML_FUNCTION_WRAPPER(CODE) \
		if (FMagicLeapSDKDetection::IsSDKDetected()) \
		{ \
			CODE; \
		}
#endif

#else //WITH_MLSDK
#define ML_FUNCTION_WRAPPER(CODE) 
#endif //WITH_MLSDK


class MAGICLEAP_API FMagicLeapSDKDetection
{
public:
	static void DetectSDK();
	static bool IsSDKDetected();

private:
	static bool bMLSDKPresent;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapHMDFunctionLibrary.h"
#include "MagicLeapHMD.h"
#include "Engine/Engine.h"
#if WITH_MLSDK
#include "ml_version.h"
#endif //WITH_MLSDK

static const FName MLDeviceName(TEXT("MagicLeap"));

// Internal helper.
static FMagicLeapHMD* GetMagicLeapHMD()
{
	IXRTrackingSystem* const XR = GEngine->XRSystem.Get();
	if (XR && (XR->GetSystemName() == MLDeviceName))
	{
		IHeadMountedDisplay* const HMD = XR->GetHMDDevice();
		if (HMD)
		{
			// we know it's a FMagicLeapHMD by the name match above
			return static_cast<FMagicLeapHMD*>(HMD);
		}
	}

	return nullptr;
}

void UMagicLeapHMDFunctionLibrary::SetBasePosition(const FVector& InBasePosition)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetBasePosition(InBasePosition);
	}
}

void UMagicLeapHMDFunctionLibrary::SetBaseOrientation(const FQuat& InBaseOrientation)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetBaseOrientation(InBaseOrientation);
	}
}

void UMagicLeapHMDFunctionLibrary::SetBaseRotation(const FRotator& InBaseRotation)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetBaseRotation(InBaseRotation);
	}
}

void UMagicLeapHMDFunctionLibrary::SetFocusActor(const AActor* InFocusActor)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetFocusActor(InFocusActor);
	}
}

int32 UMagicLeapHMDFunctionLibrary::GetMLSDKVersionMajor()
{
#if WITH_MLSDK
	return MLSDK_VERSION_MAJOR;
#else
	return 0;
#endif //WITH_MLSDK
}

int32 UMagicLeapHMDFunctionLibrary::GetMLSDKVersionMinor()
{
#if WITH_MLSDK
	return MLSDK_VERSION_MINOR;
#else
	return 0;
#endif //WITH_MLSDK
}

int32 UMagicLeapHMDFunctionLibrary::GetMLSDKVersionRevision()
{
#if WITH_MLSDK
	return MLSDK_VERSION_REVISION;
#else
	return 0;
#endif //WITH_MLSDK
}

FString UMagicLeapHMDFunctionLibrary::GetMLSDKVersion()
{
#if WITH_MLSDK
	return TEXT(MLSDK_VERSION_NAME);
#else
	return FString();
#endif //WITH_MLSDK
}

bool UMagicLeapHMDFunctionLibrary::IsRunningOnMagicLeapHMD()
{
#if PLATFORM_LUMIN
	return true;
#else
	return false;
#endif
}

bool UMagicLeapHMDFunctionLibrary::GetHeadTrackingState(FHeadTrackingState& State)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		return HMD->GetHeadTrackingState(State);
	}

	return false;
}

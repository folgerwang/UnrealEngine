// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitTimecodeProvider.h"
#include "HAL/PlatformTime.h"


UAppleARKitTimecodeProvider::UAppleARKitTimecodeProvider()
	: FrameRate(60, 1)
{
}

bool UAppleARKitTimecodeProvider::Initialize(class UEngine*)
{
	return true;
}

void UAppleARKitTimecodeProvider::Shutdown(class UEngine*)
{
}

FTimecode UAppleARKitTimecodeProvider::GetTimecode() const
{
	return FTimecode(FPlatformTime::Seconds(), FrameRate, FTimecode::IsDropFormatTimecodeSupported(FrameRate), true);
}

FFrameRate UAppleARKitTimecodeProvider::GetFrameRate() const
{
	return FrameRate;
}

ETimecodeProviderSynchronizationState UAppleARKitTimecodeProvider::GetSynchronizationState() const
{
	return ETimecodeProviderSynchronizationState::Synchronized;
}


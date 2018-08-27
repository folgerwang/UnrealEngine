// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealityFunctionLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "WindowsMixedRealityHMD.h"

UWindowsMixedRealityFunctionLibrary::UWindowsMixedRealityFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

WindowsMixedReality::FWindowsMixedRealityHMD* GetWindowsMixedRealityHMD() noexcept
{
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FName("WindowsMixedRealityHMD")))
	{
		return static_cast<WindowsMixedReality::FWindowsMixedRealityHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

FString UWindowsMixedRealityFunctionLibrary::GetVersionString()
{
	WindowsMixedReality::FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();
	if (hmd == nullptr)
	{
		return FString();
	}

	return hmd->GetVersionString();
}

void UWindowsMixedRealityFunctionLibrary::ToggleImmersive(bool immersive)
{
	WindowsMixedReality::FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();
	if (hmd == nullptr)
	{
		return;
	}

	hmd->EnableStereo(immersive);
}

bool UWindowsMixedRealityFunctionLibrary::IsCurrentlyImmersive()
{
	WindowsMixedReality::FWindowsMixedRealityHMD* hmd = GetWindowsMixedRealityHMD();
	if (hmd == nullptr)
	{
		return false;
	}

	return hmd->IsCurrentlyImmersive();
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LuminRuntimeSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "CoreGlobals.h"
#include "Misc/App.h"

#if WITH_EDITOR

bool ULuminRuntimeSettings::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULuminRuntimeSettings, bBuildWithNvTegraGfxDebugger)) ||
		(InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULuminRuntimeSettings, bUseMobileRendering)))
	{
		return !FApp::IsEngineInstalled();
	}

	return ParentVal;
}

void ULuminRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	GConfig->Flush(1);
}

#endif
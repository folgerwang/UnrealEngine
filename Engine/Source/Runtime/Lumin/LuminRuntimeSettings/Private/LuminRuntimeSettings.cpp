// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LuminRuntimeSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "CoreGlobals.h"

#if WITH_EDITOR

void ULuminRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	GConfig->Flush(1);
}

#endif
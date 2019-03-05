// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingSettings.h"

ULiveCodingSettings::ULiveCodingSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIncludeProjectModules = true;
	bIncludeProjectPluginModules = true;
}

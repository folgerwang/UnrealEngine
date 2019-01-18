// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "USDLevelInfo.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "IPythonScriptPlugin.h"

AUSDLevelInfo::AUSDLevelInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FileScale = 1.0;
}

void AUSDLevelInfo::SaveUSD()
{
	if (IPythonScriptPlugin::Get()->IsPythonAvailable())
	{
		IPythonScriptPlugin::Get()->ExecPythonCommand(TEXT("import usd_unreal.export_level; usd_unreal.export_level.export_current_level(None)"));
	}
}


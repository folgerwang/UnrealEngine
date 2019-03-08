// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingSettings.h"
#include "Misc/App.h"

ULiveCodingSettings::ULiveCodingSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	UProperty* EngineModulesProperty = StaticClass()->FindPropertyByName("bIncludeEngineModules");
	check(EngineModulesProperty != nullptr);

	UProperty* EnginePluginModulesProperty = StaticClass()->FindPropertyByName("bIncludeEnginePluginModules");
	check(EnginePluginModulesProperty != nullptr);

	if (FApp::IsEngineInstalled())
	{
		EngineModulesProperty->ClearPropertyFlags(CPF_Edit);
		EnginePluginModulesProperty->ClearPropertyFlags(CPF_Edit);
	}

	bIncludeProjectModules = true;
	bIncludeProjectPluginModules = true;
}

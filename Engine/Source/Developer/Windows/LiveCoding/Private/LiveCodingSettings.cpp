// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingSettings.h"
#include "Misc/App.h"

ULiveCodingSettings::ULiveCodingSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	UProperty* EngineModulesProperty = StaticClass()->FindPropertyByName("bPreloadEngineModules");
	check(EngineModulesProperty != nullptr);

	UProperty* EnginePluginModulesProperty = StaticClass()->FindPropertyByName("bPreloadEnginePluginModules");
	check(EnginePluginModulesProperty != nullptr);

	if (FApp::IsEngineInstalled())
	{
		EngineModulesProperty->ClearPropertyFlags(CPF_Edit);
		EnginePluginModulesProperty->ClearPropertyFlags(CPF_Edit);
	}

	bPreloadProjectModules = true;
	bPreloadProjectPluginModules = true;
}

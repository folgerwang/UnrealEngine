// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PythonScriptPluginSettings.h"

#define LOCTEXT_NAMESPACE "PythonScriptPlugin"

UPythonScriptPluginSettings::UPythonScriptPluginSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName  = TEXT("Python");
}

FText UPythonScriptPluginSettings::GetSectionText() const
{
	return LOCTEXT("SettingsDisplayName", "Python");
}

#undef LOCTEXT_NAMESPACE

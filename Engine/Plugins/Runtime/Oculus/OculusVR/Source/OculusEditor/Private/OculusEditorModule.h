// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IOculusEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class FToolBarBuilder;
class FMenuBuilder;

#define OCULUS_EDITOR_MODULE_NAME "OculusEditor"


class FOculusEditorModule : public IOculusEditorModule
{
public:
	FOculusEditorModule() {};

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void PostLoadCallback() override;

	void RegisterSettings();
	void UnregisterSettings();
	
	void PluginButtonClicked();
	FReply PluginClickFn(bool text);

public:
	static const FName OculusPerfTabName;

private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};

class IDetailLayoutBuilder;

class FOculusHMDSettingsDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

	FReply PluginClickFn(bool text);
};

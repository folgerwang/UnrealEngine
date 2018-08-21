#pragma once

#include "CoreMinimal.h"
#include "IOculusEditorModule.h"
#include "Modules/ModuleInterface.h"

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

	void RegisterSettings();
	void UnregisterSettings();
	
	void PluginButtonClicked();

public:
	static const FName OculusPerfTabName;

private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};

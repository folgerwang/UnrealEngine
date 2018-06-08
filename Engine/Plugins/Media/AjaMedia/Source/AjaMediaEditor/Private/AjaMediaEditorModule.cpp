// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaFinder.h"

#include "AjaMediaSettings.h"
#include "AjaMediaFinder.h"

#include "Customizations/AjaMediaPortCustomization.h"
#include "Customizations/AjaMediaModeCustomization.h"

#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "AjaMediaEditor"

/**
 * Implements the AjaMediaEditor module.
 */
class FAjaMediaEditorModule : public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		RegisterCustomizations();
	}

	virtual void ShutdownModule() override
	{
		if (!UObjectInitialized() && !GIsRequestingExit)
		{
			UnregisterCustomizations();
		}
	}

protected:

	/** Register details view customizations. */
	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(FAjaMediaPort::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAjaMediaPortCustomization::MakeInstance));
	
		PropertyModule.RegisterCustomPropertyTypeLayout(FAjaMediaModeInput::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAjaMediaModeCustomization::MakeInputInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FAjaMediaModeOutput::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAjaMediaModeCustomization::MakeOutputInstance));
	}

	/** Unregister details view customizations. */
	void UnregisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAjaMediaPort::StaticStruct()->GetFName());

		PropertyModule.UnregisterCustomPropertyTypeLayout(FAjaMediaModeInput::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAjaMediaModeOutput::StaticStruct()->GetFName());
	}

};


IMPLEMENT_MODULE(FAjaMediaEditorModule, AjaMediaEditor);

#undef LOCTEXT_NAMESPACE


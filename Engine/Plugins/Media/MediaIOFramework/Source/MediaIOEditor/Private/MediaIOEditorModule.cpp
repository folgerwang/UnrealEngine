// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "MediaIOEditorModule.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"

#include "Customizations/MediaIODeviceCustomization.h"
#include "Customizations/MediaIOConfigurationCustomization.h"
#include "Customizations/MediaIOOutputConfigurationCustomization.h"


DEFINE_LOG_CATEGORY(LogMediaIOEditor);

class FMediaIOEditorModule : public IModuleInterface
{
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

private:
	/** Register details view customizations. */
	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(FMediaIOConfiguration::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMediaIOConfigurationCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FMediaIODevice::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMediaIODeviceCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FMediaIOOutputConfiguration::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMediaIOOutputConfigurationCustomization::MakeInstance));
	}

	/** Unregister details view customizations. */
	void UnregisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FMediaIODevice::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FMediaIOConfiguration::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FMediaIOOutputConfiguration::StaticStruct()->GetFName());
	}
};

IMPLEMENT_MODULE(FMediaIOEditorModule, MediaIOEditor)

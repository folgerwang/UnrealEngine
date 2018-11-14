// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOSRuntimeSettings.h"
#include "ISettingsModule.h"
#include "Features/IModularFeatures.h"
#include "ISettingsSection.h"
#include "IOSCustomIconProjectBuildMutatorFeature.h"

/**
 * Module for iOS as a target platform
 */
class FIOSPlatformEditorModule : public IModuleInterface
{
	// IModuleInterface interface
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	virtual void HandleSelectIOSSection();

	// Delegate to notify interested parties when the client sources have changed
	static FSimpleMulticastDelegate OnSelect;

private:
	FIOSCustomIconProjectBuildMutatorFeature ProjectBuildMutator;
};


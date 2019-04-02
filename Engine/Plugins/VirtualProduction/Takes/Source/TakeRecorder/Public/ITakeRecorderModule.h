// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FExtender;
class UTakeRecorderSources;

DECLARE_DELEGATE_TwoParams(FOnExtendSourcesMenu, TSharedRef<FExtender>, UTakeRecorderSources*)

/**
 * Public module interface for the Take Recorder module
 */
class ITakeRecorderModule : public IModuleInterface
{
public:


	/** The name under which the take recorder tab is registered and invoked */
	static FName TakeRecorderTabName;


	/** The default label for the take recorder tab */
	static FText TakeRecorderTabLabel;

	/** The tab name for the takes browser tab */
	static FName TakesBrowserTabName;

	/** The default label for the takes browser */
	static FText TakesBrowserTabLabel;

	/** The Takes Browser Content Browser Instance Name */
	static FName TakesBrowserInstanceName;

	/**
	 * Register a new extension callback for the 'Add Source' menu
	 */
	virtual FDelegateHandle RegisterSourcesMenuExtension(const FOnExtendSourcesMenu& InExtension) = 0;


	/**
	 * Unregister a previously registered extension callback for the 'Add Source' menu
	 */
	virtual void UnregisterSourcesMenuExtension(FDelegateHandle Handle) = 0;


	/**
	 * Register a new class default object that should appear on the take recorder project settings
	 */
	virtual void RegisterSettingsObject(UObject* InSettingsObject) = 0;
};

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules
 * within this plugin.
 */
class IMagicLeapScreensPlugin : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IMagicLeapScreensPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IMagicLeapScreensPlugin>( "MagicLeapScreens" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "MagicLeapScreens" );
	}

	virtual bool Tick(float DeltaTime) = 0;

	virtual bool IsEngineLoopInitComplete() const = 0;

	virtual void OnEngineLoopInitComplete() = 0;

	FTickerDelegate TickDelegate;
	FDelegateHandle TickDelegateHandle;
};

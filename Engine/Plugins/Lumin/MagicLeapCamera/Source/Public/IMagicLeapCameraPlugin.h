// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "MagicLeapCameraTypes.h"

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules
 * within this plugin.
 */
class IMagicLeapCameraPlugin : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IMagicLeapCameraPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IMagicLeapCameraPlugin>("MagicLeapCamera");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("MagicLeapCamera");
	}

	virtual bool Tick(float DeltaTime) = 0;
	virtual bool CameraConnect(const FCameraConnect& ResultDelegate) { return true; }
	virtual bool CameraDisconnect(const FCameraDisconnect& ResultDelegate) { return true; }
	virtual int64 GetPreviewHandle() const = 0;
};

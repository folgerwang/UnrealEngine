// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"

#define SpatialInputName "WindowsMixedRealitySpatialInput"

/**
* The public interface to this module.  In most cases, this interface is only public to sibling modules
* within this plugin.
*/
class IWindowsMixedRealitySpatialInputPlugin : public IInputDeviceModule
{
public:
	/**
	* Creates a Windows Mixed Reality Spatial Input device using default settings.
	*
	* @param InMessageHandler - Message handler used to send controller messages.
	*/	
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(
		const TSharedRef<FGenericApplicationMessageHandler> & InMessageHandler) = 0;

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IWindowsMixedRealitySpatialInputPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IWindowsMixedRealitySpatialInputPlugin >(SpatialInputName);
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(SpatialInputName);
	}
};

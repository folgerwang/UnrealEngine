// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"
#include "DisplayClusterInputTypes.h"


/**
 * Public interface to this module. In most cases, this interface is only public to sibling modules within this plugin.
 */
class IDisplayClusterInputModule : public IInputDeviceModule
{
public:
	static constexpr auto ModuleName = TEXT("DisplayClusterInput");

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IDisplayClusterInputModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IDisplayClusterInputModule >(IDisplayClusterInputModule::ModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(IDisplayClusterInputModule::ModuleName);
	}

	/**
	* Create new bind from vrpn device channel to UE4 target by user friendly target name
	*
	* @return True, if bind created successfully
	*/	
	virtual bool BindVrpnChannel(const FString& VrpnDeviceId, uint32 VrpnChannel, const FString& BindTargetName) = 0;

	/*
	 * Bind all keyboard keys to ue4 (default keyboard and|or nDisplay second keyboard namespaces)
	 *
	 * @return true, if vrpn keyboard name is valid and op success
	 */
	virtual bool SetVrpnKeyboardReflectionMode(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode) = 0;
};

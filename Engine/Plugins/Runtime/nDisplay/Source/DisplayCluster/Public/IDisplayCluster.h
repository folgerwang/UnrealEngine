// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "DisplayClusterOperationMode.h"


struct IDisplayClusterRenderManager;
struct IDisplayClusterClusterManager;
struct IDisplayClusterInputManager;
struct IDisplayClusterConfigManager;
struct IDisplayClusterGameManager;


/**
 * Public module interface
 */
struct IDisplayCluster
	: public IModuleInterface
{
	static constexpr auto ModuleName = "DisplayCluster";

	virtual ~IDisplayCluster() = 0
	{ }

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IDisplayCluster& Get()
	{
		return FModuleManager::LoadModuleChecked<IDisplayCluster>(IDisplayCluster::ModuleName);
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(IDisplayCluster::ModuleName);
	}


	/**
	* Checks if the module has been initialized.
	*
	* @return Is initialized
	*/
	virtual bool IsModuleInitialized() const = 0;

	/**
	* Returns current operation mode.
	*
	* @return Display Cluster operation mode
	*/
	virtual EDisplayClusterOperationMode GetOperationMode() const = 0;

	/**
	* Access to the device manager.
	*
	* @return Current device manager or nullptr
	*/
	virtual IDisplayClusterRenderManager* GetRenderMgr() const = 0;

	/**
	* Access to the cluster manager.
	*
	* @return Current cluster manager or nullptr
	*/
	virtual IDisplayClusterClusterManager* GetClusterMgr() const = 0;

	/**
	* Access to the input manager.
	*
	* @return Current cluster manager or nullptr
	*/
	virtual IDisplayClusterInputManager* GetInputMgr() const = 0;

	/**
	* Access to the config manager.
	*
	* @return Current config manager or nullptr
	*/
	virtual IDisplayClusterConfigManager* GetConfigMgr() const = 0;

	/**
	* Access to the game manager.
	*
	* @return Current game manager or nullptr
	*/
	virtual IDisplayClusterGameManager* GetGameMgr() const = 0;
};

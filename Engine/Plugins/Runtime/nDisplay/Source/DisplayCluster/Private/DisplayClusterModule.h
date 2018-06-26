// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPDisplayCluster.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Game/IPDisplayClusterGameManager.h"
#include "Input/IPDisplayClusterInputManager.h"
#include "Render/IPDisplayClusterRenderManager.h"


/**
 * Display Cluster module implementation
 */
class FDisplayClusterModule :
	public  IPDisplayCluster
{
public:
	FDisplayClusterModule();
	virtual ~FDisplayClusterModule();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayCluster
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual IDisplayClusterRenderManager*    GetRenderMgr()    const override { return MgrRender; }
	virtual IDisplayClusterClusterManager*   GetClusterMgr()   const override { return MgrCluster; }
	virtual IDisplayClusterInputManager*     GetInputMgr()     const override { return MgrInput; }
	virtual IDisplayClusterConfigManager*    GetConfigMgr()    const override { return MgrConfig; }
	virtual IDisplayClusterGameManager*      GetGameMgr()      const override { return MgrGame; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayCluster
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual IPDisplayClusterRenderManager*    GetPrivateRenderMgr()    const override { return MgrRender; }
	virtual IPDisplayClusterClusterManager*   GetPrivateClusterMgr()   const override { return MgrCluster; }
	virtual IPDisplayClusterInputManager*     GetPrivateInputMgr()     const override { return MgrInput; }
	virtual IPDisplayClusterConfigManager*    GetPrivateConfigMgr()    const override { return MgrConfig; }
	virtual IPDisplayClusterGameManager*      GetPrivateGameMgr()      const override { return MgrGame; }

	virtual EDisplayClusterOperationMode       GetOperationMode() const override
	{ return CurrentOperationMode; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init(EDisplayClusterOperationMode OperationMode) override;
	virtual void Release() override;
	virtual bool StartSession(const FString& configPath, const FString& nodeId) override;
	virtual void EndSession() override;
	virtual bool StartScene(UWorld* pWorld) override;
	virtual void EndScene() override;
	virtual void PreTick(float DeltaSeconds) override;

private:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IModuleInterface
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
#if 0
	virtual void PreUnloadCallback() override;
	virtual void PostLoadCallback() override;
	virtual bool SupportsDynamicReloading() override;
	virtual bool SupportsAutomaticShutdown() override;
	virtual bool IsGameModule() const override;
#endif

private:
	// DisplayCluster subsystems
	IPDisplayClusterClusterManager*   MgrCluster   = nullptr;
	IPDisplayClusterRenderManager*    MgrRender    = nullptr;
	IPDisplayClusterInputManager*     MgrInput     = nullptr;
	IPDisplayClusterConfigManager*    MgrConfig    = nullptr;
	IPDisplayClusterGameManager*      MgrGame      = nullptr;
	
	// Array of available managers
	TArray<IPDisplayClusterManager*> Managers;

	// Runtime
	EDisplayClusterOperationMode CurrentOperationMode = EDisplayClusterOperationMode::Disabled;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameEngine.h"

#include "Config/DisplayClusterConfigTypes.h"
#include "DisplayClusterOperationMode.h"

#include "DisplayClusterGameEngine.generated.h"


struct IPDisplayClusterClusterManager;
struct IPDisplayClusterNodeController;
struct IPDisplayClusterInputManager;


/**
 * Extended game engine
 */
UCLASS()
class DISPLAYCLUSTER_API UDisplayClusterGameEngine
	: public UGameEngine
{
	GENERATED_BODY()
	
public:
	virtual void Init(class IEngineLoop* InEngineLoop) override;
	virtual void PreExit() override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual bool LoadMap(FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error) override;

protected:
	virtual bool InitializeInternals();
	EDisplayClusterOperationMode DetectOperationMode();

private:
	IPDisplayClusterClusterManager* ClusterMgr = nullptr;
	IPDisplayClusterNodeController* NodeController = nullptr;
	IPDisplayClusterInputManager*   InputMgr = nullptr;

	FDisplayClusterConfigDebug CfgDebug;
	EDisplayClusterOperationMode OperationMode = EDisplayClusterOperationMode::Disabled;
};

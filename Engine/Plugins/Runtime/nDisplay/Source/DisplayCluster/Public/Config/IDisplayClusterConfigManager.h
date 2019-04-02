// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterConfigTypes.h"


/**
 * Public config manager interface
 */
class IDisplayClusterConfigManager
{
public:
	virtual ~IDisplayClusterConfigManager()
	{ }

	virtual int32 GetClusterNodesAmount() const = 0;
	virtual TArray<FDisplayClusterConfigClusterNode> GetClusterNodes() const = 0;
	virtual bool GetClusterNode(int32 idx, FDisplayClusterConfigClusterNode& cnode) const = 0;
	virtual bool GetClusterNode(const FString& id, FDisplayClusterConfigClusterNode& cnode) const = 0;
	virtual bool GetMasterClusterNode(FDisplayClusterConfigClusterNode& cnode) const = 0;

	virtual int32 GetWindowsAmount() const = 0;
	virtual TArray<FDisplayClusterConfigWindow> GetWindows() const = 0;
	virtual bool GetWindow(const FString& ID, FDisplayClusterConfigWindow& Window) const = 0;
	virtual bool GetMasterWindow(FDisplayClusterConfigWindow& Window) const = 0;

	virtual int32 GetScreensAmount() const = 0;
	virtual TArray<FDisplayClusterConfigScreen> GetScreens() const = 0;
	virtual bool GetScreen(int32 idx, FDisplayClusterConfigScreen& screen) const = 0;
	virtual bool GetScreen(const FString& id, FDisplayClusterConfigScreen& screen) const = 0;

	virtual int32 GetCamerasAmount() const = 0;
	virtual TArray<FDisplayClusterConfigCamera> GetCameras() const = 0;
	virtual bool GetCamera(int32 idx, FDisplayClusterConfigCamera& camera) const = 0;
	virtual bool GetCamera(const FString& id, FDisplayClusterConfigCamera& camera) const = 0;

	virtual int32 GetViewportsAmount() const = 0;
	virtual TArray<FDisplayClusterConfigViewport> GetViewports() const = 0;
	virtual bool GetViewport(int32 idx, FDisplayClusterConfigViewport& viewport) const = 0;
	virtual bool GetViewport(const FString& id, FDisplayClusterConfigViewport& viewport) const = 0;

	virtual int32 GetSceneNodesAmount() const = 0;
	virtual TArray<FDisplayClusterConfigSceneNode> GetSceneNodes() const = 0;
	virtual bool GetSceneNode(int32 idx, FDisplayClusterConfigSceneNode& snode) const = 0;
	virtual bool GetSceneNode(const FString& id, FDisplayClusterConfigSceneNode& snode) const = 0;

	virtual int32 GetInputDevicesAmount() const = 0;
	virtual TArray<FDisplayClusterConfigInput> GetInputDevices() const = 0;
	virtual bool GetInputDevice(int32 idx, FDisplayClusterConfigInput& input) const = 0;
	virtual bool GetInputDevice(const FString& id, FDisplayClusterConfigInput& input) const = 0;

	virtual TArray<FDisplayClusterConfigInputSetup> GetInputSetupRecords() const = 0;
	virtual bool GetInputSetupRecord(const FString& id, FDisplayClusterConfigInputSetup& input) const = 0;

	virtual FDisplayClusterConfigGeneral GetConfigGeneral() const = 0;
	virtual FDisplayClusterConfigStereo  GetConfigStereo()  const = 0;
	virtual FDisplayClusterConfigRender  GetConfigRender()  const = 0;
	virtual FDisplayClusterConfigNetwork GetConfigNetwork() const = 0;
	virtual FDisplayClusterConfigDebug   GetConfigDebug()   const = 0;
	virtual FDisplayClusterConfigCustom  GetConfigCustom()  const = 0;
};

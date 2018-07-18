// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPDisplayClusterConfigManager.h"

#include "Parser/IDisplayClusterConfigParserListener.h"

#include "DisplayClusterBuildConfig.h"


class FDisplayClusterConfigParser;


/**
 * Config manager. Responsible for loading data from config file and providing with it to any other class.
 */
class FDisplayClusterConfigManager
	: public    IPDisplayClusterConfigManager
	, protected IDisplayClusterConfigParserListener
{
public:
	FDisplayClusterConfigManager();
	virtual ~FDisplayClusterConfigManager();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init(EDisplayClusterOperationMode OperationMode) override;
	virtual void Release() override;
	virtual bool StartSession(const FString& configPath, const FString& nodeId) override;
	virtual void EndSession() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterConfigManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual TArray<FDisplayClusterConfigClusterNode> GetClusterNodes() const override;
	virtual int32 GetClusterNodesAmount() const override;
	virtual bool GetClusterNode(int32 idx, FDisplayClusterConfigClusterNode& node) const override;
	virtual bool GetClusterNode(const FString& id, FDisplayClusterConfigClusterNode& node) const override;
	virtual bool GetMasterClusterNode(FDisplayClusterConfigClusterNode& node) const override;
	virtual bool GetLocalClusterNode(FDisplayClusterConfigClusterNode& node) const override;

	virtual TArray<FDisplayClusterConfigScreen> GetScreens() const override;
	virtual int32 GetScreensAmount() const override;
	virtual bool GetScreen(int32 idx, FDisplayClusterConfigScreen& screen) const override;
	virtual bool GetScreen(const FString& id, FDisplayClusterConfigScreen& screen) const override;
	virtual bool GetLocalScreen(FDisplayClusterConfigScreen& screen) const override;

	virtual TArray<FDisplayClusterConfigCamera> GetCameras() const override;
	virtual int32 GetCamerasAmount() const override;
	virtual bool GetCamera(int32 idx, FDisplayClusterConfigCamera& camera) const override;
	virtual bool GetCamera(const FString& id, FDisplayClusterConfigCamera& camera) const override;

	virtual TArray<FDisplayClusterConfigViewport> GetViewports() const override;
	virtual int32 GetViewportsAmount() const override;
	virtual bool GetViewport(int32 idx, FDisplayClusterConfigViewport& viewport) const override;
	virtual bool GetViewport(const FString& id, FDisplayClusterConfigViewport& viewport) const override;
	virtual bool GetLocalViewport(FDisplayClusterConfigViewport& screen) const override;

	virtual TArray<FDisplayClusterConfigSceneNode> GetSceneNodes() const override;
	virtual int32 GetSceneNodesAmount() const override;
	virtual bool GetSceneNode(int32 idx, FDisplayClusterConfigSceneNode& actor) const override;
	virtual bool GetSceneNode(const FString& id, FDisplayClusterConfigSceneNode& actor) const override;

	virtual TArray<FDisplayClusterConfigInput> GetInputDevices() const override;
	virtual int32 GetInputDevicesAmount() const override;
	virtual bool GetInputDevice(int32 idx, FDisplayClusterConfigInput& input) const override;
	virtual bool GetInputDevice(const FString& id, FDisplayClusterConfigInput& input) const override;

	virtual FDisplayClusterConfigGeneral GetConfigGeneral() const override
	{ return CfgGeneral; }

	virtual FDisplayClusterConfigStereo  GetConfigStereo() const override
	{ return CfgStereo; }

	virtual FDisplayClusterConfigRender  GetConfigRender() const override
	{ return CfgRender; }

	virtual FDisplayClusterConfigDebug   GetConfigDebug() const override
	{ return CfgDebug; }

	virtual FDisplayClusterConfigCustom  GetConfigCustom() const override
	{ return CfgCustom; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterConfigManager
	//////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	virtual bool IsRunningDebugAuto() const override
	{ return bIsDebugAuto; }
#endif

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterConfigParserListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void AddClusterNode(const FDisplayClusterConfigClusterNode& cfgCNode) override;
	virtual void AddScreen(const FDisplayClusterConfigScreen& cfgScreen) override;
	virtual void AddViewport(const FDisplayClusterConfigViewport& cfgViewport) override;
	virtual void AddCamera(const FDisplayClusterConfigCamera& cfgCamera) override;
	virtual void AddSceneNode(const FDisplayClusterConfigSceneNode& cfgSNode)  override;
	virtual void AddGeneral(const FDisplayClusterConfigGeneral& cfgGeneral)  override;
	virtual void AddRender(const FDisplayClusterConfigRender& cfgRender)  override;
	virtual void AddStereo(const FDisplayClusterConfigStereo& cfgStereo)  override;
	virtual void AddDebug(const FDisplayClusterConfigDebug& cfgDebug)  override;
	virtual void AddInput(const FDisplayClusterConfigInput& cfgInput)  override;
	virtual void AddCustom(const FDisplayClusterConfigCustom& cfgCustom) override;

private:
	enum class EConfigFileType
	{
		Unknown,
#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
		DebugAuto,
#endif
		Text,
		Xml
	};

	template <typename DataType>
	bool GetItem(const TArray<DataType>& container, uint32 idx, DataType& item, const FString& logHeader) const;

	template <typename DataType>
	bool GetItem(const TArray<DataType>& container, const FString& id, DataType& item, const FString& logHeader) const;

	EConfigFileType GetConfigFileType(const FString& cfgPath) const;
	bool LoadConfig(const FString& cfgPath);
	void ResetConfigData();

private:
	FString ConfigPath;
	FString ClusterNodeId;

	TArray<FDisplayClusterConfigClusterNode> CfgClusterNodes;
	TArray<FDisplayClusterConfigScreen>      CfgScreens;
	TArray<FDisplayClusterConfigViewport>    CfgViewports;
	TArray<FDisplayClusterConfigCamera>      CfgCameras;
	TArray<FDisplayClusterConfigSceneNode>   CfgSceneNodes;
	TArray<FDisplayClusterConfigInput>       CfgInputDevices;

	FDisplayClusterConfigGeneral CfgGeneral;
	FDisplayClusterConfigStereo  CfgStereo;
	FDisplayClusterConfigRender  CfgRender;
	FDisplayClusterConfigDebug   CfgDebug;
	FDisplayClusterConfigCustom  CfgCustom;

#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	bool bIsDebugAuto = false;
#endif
};

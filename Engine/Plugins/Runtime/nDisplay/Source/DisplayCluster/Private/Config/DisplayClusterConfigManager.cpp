// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigManager.h"

#include "Cluster/IPDisplayClusterClusterManager.h"

#include "Config/DisplayClusterConfigTypes.h"
#include "Config/Parser/DisplayClusterConfigParserText.h"
#include "Config/Parser/DisplayClusterConfigParserXml.h"
#include "Config/Parser/DisplayClusterConfigParserDebugAuto.h"

#include "DisplayClusterBuildConfig.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/Paths.h"
#include "DisplayClusterGlobals.h"
#include "DisplayClusterStrings.h"
#include "IPDisplayCluster.h"


FDisplayClusterConfigManager::FDisplayClusterConfigManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
}

FDisplayClusterConfigManager::~FDisplayClusterConfigManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigManager::Init(EDisplayClusterOperationMode OperationMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	
	return true;
}

void FDisplayClusterConfigManager::Release()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
}

bool FDisplayClusterConfigManager::StartSession(const FString& configPath, const FString& nodeId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	
	ConfigPath = configPath;
	ClusterNodeId = nodeId;

	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Starting session with config: %s"), *ConfigPath);

	// Load data
	return LoadConfig(ConfigPath);
}

void FDisplayClusterConfigManager::EndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);

	ResetConfigData();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterConfigManager
//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster nodes
TArray<FDisplayClusterConfigClusterNode> FDisplayClusterConfigManager::GetClusterNodes() const
{
	return CfgClusterNodes;
}

int32 FDisplayClusterConfigManager::GetClusterNodesAmount() const
{
	return CfgClusterNodes.Num();
}

bool FDisplayClusterConfigManager::GetClusterNode(int32 idx, FDisplayClusterConfigClusterNode& node) const
{
	return GetItem(CfgClusterNodes, idx, node, FString("GetNode"));
}

bool FDisplayClusterConfigManager::GetClusterNode(const FString& id, FDisplayClusterConfigClusterNode& node) const
{
	return GetItem(CfgClusterNodes, id, node, FString("GetNode"));
}

bool FDisplayClusterConfigManager::GetMasterClusterNode(FDisplayClusterConfigClusterNode& node) const
{
	const FDisplayClusterConfigClusterNode* const pFound = CfgClusterNodes.FindByPredicate([](const FDisplayClusterConfigClusterNode& item)
	{
		return item.IsMaster == true;
	});

	if (!pFound)
	{
		UE_LOG(LogDisplayClusterConfig, Error, TEXT("Master node configuration not found"));
		return false;
	}

	node = *pFound;
	return true;
}

bool FDisplayClusterConfigManager::GetLocalClusterNode(FDisplayClusterConfigClusterNode& node) const
{
	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	const FString nodeId = GDisplayCluster->GetPrivateClusterMgr()->GetNodeId();
	return GetItem(CfgClusterNodes, nodeId, node, FString("GetLocalNode"));
}


// Screens
TArray<FDisplayClusterConfigScreen> FDisplayClusterConfigManager::GetScreens() const
{
	return CfgScreens;
}

int32 FDisplayClusterConfigManager::GetScreensAmount() const
{
	return CfgScreens.Num();
}

bool FDisplayClusterConfigManager::GetScreen(int32 idx, FDisplayClusterConfigScreen& screen) const
{
	return GetItem(CfgScreens, idx, screen, FString("GetScreen"));
}

bool FDisplayClusterConfigManager::GetScreen(const FString& id, FDisplayClusterConfigScreen& screen) const
{
	return GetItem(CfgScreens, id, screen, FString("GetScreen"));
}

bool FDisplayClusterConfigManager::GetLocalScreen(FDisplayClusterConfigScreen& screen) const
{
	FDisplayClusterConfigClusterNode localNode;
	if (GetLocalClusterNode(localNode))
	{
		return GetItem(CfgScreens, localNode.ScreenId, screen, FString("GetLocalScreen"));
	}

	return false;
}


// Cameras
TArray<FDisplayClusterConfigCamera> FDisplayClusterConfigManager::GetCameras() const
{
	return CfgCameras;
}

int32 FDisplayClusterConfigManager::GetCamerasAmount() const
{
	return CfgCameras.Num();
}

bool FDisplayClusterConfigManager::GetCamera(int32 idx, FDisplayClusterConfigCamera& camera) const
{
	return GetItem(CfgCameras, idx, camera, FString("GetCamera"));
}

bool FDisplayClusterConfigManager::GetCamera(const FString& id, FDisplayClusterConfigCamera& camera) const
{
	return GetItem(CfgCameras, id, camera, FString("GetCamera"));
}


// Viewports
TArray<FDisplayClusterConfigViewport> FDisplayClusterConfigManager::GetViewports() const
{
	return CfgViewports;
}

int32 FDisplayClusterConfigManager::GetViewportsAmount() const
{
	return static_cast<uint32>(CfgViewports.Num());
}

bool FDisplayClusterConfigManager::GetViewport(int32 idx, FDisplayClusterConfigViewport& viewport) const
{
	return GetItem(CfgViewports, idx, viewport, FString("GetViewport"));
}

bool FDisplayClusterConfigManager::GetViewport(const FString& id, FDisplayClusterConfigViewport& viewport) const
{
	return GetItem(CfgViewports, id, viewport, FString("GetViewport"));
}

//@todo: remove all GetLocal* functions. Config manager doesn't have to know its place in cluster. 
bool FDisplayClusterConfigManager::GetLocalViewport(FDisplayClusterConfigViewport& viewport) const
{
	FDisplayClusterConfigClusterNode localNode;
	if (GetLocalClusterNode(localNode))
	{
		return GetItem(CfgViewports, localNode.ViewportId, viewport, FString("GetLocalViewport"));
	}

	return false;
}


// Scene nodes
TArray<FDisplayClusterConfigSceneNode> FDisplayClusterConfigManager::GetSceneNodes() const
{
	return CfgSceneNodes;
}

int32 FDisplayClusterConfigManager::GetSceneNodesAmount() const
{
	return static_cast<uint32>(CfgSceneNodes.Num());
}

bool FDisplayClusterConfigManager::GetSceneNode(int32 idx, FDisplayClusterConfigSceneNode& actor) const
{
	return GetItem(CfgSceneNodes, idx, actor, FString("GetActor"));
}

bool FDisplayClusterConfigManager::GetSceneNode(const FString& id, FDisplayClusterConfigSceneNode& actor) const
{
	return GetItem(CfgSceneNodes, id, actor, FString("GetActor"));
}


// Input devices
TArray<FDisplayClusterConfigInput> FDisplayClusterConfigManager::GetInputDevices() const
{
	return CfgInputDevices;
}

int32 FDisplayClusterConfigManager::GetInputDevicesAmount() const
{
	return CfgInputDevices.Num();
}

bool FDisplayClusterConfigManager::GetInputDevice(int32 idx, FDisplayClusterConfigInput& input) const
{
	return GetItem(CfgInputDevices, idx, input, FString("GetInputDevice"));
}

bool FDisplayClusterConfigManager::GetInputDevice(const FString& id, FDisplayClusterConfigInput& input) const
{
	return GetItem(CfgInputDevices, id, input, FString("GetInputDevice"));
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterConfigParserListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterConfigManager::AddClusterNode(const FDisplayClusterConfigClusterNode& cfgCNode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found cluster node: %s"), *cfgCNode.ToString());
	CfgClusterNodes.Add(cfgCNode);
}

void FDisplayClusterConfigManager::AddScreen(const FDisplayClusterConfigScreen& cfgScreen)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found screen: %s"), *cfgScreen.ToString());
	CfgScreens.Add(cfgScreen);
}

void FDisplayClusterConfigManager::AddViewport(const FDisplayClusterConfigViewport& cfgViewport)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found viewport: %s"), *cfgViewport.ToString());
	CfgViewports.Add(cfgViewport);
}

void FDisplayClusterConfigManager::AddCamera(const FDisplayClusterConfigCamera& cfgCamera)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found camera: %s"), *cfgCamera.ToString());
	CfgCameras.Add(cfgCamera);
}

void FDisplayClusterConfigManager::AddSceneNode(const FDisplayClusterConfigSceneNode& cfgSNode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found scene node: %s"), *cfgSNode.ToString());
	CfgSceneNodes.Add(cfgSNode);
}

void FDisplayClusterConfigManager::AddInput(const FDisplayClusterConfigInput& cfgInput)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found input device: %s"), *cfgInput.ToString());
	CfgInputDevices.Add(cfgInput);
}

void FDisplayClusterConfigManager::AddGeneral(const FDisplayClusterConfigGeneral& cfgGeneral)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found general: %s"), *cfgGeneral.ToString());
	CfgGeneral = cfgGeneral;
}

void FDisplayClusterConfigManager::AddRender(const FDisplayClusterConfigRender& cfgRender)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found render: %s"), *cfgRender.ToString());
	CfgRender = cfgRender;
}

void FDisplayClusterConfigManager::AddStereo(const FDisplayClusterConfigStereo& cfgStereo)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found stereo: %s"), *cfgStereo.ToString());
	CfgStereo = cfgStereo;
}

void FDisplayClusterConfigManager::AddDebug(const FDisplayClusterConfigDebug& cfgDebug)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found debug: %s"), *cfgDebug.ToString());
	CfgDebug = cfgDebug;
}

void FDisplayClusterConfigManager::AddCustom(const FDisplayClusterConfigCustom& cfgCustom)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found custom: %s"), *cfgCustom.ToString());
	CfgCustom = cfgCustom;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigManager
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterConfigManager::EConfigFileType FDisplayClusterConfigManager::GetConfigFileType(const FString& cfgPath) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);

#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	if (cfgPath == DisplayClusterStrings::misc::DbgStubConfig)
	{
		UE_LOG(LogDisplayClusterConfig, Log, TEXT("Debug auto config requested"));
		return EConfigFileType::DebugAuto;
	}
#endif

	const FString ext = FPaths::GetExtension(cfgPath).ToLower();
	if (ext == FString(DisplayClusterStrings::cfg::file::FileExtXml).ToLower())
	{
		UE_LOG(LogDisplayClusterConfig, Log, TEXT("XML config: %s"), *cfgPath);
		return EConfigFileType::Xml;
	}
	else if (
		ext == FString(DisplayClusterStrings::cfg::file::FileExtCfg1).ToLower() ||
		ext == FString(DisplayClusterStrings::cfg::file::FileExtCfg2).ToLower() ||
		ext == FString(DisplayClusterStrings::cfg::file::FileExtCfg3).ToLower() ||
		ext == FString(DisplayClusterStrings::cfg::file::FileExtTxt).ToLower())
	{
		UE_LOG(LogDisplayClusterConfig, Log, TEXT("TXT config: %s"), *cfgPath);
		return EConfigFileType::Text;
	}

	UE_LOG(LogDisplayClusterConfig, Warning, TEXT("Unknown file extension: %s"), *ext);
	return EConfigFileType::Unknown;
}

bool FDisplayClusterConfigManager::LoadConfig(const FString& cfgPath)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);

	// Actually the data is reset on EndFrame. This one is a safety call.
	ResetConfigData();

#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	if (cfgPath.Compare(FString(DisplayClusterStrings::misc::DbgStubConfig), ESearchCase::IgnoreCase) != 0 &&
		FPaths::FileExists(cfgPath) == false)
	{
		UE_LOG(LogDisplayClusterConfig, Error, TEXT("File not found: %s"), *cfgPath);
		return false;
	}
#else
	if (FPaths::FileExists(cfgPath) == false)
	{
		UE_LOG(LogDisplayClusterConfig, Error, TEXT("File not found: %s"), *cfgPath);
		return false;
	}
#endif

	// Instantiate appropriate parser
	TUniquePtr<FDisplayClusterConfigParser> parser;
	switch (GetConfigFileType(cfgPath))
	{
	case EConfigFileType::Text:
		parser.Reset(new FDisplayClusterConfigParserText(this));
		break;

	case EConfigFileType::Xml:
		parser.Reset(new FDisplayClusterConfigParserXml(this));
		break;

#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	case EConfigFileType::DebugAuto:
		bIsDebugAuto = true;
		parser.Reset(new FDisplayClusterConfigParserDebugAuto(this));
		break;
#endif

	default:
		UE_LOG(LogDisplayClusterConfig, Error, TEXT("Unknown config type"));
		return false;
	}

	return parser->ParseFile(cfgPath);
}

void FDisplayClusterConfigManager::ResetConfigData()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterConfig);

	CfgClusterNodes.Reset();
	CfgScreens.Reset();
	CfgViewports.Reset();
	CfgCameras.Reset();
	CfgSceneNodes.Reset();
	CfgInputDevices.Reset();

	CfgGeneral = FDisplayClusterConfigGeneral();
	CfgStereo  = FDisplayClusterConfigStereo();
	CfgRender  = FDisplayClusterConfigRender();
	CfgDebug   = FDisplayClusterConfigDebug();
	CfgCustom  = FDisplayClusterConfigCustom();
}

template <typename DataType>
bool FDisplayClusterConfigManager::GetItem(const TArray<DataType>& container, uint32 idx, DataType& item, const FString& logHeader) const
{
	if (idx >= static_cast<uint32>(container.Num()))
	{
		UE_LOG(LogDisplayClusterConfig, Error, TEXT("%s: index is out of bound <%d>"), *logHeader, idx);
		return false;
	}

	item = container[static_cast<int32>(idx)];
	return true;
}

template <typename DataType>
bool FDisplayClusterConfigManager::GetItem(const TArray<DataType>& container, const FString& id, DataType& item, const FString& logHeader) const
{
	auto pFound = container.FindByPredicate([id](const DataType& _item)
	{
		return _item.Id == id;
	});

	if (!pFound)
	{
		UE_LOG(LogDisplayClusterConfig, Warning, TEXT("%s: ID not found <%s>"), *logHeader, *id);
		return false;
	}

	item = *pFound;
	return true;
}

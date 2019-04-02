// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterStringSerializable.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// Base interface for config data holders
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigBase : public IDisplayClusterStringSerializable
{
	virtual ~FDisplayClusterConfigBase()
	{ }

	// Prints in human readable format
	virtual FString ToString() const
	{ return FString("[]"); }

	// Currently no need to serialize the data
	virtual FString SerializeToString() const override final
	{ return FString(); }

	// Deserialization from config file
	virtual bool    DeserializeFromString(const FString& line) override
	{ return true; }
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Config info
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigInfo : public FDisplayClusterConfigBase
{
	FString Version;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster node configuration (separate application)
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigClusterNode : public FDisplayClusterConfigBase
{
	FString Id;
	FString Addr;
	FString WindowId;
	bool    IsMaster = false;
	int32   Port_CS = 41001;
	int32   Port_SS = 41002;
	int32   Port_CE = 41003;
	bool    SoundEnabled = false;
	bool    EyeSwap = false;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Application window configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigWindow : public FDisplayClusterConfigBase
{
	FString Id;
	TArray<FString> ViewportIds;
	bool IsFullscreen = false;
	int32 WinX = 0;
	int32 WinY = 0;
	int32 ResX = 0;
	int32 ResY = 0;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Viewport configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigViewport : public FDisplayClusterConfigBase
{
	FString Id;
	FString ScreenId;
	FIntPoint Loc  = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint::ZeroValue;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Scene node configuration (DisplayCluster hierarchy is built from such nodes)
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigSceneNode : public FDisplayClusterConfigBase
{
	FString  Id;
	FString  ParentId;
	FVector  Loc = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	FString  TrackerId;
	int32    TrackerCh = -1;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Projection screen configuration (used for asymmetric frustum calculation)
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigScreen : public FDisplayClusterConfigSceneNode
{
	FVector2D Size = FVector2D::ZeroVector;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Camera configuration (DisplayCluster camera)
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigCamera : public FDisplayClusterConfigSceneNode
{

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Input device configuration (VRPN and other possible devices)
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigInput : public FDisplayClusterConfigBase
{
	FString Id;
	FString Type;
	FString Params;
	TMap<int32, int32> ChMap;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

struct DISPLAYCLUSTER_API FDisplayClusterConfigInputSetup : public FDisplayClusterConfigBase
{
	// VRPN device unique name
	FString Id;
	// VRPN device channel to bind
	int32 Channel = -1;
	// Keyboard key name (for keyboard devices only)
	FString Key;
	// Target name to bind
	FString BindName;
	
	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// General DisplayCluster configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigGeneral : public FDisplayClusterConfigBase
{
	int32 SwapSyncPolicy = 0;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Render configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigRender : public FDisplayClusterConfigBase
{

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Stereo configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigStereo : public FDisplayClusterConfigBase
{
	float EyeDist = 0.064f;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Network configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigNetwork : public FDisplayClusterConfigBase
{
	int32 ClientConnectTriesAmount    = 10;    // times
	int32 ClientConnectRetryDelay     = 1000;  // ms
	int32 BarrierGameStartWaitTimeout = 30000; // ms
	int32 BarrierWaitTimeout          = 5000;  // ms

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Debug settings
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigDebug : public FDisplayClusterConfigBase
{
	bool  DrawStats = false;
	bool  LagSimulateEnabled = false;
	float LagMaxTime = 0.5f; // seconds

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Custom development settings
//////////////////////////////////////////////////////////////////////////////////////////////
struct DISPLAYCLUSTER_API FDisplayClusterConfigCustom : public FDisplayClusterConfigBase
{
	TMap<FString, FString> Args;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

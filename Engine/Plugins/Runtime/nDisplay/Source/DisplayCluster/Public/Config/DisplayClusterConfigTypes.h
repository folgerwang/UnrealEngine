// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterStringSerializable.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// Base interface for config data holders
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigBase : public IDisplayClusterStringSerializable
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
// Cluster node configuration (separate application)
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigClusterNode : public FDisplayClusterConfigBase
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
struct FDisplayClusterConfigWindow : public FDisplayClusterConfigBase
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
struct FDisplayClusterConfigViewport : public FDisplayClusterConfigBase
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
struct FDisplayClusterConfigSceneNode : public FDisplayClusterConfigBase
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
struct FDisplayClusterConfigScreen : public FDisplayClusterConfigSceneNode
{
	FVector2D Size = FVector2D::ZeroVector;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Camera configuration (DisplayCluster camera)
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigCamera : public FDisplayClusterConfigSceneNode
{

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Input device configuration (VRPN and other possible devices)
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigInput : public FDisplayClusterConfigBase
{
	FString Id;
	FString Type;
	FString Params;
	TMap<int32, int32> ChMap;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// General DisplayCluster configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigGeneral : public FDisplayClusterConfigBase
{
	int32 SwapSyncPolicy = 0;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Render configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigRender : public FDisplayClusterConfigBase
{

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Stereo configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigStereo : public FDisplayClusterConfigBase
{
	float EyeDist = 0.064f;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Network configuration
//////////////////////////////////////////////////////////////////////////////////////////////
struct FDisplayClusterConfigNetwork : public FDisplayClusterConfigBase
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
struct FDisplayClusterConfigDebug : public FDisplayClusterConfigBase
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
struct FDisplayClusterConfigCustom : public FDisplayClusterConfigBase
{
	TMap<FString, FString> Args;

	virtual FString ToString() const override;
	virtual bool    DeserializeFromString(const FString& line) override;
};

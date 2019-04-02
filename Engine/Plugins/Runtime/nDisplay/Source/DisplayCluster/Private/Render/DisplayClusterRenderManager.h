// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/IPDisplayClusterRenderManager.h"

class FDisplayClusterDeviceBase;
class FDisplayClusterNativePresentHandler;

/**
 * Render manager. Responsible for anything related to a visual part.
 */
class FDisplayClusterRenderManager
	: public IPDisplayClusterRenderManager
{
public:
	FDisplayClusterRenderManager();
	virtual ~FDisplayClusterRenderManager();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init(EDisplayClusterOperationMode OperationMode) override;
	virtual void Release() override;
	virtual bool StartSession(const FString& configPath, const FString& nodeId) override;
	virtual void EndSession() override;
	virtual void PreTick(float DeltaSeconds) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void AddViewport(const FString& ViewportId, IDisplayClusterProjectionScreenDataProvider* DataProvider) override;
	virtual void RemoveViewport(const FString& ViewportId) override;
	virtual void RemoveAllViewports() override;
	virtual void SetDesktopStereoParams(float FOV) override;
	virtual void SetDesktopStereoParams(const FVector2D& screenSize, const FIntPoint& screenRes, float screenDist) override;
	virtual void  SetInterpupillaryDistance(float dist) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual void SetEyesSwap(bool swap) override;
	virtual bool GetEyesSwap() const override;
	virtual bool ToggleEyesSwap() override;
	virtual void SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy) override;
	virtual EDisplayClusterSwapSyncPolicy GetSwapSyncPolicy() const override;
	virtual void GetCullingDistance(float& NearDistance, float& FarDistance) const override;
	virtual void SetCullingDistance(float NearDistance, float FarDistance) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterRenderManager
	//////////////////////////////////////////////////////////////////////////////////////////////

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterRenderManager
	//////////////////////////////////////////////////////////////////////////////////////////////

private:
	FDisplayClusterDeviceBase* CreateStereoDevice();
	void ResizeWindow(int32 WinX, int32 WinY, int32 ResX, int32 ResY);
	void OnViewportCreatedHandler();
	void OnBeginDrawHandler();

private:
	EDisplayClusterOperationMode CurrentOperationMode;
	FString ConfigPath;
	FString ClusterNodeId;

	// Interface pointer to eliminate type casting
	IDisplayClusterStereoRendering* StereoDevice = nullptr;
	FDisplayClusterNativePresentHandler* NativePresentHandler;
	bool bWindowAdjusted = false;
};


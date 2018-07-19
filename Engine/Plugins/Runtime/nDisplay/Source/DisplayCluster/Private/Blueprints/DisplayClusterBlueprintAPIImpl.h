// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprints/IDisplayClusterBlueprintAPI.h"
#include "DisplayClusterBlueprintAPIImpl.generated.h"


/**
 * Blueprint API interface implementation
 */
UCLASS()
class DISPLAYCLUSTER_API UDisplayClusterBlueprintAPIImpl
	: public UObject
	, public IDisplayClusterBlueprintAPI
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// DisplayCluster module API
	//////////////////////////////////////////////////////////////////////////////////////////////
	/** Return if the module has been initialized. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is module initialized"), Category = "DisplayCluster")
	virtual bool IsModuleInitialized() override;

	/** Return current operation mode. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get operation mode"), Category = "DisplayCluster")
	virtual EDisplayClusterOperationMode GetOperationMode() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Cluster API
	//////////////////////////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is master node"), Category = "DisplayCluster|Cluster")
	virtual bool IsMaster() override;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is slave node"), Category = "DisplayCluster|Cluster")
	virtual bool IsSlave() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is cluster mode"), Category = "DisplayCluster|Cluster")
	virtual bool IsCluster() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is standalone mode"), Category = "DisplayCluster|Cluster")
	virtual bool IsStandalone() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get node ID"), Category = "DisplayCluster|Cluster")
	virtual FString GetNodeId() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get nodes amount"), Category = "DisplayCluster|Cluster")
	virtual int32 GetNodesAmount() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Config API
	//////////////////////////////////////////////////////////////////////////////////////////////


public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Game API
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Root
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get root"), Category = "DisplayCluster|Game")
	virtual ADisplayClusterPawn* GetRoot() override;

	// Screens
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get active screen"), Category = "DisplayCluster|Game")
	virtual UDisplayClusterScreenComponent* GetActiveScreen() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get screen by ID"), Category = "DisplayCluster|Game")
	virtual UDisplayClusterScreenComponent* GetScreenById(const FString& id) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get all screens"), Category = "DisplayCluster|Game")
	virtual TArray<UDisplayClusterScreenComponent*> GetAllScreens() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of screens"), Category = "DisplayCluster|Game")
	virtual int32 GetScreensAmount() override;

	// Cameras
	/*
	virtual UDisplayClusterCameraComponent*         GetActiveCamera() const override;
	virtual UDisplayClusterCameraComponent*         GetCameraById(const FString& id) const override;
	virtual TArray<UDisplayClusterCameraComponent*> GetAllCameras() const override;
	virtual int32                        GetCamerasAmount() const override;
	virtual void                         SetActiveCamera(int32 idx) override;
	virtual void                         SetActiveCamera(const FString& id) override;
	*/

	// Nodes
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get node by ID"), Category = "DisplayCluster|Game")
	virtual UDisplayClusterSceneComponent* GetNodeById(const FString& id) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get all nodes"), Category = "DisplayCluster|Game")
	virtual TArray<UDisplayClusterSceneComponent*> GetAllNodes() override;

	// Navigation
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get translation direction component"), Category = "DisplayCluster|Game")
	virtual USceneComponent* GetTranslationDirectionComponent() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set translation direction component"), Category = "DisplayCluster|Game")
	virtual void SetTranslationDirectionComponent(USceneComponent* pComp) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set translation direction component by ID"), Category = "DisplayCluster|Game")
	virtual void SetTranslationDirectionComponentId(const FString& id) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get rotate around component"), Category = "DisplayCluster|Game")
	virtual USceneComponent* GetRotateAroundComponent() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set rotate around component"), Category = "DisplayCluster|Game")
	virtual void SetRotateAroundComponent(USceneComponent* pComp) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set rotate around component by ID"), Category = "DisplayCluster|Game")
	virtual void SetRotateAroundComponentId(const FString& id) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Input API
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Device information
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of VRPN axis devices"), Category = "DisplayCluster|Input")
	virtual int32 GetAxisDeviceAmount() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of VRPN button devices"), Category = "DisplayCluster|Input")
	virtual int32 GetButtonDeviceAmount() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of VRPN tracker devices"), Category = "DisplayCluster|Input")
	virtual int32 GetTrackerDeviceAmount() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get IDs of VRPN axis devices"), Category = "DisplayCluster|Input")
	virtual bool GetAxisDeviceIds(TArray<FString>& IDs) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get IDs of VRPN button devices"), Category = "DisplayCluster|Input")
	virtual bool GetButtonDeviceIds(TArray<FString>& IDs) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get IDs of VRPN tracker devices"), Category = "DisplayCluster|Input")
	virtual bool GetTrackerDeviceIds(TArray<FString>& IDs) override;

	// Buttons
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN button state"), Category = "DisplayCluster|Input")
	virtual void GetButtonState(const FString& DeviceId, uint8 DeviceChannel, bool& CurState, bool& IsChannelAvailable) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is VRPN button pressed"), Category = "DisplayCluster|Input")
	virtual void IsButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& CurPressed, bool& IsChannelAvailable) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is VRPN button released"), Category = "DisplayCluster|Input")
	virtual void IsButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& CurReleased, bool& IsChannelAvailable) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Was VRPN button pressed"), Category = "DisplayCluster|Input")
	virtual void WasButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& WasPressed, bool& IsChannelAvailable) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Was VRPN button released"), Category = "DisplayCluster|Input")
	virtual void WasButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& WasReleased, bool& IsChannelAvailable) override;

	// Axes
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN axis value"), Category = "DisplayCluster|Input")
	virtual void GetAxis(const FString& DeviceId, uint8 DeviceChannel, float& Value, bool& IsAvailable) override;

	// Trackers
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN tracker location"), Category = "DisplayCluster|Input")
	virtual void GetTrackerLocation(const FString& DeviceId, uint8 DeviceChannel, FVector& Location, bool& IsChannelAvailable) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN tracker rotation (as quaternion)"), Category = "DisplayCluster|Input")
	virtual void GetTrackerQuat(const FString& DeviceId, uint8 DeviceChannel, FQuat& Rotation, bool& IsChannelAvailable) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Render API
	//////////////////////////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set interpuppillary distance"), Category = "DisplayCluster|Render")
	virtual void SetInterpupillaryDistance(float dist) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get interpuppillary distance"), Category = "DisplayCluster|Render")
	virtual float GetInterpupillaryDistance() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set eye swap"), Category = "DisplayCluster|Render")
	virtual void SetEyesSwap(bool swap) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get eye swap"), Category = "DisplayCluster|Render")
	virtual bool GetEyesSwap() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Toggle eye swap"), Category = "DisplayCluster|Render")
	virtual bool ToggleEyesSwap() override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set output flip"), Category = "DisplayCluster|Render")
	virtual void SetOutputFlip(bool flipH, bool flipV) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get output flip"), Category = "DisplayCluster|Render")
	virtual void GetOutputFlip(bool& flipH, bool& flipV) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get near and far clipping distance"), Category = "DisplayCluster|Render")
	virtual void GetCullingDistance(float& NearClipPlane, float& FarClipPlane) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set near and far clipping distance"), Category = "DisplayCluster|Render")
	virtual void SetCullingDistance(float NearClipPlane, float FarClipPlane) override;
};

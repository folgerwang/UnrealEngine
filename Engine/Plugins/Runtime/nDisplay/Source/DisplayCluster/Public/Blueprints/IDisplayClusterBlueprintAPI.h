// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DisplayClusterOperationMode.h"
#include "IDisplayClusterBlueprintAPI.generated.h"


UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class DISPLAYCLUSTER_API UDisplayClusterBlueprintAPI : public UInterface
{
	GENERATED_BODY()
};


/**
 * Blueprint API interface
 */
class DISPLAYCLUSTER_API IDisplayClusterBlueprintAPI
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// DisplayCluster module API
	//////////////////////////////////////////////////////////////////////////////////////////////
	/** Return if the module has been initialized. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is module initialized"), Category = "DisplayCluster")
	virtual bool IsModuleInitialized() = 0;

	/** Return current operation mode. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get operation mode"), Category = "DisplayCluster")
	virtual EDisplayClusterOperationMode GetOperationMode() = 0;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Cluster API
	//////////////////////////////////////////////////////////////////////////////////////////////
	/** Return if current node is a master computer in cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is master node"), Category = "DisplayCluster|Cluster")
	virtual bool IsMaster() = 0;
	
	/** Return if current node is not a master computer in cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is slave node"), Category = "DisplayCluster|Cluster")
	virtual bool IsSlave() = 0;

	/** Whether application is in cluster mode or not. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is cluster mode"), Category = "DisplayCluster|Cluster")
	virtual bool IsCluster() = 0;

	/** Whether application is in standalone mode or not. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is standalone mode"), Category = "DisplayCluster|Cluster")
	virtual bool IsStandalone() = 0;

	/** Returns node name of the current application instance. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get node ID"), Category = "DisplayCluster|Cluster")
	virtual FString GetNodeId() = 0;

	/** Returns amount of nodes in cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get nodes amount"), Category = "DisplayCluster|Cluster")
	virtual int32 GetNodesAmount() = 0;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Config API
	//////////////////////////////////////////////////////////////////////////////////////////////


public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Game API
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Root
	/** Returns Cluster Pawn. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get root"), Category = "DisplayCluster|Game")
	virtual ADisplayClusterPawn* GetRoot() = 0;

	// Screens
	/** Returns screen reference used for computing frustum output. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get active screen"), Category = "DisplayCluster|Game")
	virtual UDisplayClusterScreenComponent* GetActiveScreen() = 0;

	/** Returns screen reference by id name. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get screen by ID"), Category = "DisplayCluster|Game")
	virtual UDisplayClusterScreenComponent* GetScreenById(const FString& id) = 0;

	/** Returns array of all screen references. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get all screens"), Category = "DisplayCluster|Game")
	virtual TArray<UDisplayClusterScreenComponent*> GetAllScreens() = 0;

	/** Returns amount of screens defined in configuration file. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of screens"), Category = "DisplayCluster|Game")
	virtual int32 GetScreensAmount() = 0;

	// Cameras
	/*
	virtual UDisplayClusterCameraComponent*         GetActiveCamera() const = 0;
	virtual UDisplayClusterCameraComponent*         GetCameraById(const FString& id) const = 0;
	virtual TArray<UDisplayClusterCameraComponent*> GetAllCameras() const = 0;
	virtual int32                        GetCamerasAmount() const = 0;
	virtual void                         SetActiveCamera(int32 idx) = 0;
	virtual void                         SetActiveCamera(const FString& id) = 0;
	*/

	// Nodes
	/** Returns node reference by its id name. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get node by ID"), Category = "DisplayCluster|Game")
	virtual UDisplayClusterSceneComponent* GetNodeById(const FString& id) = 0;

	/** Returns array of all nodes references by its id name. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get all nodes"), Category = "DisplayCluster|Game")
	virtual TArray<UDisplayClusterSceneComponent*> GetAllNodes() = 0;

	// Navigation
	/** Returns scene component used for default pawn navigation. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get translation direction component"), Category = "DisplayCluster|Game")
	virtual USceneComponent* GetTranslationDirectionComponent() = 0;

	/** Set scene component to be used for default pawn navigation. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set translation direction component"), Category = "DisplayCluster|Game")
	virtual void SetTranslationDirectionComponent(USceneComponent* pComp) = 0;

	/** Set scene component to be used for default pawn navigation by id name. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set translation direction component by ID"), Category = "DisplayCluster|Game")
	virtual void SetTranslationDirectionComponentId(const FString& id) = 0;

	/** Return scene component used as a pivot point for rotation of the scene node hierarchy. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get rotate around component"), Category = "DisplayCluster|Game")
	virtual USceneComponent* GetRotateAroundComponent() = 0;

	/** Set scene component used as a pivot point for rotation of the scene node hierarchy. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set rotate around component"), Category = "DisplayCluster|Game")
	virtual void SetRotateAroundComponent(USceneComponent* pComp) = 0;

	/** Set scene component used as a pivot point for rotation of the scene node hierarchy by id name. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set rotate around component by ID"), Category = "DisplayCluster|Game")
	virtual void SetRotateAroundComponentId(const FString& id) = 0;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Input API
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Device information
	/** Return amount of VRPN axis devices. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of VRPN axis devices"), Category = "DisplayCluster|Input")
	virtual int32 GetAxisDeviceAmount() = 0;

	/** Return amount of VRPN button devices. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of VRPN button devices"), Category = "DisplayCluster|Input")
	virtual int32 GetButtonDeviceAmount() = 0;

	/** Return amount of VRPN tracker devices. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get amount of VRPN tracker devices"), Category = "DisplayCluster|Input")
	virtual int32 GetTrackerDeviceAmount() = 0;

	/** Return array of names of all VRPN axis devices. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get IDs of VRPN axis devices"), Category = "DisplayCluster|Input")
	virtual bool GetAxisDeviceIds(TArray<FString>& IDs) = 0;

	/** Return array of names of all VRPN button devices. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get IDs of VRPN button devices"), Category = "DisplayCluster|Input")
	virtual bool GetButtonDeviceIds(TArray<FString>& IDs) = 0;

	/** Return array of names of all VRPN tracker devices. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get IDs of VRPN tracker devices"), Category = "DisplayCluster|Input")
	virtual bool GetTrackerDeviceIds(TArray<FString>& IDs) = 0;

	// Buttons
	/** Return state of VRPN button at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN button state"), Category = "DisplayCluster|Input")
	virtual void GetButtonState(const FString& DeviceId, uint8 DeviceChannel, bool& CurState, bool& IsChannelAvailable) = 0;

	/** Return whether VRPN button is pressed at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is VRPN button pressed"), Category = "DisplayCluster|Input")
	virtual void IsButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& CurPressed, bool& IsChannelAvailable) = 0;

	/** Return whether VRPN button is released at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is VRPN button released"), Category = "DisplayCluster|Input")
	virtual void IsButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& CurReleased, bool& IsChannelAvailable) = 0;

	/** Return whether VRPN button was released at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Was VRPN button pressed"), Category = "DisplayCluster|Input")
	virtual void WasButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& WasPressed, bool& IsChannelAvailable) = 0;

	/** Return whether VRPN button was released at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Was VRPN button released"), Category = "DisplayCluster|Input")
	virtual void WasButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& WasReleased, bool& IsChannelAvailable) = 0;

	// Axes
	/** Return axis value at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN axis value"), Category = "DisplayCluster|Input")
	virtual void GetAxis(const FString& DeviceId, uint8 DeviceChannel, float& Value, bool& IsAvailable) = 0;

	// Trackers
	/** Return tracker location values at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN tracker location"), Category = "DisplayCluster|Input")
	virtual void GetTrackerLocation(const FString& DeviceId, uint8 DeviceChannel, FVector& Location, bool& IsChannelAvailable) = 0;

	/** Return tracker quanternion values at specified device and channel. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get VRPN tracker rotation (as quaternion)"), Category = "DisplayCluster|Input")
	virtual void GetTrackerQuat(const FString& DeviceId, uint8 DeviceChannel, FQuat& Rotation, bool& IsChannelAvailable) = 0;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Render API
	//////////////////////////////////////////////////////////////////////////////////////////////
	/** Set eye interpupillary distance (eye separation) for stereoscopic rendering. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set interpuppillary distance"), Category = "DisplayCluster|Render")
	virtual void SetInterpupillaryDistance(float dist) = 0;

	/** Return eye interpupillary distance (eye separation) for stereoscopic rendering. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get interpuppillary distance"), Category = "DisplayCluster|Render")
	virtual float GetInterpupillaryDistance() = 0;

	/** Swap eye rendering. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set eye swap"), Category = "DisplayCluster|Render")
	virtual void SetEyesSwap(bool swap) = 0;

	/** Get Swap eye rendering state. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get eye swap"), Category = "DisplayCluster|Render")
	virtual bool GetEyesSwap() = 0;

	/** Toggle current eye swap state. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Toggle eye swap"), Category = "DisplayCluster|Render")
	virtual bool ToggleEyesSwap() = 0;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set output flip"), Category = "DisplayCluster|Render")
	virtual void SetOutputFlip(bool flipH, bool flipV) = 0;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get output flip"), Category = "DisplayCluster|Render")
	virtual void GetOutputFlip(bool& flipH, bool& flipV) = 0;

	/** Return near and far plane clip plane distances. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get near and far clipping distance"), Category = "DisplayCluster|Render")
	virtual void GetCullingDistance(float& NearClipPlane, float& FarClipPlane) = 0;

	/** Set near and far plane clip plane distances. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set near and far clipping distance"), Category = "DisplayCluster|Render")
	virtual void SetCullingDistance(float NearClipPlane, float FarClipPlane) = 0;
};

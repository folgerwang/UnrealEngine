// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IPDisplayClusterGameManager.h"
#include "DisplayClusterOperationMode.h"

#include "DisplayClusterGameMode.h"
#include "DisplayClusterPawn.h"
#include "DisplayClusterSettings.h"
#include "DisplayClusterScreenComponent.h"
#include "DisplayClusterCameraComponent.h"


/**
 * Game manager. Responsible for building VR object hierarchy from a config file. Implements some in-game logic.
 */
class FDisplayClusterGameManager
	: public    IPDisplayClusterGameManager
{
public:
	FDisplayClusterGameManager();
	virtual ~FDisplayClusterGameManager();

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

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterGameManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual ADisplayClusterPawn*                    GetRoot() const override;

	virtual TArray<UDisplayClusterScreenComponent*> GetAllScreens() const override;
	virtual UDisplayClusterScreenComponent*         GetActiveScreen() const override;
	virtual UDisplayClusterScreenComponent*         GetScreenById(const FString& id) const override;
	virtual int32                        GetScreensAmount() const override;

	virtual TArray<UDisplayClusterCameraComponent*> GetAllCameras() const override;
	virtual UDisplayClusterCameraComponent*         GetActiveCamera() const override;
	virtual UDisplayClusterCameraComponent*         GetCameraById(const FString& id) const override;
	virtual int32                        GetCamerasAmount() const override;
	virtual void                         SetActiveCamera(int32 idx) override;
	virtual void                         SetActiveCamera(const FString& id) override;

	virtual TArray<UDisplayClusterSceneComponent*>  GetAllNodes() const override;
	virtual UDisplayClusterSceneComponent*          GetNodeById(const FString& id) const override;

	virtual USceneComponent*             GetTranslationDirectionComponent() const override;
	virtual void                         SetTranslationDirectionComponent(USceneComponent* pComp) override;
	virtual void                         SetTranslationDirectionComponent(const FString& id) override;

	virtual USceneComponent*             GetRotateAroundComponent() const override;
	virtual void                         SetRotateAroundComponent(USceneComponent* pComp) override;
	virtual void                         SetRotateAroundComponent(const FString& id) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterGameManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsDisplayClusterActive() const override
	{ return ((CurrentOperationMode != EDisplayClusterOperationMode::Disabled) && (CurrentGameMode ? CurrentGameMode->IsDisplayClusterActive() : false)); }
	
	virtual void SetDisplayClusterGameMode(ADisplayClusterGameMode* pGameMode) override
	{ CurrentGameMode = pGameMode; }

	virtual ADisplayClusterGameMode* GetDisplayClusterGameMode() const override
	{ return CurrentGameMode; }

	virtual void SetDisplayClusterSceneSettings(ADisplayClusterSettings* pSceneSettings) override
	{ CurrentSceneSettings = pSceneSettings; }

	virtual ADisplayClusterSettings* GetDisplayClusterSceneSettings() const override
	{ return CurrentSceneSettings; }

private:
	// Creates DisplayCluster actor and fulfills with components hierarchy
	bool InitializeDisplayClusterActor();

	bool CreateScreens();
	bool CreateNodes();
	bool CreateCameras();

	// Extracts array of values from a map
	template <typename ObjType>
	TArray<ObjType*> GetMapValues(const TMap<FString, ObjType*>& container) const;

	// Gets item by id. Performs checks and logging.
	template <typename DataType>
	DataType* GetItem(const TMap<FString, DataType*>& container, const FString& id, const FString& logHeader) const;

private:
	// DisplayCluster root actor
	ADisplayClusterPawn* VRRootActor = nullptr;
	// Currently active projection screen (for this cluster node)
	UDisplayClusterScreenComponent* ActiveScreenComponent = nullptr;
	// Currently active camera (joint component)
	UDisplayClusterCameraComponent* ActiveCameraComponent = nullptr;

	// Available screens (from config file)
	TMap<FString, UDisplayClusterScreenComponent*> ScreenComponents;
	// Available cameras (from config file)
	TMap<FString, UDisplayClusterCameraComponent*> CameraComponents;
	// All available DisplayCluster nodes in hierarchy
	TMap<FString, UDisplayClusterSceneComponent*> SceneNodeComponents;

	EDisplayClusterOperationMode CurrentOperationMode;
	FString ConfigPath;
	FString ClusterNodeId;
	UWorld* CurrentWorld;

	ADisplayClusterSettings* CurrentSceneSettings = nullptr;
	ADisplayClusterGameMode* CurrentGameMode = nullptr;

	mutable FCriticalSection InternalsSyncScope;
};


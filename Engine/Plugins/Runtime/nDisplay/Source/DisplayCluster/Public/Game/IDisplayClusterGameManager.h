// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterCameraComponent.h"
#include "DisplayClusterScreenComponent.h"
#include "DisplayClusterPawn.h"


/**
 * Public game manager interface
 */
struct IDisplayClusterGameManager
{
	virtual ~IDisplayClusterGameManager()
	{ }

	virtual ADisplayClusterPawn*                    GetRoot() const = 0;

	virtual TArray<UDisplayClusterScreenComponent*> GetAllScreens() const = 0;
	virtual UDisplayClusterScreenComponent*         GetActiveScreen() const = 0;
	virtual UDisplayClusterScreenComponent*         GetScreenById(const FString& id) const = 0;
	virtual int32                                   GetScreensAmount() const = 0;

	virtual TArray<UDisplayClusterCameraComponent*> GetAllCameras() const = 0;
	virtual UDisplayClusterCameraComponent*         GetActiveCamera() const = 0;
	virtual UDisplayClusterCameraComponent*         GetCameraById(const FString& id) const = 0;
	virtual int32                                   GetCamerasAmount() const = 0;
	virtual void                                    SetActiveCamera(int32 idx) = 0;
	virtual void                                    SetActiveCamera(const FString& id) = 0;

	virtual TArray<UDisplayClusterSceneComponent*>  GetAllNodes() const = 0;
	virtual UDisplayClusterSceneComponent*          GetNodeById(const FString& id) const = 0;

	virtual USceneComponent*                        GetTranslationDirectionComponent() const = 0;
	virtual void                                    SetTranslationDirectionComponent(USceneComponent* const pComp) = 0;
	virtual void                                    SetTranslationDirectionComponent(const FString& id) = 0;

	virtual USceneComponent*                        GetRotateAroundComponent() const = 0;
	virtual void                                    SetRotateAroundComponent(USceneComponent* const pComp) = 0;
	virtual void                                    SetRotateAroundComponent(const FString& id) = 0;
};

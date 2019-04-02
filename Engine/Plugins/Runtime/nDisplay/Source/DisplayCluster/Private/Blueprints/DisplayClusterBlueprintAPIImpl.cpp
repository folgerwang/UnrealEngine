// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterBlueprintAPIImpl.h"

#include "IDisplayCluster.h"

#include "Cluster/DisplayClusterClusterEvent.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"
#include "Input/IDisplayClusterInputManager.h"
#include "Misc/DisplayClusterLog.h"
#include "Render/IDisplayClusterRenderManager.h"



//////////////////////////////////////////////////////////////////////////////////////////////
// DisplayCluster module API
//////////////////////////////////////////////////////////////////////////////////////////////
/** Return if the module has been initialized. */
bool UDisplayClusterBlueprintAPIImpl::IsModuleInitialized()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);
	return IDisplayCluster::Get().IsModuleInitialized();
}

EDisplayClusterOperationMode UDisplayClusterBlueprintAPIImpl::GetOperationMode()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);
	return IDisplayCluster::Get().GetOperationMode();
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster API
//////////////////////////////////////////////////////////////////////////////////////////////
bool UDisplayClusterBlueprintAPIImpl::IsMaster()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->IsMaster();
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::IsSlave()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	return !IsMaster();
}

bool UDisplayClusterBlueprintAPIImpl::IsCluster()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->IsCluster();
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::IsStandalone()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	return !IsCluster();
}

FString UDisplayClusterBlueprintAPIImpl::GetNodeId()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->GetNodeId();
	}

	return FString();
}

int32 UDisplayClusterBlueprintAPIImpl::GetNodesAmount()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->GetNodesAmount();
	}

	return 0;
}

void UDisplayClusterBlueprintAPIImpl::AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->AddClusterEventListener(Listener);
	}
}

void UDisplayClusterBlueprintAPIImpl::RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->RemoveClusterEventListener(Listener);
	}
}

void UDisplayClusterBlueprintAPIImpl::EmitClusterEvent(const FDisplayClusterClusterEvent& Event, bool MasterOnly)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->EmitClusterEvent(Event, MasterOnly);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Config API
//////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////
// Game API
//////////////////////////////////////////////////////////////////////////////////////////////
// Root
ADisplayClusterPawn* UDisplayClusterBlueprintAPIImpl::GetRoot()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetRoot();
	}

	return nullptr;
}

// Screens
TArray<UDisplayClusterScreenComponent*> UDisplayClusterBlueprintAPIImpl::GetActiveScreens()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	const IDisplayClusterGameManager* const GameManager = IDisplayCluster::Get().GetGameMgr();
	if (GameManager)
	{
		return GameManager->GetActiveScreens();
	}

	return TArray<UDisplayClusterScreenComponent*>();
}

UDisplayClusterScreenComponent* UDisplayClusterBlueprintAPIImpl::GetScreenById(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetScreenById(id);
	}

	return nullptr;
}

TArray<UDisplayClusterScreenComponent*> UDisplayClusterBlueprintAPIImpl::GetAllScreens()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetAllScreens();
	}

	return TArray<UDisplayClusterScreenComponent*>();
}

int32 UDisplayClusterBlueprintAPIImpl::GetScreensAmount()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetScreensAmount();
	}

	return 0;
}

// Cameras


// Nodes
UDisplayClusterSceneComponent* UDisplayClusterBlueprintAPIImpl::GetNodeById(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetNodeById(id);
	}

	return nullptr;
}

TArray<UDisplayClusterSceneComponent*> UDisplayClusterBlueprintAPIImpl::GetAllNodes()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetAllNodes();
	}

	return TArray<UDisplayClusterSceneComponent*>();
}

// Navigation
USceneComponent* UDisplayClusterBlueprintAPIImpl::GetTranslationDirectionComponent()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetTranslationDirectionComponent();
	}

	return nullptr;
}

void UDisplayClusterBlueprintAPIImpl::SetTranslationDirectionComponent(USceneComponent* const pComp)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetTranslationDirectionComponent(pComp);
	}
}

void UDisplayClusterBlueprintAPIImpl::SetTranslationDirectionComponentId(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetTranslationDirectionComponent(id);
	}
}

USceneComponent* UDisplayClusterBlueprintAPIImpl::GetRotateAroundComponent()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetRotateAroundComponent();
	}

	return nullptr;
}

void UDisplayClusterBlueprintAPIImpl::SetRotateAroundComponent(USceneComponent* const pComp)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetRotateAroundComponent(pComp);
	}
}

void UDisplayClusterBlueprintAPIImpl::SetRotateAroundComponentId(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetRotateAroundComponent(id);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Input API
//////////////////////////////////////////////////////////////////////////////////////////////
// Device information
int32 UDisplayClusterBlueprintAPIImpl::GetAxisDeviceAmount()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		Manager->GetAxisDeviceAmount();
	}

	return 0;
}

int32 UDisplayClusterBlueprintAPIImpl::GetButtonDeviceAmount()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		Manager->GetButtonDeviceAmount();
	}

	return 0;
}

int32 UDisplayClusterBlueprintAPIImpl::GetTrackerDeviceAmount()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		Manager->GetTrackerDeviceAmount();
	}

	return 0;
}

bool UDisplayClusterBlueprintAPIImpl::GetAxisDeviceIds(TArray<FString>& IDs)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	TArray<FString> result;
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		return Manager->GetAxisDeviceIds(IDs);
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::GetButtonDeviceIds(TArray<FString>& IDs)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		return Manager->GetButtonDeviceIds(IDs);
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::GetTrackerDeviceIds(TArray<FString>& IDs)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		return Manager->GetTrackerDeviceIds(IDs);
	}

	return false;
}

// Buttons
void UDisplayClusterBlueprintAPIImpl::GetButtonState(const FString& DeviceId, uint8 DeviceChannel, bool& CurState, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetButtonState(DeviceId, DeviceChannel, CurState);
	}
}

void UDisplayClusterBlueprintAPIImpl::IsButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& CurPressed, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->IsButtonPressed(DeviceId, DeviceChannel, CurPressed);
	}
}

void UDisplayClusterBlueprintAPIImpl::IsButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& CurReleased, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->IsButtonReleased(DeviceId, DeviceChannel, CurReleased);
	}
}

void UDisplayClusterBlueprintAPIImpl::WasButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& WasPressed, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->WasButtonPressed(DeviceId, DeviceChannel, WasPressed);
	}
}

void UDisplayClusterBlueprintAPIImpl::WasButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& WasReleased, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->WasButtonReleased(DeviceId, DeviceChannel, WasReleased);
	}
}

// Axes
void UDisplayClusterBlueprintAPIImpl::GetAxis(const FString& DeviceId, uint8 DeviceChannel, float& Value, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetAxis(DeviceId, DeviceChannel, Value);
	}
}

// Trackers
void UDisplayClusterBlueprintAPIImpl::GetTrackerLocation(const FString& DeviceId, uint8 DeviceChannel, FVector& Location, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetTrackerLocation(DeviceId, DeviceChannel, Location);
	}
}

void UDisplayClusterBlueprintAPIImpl::GetTrackerQuat(const FString& DeviceId, uint8 DeviceChannel, FQuat& Rotation, bool& IsChannelAvailable)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetTrackerQuat(DeviceId, DeviceChannel, Rotation);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Render API
//////////////////////////////////////////////////////////////////////////////////////////////
void  UDisplayClusterBlueprintAPIImpl::SetInterpupillaryDistance(float dist)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->SetInterpupillaryDistance(dist);
	}

	return;
}

float UDisplayClusterBlueprintAPIImpl::GetInterpupillaryDistance()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetInterpupillaryDistance();
	}

	return 0.f;
}

void UDisplayClusterBlueprintAPIImpl::SetEyesSwap(bool swap)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->SetEyesSwap(swap);
	}

	return;
}

bool UDisplayClusterBlueprintAPIImpl::GetEyesSwap()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetEyesSwap();
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::ToggleEyesSwap()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->ToggleEyesSwap();
	}

	return false;
}

void UDisplayClusterBlueprintAPIImpl::GetCullingDistance(float& NearClipPlane, float& FarClipPlane)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetCullingDistance(NearClipPlane, FarClipPlane);
	}

	return;
}

void UDisplayClusterBlueprintAPIImpl::SetCullingDistance(float NearClipPlane, float FarClipPlane)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterBlueprint);

	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->SetCullingDistance(NearClipPlane, FarClipPlane);
	}

	return;
}

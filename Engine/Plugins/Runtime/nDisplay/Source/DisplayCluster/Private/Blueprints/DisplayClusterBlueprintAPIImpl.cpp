// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterBlueprintAPIImpl.h"

#include "IDisplayCluster.h"

#include "Cluster/IDisplayClusterClusterManager.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"
#include "Input/IDisplayClusterInputManager.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Render/IDisplayClusterStereoDevice.h"



//////////////////////////////////////////////////////////////////////////////////////////////
// DisplayCluster module API
//////////////////////////////////////////////////////////////////////////////////////////////
/** Return if the module has been initialized. */
bool UDisplayClusterBlueprintAPIImpl::IsModuleInitialized()
{
	return IDisplayCluster::Get().IsModuleInitialized();
}

EDisplayClusterOperationMode UDisplayClusterBlueprintAPIImpl::GetOperationMode()
{
	return IDisplayCluster::Get().GetOperationMode();
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Cluster API
//////////////////////////////////////////////////////////////////////////////////////////////
bool UDisplayClusterBlueprintAPIImpl::IsMaster()
{
	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->IsMaster();
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::IsSlave()
{
	return !IsMaster();
}

bool UDisplayClusterBlueprintAPIImpl::IsCluster()
{
	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->IsCluster();
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::IsStandalone()
{
	return !IsCluster();
}

FString UDisplayClusterBlueprintAPIImpl::GetNodeId()
{
	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->GetNodeId();
	}

	return FString();
}

int32 UDisplayClusterBlueprintAPIImpl::GetNodesAmount()
{
	IDisplayClusterClusterManager* const Manager = IDisplayCluster::Get().GetClusterMgr();
	if (Manager)
	{
		return Manager->GetNodesAmount();
	}

	return 0;
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
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetRoot();
	}

	return nullptr;
}

// Screens
UDisplayClusterScreenComponent* UDisplayClusterBlueprintAPIImpl::GetActiveScreen()
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetActiveScreen();
	}

	return nullptr;
}

UDisplayClusterScreenComponent* UDisplayClusterBlueprintAPIImpl::GetScreenById(const FString& id)
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetScreenById(id);
	}

	return nullptr;
}

TArray<UDisplayClusterScreenComponent*> UDisplayClusterBlueprintAPIImpl::GetAllScreens()
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetAllScreens();
	}

	return TArray<UDisplayClusterScreenComponent*>();
}

int32 UDisplayClusterBlueprintAPIImpl::GetScreensAmount()
{
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
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetNodeById(id);
	}

	return nullptr;
}

TArray<UDisplayClusterSceneComponent*> UDisplayClusterBlueprintAPIImpl::GetAllNodes()
{
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
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetTranslationDirectionComponent();
	}

	return nullptr;
}

void UDisplayClusterBlueprintAPIImpl::SetTranslationDirectionComponent(USceneComponent* const pComp)
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetTranslationDirectionComponent(pComp);
	}
}

void UDisplayClusterBlueprintAPIImpl::SetTranslationDirectionComponentId(const FString& id)
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetTranslationDirectionComponent(id);
	}
}

USceneComponent* UDisplayClusterBlueprintAPIImpl::GetRotateAroundComponent()
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		return Manager->GetRotateAroundComponent();
	}

	return nullptr;
}

void UDisplayClusterBlueprintAPIImpl::SetRotateAroundComponent(USceneComponent* const pComp)
{
	IDisplayClusterGameManager* const Manager = IDisplayCluster::Get().GetGameMgr();
	if (Manager)
	{
		Manager->SetRotateAroundComponent(pComp);
	}
}

void UDisplayClusterBlueprintAPIImpl::SetRotateAroundComponentId(const FString& id)
{
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
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		Manager->GetAxisDeviceAmount();
	}

	return 0;
}

int32 UDisplayClusterBlueprintAPIImpl::GetButtonDeviceAmount()
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		Manager->GetButtonDeviceAmount();
	}

	return 0;
}

int32 UDisplayClusterBlueprintAPIImpl::GetTrackerDeviceAmount()
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		Manager->GetTrackerDeviceAmount();
	}

	return 0;
}

bool UDisplayClusterBlueprintAPIImpl::GetAxisDeviceIds(TArray<FString>& IDs)
{
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
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		return Manager->GetButtonDeviceIds(IDs);
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::GetTrackerDeviceIds(TArray<FString>& IDs)
{
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
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetButtonState(DeviceId, DeviceChannel, CurState);
	}
}

void UDisplayClusterBlueprintAPIImpl::IsButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& CurPressed, bool& IsChannelAvailable)
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->IsButtonPressed(DeviceId, DeviceChannel, CurPressed);
	}
}

void UDisplayClusterBlueprintAPIImpl::IsButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& CurReleased, bool& IsChannelAvailable)
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->IsButtonReleased(DeviceId, DeviceChannel, CurReleased);
	}
}

void UDisplayClusterBlueprintAPIImpl::WasButtonPressed(const FString& DeviceId, uint8 DeviceChannel, bool& WasPressed, bool& IsChannelAvailable)
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->WasButtonPressed(DeviceId, DeviceChannel, WasPressed);
	}
}

void UDisplayClusterBlueprintAPIImpl::WasButtonReleased(const FString& DeviceId, uint8 DeviceChannel, bool& WasReleased, bool& IsChannelAvailable)
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->WasButtonReleased(DeviceId, DeviceChannel, WasReleased);
	}
}

// Axes
void UDisplayClusterBlueprintAPIImpl::GetAxis(const FString& DeviceId, uint8 DeviceChannel, float& Value, bool& IsChannelAvailable)
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetAxis(DeviceId, DeviceChannel, Value);
	}
}

// Trackers
void UDisplayClusterBlueprintAPIImpl::GetTrackerLocation(const FString& DeviceId, uint8 DeviceChannel, FVector& Location, bool& IsChannelAvailable)
{
	IDisplayClusterInputManager* const Manager = IDisplayCluster::Get().GetInputMgr();
	if (Manager)
	{
		IsChannelAvailable = Manager->GetTrackerLocation(DeviceId, DeviceChannel, Location);
	}
}

void UDisplayClusterBlueprintAPIImpl::GetTrackerQuat(const FString& DeviceId, uint8 DeviceChannel, FQuat& Rotation, bool& IsChannelAvailable)
{
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
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->SetInterpupillaryDistance(dist);
	}

	return;
}

float UDisplayClusterBlueprintAPIImpl::GetInterpupillaryDistance()
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->GetInterpupillaryDistance();
	}

	return 0.f;
}

void UDisplayClusterBlueprintAPIImpl::SetEyesSwap(bool swap)
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->SetEyesSwap(swap);
	}

	return;
}

bool UDisplayClusterBlueprintAPIImpl::GetEyesSwap()
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->GetEyesSwap();
	}

	return false;
}

bool UDisplayClusterBlueprintAPIImpl::ToggleEyesSwap()
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->ToggleEyesSwap();
	}

	return false;
}

void UDisplayClusterBlueprintAPIImpl::SetOutputFlip(bool flipH, bool flipV)
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->SetOutputFlip(flipH, flipV);
	}

	return;
}

void UDisplayClusterBlueprintAPIImpl::GetOutputFlip(bool& flipH, bool& flipV)
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		return Manager->GetStereoDevice()->GetOutputFlip(flipH, flipV);
	}

	return;
}

void UDisplayClusterBlueprintAPIImpl::GetCullingDistance(float& NearClipPlane, float& FarClipPlane)
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		IDisplayClusterStereoDevice* pDev = Manager->GetStereoDevice();
		if (pDev)
		{
			return pDev->GetCullingDistance(NearClipPlane, FarClipPlane);
		}
	}

	return;
}

void UDisplayClusterBlueprintAPIImpl::SetCullingDistance(float NearClipPlane, float FarClipPlane)
{
	IDisplayClusterRenderManager* const Manager = IDisplayCluster::Get().GetRenderMgr();
	if (Manager)
	{
		IDisplayClusterStereoDevice* pDev = Manager->GetStereoDevice();
		if (pDev)
		{
			return pDev->SetCullingDistance(NearClipPlane, FarClipPlane);
		}
	}

	return;
}

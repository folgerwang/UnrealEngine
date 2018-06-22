// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterNodeCtrlBase.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterNodeCtrlBase::FDisplayClusterNodeCtrlBase(const FString& ctrlName, const FString& nodeName) :
	NodeName(nodeName),
	ControllerName(ctrlName)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterNodeController
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterNodeCtrlBase::Initialize()
{
	if (!InitializeStereo())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Stereo initialization failed"));
		return false;
	}

	if (!InitializeServers())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Servers initialization failed"));
		return false;
	}

	if (!InitializeClients())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Clients initialization failed"));
		return false;
	}

	if (!StartServers())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("An error occurred during servers start"));
		return false;
	}

	if (!StartClients())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("An error occurred during clients start"));
		return false;
	}

	return true;
}

void FDisplayClusterNodeCtrlBase::Release()
{
	StopServers();
	StopClients();
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterService.h"
#include "Network/DisplayClusterSession.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Config/DisplayClusterConfigTypes.h"

#include "Misc/DisplayClusterAppExit.h"
#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


FDisplayClusterService::FDisplayClusterService(const FString& name, const FString& addr, const int32 port) :
	FDisplayClusterServer(name, addr, port)
{
}

bool FDisplayClusterService::IsClusterIP(const FIPv4Endpoint& ep)
{
	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	TArray<FDisplayClusterConfigClusterNode> nodes = GDisplayCluster->GetPrivateConfigMgr()->GetClusterNodes();
	const FString addr = ep.Address.ToString();
	
	return nullptr != nodes.FindByPredicate([addr](const FDisplayClusterConfigClusterNode& node)
	{
		return addr == node.Addr;
	});
}

bool FDisplayClusterService::IsConnectionAllowed(FSocket* pSock, const FIPv4Endpoint& ep)
{
	// By default any DisplayCluster service must be within a cluster
	return FDisplayClusterService::IsClusterIP(ep);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterService::NotifySessionOpen(FDisplayClusterSession* pSession)
{
	FDisplayClusterServer::NotifySessionOpen(pSession);
}

void FDisplayClusterService::NotifySessionClose(FDisplayClusterSession* pSession)
{
	FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, GetName() + FString(" - Connection interrupted. Application exit requested."));
	FDisplayClusterServer::NotifySessionClose(pSession);
}


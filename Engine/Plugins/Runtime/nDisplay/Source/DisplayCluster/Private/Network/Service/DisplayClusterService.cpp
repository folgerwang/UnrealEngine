// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Service/DisplayClusterService.h"
#include "Network/Session/DisplayClusterSessionBase.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Config/DisplayClusterConfigTypes.h"

#include "Misc/DisplayClusterAppExit.h"
#include "DisplayClusterGlobals.h"


FDisplayClusterService::FDisplayClusterService(const FString& InName, const FString& InAddr, const int32 InPort) :
	FDisplayClusterServer(InName, InAddr, InPort)
{
}

bool FDisplayClusterService::IsClusterIP(const FIPv4Endpoint& InEP)
{
	IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
	if (ConfigMgr == nullptr)
	{
		return false;
	}

	TArray<FDisplayClusterConfigClusterNode> nodes = ConfigMgr->GetClusterNodes();
	const FString addr = InEP.Address.ToString();
	
	return nullptr != nodes.FindByPredicate([addr](const FDisplayClusterConfigClusterNode& node)
	{
		return addr == node.Addr;
	});
}

bool FDisplayClusterService::IsConnectionAllowed(FSocket* InSocket, const FIPv4Endpoint& InEP)
{
	// By default any DisplayCluster service must be within a cluster
	return FDisplayClusterService::IsClusterIP(InEP);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterService::NotifySessionOpen(FDisplayClusterSessionBase* InSession)
{
	FDisplayClusterServer::NotifySessionOpen(InSession);
}

void FDisplayClusterService::NotifySessionClose(FDisplayClusterSessionBase* InSession)
{
	FDisplayClusterServer::NotifySessionClose(InSession);
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSlave.h"

#include "Network/DisplayClusterMessage.h"

class FDisplayClusterClusterSyncService;
class FDisplayClusterSwapSyncService;
class FDisplayClusterClusterEventsService;


/**
 * Master node controller implementation (cluster mode). Manages servers on master side.
 */
class FDisplayClusterClusterNodeCtrlMaster
	: public FDisplayClusterClusterNodeCtrlSlave
{
public:
	FDisplayClusterClusterNodeCtrlMaster(const FString& ctrlName, const FString& nodeName);
	virtual ~FDisplayClusterClusterNodeCtrlMaster();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void GetTimecode(FTimecode& timecode, FFrameRate& frameRate) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterEventsProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void EmitClusterEvent(const FDisplayClusterClusterEvent& Event) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsSlave() const override final
	{ return false; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void GetSyncData(FDisplayClusterMessage::DataType& data)   override;
	virtual void GetInputData(FDisplayClusterMessage::DataType& data)  override;
	virtual void GetEventsData(FDisplayClusterMessage::DataType& data) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FDisplayClusterNodeCtrlBase
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool InitializeServers() override;
	virtual bool StartServers()      override;
	virtual void StopServers()       override;

	virtual bool InitializeClients() override;
	virtual bool StartClients()      override;
	virtual void StopClients()       override;

private:
	// Node servers
	TUniquePtr<FDisplayClusterClusterSyncService>   ClusterSyncServer;
	TUniquePtr<FDisplayClusterSwapSyncService>      SwapSyncServer;
	TUniquePtr<FDisplayClusterClusterEventsService> ClusterEventsServer;
};


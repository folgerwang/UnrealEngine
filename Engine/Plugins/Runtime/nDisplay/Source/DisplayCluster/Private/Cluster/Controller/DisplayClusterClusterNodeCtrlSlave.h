// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlBase.h"
#include "Network/DisplayClusterMessage.h"

class FDisplayClusterClusterSyncClient;
class FDisplayClusterSwapSyncClient;
class FDisplayClusterClusterEventsClient;


/**
 * Slave node controller implementation (cluster mode). . Manages clients on client side.
 */
class FDisplayClusterClusterNodeCtrlSlave
	: public FDisplayClusterClusterNodeCtrlBase
{
public:
	FDisplayClusterClusterNodeCtrlSlave(const FString& ctrlName, const FString& nodeName);
	virtual ~FDisplayClusterClusterNodeCtrlSlave();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsSlave() const override
	{ return true; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForGameStart()  override final;
	virtual void WaitForFrameStart() override final;
	virtual void WaitForFrameEnd()   override final;
	virtual void WaitForTickEnd()    override final;
	virtual void GetDeltaTime(float& deltaTime) override final;
	virtual void GetTimecode(FTimecode& timecode, FFrameRate& frameRate) override;
	virtual void GetSyncData(FDisplayClusterMessage::DataType& data)  override;
	virtual void GetInputData(FDisplayClusterMessage::DataType& data) override;
	virtual void GetEventsData(FDisplayClusterMessage::DataType& data) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterSwapSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime) override final;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterEventsProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void EmitClusterEvent(const FDisplayClusterClusterEvent& Event) override;

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
	// Cluster node clients
	TUniquePtr<FDisplayClusterClusterSyncClient>   ClusterSyncClient;
	TUniquePtr<FDisplayClusterSwapSyncClient>      SwapSyncClient;
	TUniquePtr<FDisplayClusterClusterEventsClient> ClusterEventsClient;
};


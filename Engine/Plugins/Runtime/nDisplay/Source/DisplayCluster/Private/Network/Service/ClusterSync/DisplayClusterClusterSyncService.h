// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DisplayClusterBarrier.h"
#include "Network/DisplayClusterMessage.h"
#include "Network/Service/DisplayClusterService.h"
#include "Network/Protocol/IPDisplayClusterClusterSyncProtocol.h"



/**
 * Cluster synchronization server
 */
class FDisplayClusterClusterSyncService
	: public  FDisplayClusterService
	, private IPDisplayClusterClusterSyncProtocol
{
public:
	FDisplayClusterClusterSyncService(const FString& addr, const int32 port);
	virtual ~FDisplayClusterClusterSyncService();

public:
	virtual bool Start() override;
	void Shutdown() override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void NotifySessionOpen(FDisplayClusterSession* pSession) override;
	virtual void NotifySessionClose(FDisplayClusterSession* pSession) override;
	virtual FDisplayClusterMessage::Ptr ProcessMessage(FDisplayClusterMessage::Ptr msg) override;

private:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForGameStart() override;
	virtual void WaitForFrameStart() override;
	virtual void WaitForFrameEnd() override;
	virtual void WaitForTickEnd() override;
	virtual void GetDeltaTime(float& deltaTime) override;
	virtual void GetSyncData(FDisplayClusterMessage::DataType& data)  override;
	virtual void GetInputData(FDisplayClusterMessage::DataType& data) override;

private:
	// Game start sync barrier
	FDisplayClusterBarrier BarrierGameStart;
	// Frame start barrier
	FDisplayClusterBarrier BarrierFrameStart;
	// Frame end barrier
	FDisplayClusterBarrier BarrierFrameEnd;
	// Tick end barrier
	FDisplayClusterBarrier BarrierTickEnd;
};


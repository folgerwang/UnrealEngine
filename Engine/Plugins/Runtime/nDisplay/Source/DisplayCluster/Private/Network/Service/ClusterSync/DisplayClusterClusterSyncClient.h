// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterClient.h"
#include "Network/DisplayClusterMessage.h"
#include "Network/Protocol/IPDisplayClusterClusterSyncProtocol.h"


/**
 * Cluster synchronization client
 */
class FDisplayClusterClusterSyncClient
	: public FDisplayClusterClient
	, public IPDisplayClusterClusterSyncProtocol
{
public:
	FDisplayClusterClusterSyncClient();
	FDisplayClusterClusterSyncClient(const FString& name);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForGameStart() override;
	virtual void WaitForFrameStart() override;
	virtual void WaitForFrameEnd() override;
	virtual void WaitForTickEnd() override;
	virtual void GetDeltaTime(float& deltaTime) override;
	virtual void GetTimecode(FTimecode& timecode, FFrameRate& frameRate) override;
	virtual void GetSyncData(FDisplayClusterMessage::DataType& data) override;
	virtual void GetInputData(FDisplayClusterMessage::DataType& data) override;
};


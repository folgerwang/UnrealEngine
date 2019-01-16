// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	FDisplayClusterClusterSyncClient(const FString& InName);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForGameStart() override;
	virtual void WaitForFrameStart() override;
	virtual void WaitForFrameEnd() override;
	virtual void WaitForTickEnd() override;
	virtual void GetDeltaTime(float& DeltaTime) override;
	virtual void GetTimecode(FTimecode& Timecode, FFrameRate& FrameRate) override;
	virtual void GetSyncData(FDisplayClusterMessage::DataType& Data) override;
	virtual void GetInputData(FDisplayClusterMessage::DataType& Data) override;
	virtual void GetEventsData(FDisplayClusterMessage::DataType& Data) override;
};


// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "IPDisplayClusterManager.h"

#include "Network/DisplayClusterMessage.h"

struct IPDisplayClusterNodeController;
struct IDisplayClusterClusterSyncObject;


/**
 * Cluster manager private interface
 */
struct IPDisplayClusterClusterManager :
	public IDisplayClusterClusterManager,
	public IPDisplayClusterManager
{
	virtual ~IPDisplayClusterClusterManager()
	{ }

	virtual IPDisplayClusterNodeController* GetController() const = 0;
	
	virtual float GetDeltaTime() const = 0;
	virtual void  SetDeltaTime(float deltaTime) = 0;

	virtual void GetTimecode(FTimecode& timecode, FFrameRate& frameRate) const = 0;
	virtual void  SetTimecode(const FTimecode& timecode, const FFrameRate& frameRate) = 0;
	
	virtual void RegisterSyncObject  (IDisplayClusterClusterSyncObject* pSyncObj) = 0;
	virtual void UnregisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj) = 0;

	virtual void ExportSyncData(FDisplayClusterMessage::DataType& data) const = 0;
	virtual void ImportSyncData(const FDisplayClusterMessage::DataType& data) = 0;

	virtual void SyncObjects() = 0;
	virtual void SyncInput()   = 0;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITimeManagementModule.h"

class FTimeManagementModule : public ITimeManagementModule
{
public:
	FTimeManagementModule();

public:
	//~ Begin ITimeManagementModule Interface
	virtual ITimeSynchronizationManager* GetTimeSynchronizationManager() override { return TimeSyncManager; }
	virtual ITimeSynchronizationManager* GetTimeSynchronizationManager() const override { return TimeSyncManager; }

	virtual void SetTimeSynchronizationManager(ITimeSynchronizationManager* Manager) override { TimeSyncManager = Manager; }
	//~ End ITimeManagementModule Interface

private:
	ITimeSynchronizationManager* TimeSyncManager;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITimeManagementModule.h"

class ITimecodeProvider;

class FTimeManagementModule : public ITimeManagementModule
{
public:
	FTimeManagementModule();

public:
	//~ Begin ITimeManagementModule Interface
	virtual ITimecodeProvider* GetTimecodeProvider() override { return TimeSyncManager; }
	virtual void SetTimecodeProvider(ITimecodeProvider* Provider) override { TimeSyncManager = Provider; }
	//~ End ITimeManagementModule Interface

private:
	ITimecodeProvider * TimeSyncManager;
};

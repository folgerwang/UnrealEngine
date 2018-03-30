// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TimeManagementModule.h"


FTimeManagementModule::FTimeManagementModule()
	: TimeSyncManager(nullptr)
{
}

IMPLEMENT_MODULE(FTimeManagementModule, TimeManagement);
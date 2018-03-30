// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ITimeSynchronizationManager.h"

#include "ITimeManagementModule.h"


FTimecode UTimeSynchronizationManagerHelpers::GetCurrentTimecode()
{
	if (const ITimeSynchronizationManager* Manager = ITimeManagementModule::Get().GetTimeSynchronizationManager())
	{
		return Manager->GetCurrentTimecode();
	}
	return FTimecode();
}

bool UTimeSynchronizationManagerHelpers::IsSynchronizing()
{
	bool bResult = false;
	if (const ITimeSynchronizationManager* Manager = ITimeManagementModule::Get().GetTimeSynchronizationManager())
	{
		bResult = Manager->IsSynchronizing();
	}
	return bResult;
}

bool UTimeSynchronizationManagerHelpers::IsSynchronized()
{
	bool bResult = false;
	if (const ITimeSynchronizationManager* Manager = ITimeManagementModule::Get().GetTimeSynchronizationManager())
	{
		bResult = Manager->IsSynchronized();
	}
	return bResult;
}

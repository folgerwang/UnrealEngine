// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "ARTypes.h"
#include "HAL/ThreadSafeBool.h"

//@joeg -- ARKit 2.0 support

bool FARAsyncTask::HadError() const
{
	return bHadError;
}

FString FARAsyncTask::GetErrorString() const
{
	if (bIsDone)
	{
		return Error;
	}
	return FString();
}

bool FARAsyncTask::IsDone() const
{
	return bIsDone;
}

TArray<uint8> FARSaveWorldAsyncTask::GetSavedWorldData()
{
	if (bIsDone)
	{
		return MoveTemp(WorldData);
	}
	return TArray<uint8>();
}

//@joeg -- End additions

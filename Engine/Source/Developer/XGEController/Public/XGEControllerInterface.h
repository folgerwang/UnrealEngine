// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Async/Async.h"

#if WITH_XGE_CONTROLLER

struct FXGETaskResult
{
	int32 ReturnCode;
	bool bCompleted;
};

class IXGEController : public IModuleInterface
{
public:
	virtual bool SupportsDynamicReloading() override { return false; }

	// Returns true if the XGE controller may be used.
	virtual bool IsSupported() = 0;

	// Returns a new file path to be used for writing input data to.
	virtual FString CreateUniqueFilePath() = 0;

	// Launches a task within XGE. Returns a future which can be waited on for the results.
	virtual TFuture<FXGETaskResult> EnqueueTask(const FString& Command, const FString& CommandArgs) = 0;

	static XGECONTROLLER_API IXGEController& Get();
};

#endif // WITH_XGE_CONTROLLER

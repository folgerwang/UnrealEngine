// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "INUTUnrealEngine4.h"

#include "UnrealEngine4Environment.h"

/**
 * Module implementation
 */
class FNUTUnrealEngine4 : public INUTUnrealEngine4
{
public:
	virtual void StartupModule() override
	{
		FNUTModuleInterface::StartupModule();

		FShooterGameEnvironment::Register();
		FQAGameEnvironment::Register();
		FUTEnvironment::Register();
	}

	virtual void ShutdownModule() override
	{
		FNUTModuleInterface::ShutdownModule();
	}
};


IMPLEMENT_MODULE(FNUTUnrealEngine4, NUTUnrealEngine4);


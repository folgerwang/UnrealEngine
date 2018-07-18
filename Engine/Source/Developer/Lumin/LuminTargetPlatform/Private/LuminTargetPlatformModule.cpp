// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "LuminTargetPlatform.h"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* LuminTargetSingleton = NULL;

/**
 * Module for the Android target platform.
 */
class FLuminTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FLuminTargetPlatformModule( )
	{
		LuminTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (LuminTargetSingleton == NULL)
		{
			bool bClient = true;
			LuminTargetSingleton = new FLuminTargetPlatform(bClient);
		}
		
		return LuminTargetSingleton;
	}

	// End ITargetPlatformModule interface

public:
	// Begin IModuleInterface interface
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
	// End IModuleInterface interface
};


IMPLEMENT_MODULE( FLuminTargetPlatformModule, LuminTargetPlatform);

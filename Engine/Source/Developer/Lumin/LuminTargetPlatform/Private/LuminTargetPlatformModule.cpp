// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "LuminTargetPlatform.h"

/**
 * Module for the Android target platform.
 */
class FLuminTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	virtual ~FLuminTargetPlatformModule( )
	{
		for (ITargetPlatform* TP : TargetPlatforms)
		{
			delete TP;
		}
		TargetPlatforms.Empty();
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual TArray<ITargetPlatform*> GetTargetPlatforms() override
	{
		if (TargetPlatforms.Num() == 0)
		{
			TargetPlatforms.Add(new FLuminTargetPlatform(false));
			TargetPlatforms.Add(new FLuminTargetPlatform(true));
		}
		
		return TargetPlatforms;
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

private:
	/** Holds the target platforms. */
	TArray<ITargetPlatform*> TargetPlatforms;
};


IMPLEMENT_MODULE( FLuminTargetPlatformModule, LuminTargetPlatform);

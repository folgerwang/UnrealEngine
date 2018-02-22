// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/**
 * Module for TVOS as a target platform
 */
class FTVOSTargetPlatformModule	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FTVOSTargetPlatformModule()
	{
		for (ITargetPlatform* TP : TargetPlatforms)
		{
			delete TP;
		}
		TargetPlatforms.Empty();
	}

	//~ ITargetPlatformModule interface

	virtual TArray<ITargetPlatform*> GetTargetPlatforms() override
	{
		if (TargetPlatforms.Num() == 0)
		{
			// add Game and Client TPs
			TargetPlatforms.Add(new FIOSTargetPlatform(true, true));
			TargetPlatforms.Add(new FIOSTargetPlatform(true, false));
		}

		return TargetPlatforms;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

protected:
	/** Holds the target platforms. */
	TArray<ITargetPlatform*> TargetPlatforms;
};


IMPLEMENT_MODULE(FTVOSTargetPlatformModule, TVOSTargetPlatform);

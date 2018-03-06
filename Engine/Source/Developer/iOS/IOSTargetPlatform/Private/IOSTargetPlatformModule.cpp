// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/**
 * Module for iOS as a target platform
 */
class FIOSTargetPlatformModule : public ITargetPlatformModule
{
public:

	/** Destructor. */
	~FIOSTargetPlatformModule()
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
			TargetPlatforms.Add(new FIOSTargetPlatform(false, true));
			TargetPlatforms.Add(new FIOSTargetPlatform(false, false));
		}

		return TargetPlatforms;
	}

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

protected:
	/** Holds the target platforms. */
	TArray<ITargetPlatform*> TargetPlatforms;
	
};


IMPLEMENT_MODULE(FIOSTargetPlatformModule, IOSTargetPlatform);

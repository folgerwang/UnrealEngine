// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "CoreMinimal.h"
#include "AppFramework.h"

/**
 * This is the base module interface which all magic leap modules should inherit from.
 * The purpose of this class is to allow the system to manage the order in which magic
 * leap plugins are loaded and unloaded.
 */
class IMagicLeapModule
{
public:
	/** Registers the module with the AppFramework instance.*/
	IMagicLeapModule(FName InName) : Name(InName)
	{
		FAppFramework::RegisterMagicLeapModule(this);
	}

	/** Unregisters the module with the Appframework instance.*/
	virtual ~IMagicLeapModule()
	{
		FAppFramework::UnregisterMagicLeapModule(this);
	}

	/** Returns the name of the module.*/
	FName GetName() const
	{
		return Name;
	}

	/** Override to perform order-dependant initialization of your plugin.*/
	virtual void Enable() {}

	/** Override to perform order-dependant cleanup of your plugin.*/
	virtual void Disable() {}

protected:
	const FName Name;
};

#define ENABLE_MAGIC_LEAP_MODULE(Name) \
{ \
	IMagicLeapModule* MagicLeapModule = FAppFramework::GetMagicLeapModule(Name); \
	if (nullptr != MagicLeapModule) \
	{ \
		MagicLeapModule->Enable(); \
	} \
}

#define DISABLE_MAGIC_LEAP_MODULE(Name) \
{ \
	IMagicLeapModule* MagicLeapModule = FAppFramework::GetMagicLeapModule(Name); \
	if (nullptr != MagicLeapModule) \
	{ \
		MagicLeapModule->Disable(); \
	} \
}
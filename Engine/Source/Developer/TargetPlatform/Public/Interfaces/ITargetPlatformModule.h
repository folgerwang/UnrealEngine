// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class ITargetPlatform;

/**
 * Interface for target platform modules.
 */
class ITargetPlatformModule
	: public IModuleInterface
{
public:

protected:
	virtual ITargetPlatform* GetTargetPlatform()
	{
		return nullptr;
	};
	
public:

	/**
	 * Gets the module's target platforms. This should be overridden by each platform, but 
	 * currently, we are re-using the single internal GetTargetPlatform method the old TPModules will implement
	 *
	 * @return The target platform.
	 */
	virtual TArray<ITargetPlatform*> GetTargetPlatforms()
	{
		TArray<ITargetPlatform*> TargetPlatforms;
		ITargetPlatform* TargetPlatform = GetTargetPlatform();
		if (TargetPlatform != nullptr)
		{
			TargetPlatforms.Add(TargetPlatform);
		}
		return TargetPlatforms;
	}

public:

	/** Virtual destructor. */
	virtual ~ITargetPlatformModule() { }
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Containers/Array.h"
#include "Modules/ModuleManager.h"
/**
 * Interface for messaging modules.
 */
class ILiveLinkModule
	: public IModuleInterface
{
public:

	/**
	 * Gets a reference to the live link module instance.
	 *
	 * @return A reference to the live link module.
	 * @todo gmp: better implementation using dependency injection.
	 */
	static ILiveLinkModule& Get()
	{
#if PLATFORM_IOS
        static ILiveLinkModule& LiveLinkModule = FModuleManager::LoadModuleChecked<ILiveLinkModule>("LiveLink");
        return LiveLinkModule;
#else
        return FModuleManager::LoadModuleChecked<ILiveLinkModule>("LiveLink");
#endif
	}

public:

	/** Virtual destructor. */
	virtual ~ILiveLinkModule() { }
};


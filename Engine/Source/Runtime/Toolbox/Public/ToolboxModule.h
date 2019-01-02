// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FWorkspaceItem;

/**
 * The toolbox module is a lightweight UI for quickly summoning
 * development tools: profilers, widget inspector, etc.
 */
class IToolboxModule : public IModuleInterface
{
public:

	/** Open the toolbox tab,  */
	virtual void SummonToolbox() = 0;
};

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"


DECLARE_LOG_CATEGORY_EXTERN(LogOpenColorIOEditor, Log, All);

/**
 * Interface for the OpenColorIOEditor module.
 */
class IOpenColorIOEditorModule : public IModuleInterface
{
public:
	
	/** @return true if the OpenColorIOEditor module and OpenColorIO dll could be loaded */
	virtual bool IsInitialized() const = 0;
};


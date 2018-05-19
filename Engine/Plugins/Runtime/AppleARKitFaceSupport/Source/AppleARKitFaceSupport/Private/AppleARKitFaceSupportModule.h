// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class APPLEARKITFACESUPPORT_API FAppleARKitFaceSupportModule :
	public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogAppleARKitFace, Log, All);


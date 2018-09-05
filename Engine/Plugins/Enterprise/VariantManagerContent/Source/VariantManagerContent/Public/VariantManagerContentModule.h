// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"		// For inline LoadModuleChecked()

#define VARIANTMANAGERCONTENTMODULE_MODULE_NAME TEXT("VariantManagerContentModule")


class IVariantManagerContentModule : public IModuleInterface
{
public:
	static inline IVariantManagerContentModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IVariantManagerContentModule>(VARIANTMANAGERCONTENTMODULE_MODULE_NAME);
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(VARIANTMANAGERCONTENTMODULE_MODULE_NAME);
	}
};


// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "INetcodeUnitTest.h"


class INUTUnrealEngine4 : public FNUTModuleInterface
{
public:
	INUTUnrealEngine4()
		: FNUTModuleInterface(TEXT("NUTUnrealEngine4"))
	{
	}

	static inline INUTUnrealEngine4& Get()
	{
		return FModuleManager::LoadModuleChecked<INUTUnrealEngine4>("NUTUnrealEngine4");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NUTUnrealEngine4");
	}
};


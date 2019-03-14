// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"

#define LIVE_CODING_MODULE_NAME "LiveCoding"

class ILiveCodingModule : public IModuleInterface
{
public:
	virtual void EnableByDefault(bool bEnabled) = 0;
	virtual bool IsEnabledByDefault() const = 0;

	virtual void EnableForSession(bool bEnabled) = 0;
	virtual bool IsEnabledForSession() const = 0;
	virtual bool CanEnableForSession() const = 0;

	virtual bool HasStarted() const = 0;

	virtual void ShowConsole() = 0;
	virtual void Compile() = 0;
	virtual bool IsCompiling() const = 0;
	virtual void Tick() = 0;
};


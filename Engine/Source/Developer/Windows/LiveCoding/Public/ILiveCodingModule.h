#pragma once

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"

#define LIVE_CODING_MODULE_NAME "LiveCoding"

class ILiveCodingModule : public IModuleInterface
{
public:
	virtual void Enable(bool bEnabled) = 0;
	virtual bool IsEnabled() const = 0;
	virtual void ShowConsole() = 0;
	virtual void TriggerRecompile() = 0;
};


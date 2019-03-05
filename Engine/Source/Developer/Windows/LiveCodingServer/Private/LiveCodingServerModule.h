#pragma once

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"
#include "ILiveCodingServer.h"

class FLiveCodingServerModule : public IModuleInterface
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

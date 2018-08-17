#pragma once
 
#include "Modules/ModuleManager.h"
 
class OculusAvatarModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	void StartupModule();
	void ShutdownModule();
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IAnalyticsProviderModule.h"
#include "Modules/ModuleManager.h"

class IAnalyticsProvider;

/**
 * The public interface to this module
 */
class MAGICLEAPANALYTICS_API IMagicLeapAnalyticsPlugin : public IAnalyticsProviderModule
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IMagicLeapAnalyticsPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IMagicLeapAnalyticsPlugin >( "MagicLeapAnalytics" );
	}
};


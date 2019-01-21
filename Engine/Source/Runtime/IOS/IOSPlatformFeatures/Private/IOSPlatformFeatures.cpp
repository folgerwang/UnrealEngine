// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "PlatformFeatures.h"
#include "IOSSaveGameSystem.h"

class FIOSPlatformFeatures : public IPlatformFeaturesModule
{
public:
	virtual class ISaveGameSystem* GetSaveGameSystem() override
	{
		static FIOSSaveGameSystem IOSSaveGameSystem;
		return &IOSSaveGameSystem;
	}
};

IMPLEMENT_MODULE(FIOSPlatformFeatures, IOSPlatformFeatures);

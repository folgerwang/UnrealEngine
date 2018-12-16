// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "CryptoKeysProjectBuildMutatorFeature.h"
#include "CryptoKeysSettings.h"

bool FCryptoKeysProjectBuildMutatorFeature ::RequiresProjectBuild(FName InPlatformInfoName) const
{
	UCryptoKeysSettings* Settings = GetMutableDefault<UCryptoKeysSettings>();
	return Settings->IsEncryptionEnabled() || Settings->IsSigningEnabled();
}
// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusAvatarModule.h"
#include "Features/IModularFeatures.h"
#include "OvrAvatarManager.h"

void OculusAvatarModule::StartupModule()
{
	FOvrAvatarManager::Get().InitializeSDK();
}
 
void OculusAvatarModule::ShutdownModule()
{
	FOvrAvatarManager::Get().ShutdownSDK();
	FOvrAvatarManager::Destroy();
}

IMPLEMENT_MODULE(OculusAvatarModule, OculusAvatar)

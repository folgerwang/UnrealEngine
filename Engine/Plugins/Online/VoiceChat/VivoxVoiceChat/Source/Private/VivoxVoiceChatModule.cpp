// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VivoxVoiceChatModule.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FVivoxVoiceChatModule, VivoxVoiceChat);

extern TUniquePtr<FVivoxVoiceChat> CreateVivoxObject();

void FVivoxVoiceChatModule::StartupModule()
{
	VivoxObj = CreateVivoxObject();
	if (VivoxObj.IsValid())
	{
		IModularFeatures::Get().RegisterModularFeature(TEXT("VoiceChat"), VivoxObj.Get());
	}
}

void FVivoxVoiceChatModule::ShutdownModule()
{
	if (VivoxObj.IsValid())
	{
		IModularFeatures::Get().UnregisterModularFeature(TEXT("VoiceChat"), VivoxObj.Get());
		VivoxObj->Uninitialize();
		VivoxObj.Reset();
	}
}

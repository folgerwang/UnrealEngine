// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#include "VivoxVoiceChat.h"

class FVivoxVoiceChatModule : public IModuleInterface
{
public:
	FVivoxVoiceChatModule() = default;
	~FVivoxVoiceChatModule() = default;

	// ~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~End IModuleInterface

private:

	/** Singleton Vivox object */
	TUniquePtr<FVivoxVoiceChat> VivoxObj;
};
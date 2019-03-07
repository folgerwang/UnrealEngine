// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VivoxVoiceChat.h"

class FWindowsVivoxVoiceChat : public FVivoxVoiceChat
{
public:
	FWindowsVivoxVoiceChat();
	virtual ~FWindowsVivoxVoiceChat();

	// ~Begin IVoiceChat Interface
	virtual bool Initialize() override;
	// ~End IVoiceChat Interface

private:

	bool LoadVivoxModules();
	void UnloadVivoxModules();

	void* VivoxORTPHandle = nullptr;
	void* VivoxSDKHandle = nullptr;
};

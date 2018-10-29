// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FWebMAudioBackendNull
{
public:
	bool InitializePlatform() { return true;  }
	void ShutdownPlatform() {}
	bool StartStreaming(int32 SampleRate, int32 NumOfChannels) { return true; }
	void StopStreaming() {}
	bool SendAudio(const uint8* Buffer, size_t BufferSize) { return true; }
};

using FWebMAudioBackend = FWebMAudioBackendNull;

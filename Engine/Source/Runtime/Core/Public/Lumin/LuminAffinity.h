// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
LuminAffinity.h: Lumin affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class FLuminAffinity : public FGenericPlatformAffinity
{
	const static uint64 ArmCores = MAKEAFFINITYMASK2(3,4);
	const static uint64 DenverCores = MAKEAFFINITYMASK1(2);
public:
	static const CORE_API uint64 GetMainGameMask() {
		return ArmCores;
	}

	static const CORE_API uint64 GetRenderingThreadMask() {
		return ArmCores;
	}

	static const CORE_API uint64 GetRHIThreadMask() {
		return ArmCores;
	}

	static const CORE_API uint64 GetRTHeartBeatMask() {
		return ArmCores;
	}

	static const CORE_API uint64 GetPoolThreadMask() {
		return DenverCores;
	}

	static const CORE_API uint64 GetTaskGraphThreadMask() {
		return ArmCores;
	}

	static const CORE_API uint64 GetStatsThreadMask() {
		return ArmCores;
	}

	static const CORE_API uint64 GetAudioThreadMask() {
		return DenverCores;
	}

	static const CORE_API uint64 GetNoAffinityMask() {
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetTaskGraphBackgroundTaskMask() {
		return ArmCores;
	}

	static EThreadPriority GetRenderingThreadPriority() {
		return TPri_Normal;
	}

	static EThreadPriority GetRHIThreadPriority() {
		return TPri_SlightlyBelowNormal;
	}
};

typedef FLuminAffinity FPlatformAffinity;

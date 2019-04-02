// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
AndroidAffinity.h: Android affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class FAndroidAffinity : public FGenericPlatformAffinity
{
private:
	static uint64 GetLittleCoreMask();
	const static uint64 AllCores = 0xFFFFFFFFFF;
public:
	static const CORE_API uint64 GetMainGameMask()
	{
		return GameThreadMask;
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		return RenderingThreadMask;
	}

	static const CORE_API uint64 GetRHIThreadMask()
	{
		return AllCores;
	}

	static const CORE_API uint64 GetRTHeartBeatMask()
	{
		return GetLittleCoreMask();
	}

	static const CORE_API uint64 GetPoolThreadMask()
	{
		return GetLittleCoreMask();
	}

	static const CORE_API uint64 GetTaskGraphThreadMask()
	{
		return GetLittleCoreMask();
	}

	static const CORE_API uint64 GetStatsThreadMask()
	{
		return GetLittleCoreMask();
	}

	static const CORE_API uint64 GetAudioThreadMask()
	{
		return GetLittleCoreMask();
	}

	static const CORE_API uint64 GetTaskGraphBackgroundTaskMask()
	{
		return GetLittleCoreMask();
	}

	static const CORE_API uint64 GetTaskGraphHighPriorityTaskMask()
	{
		return AllCores;
	}

	static const CORE_API uint64 GetAsyncLoadingThreadMask()
	{
		return AllCores;
	}

	static EThreadPriority GetRenderingThreadPriority()
	{
		return TPri_SlightlyBelowNormal;
	}

	static EThreadPriority GetRHIThreadPriority()
	{
		return TPri_Normal;
	}

public:
	static int64 GameThreadMask;
	static int64 RenderingThreadMask;
};

typedef FAndroidAffinity FPlatformAffinity;

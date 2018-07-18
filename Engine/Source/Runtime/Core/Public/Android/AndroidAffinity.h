// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
AndroidAffinity.h: Android affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class FAndroidAffinity : public FGenericPlatformAffinity
{
public:
	static const CORE_API uint64 GetMainGameMask()
	{
		return GameThreadMask;
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		return RenderingThreadMask;
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

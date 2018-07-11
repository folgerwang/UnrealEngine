// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterGameModeDefault.h"
#include "DisplayClusterPawnDefault.h"

#include "Misc/DisplayClusterLog.h"
#include "IPDisplayCluster.h"
#include "DisplayClusterGlobals.h"


ADisplayClusterGameModeDefault::ADisplayClusterGameModeDefault() :
	Super()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!bIsDisplayClusterActive)
	{
		return;
	}
	
	DefaultPawnClass = ADisplayClusterPawnDefault::StaticClass();
}

ADisplayClusterGameModeDefault::~ADisplayClusterGameModeDefault()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
}


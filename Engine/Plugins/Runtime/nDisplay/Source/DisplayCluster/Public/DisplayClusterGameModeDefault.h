// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterGameMode.h"
#include "DisplayClusterGameModeDefault.generated.h"


/**
 * Extended game mode with some implemented features (navigation)
 */
UCLASS()
class DISPLAYCLUSTER_API ADisplayClusterGameModeDefault
	: public ADisplayClusterGameMode
{
	GENERATED_BODY()
	
public:
	ADisplayClusterGameModeDefault();
	virtual ~ADisplayClusterGameModeDefault();
};

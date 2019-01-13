// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

#include "Network/Session/DisplayClusterSessionBase.h"
#include "Network/Session/IDisplayClusterSessionListener.h"


/**
 * TCP connection session for nDisplay internal communication
 */
class FDisplayClusterSessionInternal : public FDisplayClusterSessionBase
{
public:
	FDisplayClusterSessionInternal(FSocket* InSocket, IDisplayClusterSessionListener* InListener, const FString& InName = FString("SessionInternal"));
	virtual ~FDisplayClusterSessionInternal();

public:
	virtual uint32 Run() override;
};

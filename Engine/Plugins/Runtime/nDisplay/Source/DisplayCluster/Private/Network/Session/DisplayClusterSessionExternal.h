// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/Session/DisplayClusterSessionBase.h"

class IDisplayClusterSessionListener;


/**
 * TCP connection session for external clients (no internal protocols used)
 */
class FDisplayClusterSessionExternal : public FDisplayClusterSessionBase
{
public:
	FDisplayClusterSessionExternal(FSocket* InSocket, IDisplayClusterSessionListener* InListener, const FString& InName = FString("SessionExternal"));
	virtual ~FDisplayClusterSessionExternal();

public:
	virtual uint32 Run() override;
};

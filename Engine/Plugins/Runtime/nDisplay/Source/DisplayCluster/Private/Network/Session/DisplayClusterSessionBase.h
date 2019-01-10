// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

#include "Network/DisplayClusterSocketOps.h"
#include "Network/Session/IDisplayClusterSessionListener.h"


/**
 * Base class for TCP connection session
 */
class FDisplayClusterSessionBase
	: public    FRunnable
	, protected FDisplayClusterSocketOps
{
public:
	FDisplayClusterSessionBase(FSocket* InSocket, IDisplayClusterSessionListener* InListener, const FString& InName = FString("DisplayClusterSession"));
	virtual ~FDisplayClusterSessionBase();

	virtual FString GetName() const override final
	{ return Name; }

	virtual void StartSession();

public:
	virtual void Stop() override;

protected:
	IDisplayClusterSessionListener* GetListener() const
	{ return Listener; }

private:
	const FString        Name;
	IDisplayClusterSessionListener* Listener = nullptr;
	FRunnableThread* ThreadObj = nullptr;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "DisplayClusterSocketOps.h"

#include "IDisplayClusterSessionListener.h"


/**
 * TCP connection session
 */
class FDisplayClusterSession
	: public    FRunnable
	, protected FDisplayClusterSocketOps
{
public:
	FDisplayClusterSession(FSocket* pSock, IDisplayClusterSessionListener* pListener, const FString& name = FString("DisplayClusterSession"));
	~FDisplayClusterSession();

	virtual FString GetName() const override final
	{ return Name; }

private:
	virtual uint32 Run() override;
	virtual void   Stop() override;

private:
	const FString        Name;
	IDisplayClusterSessionListener* Listener = nullptr;
	FRunnableThread* ThreadObj = nullptr;
};


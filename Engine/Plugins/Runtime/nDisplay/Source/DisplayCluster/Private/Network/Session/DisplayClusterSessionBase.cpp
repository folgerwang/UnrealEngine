// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Session/DisplayClusterSessionBase.h"
#include "Network/DisplayClusterServer.h"
#include "Network/DisplayClusterMessage.h"

#include "HAL/RunnableThread.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterSessionBase::FDisplayClusterSessionBase(FSocket* InSocket, IDisplayClusterSessionListener* InListener, const FString& InName) :
	FDisplayClusterSocketOps(InSocket),
	Name(InName),
	Listener(InListener)
{
	check(InSocket);
	check(InListener);
}

FDisplayClusterSessionBase::~FDisplayClusterSessionBase()
{
	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("Session %s .dtor"), *Name);

	ThreadObj->WaitForCompletion();
	delete ThreadObj;
}

void FDisplayClusterSessionBase::StartSession()
{
	Listener->NotifySessionOpen(this);

	ThreadObj = FRunnableThread::Create(this, *(Name + FString("_thread")), 128 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
	ensure(ThreadObj);

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session %s started"), *Name);
}

void FDisplayClusterSessionBase::Stop()
{
	GetSocket()->Close();
	GetListener()->NotifySessionClose(this);
}

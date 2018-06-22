// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSession.h"
#include "DisplayClusterServer.h"
#include "DisplayClusterMessage.h"

#include "HAL/RunnableThread.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterSession::FDisplayClusterSession(FSocket* pSock, IDisplayClusterSessionListener* pListener, const FString& name) :
	FDisplayClusterSocketOps(pSock),
	Name(name),
	Listener(pListener)
{
	check(pSock);
	check(pListener);

	ThreadObj = FRunnableThread::Create(this, *(Name + FString("_thread")), 128 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
	ensure(ThreadObj);

	Listener->NotifySessionOpen(this);

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session %s started"), *Name);
}

FDisplayClusterSession::~FDisplayClusterSession()
{
	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("Session %s .dtor"), *Name);

	Stop();
	ThreadObj->WaitForCompletion();
	delete ThreadObj;
}

void FDisplayClusterSession::Stop()
{
	GetSocket()->Close();
}

uint32 FDisplayClusterSession::Run()
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session thread %s started"), *Name);

	while (IsOpen())
	{
		FDisplayClusterMessage::Ptr req = RecvMsg();
		if (req.IsValid())
		{
			FDisplayClusterMessage::Ptr resp = Listener->ProcessMessage(req);
			if (resp.IsValid())
			{
				if (SendMsg(resp))
				{
					// 'Transaction' has been completed successfully so we continue the processing
					continue;
				}
			}
		}

		GetSocket()->Close();
		Listener->NotifySessionClose(this);
	}

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session thread %s finished"), *Name);
	return 0;
}

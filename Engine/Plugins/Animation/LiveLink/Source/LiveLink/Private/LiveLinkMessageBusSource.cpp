// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusSource.h"
#include "LiveLinkMessages.h"
#include "ILiveLinkClient.h"
#include "LiveLinkMessageBusHeartbeatManager.h"

#include "MessageEndpointBuilder.h"

const double LL_CONNECTION_TIMEOUT = 15.0;
const double LL_HALF_CONNECTION_TIMEOUT = LL_CONNECTION_TIMEOUT / 2.0;

void FLiveLinkMessageBusSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;

	MessageEndpoint = FMessageEndpoint::Builder(TEXT("LiveLinkMessageBusSource"))
					  .Handling<FLiveLinkSubjectDataMessage>(this, &FLiveLinkMessageBusSource::HandleSubjectData)
					  .Handling<FLiveLinkSubjectFrameMessage>(this, &FLiveLinkMessageBusSource::HandleSubjectFrame)
					  .Handling<FLiveLinkHeartbeatMessage>(this, &FLiveLinkMessageBusSource::HandleHeartbeat)
					  .Handling<FLiveLinkClearSubject>(this, &FLiveLinkMessageBusSource::HandleClearSubject)
					  .ReceivingOnAnyThread();


	MessageEndpoint->Send(new FLiveLinkConnectMessage(), ConnectionAddress);
	
	// Register for heartbeats
	bIsValid = true;
	FHeartbeatManager::Get()->RegisterSource(this);

	UpdateConnectionLastActive();
}

bool FLiveLinkMessageBusSource::SendHeartbeat()
{
	const double CurrentTime = FPlatformTime::Seconds();

	{
		// Ensure the read of ConnectionLastActive is Threadsafe
		FScopeLock ConnectionTimeLock(&ConnectionLastActiveSection);

		if (HeartbeatLastSent > (CurrentTime - LL_HALF_CONNECTION_TIMEOUT) &&
			ConnectionLastActive < (CurrentTime - LL_CONNECTION_TIMEOUT))
		{
			//We have recently tried to heartbeat and not received anything back
			bIsValid = false;
		}
	}

	MessageEndpoint->Send(new FLiveLinkHeartbeatMessage(), ConnectionAddress);
	HeartbeatLastSent = CurrentTime;
	return bIsValid;
}


bool FLiveLinkMessageBusSource::IsSourceStillValid()
{
	return bIsValid;
}

void FLiveLinkMessageBusSource::HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();
}

void FLiveLinkMessageBusSource::HandleClearSubject(const FLiveLinkClearSubject& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();

	Client->ClearSubject(Message.SubjectName);
}

FORCEINLINE void FLiveLinkMessageBusSource::UpdateConnectionLastActive()
{
	FScopeLock ConnectionTimeLock(&ConnectionLastActiveSection);

	ConnectionLastActive = FPlatformTime::Seconds();
}

bool FLiveLinkMessageBusSource::RequestSourceShutdown()
{
	FHeartbeatManager* HeartbeatManager = FHeartbeatManager::Get();
	if (HeartbeatManager->IsRunning())
	{
		HeartbeatManager->RemoveSource(this);
	}
	FMessageEndpoint::SafeRelease(MessageEndpoint);
	return true;
}

void FLiveLinkMessageBusSource::HandleSubjectData(const FLiveLinkSubjectDataMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();

	Client->PushSubjectSkeleton(SourceGuid, Message.SubjectName, Message.RefSkeleton);
}

void FLiveLinkMessageBusSource::HandleSubjectFrame(const FLiveLinkSubjectFrameMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();

	FLiveLinkFrameData FrameData;
	FrameData.Transforms = Message.Transforms;
	FrameData.CurveElements = Message.Curves;
	FrameData.MetaData = Message.MetaData;
	FrameData.WorldTime = FLiveLinkWorldTime(Message.Time);
	Client->PushSubjectData(SourceGuid, Message.SubjectName, FrameData);
}
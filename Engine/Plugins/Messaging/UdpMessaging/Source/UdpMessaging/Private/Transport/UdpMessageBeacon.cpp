// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageBeacon.h"
#include "UdpMessagingPrivate.h"

#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IPAddress.h"
#include "Serialization/ArrayWriter.h"
#include "Sockets.h"


/* FUdpMessageHelloSender static initialization
 *****************************************************************************/

const FTimespan FUdpMessageBeacon::IntervalPerEndpoint = FTimespan::FromMilliseconds(200);
const FTimespan FUdpMessageBeacon::MinimumInterval = FTimespan::FromMilliseconds(1000);


/* FUdpMessageHelloSender structors
 *****************************************************************************/

FUdpMessageBeacon::FUdpMessageBeacon(FSocket* InSocket, const FGuid& InSocketId, const FIPv4Endpoint& InMulticastEndpoint)
	: BeaconInterval(MinimumInterval)
	, LastEndpointCount(1)
	, LastHelloSent(FDateTime::MinValue())
	, NextHelloTime(FDateTime::UtcNow())
	, NodeId(InSocketId)
	, Socket(InSocket)
	, Stopping(false)
{
	EndpointLeftEvent = FPlatformProcess::GetSynchEventFromPool(false);
	MulticastAddress = InMulticastEndpoint.ToInternetAddr();

	Thread = FRunnableThread::Create(this, TEXT("FUdpMessageBeacon"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}


FUdpMessageBeacon::~FUdpMessageBeacon()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
	}

	MulticastAddress = nullptr;

	FPlatformProcess::ReturnSynchEventToPool(EndpointLeftEvent);
	EndpointLeftEvent = nullptr;
}


/* FUdpMessageHelloSender interface
 *****************************************************************************/

void FUdpMessageBeacon::SetEndpointCount(int32 EndpointCount)
{
	check(EndpointCount > 0);

	if (EndpointCount < LastEndpointCount)
	{
		FDateTime CurrentTime = FDateTime::UtcNow();

		// adjust the send interval for reduced number of endpoints
		NextHelloTime = CurrentTime + (EndpointCount / LastEndpointCount) * (NextHelloTime - CurrentTime);
		LastHelloSent = CurrentTime - (EndpointCount / LastEndpointCount) * (CurrentTime - LastHelloSent);
		LastEndpointCount = EndpointCount;

		EndpointLeftEvent->Trigger();
	}
}


/* FRunnable interface
 *****************************************************************************/

FSingleThreadRunnable* FUdpMessageBeacon::GetSingleThreadInterface()
{
	return this;
}


bool FUdpMessageBeacon::Init()
{
	return true;
}


uint32 FUdpMessageBeacon::Run()
{
	while (!Stopping)
	{
		FDateTime CurrentTime = FDateTime::UtcNow();
		Update(CurrentTime, BeaconInterval);
		EndpointLeftEvent->Wait(NextHelloTime - CurrentTime);
	}

	SendSegment(EUdpMessageSegments::Bye, BeaconInterval);

	return 0;
}


void FUdpMessageBeacon::Stop()
{
	Stopping = true;
}


/* FUdpMessageHelloSender implementation
 *****************************************************************************/

bool FUdpMessageBeacon::SendSegment(EUdpMessageSegments SegmentType, const FTimespan& SocketWaitTime)
{
	FUdpMessageSegment::FHeader Header;
	{
		Header.SenderNodeId = NodeId;
		Header.ProtocolVersion = UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION;
		Header.SegmentType = SegmentType;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		Writer << NodeId;
	}

	int32 Sent;

	if (!Socket->Wait(ESocketWaitConditions::WaitForWrite, SocketWaitTime))
	{
		return false; // socket not ready for sending
	}

	if (!Socket->SendTo(Writer.GetData(), Writer.Num(), Sent, *MulticastAddress))
	{
		return false; // send failed
	}

	return true;
}


void FUdpMessageBeacon::Update(const FDateTime& CurrentTime, const FTimespan& SocketWaitTime)
{
	if (CurrentTime < NextHelloTime)
	{
		return;
	}

	BeaconInterval = FMath::Max(MinimumInterval, IntervalPerEndpoint * LastEndpointCount);

	if (SendSegment(EUdpMessageSegments::Hello, SocketWaitTime))
	{
		NextHelloTime = CurrentTime + BeaconInterval;
	}
}


/* FSingleThreadRunnable interface
 *****************************************************************************/

void FUdpMessageBeacon::Tick()
{
	Update(FDateTime::UtcNow(), FTimespan::Zero());
}

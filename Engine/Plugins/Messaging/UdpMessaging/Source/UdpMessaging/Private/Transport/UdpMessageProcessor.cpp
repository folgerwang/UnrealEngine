// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageProcessor.h"
#include "UdpMessagingPrivate.h"

#include "Common/UdpSocketSender.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "IMessageAttachment.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Sockets.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/Class.h"

#include "Shared/UdpMessagingSettings.h"
#include "Transport/UdpMessageBeacon.h"
#include "Transport/UdpMessageSegmenter.h"
#include "Transport/UdpReassembledMessage.h"
#include "Transport/UdpSerializedMessage.h"
#include "Transport/UdpSerializeMessageTask.h"


/* FUdpMessageHelloSender static initialization
 *****************************************************************************/

const int32 FUdpMessageProcessor::DeadHelloIntervals = 5;


/* FUdpMessageProcessor structors
 *****************************************************************************/

FUdpMessageProcessor::FUdpMessageProcessor(FSocket& InSocket, const FGuid& InNodeId, const FIPv4Endpoint& InMulticastEndpoint)
	: Beacon(nullptr)
	, LocalNodeId(InNodeId)
	, LastSentMessage(-1)
	, MulticastEndpoint(InMulticastEndpoint)
	, Socket(&InSocket)
	, SocketSender(nullptr)
	, Stopping(false)
{
	WorkEvent = MakeShareable(FPlatformProcess::GetSynchEventFromPool(), [](FEvent* EventToDelete)
	{
		FPlatformProcess::ReturnSynchEventToPool(EventToDelete);
	});

	const UUdpMessagingSettings& Settings = *GetDefault<UUdpMessagingSettings>();

	for (auto& StaticEndpoint : Settings.StaticEndpoints)
	{
		FIPv4Endpoint Endpoint;

		if (FIPv4Endpoint::Parse(StaticEndpoint, Endpoint))
		{
			FNodeInfo& NodeInfo = StaticNodes.FindOrAdd(Endpoint);
			NodeInfo.Endpoint = Endpoint;
		}
		else
		{
			UE_LOG(LogUdpMessaging, Warning, TEXT("Invalid UDP Messaging Static Endpoint '%s'"), *StaticEndpoint);
		}
	}

	Thread = FRunnableThread::Create(this, TEXT("FUdpMessageProcessor"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}


FUdpMessageProcessor::~FUdpMessageProcessor()
{
	// shut down worker thread
	Thread->Kill(true);
	delete Thread;
	Thread = nullptr;

	// remove all transport nodes
	if (NodeLostDelegate.IsBound())
	{
		for (auto& KnownNodePair : KnownNodes)
		{
			NodeLostDelegate.Execute(KnownNodePair.Key);
		}
	}

	KnownNodes.Empty();
}


/* FUdpMessageProcessor interface
 *****************************************************************************/

TMap<uint8, TArray<FGuid>> FUdpMessageProcessor::GetRecipientsPerProtocolVersion(const TArray<FGuid>& Recipients)
{
	TMap<uint8, TArray<FGuid>> NodesPerVersion;
	{
		FScopeLock NodeVersionsLock(&NodeVersionCS);

		// No recipients means a publish, so broadcast to all known nodes (static nodes are in known nodes.)
		// We used to broadcast on the multicast endpoint, but the discovery of nodes should have found available nodes using multicast already
		if (Recipients.Num() == 0)
		{
			for (auto& NodePair : NodeVersions)
			{
				NodesPerVersion.FindOrAdd(NodePair.Value).Add(NodePair.Key);
			}
		}
		else
		{
			for (const FGuid& Recipient : Recipients)
			{
				uint8* Version = NodeVersions.Find(Recipient);
				if (Version)
				{
					NodesPerVersion.FindOrAdd(*Version).Add(Recipient);
				}
			}
		}
	}
	return NodesPerVersion;
}

bool FUdpMessageProcessor::EnqueueInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& InSender)
{
	if (!InboundSegments.Enqueue(FInboundSegment(Data, InSender)))
	{
		return false;
	}

	WorkEvent->Trigger();

	return true;
}

bool FUdpMessageProcessor::EnqueueOutboundMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, const TArray<FGuid>& Recipients)
{
	TMap<uint8, TArray<FGuid>> RecipientPerVersions = GetRecipientsPerProtocolVersion(Recipients);
	for (const auto& RecipientVersion : RecipientPerVersions)
	{
		// Create a message to serialize using that protocol version
		TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage = MakeShared<FUdpSerializedMessage, ESPMode::ThreadSafe>(RecipientVersion.Key, MessageContext->GetFlags());

		// Kick off the serialization task
		TGraphTask<FUdpSerializeMessageTask>::CreateTask().ConstructAndDispatchWhenReady(MessageContext, SerializedMessage, WorkEvent);

		// Enqueue the message
		if (!OutboundMessages.Enqueue(FOutboundMessage(SerializedMessage, RecipientVersion.Value)))
		{
			return false;
		}
	}

	return true;
}

/* FRunnable interface
 *****************************************************************************/

FSingleThreadRunnable* FUdpMessageProcessor::GetSingleThreadInterface()
{
	return this;
}


bool FUdpMessageProcessor::Init()
{
	TArray<FIPv4Endpoint> StaticEndpoints;
	StaticNodes.GenerateKeyArray(StaticEndpoints);

	Beacon = new FUdpMessageBeacon(Socket, LocalNodeId, MulticastEndpoint, StaticEndpoints);
	SocketSender = new FUdpSocketSender(Socket, TEXT("FUdpMessageProcessor.Sender"));

	SupportedProtocolVersions.Add(UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION);
	// Support Protocol version 10
	SupportedProtocolVersions.Add(10);

	return true;
}


uint32 FUdpMessageProcessor::Run()
{
	while (!Stopping)
	{
		CurrentTime = FDateTime::UtcNow();

		if (WorkEvent->Wait(CalculateWaitTime()))
		{
			ConsumeInboundSegments();
			ConsumeOutboundMessages();
		}
		UpdateKnownNodes();
		UpdateStaticNodes();
	}
	
	delete Beacon;
	Beacon = nullptr;

	delete SocketSender;
	SocketSender = nullptr;

	return 0;
}


void FUdpMessageProcessor::Stop()
{
	Stopping = true;
	WorkEvent->Trigger();
}


/* FSingleThreadRunnable interface
*****************************************************************************/

void FUdpMessageProcessor::Tick()
{
	CurrentTime = FDateTime::UtcNow();

	ConsumeInboundSegments();
	ConsumeOutboundMessages();
	UpdateKnownNodes();
	UpdateStaticNodes();
}

/* FUdpMessageProcessor implementation
 *****************************************************************************/

void FUdpMessageProcessor::AcknowledgeReceipt(int32 MessageId, const FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FHeader Header;
	{
		Header.ProtocolVersion = NodeInfo.ProtocolVersion;
		Header.RecipientNodeId = NodeInfo.NodeId;
		Header.SenderNodeId = LocalNodeId;
		Header.SegmentType = EUdpMessageSegments::Acknowledge;
	}

	FUdpMessageSegment::FAcknowledgeChunk AcknowledgeChunk;
	{
		AcknowledgeChunk.MessageId = MessageId;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		AcknowledgeChunk.Serialize(Writer, NodeInfo.ProtocolVersion);
	}

	int32 OutSent;
	Socket->SendTo(Writer.GetData(), Writer.Num(), OutSent, *NodeInfo.Endpoint.ToInternetAddr());
}


FTimespan FUdpMessageProcessor::CalculateWaitTime() const
{
	return FTimespan::FromMilliseconds(10);
}


void FUdpMessageProcessor::ConsumeInboundSegments()
{
	FInboundSegment Segment;

	while (InboundSegments.Dequeue(Segment))
	{
		// quick hack for TTP# 247103
		if (!Segment.Data.IsValid())
		{
			continue;
		}

		FUdpMessageSegment::FHeader Header;
		*Segment.Data << Header;

		if (FilterSegment(Header))
		{
			FNodeInfo& NodeInfo = KnownNodes.FindOrAdd(Header.SenderNodeId);

			if (!NodeInfo.NodeId.IsValid())
			{
				NodeInfo.NodeId = Header.SenderNodeId;
				NodeInfo.ProtocolVersion = Header.ProtocolVersion;
				NodeDiscoveredDelegate.ExecuteIfBound(NodeInfo.NodeId);
			}

			NodeInfo.ProtocolVersion = Header.ProtocolVersion;
			NodeInfo.Endpoint = Segment.Sender;
			NodeInfo.LastSegmentReceivedTime = CurrentTime;

			switch (Header.SegmentType)
			{
			case EUdpMessageSegments::Abort:
				ProcessAbortSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Acknowledge:
				ProcessAcknowledgeSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::AcknowledgeSegments:
				ProcessAcknowledgeSegmentsSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Bye:
				ProcessByeSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Data:			
				ProcessDataSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Hello:
				ProcessHelloSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Ping:
				ProcessPingSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Pong:
				ProcessPongSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Retransmit:
				ProcessRetransmitSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Timeout:
				ProcessTimeoutSegment(Segment, NodeInfo);
				break;

			default:
				ProcessUnknownSegment(Segment, NodeInfo, (uint8)Header.SegmentType);
			}
		}
	}
}


void FUdpMessageProcessor::ConsumeOutboundMessages()
{
	FOutboundMessage OutboundMessage;

	while (OutboundMessages.Dequeue(OutboundMessage))
	{
		++LastSentMessage;

		for (const FGuid& RecipientId : OutboundMessage.RecipientIds)
		{
			FNodeInfo* RecipientNodeInfo = KnownNodes.Find(RecipientId);
			// Queue segmenters to the nodes we are dispatching to
			if (RecipientNodeInfo)
			{
				RecipientNodeInfo->Segmenters.Add(
					LastSentMessage,
					MakeShared<FUdpMessageSegmenter>(OutboundMessage.SerializedMessage.ToSharedRef(), UDP_MESSAGING_SEGMENT_SIZE)
				);
			}
		}
	}
}


bool FUdpMessageProcessor::FilterSegment(const FUdpMessageSegment::FHeader& Header)
{
	// filter locally generated segments
	if (Header.SenderNodeId == LocalNodeId)
	{
		return false;
	}

	// filter unsupported protocol versions
	if (!SupportedProtocolVersions.Contains(Header.ProtocolVersion))
	{
		return false;
	}

	return true;
}


void FUdpMessageProcessor::ProcessAbortSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FAbortChunk AbortChunk;
	AbortChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	NodeInfo.Segmenters.Remove(AbortChunk.MessageId);
}


void FUdpMessageProcessor::ProcessAcknowledgeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FAcknowledgeChunk AcknowledgeChunk;
	AcknowledgeChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	NodeInfo.Segmenters.Remove(AcknowledgeChunk.MessageId);
}


void FUdpMessageProcessor::ProcessAcknowledgeSegmentsSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo) // TODO: Rename function
{
	FUdpMessageSegment::FAcknowledgeSegmentsChunk AcknowledgeChunk;
	AcknowledgeChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(AcknowledgeChunk.MessageId);
	if (Segmenter.IsValid())
	{
		Segmenter->MarkAsAcknowledged(AcknowledgeChunk.Segments);
		if (Segmenter->IsComplete())
		{
			NodeInfo.Segmenters.Remove(AcknowledgeChunk.MessageId);
		}
	}
}


void FUdpMessageProcessor::ProcessByeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid() && (RemoteNodeId == NodeInfo.NodeId))
	{
		RemoveKnownNode(RemoteNodeId);
	}
}


void FUdpMessageProcessor::ProcessDataSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FDataChunk DataChunk;
	DataChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);
	
	// Discard late segments for sequenced messages
	if ((DataChunk.Sequence != 0) && (DataChunk.Sequence < NodeInfo.Resequencer.GetNextSequence()))
	{
		return;
	}

	TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage = NodeInfo.ReassembledMessages.FindOrAdd(DataChunk.MessageId);

	// Reassemble message
	if (!ReassembledMessage.IsValid())
	{
		ReassembledMessage = MakeShared<FUdpReassembledMessage, ESPMode::ThreadSafe>(NodeInfo.ProtocolVersion, DataChunk.MessageFlags, DataChunk.MessageSize, DataChunk.TotalSegments, DataChunk.Sequence, Segment.Sender);
	}

	ReassembledMessage->Reassemble(DataChunk.SegmentNumber, DataChunk.SegmentOffset, DataChunk.Data, CurrentTime);

	// Deliver or re-sequence message
	if (!ReassembledMessage->IsComplete() || ReassembledMessage->IsDelivered())
	{
		return;
	}

	AcknowledgeReceipt(DataChunk.MessageId, NodeInfo);

	if (ReassembledMessage->GetSequence() == 0)
	{
		if (NodeInfo.NodeId.IsValid())
		{
			MessageReassembledDelegate.ExecuteIfBound(*ReassembledMessage, nullptr, NodeInfo.NodeId);
		}
	}
	else if (NodeInfo.Resequencer.Resequence(ReassembledMessage))
	{
		TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe> ResequencedMessage;

		while (NodeInfo.Resequencer.Pop(ResequencedMessage))
		{
			if (NodeInfo.NodeId.IsValid())
			{
				MessageReassembledDelegate.ExecuteIfBound(*ResequencedMessage, nullptr, NodeInfo.NodeId);
			}
		}
	}
	// Mark the message delivered but do not remove it from the list yet, this is to prevent the double delivery of reliable message
	ReassembledMessage->MarkDelivered();
}


void FUdpMessageProcessor::ProcessHelloSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}
}

void FUdpMessageProcessor::ProcessPingSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;
	uint8 NodeProtocolVersion;
	*Segment.Data << NodeProtocolVersion;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}
	
	// The protocol version we are going to use to communicate to this node is the smallest between its version and our own
	uint8 ProtocolVersion = FMath::Min<uint8>(NodeProtocolVersion, UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION);

	// if that protocol isn't in our supported protocols we do not reply to the pong and remove this node since we don't support its version
	if (!SupportedProtocolVersions.Contains(ProtocolVersion))
	{
		RemoveKnownNode(NodeInfo.NodeId);
		return;
	}

	// Set this node protocol to our agreed protocol
	NodeInfo.ProtocolVersion = ProtocolVersion;

	// Send the pong
	FUdpMessageSegment::FHeader Header;
	{
		// Reply to the ping using the agreed protocol
		Header.ProtocolVersion = ProtocolVersion;
		Header.RecipientNodeId = NodeInfo.NodeId;
		Header.SenderNodeId = LocalNodeId;
		Header.SegmentType = EUdpMessageSegments::Pong;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		Writer << LocalNodeId;
	}

	int32 OutSent;
	Socket->SendTo(Writer.GetData(), Writer.Num(), OutSent, *NodeInfo.Endpoint.ToInternetAddr());
}


void FUdpMessageProcessor::ProcessPongSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}
}


void FUdpMessageProcessor::ProcessRetransmitSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FRetransmitChunk RetransmitChunk;
	RetransmitChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(RetransmitChunk.MessageId);

	if (Segmenter.IsValid())
	{
		Segmenter->MarkForRetransmission(RetransmitChunk.Segments);
	}
}


void FUdpMessageProcessor::ProcessTimeoutSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FTimeoutChunk TimeoutChunk;
	TimeoutChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(TimeoutChunk.MessageId);

	if (Segmenter.IsValid())
	{
		Segmenter->MarkForRetransmission();
	}
}


void FUdpMessageProcessor::ProcessUnknownSegment(FInboundSegment& Segment, FNodeInfo& EndpointInfo, uint8 SegmentType)
{
	UE_LOG(LogUdpMessaging, Verbose, TEXT("Received unknown segment type '%i' from %s"), SegmentType, *Segment.Sender.ToText().ToString());
}


void FUdpMessageProcessor::RemoveKnownNode(const FGuid& NodeId)
{
	NodeLostDelegate.ExecuteIfBound(NodeId);
	KnownNodes.Remove(NodeId);
}

void FUdpMessageProcessor::UpdateKnownNodes()
{
	// remove dead remote endpoints
	FTimespan DeadHelloTimespan = DeadHelloIntervals * Beacon->GetBeaconInterval();
	TArray<FGuid> NodesToRemove;

	for (auto& KnownNodePair : KnownNodes)
	{
		FGuid& NodeId = KnownNodePair.Key;
		FNodeInfo& NodeInfo = KnownNodePair.Value;

		if ((NodeId.IsValid()) && ((NodeInfo.LastSegmentReceivedTime + DeadHelloTimespan) <= CurrentTime))
		{
			NodesToRemove.Add(NodeId);
		}
		else
		{
			UpdateSegmenters(NodeInfo);
			UpdateReassemblers(NodeInfo);
		}
	}

	for (const auto& Node : NodesToRemove)
	{
		RemoveKnownNode(Node);
	}

	UpdateNodesPerVersion();

	Beacon->SetEndpointCount(KnownNodes.Num() + 1);
}


void FUdpMessageProcessor::UpdateSegmenters(FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FHeader Header
	{
		NodeInfo.ProtocolVersion,		// Header.ProtocolVersion - Send data segment using the node protocol version
		NodeInfo.NodeId,				// Header.RecipientNodeId
		LocalNodeId,					// Header.SenderNodeId
		EUdpMessageSegments::Data		// Header.SegmentType
	};

	for (TMap<int32, TSharedPtr<FUdpMessageSegmenter> >::TIterator It(NodeInfo.Segmenters); It; ++It)
	{
		TSharedPtr<FUdpMessageSegmenter>& Segmenter = It.Value();

		Segmenter->Initialize();

		if (Segmenter->IsInitialized() && Segmenter->NeedSending(CurrentTime))
		{
			FUdpMessageSegment::FDataChunk DataChunk;

			for (TConstSetBitIterator<> BIt(Segmenter->GetPendingSegments()); BIt; ++BIt)
			{
				Segmenter->GetPendingSegment(BIt.GetIndex(), DataChunk.Data);
				DataChunk.SegmentNumber = BIt.GetIndex();

				DataChunk.MessageId = It.Key();
				DataChunk.MessageFlags = Segmenter->GetMessageFlags();
				DataChunk.MessageSize = Segmenter->GetMessageSize();
				DataChunk.SegmentOffset = UDP_MESSAGING_SEGMENT_SIZE * DataChunk.SegmentNumber;
				DataChunk.Sequence = 0; // @todo gmp: implement message sequencing
				DataChunk.TotalSegments = Segmenter->GetSegmentCount();

				// validate with are sending message in the proper protocol version
				check(Header.ProtocolVersion == Segmenter->GetProtocolVersion());

				TSharedRef<FArrayWriter, ESPMode::ThreadSafe> Writer = MakeShared<FArrayWriter, ESPMode::ThreadSafe>();
				{
					*Writer << Header;
					DataChunk.Serialize(*Writer, Header.ProtocolVersion);
				}

				if (!SocketSender->Send(Writer, NodeInfo.Endpoint))
				{
					return;
 				}
			}

			// update sent time for reliable messages
			if (EnumHasAnyFlags(Segmenter->GetMessageFlags(), EMessageFlags::Reliable))
			{
				Segmenter->UpdateSentTime(CurrentTime);
			}
			// if message isn't reliable no need to keep track
			else
			{
				It.RemoveCurrent();
			}
		}
		else if (Segmenter->IsInvalid())
		{
			It.RemoveCurrent();
		}
	}
}


const FTimespan FUdpMessageProcessor::StaleReassemblyInterval = FTimespan::FromSeconds(30);

void FUdpMessageProcessor::UpdateReassemblers(FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FHeader Header
	{
		FMath::Max(NodeInfo.ProtocolVersion, (uint8)11),	// Header.ProtocolVersion, AcknowledgeSegments are version 11 and onward segment
		NodeInfo.NodeId,									// Header.RecipientNodeId
		LocalNodeId,										// Header.SenderNodeId
		EUdpMessageSegments::AcknowledgeSegments			// Header.SegmentType
	};

	for (TMap<int32, TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>>::TIterator It(NodeInfo.ReassembledMessages); It; ++It)
	{
		TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage = It.Value();
		FUdpMessageSegment::FAcknowledgeSegmentsChunk AcknowledgeChunk
		{
			It.Key(),										// MessageId
			ReassembledMessage->GetPendingAcknowledgments()	// Segments
		};

		// Send pending acknowledgments
		if (AcknowledgeChunk.Segments.Num() > 0)
		{
			TSharedRef<FArrayWriter, ESPMode::ThreadSafe> Writer = MakeShared<FArrayWriter, ESPMode::ThreadSafe>();
			{
				*Writer << Header;
				AcknowledgeChunk.Serialize(*Writer, Header.ProtocolVersion);				
			}

			if (!SocketSender->Send(Writer, NodeInfo.Endpoint))
			{
				return;
			}
		}

		// Remove stale reassembled message if they aren't reliable or are marked delivered
		if (ReassembledMessage->GetLastSegmentTime() + StaleReassemblyInterval <= CurrentTime &&
			(!EnumHasAnyFlags(ReassembledMessage->GetFlags(), EMessageFlags::Reliable) || ReassembledMessage->IsDelivered()))
		{
			It.RemoveCurrent();
		}
	}
}


void FUdpMessageProcessor::UpdateStaticNodes()
{
	for (auto& StaticNodePair : StaticNodes)
	{
		UpdateSegmenters(StaticNodePair.Value);
	}
}

void FUdpMessageProcessor::UpdateNodesPerVersion()
{
	FScopeLock NodeVersionLock(&NodeVersionCS);
	NodeVersions.Empty();
	for (auto& NodePair : KnownNodes)
	{
		NodeVersions.Add(NodePair.Key, NodePair.Value.ProtocolVersion);
	}
}

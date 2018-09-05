// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Common/UdpSocketReceiver.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "IMessageTransport.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/SingleThreadRunnable.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

#include "UdpMessagingPrivate.h"
#include "Shared/UdpMessageSegment.h"
#include "Transport/UdpMessageResequencer.h"

class FArrayReader;
class FEvent;
class FRunnableThread;
class FSocket;
class FUdpMessageBeacon;
class FUdpMessageSegmenter;
class FUdpReassembledMessage;
class FUdpSerializedMessage;
class FUdpSocketSender;
class IMessageAttachment;


/**
 * Implements a message processor for UDP messages.
 */
class FUdpMessageProcessor
	: public FRunnable
	, private FSingleThreadRunnable
{
	/** Structure for known remote endpoints. */
	struct FNodeInfo
	{
		/** Holds the node's IP endpoint. */
		FIPv4Endpoint Endpoint;

		/** Holds the time at which the last Hello was received. */
		FDateTime LastSegmentReceivedTime;

		/** Holds the endpoint's node identifier. */
		FGuid NodeId;

		/** Holds the protocol version this node is communicating with */
		uint8 ProtocolVersion;

		/** Holds the collection of reassembled messages. */
		TMap<int32, TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>> ReassembledMessages;

		/** Holds the message resequencer. */
		FUdpMessageResequencer Resequencer;

		/** Holds the collection of message segmenters. */
		TMap<int32, TSharedPtr<FUdpMessageSegmenter>> Segmenters;

		/** Default constructor. */
		FNodeInfo()
			: LastSegmentReceivedTime(FDateTime::MinValue())
			, NodeId()
			, ProtocolVersion(UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION)
		{ }

		/** Resets the endpoint info. */
		void ResetIfRestarted(const FGuid& NewNodeId)
		{
			if (NewNodeId != NodeId)
			{
				ReassembledMessages.Reset();
				Resequencer.Reset();

				NodeId = NewNodeId;
			}
		}
	};


	/** Structure for inbound segments. */
	struct FInboundSegment
	{
		/** Holds the segment data. */
		TSharedPtr<FArrayReader, ESPMode::ThreadSafe> Data;

		/** Holds the sender's network endpoint. */
		FIPv4Endpoint Sender;

		/** Default constructor. */
		FInboundSegment() { }

		/** Creates and initializes a new instance. */
		FInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& InData, const FIPv4Endpoint& InSender)
			: Data(InData)
			, Sender(InSender)
		{ }
	};


	/** Structure for outbound messages. */
	struct FOutboundMessage
	{
		/** Holds the serialized message. */
		TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage;

		/** Holds the recipients. */
		TArray<FGuid> RecipientIds;

		/** Default constructor. */
		FOutboundMessage() { }

		/** Creates and initializes a new instance. */
		FOutboundMessage(TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> InSerializedMessage, const TArray<FGuid>& InRecipientIds)
			: SerializedMessage(InSerializedMessage)
			, RecipientIds(InRecipientIds)
		{ }
	};

public:

	/**
	 * Creates and initializes a new message processor.
	 *
	 * @param InSocket The network socket used to transport messages.
	 * @param InNodeId The local node identifier (used to detect the unicast endpoint).
	 * @param InMulticastEndpoint The multicast group endpoint to transport messages to.
	 */
	FUdpMessageProcessor(FSocket& InSocket, const FGuid& InNodeId, const FIPv4Endpoint& InMulticastEndpoint);

	/** Virtual destructor. */
	virtual ~FUdpMessageProcessor();

public:

	/**
	 * Get a list of Nodes Ids split by supported Protocol version
	 *
	 * @param Recipients The list of recipients Ids
	 * @return A map of protocol version -> list of node ids for that protocol
	 */
	TMap<uint8, TArray<FGuid>> GetRecipientsPerProtocolVersion(const TArray<FGuid>& Recipients);

	/**
	 * Queues up an inbound message segment.
	 *
	 * @param Data The segment data.
	 * @param Sender The sender's network endpoint.
	 * @return true if the segment was queued up, false otherwise.
	 */
	bool EnqueueInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& Sender);

	/**
	 * Queues up an outbound message.
	 *
	 * @param MessageContext The message to serialize and send.
	 * @param Recipients The recipients ids to send to.
	 * @return true if the message was queued up, false otherwise.
	 */
	bool EnqueueOutboundMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, const TArray<FGuid>& Recipients);

	/**
	 * Get the event used to signal the message processor that work is available.
	 *
	 * @return The event.
	 */
	const TSharedPtr<FEvent, ESPMode::ThreadSafe>& GetWorkEvent() const
	{
		return WorkEvent;
	}

public:

	// @todo gmp: remove the need for this typedef
	typedef TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> IMessageAttachmentPtr;

	/**
	 * Returns a delegate that is executed when message data has been received.
	 *
	 * @return The delegate.
	 */
	DECLARE_DELEGATE_ThreeParams(FOnMessageReassembled, const FUdpReassembledMessage& /*ReassembledMessage*/, const IMessageAttachmentPtr& /*Attachment*/, const FGuid& /*NodeId*/)
	FOnMessageReassembled& OnMessageReassembled()
	{
		return MessageReassembledDelegate;
	}

	/**
	 * Returns a delegate that is executed when a remote node has been discovered.
	 *
	 * @return The delegate.
	 * @see OnNodeLost
	 */
	DECLARE_DELEGATE_OneParam(FOnNodeDiscovered, const FGuid& /*NodeId*/)
	FOnNodeDiscovered& OnNodeDiscovered()
	{
		return NodeDiscoveredDelegate;
	}

	/**
	 * Returns a delegate that is executed when a remote node was closed or timed out.
	 *
	 * @return The delegate.
	 * @see OnNodeDiscovered
	 */
	DECLARE_DELEGATE_OneParam(FOnNodeLost, const FGuid& /*NodeId*/)
	FOnNodeLost& OnNodeLost()
	{
		return NodeLostDelegate;
	}
	
public:

	//~ FRunnable interface

	virtual FSingleThreadRunnable* GetSingleThreadInterface() override;
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override { }

protected:

	/**
	 * Acknowledges receipt of a message.
	 *
	 * @param MessageId The identifier of the message to acknowledge.
	 * @param NodeInfo Details for the node to send the acknowledgment to.
	 * @todo gmp: batch multiple of these into a single message
	 */
	void AcknowledgeReceipt(int32 MessageId, const FNodeInfo& NodeInfo);

	/**
	 * Calculates the time span that the thread should wait for work.
	 *
	 * @return Wait time.
	 */
	FTimespan CalculateWaitTime() const;

	/** Consumes all inbound segments. */
	void ConsumeInboundSegments();

	/** Consumes all outbound messages. */
	void ConsumeOutboundMessages();

	/**
	 * Filters the specified message segment.
	 *
	 * @param Header The segment header.
	 * @param Data The segment data.
	 * @param Sender The segment sender.
	 * @return true if the segment passed the filter, false otherwise.
	 */
	bool FilterSegment(const FUdpMessageSegment::FHeader& Header);

	/**
	 * Processes an Abort segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessAbortSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes an Acknowledgement segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessAcknowledgeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes an AcknowledgmentSegments segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessAcknowledgeSegmentsSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Bye segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessByeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Ping segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessDataSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Hello segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessHelloSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Ping segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessPingSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Pong segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	*/
	void ProcessPongSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Retransmit segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessRetransmitSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Timeout segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessTimeoutSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes an unknown segment type.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 * @param SegmentType The segment type.
	 */
	void ProcessUnknownSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo, uint8 SegmentType);

	/**
	 * Removes the specified node from the list of known remote endpoints.
	 *
	 * @param NodeId The identifier of the node to remove.
	 */
	void RemoveKnownNode(const FGuid& NodeId);


	/** Updates all known remote nodes. */
	void UpdateKnownNodes();

	/**
	 * Updates all segmenters of the specified node.
	 *
	 * @param NodeInfo Details for the node to update.
	 */
	void UpdateSegmenters(FNodeInfo& NodeInfo);

	/**
	 * Updates all reassemblers of the specified node.
	 *
	 * @param NodeInfo Details for the node to update.
	 */
	void UpdateReassemblers(FNodeInfo& NodeInfo);


	/** Updates all static remote nodes. */
	void UpdateStaticNodes();

	/** Updates nodes per protocol version map */
	void UpdateNodesPerVersion();

protected:

	//~ FSingleThreadRunnable interface

	virtual void Tick() override;

private:

	/** Holds the queue of outbound messages. */
	TQueue<FInboundSegment, EQueueMode::Mpsc> InboundSegments;

	/** Holds the queue of outbound messages. */
	TQueue<FOutboundMessage, EQueueMode::Mpsc> OutboundMessages;

private:

	/** Holds the hello sender. */
	FUdpMessageBeacon* Beacon;

	/** Holds the current time. */
	FDateTime CurrentTime;

	/** Holds the protocol version that can be communicated in. */
	TArray<uint8> SupportedProtocolVersions;

	/** Mutex protecting access to the NodeVersions map. */
	mutable FCriticalSection NodeVersionCS;

	/** Holds the protocol version of each nodes separately for safe access (NodeId -> Protocol Version). */
	TMap<FGuid, uint8> NodeVersions;

	/** Holds the collection of known remote nodes. */
	TMap<FGuid, FNodeInfo> KnownNodes;

	/** Holds the collection of static remote nodes. */
	TMap<FIPv4Endpoint, FNodeInfo> StaticNodes;

	/** Holds the local node identifier. */
	FGuid LocalNodeId;

	/** Holds the last sent message number. */
	int32 LastSentMessage;

	/** Holds the multicast endpoint. */
	FIPv4Endpoint MulticastEndpoint;

	/** Holds the network socket used to transport messages. */
	FSocket* Socket;

	/** Holds the socket sender. */
	FUdpSocketSender* SocketSender;

	/** Holds a flag indicating that the thread is stopping. */
	bool Stopping;

	/** Holds the thread object. */
	FRunnableThread* Thread;

	/** Holds an event signaling that inbound messages need to be processed. */
	TSharedPtr<FEvent, ESPMode::ThreadSafe> WorkEvent;

private:

	/** Holds a delegate to be invoked when a message was received on the transport channel. */
	FOnMessageReassembled MessageReassembledDelegate;

	/** Holds a delegate to be invoked when a network node was discovered. */
	FOnNodeDiscovered NodeDiscoveredDelegate;

	/** Holds a delegate to be invoked when a network node was lost. */
	FOnNodeLost NodeLostDelegate;

private:

	/** Defines the maximum number of Hello segments that can be dropped before a remote endpoint is considered dead. */
	static const int32 DeadHelloIntervals;

	/** Defines a timespan after which non fully reassembled messages that have stopped receiving segments are dropped. */
	static const FTimespan StaleReassemblyInterval;
};

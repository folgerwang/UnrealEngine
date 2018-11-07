// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/DateTime.h"

// IMessageContext forward declaration
enum class EMessageFlags : uint32;

/**
 * Implements a reassembled message.
 */
class FUdpReassembledMessage
{
public:

	/** Default constructor. */
	FUdpReassembledMessage() = default;

	/**
	 * Creates and initializes a new inbound message info.
	 *
	 * @param ProtocolVersion The protocol version the message is serialized in.
	 * @param MessageFlags The message flags of the reassembled message.
	 * @param MessageSize The total size of the message in bytes.
	 * @param SegmentCount The total number of segments that need to be received for this message.
	 * @param InSequence The message sequence number.
	 * @param InSender The IPv4 endpoint of the sender.
	 */
	FUdpReassembledMessage(uint8 InProtocolVersion, EMessageFlags InFlags, int32 MessageSize, int32 SegmentCount, uint64 InSequence, const FIPv4Endpoint& InSender)
		: ProtocolVersion(InProtocolVersion)
		, MessageFlags(InFlags)
		, PendingSegments(true, SegmentCount)
		, PendingSegmentsCount(SegmentCount)
		, ReceivedBytes(0)
		, bIsDelivered(false)
		, RetransmitRequestsCount(0)
		, Sender(InSender)
		, Sequence(InSequence)
	{
		Data.AddUninitialized(MessageSize);
	}

	~FUdpReassembledMessage() = default;

public:

	/**
	 * Gets the message protocol version.
	 *
	 * @return The Message protocol version.
	 */
	uint8 GetProtocolVersion() const
	{
		return ProtocolVersion;
	}

	/**
	 * Gets the message flags
	 *
	 * @return The Message flags.
	 */
	EMessageFlags GetFlags() const
	{
		return MessageFlags;
	}

	/**
	 * Gets the message data.
	 *
	 * @return Message data.
	 */
	const TArray<uint8>& GetData() const
	{
		return Data;
	}

	/**
	 * Gets the time at which the last segment was received.
	 *
	 * @return Last receive time.
	 */
	FDateTime GetLastSegmentTime() const
	{
		return LastSegmentTime;
	}

	/**
	 * Gets the list of segments that haven't been received yet.
	 *
	 * @return List of pending segment numbers.
	 */
	TArray<uint16> GetPendingSegments() const
	{
		TArray<uint16> Result;

		if (PendingSegmentsCount > 0)
		{
			for (TConstSetBitIterator<> It(PendingSegments); It; ++It)
			{
				Result.Add(It.GetIndex());
			}
		}

		return Result;
	}


	/**
	 * Gets the total number of segments.
	 *
	 * @return Number of total segments.
	 */
	uint16 GetTotalSegmentsCount() const
	{
		return PendingSegments.Num();
	}

	/**
	 * Gets the number of segments that haven't been received yet.
	 *
	 * @return Number of pending segments.
	 */
	uint16 GetPendingSegmentsCount() const
	{
		return PendingSegmentsCount;
	}

	/**
	 * Gets the number of retransmit requests that were sent for this payload.
	 *
	 * @return Number of retransmit requests.
	 */
	int32 GetRetransmitRequestsCount() const
	{
		return RetransmitRequestsCount;
	}

	/**
	 * Gets the message's sequence number.
	 *
	 * @return Sequence number.
	 */
	uint64 GetSequence() const
	{
		return Sequence;
	}

	/**
	 * Checks whether this message is complete.
	 *
	 * @return true if the message is complete, false otherwise.
	 */
	bool IsComplete() const
	{
		return (PendingSegmentsCount == 0);
	}

	/**
	 * Checks whether this message has been initialized.
	 *
	 * @return true if the message is initialized, false otherwise.
	 */
	bool IsInitialized() const
	{
		return (Data.Num() < 0);
	}

	/**
	 * Get the list of pending Acknowledgments and clear it
	 *
	 * @return The array of pending acknowledgments
	 */
	TArray<uint16> GetPendingAcknowledgments() 
	{
		TArray<uint16> Temp;
		Swap(Temp, PendingAcknowledgments);
		return Temp;
	}

	/**
	 * Get if the message has been delivered.
	 * @return true if the message has been marked delivered.
	 */
	bool IsDelivered() const
	{
		return bIsDelivered;
	}

	/**
	 * Mark the reasembled message as delivered.
	 */
	void MarkDelivered()
	{
		bIsDelivered = true;
	}


	/**
	 * Reassembles a segment into the specified message.
	 *
	 * @param SegmentNumber The number of the message segment.
	 * @param SegmentOffset The segment's offset within the message.
	 * @param SegmentData The segment data.
	 * @param CurrentTime The current time.
	 */
	void Reassemble(int32 SegmentNumber, int32 SegmentOffset, const TArray<uint8>& SegmentData, const FDateTime& CurrentTime)
	{
		if (SegmentNumber >= PendingSegments.Num())
		{
			// temp hack sanity check
			return;
		}

		LastSegmentTime = CurrentTime;

		if (PendingSegments[SegmentNumber])
		{
			if (SegmentOffset + SegmentData.Num() <= Data.Num())
			{
				FMemory::Memcpy(Data.GetData() + SegmentOffset, SegmentData.GetData(), SegmentData.Num());

				PendingSegments[SegmentNumber] = false;
				--PendingSegmentsCount;
				ReceivedBytes += SegmentData.Num();
			}
		}
		PendingAcknowledgments.Add(SegmentNumber);
	}

private:
	/** Holds the message protocol version. */
	uint8 ProtocolVersion;

	/** */
	EMessageFlags MessageFlags;

	/** Holds the message data. */
	TArray<uint8> Data;

	/** Holds the time at which the last segment was received. */
	FDateTime LastSegmentTime;

	/** Holds an array of bits that indicate which segments still need to be received. */
	TBitArray<> PendingSegments;

	/** Holds the number of segments that haven't been received yet. */
	uint16 PendingSegmentsCount;

	/** Acknowledgment yet to be sent about segments we received */
	TArray<uint16> PendingAcknowledgments;

	/** Holds the number of bytes received so far. */
	int32 ReceivedBytes;

	/** Holds if the reassembled message has been delivered */
	bool bIsDelivered;

	/** Holds the number of retransmit requests that were sent since the last segment was received. */
	int32 RetransmitRequestsCount;

	/** Holds the sender. */
	FIPv4Endpoint Sender;

	/** Holds the message sequence. */
	uint64 Sequence;
};

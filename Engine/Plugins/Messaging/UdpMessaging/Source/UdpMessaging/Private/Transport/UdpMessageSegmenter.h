// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Templates/SharedPointer.h"
#include "Misc/DateTime.h"

// IMessageContext forward declaration
enum class EMessageFlags : uint32;

class FArchive;
class FUdpSerializedMessage;


/**
 * Implements a message segmenter.
 *
 * This class breaks up a message into smaller sized segments that fit into UDP datagrams.
 * It also tracks the segments that still need to be sent.
 */
class FUdpMessageSegmenter
{
public:

	/** Default constructor. */
	FUdpMessageSegmenter()
		: MessageReader(nullptr)
		, SentNumber(0)
	{ }

	/**
	 * Creates and initializes a new message segmenter.
	 *
	 * @param InMessage The serialized message to segment.
	 */
	FUdpMessageSegmenter(const TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe>& InSerializedMessage, uint16 InSegmentSize)
		: MessageReader(nullptr)
		, SegmentSize(InSegmentSize)
		, SentNumber(0)
		, LastSentTime(0)
		, SerializedMessage(InSerializedMessage)
	{ }

	/** Destructor. */
	~FUdpMessageSegmenter();

public:

	/**
	 * Gets the total size of the message in bytes.
	 *
	 * @return Message size.
	 */
	int64 GetMessageSize() const;

	/**
	 * Gets the next pending segment.
	 *
	 * @param OutData Will hold the segment data.
	 * @param OutSegment Will hold the segment number.
	 * @return true if a segment was returned, false if there are no more pending segments.
	 */
	bool GetNextPendingSegment(TArray<uint8>& OutData, uint16& OutSegment) const;

	/**
	 * Gets the pending segment at.
	 *
	 * @param InSegment the segment number we are requesting the data for.
	 * @param OutData Will hold the segment data.
	 * @return true if a segment was returned, false if that segment is no longer pending or the segment number is invalid.
	 */
	bool GetPendingSegment(uint16 InSegment, TArray<uint8>& OutData) const;


	/**
	 * Get the pending segments array.
	 * @return the list of pending segments flags.
	 */
	const TBitArray<>& GetPendingSegments() const
	{
		return PendingSegments;
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
	 * Gets the total number of segments that make up the message.
	 *
	 * @return Segment count.
	 */
	uint16 GetSegmentCount() const
	{
		return PendingSegments.Num();
	}

	/** Initializes the segmenter. */
	void Initialize();

	/**
	 * Checks whether all segments have been sent.
	 *
	 * @return true if all segments were sent, false otherwise.
	 */
	bool IsComplete() const
	{
		return (PendingSegmentsCount == 0);
	}

	/**
	 * Checks whether this segmenter has been initialized.
	 *
	 * @return true if it is initialized, false otherwise.
	 */
	bool IsInitialized() const
	{
		return (MessageReader != nullptr);
	}

	/**
	 * Checks whether this segmenter is invalid.
	 *
	 * @return true if the segmenter is invalid, false otherwise.
	 */
	bool IsInvalid() const;

	/** Return the Protocol Version for this segmenter.	*/
	uint8 GetProtocolVersion() const;

	/** @return the message flags. */
	EMessageFlags GetMessageFlags() const;

	/**
	* Marks the specified segments as acknowledged.
	*
	* @param Segments The acknowledged segments.
	*/
	void MarkAsAcknowledged(const TArray<uint16>& Segments);

	/**
	 * Marks the entire message for retransmission.
	 */
	void MarkForRetransmission()
	{
		PendingSegments.Init(true, PendingSegments.Num());
	}

	/**
	 * Marks the specified segments for retransmission.
	 *
	 * @param Segments The data segments to retransmit.
	 */
	void MarkForRetransmission(const TArray<uint16>& Segments);

	/**
	 * Checks if this segmenter needs to send segments
	 *
	 * @return true if the segmenter needs to send
	 */
	bool NeedSending(const FDateTime& CurrentTime);

	/**
	 * Update the last sent time and increment the sent number for this segmenter
	 *
	 * @param CurrentTime the time to update to.
	 */
	void UpdateSentTime(const FDateTime& CurrentTime);

private:
	/** Defines the time interval for sending. */
	static const FTimespan SendInterval;

	/** temp hack to support new transport API. */
	FArchive* MessageReader;

	/** Holds an array of bits that indicate which segments still need to be sent. */
	TBitArray<> PendingSegments;

	/** Holds the number of segments that haven't been sent yet. */
	uint16 PendingSegmentsCount;

	/** Holds the segment size. */
	uint16 SegmentSize;

	/** Holds the number of time we sent the segments */
	uint16 SentNumber;

	/** Holds the time at which we last sent */
	FDateTime LastSentTime;

	/** Holds the message. */
	TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage;
};

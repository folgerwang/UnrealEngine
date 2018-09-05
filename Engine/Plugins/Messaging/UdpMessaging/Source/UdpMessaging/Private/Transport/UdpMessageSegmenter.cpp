// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageSegmenter.h"

#include "Serialization/Archive.h"

#include "Transport/UdpSerializedMessage.h"


/* FUdpMessageSegmenter structors
 *****************************************************************************/

FUdpMessageSegmenter::~FUdpMessageSegmenter()
{
	if (MessageReader != nullptr)
	{
		delete MessageReader;
	}
}


/* FUdpMessageSegmenter interface
 *****************************************************************************/

int64 FUdpMessageSegmenter::GetMessageSize() const
{
	if (MessageReader == nullptr)
	{
		return 0;
	}

	return MessageReader->TotalSize();
}


bool FUdpMessageSegmenter::GetNextPendingSegment(TArray<uint8>& OutData, uint16& OutSegment) const
{
	if (MessageReader == nullptr)
	{
		return false;
	}

	for (TConstSetBitIterator<> It(PendingSegments); It; ++It)
	{
		OutSegment = It.GetIndex();

		uint32 SegmentOffset = OutSegment * SegmentSize;
		int32 ActualSegmentSize = MessageReader->TotalSize() - SegmentOffset;

		if (ActualSegmentSize > SegmentSize)
		{
			ActualSegmentSize = SegmentSize;
		}

		OutData.Reset(ActualSegmentSize);
		OutData.AddUninitialized(ActualSegmentSize);

		MessageReader->Seek(SegmentOffset);
		MessageReader->Serialize(OutData.GetData(), ActualSegmentSize);

		//FMemory::Memcpy(OutData.GetTypedData(), Message->GetTypedData() + SegmentOffset, ActualSegmentSize);

		return true;
	}

	return false;
}


bool FUdpMessageSegmenter::GetPendingSegment(uint16 InSegment, TArray<uint8>& OutData) const
{
	if (MessageReader == nullptr)
	{
		return false;
	}

	if (InSegment < PendingSegments.Num() && PendingSegments[InSegment])
	{
		uint32 SegmentOffset = InSegment * SegmentSize;
		int32 ActualSegmentSize = MessageReader->TotalSize() - SegmentOffset;

		if (ActualSegmentSize > SegmentSize)
		{
			ActualSegmentSize = SegmentSize;
		}

		OutData.Reset(ActualSegmentSize);
		OutData.AddUninitialized(ActualSegmentSize);

		MessageReader->Seek(SegmentOffset);
		MessageReader->Serialize(OutData.GetData(), ActualSegmentSize);

		return true;
	}

	return false;
}


void FUdpMessageSegmenter::Initialize()
{
	if (MessageReader != nullptr)
	{
		return;
	}

	if (SerializedMessage->GetState() == EUdpSerializedMessageState::Complete)
	{
		MessageReader = SerializedMessage->CreateReader();
		PendingSegmentsCount = (MessageReader->TotalSize() + SegmentSize - 1) / SegmentSize;
		PendingSegments.Init(true, PendingSegmentsCount);
	}
}


bool FUdpMessageSegmenter::IsInvalid() const
{
	return (SerializedMessage->GetState() == EUdpSerializedMessageState::Invalid);
}


uint8 FUdpMessageSegmenter::GetProtocolVersion() const
{
	return SerializedMessage->GetProtocolVersion();
}


EMessageFlags FUdpMessageSegmenter::GetMessageFlags() const
{
	return SerializedMessage->GetFlags();
}


void FUdpMessageSegmenter::MarkAsAcknowledged(const TArray<uint16>& Segments)
{
	for (const auto& Segment : Segments)
	{
		if (Segment < PendingSegments.Num())
		{
			PendingSegments[Segment] = false;
			--PendingSegmentsCount;
		}
	}
}


void FUdpMessageSegmenter::MarkForRetransmission(const TArray<uint16>& Segments)
{
	for (const auto& Segment : Segments)
	{
		if (Segment < PendingSegments.Num())
		{
			PendingSegments[Segment] = true;
		}
	}
}

const FTimespan FUdpMessageSegmenter::SendInterval = FTimespan::FromMilliseconds(100);

bool FUdpMessageSegmenter::NeedSending(const FDateTime& CurrentTime)
{
	return LastSentTime + SendInterval <= CurrentTime;
}

void FUdpMessageSegmenter::UpdateSentTime(const FDateTime& CurrentTime)
{
	LastSentTime = CurrentTime;
	++SentNumber;
}

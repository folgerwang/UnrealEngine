// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Net/NetPacketNotify.h"

void FNetPacketNotify::Init(SequenceNumberT InitialInSeq, SequenceNumberT InitialOutSeq)
{
	InSeqHistory.Reset();
	InSeq = InitialInSeq;
	InAckSeq = InitialInSeq;
	OutSeq = InitialOutSeq;
	OutAckSeq = SequenceNumberT(InitialOutSeq.Get() - 1);
}

void FNetPacketNotify::GetHeader(FNotificationHeader& DataOut) const
{
	DataOut.Seq = OutSeq;
	DataOut.AckedSeq = InAckSeq;
	DataOut.History = InSeqHistory;
}

void FNetPacketNotify::AckSeq(SequenceNumberT AckedSeq, bool IsAck)
{
	check( AckedSeq == InSeq);

	while (AckedSeq > InAckSeq)
	{
		++InAckSeq;

		const bool bReportAcked = InAckSeq == AckedSeq ? IsAck : false;

		UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::AckSeq - AckedSeq: %u, IsAck %u"), InAckSeq.Get(), bReportAcked ? 1u : 0u);

		InSeqHistory.AddDeliveryStatus(bReportAcked);		
	}
}

// These methods must always write and read the exact same number of bits, that is the reason for not using WriteInt/WrittedWrappedInt
void FNetPacketNotify::WriteHeader(FBitWriter& Writer, const FNotificationHeader& Data)
{
	SequenceNumberT::SequenceT Seq = Data.Seq.Get();
	SequenceNumberT::SequenceT AckedSeq = Data.AckedSeq.Get();
	
	Writer << Seq;
	Writer << AckedSeq;
	Data.History.Write(Writer);
}

bool FNetPacketNotify::ReadHeader(FNotificationHeader& Data, FBitReader& Reader)
{
	SequenceNumberT::SequenceT RcvdSeq;
	SequenceNumberT::SequenceT RcvdAckedSeq;

	Reader << RcvdSeq;
	Reader << RcvdAckedSeq;
	Data.History.Read(Reader);

	Data.Seq = SequenceNumberT(RcvdSeq);
	Data.AckedSeq = SequenceNumberT(RcvdAckedSeq);

	return Reader.IsError() == false;
}


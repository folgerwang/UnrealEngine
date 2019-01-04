// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/UnrealMemory.h"
#include "Templates/IsSigned.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_PACKET_NOTIFY_LOG 0
#else
#	define UE_NET_ENABLE_PACKET_NOTIFY_LOG 1
#endif 

#if UE_NET_ENABLE_PACKET_NOTIFY_LOG
#	include "Logging/LogMacros.h"
#	include "EngineLogs.h"
#	define UE_LOG_PACKET_NOTIFY(Format, ...)  UE_LOG(LogNetTraffic, Verbose, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_PACKET_NOTIFY(...)
#endif

struct FBitWriter;
struct FBitReader;

/** Util class to manage history of received sequence numbers */
template <size_t HistorySize>
class TSequenceHistory
{
	typedef uint32 WordT;
	
	static const size_t BitsPerWord = sizeof(WordT) * 8;
	static const size_t WordCount = HistorySize / BitsPerWord;
	
	static_assert(HistorySize > 0, "Invalid History");
	static_assert(HistorySize % BitsPerWord == 0, "HistorySize must be a modulo of the wordsize");

public:	
	TSequenceHistory();

#if WITH_DEV_AUTOMATION_TESTS
	explicit TSequenceHistory(WordT Value);
#endif
	/** Reset */
	void Reset();

	/** Store delivery status, oldest will be dropped */
	void AddDeliveryStatus(bool Delivered);

	/** Query the status of a specific index, index 0 is last stored status */
	bool IsDelivered(size_t Index) const;

	/** Return the size of the history buffer */
	size_t Size() const { return HistorySize; }	
	
	bool operator==(const TSequenceHistory& Other) const
	{
		return FMemory::Memcmp(Data(), Other.Data(), WordCount * sizeof (WordT)) == 0;
	}

	bool operator!=(const TSequenceHistory& Other) const
	{
		return FMemory::Memcmp(Data(), Other.Data(), WordCount * sizeof (WordT)) != 0;
	}

	/** Write history to BitStream */
	void Write(FBitWriter& Writer) const;

	/** Read history from BitStream */
	void Read(FBitReader& Reader);

private:
	const WordT* Data() const { return &Storage[0]; }
	size_t NumWords() const { return WordCount; }

	WordT Storage[WordCount];
};

/** Helper class to work with sequence numbers */
template <size_t NumBits, typename SequenceType>
class TSequenceNumber
{
	static_assert(TIsSigned<SequenceType>::Value == false, "The base type for sequence numbers must be unsigned");

public:
	typedef SequenceType SequenceT;
	typedef int32 DifferenceT;

	// Constants
	enum { SeqNumberBits = NumBits };
	enum { SeqNumberCount = SequenceT(1) << NumBits };
	enum { SeqNumberHalf = SequenceT(1) << (NumBits - 1) };
	enum { SeqNumberMax = SeqNumberCount - 1u };
	enum { SeqNumberMask = SeqNumberMax };

	/** Default constructor */
	TSequenceNumber() : Value(0u) {}

	/** Constructor with given value */
	TSequenceNumber(SequenceT ValueIn) : Value(ValueIn & SeqNumberMask) {}
	
	/** Get Current Value */	
	SequenceT Get() const { return Value; }

	/** Diff between sequence numbers (A - B) only valid if (A - B) < SeqNumberHalf */
	static DifferenceT Diff(TSequenceNumber A, TSequenceNumber B);
	
	/** return true if this is > Other, this is only considered to be the case if (A - B) < SeqNumberHalf since we have to be able to detect wraparounds */
	bool operator>(const TSequenceNumber& Other) const { return (Value != Other.Value) && (((Value - Other.Value) & SeqNumberMask) < SeqNumberHalf); }

	/** Check if this is >= Other, See above */
	bool operator>=(const TSequenceNumber& Other) const { return ((Value - Other.Value) & SeqNumberMask) < SeqNumberHalf; }

	/** Equals, NOTE that serial numbers wrap around so 0 == 0 + SequenceNumberCount */
	bool operator==(const TSequenceNumber& Other) const { return Value == Other.Value; }

	bool operator!=(const TSequenceNumber& Other) const { return Value != Other.Value; }

	/** Pre-increment and wrap around */
	TSequenceNumber& operator++() { Increment(1u); return *this; }
	
	/** Post-increment and wrap around */
	TSequenceNumber operator++(int) { TSequenceNumber Tmp(*this); Increment(1u); return Tmp; }

private:
	void Increment(SequenceT InValue) { *this = TSequenceNumber(Value + InValue); }
	SequenceT Value;
};

/** 
	FNetPacketNotify - Drives delivery of sequence numbers, acknowledgments and notifications of delivery sequence numbers
*/
class FNetPacketNotify
{
public:
	enum { SequenceNumberBits = 14 };
	enum { SequenceHistoryLength = 32 };

	typedef TSequenceNumber<SequenceNumberBits, uint16> SequenceNumberT;
	typedef TSequenceHistory<SequenceHistoryLength> SequenceHistoryT;

	struct FNotificationHeader
	{
		SequenceHistoryT History;
		SequenceNumberT Seq;
		SequenceNumberT AckedSeq;
	};

	/** Init notification with expected initial sequence numbers */
	void Init(SequenceNumberT InitialInSeq, SequenceNumberT InitialOutSeq);

	/** Mark Seq as received and update current InSeq, missing sequence numbers will be marked as lost */
	void AckSeq(SequenceNumberT Seq) { AckSeq(Seq, true); }

	/** Explicitly mark Seq as not received and update current InSeq, additional missing sequence numbers will be marked as lost */
	void NakSeq(SequenceNumberT Seq) { AckSeq(Seq, false); }

	/** Increment outgoing seq number */
	SequenceNumberT IncrementOutSeq() { return ++OutSeq; }	

	/** Get header that should be written to stream */
	void GetHeader(FNotificationHeader& DataOut) const;

	/** Write header to stream, note this must always write the exact same number of bits as we go back and update the header */
	static void WriteHeader(FBitWriter& Writer, const FNotificationHeader& Data);

	/** Read header from stream */
	static bool ReadHeader(FNotificationHeader& Data, FBitReader& Reader);

	/** Update state of PacketNotification based on received header and invoke packet notifications for received acks.
		InFunc is a function in the format void)(FNetPacketNotify::SequenceNumberT AckedSequence, bool delivered) to handle packetNotifications.

		Returns the positive delta of the incoming seq number if it is within half the sequence number space
		Returns 0 if the received sequence number is out of the current window or if the acknowledged seq received by the remote is invalid.
	*/
	template<class Functor>
	SequenceNumberT::DifferenceT Update(const FNotificationHeader& NotificationData, Functor&& InFunc)
	{		
		if (NotificationData.Seq > InSeq && NotificationData.AckedSeq >= OutAckSeq)
		{
			UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::Update - Seq %u, InSeq %u"), NotificationData.Seq.Get(), InSeq.Get());

			const SequenceNumberT::DifferenceT InSeqDelta = SequenceNumberT::Diff(NotificationData.Seq, InSeq);
			
			ProcessReceivedAcks(NotificationData, InFunc);

			// accept sequence
			InSeq = NotificationData.Seq;

			return InSeqDelta;
		}
		else
		{
			return 0;
		}
	}

	/** Get the current SequenceHistory */
	const SequenceHistoryT& GetInSeqHistory() const { return InSeqHistory; }

	/** Get the last received in sequence number */
	SequenceNumberT GetInSeq() const { return InSeq; }

	/** Get the last received sequence number that we have accepted, InAckSeq cannot be larger than InSeq */
	SequenceNumberT GetInAckSeq() const { return InAckSeq; }

	/** Get the current outgoing sequence number */
	SequenceNumberT GetOutSeq() const { return OutSeq; }

	/** Get the last outgoing sequence number acknowledged by remote */
	SequenceNumberT GetOutAckSeq() const { return OutAckSeq; }

	/** If we do have more unacknowledged sequence numbers in-flight than our maximum sendwindow we should not send more as the receiving end will not be able to detect if the sequence number has wrapped around */
	bool CanSend() const { SequenceNumberT NextOutSeq = OutSeq; ++NextOutSeq; return NextOutSeq >= OutAckSeq; }

private:
	SequenceHistoryT InSeqHistory;	// BitBuffer containing a bitfield describing the history of received packets
	SequenceNumberT InSeq;			// Last sequence number received from remote
	SequenceNumberT InAckSeq;		// Last sequence number received from remote that we have acknowledged, this is needed since we support accepting a packet but explicitly not acknowledge it as received.
	SequenceNumberT OutSeq;			// Outgoing sequence number
	SequenceNumberT OutAckSeq;		// Last sequence number that we know that the remote side have received.

	template<class Functor>
	void ProcessReceivedAcks(const FNotificationHeader& NotificationData, Functor&& InFunc)
	{
		if (NotificationData.AckedSeq > OutAckSeq)
		{
			UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks - AckedSeq: %u, OutAckSeq: %u"), NotificationData.AckedSeq.Get(), OutAckSeq.Get());

			SequenceNumberT::DifferenceT AckCount = SequenceNumberT::Diff(NotificationData.AckedSeq, OutAckSeq);

			// ExpectedAck = OutAckSeq + 1
			SequenceNumberT CurrentAck(OutAckSeq);
			++CurrentAck;

			// Everything not found in the history buffer is treated as lost
			while (AckCount > (SequenceNumberT::DifferenceT)(NotificationData.History.Size()))
			{
				--AckCount;
				UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks Seq: %u - IsAck: 0 HistoryIndex: N/A"), CurrentAck.Get());
				InFunc(CurrentAck, false);
				++CurrentAck;
			}

			// For sequence numbers contained in the history we lookup the delivery status
			while (AckCount > 0)
			{
				--AckCount;
				UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks Seq: %u - IsAck: %u HistoryIndex: %u"), CurrentAck.Get(), NotificationData.History.IsDelivered(AckCount) ? 1u : 0u, AckCount);
				InFunc(CurrentAck, NotificationData.History.IsDelivered(AckCount));
				++CurrentAck;
			}
			OutAckSeq = NotificationData.AckedSeq;	
		}
	}

	void AckSeq(SequenceNumberT AckedSeq, bool IsAck);

#if WITH_DEV_AUTOMATION_TESTS
	friend struct FNetPacketNotifyTestUtil;
#endif
};


//////////////////////////////////////////////////////////////////////////
// Implementation of TSequenceHistory
//////////////////////////////////////////////////////////////////////////
template <size_t HistorySize>
TSequenceHistory<HistorySize>::TSequenceHistory()
{
	Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
template <size_t HistorySize>
TSequenceHistory<HistorySize>::TSequenceHistory(WordT Value)
{
	for (size_t CurrentWordIt = 0; CurrentWordIt < WordCount; ++CurrentWordIt)
	{
		Storage[CurrentWordIt] = Value;
	}	
}
#endif 

template <size_t HistorySize>
void TSequenceHistory<HistorySize>::Reset()
{
	memset(&Storage[0], 0, WordCount * sizeof(WordT));
}

template <size_t HistorySize>
void TSequenceHistory<HistorySize>::AddDeliveryStatus(bool Delivered)
{
	WordT Carry = Delivered ? 1u : 0u;
	const WordT ValueMask = 1u << (BitsPerWord - 1);
	
	for (size_t CurrentWordIt = 0; CurrentWordIt < WordCount; ++CurrentWordIt)
	{
		const WordT OldValue = Carry;
		
		// carry over highest bit in each word to the next word
		Carry = (Storage[CurrentWordIt] & ValueMask) >> (BitsPerWord - 1);
		Storage[CurrentWordIt] = (Storage[CurrentWordIt] << 1u) | OldValue;
	}
}

template <size_t HistorySize>
bool TSequenceHistory<HistorySize>::IsDelivered(size_t Index) const
{
	check(Index < Size());

	const size_t WordIndex = Index / BitsPerWord;
	const WordT WordMask = (WordT(1) << (Index & (BitsPerWord - 1)));
	
	return (Storage[WordIndex] & WordMask) != 0u;
}

template <size_t HistorySize>
void TSequenceHistory<HistorySize>::Write(FBitWriter& Writer) const
{
	for (size_t CurrentWordIt = 0; CurrentWordIt < WordCount; ++CurrentWordIt)
	{
		WordT temp = Storage[CurrentWordIt];
		Writer << temp;
	}
}

template <size_t HistorySize>
void TSequenceHistory<HistorySize>::Read(FBitReader& Reader)
{
	for (size_t CurrentWordIt = 0; CurrentWordIt < WordCount; ++CurrentWordIt)
	{
		Reader << Storage[CurrentWordIt];
	}
}

//////////////////////////////////////////////////////////////////////////
// Implementation of TSequenceNumber
//////////////////////////////////////////////////////////////////////////
template <size_t NumBits, typename SequenceType>
typename TSequenceNumber<NumBits, SequenceType>::DifferenceT TSequenceNumber<NumBits, SequenceType>::Diff(TSequenceNumber A, TSequenceNumber B) 
{ 
	const size_t ShiftValue = sizeof(DifferenceT)*8 - NumBits;

	const SequenceT ValueA = A.Value;
	const SequenceT ValueB = B.Value;

	return (DifferenceT)((ValueA - ValueB) << ShiftValue) >> ShiftValue;
};

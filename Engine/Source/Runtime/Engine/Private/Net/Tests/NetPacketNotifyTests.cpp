#include "Misc/AutomationTest.h"
#include "Net/NetPacketNotify.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNetPacketNotifyTest, "Network.PacketNotifyTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

struct FNetPacketNotifyTestUtil
{
	FNetPacketNotify DefaultNotify;

	FNetPacketNotifyTestUtil()
	{
		DefaultNotify.Init(FNetPacketNotify::SequenceNumberT(-1),  FNetPacketNotify::SequenceNumberT(0));
	}

	// Pretend to receive incoming packet to generate ackdata
	static int32 PretendReceiveSeq(FNetPacketNotify& PacketNotify, FNetPacketNotify::SequenceNumberT Seq, bool Ack = true)
	{
		FNetPacketNotify::FNotificationHeader Data;
		Data.Seq = Seq;
		Data.AckedSeq = PacketNotify.GetOutAckSeq();
		Data.History = FNetPacketNotify::SequenceHistoryT(0);
		
		FNetPacketNotify::SequenceNumberT::DifferenceT SeqDelta = PacketNotify.Update(Data, [](FNetPacketNotify::SequenceNumberT AckedSequence, bool delivered) {});
		if (SeqDelta > 0)
		{
			if (Ack)
				PacketNotify.AckSeq(Seq);
		}

		return SeqDelta;
	}

	static bool VerifyNotificaitonState(const FNetPacketNotify& A, const FNetPacketNotify& B)
	{
		bool bEquals = 
			A.GetInSeq() == B.GetInSeq() &&
			A.GetInSeqHistory() == B.GetInSeqHistory() &&
			A.GetOutSeq() == B.GetOutSeq() &&
			A.GetOutAckSeq() == B.GetOutAckSeq();

		return bEquals;
	}

	// pretend to ack array of sequence numbers
	template<typename T>
	static void PretendAckSequenceNumbers(FNetPacketNotify& PacketNotify, const T& InSequenceNumbers)
	{
		size_t SequenceNumberCount = sizeof(InSequenceNumbers) / sizeof(InSequenceNumbers[0]);

		for (int32 I=0; I<SequenceNumberCount; ++I)
		{
			FNetPacketNotifyTestUtil::PretendReceiveSeq(PacketNotify, InSequenceNumbers[I]);
		}
	}
	
	// pretend to deliver notifications for incoming seq header
	template<typename T>
	static size_t PretendDeliverNotifications(FNetPacketNotify& PacketNotify, const FNetPacketNotify::FNotificationHeader Data, T& OutSequenceNumbers)
	{
		size_t NotificationCount = 0;

		auto HandleAck = [&OutSequenceNumbers, &NotificationCount](FNetPacketNotify::SequenceNumberT Seq, bool delivered)
		{
			const size_t MaxSequenceNumberCount = sizeof(OutSequenceNumbers) / sizeof(OutSequenceNumbers[0]);

			if (delivered)
			{
				if (NotificationCount < MaxSequenceNumberCount)
				{
					OutSequenceNumbers[NotificationCount] = Seq;
				}
				++NotificationCount;		
			}
		};
		return PacketNotify.Update(Data, HandleAck);
	}

	template<typename T>
	static bool TestNotificationSequence(const T& InSequenceNumbers, FNetPacketNotify::SequenceNumberT FirstSequence = 0)
	{
		T NotifiedSequenceNumbers = { 0 };
		//FPlatformMemory::Memset(&NotifiedSequenceNumbers[0], 0, sizeof(NotifiedSequenceNumbers));

		FNetPacketNotify Acked;
		Acked.Init(FNetPacketNotify::SequenceNumberT(FirstSequence.Get() - 1), FirstSequence);
		PretendAckSequenceNumbers(Acked, InSequenceNumbers);

		// In order to be able to accepts the acks we must pretend that we have sent a packet which we will get an ack for.
		FNetPacketNotify Notified;
		Notified.Init(FNetPacketNotify::SequenceNumberT(FirstSequence.Get() - 1), FirstSequence);
		FNetPacketNotify::FNotificationHeader Data;
		Acked.GetHeader(Data);

		PretendDeliverNotifications(Notified, Data, NotifiedSequenceNumbers);

		return FPlatformMemory::Memcmp(&InSequenceNumbers[0], &NotifiedSequenceNumbers[0], sizeof(InSequenceNumbers)) == 0;
	}
};


bool FNetPacketNotifyTest::RunTest(const FString& Parameters)
{
	FNetPacketNotifyTestUtil Util;
	// Test fill
	{
		FNetPacketNotify::SequenceNumberT ExpectedInSeq(31);
		FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0xffffffffu);

		FNetPacketNotify Acks = Util.DefaultNotify;
				
		for (int32 I=0; I<32; ++I)
		{
			FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, I, true);
		}

		TestEqual(TEXT("Test fill - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test fill - History"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	// Test drop every other
	{
		FNetPacketNotify::SequenceNumberT ExpectedInSeq(30);
		FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x55555555u);

		FNetPacketNotify Acks = Util.DefaultNotify;
				
		for (int32 I=0; I<16; ++I)
		{
			FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, I*2);
		}
		
		TestEqual(TEXT("Test drop every other - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test drop every other - History"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	// Test burst drop
	{
		FNetPacketNotify::SequenceNumberT ExpectedInSeq(128);
		FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x1u);

		FNetPacketNotify Acks = Util.DefaultNotify;

		// Drop early
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, 3);

		// Large gap until next seq
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, 128);

		TestEqual(TEXT("Test burst drop - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test burst drop - History"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	// Test window overflow
	{
		FNetPacketNotify Acks = Util.DefaultNotify;

		const FNetPacketNotify::SequenceNumberT ExpectedInSeq(0);
		const FNetPacketNotify::SequenceNumberT MaxWindowSeq(FNetPacketNotify::SequenceNumberT::SeqNumberHalf);

		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, ExpectedInSeq, true);
		TestEqual(TEXT("Test window overflow - Expect InSeq 0"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test window overflow - Expect InAckSeq 0"), Acks.GetInAckSeq(), ExpectedInSeq);

		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, MaxWindowSeq, true);
		TestEqual(TEXT("Test window overflow - Expect Seq reject"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Test window overflow - Expect Seq reject"), Acks.GetInAckSeq(), ExpectedInSeq);

		const FNetPacketNotify::SequenceNumberT NextExpectedInSeq(1);
		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, NextExpectedInSeq, true);
		TestEqual(TEXT("Test window overflow - Expect InSeq 1"), Acks.GetInSeq(), NextExpectedInSeq);
		TestEqual(TEXT("Test window overflow - Expect InAckSeq 1"), Acks.GetInAckSeq(), NextExpectedInSeq);

		FNetPacketNotifyTestUtil::PretendReceiveSeq(Acks, MaxWindowSeq, true);
		TestEqual(TEXT("Test window overflow - Expect InSeq MaxWindowSeq"), Acks.GetInSeq(), MaxWindowSeq);
		TestEqual(TEXT("Test window overflow - Expect InSeq MaxWindowSeq"), Acks.GetInAckSeq(), MaxWindowSeq);
	}

	// create history
	{
		const FNetPacketNotify::SequenceNumberT ExpectedInSeq(18);
		const FNetPacketNotify::SequenceHistoryT ExpectedInSeqHistory(0x8853u);

		const FNetPacketNotify::SequenceNumberT AckdPacketIds[] = {3, 7, 12, 14, 17, 18};
		const size_t ExpectedCount = sizeof(AckdPacketIds)/sizeof((AckdPacketIds)[0]);		

		FNetPacketNotify Acks = Util.DefaultNotify;
		FNetPacketNotifyTestUtil::PretendAckSequenceNumbers(Acks, AckdPacketIds);
 
		TestEqual(TEXT("Create history - InSeq"), Acks.GetInSeq(), ExpectedInSeq);
		TestEqual(TEXT("Create history - InSeqHistory"), Acks.GetInSeqHistory(), ExpectedInSeqHistory);
	}

	
	// test notifications
	{
		static const FNetPacketNotify::SequenceNumberT ExpectedAckdPacketIds[] = {3, 7, 12, 14, 17, 18};
		static const size_t ExpectedCount = sizeof(ExpectedAckdPacketIds)/sizeof((ExpectedAckdPacketIds)[0]);		

		FNetPacketNotify::SequenceNumberT RcvdAcks[ExpectedCount] = { 0 };
		size_t RcvdCount = 0;

		// Create src data
		FNetPacketNotify Acks = Util.DefaultNotify;

		// Fill in some data
		FNetPacketNotify::FNotificationHeader Data;
		Data.Seq = FNetPacketNotify::SequenceNumberT(0);
		Data.AckedSeq = FNetPacketNotify::SequenceNumberT(18);
		Data.History = FNetPacketNotify::SequenceHistoryT(0x8853u);

		size_t DeltaSeq = FNetPacketNotifyTestUtil::PretendDeliverNotifications(Acks, Data, RcvdAcks);

		TestEqual(TEXT("Notifications - Create sequence delta"), DeltaSeq, 1);
		TestEqual(TEXT("Notifications - Create sequence"), FPlatformMemory::Memcmp(ExpectedAckdPacketIds, RcvdAcks, sizeof(ExpectedAckdPacketIds)), 0u);
	}

	// test various sequence
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {3, 7, 12, 14, 17, 18};	
		TestTrue(TEXT("Test Seq {3, 7, 12, 14, 17, 18}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};	
		TestTrue(TEXT("Test Seq {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};	
		TestTrue(TEXT("{2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, 31};	
		TestTrue(TEXT("Test Seq {0, 31}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, 32};	
		TestFalse(TEXT("Test Seq {0, 32}"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, FNetPacketNotify::SequenceNumberT::SeqNumberHalf };
		TestFalse(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {0, FNetPacketNotify::SequenceNumberT::SeqNumberHalf - 1};
		TestFalse(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};
		TestFalse(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0} From 0;"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs));
	}
	{
		static const FNetPacketNotify::SequenceNumberT TestSeqs[] = {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0};
		TestTrue(TEXT("Test Seq {FNetPacketNotify::SequenceNumberT::SeqNumberMax, 0} From SeqNumberHalf + 1;"), FNetPacketNotifyTestUtil::TestNotificationSequence(TestSeqs, FNetPacketNotify::SequenceNumberT::SeqNumberHalf + 2));
	}

	// Test sequence numbers
	{
		// Valid sequence = 0-7, max distance between sequence number in order to determine order is half the sequence space (0-3)
		typedef TSequenceNumber<3, uint16> FSequence3;

		for (int32 I = 0; I < FSequence3::SeqNumberCount; ++I)
		{
			FSequence3 Seq(I);
			FSequence3 Ref(I);
			
			for (int32 U = 0; U < FSequence3::SeqNumberCount; ++U)
			{	
				FSequence3::DifferenceT Diff = FSequence3::Diff(Seq, Ref);

				if (U < FSequence3::SeqNumberHalf)
				{
					if (!(Diff == U))
					{
						TestTrue(TEXT("SequenceNumbers - Expected Diff "), Diff == U);
					}
				}
				else
				{
					if (!(Diff == (U - FSequence3::SeqNumberCount)))
					{
						TestTrue(TEXT("SequenceNumbers - Expected Diff"), Diff == (U - FSequence3::SeqNumberCount));
					}
				}
				++Seq;
			}
		}

		check(!HasAnyErrors());

	}

	return true;
}

#endif // #if WITH_DEV_AUTOMATION_TESTS
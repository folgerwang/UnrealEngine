// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Net/Util/ResizableCircularQueue.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResizableCircularQueueTest, "Network.ResizableCircularQueueTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

struct FResizableCircularQueueTestUtil
{
	typedef TResizableCircularQueue<uint32> QueueT;

	static bool VerifyQueueIntegrity(const QueueT& Queue, uint32 ExpectedValueAtFront, uint32 Increment)
	{
		bool bSuccess = true;
		SIZE_T Offset = Queue.Count() - 1;
	
		uint32 ExpectedValue = ExpectedValueAtFront;

		// Peek elements in queue at given offset, peek from back to front
		for (SIZE_T It = 0; It < Queue.Count(); ++It)
		{
			bSuccess = bSuccess && (ExpectedValue == Queue.PeekAtOffset(It));
			ExpectedValue += Increment;
		}

		return bSuccess;
	}

	static void OverideHeadAndTail(QueueT& Queue, uint32 Head, uint32 Tail)
	{
		Queue.Head = Head;
		Queue.Tail = Tail;
	}
};

bool FResizableCircularQueueTest::RunTest(const FString& Parameters)
{
	// Test empty
	{
		FResizableCircularQueueTestUtil::QueueT Q(0);

		TestEqual(TEXT("Test empty - Size"), Q.Count(), 0);
		TestTrue(TEXT("Test empty - IsEmpty"), Q.IsEmpty());
		TestEqual(TEXT("Test empty - Capacity"), Q.AllocatedCapacity(), 0);
	}

	// Test Push to Capacity
	{
		const SIZE_T ElementsToPush = 8;

		FResizableCircularQueueTestUtil::QueueT Q(ElementsToPush);

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Enqueue(It);
		}
		
		TestEqual(TEXT("Test Push to Capacity - Size"), Q.Count(), ElementsToPush);
		TestEqual(TEXT("Test Push to Capacity - Capacity"), Q.AllocatedCapacity(), ElementsToPush);
 		TestTrue(TEXT("Test Push to Capacity - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, 0, 1));
	}

	// Test Push over Capacity
	{
		const SIZE_T ElementsToPush = 32;

		FResizableCircularQueueTestUtil::QueueT Q(ElementsToPush);

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Enqueue(It);
		}
		
		TestEqual(TEXT("Test Push over Capacity - Size"), Q.Count(), ElementsToPush);
		TestEqual(TEXT("Test Push over Capacity - Capacity"), Q.AllocatedCapacity(), ElementsToPush);
 		TestTrue(TEXT("Test Push over Capacity - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, 0, 1));
	}

	// Test Push and Pop
	{
		const SIZE_T ElementsToPush = 256;
		const SIZE_T ElementPopMod = 16;
		const SIZE_T ExpectedSize = 256 - ElementPopMod;
		const SIZE_T ExpectedCapacity = 256;

		FResizableCircularQueueTestUtil::QueueT Q(4);

		uint32 ExpectedPoppedValue = 0;
		for (uint32 It = 0; It < 256; ++It)
		{
			Q.Enqueue(It);
			TestEqual(TEXT("Test Push and pop - Push"), It, Q.PeekAtOffset(Q.Count() - 1));

			if (It % ElementPopMod == 0)
			{
				const uint32 PoppedValue = Q.PeekAtOffset(0);
				TestEqual(TEXT("Test Push and pop - Pop"), ExpectedPoppedValue, PoppedValue);
				++ExpectedPoppedValue;
				Q.Pop();
			}
		}

		TestEqual(TEXT("Test Push and pop - Size"), Q.Count(), ExpectedSize);
		TestEqual(TEXT("Test Push and pop - Capacity"), Q.AllocatedCapacity(), ExpectedCapacity);
 		TestTrue (TEXT("Test Push and pop - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, ElementPopMod, 1));	
	}

	// Test Push and pop all
	{
		const SIZE_T ElementsToPush = 256;

		FResizableCircularQueueTestUtil::QueueT Q(ElementsToPush);

		TestTrue(TEXT ("Test Push and pop all - IsEmpty before"), Q.IsEmpty());
		TestEqual(TEXT("Test Push and pop all - Size before"), Q.Count(), 0);

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Enqueue(It);
		}
		
		TestEqual(TEXT("Test Push and pop all - Size"), Q.Count(), ElementsToPush);
		TestEqual(TEXT("Test Push and pop all - Capacity"), Q.AllocatedCapacity(), ElementsToPush);
 		TestTrue (TEXT("Test Push and pop all - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, 0, 1));

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Pop();
		}
		
		TestTrue( TEXT("Test Push and pop all - IsEmpty after"), Q.IsEmpty());
		TestEqual(TEXT("Test Push and pop all - Size after"), Q.Count(), 0);
		TestEqual(TEXT("Test Push and pop all - Capacity after"), Q.AllocatedCapacity(), ElementsToPush);
	}

	// Test index wrap
	{
		const SIZE_T ElementsToPush = 256;
		const SIZE_T ElementPopMod = 16;
		const SIZE_T ExpectedSize = 256 - ElementPopMod;
		const SIZE_T ExpectedCapacity = 256;

		FResizableCircularQueueTestUtil::QueueT Q(4);

		// Set head and tail at the end of the space to test index wraparound
		FResizableCircularQueueTestUtil::OverideHeadAndTail(Q, uint32(-2), uint32(-2));

		TestTrue(TEXT ("Test index wrap - IsEmpty before"), Q.IsEmpty());
		TestEqual(TEXT("Test index wrap - Size before"), Q.Count(), 0);

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Enqueue(It);
		}
		
		TestEqual(TEXT("Test index wrap - Size"), Q.Count(), ElementsToPush);
		TestEqual(TEXT("Test index wrap - Capacity"), Q.AllocatedCapacity(), ElementsToPush);
 		TestTrue (TEXT("Test index wrap - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, 0, 1));

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Pop();
		}
		
		TestTrue( TEXT("Test index wrap - IsEmpty after"), Q.IsEmpty());
		TestEqual(TEXT("Test index wrap - Size after"), Q.Count(), 0);
		TestEqual(TEXT("Test index wrap - Capacity after"), Q.AllocatedCapacity(), ElementsToPush);
	}

	// Test Trim
	{
		const SIZE_T ElementsToPush = 9;
		const SIZE_T ElementsToPop = 5;
		const SIZE_T ExpectedCapacity = 16;
		const SIZE_T ExpectedCapacityAfterTrim = 4;

		FResizableCircularQueueTestUtil::QueueT Q(0);

		for (SIZE_T It=0; It < ElementsToPush; ++It)
		{
			Q.Enqueue(It);
		}
		
		TestEqual(TEXT("Test Trim - Size"), Q.Count(), ElementsToPush);
		TestEqual(TEXT("Test Trim - Capacity"), Q.AllocatedCapacity(), ExpectedCapacity);
 		TestTrue(TEXT("Test Trim - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, 0, 1));

		for (SIZE_T It=0; It < ElementsToPop; ++It)
		{
			Q.Pop();
		}

		Q.Trim();

		TestEqual(TEXT("Test Trim - Size"), Q.Count(), ElementsToPush - ElementsToPop);
		TestEqual(TEXT("Test Trim - Capacity"), Q.AllocatedCapacity(), ExpectedCapacityAfterTrim);
 		TestTrue(TEXT("Test Trim - Expected"), FResizableCircularQueueTestUtil::VerifyQueueIntegrity(Q, ElementsToPop, 1));
	}

	// Test Trim empty
	{
		FResizableCircularQueueTestUtil::QueueT Q(0);

		Q.Trim();

		TestEqual(TEXT("Test trim empty - Size"), Q.Count(), 0);
		TestEqual(TEXT("Test trim empty - Capacity"), Q.AllocatedCapacity(), 0);
	}


	check(!HasAnyErrors());

	return true;
}

#endif // #if WITH_DEV_AUTOMATION_TESTS
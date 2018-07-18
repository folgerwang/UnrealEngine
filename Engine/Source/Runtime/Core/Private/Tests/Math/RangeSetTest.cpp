// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"

#include "Math/Range.h"
#include "Math/RangeSet.h"
#include "Misc/Timespan.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeSetTest, "System.Core.Math.RangeSet", EAutomationTestFlags::Disabled | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FRangeSetTest::RunTest(const FString& Parameters)
{
	// @todo: Write an actual unit test (the following just checks compilation of certain use cases)

	{
		TRange<int32> Range = TRange<int32>::AtLeast(0);
		TRangeSet<int32> RangeSet;

		RangeSet.Add(Range);
		int32 Value = RangeSet.GetMinBoundValue();
	}

	{
		TRange<FTimespan> Range = TRange<FTimespan>::AtLeast(FTimespan::Zero());
		TRangeSet<FTimespan> RangeSet;

		RangeSet.Add(Range);
		FTimespan Value = RangeSet.GetMinBoundValue();
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS

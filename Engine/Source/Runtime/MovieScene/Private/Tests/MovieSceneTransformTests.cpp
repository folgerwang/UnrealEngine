// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieSceneSectionParameters.h"
#include "Containers/ArrayView.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneTestsCommon.h"

#define LOCTEXT_NAMESPACE "MovieSceneTransformTests"

bool IsEqual(TRangeBound<FFrameNumber> A, TRangeBound<FFrameNumber> B)
{
	if (A.IsOpen() || B.IsOpen())
	{
		return A.IsOpen() == B.IsOpen();
	}
	else if (A.IsInclusive() != B.IsInclusive())
	{
		return false;
	}
	
	return A.GetValue() == B.GetValue();
}

bool IsEqual(TRange<FFrameNumber> A, TRange<FFrameNumber> B)
{
	return IsEqual(A.GetLowerBound(), B.GetLowerBound()) && IsEqual(A.GetUpperBound(), B.GetUpperBound());
}

FString LexToString(const TRange<FFrameNumber>& InRange)
{
	TRangeBound<FFrameNumber> SourceLower = InRange.GetLowerBound();
	TRangeBound<FFrameNumber> SourceUpper = InRange.GetUpperBound();

	return *FString::Printf(TEXT("%s-%s"),
		SourceLower.IsOpen() ? 
			TEXT("[...") : 
			SourceLower.IsInclusive() ?
				*FString::Printf(TEXT("[%i"), SourceLower.GetValue().Value) :
				*FString::Printf(TEXT("(%i"), SourceLower.GetValue().Value),

		SourceUpper.IsOpen() ? 
			TEXT("...]") : 
			SourceUpper.IsInclusive() ?
				*FString::Printf(TEXT("%i]"), SourceUpper.GetValue().Value) :
				*FString::Printf(TEXT("%i)"), SourceUpper.GetValue().Value)
		);
}

bool TestTransform(FAutomationTestBase& Test, FMovieSceneSequenceTransform Transform, TArrayView<TRange<FFrameNumber>> InSource, TArrayView<TRange<FFrameNumber>> InExpected, const TCHAR* TestName)
{
	check(InSource.Num() == InExpected.Num());

	bool bSuccess = true;
	for (int32 Index = 0; Index < InSource.Num(); ++Index)
	{
		TRange<FFrameNumber> Result = InSource[Index] * Transform;
		if (!IsEqual(Result, InExpected[Index]))
		{
			Test.AddError(FString::Printf(TEXT("Test '%s' failed (Index %d). Transform (Scale %.3f, Offset %i+%.3f) did not apply correctly (%s != %s)"),
				TestName,
				Index,
				Transform.TimeScale,
				Transform.Offset.FrameNumber.Value,
				Transform.Offset.GetSubFrame(),
				*LexToString(Result),
				*LexToString(InExpected[Index])));

			bSuccess = false;
		}
	}

	return bSuccess;
}

// Calculate the transform that transforms from range A to range B
FMovieSceneSequenceTransform TransformRange(FFrameNumber StartA, FFrameNumber EndA, FFrameNumber StartB, FFrameNumber EndB)
{
	float Scale = double( (EndB - StartB).Value ) / (EndA - StartA).Value;
	return FMovieSceneSequenceTransform(StartB, Scale) * FMovieSceneSequenceTransform(-StartA);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneSubSectionCoreTransformsTest, "System.Engine.Sequencer.Core.Transforms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneSubSectionCoreTransformsTest::RunTest(const FString& Parameters)
{
	// We test using ranges since that implicitly tests frame number transformation as well
	static const TRangeBound<FFrameNumber> OpenBound;

	TRange<FFrameNumber> InfiniteRange(OpenBound, OpenBound);
	TRange<FFrameNumber> OpenLowerRange(OpenBound, FFrameNumber(200));
	TRange<FFrameNumber> OpenUpperRange(FFrameNumber(100), OpenBound);
	TRange<FFrameNumber> ClosedRange(FFrameNumber(100), FFrameNumber(200));

	TRange<FFrameNumber> SourceRanges[] = {
		InfiniteRange, OpenLowerRange, OpenUpperRange, ClosedRange
	};

	bool bSuccess = true;

	{
		// Test Multiplication with an identity transform
		FMovieSceneSequenceTransform IdentityTransform;

		TRange<FFrameNumber> Expected[] = {
			InfiniteRange, OpenLowerRange, OpenUpperRange, ClosedRange
		};
		
		bSuccess = TestTransform(*this, IdentityTransform, SourceRanges, Expected, TEXT("IdentityTransform")) && bSuccess;
	}

	{
		// Test a simple translation
		FMovieSceneSequenceTransform Transform(100, 1);

		TRange<FFrameNumber> Expected[] = {
			InfiniteRange, TRange<FFrameNumber>(OpenBound, FFrameNumber(300)), TRange<FFrameNumber>(FFrameNumber(200), OpenBound), TRange<FFrameNumber>(200, 300)
		};

		bSuccess = TestTransform(*this, Transform, SourceRanges, Expected, TEXT("Simple Translation")) && bSuccess;
	}

	{
		// Test a simple translation + time scale

		// Transform 100 - 200 to -200 - 1000
		FMovieSceneSequenceTransform Transform = TransformRange(100, 200, -200, 1000);

		TRange<FFrameNumber> Expected[] = {
			InfiniteRange, TRange<FFrameNumber>(OpenBound, FFrameNumber(1000)), TRange<FFrameNumber>(FFrameNumber(-200), OpenBound), TRange<FFrameNumber>(-200, 1000)
		};

		bSuccess = TestTransform(*this, Transform, SourceRanges, Expected, TEXT("Simple Translation + half speed")) && bSuccess;
	}

	{
		// Test that transforming a frame number by the same transform multiple times, does the same as the equivalent accumulated transform

		// scales by 2, then offsets by 100
		FMovieSceneSequenceTransform SeedTransform = FMovieSceneSequenceTransform(100, 0.5f);
		FMovieSceneSequenceTransform AccumulatedTransform;

		FFrameTime SeedValue = 10;
		for (int32 i = 0; i < 5; ++i)
		{
			AccumulatedTransform = SeedTransform * AccumulatedTransform;

			SeedValue = SeedValue * SeedTransform;
		}

		FFrameTime AccumValue = FFrameTime(10) * AccumulatedTransform;
		if (AccumValue != SeedValue)
		{
			AddError(FString::Printf(TEXT("Accumulated transform does not have the same effect as separate transformations (%i+%.5f != %i+%.5f)"), AccumValue.FrameNumber.Value, AccumValue.GetSubFrame(), SeedValue.FrameNumber.Value, SeedValue.GetSubFrame()));
		}

		FMovieSceneSequenceTransform InverseTransform = AccumulatedTransform.Inverse();

		FFrameTime InverseValue = AccumValue * InverseTransform;
		if (InverseValue != 10)
		{
			AddError(FString::Printf(TEXT("Inverse accumulated transform does not return value back to its original value (%i+%.5f != 10)"), InverseValue.FrameNumber.Value, InverseValue.GetSubFrame()));
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTestObjects.h"
#include "Misc/AutomationTest.h"
#include "Compilation/MovieSceneCompiler.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "MovieSceneTestsCommon.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneCompilerPerfTest, "System.Engine.Sequencer.Compiler.Perf", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled)
bool FMovieSceneCompilerPerfTest::RunTest(const FString& Parameters)
{
	static bool  bFullCompile              = true;
	static bool  bInvalidateEveryIteration = false;
	static int32 NumIterations             = 1000000;

	FFrameRate TickResolution(1000, 1);

	UTestMovieSceneSequence* Sequence = NewObject<UTestMovieSceneSequence>(GetTransientPackage());
	Sequence->MovieScene->SetTickResolutionDirectly(TickResolution);

	for (int32 i = 0; i < 100; ++i)
	{
		UTestMovieSceneTrack* Track = Sequence->MovieScene->AddMasterTrack<UTestMovieSceneTrack>();

		int32 NumSections = FMath::Rand() % 10;
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			UTestMovieSceneSection* Section = NewObject<UTestMovieSceneSection>(Track);

			double StartSeconds    = FMath::FRand() * 60.f;
			double DurationSeconds = FMath::FRand() * 60.f;
			Section->SetRange(TRange<FFrameNumber>::Inclusive( (StartSeconds * TickResolution).RoundToFrame(), ((StartSeconds + DurationSeconds) * TickResolution).RoundToFrame() ));
			Track->SectionArray.Add(Section);
		}
	}

	struct FTestMovieScenePlayer : IMovieScenePlayer
	{
		FMovieSceneRootEvaluationTemplateInstance RootInstance;
		virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootInstance; }
		virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject = nullptr, bool bJumpCut = false) override {}
		virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
		virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
		virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override { return EMovieScenePlayerStatus::Playing; }
		virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	} TestPlayer;

	TestPlayer.RootInstance.Initialize(*Sequence, TestPlayer);

	if (bFullCompile)
	{
		FMovieSceneSequencePrecompiledTemplateStore Store;
		FMovieSceneCompiler::Compile(*Sequence, Store);
	}

	for (int32 i = 0; i < NumIterations; ++i)
	{
		if (bInvalidateEveryIteration)
		{
			Sequence->PrecompiledEvaluationTemplate.EvaluationField = FMovieSceneEvaluationField();
		}

		double StartSeconds    = FMath::FRand() * 60.f;
		double DurationSeconds = FMath::FRand() * 1.f;

		FMovieSceneEvaluationRange EvaluatedRange(TRange<FFrameTime>(StartSeconds * TickResolution, (StartSeconds + DurationSeconds) * TickResolution), TickResolution, EPlayDirection::Forwards);
		TestPlayer.RootInstance.Evaluate(EvaluatedRange, TestPlayer);
	}

	return true;
}

TRange<FFrameNumber> MakeRange(FFrameNumber LowerBound, FFrameNumber UpperBound, ERangeBoundTypes::Type LowerType, ERangeBoundTypes::Type UpperType)
{
	return LowerType == ERangeBoundTypes::Inclusive
		? UpperType == ERangeBoundTypes::Inclusive
			? TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(LowerBound), TRangeBound<FFrameNumber>::Inclusive(UpperBound))
			: TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(LowerBound), TRangeBound<FFrameNumber>::Exclusive(UpperBound))
		: UpperType == ERangeBoundTypes::Inclusive
			? TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Exclusive(LowerBound), TRangeBound<FFrameNumber>::Inclusive(UpperBound))
			: TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Exclusive(LowerBound), TRangeBound<FFrameNumber>::Exclusive(UpperBound));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneCompilerRangeTest, "System.Engine.Sequencer.Compiler.Ranges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneCompilerRangeTest::RunTest(const FString& Parameters)
{
	FFrameNumber CompileAtTimes[] = {
		-3, -2, -1, 0, 1, 2, 3, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	};
	// Test each combination of inc/excl boundary conditions for adjacent and adjoining ranges
	TRange<FFrameNumber> Ranges[] = {
		MakeRange(-2, -1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange(-2, -1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange(-2, -1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange(-2, -1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange(-1, -1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange(-1, -1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange(-1, -1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange(-1, -1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange(-1,  0, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange(-1,  0, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange(-1,  0, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange(-1,  0, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange( 0,  0, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 0,  0, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange( 0,  0, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 0,  0, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange( 0,  1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 0,  1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange( 0,  1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 0,  1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange( 1,  1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 1,  1, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange( 1,  1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 1,  1, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange( 0,  2, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 0,  2, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange( 0,  2, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 0,  2, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Exclusive),

		MakeRange( 10, 15, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 9,  15, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 10, 15, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 11, 15, ERangeBoundTypes::Exclusive, ERangeBoundTypes::Inclusive),
		
		MakeRange( 13, 17, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 13, 18, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Exclusive),
		MakeRange( 13, 19, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 13, 18, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),

		// Explicitly test two adjacent ranges that would produce effectively empty space in between them when iterating
		MakeRange( 21, 22, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
		MakeRange( 23, 24, ERangeBoundTypes::Inclusive, ERangeBoundTypes::Inclusive),
	};

	UTestMovieSceneSequence* Sequence = NewObject<UTestMovieSceneSequence>(GetTransientPackage());
	for (TRange<FFrameNumber> Range : Ranges)
	{
		UTestMovieSceneTrack*   Track   = Sequence->MovieScene->AddMasterTrack<UTestMovieSceneTrack>();
		UTestMovieSceneSection* Section = NewObject<UTestMovieSceneSection>(Track);

		Section->SetRange(Range);
		Track->SectionArray.Add(Section);
	}

	struct FTemplateStore : IMovieSceneSequenceTemplateStore
	{
		FMovieSceneEvaluationTemplate& AccessTemplate(UMovieSceneSequence&) override
		{
			return Template;
		}

		FMovieSceneEvaluationTemplate Template;
	} Store;

	// Compile individual times
	for (FFrameNumber Time : CompileAtTimes)
	{
		FMovieSceneCompiler::CompileRange(TRange<FFrameNumber>::Inclusive(Time, Time), *Sequence, Store);
	}

	// Compile a whole range
	Store.Template = FMovieSceneEvaluationTemplate();
	FMovieSceneCompiler::CompileRange(TRange<FFrameNumber>::All(), *Sequence, Store);

	// Compile the whole sequence
	Store.Template = FMovieSceneEvaluationTemplate();
	FMovieSceneCompiler::Compile(*Sequence, Store);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
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
		UTestMovieSceneTrack* Track = NewObject<UTestMovieSceneTrack>(GetTransientPackage());

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

#endif // WITH_DEV_AUTOMATION_TESTS
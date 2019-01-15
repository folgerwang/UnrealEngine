// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakesUtils.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Math/Range.h"
#include "MovieSceneSection.h"
#include "MovieSceneTimeHelpers.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"

namespace TakesUtils
{

/** Helper function - get the first PIE world (or first PIE client world if there is more than one) */
UWorld* GetFirstPIEWorld()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World()->IsPlayInEditor())
		{
			if (Context.World()->GetNetMode() == ENetMode::NM_Standalone ||
				(Context.World()->GetNetMode() == ENetMode::NM_Client && Context.PIEInstance == 2))
			{
				return Context.World();
			}
		}
	}

	return nullptr;
}

void ClampPlaybackRangeToEncompassAllSections(UMovieScene* InMovieScene)
{
	check(InMovieScene);

	TRange<FFrameNumber> OriginalPlayRange = InMovieScene->GetPlaybackRange();
	TRange<FFrameNumber> PlayRange(OriginalPlayRange.GetLowerBoundValue());

	TArray<UMovieSceneSection*> MovieSceneSections = InMovieScene->GetAllSections();
	for (UMovieSceneSection* Section : MovieSceneSections)
	{
		TRange<FFrameNumber> SectionRange = Section->GetRange();
		if (SectionRange.GetLowerBound().IsClosed() && SectionRange.GetUpperBound().IsClosed())
		{
			PlayRange = TRange<FFrameNumber>::Hull(PlayRange, SectionRange);
		}
	}

	InMovieScene->SetPlaybackRange(TRange<FFrameNumber>(OriginalPlayRange.GetLowerBoundValue(), PlayRange.GetUpperBoundValue()));

	// Initialize the working and view range with a little bit more space
	FFrameRate  TickResolution = InMovieScene->GetTickResolution();
	const double OutputViewSize = PlayRange.Size<FFrameNumber>() / TickResolution;
	const double OutputChange = OutputViewSize * 0.1;

	TRange<double> NewRange = MovieScene::ExpandRange(PlayRange / TickResolution, OutputChange);
	FMovieSceneEditorData& EditorData = InMovieScene->GetEditorData();
	EditorData.ViewStart = EditorData.WorkStart = NewRange.GetLowerBoundValue();
	EditorData.ViewEnd = EditorData.WorkEnd = NewRange.GetUpperBoundValue();
}

void SaveAsset(UObject* InObject)
{
	if (!InObject)
	{
		return;
	}

	// auto-save asset outside of the editor
	UPackage* const Package = InObject->GetOutermost();
	FString const PackageName = Package->GetName();
	FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);
}

void CreateCameraCutTrack(ULevelSequence* LevelSequence, const FGuid& RecordedCameraGuid, const FMovieSceneSequenceID& SequenceID, const TRange<FFrameNumber>& InRange)
{
	if (!RecordedCameraGuid.IsValid() || !LevelSequence)
	{
		return;
	}

	UMovieSceneTrack* CameraCutTrack = LevelSequence->GetMovieScene()->GetCameraCutTrack();
	if (CameraCutTrack && CameraCutTrack->GetAllSections().Num() > 1)
	{
		return;
	}


	if (!CameraCutTrack)
	{
		CameraCutTrack = LevelSequence->GetMovieScene()->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
	}
	else
	{
		CameraCutTrack->RemoveAllAnimationData();
	}

	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
	CameraCutSection->SetCameraBindingID(FMovieSceneObjectBindingID(RecordedCameraGuid, SequenceID, EMovieSceneObjectBindingSpace::Local));
	CameraCutSection->SetRange(InRange);
	CameraCutTrack->AddSection(*CameraCutSection);
}

}
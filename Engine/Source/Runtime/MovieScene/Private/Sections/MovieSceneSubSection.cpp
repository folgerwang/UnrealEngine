// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSubSection.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTimeHelpers.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Misc/FrameRate.h"

TWeakObjectPtr<UMovieSceneSubSection> UMovieSceneSubSection::TheRecordingSection;

float DeprecatedMagicNumber = TNumericLimits<float>::Lowest();

/* UMovieSceneSubSection structors
 *****************************************************************************/

UMovieSceneSubSection::UMovieSceneSubSection()
	: StartOffset_DEPRECATED(DeprecatedMagicNumber)
	, TimeScale_DEPRECATED(DeprecatedMagicNumber)
	, PrerollTime_DEPRECATED(DeprecatedMagicNumber)
{
}

FMovieSceneSequenceTransform UMovieSceneSubSection::OuterToInnerTransform() const
{
	UMovieSceneSequence* SequencePtr   = GetSequence();
	if (!SequencePtr)
	{
		return FMovieSceneSequenceTransform();
	}

	UMovieScene*         MovieScenePtr = SequencePtr->GetMovieScene();

	TRange<FFrameNumber> SubRange = GetRange();
	if (!MovieScenePtr || SubRange.GetLowerBound().IsOpen())
	{
		return FMovieSceneSequenceTransform();
	}

	const FFrameNumber InnerStartTime = MovieScene::DiscreteInclusiveLower(MovieScenePtr->GetPlaybackRange()) + Parameters.GetStartFrameOffset();
	const FFrameNumber OuterStartTime = MovieScene::DiscreteInclusiveLower(SubRange);

	const FFrameRate   InnerFrameRate = MovieScenePtr->GetTickResolution();
	const FFrameRate   OuterFrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	const float        FrameRateScale = (OuterFrameRate == InnerFrameRate) ? 1.f : (InnerFrameRate / OuterFrameRate).AsDecimal();

	return
		// Inner play offset
		FMovieSceneSequenceTransform(InnerStartTime)
		// Inner play rate
		* FMovieSceneSequenceTransform(0, Parameters.TimeScale * FrameRateScale)
		// Outer section start time
		* FMovieSceneSequenceTransform(-OuterStartTime);
}

FString UMovieSceneSubSection::GetPathNameInMovieScene() const
{
	UMovieScene* OuterMovieScene = GetTypedOuter<UMovieScene>();
	check(OuterMovieScene);
	return GetPathName(OuterMovieScene);
}

FMovieSceneSequenceID UMovieSceneSubSection::GetSequenceID() const
{
	FString FullPath = GetPathNameInMovieScene();
	if (SubSequence)
	{
		FullPath += TEXT(" / ");
		FullPath += SubSequence->GetPathName();
	}

	return FMovieSceneSequenceID(FCrc::Strihash_DEPRECATED(*FullPath));
}

void UMovieSceneSubSection::PostLoad()
{
	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	TOptional<double> StartOffsetToUpgrade;
	if (StartOffset_DEPRECATED != DeprecatedMagicNumber)
	{
		StartOffsetToUpgrade = StartOffset_DEPRECATED;
	}
	else if (Parameters.StartOffset_DEPRECATED != 0.f)
	{
		StartOffsetToUpgrade = Parameters.StartOffset_DEPRECATED;
	}

	if (StartOffsetToUpgrade.IsSet())
	{
		FFrameNumber StartFrame = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, StartOffsetToUpgrade.GetValue());
		Parameters.SetStartFrameOffset(StartFrame.Value);
	}

	if (TimeScale_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.TimeScale = TimeScale_DEPRECATED;
	}

	if (PrerollTime_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.PrerollTime_DEPRECATED = PrerollTime_DEPRECATED;
	}

	// Pre and post roll is now supported generically
	if (Parameters.PrerollTime_DEPRECATED > 0.f)
	{
		FFrameNumber ClampedPreRollFrames = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Parameters.PrerollTime_DEPRECATED);
		SetPreRollFrames(ClampedPreRollFrames.Value);
	}

	if (Parameters.PostrollTime_DEPRECATED > 0.f)
	{
		FFrameNumber ClampedPostRollFrames = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Parameters.PostrollTime_DEPRECATED);
		SetPreRollFrames(ClampedPostRollFrames.Value);
	}

	Super::PostLoad();
}

void UMovieSceneSubSection::SetSequence(UMovieSceneSequence* Sequence)
{
	SubSequence = Sequence;

#if WITH_EDITOR
	OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
#endif
}

UMovieSceneSequence* UMovieSceneSubSection::GetSequence() const
{
	// when recording we need to act as if we have no sequence
	// the sequence is patched at the end of recording
	if(GetRecordingSection() == this)
	{
		return nullptr;
	}
	else
	{
		return SubSequence;
	}
}

UMovieSceneSubSection* UMovieSceneSubSection::GetRecordingSection()
{
	// check if the section is still valid and part of a track (i.e. it has not been deleted or GCed)
	if(TheRecordingSection.IsValid())
	{
		UMovieSceneTrack* TrackOuter = Cast<UMovieSceneTrack>(TheRecordingSection->GetOuter());
		if(TrackOuter)
		{
			if(TrackOuter->HasSection(*TheRecordingSection.Get()))
			{
				return TheRecordingSection.Get();
			}
		}
	}

	return nullptr;
}

void UMovieSceneSubSection::SetAsRecording(bool bRecord)
{
	if(bRecord)
	{
		TheRecordingSection = this;
	}
	else
	{
		TheRecordingSection = nullptr;
	}
}

bool UMovieSceneSubSection::IsSetAsRecording()
{
	return GetRecordingSection() != nullptr;
}

AActor* UMovieSceneSubSection::GetActorToRecord()
{
	UMovieSceneSubSection* RecordingSection = GetRecordingSection();
	if(RecordingSection)
	{
		return RecordingSection->ActorToRecord.Get();
	}

	return nullptr;
}

#if WITH_EDITOR
void UMovieSceneSubSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// recreate runtime instance when sequence is changed
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
	}
}
#endif

UMovieSceneSection* UMovieSceneSubSection::SplitSection( FQualifiedFrameTime SplitTime )
{
	// GetRange is in owning sequence resolution so we check against the incoming SplitTime without converting it.
	TRange<FFrameNumber> InitialRange = GetRange();
	if ( !InitialRange.Contains(SplitTime.Time.FrameNumber) )
	{
		return nullptr;
	}

	FFrameNumber InitialStartOffset = Parameters.GetStartFrameOffset();

	UMovieSceneSubSection* NewSection = Cast<UMovieSceneSubSection>( UMovieSceneSection::SplitSection( SplitTime ) );
	if ( NewSection )
	{
		if (InitialRange.GetLowerBound().IsClosed())
		{
			// Sections need their offsets calculated in their local resolution. Different sequences can have different tick resolutions 
			// so we need to transform from the parent resolution to the local one before splitting them.
			FFrameRate LocalTickResolution;
			if (GetSequence())
			{
				LocalTickResolution = GetSequence()->GetMovieScene()->GetTickResolution();
			}
			else
			{
				UMovieScene* OuterScene = GetTypedOuter<UMovieScene>();
				if (OuterScene)
				{
					LocalTickResolution = OuterScene->GetTickResolution();
				}
			}

			FFrameNumber LocalResolutionStartOffset = FFrameRate::TransformTime(SplitTime.Time.GetFrame() - MovieScene::DiscreteInclusiveLower(InitialRange), SplitTime.Rate, LocalTickResolution).FrameNumber;

			FFrameNumber NewStartOffset = LocalResolutionStartOffset / Parameters.TimeScale;
			NewStartOffset += InitialStartOffset;

			if (NewStartOffset >= 0)
			{
				NewSection->Parameters.SetStartFrameOffset(NewStartOffset.Value);
			}
		}

		return NewSection;
	}

	return nullptr;
}

TOptional<TRange<FFrameNumber> > UMovieSceneSubSection::GetAutoSizeRange() const
{
	if (SubSequence && SubSequence->GetMovieScene())
	{
		FMovieSceneSequenceTransform InnerToOuter = OuterToInnerTransform().Inverse();
		UMovieScene* InnerMovieScene = SubSequence->GetMovieScene();

		FFrameTime IncAutoStartTime = FFrameTime(MovieScene::DiscreteInclusiveLower(InnerMovieScene->GetPlaybackRange())) * InnerToOuter;
		FFrameTime ExcAutoEndTime   = FFrameTime(MovieScene::DiscreteExclusiveUpper(InnerMovieScene->GetPlaybackRange())) * InnerToOuter;

		return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + (ExcAutoEndTime.RoundToFrame() - IncAutoStartTime.RoundToFrame()));
	}

	return Super::GetAutoSizeRange();
}

void UMovieSceneSubSection::TrimSection( FQualifiedFrameTime TrimTime, bool bTrimLeft )
{
	TRange<FFrameNumber> InitialRange = GetRange();
	if ( !InitialRange.Contains( TrimTime.Time.GetFrame() ) )
	{
		return;
	}

	FFrameNumber InitialStartOffset = Parameters.GetStartFrameOffset();

	UMovieSceneSection::TrimSection( TrimTime, bTrimLeft );

	// If trimming off the left, set the offset of the shot
	if ( bTrimLeft && InitialRange.GetLowerBound().IsClosed() )
	{
		// Sections need their offsets calculated in their local resolution. Different sequences can have different tick resolutions 
		// so we need to transform from the parent resolution to the local one before splitting them.
		FFrameRate LocalTickResolution = GetSequence()->GetMovieScene()->GetTickResolution();
		FFrameNumber LocalResolutionStartOffset = FFrameRate::TransformTime(TrimTime.Time.GetFrame() - MovieScene::DiscreteInclusiveLower(InitialRange), TrimTime.Rate, LocalTickResolution).FrameNumber;


		FFrameNumber NewStartOffset = LocalResolutionStartOffset / Parameters.TimeScale;
		NewStartOffset += InitialStartOffset;

		// Ensure start offset is not less than 0
		if (NewStartOffset >= 0)
		{
			Parameters.SetStartFrameOffset(NewStartOffset.Value);
		}
	}
}

FMovieSceneSubSequenceData UMovieSceneSubSection::GenerateSubSequenceData(const FSubSequenceInstanceDataParams& Params) const
{
	return FMovieSceneSubSequenceData(*this);
}

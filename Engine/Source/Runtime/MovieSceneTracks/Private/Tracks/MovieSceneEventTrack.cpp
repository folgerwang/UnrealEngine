// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneEventTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "Evaluation/MovieSceneEventTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "IMovieSceneTracksModule.h"

#define LOCTEXT_NAMESPACE "MovieSceneEventTrack"


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneEventTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


UMovieSceneSection* UMovieSceneEventTrack::CreateNewSection()
{
	return NewObject<UMovieSceneEventTriggerSection>(this, NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneEventTrack::GetAllSections() const
{
	return Sections;
}


bool UMovieSceneEventTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneEventTrack::IsEmpty() const
{
	return (Sections.Num() == 0);
}


void UMovieSceneEventTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneEventTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

FMovieSceneEvalTemplatePtr UMovieSceneEventTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	if (const UMovieSceneEventSection* LegacyEventSection = Cast<const UMovieSceneEventSection>(&InSection))
	{
		return FMovieSceneEventSectionTemplate(*LegacyEventSection, *this);
	}
	else if (const UMovieSceneEventTriggerSection* TriggerSection = Cast<const UMovieSceneEventTriggerSection>(&InSection))
	{
		return FMovieSceneEventTriggerTemplate(*TriggerSection, *this);
	}
	else if (const UMovieSceneEventRepeaterSection* RepeaterSection = Cast<const UMovieSceneEventRepeaterSection>(&InSection))
	{
		return FMovieSceneEventRepeaterTemplate(*RepeaterSection, *this);
	}
	else
	{
		return FMovieSceneEvalTemplatePtr();
	}
}

void UMovieSceneEventTrack::PostCompile(FMovieSceneEvaluationTrack& Track, const FMovieSceneTrackCompilerArgs& Args) const
{
	switch (EventPosition)
	{
	case EFireEventsAtPosition::AtStartOfEvaluation:
		Track.SetEvaluationGroup(IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::PreEvaluation));
		break;

	case EFireEventsAtPosition::AtEndOfEvaluation:
		Track.SetEvaluationGroup(IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::PostEvaluation));
		break;

	default:
		Track.SetEvaluationGroup(IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::SpawnObjects));
		Track.SetEvaluationPriority(UMovieSceneSpawnTrack::GetEvaluationPriority() - 100);
		break;
	}

	Track.SetEvaluationMethod(EEvaluationMethod::Swept);
}

FMovieSceneTrackSegmentBlenderPtr UMovieSceneEventTrack::GetTrackSegmentBlender() const
{
	// This is a temporary measure to alleviate some issues with event tracks with finite ranges.
	// By filling empty space between sections, we're essentially always making this track evaluate
	// which allows it to sweep sections correctly when the play-head moves from a finite section
	// to empty space. This doesn't address the issue of the play-head moving from inside a sub-sequence
	// to outside, but that specific issue is even more nuanced and complicated to address.
	struct FMovieSceneEventTrackSegmentBlender : FMovieSceneTrackSegmentBlender
	{
		FMovieSceneEventTrackSegmentBlender()
		{
			bCanFillEmptySpace = true;
			bAllowEmptySegments = true;
		}
		virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<FFrameNumber>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const
		{
			return FMovieSceneSegment(Range);
		}
	};
	return FMovieSceneEventTrackSegmentBlender();
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneEventTrack::GetDefaultDisplayName() const
{ 
	return LOCTEXT("TrackName", "Events"); 
}

#endif


#undef LOCTEXT_NAMESPACE

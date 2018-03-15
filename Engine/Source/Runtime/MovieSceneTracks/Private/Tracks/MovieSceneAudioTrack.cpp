// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneAudioTrack.h"
#include "Audio.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "MovieScene.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDecompress.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"


#define LOCTEXT_NAMESPACE "MovieSceneAudioTrack"


UMovieSceneAudioTrack::UMovieSceneAudioTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(93, 95, 136);
	RowHeight = 50;
#endif
}


const TArray<UMovieSceneSection*>& UMovieSceneAudioTrack::GetAllSections() const
{
	return AudioSections;
}


bool UMovieSceneAudioTrack::SupportsMultipleRows() const
{
	return true;
}


void UMovieSceneAudioTrack::RemoveAllAnimationData()
{
	// do nothing
}


bool UMovieSceneAudioTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AudioSections.Contains(&Section);
}


void UMovieSceneAudioTrack::AddSection(UMovieSceneSection& Section)
{
	AudioSections.Add(&Section);
}


void UMovieSceneAudioTrack::RemoveSection(UMovieSceneSection& Section)
{
	AudioSections.Remove(&Section);
}


bool UMovieSceneAudioTrack::IsEmpty() const
{
	return AudioSections.Num() == 0;
}

float UMovieSceneAudioTrack::GetSoundDuration(USoundBase* Sound)
{
	USoundWave* SoundWave = nullptr;

	if (Sound->IsA<USoundWave>())
	{
		SoundWave = Cast<USoundWave>(Sound);
	}
	else if (Sound->IsA<USoundCue>())
	{
#if WITH_EDITORONLY_DATA
		USoundCue* SoundCue = Cast<USoundCue>(Sound);

		// @todo Sequencer - Right now for sound cues, we just use the first sound wave in the cue
		// In the future, it would be better to properly generate the sound cue's data after forcing determinism
		const TArray<USoundNode*>& AllNodes = SoundCue->AllNodes;
		for (int32 i = 0; i < AllNodes.Num() && SoundWave == nullptr; ++i)
		{
			if (AllNodes[i]->IsA<USoundNodeWavePlayer>())
			{
				SoundWave = Cast<USoundNodeWavePlayer>(AllNodes[i])->GetSoundWave();
			}
		}
#endif
	}

	const float Duration = (SoundWave ? SoundWave->GetDuration() : 0.f);
	return Duration == INDEFINITELY_LOOPING_DURATION ? SoundWave->Duration : Duration;
}

UMovieSceneSection* UMovieSceneAudioTrack::AddNewSoundOnRow(USoundBase* Sound, FFrameNumber Time, int32 RowIndex)
{
	check(Sound);
	
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetFrameResolution();

	// determine initial duration
	// @todo Once we have infinite sections, we can remove this
	// @todo ^^ Why? Infinte sections would mean there's no starting time?
	FFrameTime DurationToUse = 1.f * FrameRate; // if all else fails, use 1 second duration

	float SoundDuration = GetSoundDuration(Sound);
	if (SoundDuration != INDEFINITELY_LOOPING_DURATION)
	{
		DurationToUse = SoundDuration * FrameRate;
	}

	// add the section
	UMovieSceneAudioSection* NewSection = NewObject<UMovieSceneAudioSection>(this);
	NewSection->InitialPlacementOnRow( AudioSections, Time, DurationToUse.FrameNumber.Value, RowIndex );
	NewSection->SetSound(Sound);

	AudioSections.Add(NewSection);

	return NewSection;
}


bool UMovieSceneAudioTrack::IsAMasterTrack() const
{
	return Cast<UMovieScene>(GetOuter())->IsAMasterTrack(*this);
}


FMovieSceneTrackRowSegmentBlenderPtr UMovieSceneAudioTrack::GetRowSegmentBlender() const
{
	struct FBlender : FMovieSceneTrackRowSegmentBlender
	{
		virtual void Blend(FSegmentBlendData& BlendData) const override
		{
			// Run the default high pass filter for overlap priority
			MovieSceneSegmentCompiler::FilterOutUnderlappingSections(BlendData);

			// Weed out based on array index (legacy behaviour)
			MovieSceneSegmentCompiler::BlendSegmentLegacySectionOrder(BlendData);
		}
	};
	return FBlender();
}


#undef LOCTEXT_NAMESPACE

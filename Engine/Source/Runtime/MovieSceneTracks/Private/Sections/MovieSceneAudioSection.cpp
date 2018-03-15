// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundBase.h"
#include "Evaluation/MovieSceneAudioTemplate.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"

#if WITH_EDITOR

struct FAudioChannelEditorData
{
	FAudioChannelEditorData()
	{
		Data[0].SetIdentifiers("Volume", NSLOCTEXT("MovieSceneAudioSection", "SoundVolumeText", "Volume"));
		Data[1].SetIdentifiers("Pitch", NSLOCTEXT("MovieSceneAudioSection", "PitchText", "Pitch"));
	}

	FMovieSceneChannelEditorData Data[2];
};

#endif // WITH_EDITOR

UMovieSceneAudioSection::UMovieSceneAudioSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Sound = nullptr;
	StartOffset = 0.f;
	AudioStartTime_DEPRECATED = 0.f;
	AudioDilationFactor_DEPRECATED = 1.f;
	AudioVolume_DEPRECATED = 1.f;
	bSuppressSubtitles = false;
	bOverrideAttenuation = false;

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

	SoundVolume.SetDefault(1.f);
	PitchMultiplier.SetDefault(1.f);

	// Set up the channel proxy
	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	static const FAudioChannelEditorData EditorData;
	Channels.Add(SoundVolume,     EditorData.Data[0], TMovieSceneExternalValue<float>());
	Channels.Add(PitchMultiplier, EditorData.Data[1], TMovieSceneExternalValue<float>());

#else

	Channels.Add(SoundVolume);
	Channels.Add(PitchMultiplier);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

FMovieSceneEvalTemplatePtr UMovieSceneAudioSection::GenerateTemplate() const
{
	return FMovieSceneAudioSectionTemplate(*this);
}

TOptional<FFrameTime> UMovieSceneAudioSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(StartOffset * GetTypedOuter<UMovieScene>()->GetFrameResolution());
}

void UMovieSceneAudioSection::PostLoad()
{
	Super::PostLoad();

	if (AudioDilationFactor_DEPRECATED != FLT_MAX)
	{
		PitchMultiplier.SetDefault(AudioDilationFactor_DEPRECATED);

		AudioDilationFactor_DEPRECATED = FLT_MAX;
	}

	if (AudioVolume_DEPRECATED != FLT_MAX)
	{
		SoundVolume.SetDefault(AudioVolume_DEPRECATED);

		AudioVolume_DEPRECATED = FLT_MAX;
	}

	if (AudioStartTime_DEPRECATED != FLT_MAX)
	{
		// Previously, start time in relation to the sequence. Start time was used to calculate the offset into the 
		// clip at the start of the section evaluation as such: Section Start Time - Start Time. 
		if (AudioStartTime_DEPRECATED != 0.f && HasStartFrame())
		{
			StartOffset = GetInclusiveStartFrame() / GetTypedOuter<UMovieScene>()->GetFrameResolution() - AudioStartTime_DEPRECATED;
		}
		AudioStartTime_DEPRECATED = FLT_MAX;
	}
}


UMovieSceneSection* UMovieSceneAudioSection::SplitSection(FFrameNumber SplitTime)
{
	const float NewOffset = HasStartFrame() ? StartOffset + (SplitTime - GetInclusiveStartFrame()) / GetTypedOuter<UMovieScene>()->GetFrameResolution() : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneAudioSection* NewAudioSection = Cast<UMovieSceneAudioSection>(NewSection);
		NewAudioSection->StartOffset = NewOffset;
	}
	return NewSection;
}

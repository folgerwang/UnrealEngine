// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundBase.h"
#include "Evaluation/MovieSceneAudioTemplate.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"

#if WITH_EDITOR

struct FAudioChannelEditorData
{
	FAudioChannelEditorData()
	{
		Data[0].SetIdentifiers("Volume", NSLOCTEXT("MovieSceneAudioSection", "SoundVolumeText", "Volume"));
		Data[1].SetIdentifiers("Pitch", NSLOCTEXT("MovieSceneAudioSection", "PitchText", "Pitch"));
	}

	FMovieSceneChannelMetaData Data[2];
};

#endif // WITH_EDITOR

namespace
{
	float AudioDeprecatedMagicNumber = TNumericLimits<float>::Lowest();
}

UMovieSceneAudioSection::UMovieSceneAudioSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Sound = nullptr;
	StartOffset_DEPRECATED = AudioDeprecatedMagicNumber;
	AudioStartTime_DEPRECATED = AudioDeprecatedMagicNumber;
	AudioDilationFactor_DEPRECATED = AudioDeprecatedMagicNumber;
	AudioVolume_DEPRECATED = AudioDeprecatedMagicNumber;
	bSuppressSubtitles = false;
	bOverrideAttenuation = false;

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

	SoundVolume.SetDefault(1.f);
	PitchMultiplier.SetDefault(1.f);

	// Set up the channel proxy
	FMovieSceneChannelProxyData Channels;

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
	return TOptional<FFrameTime>(StartFrameOffset);
}

void UMovieSceneAudioSection::PostLoad()
{
	Super::PostLoad();

	if (AudioDilationFactor_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		PitchMultiplier.SetDefault(AudioDilationFactor_DEPRECATED);

		AudioDilationFactor_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	if (AudioVolume_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		SoundVolume.SetDefault(AudioVolume_DEPRECATED);

		AudioVolume_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	TOptional<double> StartOffsetToUpgrade;
	if (AudioStartTime_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		// Previously, start time in relation to the sequence. Start time was used to calculate the offset into the 
		// clip at the start of the section evaluation as such: Section Start Time - Start Time. 
		if (AudioStartTime_DEPRECATED != 0.f && HasStartFrame())
		{
			StartOffsetToUpgrade = GetInclusiveStartFrame() / GetTypedOuter<UMovieScene>()->GetTickResolution() - AudioStartTime_DEPRECATED;
		}
		AudioStartTime_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	if (StartOffset_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		StartOffsetToUpgrade = StartOffset_DEPRECATED;

		StartOffset_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	if (StartOffsetToUpgrade.IsSet())
	{
		FFrameNumber StartFrame = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, StartOffsetToUpgrade.GetValue());
		StartFrameOffset = StartFrame.Value;
	}
}

FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, FFrameNumber StartOffset, FFrameNumber StartFrame)
{
	return StartOffset + TrimTime.Time.FrameNumber - StartFrame;
}

	
TOptional<TRange<FFrameNumber> > UMovieSceneAudioSection::GetAutoSizeRange() const
{
	if (!Sound)
	{
		return TRange<FFrameNumber>();
	}

	float SoundDuration = MovieSceneHelpers::GetSoundDuration(Sound);

	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	// determine initial duration
	// @todo Once we have infinite sections, we can remove this
	// @todo ^^ Why? Infinte sections would mean there's no starting time?
	FFrameTime DurationToUse = 1.f * FrameRate; // if all else fails, use 1 second duration

	if (SoundDuration != INDEFINITELY_LOOPING_DURATION)
	{
		DurationToUse = SoundDuration * FrameRate;
	}

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + DurationToUse.FrameNumber);
}

	
void UMovieSceneAudioSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft)
{
	SetFlags(RF_Transactional);

	if (TryModify())
	{
		if (bTrimLeft)
		{
			StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft);
	}
}

UMovieSceneSection* UMovieSceneAudioSection::SplitSection(FQualifiedFrameTime SplitTime)
{
	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneAudioSection* NewAudioSection = Cast<UMovieSceneAudioSection>(NewSection);
		NewAudioSection->StartFrameOffset = NewOffset;
	}
	return NewSection;
}

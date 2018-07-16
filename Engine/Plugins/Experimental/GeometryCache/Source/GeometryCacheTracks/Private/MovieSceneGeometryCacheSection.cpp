// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneGeometryCacheSection.h"
#include "MovieSceneGeometryCacheTemplate.h"
#include "Logging/MessageLog.h"
#include "MovieScene.h"
#include "UObject/SequencerObjectVersion.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "MovieSceneGeometryCacheSection"


FMovieSceneGeometryCacheParams::FMovieSceneGeometryCacheParams()
{
	StartOffset = 0.f;
	EndOffset = 0.f;
	PlayRate = 1.f;
	bReverse = false;
}

UMovieSceneGeometryCacheSection::UMovieSceneGeometryCacheSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

	BlendType = EMovieSceneBlendType::Absolute;
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::ProjectDefault);

#if WITH_EDITOR

	PreviousPlayRate = Params.PlayRate;

#endif
}

TOptional<FFrameTime> UMovieSceneGeometryCacheSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(Params.StartOffset * GetTypedOuter<UMovieScene>()->GetTickResolution());
}

void UMovieSceneGeometryCacheSection::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	Super::Serialize(Ar);
}


FMovieSceneEvalTemplatePtr UMovieSceneGeometryCacheSection::GenerateTemplate() const
{
	return FMovieSceneGeometryCacheSectionTemplate(*this);
}

float GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, const FMovieSceneGeometryCacheParams& Params, FFrameNumber StartFrame)
{
	float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	float AnimPosition = (TrimTime.Time - StartFrame) / TrimTime.Rate * AnimPlayRate;
	float SeqLength = Params.GetSequenceLength() - (Params.StartOffset + Params.EndOffset);

	float NewOffset = FMath::Fmod(AnimPosition, SeqLength);
	NewOffset += Params.StartOffset;

	return NewOffset;
}


TOptional<TRange<FFrameNumber> > UMovieSceneGeometryCacheSection::GetAutoSizeRange() const
{
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	FFrameTime AnimationLength = Params.GetSequenceLength() * FrameRate;

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + AnimationLength.FrameNumber);
}


void UMovieSceneGeometryCacheSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft)
{
	SetFlags(RF_Transactional);

	if (TryModify())
	{
		if (bTrimLeft)
		{
			Params.StartOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, Params, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft);
	}
}

UMovieSceneSection* UMovieSceneGeometryCacheSection::SplitSection(FQualifiedFrameTime SplitTime)
{
	const float NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, Params, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneGeometryCacheSection* NewGeometrySection = Cast<UMovieSceneGeometryCacheSection>(NewSection);
		NewGeometrySection->Params.StartOffset = NewOffset;
	}
	return NewSection;
}


void UMovieSceneGeometryCacheSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	const FFrameRate   FrameRate  = GetTypedOuter<UMovieScene>()->GetTickResolution();
	const FFrameNumber StartFrame = GetInclusiveStartFrame();
	const FFrameNumber EndFrame   = GetExclusiveEndFrame() - 1; // -1 because we don't need to add the end frame twice

	const float AnimPlayRate     = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	const float SeqLengthSeconds = (Params.GetSequenceLength() - (Params.StartOffset + Params.EndOffset)) / AnimPlayRate;

	FFrameTime SequenceFrameLength = SeqLengthSeconds * FrameRate;
	if (SequenceFrameLength.FrameNumber > 1)
	{
		// Snap to the repeat times
		FFrameTime CurrentTime = StartFrame;
		while (CurrentTime < EndFrame)
		{
			OutSnapTimes.Add(CurrentTime.FrameNumber);
			CurrentTime += SequenceFrameLength;
		}
	}
}

float UMovieSceneGeometryCacheSection::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
	FMovieSceneGeometryCacheSectionTemplateParameters TemplateParams(Params, GetInclusiveStartFrame(), GetExclusiveEndFrame());
	return TemplateParams.MapTimeToAnimation(InPosition, InFrameRate);
}


#if WITH_EDITOR
void UMovieSceneGeometryCacheSection::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Store the current play rate so that we can compute the amount to compensate the section end time when the play rate changes
	PreviousPlayRate = Params.PlayRate;

	Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneGeometryCacheSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Adjust the duration automatically if the play rate changes
	if (PropertyChangedEvent.Property != nullptr &&
		PropertyChangedEvent.Property->GetFName() == TEXT("PlayRate"))
	{
		float NewPlayRate = Params.PlayRate;

		if (!FMath::IsNearlyZero(NewPlayRate))
		{
			float CurrentDuration = MovieScene::DiscreteSize(GetRange());
			float NewDuration = CurrentDuration * (PreviousPlayRate / NewPlayRate);
			SetEndFrame( GetInclusiveStartFrame() + FMath::FloorToInt(NewDuration) );

			PreviousPlayRate = NewPlayRate;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE 

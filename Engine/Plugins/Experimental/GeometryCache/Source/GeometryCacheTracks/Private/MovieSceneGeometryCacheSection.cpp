// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneGeometryCacheSection.h"
#include "MovieSceneGeometryCacheTemplate.h"
#include "Logging/MessageLog.h"
#include "MovieScene.h"
#include "UObject/SequencerObjectVersion.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "MovieSceneGeometryCacheSection"

namespace
{
	float GeometryCacheDeprecatedMagicNumber = TNumericLimits<float>::Lowest();
}

FMovieSceneGeometryCacheParams::FMovieSceneGeometryCacheParams()
{
	GeometryCacheAsset = nullptr;
	GeometryCache_DEPRECATED = nullptr;
	StartOffset_DEPRECATED = GeometryCacheDeprecatedMagicNumber;
	EndOffset_DEPRECATED = GeometryCacheDeprecatedMagicNumber;
	PlayRate = 1.f;
	bReverse = false;
}

UMovieSceneGeometryCacheSection::UMovieSceneGeometryCacheSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BlendType = EMovieSceneBlendType::Absolute;
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::ProjectDefault);

#if WITH_EDITOR

	PreviousPlayRate = Params.PlayRate;

#endif
}

TOptional<FFrameTime> UMovieSceneGeometryCacheSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(Params.StartFrameOffset);
}

void UMovieSceneGeometryCacheSection::PostLoad()
{
	Super::PostLoad();

	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	if (Params.StartOffset_DEPRECATED != GeometryCacheDeprecatedMagicNumber)
	{
		Params.StartFrameOffset = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Params.StartOffset_DEPRECATED).Value;

		Params.StartOffset_DEPRECATED = GeometryCacheDeprecatedMagicNumber;
	}

	if (Params.EndOffset_DEPRECATED != GeometryCacheDeprecatedMagicNumber)
	{
		Params.EndFrameOffset = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Params.EndOffset_DEPRECATED).Value;

		Params.EndOffset_DEPRECATED = GeometryCacheDeprecatedMagicNumber;
	}

	if (Params.GeometryCache_DEPRECATED.ResolveObject() != nullptr
		&& Params.GeometryCacheAsset == nullptr)
	{
		UGeometryCacheComponent *Comp = Cast<UGeometryCacheComponent>(Params.GeometryCache_DEPRECATED.ResolveObject());
		if (Comp)
		{
			Params.GeometryCacheAsset = (Comp->GetGeometryCache());
		}
	}
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

FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, const FMovieSceneGeometryCacheParams& Params, FFrameNumber StartFrame, FFrameRate FrameRate)
{
	float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	float AnimPosition = (TrimTime.Time - StartFrame) / TrimTime.Rate * AnimPlayRate;
	float SeqLength = Params.GetSequenceLength() - FrameRate.AsSeconds(Params.StartFrameOffset + Params.EndFrameOffset) / AnimPlayRate;

	FFrameNumber NewOffset = FrameRate.AsFrameNumber(FMath::Fmod(AnimPosition, SeqLength));
	NewOffset += Params.StartFrameOffset;

	return NewOffset;
}


TOptional<TRange<FFrameNumber> > UMovieSceneGeometryCacheSection::GetAutoSizeRange() const
{
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameTime AnimationLength = Params.GetSequenceLength() * FrameRate;
	int32 IFrameNumber = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f);

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + IFrameNumber + 1);
}


void UMovieSceneGeometryCacheSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft)
{
	SetFlags(RF_Transactional);

	if (TryModify())
	{
		if (bTrimLeft)
		{
			FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

			Params.StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft);
	}
}

UMovieSceneSection* UMovieSceneGeometryCacheSection::SplitSection(FQualifiedFrameTime SplitTime)
{
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneGeometryCacheSection* NewGeometrySection = Cast<UMovieSceneGeometryCacheSection>(NewSection);
		NewGeometrySection->Params.StartFrameOffset = NewOffset;
	}
	return NewSection;
}


void UMovieSceneGeometryCacheSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	const FFrameRate   FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
	const FFrameNumber StartFrame = GetInclusiveStartFrame();
	const FFrameNumber EndFrame = GetExclusiveEndFrame() - 1; // -1 because we don't need to add the end frame twice

	const float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	const float SeqLengthSeconds = Params.GetSequenceLength() - FrameRate.AsSeconds(Params.StartFrameOffset + Params.EndFrameOffset) / AnimPlayRate;

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

float UMovieSceneGeometryCacheSection::MapTimeToAnimation(float ComponentDuration, FFrameTime InPosition, FFrameRate InFrameRate) const
{
	FMovieSceneGeometryCacheSectionTemplateParameters TemplateParams(Params, GetInclusiveStartFrame(), GetExclusiveEndFrame());
	return TemplateParams.MapTimeToAnimation(ComponentDuration, InPosition, InFrameRate);
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
			SetEndFrame(GetInclusiveStartFrame() + FMath::FloorToInt(NewDuration));

			PreviousPlayRate = NewPlayRate;
		}
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

float FMovieSceneGeometryCacheParams::GetSequenceLength() const
{
	return GeometryCacheAsset != nullptr ? GeometryCacheAsset->CalculateDuration() : 0.f;
}

#undef LOCTEXT_NAMESPACE 

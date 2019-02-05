// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Animation/AnimSequence.h"
#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"
#include "Logging/MessageLog.h"
#include "MovieScene.h"
#include "UObject/SequencerObjectVersion.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "MovieSceneSkeletalAnimationSection"

namespace
{
	FName DefaultSlotName( "DefaultSlot" );
	float SkeletalDeprecatedMagicNumber = TNumericLimits<float>::Lowest();
}

FMovieSceneSkeletalAnimationParams::FMovieSceneSkeletalAnimationParams()
{
	Animation = nullptr;
	StartOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
	EndOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
	PlayRate = 1.f;
	bReverse = false;
	SlotName = DefaultSlotName;
	Weight.SetDefault(1.f);
	bSkipAnimNotifiers = false;
	bForceCustomMode = false;
}

UMovieSceneSkeletalAnimationSection::UMovieSceneSkeletalAnimationSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AnimSequence_DEPRECATED = nullptr;
	Animation_DEPRECATED = nullptr;
	StartOffset_DEPRECATED = 0.f;
	EndOffset_DEPRECATED = 0.f;
	PlayRate_DEPRECATED = 1.f;
	bReverse_DEPRECATED = false;
	SlotName_DEPRECATED = DefaultSlotName;

	BlendType = EMovieSceneBlendType::Absolute;
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

#if WITH_EDITOR

	PreviousPlayRate = Params.PlayRate;

	static FMovieSceneChannelMetaData MetaData("Weight", LOCTEXT("WeightChannelName", "Weight"));
	MetaData.bCanCollapseToTrack = false;
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Params.Weight, MetaData, TMovieSceneExternalValue<float>());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Params.Weight);

#endif
}

TOptional<FFrameTime> UMovieSceneSkeletalAnimationSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(Params.StartFrameOffset);
}

void UMovieSceneSkeletalAnimationSection::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	Super::Serialize(Ar);
}

void UMovieSceneSkeletalAnimationSection::PostLoad()
{
	if (AnimSequence_DEPRECATED)
	{
		Params.Animation = AnimSequence_DEPRECATED;
	}

	if (Animation_DEPRECATED != nullptr)
	{
		Params.Animation = Animation_DEPRECATED;
	}

	if (StartOffset_DEPRECATED != 0.f)
	{
		Params.StartOffset_DEPRECATED = StartOffset_DEPRECATED;
	}

	if (EndOffset_DEPRECATED != 0.f)
	{
		Params.EndOffset_DEPRECATED = EndOffset_DEPRECATED;
	}

	if (PlayRate_DEPRECATED != 1.f)
	{
		Params.PlayRate = PlayRate_DEPRECATED;
	}

	if (bReverse_DEPRECATED != false)
	{
		Params.bReverse = bReverse_DEPRECATED;
	}

	if (SlotName_DEPRECATED != DefaultSlotName)
	{
		Params.SlotName = SlotName_DEPRECATED;
	}

	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	if (Params.StartOffset_DEPRECATED != SkeletalDeprecatedMagicNumber)
	{
		Params.StartFrameOffset = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Params.StartOffset_DEPRECATED).Value;

		Params.StartOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
	}

	if (Params.EndOffset_DEPRECATED != SkeletalDeprecatedMagicNumber)
	{
		Params.EndFrameOffset = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Params.EndOffset_DEPRECATED).Value;

		Params.EndOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
	}

	// if version is less than this
	if (GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::ConvertEnableRootMotionToForceRootLock)
	{
		UAnimSequence* AnimSeq = Cast<UAnimSequence>(Params.Animation);
		if (AnimSeq && AnimSeq->bEnableRootMotion && !AnimSeq->bForceRootLock)
		{
			// this is not ideal, but previously single player node was using this flag to whether or not to extract root motion
			// with new anim sequencer instance, this would break because we use the instance flag to extract root motion or not
			// so instead of setting that flag, we use bForceRootLock flag to asset
			// this can have side effect, where users didn't want that to be on to start with
			// So we'll notify users to let them know this has to be saved
			AnimSeq->bForceRootLock = true;
			AnimSeq->MarkPackageDirty();
			// warning to users
#if WITH_EDITOR			
			if (!IsRunningGame())
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Warning();
				Message->AddToken(FTextToken::Create(LOCTEXT("RootMotionFixUp1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(AnimSeq->GetPathName(), FText::FromString(GetNameSafe(AnimSeq))));
				Message->AddToken(FTextToken::Create(LOCTEXT("RootMotionFixUp2", "will be set to ForceRootLock on. Please save the animation if you want to keep this change.")));
				Message->SetSeverity(EMessageSeverity::Warning);
				LoadErrors.Notify();
			}
#endif // WITH_EDITOR

			UE_LOG(LogMovieScene, Warning, TEXT("%s Animation has set ForceRootLock to be used in Sequencer. If this animation is used in anywhere else using root motion, that will cause conflict."), *AnimSeq->GetName());
		}
	}

	Super::PostLoad();
}

FMovieSceneEvalTemplatePtr UMovieSceneSkeletalAnimationSection::GenerateTemplate() const
{
	return FMovieSceneSkeletalAnimationSectionTemplate(*this);
}

FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, const FMovieSceneSkeletalAnimationParams& Params, FFrameNumber StartFrame, FFrameRate FrameRate)
{
	float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	float AnimPosition = (TrimTime.Time - StartFrame) / TrimTime.Rate * AnimPlayRate;
	float SeqLength = Params.GetSequenceLength() - FrameRate.AsSeconds(Params.StartFrameOffset + Params.EndFrameOffset) / AnimPlayRate;

	FFrameNumber NewOffset = FrameRate.AsFrameNumber(FMath::Fmod(AnimPosition, SeqLength));
	NewOffset += Params.StartFrameOffset;

	return NewOffset;
}


TOptional<TRange<FFrameNumber> > UMovieSceneSkeletalAnimationSection::GetAutoSizeRange() const
{
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	FFrameTime AnimationLength = Params.GetSequenceLength() * FrameRate;

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + AnimationLength.FrameNumber);
}


void UMovieSceneSkeletalAnimationSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft)
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

UMovieSceneSection* UMovieSceneSkeletalAnimationSection::SplitSection(FQualifiedFrameTime SplitTime)
{
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneSkeletalAnimationSection* NewSkeletalSection = Cast<UMovieSceneSkeletalAnimationSection>(NewSection);
		NewSkeletalSection->Params.StartFrameOffset = NewOffset;
	}
	return NewSection;
}


void UMovieSceneSkeletalAnimationSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	const FFrameRate   FrameRate  = GetTypedOuter<UMovieScene>()->GetTickResolution();
	const FFrameNumber StartFrame = GetInclusiveStartFrame();
	const FFrameNumber EndFrame   = GetExclusiveEndFrame() - 1; // -1 because we don't need to add the end frame twice

	const float AnimPlayRate     = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
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

float UMovieSceneSkeletalAnimationSection::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
	FMovieSceneSkeletalAnimationSectionTemplateParameters TemplateParams(Params, GetInclusiveStartFrame(), GetExclusiveEndFrame());
	return TemplateParams.MapTimeToAnimation(InPosition, InFrameRate);
}

float UMovieSceneSkeletalAnimationSection::GetTotalWeightValue(FFrameTime InTime) const
{
	float ManualWeight = 1.f;
	Params.Weight.Evaluate(InTime, ManualWeight);
	return ManualWeight *  EvaluateEasing(InTime);
}


#if WITH_EDITOR
void UMovieSceneSkeletalAnimationSection::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Store the current play rate so that we can compute the amount to compensate the section end time when the play rate changes
	PreviousPlayRate = Params.PlayRate;

	Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneSkeletalAnimationSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
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

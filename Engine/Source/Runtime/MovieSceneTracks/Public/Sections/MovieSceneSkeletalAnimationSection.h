// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Animation/AnimSequenceBase.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneSkeletalAnimationSection.generated.h"

USTRUCT(BlueprintType)
struct FMovieSceneSkeletalAnimationParams
{
	GENERATED_BODY()

	FMovieSceneSkeletalAnimationParams();

	/** Gets the animation duration, modified by play rate */
	float GetDuration() const { return FMath::IsNearlyZero(PlayRate) || Animation == nullptr ? 0.f : Animation->SequenceLength / PlayRate; }

	/** Gets the animation sequence length, not modified by play rate */
	float GetSequenceLength() const { return Animation != nullptr ? Animation->SequenceLength : 0.f; }

	/** The animation this section plays */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation", meta=(AllowedClasses = "AnimSequence, AnimComposite"))
	UAnimSequenceBase* Animation;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	FFrameNumber StartFrameOffset;

	/** The offset into the end of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	FFrameNumber EndFrameOffset;

	/** The playback rate of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	uint32 bReverse:1;

	/** The slot name to use for the animation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation" )
	FName SlotName;

	/** The weight curve for this animation section */
	UPROPERTY()
	FMovieSceneFloatChannel Weight;

	/** If on will skip sending animation notifies */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	bool bSkipAnimNotifiers;

	/** If on animation sequence will always play when active even if the animation is controlled by a Blueprint or Anim Instance Class*/
	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bForceCustomMode;

	UPROPERTY()
	float StartOffset_DEPRECATED;

	UPROPERTY()
	float EndOffset_DEPRECATED;
};

/**
 * Movie scene section that control skeletal animation
 */
UCLASS( MinimalAPI )
class UMovieSceneSkeletalAnimationSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation", meta=(ShowOnlyInnerProperties))
	FMovieSceneSkeletalAnimationParams Params;

	/** Get Frame Time as Animation Time*/
	MOVIESCENETRACKS_API float MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const;

protected:

	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime) override;
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
	virtual float GetTotalWeightValue(FFrameTime InTime) const override;
	/** ~UObject interface */
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

private:

	//~ UObject interface

#if WITH_EDITOR

	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	float PreviousPlayRate;

#endif

private:

	UPROPERTY()
	class UAnimSequence* AnimSequence_DEPRECATED;

	UPROPERTY()
	UAnimSequenceBase* Animation_DEPRECATED;

	UPROPERTY()
	float StartOffset_DEPRECATED;
	
	UPROPERTY()
	float EndOffset_DEPRECATED;
	
	UPROPERTY()
	float PlayRate_DEPRECATED;

	UPROPERTY()
	uint32 bReverse_DEPRECATED:1;

	UPROPERTY()
	FName SlotName_DEPRECATED;
};

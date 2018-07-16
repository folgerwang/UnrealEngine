// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "GeometryCacheComponent.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "UObject/SoftObjectPath.h"
#include "MovieSceneGeometryCacheSection.generated.h"

USTRUCT()
struct FMovieSceneGeometryCacheParams
{
	GENERATED_BODY()

	FMovieSceneGeometryCacheParams();

	/** Gets the animation duration, modified by play rate */
	float GetDuration() const { return FMath::IsNearlyZero(PlayRate) || GeometryCache.ResolveObject() == nullptr ? 0.f : Cast<UGeometryCacheComponent>(GeometryCache.ResolveObject())->GetDuration() / PlayRate; }

	/** Gets the animation sequence length, not modified by play rate */
	float GetSequenceLength() const { return GeometryCache.ResolveObject() != nullptr ? Cast<UGeometryCacheComponent>(GeometryCache.ResolveObject())->GetDuration() : 0.f; }

	/** The animation this section plays */
	UPROPERTY(EditAnywhere, Category="GeometryCache", meta=(AllowedClasses = "GeometryCacheComponent"))
	FSoftObjectPath GeometryCache;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(EditAnywhere, Category="GeometryCache")
	float StartOffset;
	
	/** The offset into the end of the animation clip */
	UPROPERTY(EditAnywhere, Category="GeometryCache")
	float EndOffset;
	
	/** The playback rate of the animation clip */
	UPROPERTY(EditAnywhere, Category="GeometryCache")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	uint32 bReverse:1;

};

/**
 * Movie scene section that control geometry cache playback
 */
UCLASS( MinimalAPI )
class UMovieSceneGeometryCacheSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Animation", meta=(ShowOnlyInnerProperties))
	FMovieSceneGeometryCacheParams Params;

	/** Get Frame Time as Animation Time*/
    virtual float MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const;

protected:
	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime) override;
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;

	/** ~UObject interface */
	virtual void Serialize(FArchive& Ar) override;

private:

	//~ UObject interface

#if WITH_EDITOR

	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	float PreviousPlayRate;

#endif


};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "MovieSceneSectionParameters.generated.h"

USTRUCT()
struct FMovieSceneSectionParameters
{
	GENERATED_BODY()

	/** Default constructor */
	FMovieSceneSectionParameters()
		: StartFrameOffset(0)
		, TimeScale(1.0f)
		, HierarchicalBias(100)
		, StartOffset_DEPRECATED(0.f)
		, PrerollTime_DEPRECATED(0.0f)
		, PostrollTime_DEPRECATED(0.0f)
	{}

	/** StartFrameOffset is treated as a FFrameNumber for details customizations but we continue to expose the API publicly as it was before via int32. */
	int32 GetStartFrameOffset() const { return StartFrameOffset.Value; }
	void SetStartFrameOffset(int32 InOffset) { StartFrameOffset = FFrameNumber(InOffset); }

private:

	/** Number of frames to skip at the beginning of the sub-sequence. */
	UPROPERTY(EditAnywhere, Category = "Clipping")
	FFrameNumber StartFrameOffset;

public:

	/** Playback time scaling factor. */
	UPROPERTY(EditAnywhere, Category="Timing")
	float TimeScale;

	/** Hierachical bias. Higher bias will take precedence. */
	UPROPERTY(EditAnywhere, Category="Sequence")
	int32 HierarchicalBias;
	UPROPERTY()
	float StartOffset_DEPRECATED;
	UPROPERTY()
	float PrerollTime_DEPRECATED;
	UPROPERTY()
	float PostrollTime_DEPRECATED;
};

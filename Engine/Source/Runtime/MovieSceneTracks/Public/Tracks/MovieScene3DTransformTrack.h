// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "MovieScene3DTransformTrack.generated.h"

enum class EMovieSceneTransformChannel : uint32;
class UMovieScene3DTransformSection;
struct FMovieSceneInterrogationKey;

struct FTrajectoryKey
{
	struct FData
	{
		FData(UMovieScene3DTransformSection* InSection, TOptional<FKeyHandle> InKeyHandle, ERichCurveInterpMode InInterpMode, FName InChannelName)
			: Section(InSection), KeyHandle(InKeyHandle), ChannelName(InChannelName), InterpMode(InInterpMode)
		{}

		TWeakObjectPtr<UMovieScene3DTransformSection> Section;
		TOptional<FKeyHandle> KeyHandle;
		FName ChannelName;
		ERichCurveInterpMode InterpMode;
	};

	FTrajectoryKey(FFrameNumber InTime) : Time(InTime) {}

	bool Is(ERichCurveInterpMode InInterpMode) const
	{
		if (KeyData.Num())
		{
			for (const FData& Value : KeyData)
			{
				if (Value.InterpMode != InInterpMode)
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}

	FFrameNumber Time;

	TArray<FData, TInlineAllocator<1>> KeyData;
};

/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieScene3DTransformTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;

	/**
	 * Access the interrogation key for transform data - any interrgation data stored with this key is guaranteed to be of type 'FTransform'
	 */
	MOVIESCENETRACKS_API static FMovieSceneInterrogationKey GetInterrogationKey();


#if WITH_EDITOR

	MOVIESCENETRACKS_API TArray<FTrajectoryKey> GetTrajectoryData(FFrameNumber Time, int32 MaxNumDataPoints) const;

#endif
};

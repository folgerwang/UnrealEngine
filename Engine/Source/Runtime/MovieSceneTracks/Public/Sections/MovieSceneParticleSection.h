// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneByteChannel.h"
#include "MovieSceneParticleSection.generated.h"

/**
 * Defines the types of particle keys.
 */
UENUM()
enum class EParticleKey : uint8
{
	Activate   = 0,
	Deactivate = 1,
	Trigger    = 2,
};

USTRUCT()
struct FMovieSceneParticleChannel : public FMovieSceneByteChannel
{
	GENERATED_BODY()

	MOVIESCENETRACKS_API FMovieSceneParticleChannel();
};

template<>
struct TStructOpsTypeTraits<FMovieSceneParticleChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneParticleChannel>
{
	enum { WithStructuredSerializeFromMismatchedTag = true };
};


template<>
struct TMovieSceneChannelTraits<FMovieSceneParticleChannel> : TMovieSceneChannelTraitsBase<FMovieSceneParticleChannel>
{
	enum { SupportsDefaults = false };

#if WITH_EDITOR

	/** Byte channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<uint8> ExtendedEditorDataType;

#endif
};

/**
 * Particle section, for particle toggling and triggering.
 */
UCLASS(MinimalAPI)
class UMovieSceneParticleSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Curve containing the particle keys. */
	UPROPERTY()
	FMovieSceneParticleChannel ParticleKeys;

protected:

	//~ UMovieSceneSection interface
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
};


inline void AssignValue(FMovieSceneParticleChannel* InChannel, FKeyHandle InKeyHandle, EParticleKey InValue)
{
	TMovieSceneChannelData<uint8> ChannelData = InChannel->GetData();
	int32 ValueIndex = ChannelData.GetIndex(InKeyHandle);

	if (ValueIndex != INDEX_NONE)
	{
		ChannelData.GetValues()[ValueIndex] = (uint8)InValue;
	}
}

inline bool EvaluateChannel(const FMovieSceneParticleChannel* InChannel, FFrameTime InTime, EParticleKey& OutValue)
{
	uint8 RawValue = 0;
	if (InChannel->Evaluate(InTime, RawValue))
	{
		OutValue = (EParticleKey)RawValue;
		return true;
	}
	return false;
}
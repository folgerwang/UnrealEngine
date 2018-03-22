// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	MOVIESCENETRACKS_API static uint32 GetChannelID();
};

template<>
struct TStructOpsTypeTraits<FMovieSceneParticleChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneParticleChannel>
{
	enum { WithSerializeFromMismatchedTag = true };
};


template<>
struct TMovieSceneChannelTraits<FMovieSceneParticleChannel>
{
#if WITH_EDITORONLY_DATA

	/** Byte channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<uint8> EditorDataType;

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


inline void AssignValue(FMovieSceneParticleChannel* InChannel, int32 ValueIndex, EParticleKey InValue)
{
	InChannel->GetInterface().GetValues()[ValueIndex] = (uint8)InValue;
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

// Stubbed out functions since FMovieSceneParticleChannel doesn't have an explicit default
inline void SetChannelDefault(FMovieSceneParticleChannel* InChannel, EParticleKey DefaultValue)
{}

inline void ClearChannelDefault(FMovieSceneParticleChannel* InChannel)
{}

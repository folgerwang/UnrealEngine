// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Curves/NameCurve.h"
#include "UObject/StructOnScope.h"
#include "Engine/Engine.h"
#include "UObject/SoftObjectPath.h"
#include "Channels/MovieSceneChannel.h"
#include "MovieSceneEventSection.generated.h"

struct EventData;

USTRUCT()
struct MOVIESCENETRACKS_API FMovieSceneEventParameters
{
	GENERATED_BODY()

	FMovieSceneEventParameters() {}

	/** Construction from a struct type */
	FMovieSceneEventParameters(UScriptStruct& InStruct)
		: StructType(&InStruct)
	{
	}

	MOVIESCENETRACKS_API friend bool operator==(const FMovieSceneEventParameters& A, const FMovieSceneEventParameters& B);

	MOVIESCENETRACKS_API friend bool operator!=(const FMovieSceneEventParameters& A, const FMovieSceneEventParameters& B);

	/**
	 * Access the struct type of this event parameter payload
	 * @return A valid UScriptStruct* or nullptr if the struct is not set, or no longer available
	 */
	UScriptStruct* GetStructType() const
	{
		return Cast<UScriptStruct>(StructType.TryLoad());
	}

	/**
	 * Change the type of this event parameter payload to be the specified struct
	 */
	void Reassign(UScriptStruct* NewStruct)
	{
		StructType = NewStruct;

		if (!NewStruct)
		{
			StructBytes.Reset();
		}
	}

	/**
	 * Retrieve an instance of this payload
	 *
	 * @param OutStruct Structure to receive the instance
	 */
	void GetInstance(FStructOnScope& OutStruct) const;

	/**
	 * Overwrite this payload with another instance of the same type.
	 *
	 * @param InstancePtr A valid pointer to an instance of the type represented by GetStructType
	 */
	void OverwriteWith(uint8* InstancePtr);

	/**
	 * Serialization implementation
	 */
	bool Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FMovieSceneEventParameters& Payload)
	{
		Payload.Serialize(Ar);
		return Ar;
	}

private:

	/** Soft object path to the type of this parameter payload */
	FSoftObjectPath StructType;

	/** Serialized bytes that represent the payload. Serialized internally with FEventParameterArchive */
	TArray<uint8> StructBytes;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneEventParameters> : public TStructOpsTypeTraitsBase2<FMovieSceneEventParameters>
{
	enum 
	{
		WithCopy = true,
		WithSerializer = true
	};
};

USTRUCT()
struct FEventPayload
{
	GENERATED_BODY()

	FEventPayload() {}
	FEventPayload(FName InEventName) : EventName(InEventName) {}

	friend bool operator==(const FEventPayload& A, const FEventPayload& B)
	{
		return A.EventName == B.EventName && A.Parameters == B.Parameters;
	}

	friend bool operator!=(const FEventPayload& A, const FEventPayload& B)
	{
		return A.EventName != B.EventName || A.Parameters != B.Parameters;
	}
	/** The name of the event to trigger */
	UPROPERTY(EditAnywhere, Category=Event)
	FName EventName;

	/** The event parameters */
	UPROPERTY(EditAnywhere, Category=Event, meta=(ShowOnlyInnerProperties))
	FMovieSceneEventParameters Parameters;
};

/** A curve of events */
USTRUCT()
struct FMovieSceneEventSectionData
{
	GENERATED_BODY()

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	MOVIESCENETRACKS_API static uint32 GetChannelID();

	/**
	 * Called after this section data has been serialized to upgrade old data
	 */
	MOVIESCENETRACKS_API void PostSerialize(const FArchive& Ar);

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannel<FEventPayload> GetInterface()
	{
		return TMovieSceneChannel<FEventPayload>(&Times, &KeyValues, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const FEventPayload> GetInterface() const
	{
		return TMovieSceneChannel<const FEventPayload>(&Times, &KeyValues);
	}

	TArrayView<const FFrameNumber> GetKeyTimes() const
	{
		return Times;
	}

	TArrayView<const FEventPayload> GetKeyValues() const
	{
		return KeyValues;
	}

private:

	UPROPERTY()
	TArray<FFrameNumber> Times;

	/** Array of values that correspond to each key time */
	UPROPERTY()
	TArray<FEventPayload> KeyValues;

	FMovieSceneKeyHandleMap KeyHandles;


#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TArray<float> KeyTimes_DEPRECATED;

#endif
};


template<>
struct TStructOpsTypeTraits<FMovieSceneEventSectionData> : public TStructOpsTypeTraitsBase2<FMovieSceneEventSectionData>
{
	enum { WithPostSerialize = true };
};


/**
 * Implements a section in movie scene event tracks.
 */
UCLASS(MinimalAPI)
class UMovieSceneEventSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneEventSection();

public:
	
	// ~UObject interface
	virtual void PostLoad() override;

	/**
	 * Get the section's event data.
	 *
	 * @return Event data.
	 */
	const FMovieSceneEventSectionData& GetEventData() const { return EventData; }

protected:

private:

	UPROPERTY()
	FNameCurve Events_DEPRECATED;

	UPROPERTY()
	FMovieSceneEventSectionData EventData;
};



/** Stub out unnecessary functions */
inline bool EvaluateChannel(const FMovieSceneEventSectionData* InChannel, FFrameTime InTime, FEventPayload& OutValue)
{
	// Can't evaluate event section data in the typical sense
	return false;
}

inline bool ValueExistsAtTime(const FMovieSceneEventSectionData* InChannel, FFrameNumber Time, const FEventPayload& Value, int32 Tolerance = 0)
{
	// true if any value exists
	return InChannel->GetInterface().FindKey(Time, Tolerance) != INDEX_NONE;
}

inline void Optimize(FMovieSceneEventSectionData* InChannel, const FKeyDataOptimizationParams& Params)
{}

inline void SetChannelDefault(FMovieSceneEventSectionData* Channel, const FEventPayload& DefaultValue)
{}

inline void ClearChannelDefault(FMovieSceneEventSectionData* InChannel)
{}
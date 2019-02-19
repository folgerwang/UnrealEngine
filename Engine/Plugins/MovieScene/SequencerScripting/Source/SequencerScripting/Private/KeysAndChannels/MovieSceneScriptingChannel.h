// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "KeyParams.h"
#include "SequencerScriptingRange.h"
#include "SequenceTimeUnit.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"

#include "MovieSceneScriptingChannel.generated.h"

UCLASS(BlueprintType)
class UMovieSceneScriptingKey : public UObject
{
	GENERATED_BODY()

public:
	/**
	* Gets the time for this key from the owning channel.
	* @param TimeUnit (Optional) Should the time be returned in Display Rate frames (possibly with a sub-frame value) or in Tick Resolution with no sub-frame values? Defaults to Display Rate.
	* @return					 The FrameTime of this key which combines both the frame number and the sub-frame it is on. Sub-frame will be zero if you request Tick Resolution.
	*/
	virtual FFrameTime GetTime(ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate) const PURE_VIRTUAL(UMovieSceneScriptingKey::GetTime, return FFrameTime(););
public:
	FKeyHandle KeyHandle;
	TWeakObjectPtr<UMovieSceneSequence> OwningSequence;
};


UCLASS(BlueprintType)
class UMovieSceneScriptingChannel : public UObject
{
	GENERATED_BODY()

public:
	/**
	* Gets all of the keys in this channel.
	* @return	An array of UMovieSceneScriptingKey's contained by this channel.
	*			Returns all keys even if clipped by the owning section's boundaries or outside of the current sequence play range.
	*/
	virtual TArray<UMovieSceneScriptingKey*> GetKeys() const PURE_VIRTUAL(UMovieSceneScriptingChannel::GetKeys, return TArray<UMovieSceneScriptingKey*>(););
};

template<typename ChannelType, typename ScriptingKeyType, typename ScriptingKeyValueType>
struct TMovieSceneScriptingChannel
{
	ScriptingKeyType* AddKeyInChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, TWeakObjectPtr<UMovieSceneSequence> Sequence, const FFrameNumber InTime, ScriptingKeyValueType& NewValue, float SubFrame, ESequenceTimeUnit TimeUnit, EMovieSceneKeyInterpolation Interpolation)
	{
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			ScriptingKeyType* Key = NewObject<ScriptingKeyType>();

			// The Key's time is always going to be in Tick Resolution space, but the user may want to set it via Display Rate, so we convert.
			FFrameNumber KeyTime = InTime;
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				KeyTime = FFrameRate::TransformTime(KeyTime, UMovieSceneSequenceExtensions::GetDisplayRate(Sequence.Get()), UMovieSceneSequenceExtensions::GetTickResolution(Sequence.Get())).RoundToFrame();
			}

			using namespace MovieScene;
			Key->KeyHandle = AddKeyToChannel(Channel, KeyTime, NewValue, Interpolation);
			Key->ChannelHandle = ChannelHandle;
			Key->OwningSequence = Sequence;

			return Key;
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to add key."));
		return nullptr;
	}

	void RemoveKeyFromChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, UMovieSceneScriptingKey* Key)
	{
		if (!Key)
		{
			return;
		}

		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			Channel->DeleteKeys(MakeArrayView(&Key->KeyHandle, 1));
			return;
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to remove key."));
	}

	TArray<UMovieSceneScriptingKey*> GetKeysInChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, TWeakObjectPtr<UMovieSceneSequence> Sequence) const
	{
		TArray<UMovieSceneScriptingKey*> OutScriptingKeys;
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			TArray<FFrameNumber> OutTimes;
			TArray<FKeyHandle> OutKeys;
			Channel->GetKeys(TRange<FFrameNumber>(), &OutTimes, &OutKeys);

			for (int32 i = 0; i < OutTimes.Num(); i++)
			{
				ScriptingKeyType* Key = NewObject<ScriptingKeyType>();
				Key->KeyHandle = OutKeys[i];
				Key->ChannelHandle = ChannelHandle;
				Key->OwningSequence = Sequence;
				OutScriptingKeys.Add(Key);
			}
		}
		else
		{
			UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to get keys."));
		}

		return OutScriptingKeys;
	}

	TArray<ScriptingKeyValueType> EvaluateKeysInChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, TWeakObjectPtr<UMovieSceneSequence> Sequence, FSequencerScriptingRange ScriptingRange, FFrameRate FrameRate) const
	{
		TArray<ScriptingKeyValueType> OutValues;
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			UMovieSceneSequence* MovieSceneSequence = Sequence.Get();
			FFrameRate Resolution = UMovieSceneSequenceExtensions::GetTickResolution(MovieSceneSequence);
			TRange<FFrameNumber> SpecifiedRange = ScriptingRange.ToNative(Resolution);
			if (SpecifiedRange.HasLowerBound() && SpecifiedRange.HasUpperBound())
			{
				FFrameTime Interval = FFrameRate::TransformTime(1, FrameRate, Resolution);
				FFrameNumber InFrame = MovieScene::DiscreteInclusiveLower(SpecifiedRange);
				FFrameNumber OutFrame = MovieScene::DiscreteExclusiveUpper(SpecifiedRange);
				for (FFrameTime EvalTime = InFrame; EvalTime < OutFrame; EvalTime += Interval)
				{
					FFrameNumber KeyTime = FFrameRate::Snap(EvalTime, Resolution, FrameRate).FloorToFrame();
					Channel->Evaluate(KeyTime, OutValues.Emplace_GetRef());
				}
			}
			else
			{
				UE_LOG(LogMovieScene, Error, TEXT("Unbounded range passed to evaluate keys."));
			}
		}
		else
		{
			UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to evaluate keys."));
		}

		return OutValues;
	}

	FSequencerScriptingRange ComputeEffectiveRangeInChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, TWeakObjectPtr<UMovieSceneSequence> Sequence) const
	{
		FSequencerScriptingRange ScriptingRange;
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			ScriptingRange = FSequencerScriptingRange::FromNative(Channel->ComputeEffectiveRange(), UMovieSceneSequenceExtensions::GetTickResolution(Sequence.Get()));
		}
		else
		{
			UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to get effective range."));
		}

		return ScriptingRange;
	}

	void SetDefaultInChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, ScriptingKeyValueType& InDefaultValue)
	{
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			using namespace MovieScene;
			SetChannelDefault(Channel, InDefaultValue);
			return;
		}
		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to set default value."));
	}

	void RemoveDefaultFromChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle)
	{
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			using namespace MovieScene;
			RemoveChannelDefault(Channel);
			return;
		}
		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to remove default value."));
	}

	TOptional<ScriptingKeyValueType> GetDefaultFromChannel(TMovieSceneChannelHandle<ChannelType> ChannelHandle) const
	{
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			ScriptingKeyValueType Ret;

			using namespace MovieScene;
			if (GetChannelDefault(Channel, Ret))
			{
				return TOptional<ScriptingKeyValueType>(Ret);
			}

			return TOptional<ScriptingKeyValueType>();
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to get default value."));
		return TOptional<ScriptingKeyValueType>();
	}
};

/** 
* The existing Sequencer code is heavily template-based. We cannot create templated UObjects nor create UFUNCTIONS out template functions.
* This template class serves as a way to minimize boilerplate code when creating UObject versions of the Sequencer key data.
*/
template<typename ChannelType, typename ChannelDataType>
struct TMovieSceneScriptingKey
{
	FFrameTime GetTimeFromChannel(FKeyHandle KeyHandle, TWeakObjectPtr<UMovieSceneSequence> Sequence, ESequenceTimeUnit TimeUnit) const
	{
		if (!Sequence.IsValid())
		{
			UE_LOG(LogMovieScene, Error, TEXT("GetTime called with an invalid owning sequence."));
			return FFrameNumber(0);
		}

		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			FFrameNumber KeyTime;
			Channel->GetKeyTime(KeyHandle, KeyTime);

			// The KeyTime is always going to be in Tick Resolution space, but the user may desire it in Play Rate /w a Subframe.
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				FFrameTime DisplayRateTime = FFrameRate::TransformTime(KeyTime, UMovieSceneSequenceExtensions::GetTickResolution(Sequence.Get()), UMovieSceneSequenceExtensions::GetDisplayRate(Sequence.Get()));
				return DisplayRateTime;
			}

			// Tick Resolution has no sub-frame support.
			return FFrameTime(KeyTime, 0.f);
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingKey, failed to retrieve Time."));
		return FFrameTime();
	}

	void SetTimeInChannel(FKeyHandle KeyHandle, TWeakObjectPtr<UMovieSceneSequence> Sequence, const FFrameNumber NewFrameNumber, ESequenceTimeUnit TimeUnit, float SubFrame)
	{
		if (!Sequence.IsValid())
		{
			UE_LOG(LogMovieScene, Error, TEXT("SetTime called with an invalid owning sequence."));
			return;
		}

		// Clamp sub-frames to 0-1
		SubFrame = FMath::Clamp(SubFrame, 0.f, FFrameTime::MaxSubframe);

		// TickResolution doesn't support a sub-frame as you can't get finer detailed than that.
		if (TimeUnit == ESequenceTimeUnit::TickResolution && SubFrame > 0.f)
		{
			UE_LOG(LogMovieScene, Warning, TEXT("SetTime called with a SubFrame specified for a Tick Resolution type time! SubFrames are only allowed for Display Rate types, ignoring..."));
			SubFrame = 0.f;
		}

		FFrameNumber KeyFrameNumber = NewFrameNumber;
		
		// Keys are always stored in Tick Resolution so we need to potentially convert their values.
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
		{
			KeyFrameNumber = FFrameRate::TransformTime(FFrameTime(NewFrameNumber, SubFrame), UMovieSceneSequenceExtensions::GetDisplayRate(Sequence.Get()), UMovieSceneSequenceExtensions::GetTickResolution(Sequence.Get())).RoundToFrame();
		}

		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			Channel->SetKeyTime(KeyHandle, KeyFrameNumber);
			return;
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingKey, failed to set Time."));
	}
	
	ChannelDataType GetValueFromChannel(FKeyHandle KeyHandle) const
	{
		ChannelDataType Value = ChannelDataType();

		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			using namespace MovieScene;
			if (!GetKeyValue(Channel, KeyHandle, Value))
			{
				UE_LOG(LogMovieScene, Error, TEXT("Invalid KeyIndex for MovieSceneScriptingKey, failed to get value. Did you forget to create the key through the channel?"));
				return Value;
			}

			return Value;
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingKey, failed to get value. Did you forget to create the key through the channel?"));
		return Value;
	}

	void SetValueInChannel(FKeyHandle KeyHandle, ChannelDataType InNewValue)
	{
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			using namespace MovieScene;
			if(!AssignValue(Channel, KeyHandle, InNewValue))
			{
				UE_LOG(LogMovieScene, Error, TEXT("Invalid KeyIndex for MovieSceneScriptingKey, failed to set value. Did you forget to create the key through the channel?"));
				return;
			}
		}

		UE_LOG(LogMovieScene, Error, TEXT("Invalid ChannelHandle for MovieSceneScriptingKey, failed to set value. Did you forget to create the key through the channel?"));
	}

public:
	TMovieSceneChannelHandle<ChannelType> ChannelHandle;
};
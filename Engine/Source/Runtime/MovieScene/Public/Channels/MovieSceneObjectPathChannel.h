// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameNumber.h"
#include "MovieSceneChannel.h"
#include "MovieSceneChannelData.h"
#include "MovieSceneChannelTraits.h"
#include "MovieSceneObjectPathChannel.generated.h"


USTRUCT()
struct MOVIESCENE_API FMovieSceneObjectPathChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	/** Default constructor */
	FMovieSceneObjectPathChannel()
		: PropertyClass(nullptr)
	{}

	FORCEINLINE void SetPropertyClass(UClass* InPropertyClass)
	{
		PropertyClass = InPropertyClass;
	}

	FORCEINLINE UClass* GetPropertyClass() const
	{
		return PropertyClass;
	}

	/**
	 * Access a mutable interface for this channel's data
	 *
	 * @return An object that is able to manipulate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<TSoftObjectPtr<>> GetData()
	{
		return TMovieSceneChannelData<TSoftObjectPtr<>>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel's data
	 *
	 * @return An object that is able to interrogate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const TSoftObjectPtr<>> GetData() const
	{
		return TMovieSceneChannelData<const TSoftObjectPtr<>>(&Times, &Values);
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @param OutValue   A value to receive the result
	 * @return true if the channel was evaluated successfully, false otherwise
	 */
	bool Evaluate(FFrameTime InTime, TSoftObjectPtr<>& OutValue) const;
	bool Evaluate(FFrameTime InTime, UObject*& OutValue) const;

public:

	//~ FMovieSceneChannel Interface
	virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) override;
	virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) override;
	virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) override;
	virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) override;
	virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles) override;
	virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	virtual TRange<FFrameNumber> ComputeEffectiveRange() const override;
	virtual int32 GetNumKeys() const override;
	virtual void Reset() override;
	virtual void Offset(FFrameNumber DeltaPosition) override;
	virtual void Optimize(const FKeyDataOptimizationParams& InParameters) override;
	virtual void ClearDefault() override;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(const TSoftObjectPtr<>& InDefaultValue)
	{
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE const TSoftObjectPtr<>& GetDefault() const
	{
		return DefaultValue;
	}

	/**
	 * Remove this channel's default value causing the channel to have no effect where no keys are present
	 */
	FORCEINLINE void RemoveDefault()
	{
		DefaultValue.Reset();
	}

private:

	UPROPERTY()
	UClass* PropertyClass;

	UPROPERTY(meta=(KeyTimes))
	TArray<FFrameNumber> Times;

	UPROPERTY(meta=(KeyValues))
	TArray<TSoftObjectPtr<UObject>> Values;

	UPROPERTY()
	TSoftObjectPtr<UObject> DefaultValue;

	FMovieSceneKeyHandleMap KeyHandles;
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneObjectPathChannel> : TMovieSceneChannelTraitsBase<FMovieSceneObjectPathChannel>
{
#if WITH_EDITOR

	/** Integer channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<UObject*> ExtendedEditorDataType;

#endif
};
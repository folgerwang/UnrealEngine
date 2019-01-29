// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Misc/FrameNumber.h"
#include "MovieSceneChannel.h"
#include "MovieSceneChannelData.h"
#include "MovieSceneChannelTraits.h"
#include "KeyParams.h"

#include "Curves/RichCurve.h"

#include "MovieSceneFloatChannel.generated.h"

USTRUCT()
struct FMovieSceneTangentData
{
	GENERATED_BODY()

	FMovieSceneTangentData()
		: ArriveTangent(0.f)
		, LeaveTangent(0.f)
		, TangentWeightMode(RCTWM_WeightedNone)
		, ArriveTangentWeight(0.f)
		, LeaveTangentWeight(0.f)
	{}

	bool Serialize(FArchive& Ar);
	bool operator==(const FMovieSceneTangentData& Other) const;
	bool operator!=(const FMovieSceneTangentData& Other) const;
	friend FArchive& operator<<(FArchive& Ar, FMovieSceneTangentData& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	/** If RCIM_Cubic, the arriving tangent at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float ArriveTangent;

	/** If RCIM_Cubic, the leaving tangent at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float LeaveTangent;

	/** If RCIM_Cubic, the tangent weight mode */
	UPROPERTY(EditAnywhere, Category = "Key")
	TEnumAsByte<ERichCurveTangentWeightMode> TangentWeightMode;

	/** If RCTWM_WeightedArrive or RCTWM_WeightedBoth, the weight of the left tangent */
	UPROPERTY(EditAnywhere, Category="Key")
	float ArriveTangentWeight;

	/** If RCTWM_WeightedLeave or RCTWM_WeightedBoth, the weight of the right tangent */
	UPROPERTY(EditAnywhere, Category="Key")
	float LeaveTangentWeight;

};

template<>
struct TIsPODType<FMovieSceneTangentData>
{
	enum { Value = true };
};


template<>
struct TStructOpsTypeTraits<FMovieSceneTangentData>
	: public TStructOpsTypeTraitsBase2<FMovieSceneTangentData>
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};

USTRUCT()
struct FMovieSceneFloatValue
{
	GENERATED_BODY()

	FMovieSceneFloatValue()
		: Value(0.f), InterpMode(RCIM_Cubic), TangentMode(RCTM_Auto)
	{}

	explicit FMovieSceneFloatValue(float InValue)
		: Value(InValue), InterpMode(RCIM_Cubic), TangentMode(RCTM_Auto)
	{}

	bool Serialize(FArchive& Ar);
	bool operator==(const FMovieSceneFloatValue& Other) const;
	bool operator!=(const FMovieSceneFloatValue& Other) const;
	friend FArchive& operator<<(FArchive& Ar, FMovieSceneFloatValue& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	UPROPERTY(EditAnywhere, Category="Key")
	float Value;

	UPROPERTY(EditAnywhere, Category="Key")
	TEnumAsByte<ERichCurveInterpMode> InterpMode;

	UPROPERTY(EditAnywhere, Category="Key")
	TEnumAsByte<ERichCurveTangentMode> TangentMode;

	UPROPERTY(EditAnywhere, Category="Key")
	FMovieSceneTangentData Tangent;
};

template<>
struct TIsPODType<FMovieSceneFloatValue>
{
	enum { Value = true };
};


template<>
struct TStructOpsTypeTraits<FMovieSceneFloatValue>
	: public TStructOpsTypeTraitsBase2<FMovieSceneFloatValue>
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};

USTRUCT()
struct MOVIESCENE_API FMovieSceneFloatChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	FMovieSceneFloatChannel()
		: PreInfinityExtrap(RCCE_Constant), PostInfinityExtrap(RCCE_Constant), DefaultValue(), bHasDefaultValue(false)
	{}

	/**
	 * Access a mutable interface for this channel's data
	 *
	 * @return An object that is able to manipulate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<FMovieSceneFloatValue> GetData()
	{
		return TMovieSceneChannelData<FMovieSceneFloatValue>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel's data
	 *
	 * @return An object that is able to interrogate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const FMovieSceneFloatValue> GetData() const
	{
		return TMovieSceneChannelData<const FMovieSceneFloatValue>(&Times, &Values);
	}

	/**
	 * Const access to this channel's times
	 */
	FORCEINLINE TArrayView<const FFrameNumber> GetTimes() const
	{
		return Times;
	}

	/**
	 * Const access to this channel's values
	 */
	FORCEINLINE TArrayView<const FMovieSceneFloatValue> GetValues() const
	{
		return Values;
	}




	/**
	* Evaluate this channel with the frame resolution 
	*
	* @param InTime     The time to evaluate at
	* @param InTime     The Frame Rate the time to evaluate at
	* @param OutValue   A value to receive the result
	* @return true if the channel was evaluated successfully, false otherwise
	*/
	bool Evaluate(FFrameTime InTime, float& OutValue) const;



	/**
	 * Set the channel's times and values to the requested values
	 */
	FORCEINLINE void Set(TArray<FFrameNumber> InTimes, TArray<FMovieSceneFloatValue> InValues)
	{
		check(InTimes.Num() == InValues.Num());

		Times = MoveTemp(InTimes);
		Values = MoveTemp(InValues);

		KeyHandles.Reset();
		for (int32 Index = 0; Index < Times.Num(); ++Index)
		{
			KeyHandles.AllocateHandle(Index);
		}
	}

public:

	// ~ FMovieSceneChannel Interface
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
	virtual void PostEditChange() override;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(float InDefaultValue)
	{
		bHasDefaultValue = true;
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE TOptional<float> GetDefault() const
	{
		return bHasDefaultValue ? TOptional<float>(DefaultValue) : TOptional<float>();
	}

	/**
	 * Remove this channel's default value causing the channel to have no effect where no keys are present
	 */
	FORCEINLINE void RemoveDefault()
	{
		bHasDefaultValue = false;
	}

public:

	bool Serialize(FArchive& Ar);
	void PostSerialize(const FArchive& Ar);
	friend FArchive& operator<<(FArchive& Ar, FMovieSceneFloatChannel& Me)
	{
		Me.Serialize(Ar);
		return Ar;
	}


	/** Serialize this float function from a mismatching property tag (FRichCurve) */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	int32 AddConstantKey(FFrameNumber InTime, float InValue);

	int32 AddLinearKey(FFrameNumber InTime, float InValue);

	int32 AddCubicKey(FFrameNumber InTime, float InValue, ERichCurveTangentMode TangentMode = RCTM_Auto, const FMovieSceneTangentData& Tangent = FMovieSceneTangentData());

	void AutoSetTangents(float Tension = 0.f);

	/**
	 * Populate the specified array with times and values that represent the smooth interpolation of this channel across the specified range
	 *
	 * @param StartTimeSeconds      The first time in seconds to include in the resulting array
	 * @param EndTimeSeconds        The last time in seconds to include in the resulting array
	 * @param TimeThreshold         A small time threshold in seconds below which we should stop adding new points
	 * @param ValueThreshold        A small value threshold below which we should stop adding new points where the linear interpolation would suffice
	 * @param TickResolution        The tick resolution with which to interpret this channel's times
	 * @param InOutPoints           An array to populate with the evaluated points
	 */
	void PopulateCurvePoints(double StartTimeSeconds, double EndTimeSeconds, double TimeThreshold, float ValueThreshold, FFrameRate TickResolution, TArray<TTuple<double, double>>& InOutPoints) const;	

	/**
	* Add keys with these times to channel. The number of elements in both arrays much match or nothing is added.
	* Also assume that the times are greater than last time in the channel and are increasing. If not bad things can happen.
	* @param InTimes Times to add
	* @param InValues Values to add
	*/
	void AddKeys(const TArray<FFrameNumber>& InTimes, const TArray<FMovieSceneFloatValue>& InValues);
public:

	/** Pre-infinity extrapolation state */
	UPROPERTY()
	TEnumAsByte<ERichCurveExtrapolation> PreInfinityExtrap;

	/** Post-infinity extrapolation state */
	UPROPERTY()
	TEnumAsByte<ERichCurveExtrapolation> PostInfinityExtrap;

private:

	int32 InsertKeyInternal(FFrameNumber InTime);

	/**
	 * Evaluate this channel's extrapolation. Assumes more than 1 key is present.
	 *
	 * @param InTime     The time to evaluate at
	 * @param OutValue   A value to receive the result
	 * @return true if the time was evaluated with extrapolation, false otherwise
	 */
	bool EvaluateExtrapolation(FFrameTime InTime, float& OutValue) const;

	/**
	 * Adds median points between each of the supplied points if their evaluated value is significantly different than the linear interpolation of those points
	 *
	 * @param TickResolution        The tick resolution with which to interpret this channel's times
	 * @param TimeThreshold         A small time threshold in seconds below which we should stop adding new points
	 * @param ValueThreshold        A small value threshold below which we should stop adding new points where the linear interpolation would suffice
	 * @param InOutPoints           An array to populate with the evaluated points
	 */
	void RefineCurvePoints(FFrameRate TickResolution, double TimeThreshold, float ValueThreshold, TArray<TTuple<double, double>>& InOutPoints) const;

	UPROPERTY(meta=(KeyTimes))
	TArray<FFrameNumber> Times;

	UPROPERTY(meta=(KeyValues))
	TArray<FMovieSceneFloatValue> Values;

	UPROPERTY()
	float DefaultValue;

	UPROPERTY()
	bool bHasDefaultValue;

	/** This needs to be a UPROPERTY so it gets saved into editor transactions but transient so it doesn't get saved into assets. */
	UPROPERTY(Transient)
	FMovieSceneKeyHandleMap KeyHandles;

	UPROPERTY()
	FFrameRate TickResolution;

public:
	//Set it's frame resolution
	void SetTickResolution(FFrameRate InTickSolution)
	{
		TickResolution = InTickSolution;
	}
};

template<>
struct TStructOpsTypeTraits<FMovieSceneFloatChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneFloatChannel>
{
	enum 
	{ 
		WithStructuredSerializeFromMismatchedTag = true, 
	    WithSerializer = true,
		WithPostSerialize = true,
    };
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneFloatChannel> : TMovieSceneChannelTraitsBase<FMovieSceneFloatChannel>
{
#if WITH_EDITOR

	/** Float channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<float> ExtendedEditorDataType;

#endif
};

inline bool ValueExistsAtTime(const FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, float Value)
{
	const FFrameTime FrameTime(InFrameNumber);

	float ExistingValue = 0.f;
	return Channel->Evaluate(FrameTime, ExistingValue) && FMath::IsNearlyEqual(ExistingValue, Value, KINDA_SMALL_NUMBER);
}

inline bool ValueExistsAtTime(const FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, const FMovieSceneFloatValue& InValue)
{
	return ValueExistsAtTime(Channel, InFrameNumber, InValue.Value);
}

inline void AssignValue(FMovieSceneFloatChannel* InChannel, FKeyHandle InKeyHandle, float InValue)
{
	TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = InChannel->GetData();
	int32 ValueIndex = ChannelData.GetIndex(InKeyHandle);

	if (ValueIndex != INDEX_NONE)
	{
		ChannelData.GetValues()[ValueIndex].Value = InValue;
	}
}

/**
 * Overload for adding a new key to a float channel at a given time. See MovieScene::AddKeyToChannel for default implementation.
 */
MOVIESCENE_API FKeyHandle AddKeyToChannel(FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, float InValue, EMovieSceneKeyInterpolation Interpolation);


/**
 * Overload for dilating float channel data. See MovieScene::Dilate for default implementation.
 */
MOVIESCENE_API void Dilate(FMovieSceneFloatChannel* InChannel, FFrameNumber Origin, float DilationFactor);


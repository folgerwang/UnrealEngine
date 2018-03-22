// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "FrameNumber.h"
#include "MovieSceneChannel.h"
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
	{}

	/** If RCIM_Cubic, the arriving tangent at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float ArriveTangent;

	/** If RCIM_Cubic, the leaving tangent at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float LeaveTangent;
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

	UPROPERTY(EditAnywhere, Category="Key")
	float Value;

	UPROPERTY(EditAnywhere, Category="Key")
	TEnumAsByte<ERichCurveInterpMode> InterpMode;

	UPROPERTY(EditAnywhere, Category="Key")
	TEnumAsByte<ERichCurveTangentMode> TangentMode;

	UPROPERTY(EditAnywhere, Category="Key")
	FMovieSceneTangentData Tangent;
};

USTRUCT()
struct FMovieSceneFloatChannel
{
	GENERATED_BODY()

	FMovieSceneFloatChannel()
		: PreInfinityExtrap(RCCE_Constant), PostInfinityExtrap(RCCE_Constant), DefaultValue(), bHasDefaultValue(false)
	{}

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	MOVIESCENE_API static uint32 GetChannelID();

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannel<FMovieSceneFloatValue> GetInterface()
	{
		return TMovieSceneChannel<FMovieSceneFloatValue>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const FMovieSceneFloatValue> GetInterface() const
	{
		return TMovieSceneChannel<const FMovieSceneFloatValue>(&Times, &Values);
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
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @param OutValue   A value to receive the result
	 * @return true if the channel was evaluated successfully, false otherwise
	 */
	MOVIESCENE_API bool Evaluate(FFrameTime InTime, float& OutValue) const;

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

	/** Serialize this float function from a mismatching property tag (FRichCurve) */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	MOVIESCENE_API int32 AddConstantKey(FFrameNumber InTime, float InValue);

	MOVIESCENE_API int32 AddLinearKey(FFrameNumber InTime, float InValue);

	MOVIESCENE_API int32 AddCubicKey(FFrameNumber InTime, float InValue, ERichCurveTangentMode TangentMode = RCTM_Auto, const FMovieSceneTangentData& Tangent = FMovieSceneTangentData());

	MOVIESCENE_API void AutoSetTangents(float Tension = 0.f);

	/**
	 * Populate the specified array with times and values that represent the smooth interpolation of this channel across the specified range
	 *
	 * @param StartTimeSeconds      The first time in seconds to include in the resulting array
	 * @param EndTimeSeconds        The last time in seconds to include in the resulting array
	 * @param TimeThreshold         A small time threshold in seconds below which we should stop adding new points
	 * @param ValueThreshold        A small value threshold below which we should stop adding new points where the linear interpolation would suffice
	 * @param FrameResolution       The frame resolution with which to interpret this channel's times
	 * @param InOutPoints           An array to populate with the evaluated points
	 */
	MOVIESCENE_API void PopulateCurvePoints(double StartTimeSeconds, double EndTimeSeconds, double TimeThreshold, float ValueThreshold, FFrameRate FrameResolution, TArray<TTuple<double, double>>& InOutPoints) const;	

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
	 * @param FrameResolution       The frame resolution with which to interpret this channel's times
	 * @param TimeThreshold         A small time threshold in seconds below which we should stop adding new points
	 * @param ValueThreshold        A small value threshold below which we should stop adding new points where the linear interpolation would suffice
	 * @param InOutPoints           An array to populate with the evaluated points
	 */
	void RefineCurvePoints(FFrameRate FrameResolution, double TimeThreshold, float ValueThreshold, TArray<TTuple<double, double>>& InOutPoints) const;

	UPROPERTY()
	TArray<FFrameNumber> Times;

	UPROPERTY()
	TArray<FMovieSceneFloatValue> Values;

	UPROPERTY()
	float DefaultValue;

	UPROPERTY()
	bool bHasDefaultValue;

	FMovieSceneKeyHandleMap KeyHandles;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneFloatChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneFloatChannel>
{
	enum { WithSerializeFromMismatchedTag = true };
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneFloatChannel>
{
#if WITH_EDITORONLY_DATA

	/** Float channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<float> EditorDataType;

#endif
};

inline bool EvaluateChannel(const FMovieSceneFloatChannel* InChannel, float InTime, FMovieSceneFloatValue& OutValue)
{
	checkf(false, TEXT("Unimplemented: Please use EvaluateChannel(const FMovieSceneFloatChannel&, float, float&) instead."));
	return false;
}

inline bool ValueExistsAtTime(const FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, float Value, float Tolerance = KINDA_SMALL_NUMBER)
{
	const FFrameTime FrameTime(InFrameNumber);

	float ExistingValue = 0.f;
	return Channel->Evaluate(FrameTime, ExistingValue) && FMath::IsNearlyEqual(ExistingValue, Value, Tolerance);
}

inline bool ValueExistsAtTime(const FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, const FMovieSceneFloatValue& InValue, float Tolerance = KINDA_SMALL_NUMBER)
{
	return ValueExistsAtTime(Channel, InFrameNumber, InValue.Value, Tolerance);
}

inline void AssignValue(FMovieSceneFloatChannel* InChannel, int32 ValueIndex, float InValue)
{
	InChannel->GetInterface().GetValues()[ValueIndex].Value = InValue;
}

/**
 * Overload for adding a new key to a float channel at a given time. See MovieScene::AddKeyToChannel for default implementation.
 */
MOVIESCENE_API FKeyHandle AddKeyToChannel(FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, float InValue, EMovieSceneKeyInterpolation Interpolation);


/**
 * Overload for optimizing key data for a float channel (removing superfluous keys). See MovieScene::Optimize for default implementation.
 */
MOVIESCENE_API void Optimize(FMovieSceneFloatChannel* InChannel, const FKeyDataOptimizationParams& Params);


/**
 * Overload for dilating float channel data. See MovieScene::Dilate for default implementation.
 */
MOVIESCENE_API void Dilate(FMovieSceneFloatChannel* InChannel, FFrameNumber Origin, float DilationFactor);


/**
 * Overload for Converting a float channel from one frame rate to another. Ensures that both key times and tangents are migrated correctly. See MovieScene::ChangeFrameResolution for default implementation
 */
MOVIESCENE_API void ChangeFrameResolution(FMovieSceneFloatChannel* InChannel, FFrameRate SourceRate, FFrameRate DestinationRate);
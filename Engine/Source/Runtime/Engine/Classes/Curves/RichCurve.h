// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Curves/KeyHandle.h"
#include "Curves/RealCurve.h"
#include "RichCurve.generated.h"

/** If using RCIM_Cubic, this enum describes how the tangents should be controlled in editor. */
UENUM(BlueprintType)
enum ERichCurveTangentMode
{
	/** Automatically calculates tangents to create smooth curves between values. */
	RCTM_Auto UMETA(DisplayName="Auto"),
	/** User specifies the tangent as a unified tangent where the two tangents are locked to each other, presenting a consistent curve before and after. */
	RCTM_User UMETA(DisplayName="User"),
	/** User specifies the tangent as two separate broken tangents on each side of the key which can allow a sharp change in evaluation before or after. */
	RCTM_Break UMETA(DisplayName="Break"),
	/** No tangents. */
	RCTM_None UMETA(Hidden)
};


/** Enumerates tangent weight modes. */
UENUM(BlueprintType)
enum ERichCurveTangentWeightMode
{
	/** Don't take tangent weights into account. */
	RCTWM_WeightedNone UMETA(DisplayName="None"),
	/** Only take the arrival tangent weight into account for evaluation. */
	RCTWM_WeightedArrive UMETA(DisplayName="Arrive"),
	/** Only take the leaving tangent weight into account for evaluation. */
	RCTWM_WeightedLeave UMETA(DisplayName="Leave"),
	/** Take both the arrival and leaving tangent weights into account for evaluation. */
	RCTWM_WeightedBoth UMETA(DisplayName="Both")
};

/** One key in a rich, editable float curve */
USTRUCT()
struct ENGINE_API FRichCurveKey
{
	GENERATED_USTRUCT_BODY()

	/** Interpolation mode between this key and the next */
	UPROPERTY()
	TEnumAsByte<ERichCurveInterpMode> InterpMode;

	/** Mode for tangents at this key */
	UPROPERTY()
	TEnumAsByte<ERichCurveTangentMode> TangentMode;

	/** If either tangent at this key is 'weighted' */
	UPROPERTY()
	TEnumAsByte<ERichCurveTangentWeightMode> TangentWeightMode;

	/** Time at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float Time;

	/** Value at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float Value;

	/** If RCIM_Cubic, the arriving tangent at this key */
	UPROPERTY()
	float ArriveTangent;

	/** If RCTWM_WeightedArrive or RCTWM_WeightedBoth, the weight of the left tangent */
	UPROPERTY()
	float ArriveTangentWeight;

	/** If RCIM_Cubic, the leaving tangent at this key */
	UPROPERTY()
	float LeaveTangent;

	/** If RCTWM_WeightedLeave or RCTWM_WeightedBoth, the weight of the right tangent */
	UPROPERTY()
	float LeaveTangentWeight;

	FRichCurveKey()
		: InterpMode(RCIM_Linear)
		, TangentMode(RCTM_Auto)
		, TangentWeightMode(RCTWM_WeightedNone)
		, Time(0.f)
		, Value(0.f)
		, ArriveTangent(0.f)
		, ArriveTangentWeight(0.f)
		, LeaveTangent(0.f)
		, LeaveTangentWeight(0.f)
	{ }

	FRichCurveKey(float InTime, float InValue)
		: InterpMode(RCIM_Linear)
		, TangentMode(RCTM_Auto)
		, TangentWeightMode(RCTWM_WeightedNone)
		, Time(InTime)
		, Value(InValue)
		, ArriveTangent(0.f)
		, ArriveTangentWeight(0.f)
		, LeaveTangent(0.f)
		, LeaveTangentWeight(0.f)
	{ }

	FRichCurveKey(float InTime, float InValue, float InArriveTangent, const float InLeaveTangent, ERichCurveInterpMode InInterpMode)
		: InterpMode(InInterpMode)
		, TangentMode(RCTM_Auto)
		, TangentWeightMode(RCTWM_WeightedNone)
		, Time(InTime)
		, Value(InValue)
		, ArriveTangent(InArriveTangent)
		, ArriveTangentWeight(0.f)
		, LeaveTangent(InLeaveTangent)
		, LeaveTangentWeight(0.f)
	{ }

	/** Conversion constructor */
	FRichCurveKey(const FInterpCurvePoint<float>& InPoint);
	FRichCurveKey(const FInterpCurvePoint<FVector>& InPoint, int32 ComponentIndex);

	/** ICPPStructOps interface */
	bool Serialize(FArchive& Ar);
	bool operator==(const FRichCurveKey& Other) const;
	bool operator!=(const FRichCurveKey& Other) const;

	friend FArchive& operator<<(FArchive& Ar, FRichCurveKey& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};


template<>
struct TIsPODType<FRichCurveKey>
{
	enum { Value = true };
};


template<>
struct TStructOpsTypeTraits<FRichCurveKey>
	: public TStructOpsTypeTraitsBase2<FRichCurveKey>
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};


/** A rich, editable float curve */
USTRUCT()
struct ENGINE_API FRichCurve
	: public FRealCurve
{
	GENERATED_USTRUCT_BODY()

public:

	/** Gets a copy of the keys, so indices and handles can't be meddled with */
	TArray<FRichCurveKey> GetCopyOfKeys() const;

	/** Gets a const reference of the keys, so indices and handles can't be meddled with */
	const TArray<FRichCurveKey>& GetConstRefOfKeys() const;

	/** Const iterator for the keys, so the indices and handles stay valid */
	TArray<FRichCurveKey>::TConstIterator GetKeyIterator() const;
	
	/** Functions for getting keys based on handles */
	FRichCurveKey& GetKey(FKeyHandle KeyHandle);
	FRichCurveKey GetKey(FKeyHandle KeyHandle) const;
	
	/** Quick accessors for the first and last keys */
	FRichCurveKey GetFirstKey() const;
	FRichCurveKey GetLastKey() const;

	/** Get the first key that matches any of the given key handles. */
	FRichCurveKey* GetFirstMatchingKey(const TArray<FKeyHandle>& KeyHandles);

	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param	bUnwindRotation		When true, the value will be treated like a rotation value in degrees, and will automatically be unwound to prevent flipping 360 degrees from the previous key 
	  * @param  KeyHandle			Optionally can specify what handle this new key should have, otherwise, it'll make a new one
	  */
	virtual FKeyHandle AddKey(float InTime, float InValue, const bool bUnwindRotation = false, FKeyHandle KeyHandle = FKeyHandle()) final override;

	/**
	 * Sets the keys with the keys.
	 *
	 * Expects that the keys are already sorted.
	 *
	 * @see AddKey, DeleteKey
	 */
	void SetKeys(const TArray<FRichCurveKey>& InKeys);

	/**
	 *  Remove the specified key from the curve.
	 *
	 * @param KeyHandle The handle of the key to remove.
	 * @see AddKey, SetKeys
	 */
	void DeleteKey(FKeyHandle KeyHandle) final override;

	/** Finds the key at InTime, and updates its value. If it can't find the key within the KeyTimeTolerance, it adds one at that time */
	virtual FKeyHandle UpdateOrAddKey(float InTime, float InValue, const bool bUnwindRotation = false, float KeyTimeTolerance = KINDA_SMALL_NUMBER) final override;

	/** Move a key to a new time. */
	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewTime) final override;

	/** Get the time for the Key with the specified index. */
	virtual float GetKeyTime(FKeyHandle KeyHandle) const final override;

	/** Set the value of the specified key */
	virtual void SetKeyValue(FKeyHandle KeyHandle, float NewValue, bool bAutoSetTangents = true) final override;

	/** Returns the value of the specified key */
	virtual float GetKeyValue(FKeyHandle KeyHandle) const final override;

	/** Returns a <Time, Value> pair for the specified key */
	virtual TPair<float, float> GetKeyTimeValuePair(FKeyHandle KeyHandle) const final override;

	/** Set the interp mode of the specified key */
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode) final override;

	/** Set the tangent mode of the specified key */
	void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode NewTangentMode);

	/** Set the tangent weight mode of the specified key */
	void SetKeyTangentWeightMode(FKeyHandle KeyHandle, ERichCurveTangentWeightMode NewTangentWeightMode);

	/** Get the interp mode of the specified key */
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const final override;

	/** Get the tangent mode of the specified key */
	ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const;

	/** Get range of input time values. Outside this region curve continues constantly the start/end values. */
	virtual void GetTimeRange(float& MinTime, float& MaxTime) const final override;

	/** Get range of output values. */
	virtual void GetValueRange(float& MinValue, float& MaxValue) const final override;

	/** Clear all keys. */
	virtual void Reset() final override;

	/** Remap InTime based on pre and post infinity extrapolation values */
	virtual void RemapTimeValue(float& InTime, float& CycleValueOffset) const final override;

	/** Evaluate this rich curve at the specified time */
	virtual float Eval(float InTime, float InDefaultValue = 0.0f) const final override;

	/** Auto set tangents for any 'auto' keys in curve */
	void AutoSetTangents(float Tension = 0.f);

	/** Resize curve length to the [MinTimeRange, MaxTimeRange] */
	virtual void ReadjustTimeRange(float NewMinTimeRange, float NewMaxTimeRange, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime) final override;

	/** Determine if two RichCurves are the same */
	bool operator == (const FRichCurve& Curve) const;

	/** Bake curve given the sample rate */
	virtual void BakeCurve(float SampleRate) final override;
	virtual void BakeCurve(float SampleRate, float FirstKeyTime, float LastKeyTime) final override;

	/** Remove redundant keys, comparing against Tolerance */
	virtual void RemoveRedundantKeys(float Tolerance) final override;
	virtual void RemoveRedundantKeys(float Tolerance, float FirstKeyTime, float LastKeyTime) final override;

private:
	void RemoveRedundantKeysInternal(float Tolerance, int32 InStartKeepKey, int32 InEndKeepKey);
	virtual int32 GetKeyIndex(float KeyTime, float KeyTimeTolerance) const override final;

public:

	// FIndexedCurve interface

	virtual int32 GetNumKeys() const final override { return Keys.Num(); }

public:

	/** Sorted array of keys */
	UPROPERTY(EditAnywhere, EditFixedSize, Category="Curve", meta=(EditFixedOrder))
	TArray<FRichCurveKey> Keys;
};


/**
 * Info about a curve to be edited.
 */
template<class T>
struct FRichCurveEditInfoTemplate
{
	/** Name of curve, used when displaying in editor. Can include comma's to allow tree expansion in editor */
	FName CurveName;

	/** Pointer to curves to be edited */
	T CurveToEdit;

	FRichCurveEditInfoTemplate()
		: CurveName(NAME_None)
		, CurveToEdit(nullptr)
	{ }

	FRichCurveEditInfoTemplate(T InCurveToEdit)
		: CurveName(NAME_None)
		, CurveToEdit(InCurveToEdit)
	{ }

	FRichCurveEditInfoTemplate(T InCurveToEdit, FName InCurveName)
		: CurveName(InCurveName)
		, CurveToEdit(InCurveToEdit)
	{ }

	inline bool operator==(const FRichCurveEditInfoTemplate<T>& Other) const
	{
		return Other.CurveName.IsEqual(CurveName) && Other.CurveToEdit == CurveToEdit;
	}

	uint32 GetTypeHash() const
	{
		return HashCombine(::GetTypeHash(CurveName), PointerHash(CurveToEdit));
	}
};


template<class T>
inline uint32 GetTypeHash(const FRichCurveEditInfoTemplate<T>& RichCurveEditInfo)
{
	return RichCurveEditInfo.GetTypeHash();
}

// Rename and deprecate
typedef FRichCurveEditInfoTemplate<FRealCurve*>			FRichCurveEditInfo;
typedef FRichCurveEditInfoTemplate<const FRealCurve*>	FRichCurveEditInfoConst;

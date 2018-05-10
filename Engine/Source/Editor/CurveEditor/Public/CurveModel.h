// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/Color.h"
#include "Containers/ArrayView.h"
#include "Curves/RichCurve.h"
#include "CurveEditorTypes.h"

struct FKeyHandle;
struct FKeyDrawInfo;
struct FCurveDrawParams;
struct FKeyPosition;
struct FKeyAttributes;
struct FCurveAttributes;
struct FCurveEditorScreenSpace;

class FName;
class SWidget;
class FCurveEditor;
class UObject;

enum class ECurvePointType : uint8;


/**
 * Class that models an underlying curve data structure through a generic abstraction that the curve editor understands.
 */
class FCurveModel
{
public:

	FCurveModel()
		: Color(FLinearColor::White)
	{}

	virtual ~FCurveModel()
	{}

	/**
	 * Access the raw pointer of the curve data
	 */
	virtual const void* GetCurve() const = 0;

	/**
	 * Explicitly modify the curve data. Called before any change is made to the curve.
	 */
	virtual void Modify() = 0;

	/**
	 * Draw the curve for the specified curve editor by populating an array with points on the curve between which lines should be drawn
	 *
	 * @param CurveEditor             Reference to the curve editor that is drawing the curve. Can be used to cull the interpolating points to the visible region.
	 * @param OutInterpolatingPoints  Array to populate with points (time, value) that lie on the curve.
	 */
	virtual void DrawCurve(const FCurveEditor& CurveEditor, TArray<TTuple<double, double>>& InterpolatingPoints) const = 0;

	/**
	 * Retrieve all keys that lie in the specified time and value range
	 *
	 * @param CurveEditor             Reference to the curve editor that is retrieving keys.
	 * @param MinTime                 Minimum key time to return in seconds
	 * @param MaxTime                 Maximum key time to return in seconds
	 * @param MinValue                Minimum key value to return
	 * @param MaxValue                Maximum key value to return
	 * @param OutKeyHandles           Array to populate with key handles that reside within the specified ranges
	 */
	virtual void GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const = 0;

	/**
	 * Add keys to this curve
	 *
	 * @param InPositions             Key positions for the new keys
	 * @param InAttributes            Key attributes for the new keys, one per key position
	 * @param OutKeyHandles           (Optional) Pointer to an array view of size InPositions.Num() that should be populated with newly added key handles
	 */
	virtual void AddKeys(TArrayView<const FKeyPosition> InPositions, TArrayView<const FKeyAttributes> InAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles = nullptr) = 0;

	/**
	 * Remove all the keys with the specified key handles from this curve
	 *
	 * @param InKeys                  Array of key handles to be removed from this curve
	 */
	virtual void RemoveKeys(TArrayView<const FKeyHandle> InKeys) = 0;

	/**
	 * Retrieve all key positions that pertain to the specified input key handles
	 *
	 * @param InKeys                  Array of key handles to get positions for
	 * @param OutKeyPositions         Array to receive key positions, one per index of InKeys
	 */
	virtual void GetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyPosition> OutKeyPositions) const = 0;

	/**
	 * Assign key positions for the specified key handles
	 *
	 * @param InKeys                 Array of key handles to set positions for
	 * @param InKeyPositions         Array of desired key positions to be applied to each of the corresponding key handles
	 */
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions) = 0;

	/**
	 * Populate the specified draw info structure with data describing how to draw the specified point type
	 *
	 * @param PointType              The type of point to be drawn
	 * @param OutDrawInfo            Data structure to be populated with draw info for this type of point
	 */
	virtual void GetKeyDrawInfo(ECurvePointType PointType, FKeyDrawInfo& OutDrawInfo) const = 0;

	/** Get range of input time.
	* @param MinTime Minimum Time
	* @param MaxTime Minimum Time
	*
	*/
	virtual void GetTimeRange(double& MinTime, double& MaxTime) const = 0;

	/** Get range of output values.
	* @param MinValue Minimum Value
	* @param MaxValue Minimum Value
	*/
	virtual void GetValueRange(double& MinValue, double& MaxValue) const = 0;

	/**
	 * Evaluate this curve at the specified time
	 *
	 * @param InTime                 The time to evaluate at, in seconds.
	 * @param Outvalue               Value to receive the evaluation result
	 * @return true if this curve was successfully evaluated, false otherwise
	 */
	virtual bool Evaluate(double InTime, double& OutValue) const = 0;

public:

	/**
	 * Retrieve all key attributes that pertain to the specified input key handles
	 *
	 * @param InKeys                Array of key handles to get attributes for
	 * @param OutAttributes         Array to receive key attributes, one per index of InKeys
	 */
	virtual void GetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyAttributes> OutAttributes) const
	{}

	/**
	 * Assign key attributes for the specified key handles
	 *
	 * @param InKeys                 Array of key handles to set attributes for
	 * @param InAttributes           Array of desired key attributes to be applied to each of the corresponding key handles
	 */
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes)
	{}

	/**
	 * Retrieve curve attributes for this curve
	 *
	 * @param OutAttributes          Attributes structure to populate with attributes that are relevant for this curve
	 */
	virtual void GetCurveAttributes(FCurveAttributes& OutAttributes) const
	{}

	/**
	 * Assign curve attributes for this curve
	 *
	 * @param InAttributes           Attributes structure to assign to this curve
	 */
	virtual void SetCurveAttributes(const FCurveAttributes& InAttributes)
	{}

	/**
	 * Retrieve an option input display offset (in seconds) to apply to all this curve's drawing
	 */
	virtual double GetInputDisplayOffset() const
	{
		return 0.0;
	}

	/**
	 * Create key proxy objects for the specified key handles. One object should be assigned to OutObjects per index within InKeyHandles
	 *
	 * @param InKeyHandles           Array of key handles to create edit objects for
	 * @param OutObjects             (Out) Array to receive objects that should be used to edit each of the input key handles.
	 */
	virtual void CreateKeyProxies(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<UObject*> OutObjects)
	{}

public:

	/**
	 * Helper function for assigning a the same attributes to a number of keys
	 */
	CURVEEDITOR_API void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, const FKeyAttributes& InAttributes);

	/**
	 * Helper function for adding a single key to this curve
	 */
	CURVEEDITOR_API TOptional<FKeyHandle> AddKey(const FKeyPosition& NewKeyPosition, const FKeyAttributes& InAttributes);

public:

	/**
	 * Access this curve's display name
	 */
	FORCEINLINE FText GetDisplayName() const
	{
		return DisplayName;
	}

	/**
	 * Assign a display name for this curve
	 */
	FORCEINLINE void SetDisplayName(FText InDisplayName)
	{
		DisplayName = InDisplayName;
	}

	/**
	 * Retrieve this curve's color
	 */
	FORCEINLINE const FLinearColor& GetColor() const
	{
		return Color;
	}

	/**
	 * Assign a new color to this curve
	 */
	FORCEINLINE void SetColor(const FLinearColor& InColor)
	{
		Color = InColor;
	}

protected:

	/** This curve's display name */
	FText DisplayName;

	/** This curve's display color */
	FLinearColor Color;
};

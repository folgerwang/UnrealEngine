// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Templates/UniquePtr.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Math/Range.h"
#include "Misc/FrameRate.h"

#include "CurveEditorTypes.h"
#include "CurveDataAbstraction.h"
#include "CurveModel.h"
#include "ICurveEditorBounds.h"
#include "CurveEditorSelection.h"
#include "ICurveEditorDragOperation.h"

class UCurveEditorSettings;
class FUICommandList;
struct FGeometry;
struct FCurveEditorSnapMetrics;
struct FCurveEditorScreenSpace;

DECLARE_DELEGATE_OneParam(FOnSetBoolean, bool)

class CURVEEDITOR_API FCurveEditor : public TSharedFromThis<FCurveEditor>
{
public:

	/**
	 * Container holding the current key/tangent selection
	 */
	FCurveEditorSelection Selection;

	

public:

	/** Attribute used to retrieve the current input snap rate (also used for display) */
	TAttribute<FFrameRate> InputSnapRateAttribute;

	/** Attribute used to retrieve the current output snap interval */
	TAttribute<double> OutputSnapIntervalAttribute;

	/** Attribute used to determine if we should snap input values */
	TAttribute<bool> InputSnapEnabledAttribute;

	/** Attribute used to determine if we should snap output values */
	TAttribute<bool> OutputSnapEnabledAttribute;

	/** Delegate that is invoked when the input snapping has been enabled/disabled */
	FOnSetBoolean OnInputSnapEnabledChanged;

	/** Delegate that is invoked when the output snapping has been enabled/disabled */
	FOnSetBoolean OnOutputSnapEnabledChanged;

	/** Attribute used for determining default attributes to apply to a newly create key */
	TAttribute<FKeyAttributes> DefaultKeyAttributes;

public:

	/**
	 * Constructor
	 */
	FCurveEditor();

	/**
	 * Non-copyable (shared ptr semantics)
	 */
	FCurveEditor(const FCurveEditor&) = delete;
	FCurveEditor& operator=(const FCurveEditor&) = delete;

	virtual ~FCurveEditor() {}

public:

	/**
	 * Generate a utility struct for converting between screen (slate unit) space and the underlying input/output axes
	 */
	FCurveEditorScreenSpace GetScreenSpace() const;

	/**
	 * Generate a utility struct for snapping values
	 */
	FCurveEditorSnapMetrics GetSnapMetrics() const;

public:

	/**
	 * Find a curve by its ID
	 *
	 * @return a ptr to the curve if found, nullptr otherwise
	 */
	FCurveModel* FindCurve(FCurveModelID CurveID) const;

	/**
	 * Access all the curves currently being shown on this editor
	 */
	const TMap<FCurveModelID, TUniquePtr<FCurveModel>>& GetCurves() const;

	/**
	 * Add a new curve to this editor
	 */
	FCurveModelID AddCurve(TUniquePtr<FCurveModel>&& InCurve);

	/**
	 * Remove a curve from this editor
	 */
	void RemoveCurve(FCurveModelID InCurveID);

	/**
	 * Retrieve this curve editor's command list
	 */
	FORCEINLINE TSharedPtr<FUICommandList> GetCommands() const { return CommandList; }

	/**
	 * Retrieve this curve editor's settings
	 */
	FORCEINLINE UCurveEditorSettings* GetSettings() const { return Settings; }

public:

	/**
	 * Assign a new bounds container to this curve editor
	 */
	void SetBounds(TUniquePtr<ICurveEditorBounds>&& InBounds);

	/**
	 * Retrieve the current curve editor bounds implementation
	 */
	const ICurveEditorBounds& GetBounds() const
	{
		return *Bounds.Get();
	}

	/**
	 * Retrieve the current curve editor bounds implementation
	 */
	ICurveEditorBounds& GetBounds()
	{
		return *Bounds.Get();
	}

public:

	/**
	 * Check whether this curve editor can automatically zoom to the current selection
	 */
	bool ShouldAutoFrame() const;

	/**
	 * Zoom the curve editor in or out around the center point
	 *
	 * @param Amount       The amount to zoom by as a factor of the current size
	 */
	void Zoom(float Amount);

	/**
	 * Zoom the curve editor in or out around the specified point
	 *
	 * @param Amount       The amount to zoom by as a factor of the current size
	 * @param TimeOrigin   The time origin to zoom around
	 * @param ValueOrigin  The value origin to zoom around
	 */
	void ZoomAround(float Amount, double TimeOrigin, double ValueOrigin);

	/**
	 * Zoom the curve editor to fit all the currently visible curves
	 *
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFit(EAxisList::Type Axes = EAxisList::All);

	/**
	 * Zoom the curve editor to fit the requested curves.
	 *
	 * @param CurveModelIDs The curve IDs to zoom to fit.
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFitCurves(TArrayView<const FCurveModelID> CurveModelIDs, EAxisList::Type Axes = EAxisList::All);

	/**
	 * Zoom the curve editor to fit all the current key selection. Zooms to fit all if less than 2 keys are selected.
	 *
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFitSelection(EAxisList::Type Axes = EAxisList::All);

public:

	/**
	 * Check whether keys should be snapped to the input display rate when dragging around
	 */
	bool IsInputSnappingEnabled() const;
	void ToggleInputSnapping();

	/**
	 * Check whether keys should be snapped to the output snap interval when dragging around
	 */
	bool IsOutputSnappingEnabled() const;
	void ToggleOutputSnapping();

public:

	/**
	 * Delete the currently selected keys
	 */
	void DeleteSelection();

	/**
	 * Flatten the tangents on the selected keys
	 */
	void FlattenSelection();

	/**
	 * Straighten the tangents on the selected keys
	 */
	void StraightenSelection();

	/**
	 * Bake curves between selected keys by adding points at every frame of the display rate
	 */
	void BakeSelection();

	/**
	 * Simplify curves between the selected keys by removing redundant keys
	 * @param Tolerance   Threshold at which to remove keys
	 */
	void SimplifySelection(float Tolerance= 0.1);

public:

	/**
	 * Populate the specified array with curve painting parameters
	 *
	 * @param OutDrawParams    An array to populate with curve painting parameters, one per visible curve
	 */
	void GetCurveDrawParams(TArray<FCurveDrawParams>& OutDrawParams) const;

	/**
	 * Called by SCurveEditorPanel to update the allocated geometry for this curve editor.
	 */
	void UpdateGeometry(const FGeometry& CurrentGeometry);

public:

	/**
	 * Called by SCurveEditorPanel to determine where to draw grid lines along the X-axis
	 */
	virtual void GetGridLinesX(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>& MajorGridLabels) const
	{
		ConstructXGridLines(MajorGridLines, MinorGridLines, MajorGridLabels);
	}

	/**
	 * Called by SCurveEditorPanel to determine where to draw grid lines along the Y-axis
	 */
	virtual void GetGridLinesY(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>& MajorGridLabels) const
	{
		ConstructYGridLines(MajorGridLines, MinorGridLines, MajorGridLabels, 4);
	}

	/**
	 * Bind UI commands that this curve editor responds to
	 */
	void BindCommands();

public:
	/**
	* Get A Vector for the given slope, usually a tangent, and length. Used to draw the tangent.
	*/
	static FVector2D GetVectorFromSlopeAndLength(float Slope, float Length);

public:
	/**
	* Given the position of a tangent in screen space get it's position in normal time/value space.
	*/
	FVector2D GetTangentPositionInScreenSpace(const FVector2D &StartPos, float Tangent, float Weight) const;

	/**
	* Given point and tangent position in screen space, get the tangent and it's weight value in normal time/value space.
	*/
	void GetTangentAndWeightFromScreenPosition(const FVector2D &StartPos, const  FVector2D &TangentPos, float &Tangent, float &Weight) const;

protected:

	/**
	 * Construct grid lines along the current display frame rate or time-base
	 */
	void ConstructXGridLines(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>& MajorGridLabels) const;

	/**
	 * Construct grid lines for the current visible value range
	 */
	void ConstructYGridLines(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>& MajorGridLabels, uint8 MinorDivisions) const;

	/**
	 * Internal zoom to fit implementation
	 */
	void ZoomToFitInternal(EAxisList::Type Axes, double InputMin, double InputMax, double OutputMin, double OutputMax);

protected:

	/** Curve editor bounds implementation */
	TUniquePtr<ICurveEditorBounds> Bounds;

	/** Map of all the currently visible curve models */
	TMap<FCurveModelID, TUniquePtr<FCurveModel>> CurveData;

	/** UI command list of actions mapped to this curve editor */
	TSharedPtr<FUICommandList> CommandList;

	/** Curve editor settings object */
	UCurveEditorSettings* Settings;

private:

	/** Cached physical size of the panel representing this editor */
	FVector2D CachedPhysicalSize;
};

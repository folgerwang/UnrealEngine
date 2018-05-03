// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "CurveEditorScreenSpace.h"
#include "ICurveEditorDragOperation.h"
#include "CurveDrawInfo.h"
#include "CurveEditor.h"
#include "CurveEditorTypes.h"

struct FCurveEditorEditObjectContainer;

class IDetailsView;

/**
 * Curve editor widget that reflects the state of an FCurveEditor
 */
class CURVEEDITOR_API SCurveEditorPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SCurveEditorPanel)
		: _GridLineTint(FLinearColor(0.1f, 0.1f, 0.1f, 1.f))
	{}

		/** Color to draw grid lines */
		SLATE_ATTRIBUTE(FLinearColor, GridLineTint)

	SLATE_END_ARGS()

	SCurveEditorPanel();
	~SCurveEditorPanel();

	/**
	 * Construct a new curve editor panel widget
	 */
	void Construct(const FArguments& InArgs, TSharedRef<FCurveEditor> InCurveEditor);

	/**
	 * Access the draw parameters that this curve editor has cached for this frame
	 */
	const TArray<FCurveDrawParams>& GetCachedDrawParams() const
	{
		return CachedDrawParams;
	}

	/**
	 * Access the combined command list for this curve editor and panel widget
	 */
	TSharedPtr<FUICommandList> GetCommands() const
	{
		return CommandList;
	}

	/**
	 * Access the details view used for editing selected keys
	 */
	TSharedPtr<IDetailsView> GetKeyDetailsView() const
	{
		return KeyDetailsView;
	}

private:

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/*~ Mouse interaction */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/*~ Keyboard interaction */
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	/**
	 * Get the visibility for the value splitter control
	 */
	EVisibility GetSplitterVisibility() const;

	/**
	 * Get the currently hovered curve ID
	 */
	TOptional<FCurveModelID> GetHoveredCurve() const;

	/**
	 * Hit test for a point on the curve editor using a mouse position in slate units
	 */
	TOptional<FCurvePointHandle> HitPoint(FVector2D MousePixel) const;

	/*~ Event bindings */
	void UpdateEditBox();
	void UpdateCommonCurveInfo();
	void UpdateToolTip(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);
	void UpdateCurveProximities(FVector2D MousePixel);

private:

	/**
	 * Bind command mappings for this widget
	 */
	void BindCommands();

	/**
	 * Rebind contextual command mappings that rely on the mouse position
	 */
	void RebindContextualActions(FVector2D MousePosition);

	/*~ Command binding callbacks */
	void OnAddKey(FVector2D MousePixel);
	void OnAddKeyHere(FVector2D MousePixel);
	void OnAddKey(FCurveModelID CurveToAdd, FVector2D MousePixel);

	/**
	 * Assign new attributes to the currently selected keys
	 */
	void SetKeyAttributes(FKeyAttributes KeyAttributes, FText Description);

	/**
	 * Assign new curve attributes to all visible curves
	 */
	void SetCurveAttributes(FCurveAttributes CurveAttributes, FText Description);

	/** Compare all the currently selected keys' interp modes against the specified interp mode */
	bool CompareCommonInterpolationMode(ERichCurveInterpMode InterpMode) const
	{
		return CachedCommonKeyAttributes.HasInterpMode() && CachedCommonKeyAttributes.GetInterpMode() == InterpMode;
	}

	/** Compare all the currently selected keys' tangent modes against the specified tangent mode */
	bool CompareCommonTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
	{
		return CompareCommonInterpolationMode(InterpMode) && CachedCommonKeyAttributes.HasTangentMode() && CachedCommonKeyAttributes.GetTangentMode() == TangentMode;
	}

	/** Compare all the currently selected keys' tangent modes against the specified tangent mode */
	bool CompareCommonTangentWeightMode(ERichCurveInterpMode InterpMode, ERichCurveTangentWeightMode TangentWeightMode) const
	{
		return CompareCommonInterpolationMode(InterpMode) && CachedCommonKeyAttributes.HasTangentWeightMode() && CachedCommonKeyAttributes.GetTangentWeightMode() == TangentWeightMode;
	}

	/** Compare all the visible curves' pre-extrapolation modes against the specified extrapolation mode */
	bool CompareCommonPreExtrapolationMode(ERichCurveExtrapolation PreExtrapolationMode) const
	{
		return CachedCommonCurveAttributes.HasPreExtrapolation()  && CachedCommonCurveAttributes.GetPreExtrapolation() == PreExtrapolationMode;
	}

	/** Compare all the visible curves' post-extrapolation modes against the specified extrapolation mode */
	bool CompareCommonPostExtrapolationMode(ERichCurveExtrapolation PostExtrapolationMode) const
	{
		return CachedCommonCurveAttributes.HasPostExtrapolation() && CachedCommonCurveAttributes.GetPostExtrapolation() == PostExtrapolationMode;
	}

	/**
	 * Toggle weighted tangents on the current selection
	 */
	void ToggleWeightedTangents();

	/**
	 * Check whether we can toggle weighted tangents on the current selection
	 */
	bool CanToggleWeightedTangents() const;

	/**
	*  Brings up Dialog to specify Tolerance before doing the key reduction.
	*/
	void OnSimplifySelection();

	/**
	*  Callback for when the curve is simplified.
	*/
	void OnSimplifySelectionCommited(const FText& InText, ETextCommit::Type CommitInfo);

	/**
	*  Create popup menu
	*/
	void GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted);


	/**
	*  Close popup menu
	*/
	void CloseEntryPopupMenu();
public:

	/**
	 * Draw grid lines
	 */
	int32 DrawGridLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect DrawEffects) const;

	/**
	 * Draw curve data
	 */
	int32 DrawCurves(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect DrawEffects) const;

private:

	/** The curve editor pointer */
	TSharedPtr<FCurveEditor> CurveEditor;

	/** Array of curve proximities in slate units that's updated on mouse move */
	TArray<TTuple<FCurveModelID, float>> CurveProximities;

	/** (Optional) the current drag operation */
	TOptional<FCurveEditorDelayedDrag> DragOperation;

	/** Curve draw parameters that are re-generated on tick */
	TArray<FCurveDrawParams> CachedDrawParams;

	/** Cached curve attributes that are common to all visible curves */
	FCurveAttributes CachedCommonCurveAttributes;

	/** Cached key attributes that are common to all selected keys */
	FKeyAttributes CachedCommonKeyAttributes;

	/** True if the current selection supports weighted tangents, false otherwise */
	bool bSelectionSupportsWeightedTangents;

	/** Attribute used for retrieving the desired grid line color */
	TAttribute<FLinearColor> GridLineTintAttribute;

	/** Edit panel */
	TSharedPtr<IDetailsView> KeyDetailsView;

	/** Map of edit UI widgets for each curve in the current selection set */
	TMap<FCurveModelID, TSharedPtr<SWidget>> CurveToEditUI;

	/** Command list for widget specific command bindings */
	TSharedPtr<FUICommandList> CommandList;

	/** Cached serial number from the curve editor selection. Used to update edit UIs when the selection changes. */
	uint32 CachedSelectionSerialNumber;

private:

	/*~ Cached tooltip data */
	bool  IsToolTipEnabled() const;
	FText GetToolTipCurveName() const;
	FText GetToolTipTimeText() const;
	FText GetToolTipValueText() const;

	struct FCachedToolTipData
	{
		FCachedToolTipData() {}

		FText Text;
		FText EvaluatedValue;
		FText EvaluatedTime;
	};
	TOptional<FCachedToolTipData> CachedToolTipData;

	/** The tolerance to use when reducing curves */
	float ReduceTolerance;

	/** Generic Popup Entry */
	TWeakPtr<IMenu> EntryPopupMenu;

	/** Container of objects that are being used to edit keys on the curve editor */
	TUniquePtr<FCurveEditorEditObjectContainer> EditObjects;
};
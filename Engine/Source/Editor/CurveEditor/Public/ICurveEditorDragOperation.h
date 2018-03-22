// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/DelayedDrag.h"
#include "Misc/Optional.h"
#include "ScopedTransaction.h"
#include "CurveEditorTypes.h"
#include "CurveEditorSnapMetrics.h"

class FCurveEditor;

/**
 * Interface for all drag operations in the curve editor
 */
class ICurveEditorDragOperation
{
public:

	ICurveEditorDragOperation()
		: MouseLockVector(FVector2D::UnitVector)
	{}

	virtual ~ICurveEditorDragOperation() {}

	/**
	 * Begin this drag operation with the specified initial and current positions
	 */
	void BeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent);

	/**
	 * Continue this drag operation with the specified initial and current positions
	 */
	void Drag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent);

	/**
	 * Finish this drag operation with the specified initial and current positions
	 */
	void EndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent);

	/**
	 * Paint this drag operation
	 */
	int32 Paint(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId);

	/**
	 * Cancel this drag operation
	 */
	void CancelDrag();

protected:

	/** Implementation method for derived types to begin a drag */
	virtual void OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
	{}

	/** Implementation method for derived types to continue a drag */
	virtual void OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
	{}

	/** Implementation method for derived types to finish a drag */
	virtual void OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
	{}

	/** Implementation method for derived types to paint this drag */
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId)
	{
		return LayerId;
	}

	/** Implementation method for derived types to cancel a drag */
	virtual void OnCancelDrag()
	{}

	/**
	 * Determine the effective mouse position for a drag vector, potentially locked to an axis based on the current pointer event
	 */
	FVector2D GetLockedMousePosition(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent);

protected:

	/** The vector we're currently locked to, or (1.f, 1.f) if we're not locked */
	FVector2D MouseLockVector;
};

/**
 * Interface for all key drag operations in the curve editor
 */
class ICurveEditorKeyDragOperation : public ICurveEditorDragOperation
{
public:

	/** Cached (and potentially manipulated) snap metrics to be used for this drag */
	FCurveEditorSnapMetrics SnapMetrics;

	/**
	 * Initialize this drag operation using the specified curve editor pointer and an optional cardinal point

	 * @param InCurveEditor       Curve editor pointer. Guaranteed to persist for the lifetime of this drag.
	 * @param CardinalPoint       The point that should be considered the origin of this drag.
	 */
	void Initialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& CardinalPoint);

protected:

	/** Implementation method for derived types to initialize a drag */
	virtual void OnInitialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& CardinalPoint)
	{}

	virtual void OnCancelDrag() override
	{
		if (Transaction.IsValid())
		{
			Transaction->Cancel();
		}
	}

	/** Scoped transaction pointer */
	TUniquePtr<FScopedTransaction> Transaction;
};

/**
 * Utility struct used to facilitate a delayed drag operation with an implementation interface
 */
struct FCurveEditorDelayedDrag : FDelayedDrag
{
	/**
	 * The drag implementation to use once the drag has started
	 */
	TUniquePtr<ICurveEditorDragOperation> DragImpl;

	/**
	 * Start a delayed drag operation at the specified position and effective key
	 */
	FCurveEditorDelayedDrag(FVector2D InInitialPosition, FKey InEffectiveKey)
		: FDelayedDrag(InInitialPosition, InEffectiveKey)
	{}
};

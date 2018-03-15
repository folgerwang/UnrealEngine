// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ICurveEditorDragOperation.h"
#include "CurveEditor.h"
#include "Input/Events.h"

void ICurveEditorDragOperation::BeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	GetLockedMousePosition(InitialPosition, CurrentPosition, MouseEvent);
	OnBeginDrag(InitialPosition, CurrentPosition, MouseEvent);
}

void ICurveEditorDragOperation::Drag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	OnDrag(InitialPosition, CurrentPosition, MouseEvent);
}

void ICurveEditorDragOperation::EndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	OnEndDrag(InitialPosition, CurrentPosition, MouseEvent);
}

int32 ICurveEditorDragOperation::Paint(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	return OnPaint(AllottedGeometry, OutDrawElements, LayerId);
}

void ICurveEditorDragOperation::CancelDrag()
{
	OnCancelDrag();
}

FVector2D ICurveEditorDragOperation::GetLockedMousePosition(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsShiftDown())
	{
		if (MouseLockVector == FVector2D::UnitVector)
		{
			if (FMath::Abs(CurrentPosition.Y - InitialPosition.Y) <= FMath::Abs(CurrentPosition.X - InitialPosition.X))
			{
				MouseLockVector.Y = 0.f;
			}
			else
			{
				MouseLockVector.X = 0.f;
			}
		}
	}
	else
	{
		MouseLockVector = FVector2D::UnitVector;
	}
	return InitialPosition + (CurrentPosition - InitialPosition) * MouseLockVector;
}

void ICurveEditorKeyDragOperation::Initialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& CardinalPoint)
{
	SnapMetrics = InCurveEditor->GetSnapMetrics();
	OnInitialize(InCurveEditor, CardinalPoint);
}
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DragOperations/CurveEditorDragOperation_Marquee.h"
#include "CurveEditor.h"
#include "SCurveEditorPanel.h"
#include "EditorStyleSet.h"
#include "CurveDrawInfo.h"

FCurveEditorDragOperation_Marquee::FCurveEditorDragOperation_Marquee(FCurveEditor* InCurveEditor, const SCurveEditorPanel* InCurveEditorPanel)
{
	CurveEditor = InCurveEditor;
	CurveEditorPanel = InCurveEditorPanel;
}

void FCurveEditorDragOperation_Marquee::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	Marquee = FSlateRect(
		FMath::Min(InitialPosition.X, CurrentPosition.X),
		FMath::Min(InitialPosition.Y, CurrentPosition.Y),
		FMath::Max(InitialPosition.X, CurrentPosition.X),
		FMath::Max(InitialPosition.Y, CurrentPosition.Y)
		);
}

void FCurveEditorDragOperation_Marquee::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	Marquee = FSlateRect(
		FMath::Min(InitialPosition.X, CurrentPosition.X),
		FMath::Min(InitialPosition.Y, CurrentPosition.Y),
		FMath::Max(InitialPosition.X, CurrentPosition.X),
		FMath::Max(InitialPosition.Y, CurrentPosition.Y)
		);
}

void FCurveEditorDragOperation_Marquee::OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	Marquee = FSlateRect(
		FMath::Min(InitialPosition.X, CurrentPosition.X),
		FMath::Min(InitialPosition.Y, CurrentPosition.Y),
		FMath::Max(InitialPosition.X, CurrentPosition.X),
		FMath::Max(InitialPosition.Y, CurrentPosition.Y)
		);

	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	bool bRemoveFromSelection = MouseEvent.IsAltDown();

	// Only select the same types of point
	TOptional<ECurvePointType> MatchPointType;

	if (!MouseEvent.IsShiftDown() && !bRemoveFromSelection)
	{
		CurveEditor->Selection.Clear();
	}
	else if (CurveEditor->Selection.Count() != 0)
	{
		MatchPointType = CurveEditor->Selection.GetSelectionType();
	}

	FSlateRect MarqueeRectPx = Marquee;
	for (const FCurveDrawParams& DrawParams : CurveEditorPanel->GetCachedDrawParams())
	{
		for (const FCurvePointInfo& Point : DrawParams.Points)
		{
			// Can we select this type of point?
			if (MatchPointType.IsSet() && Point.Type != MatchPointType.GetValue())
			{
				continue;
			}

			const FKeyDrawInfo& DrawInfo = DrawParams.GetKeyDrawInfo(Point.Type);
			FSlateRect PointRect = FSlateRect::FromPointAndExtent(Point.ScreenPosition - DrawInfo.ScreenSize/2, DrawInfo.ScreenSize);

			if (FSlateRect::DoRectanglesIntersect(PointRect, MarqueeRectPx))
			{
				if (!MatchPointType.IsSet() && Point.Type == ECurvePointType::Key)
				{
					CurveEditor->Selection.Clear();
					MatchPointType = ECurvePointType::Key;
				}

				if (bRemoveFromSelection)
				{
					CurveEditor->Selection.Remove(DrawParams.GetID(), Point.Type, Point.KeyHandle);
				}
				else
				{
					CurveEditor->Selection.Add(DrawParams.GetID(), Point.Type, Point.KeyHandle);
				}
			}
		}
	}
}

int32 FCurveEditorDragOperation_Marquee::OnPaint(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(Marquee.GetTopLeft(), Marquee.GetBottomRight() - Marquee.GetTopLeft()),
		FEditorStyle::GetBrush(TEXT("MarqueeSelection"))
		);

	return LayerId;
}

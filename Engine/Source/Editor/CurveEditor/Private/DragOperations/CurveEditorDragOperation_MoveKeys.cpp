// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DragOperations/CurveEditorDragOperation_MoveKeys.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditor.h"

void FCurveEditorDragOperation_MoveKeys::OnInitialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& CardinalPoint)
{
	CurveEditor = InCurveEditor;
}


void FCurveEditorDragOperation_MoveKeys::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	int32 NumKeys = CurveEditor->Selection.Count();
	Transaction = MakeUnique<FScopedTransaction>(FText::Format(NSLOCTEXT("CurveEditor", "MoveKeysFormat", "Move {0}|plural(one=Key, other=Keys)"), NumKeys));

	KeysByCurve.Reset();

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->Selection.GetAll())
	{
		FCurveModelID CurveID = Pair.Key;
		FCurveModel*  Curve   = CurveEditor->FindCurve(CurveID);

		if (ensure(Curve))
		{
			Curve->Modify();

			TArrayView<const FKeyHandle> Handles = Pair.Value.AsArray();

			FKeyData& KeyData = KeysByCurve.Emplace_GetRef(CurveID);
			KeyData.Handles = TArray<FKeyHandle>(Handles.GetData(), Handles.Num());

			KeyData.StartKeyPositions.SetNumZeroed(KeyData.Handles.Num());
			Curve->GetKeyPositions(KeyData.Handles, KeyData.StartKeyPositions);
		}
	}
}

void FCurveEditorDragOperation_MoveKeys::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	TArray<FKeyPosition> NewKeyPositionScratch;

	FVector2D MousePosition = GetLockedMousePosition(InitialPosition, CurrentPosition, MouseEvent);

	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();
	double DeltaInput  =  (MousePosition.X - InitialPosition.X) / ScreenSpace.PixelsPerInput();
	double DeltaOutput = -(MousePosition.Y - InitialPosition.Y) / ScreenSpace.PixelsPerOutput();

	for (const FKeyData& KeyData : KeysByCurve)
	{
		FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID);

		if (ensure(Curve))
		{
			NewKeyPositionScratch.Reset();
			NewKeyPositionScratch.Reserve(KeyData.StartKeyPositions.Num());

			for (FKeyPosition StartPosition : KeyData.StartKeyPositions)
			{
				StartPosition.InputValue  += DeltaInput;
				StartPosition.OutputValue += DeltaOutput;

				StartPosition.InputValue  = SnapMetrics.SnapInputSeconds(StartPosition.InputValue);
				StartPosition.OutputValue = SnapMetrics.SnapOutput(StartPosition.OutputValue);

				NewKeyPositionScratch.Add(StartPosition);
			}

			Curve->SetKeyPositions(KeyData.Handles, NewKeyPositionScratch);
		}
	}
}

void FCurveEditorDragOperation_MoveKeys::OnCancelDrag()
{
	ICurveEditorKeyDragOperation::OnCancelDrag();

	for (const FKeyData& KeyData : KeysByCurve)
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID))
		{
			Curve->SetKeyPositions(KeyData.Handles, KeyData.StartKeyPositions);
		}
	}
}
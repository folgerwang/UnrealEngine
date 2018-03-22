// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Tangent.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditor.h"

void FCurveEditorDragOperation_Tangent::OnInitialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& InCardinalPoint)
{
	CurveEditor = InCurveEditor;

	// Find the position of the clicked on key (no type name)
	if (InCardinalPoint.IsSet())
	{
		if (const FCurveModel* Curve = CurveEditor->FindCurve(InCardinalPoint->CurveID))
		{
			FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

			FKeyPosition CardinalKeyPosition;

			Curve->GetKeyPositions(
				TArrayView<const FKeyHandle>(&InCardinalPoint->KeyHandle, 1),
				MakeArrayView(&CardinalKeyPosition, 1));

			CardinalPosition = FVector2D(ScreenSpace.SecondsToScreen(CardinalKeyPosition.InputValue), ScreenSpace.ValueToScreen(CardinalKeyPosition.OutputValue));
		}
	}
}

void FCurveEditorDragOperation_Tangent::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	PointType = CurveEditor->Selection.GetSelectionType();

	int32 NumKeys = CurveEditor->Selection.Count();

	FText Description = PointType == ECurvePointType::ArriveTangent
		? FText::Format(NSLOCTEXT("CurveEditor", "DragEntryTangentsFormat", "Drag Entry {0}|plural(one=Tangent, other=Tangents)"), NumKeys)
		: FText::Format(NSLOCTEXT("CurveEditor", "DragExitTangentsFormat", "Drag Exit {0}|plural(one=Tangent, other=Tangents)"), NumKeys);

	Transaction = MakeUnique<FScopedTransaction>(Description);

	KeysByCurve.Reset();
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->Selection.GetAll())
	{
		FCurveModelID CurveID = Pair.Key;
		FCurveModel* Curve    = CurveEditor->FindCurve(CurveID);

		if (ensure(Curve))
		{
			Curve->Modify();

			TArrayView<const FKeyHandle> Handles = Pair.Value.AsArray();

			FKeyData& KeyData = KeysByCurve.Emplace_GetRef(CurveID);
			KeyData.Handles = TArray<FKeyHandle>(Handles.GetData(), Handles.Num());

			KeyData.Attributes.SetNum(KeyData.Handles.Num());
			Curve->GetKeyAttributes(KeyData.Handles, KeyData.Attributes);
		}
	}
}

void FCurveEditorDragOperation_Tangent::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();
	const float DisplayRatio = (ScreenSpace.PixelsPerOutput() / ScreenSpace.PixelsPerInput());

	TOptional<float> NewTangent;
	
	FVector2D PixelDelta = CurrentPosition - CardinalPosition.Get(InitialPosition);

	if (MouseEvent.IsShiftDown())
	{
		PixelDelta = RoundTrajectory(PixelDelta);
	}

	if (PointType == ECurvePointType::ArriveTangent)
	{
		PixelDelta.X = FMath::Min(PixelDelta.X, -KINDA_SMALL_NUMBER);
	}
	else
	{
		PixelDelta.X = FMath::Max(PixelDelta.X, KINDA_SMALL_NUMBER);
	}
	//If the CardinalPosition is set that means we are doing a left mouse and we are over the key(cardinal) position
	//and we can set the tangent directly.
	//If we are doing a middle mouse move, there is no cardinal position, but rather we need to apply the
	//PixelDelta invidually to each tangent
	if (CardinalPosition.IsSet())
	{
		NewTangent = (-PixelDelta.Y / PixelDelta.X) / DisplayRatio;
	}

	TArray<FKeyAttributes> NewKeyAttributesScratch;
	TArray<FKeyPosition> KeyPositions;

	for (const FKeyData& KeyData : KeysByCurve)
	{
		FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID);
		if (ensure(Curve))
		{
			NewKeyAttributesScratch.Reset();
			NewKeyAttributesScratch.Reserve(KeyData.Attributes.Num());
			if (!NewTangent.IsSet()) //No tangent, so MMB, so cache out positions for each key
			{
				KeyPositions.SetNumUninitialized(KeyData.Attributes.Num());
				Curve->GetKeyPositions(KeyData.Handles, KeyPositions);
			}

			if (PointType == ECurvePointType::ArriveTangent)
			{
				for(int32 i =0; i < KeyData.Handles.Num();++i)
				{
					if (NewTangent.IsSet())
					{
						NewKeyAttributesScratch.Add(FKeyAttributes().SetArriveTangent(NewTangent.GetValue()));
					}
					else  //Apply Delta to each Tangent indvidually
					{
						const FKeyAttributes Attributes = KeyData.Attributes[i];
						const float ArriveTangent = Attributes.GetArriveTangent();
						const FVector2D  KeyScreenPosition = FVector2D(ScreenSpace.SecondsToScreen(KeyPositions[i].InputValue), ScreenSpace.ValueToScreen(KeyPositions[i].OutputValue));
						FVector2D TangentScreenPosition = FCurveEditor::GetVectorFromSlopeAndLength(ArriveTangent * -DisplayRatio, -60.f) + KeyScreenPosition;
						TangentScreenPosition += PixelDelta;
						TangentScreenPosition -= KeyScreenPosition;
						float Tangent = (-TangentScreenPosition.Y / TangentScreenPosition.X) / DisplayRatio;
						NewKeyAttributesScratch.Add(FKeyAttributes().SetArriveTangent(Tangent));
					}
				}
			}
			else
			{
				for (int32 i = 0; i < KeyData.Handles.Num(); ++i)
				{
					if (NewTangent.IsSet())
					{
						NewKeyAttributesScratch.Add(FKeyAttributes().SetLeaveTangent(NewTangent.GetValue()));
					}
					else  //Apply Delta to each Tangent indvidually
					{
						const FKeyAttributes Attributes = KeyData.Attributes[i];
						const float LeaveTangent = Attributes.GetLeaveTangent();
						const FVector2D  KeyScreenPosition = FVector2D(ScreenSpace.SecondsToScreen(KeyPositions[i].InputValue), ScreenSpace.ValueToScreen(KeyPositions[i].OutputValue));
						FVector2D TangentScreenPosition = FCurveEditor::GetVectorFromSlopeAndLength(LeaveTangent * -DisplayRatio,60.f) + KeyScreenPosition;
						TangentScreenPosition += PixelDelta;
						TangentScreenPosition -= KeyScreenPosition;
						float Tangent = (-TangentScreenPosition.Y / TangentScreenPosition.X) / DisplayRatio;
						NewKeyAttributesScratch.Add(FKeyAttributes().SetLeaveTangent(Tangent));
					}
				}
			}

			Curve->SetKeyAttributes(KeyData.Handles, NewKeyAttributesScratch);
		}
	}
}

void FCurveEditorDragOperation_Tangent::OnCancelDrag()
{
	ICurveEditorKeyDragOperation::OnCancelDrag();

	for (const FKeyData& KeyData : KeysByCurve)
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID))
		{
			Curve->SetKeyAttributes(KeyData.Handles, KeyData.Attributes);
		}
	}
}

FVector2D FCurveEditorDragOperation_Tangent::RoundTrajectory(FVector2D Delta)
{
	float Distance = Delta.Size();
	float Theta = FMath::Atan2(Delta.Y, Delta.X) + PI/2;

	float RoundTo = PI / 4;
	Theta = FMath::RoundToInt(Theta / RoundTo) * RoundTo - PI/2;
	return FVector2D(Distance * FMath::Cos(Theta), Distance * FMath::Sin(Theta));
}
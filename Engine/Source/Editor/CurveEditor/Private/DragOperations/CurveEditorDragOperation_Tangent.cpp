// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Tangent.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditor.h"

void FCurveEditorDragOperation_Tangent::OnInitialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& InCardinalPoint)
{
	CurveEditor = InCurveEditor;
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
	
	FVector2D PixelDelta = CurrentPosition - InitialPosition;
	if (MouseEvent.IsShiftDown())
	{
		PixelDelta = RoundTrajectory(PixelDelta);
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
			KeyPositions.SetNumUninitialized(KeyData.Attributes.Num());
			Curve->GetKeyPositions(KeyData.Handles, KeyPositions);
			

			if (PointType == ECurvePointType::ArriveTangent)
			{
				for (int32 i = 0; i < KeyData.Handles.Num(); ++i)
				{
					const FKeyAttributes Attributes = KeyData.Attributes[i];
					if (Attributes.HasArriveTangent())
					{
						const float ArriveTangent = Attributes.GetArriveTangent();
						const FVector2D  KeyScreenPosition = FVector2D(ScreenSpace.SecondsToScreen(KeyPositions[i].InputValue), ScreenSpace.ValueToScreen(KeyPositions[i].OutputValue));
						if (Attributes.HasTangentWeightMode() && Attributes.HasArriveTangentWeight() &&
							(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedArrive))
						{
							float Tangent, Weight;
							FVector2D TangentScreenPosition = CurveEditor->GetTangentPositionInScreenSpace(KeyScreenPosition, ArriveTangent, -Attributes.GetArriveTangentWeight());
							TangentScreenPosition.X += PixelDelta.X;
							TangentScreenPosition.Y += PixelDelta.Y;

							CurveEditor->GetTangentAndWeightFromScreenPosition(KeyScreenPosition, TangentScreenPosition, Tangent, Weight);

							FKeyAttributes NewAttributes;

							NewAttributes.SetArriveTangent(Tangent);
							NewAttributes.SetArriveTangentWeight(Weight);
							NewKeyAttributesScratch.Add(NewAttributes);

						}
						else
						{
							const float PixelLength = 60.0f;
							FVector2D TangentScreenPosition = FCurveEditor::GetVectorFromSlopeAndLength(ArriveTangent * -DisplayRatio, -PixelLength) + KeyScreenPosition;
							TangentScreenPosition += PixelDelta;
							TangentScreenPosition -= KeyScreenPosition;
							const float Tangent = (-TangentScreenPosition.Y / TangentScreenPosition.X) / DisplayRatio;
							FKeyAttributes NewAttributes;
							NewAttributes.SetArriveTangent(Tangent);
							NewKeyAttributesScratch.Add(NewAttributes);
						}
					}
					else //still need to add since expect attributes to equal num of selected
					{
						NewKeyAttributesScratch.Add(FKeyAttributes());
					}
				}
			}
			else
			{
				for (int32 i = 0; i < KeyData.Handles.Num(); ++i)
				{
					const FKeyAttributes Attributes = KeyData.Attributes[i];
					if (Attributes.HasLeaveTangent())
					{
						const float LeaveTangent = Attributes.GetLeaveTangent();
						const FVector2D  KeyScreenPosition = FVector2D(ScreenSpace.SecondsToScreen(KeyPositions[i].InputValue), ScreenSpace.ValueToScreen(KeyPositions[i].OutputValue));
						if (Attributes.HasTangentWeightMode() && Attributes.HasLeaveTangentWeight() &&
							(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedLeave))
						{
							float Tangent, Weight;
							FVector2D TangentScreenPosition = CurveEditor->GetTangentPositionInScreenSpace(KeyScreenPosition, LeaveTangent, Attributes.GetLeaveTangentWeight());
							TangentScreenPosition.X += PixelDelta.X;
							TangentScreenPosition.Y += PixelDelta.Y;
							CurveEditor->GetTangentAndWeightFromScreenPosition(KeyScreenPosition, TangentScreenPosition, Tangent, Weight);
								
							FKeyAttributes NewAttributes;
							NewAttributes.SetLeaveTangent(Tangent);
							NewAttributes.SetLeaveTangentWeight(Weight);
							NewKeyAttributesScratch.Add(NewAttributes);
						}
						else
						{
							const float PixelLength = 60.0f;
							FVector2D TangentScreenPosition = FCurveEditor::GetVectorFromSlopeAndLength(LeaveTangent * -DisplayRatio, PixelLength) + KeyScreenPosition;
							TangentScreenPosition += PixelDelta;
							TangentScreenPosition -= KeyScreenPosition;
							const float Tangent = (-TangentScreenPosition.Y / TangentScreenPosition.X) / DisplayRatio;
							FKeyAttributes NewAttributes;
							NewAttributes.SetLeaveTangent(Tangent);
							NewKeyAttributesScratch.Add(NewAttributes);
						}
					}
					else //still need to add since expect attributes to equal num of selected
					{
						NewKeyAttributesScratch.Add(FKeyAttributes());
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
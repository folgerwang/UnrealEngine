// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Pan.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditor.h"

FCurveEditorDragOperation_Pan::FCurveEditorDragOperation_Pan(FCurveEditor* InCurveEditor)
	: CurveEditor(InCurveEditor)
{
}

void FCurveEditorDragOperation_Pan::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	InitialInputMin  = ScreenSpace.GetInputMin();
	InitialInputMax  = ScreenSpace.GetInputMax();
	InitialOutputMin = ScreenSpace.GetOutputMin();
	InitialOutputMax = ScreenSpace.GetOutputMax();
}

void FCurveEditorDragOperation_Pan::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FVector2D PixelDelta = GetLockedMousePosition(InitialPosition, CurrentPosition, MouseEvent) - InitialPosition;

	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	double InputMin  = InitialInputMin - PixelDelta.X / ScreenSpace.PixelsPerInput();
	double InputMax  = InitialInputMax - PixelDelta.X / ScreenSpace.PixelsPerInput();

	double OutputMin = InitialOutputMin + PixelDelta.Y / ScreenSpace.PixelsPerOutput();
	double OutputMax = InitialOutputMax + PixelDelta.Y / ScreenSpace.PixelsPerOutput();

	CurveEditor->GetBounds().SetInputBounds(InputMin, InputMax);
	CurveEditor->GetBounds().SetOutputBounds(OutputMin, OutputMax);
}
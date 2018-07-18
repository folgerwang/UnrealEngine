// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Zoom.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditor.h"

FCurveEditorDragOperation_Zoom::FCurveEditorDragOperation_Zoom(FCurveEditor* InCurveEditor)
	: CurveEditor(InCurveEditor)
{
}

void FCurveEditorDragOperation_Zoom::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	ZoomFactor = InitialPosition / ScreenSpace.GetPhysicalSize();

	OriginalInputRange = ( ScreenSpace.GetInputMax() - ScreenSpace.GetInputMin() );
	OriginalOutputRange = ( ScreenSpace.GetOutputMax() - ScreenSpace.GetOutputMin() );

	ZoomOriginX = ScreenSpace.GetInputMin() + OriginalInputRange * ZoomFactor.X;
	ZoomOriginY = ScreenSpace.GetOutputMin() + OriginalOutputRange * ZoomFactor.Y;
}

void FCurveEditorDragOperation_Zoom::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FVector2D PixelDelta = GetLockedMousePosition(InitialPosition, CurrentPosition, MouseEvent) - InitialPosition;

	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	double ClampRange = 1e9f;

	double DiffX = double(PixelDelta.X) / (ScreenSpace.GetPhysicalWidth() / OriginalInputRange);
	double DiffY = double(PixelDelta.Y) / (ScreenSpace.GetPhysicalHeight() / OriginalOutputRange);

	double NewInputRange = OriginalInputRange + DiffX;
	double InputMin  = FMath::Clamp<double>(ZoomOriginX - NewInputRange * ZoomFactor.X, -ClampRange, ClampRange);
	double InputMax  = FMath::Clamp<double>(ZoomOriginX + NewInputRange * (1.f - ZoomFactor.X), InputMin, ClampRange);


	double NewOutputRange = OriginalOutputRange + DiffY;
	double OutputMin  = FMath::Clamp<double>(ZoomOriginY - NewOutputRange * ZoomFactor.Y, -ClampRange, ClampRange);
	double OutputMax  = FMath::Clamp<double>(ZoomOriginY + NewOutputRange * (1.f - ZoomFactor.Y), OutputMin, ClampRange);

	CurveEditor->GetBounds().SetInputBounds(InputMin, InputMax);
	CurveEditor->GetBounds().SetOutputBounds(OutputMin, OutputMax);
}
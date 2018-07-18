// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICurveEditorDragOperation.h"

class FCurveEditor;

class FCurveEditorDragOperation_Pan : public ICurveEditorDragOperation
{
public:
	FCurveEditorDragOperation_Pan(FCurveEditor* CurveEditor);

	virtual void OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;
	virtual void OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;

private:

	FCurveEditor* CurveEditor;
	double InitialInputMin, InitialInputMax;
	double InitialOutputMin, InitialOutputMax;
};
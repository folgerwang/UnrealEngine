// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreTypes.h"
#include "Math/Vector2D.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "ICurveEditorDragOperation.h"

class FCurveEditor;
class SCurveEditorPanel;

class FCurveEditorDragOperation_Marquee : public ICurveEditorDragOperation
{
public:

	FCurveEditorDragOperation_Marquee(FCurveEditor* InCurveEditor, const SCurveEditorPanel* InCurveEditorPanel);

	virtual void OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;
	virtual void OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;
	virtual void OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) override;

private:

	/** The current marquee rectangle */
	FSlateRect Marquee;
	/** Ptr back to the curve editor */
	FCurveEditor* CurveEditor;
	/** Ptr back to the curve editor panel */
	const SCurveEditorPanel* CurveEditorPanel;
};
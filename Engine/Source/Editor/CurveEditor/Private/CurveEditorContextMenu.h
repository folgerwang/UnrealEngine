// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"
#include "CurveEditorTypes.h"

class FCurveEditor;
class FMenuBuilder;

struct FCurveEditorContextMenu
{
	static void BuildMenu(FMenuBuilder& MenuBuilder, TWeakPtr<FCurveEditor> WeakCurveEditor, TOptional<FCurvePointHandle> ClickedPoint, TOptional<FCurveModelID> HoveredCurve);
};
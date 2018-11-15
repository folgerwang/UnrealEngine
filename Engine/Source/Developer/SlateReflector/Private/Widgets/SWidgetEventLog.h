// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Debugging/SlateDebugging.h"

/**
 * 
 */
class SWidgetEventLog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetEventLog)
		{}
	SLATE_END_ARGS()

	virtual ~SWidgetEventLog();

	void Construct(const FArguments& InArgs);

private:
	void RemoveListeners();
	void UpdateListeners();
	void OnInputEvent(const FSlateDebuggingInputEventArgs& EventArgs);
	void OnFocusEvent(const FSlateDebuggingFocusEventArgs& EventArgs);
};

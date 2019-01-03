// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Layout/WidgetPath.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

class SToolTip;
class FTranslationPickerInputProcessor;

/** Translation picker floating window to show details of FText(s) under cursor, and allow in-place translation via TranslationPickerEditWindow */
class STranslationPickerFloatingWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerFloatingWindow) {}

	SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

	virtual ~STranslationPickerFloatingWindow();

	void Construct(const FArguments& InArgs);

private:
	friend class FTranslationPickerInputProcessor;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/** Pull the FText reference out of an SWidget */
	void PickTextFromWidget(TSharedRef<SWidget> Widget);

	/** Pull the FText reference out of the child widgets of an SWidget */
	void PickTextFromChildWidgets(TSharedRef<SWidget> Widget);

	/** Handle escape being pressed */
	void OnEscapePressed();

	/** Input processor used to capture the 'Esc' key */
	TSharedPtr<FTranslationPickerInputProcessor> InputProcessor;

	/** Handle to the window that contains this widget */
	TWeakPtr<SWindow> ParentWindow;

	/** Contents of the window */
	TSharedPtr<SToolTip> WindowContents;

	/** The FTexts that we have found under the cursor */
	TArray<FText> PickedTexts;

	/**
	* The path widgets we were hovering over last tick
	*/
	FWeakWidgetPath LastTickHoveringWidgetPath;
};

#undef LOCTEXT_NAMESPACE

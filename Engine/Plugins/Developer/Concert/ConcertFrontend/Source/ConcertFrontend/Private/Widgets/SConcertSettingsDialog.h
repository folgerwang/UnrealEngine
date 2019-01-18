// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"


DECLARE_DELEGATE(FOnConfirmOrCancel)

struct FConcertSettingsDialogArgs
{
	TAttribute<FText> WindowLabel;
	TAttribute<FText> ConfirmText;
	TAttribute<FText> CancelText;
	TAttribute<FText> ConfirmTooltipText;
	TAttribute<FText> CancelTooltipText;

	TAttribute<bool> IsConfirmEnabled;
	FOnConfirmOrCancel ConfirmCallback;
	FOnConfirmOrCancel CancelCallback;

	float MinWindowWith = 400.f;
};

class FStructOnScope;
class SWindow;

class SConcertSettingsDialog : public SCompoundWidget
{
public:

	/**
	 * This function trigger the dialog window to adjust the properties of a UStruct
	 * @param OutSettings - the settings/Details that will be displayed and adjusted by the dialog
	 * @return A weak pointer to the dialog window created
	 */
	static TWeakPtr<SWindow> AddWindow(FConcertSettingsDialogArgs&& InArgs, TSharedRef<FStructOnScope> OutSettings, float ValuesColumPercentWidth = 0.65f);

private:
	SLATE_BEGIN_ARGS(SConcertSettingsDialog)
		: _ConfirmTooltipText()
		, _CancelTooltipText()
		, _WidgetWindow()
		{}
		SLATE_ATTRIBUTE(FText, ConfirmText)
		SLATE_ATTRIBUTE(FText, CancelText)
		SLATE_ATTRIBUTE(FText, ConfirmTooltipText)
		SLATE_ATTRIBUTE(FText, CancelTooltipText)
		SLATE_ATTRIBUTE(bool, IsConfirmEnabled)
		SLATE_ARGUMENT(TWeakPtr<SWindow>, WidgetWindow)
		SLATE_EVENT(FOnConfirmOrCancel, ConfirmCallback)
		SLATE_EVENT(FOnConfirmOrCancel, CancelCallback)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, float ValuesColumPercentWidth, TSharedRef<FStructOnScope> OutSettings);

	static TSharedRef<SWindow> CreateWindow(const FConcertSettingsDialogArgs& InArgs);

	TSharedRef<SWidget> CreateWindowContent(TSharedRef<SWidget> MainContent, const FArguments& InArgs);

	void OnWindowCLosed(const TSharedRef<SWindow>& Window);
	FReply OnConfirm();
	FReply OnCancel();

	TSharedPtr<FStructOnScope> Settings;
	TWeakPtr<SWindow> WidgetWindow;
	FOnConfirmOrCancel ConfirmCallback;
	FOnConfirmOrCancel CancelCallback;
	bool bWasConfirmed;
};

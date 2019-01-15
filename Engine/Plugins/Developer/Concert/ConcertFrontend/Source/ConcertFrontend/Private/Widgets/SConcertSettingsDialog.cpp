// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SConcertSettingsDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"

namespace ConcertSettingsDialogUtils
{
	FWindowStyle* GetWindowStyle()
	{
		static FWindowStyle WindowStyle = FCoreStyle::Get().GetWidgetStyle<FWindowStyle>("Window");
		WindowStyle.SetBackgroundBrush(WindowStyle.ChildBackgroundBrush);
		return &WindowStyle;
	}
}


TWeakPtr<SWindow> SConcertSettingsDialog::AddWindow(FConcertSettingsDialogArgs&& InArgs, TSharedRef<FStructOnScope> OutSettings, float ValuesColumPercentWidth)
{
	TSharedRef<SWindow> Window = CreateWindow(InArgs);

	Window->SetContent(
		SNew(SConcertSettingsDialog, ValuesColumPercentWidth, OutSettings)
		.ConfirmText(MoveTemp(InArgs.ConfirmText))
		.CancelText(MoveTemp(InArgs.CancelText))
		.ConfirmTooltipText(MoveTemp(InArgs.ConfirmTooltipText))
		.CancelTooltipText(MoveTemp(InArgs.CancelTooltipText))
		.ConfirmCallback(MoveTemp(InArgs.ConfirmCallback))
		.CancelCallback(MoveTemp(InArgs.CancelCallback))
		.IsConfirmEnabled(MoveTemp(InArgs.IsConfirmEnabled))
		.WidgetWindow(Window)
	);

	return FSlateApplication::Get().AddWindow(Window, true);
}

void SConcertSettingsDialog::Construct(const FArguments& InArgs, float ValuesColumPercentWidth, TSharedRef<FStructOnScope> OutSettings)
{
	WidgetWindow = InArgs._WidgetWindow;
	ConfirmCallback = InArgs._ConfirmCallback;
	CancelCallback = InArgs._CancelCallback;
	Settings = OutSettings;

	FOnWindowClosed OnWindowClose;
	OnWindowClose.BindSP(this, &SConcertSettingsDialog::OnWindowCLosed);
	
	if (TSharedPtr<SWindow> WindowPtr = WidgetWindow.Pin())
	{
		WindowPtr->SetOnWindowClosed(OnWindowClose);
	}

	TSharedPtr<SBox> InspectorBox;

	ChildSlot
	[
		CreateWindowContent(SAssignNew(InspectorBox, SBox), InArgs)
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.ColumnWidth = ValuesColumPercentWidth;
	DetailsViewArgs.bShowScrollBar = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructureDetailsViewArgs;
	StructureDetailsViewArgs.bShowObjects = true;
	StructureDetailsViewArgs.bShowAssets = true;
	StructureDetailsViewArgs.bShowClasses = true;
	StructureDetailsViewArgs.bShowInterfaces = true;

	TSharedRef<IStructureDetailsView> DetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsViewArgs, OutSettings);

	InspectorBox->SetContent(DetailsView->GetWidget().ToSharedRef());
}

TSharedRef<SWindow> SConcertSettingsDialog::CreateWindow(const FConcertSettingsDialogArgs& InArgs)
{
	// Compute a centered window position based on the min window size
	const float MinWindowWidth = InArgs.MinWindowWith;
	FVector2D MinWindowSize = FVector2D(MinWindowWidth, 400.f);

	FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
	FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
	FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

	float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
	MinWindowSize *= ScaleFactor;

	FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - MinWindowSize) / 2.0f) / ScaleFactor;

	return SNew(SWindow)
		.Title(InArgs.WindowLabel)
		.SizingRule(ESizingRule::Autosized)
		.MinWidth(MinWindowWidth)
		.AutoCenter(EAutoCenter::None)
		.ScreenPosition(WindowPosition)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsTopmostWindow(true)
		.Style(ConcertSettingsDialogUtils::GetWindowStyle());
}

TSharedRef<SWidget> SConcertSettingsDialog::CreateWindowContent(TSharedRef<SWidget> MainContent, const FArguments& InArgs)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			MainContent
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.IsEnabled(InArgs._IsConfirmEnabled)
				.Text(InArgs._ConfirmText)
				.ToolTipText(InArgs._ConfirmTooltipText)
				.OnClicked(this, &SConcertSettingsDialog::OnConfirm)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(InArgs._CancelText)
				.ToolTipText(InArgs._CancelTooltipText)
				.OnClicked(this, &SConcertSettingsDialog::OnCancel)
			]
		];
}

void SConcertSettingsDialog::OnWindowCLosed(const TSharedRef<SWindow>& Window)
{
	if (!bWasConfirmed)
	{
		CancelCallback.ExecuteIfBound();
	}
}

FReply SConcertSettingsDialog::OnConfirm()
{
	ConfirmCallback.ExecuteIfBound();

	bWasConfirmed = true;

	if (TSharedPtr<SWindow> WindowPtr = WidgetWindow.Pin())
	{
		WindowPtr->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SConcertSettingsDialog::OnCancel()
{
	if (TSharedPtr<SWindow> WindowPtr = WidgetWindow.Pin())
	{
		WindowPtr->RequestDestroyWindow();
	}
	return FReply::Handled();
}

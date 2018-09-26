// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewWindowTitleBar.h"
#include "PIEPreviewWindow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "PIEPreviewWindowTitleBar"

void SPIEPreviewWindowTitleBar::MakeTitleBarContentWidgets(TSharedPtr< SWidget >& OutLeftContent, TSharedPtr< SWidget >& OutRightContent)
{
	TSharedPtr< SWidget > OutRightContentBaseWindow;
	SWindowTitleBar::MakeTitleBarContentWidgets(OutLeftContent, OutRightContentBaseWindow);

	ScreenRotationButton = SNew(SButton)
		.IsFocusable(false)
		.IsEnabled(true)
		.ContentPadding(0)
		.OnClicked_Lambda
		(
			[this]()
			{
				TSharedPtr<SPIEPreviewWindow> PIEWindow = GetOwnerWindow();

				if (PIEWindow.IsValid())
				{
					PIEWindow->RotateWindow();
				}

				return FReply::Handled();
			}
		)
		.Cursor(EMouseCursor::Default)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		[
			SNew(SImage)
			.Image(this, &SPIEPreviewWindowTitleBar::GetScreenRotationButtonImage)
			.ColorAndOpacity(this, &SPIEPreviewWindowTitleBar::GetWindowTitleContentColor)
		]
		.IsEnabled_Lambda
		(
			[this]()
			{
				TSharedPtr<SPIEPreviewWindow> PIEWindow = GetOwnerWindow();
			
				if (PIEWindow.IsValid())
				{
					return PIEWindow->IsRotationAllowed();
				}

				return false;
			}
		)
	;

	TSharedRef< SHorizontalBox > WindowTitleBarButtons =
	SNew(SHorizontalBox)
	.Visibility(EVisibility::SelfHitTestInvisible);

	WindowTitleBarButtons->AddSlot()
		.AutoWidth()
		[
			ScreenRotationButton.ToSharedRef()
		];

	if (OutRightContentBaseWindow.IsValid())
	{
		WindowTitleBarButtons->AddSlot()
			.AutoWidth()
			[
				OutRightContentBaseWindow.ToSharedRef()
			];
	}

	OutRightContent = SNew(SBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
		[
			WindowTitleBarButtons
		];
}

const FSlateBrush* SPIEPreviewWindowTitleBar::GetScreenRotationButtonImage() const
{
	TSharedPtr<SWindow> OwnerWindow = OwnerWindowPtr.Pin();
	
	if (!OwnerWindow.IsValid())
	{
		return nullptr;
	}

	if (ScreenRotationButton->IsPressed())
	{
		return &FPIEPreviewWindowCoreStyle::Get().GetWidgetStyle<FPIEPreviewWindowStyle>("PIEWindow").ScreenRotationButtonStyle.Pressed;
	}
	else if (ScreenRotationButton->IsHovered())
	{
		return &FPIEPreviewWindowCoreStyle::Get().GetWidgetStyle<FPIEPreviewWindowStyle>("PIEWindow").ScreenRotationButtonStyle.Hovered;
	}
	else
	{
		return &FPIEPreviewWindowCoreStyle::Get().GetWidgetStyle<FPIEPreviewWindowStyle>("PIEWindow").ScreenRotationButtonStyle.Normal;
	}
}

#endif
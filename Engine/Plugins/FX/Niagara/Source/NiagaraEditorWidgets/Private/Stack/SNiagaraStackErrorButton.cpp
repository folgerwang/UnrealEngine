// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraStackErrorButton.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"

#define	LOCTEXT_NAMESPACE "SNiagaraStackErrorButton"

void SNiagaraStackErrorButton::Construct(const FArguments& InArgs)
{
	IssueSeverity = InArgs._IssueSeverity;
	ErrorTooltip = InArgs._ErrorTooltip;
	OnButtonClicked = InArgs._OnButtonClicked;
	FName IconName("Icons.Error");
	switch (IssueSeverity.Get())
	{
		case EStackIssueSeverity::Error:
			IconName = "Icons.Error";
			break;
		case EStackIssueSeverity::Warning:
			IconName = "Icons.Warning";
			break;
		case EStackIssueSeverity::Info:
			IconName = "Icons.Info";
			break;
		default:
			IconName = "Icons.Warning";
			break;
	}
	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ForegroundColor(FSlateColor::UseForeground())
		.ToolTipText(ErrorTooltip)
		.ContentPadding(FMargin(1.0f, 0.0f))
		.OnClicked(OnButtonClicked)
		.Content()
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(IconName))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE

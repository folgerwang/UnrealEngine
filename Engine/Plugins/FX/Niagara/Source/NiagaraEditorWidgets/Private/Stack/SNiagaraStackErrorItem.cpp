// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackErrorItem.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SNiagaraStackErrorItem"

void SNiagaraStackErrorItem::Construct(const FArguments& InArgs, UNiagaraStackErrorItem* InErrorItem, UNiagaraStackViewModel* InStackViewModel)
{
	ErrorItem = InErrorItem;
	StackViewModel = InStackViewModel;
	TSharedPtr<SHorizontalBox> ErrorInternalBox = SNew(SHorizontalBox);
	ErrorInternalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
			.Text_UObject(ErrorItem, &UNiagaraStackErrorItem::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStackErrorItem::GetTextColorForSearch)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
		];
	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("Icons.Error"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			ErrorInternalBox.ToSharedRef()
		]
	];
}

void SNiagaraStackErrorItemFix::Construct(const FArguments& InArgs, UNiagaraStackErrorItemFix* InErrorItem, UNiagaraStackViewModel* InStackViewModel)
{
	ErrorItem = InErrorItem;
	StackViewModel = InStackViewModel;
	TSharedPtr<SHorizontalBox> ErrorInternalBox = SNew(SHorizontalBox);
	ErrorInternalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text_UObject(ErrorItem, &UNiagaraStackErrorItemFix::FixDescription)
				.ColorAndOpacity(this, &SNiagaraStackErrorItemFix::GetTextColorForSearch)
				.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
		];
	ErrorInternalBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(10, 0)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
					.Text_UObject(ErrorItem, &UNiagaraStackErrorItemFix::GetFixButtonText)
					.OnClicked_UObject(ErrorItem, &UNiagaraStackErrorItemFix::OnTryFixError)
				]
		];
	ChildSlot
	[
		SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				ErrorInternalBox.ToSharedRef()
			]
	];
}

#undef LOCTEXT_NAMESPACE //"SNiagaraStackErrorItem"


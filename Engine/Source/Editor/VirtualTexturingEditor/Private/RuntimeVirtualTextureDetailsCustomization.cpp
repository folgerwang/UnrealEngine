// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureDetailsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "RuntimeVirtualTexturePlane.h"
#include "SResetToDefaultMenu.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

FRuntimeVirtualTextureDetailsCustomization::FRuntimeVirtualTextureDetailsCustomization()
	: VirtualTexture(nullptr)
{
}

TSharedRef<IDetailCustomization> FRuntimeVirtualTextureDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FRuntimeVirtualTextureDetailsCustomization);
}

namespace
{
	// Helper for adding text containing real values to the properties that are edited as power (or multiple) of 2
	void AddTextToProperty(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& CategoryBuilder, FName const& PropertyName, TSharedPtr<STextBlock>& TextBlock)
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(PropertyName);
		DetailBuilder.HideProperty(PropertyHandle);

		TSharedPtr<SResetToDefaultMenu> ResetToDefaultMenu;

		CategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(4.0f)
			[
				SNew(SWrapBox)
				.UseAllottedWidth(true)

				+ SWrapBox::Slot()
				.Padding(FMargin(0.0f, 2.0f, 2.0f, 0.0f))
				[
					SAssignNew(TextBlock, STextBlock)
				]
			]

			+ SHorizontalBox::Slot()
			[
				PropertyHandle->CreatePropertyValueWidget()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f)
			[
				// Would be better to use SResetToDefaultPropertyEditor here but that is private in the PropertyEditor lib
				SAssignNew(ResetToDefaultMenu, SResetToDefaultMenu)
			]
		];

		ResetToDefaultMenu->AddProperty(PropertyHandle.ToSharedRef());
	}
}

void FRuntimeVirtualTextureDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get and store the linked URuntimeVirtualTexture
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}
	VirtualTexture = Cast<URuntimeVirtualTexture>(ObjectsBeingCustomized[0].Get());
	if (VirtualTexture == nullptr)
	{
		return;
	}

	// Add size helpers
	IDetailCategoryBuilder& SizeCategory = DetailBuilder.EditCategory("Size", FText::GetEmpty());
	AddTextToProperty(DetailBuilder, SizeCategory, "Width", WidthText);
	AddTextToProperty(DetailBuilder, SizeCategory, "Height", HeightText);
	AddTextToProperty(DetailBuilder, SizeCategory, "TileSize", TileSizeText);
	AddTextToProperty(DetailBuilder, SizeCategory, "TileBorderSize", TileBorderSizeText);
	AddTextToProperty(DetailBuilder, SizeCategory, "RemoveLowMips", RemoveLowMipsText);

	// Add details block
	IDetailCategoryBuilder& DetailsCategory = DetailBuilder.EditCategory("Details", FText::GetEmpty(), ECategoryPriority::Important);
	static const FText RowText = LOCTEXT("Category_Details", "Details");
	DetailsCategory.AddCustomRow(RowText)
	.WholeRowContent()
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(4.0f)
		[
			SAssignNew(PageTableTextureMemoryText, STextBlock)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(4.0f)
		[
			SAssignNew(PhysicalTextureMemoryText, STextBlock)
		]
	];

	// Add refresh callback for all properties 
	DetailBuilder.GetProperty(FName(TEXT("Width")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));
	DetailBuilder.GetProperty(FName(TEXT("Height")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));
	DetailBuilder.GetProperty(FName(TEXT("TileSize")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));
	DetailBuilder.GetProperty(FName(TEXT("TileBorderSize")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));
	DetailBuilder.GetProperty(FName(TEXT("MaterialType")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));
	DetailBuilder.GetProperty(FName(TEXT("bCompressTextures")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));
	DetailBuilder.GetProperty(FName(TEXT("RemoveLowMips")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetails));

	// Initialize text blocks
	RefreshDetails();
}

void FRuntimeVirtualTextureDetailsCustomization::RefreshDetails()
{
	FNumberFormattingOptions SizeOptions;
	SizeOptions.UseGrouping = false;
	SizeOptions.MaximumFractionalDigits = 0;

 	WidthText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetWidth(), &SizeOptions)));
 	HeightText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetHeight(), &SizeOptions)));
 	TileSizeText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetTileSize(), &SizeOptions)));
 	TileBorderSizeText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetTileBorderSize(), &SizeOptions)));
 	RemoveLowMipsText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetRemoveLowMips(), &SizeOptions)));

	PageTableTextureMemoryText->SetText(FText::Format(LOCTEXT("Details_PageTableMemory", "Page Table Texture Memory (estimated): {0} KiB"), FText::AsNumber(VirtualTexture->GetEstimatedPageTableTextureMemoryKb(), &SizeOptions)));
	PhysicalTextureMemoryText->SetText(FText::Format(LOCTEXT("Details_PhysicalMemory", "Physical Texture Memory (estimated): {0} KiB"), FText::AsNumber(VirtualTexture->GetEstimatedPhysicalTextureMemoryKb(), &SizeOptions)));
}


FRuntimeVirtualTextureComponentDetailsCustomization::FRuntimeVirtualTextureComponentDetailsCustomization()
{
}

TSharedRef<IDetailCustomization> FRuntimeVirtualTextureComponentDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FRuntimeVirtualTextureComponentDetailsCustomization);
}

void FRuntimeVirtualTextureComponentDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get and store the linked ARuntimeRuntimeVirtualTextureComponent
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}
	RuntimeVirtualTextureComponent = Cast<URuntimeVirtualTextureComponent>(ObjectsBeingCustomized[0].Get());
	if (RuntimeVirtualTextureComponent == nullptr)
	{
		return;
	}

	// Use SourceActor property to add buttons
	TSharedPtr<IPropertyHandle> SourceActorValue = DetailBuilder.GetProperty("BoundsSourceActor");
	DetailBuilder.HideProperty(SourceActorValue);

	IDetailCategoryBuilder& BoundsCategory = DetailBuilder.EditCategory("TransformFromBounds", FText::GetEmpty(), ECategoryPriority::Important);
	BoundsCategory.AddCustomRow(SourceActorValue->GetPropertyDisplayName())
	.NameContent()
	[
		SourceActorValue->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.FillWidth(5.0f)
		[
			SourceActorValue->CreatePropertyValueWidget()
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			
			+ SWrapBox::Slot()
			.Padding(FMargin(0.0f, 2.0f, 2.0f, 0.0f))
			[
				SNew(SBox)
				[
					SNew(SVerticalBox)
					
					+ SVerticalBox::Slot()
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("Button_CopyRotation", "Copy Rotation"))
						.ToolTipText(LOCTEXT("Button_CopyRotation_Tooltip", "Set the virtual texture rotation to match the source actor"))
						.OnClicked(this, &FRuntimeVirtualTextureComponentDetailsCustomization::SetRotation)
					]

					+ SVerticalBox::Slot()
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("Button_CopyBounds", "Copy Bounds"))
						.ToolTipText(LOCTEXT("Button_CopyBounds_Tooltip", "Set the virtual texture transform so that it includes the full bounds of the source actor"))
						.OnClicked(this, &FRuntimeVirtualTextureComponentDetailsCustomization::SetTransformToBounds)
					]
				]
			]
		]
	];
}

FReply FRuntimeVirtualTextureComponentDetailsCustomization::SetRotation()
{
	RuntimeVirtualTextureComponent->SetRotation();
	return FReply::Handled();
}

FReply FRuntimeVirtualTextureComponentDetailsCustomization::SetTransformToBounds()
{
	RuntimeVirtualTextureComponent->SetTransformToBounds();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

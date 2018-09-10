// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaSourceDetailCustomization.h"

#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "ObjectEditorUtils.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/MediaPermutationsSelectorBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SMediaPermutationsSelector.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "AjaMediaSourceDetailCustomization"


void FAjaMediaSourceDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	for (const TWeakObjectPtr<UObject>& Obj : Objects)
	{
		if (UAjaMediaSource* Source = Cast<UAjaMediaSource>(Obj.Get()))
		{
			MediaSources.Add(Source);
		}
	}

	if (MediaSources.Num() == 0)
	{
		return;
	}

	IDetailCategoryBuilder& SourceCategory = DetailBuilder.EditCategory("Source");
	FDetailWidgetRow& UpdateRevisionRow = SourceCategory.AddCustomRow(LOCTEXT("Configuration", "Configuration"))
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0, 1, 0, 1))
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConfigurationLabel", "Configuration"))
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(MakeAttributeLambda([this]
			{
				FText Result;
				if (MediaSources[0].IsValid())
				{
					FAjaMediaConfiguration Configuration = MediaSources[0]->GetMediaConfiguration();
					Result = Configuration.ToText();
					for (int32 Index = 1; Index < MediaSources.Num(); ++Index)
					{
						if (MediaSources[Index]->GetMediaConfiguration() != Configuration)
						{
							Result = LOCTEXT("MultipleValues", "Multiple Values");
							break;
						}
					}
				}
				return Result;
			}))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FAjaMediaSourceDetailCustomization::HandleSourceComboButtonMenuContent)
			.ContentPadding(FMargin(4.0, 2.0))
		]
	];
}

TSharedRef<SWidget> FAjaMediaSourceDetailCustomization::HandleSourceComboButtonMenuContent()
{
	PermutationSelector.Reset();
	if (MediaSources.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	TArray<FAjaMediaConfiguration> MediaConfiguration;
	bool bHasInputConfiguration = FAjaMediaFinder::GetInputConfigurations(MediaConfiguration);
	if (!bHasInputConfiguration || MediaConfiguration.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	if (MediaSources[0].IsValid())
	{
		SelectedConfiguration = MediaSources[0]->GetMediaConfiguration();
	}

	auto QuadTypeVisible = [](FName ColumnName, const TArray<FAjaMediaConfiguration>& UniquePermutationsForThisColumn)
	{
		if (UniquePermutationsForThisColumn.Num() > 0)
		{
			return UniquePermutationsForThisColumn[0].MediaPort.LinkType == EAjaLinkType::QuadLink;
		}
		return false;
	};

	using TSelection = SMediaPermutationsSelector<FAjaMediaConfiguration, FMediaPermutationsSelectorBuilder>;
	TSharedRef<TSelection> Selector = SNew(TSelection)
		.PermutationsSource(MoveTemp(MediaConfiguration))
		.SelectedPermutation(SelectedConfiguration)
		.OnSelectionChanged(this, &FAjaMediaSourceDetailCustomization::OnSelectionChanged)
		.OnButtonClicked(this, &FAjaMediaSourceDetailCustomization::OnButtonClicked)
		+TSelection::Column(FMediaPermutationsSelectorBuilder::NAME_DeviceIndex)
		.Label(LOCTEXT("DeviceLabel", "Device"))
		+ TSelection::Column(FMediaPermutationsSelectorBuilder::NAME_SourceType)
		.Label(LOCTEXT("SourceTypeLabel", "Source"))
		+ TSelection::Column(FMediaPermutationsSelectorBuilder::NAME_QuadType)
		.Label(LOCTEXT("QuadTypeLabel", "Quad"))
		.IsColumnVisible_Lambda(QuadTypeVisible)
		+ TSelection::Column(FMediaPermutationsSelectorBuilder::NAME_Resolution)
		.Label(LOCTEXT("ResolutionLabel", "Resolution"))
		+ TSelection::Column(FMediaPermutationsSelectorBuilder::NAME_Standard)
		.Label(LOCTEXT("StandardLabel", "Standard"))
		+ TSelection::Column(FMediaPermutationsSelectorBuilder::NAME_FrameRate)
		.Label(LOCTEXT("FrameRateLabel", "Frame Rate"));
	PermutationSelector = Selector;

	return Selector;
}

void FAjaMediaSourceDetailCustomization::OnSelectionChanged(FAjaMediaConfiguration SelectedItem)
{
	SelectedConfiguration = SelectedItem;
}

FReply FAjaMediaSourceDetailCustomization::OnButtonClicked() const
{
	for (const TWeakObjectPtr<UAjaMediaSource>& Obj : MediaSources)
	{
		FObjectEditorUtils::SetPropertyValue(Obj.Get(), GET_MEMBER_NAME_CHECKED(UAjaMediaSource, MediaPort), SelectedConfiguration.MediaPort);
		FObjectEditorUtils::SetPropertyValue(Obj.Get(), GET_MEMBER_NAME_CHECKED(UAjaMediaSource, bIsDefaultModeOverriden), true);
		FObjectEditorUtils::SetPropertyValue(Obj.Get(), GET_MEMBER_NAME_CHECKED(UAjaMediaSource, MediaMode), SelectedConfiguration.MediaMode);
	}

	TSharedPtr<SWidget> SharedPermutationSelector = PermutationSelector.Pin();
	if (SharedPermutationSelector.IsValid())
	{
		TSharedRef<SWindow> ParentContextMenuWindow = FSlateApplication::Get().FindWidgetWindow(SharedPermutationSelector.ToSharedRef()).ToSharedRef();
		FSlateApplication::Get().RequestDestroyWindow(ParentContextMenuWindow);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

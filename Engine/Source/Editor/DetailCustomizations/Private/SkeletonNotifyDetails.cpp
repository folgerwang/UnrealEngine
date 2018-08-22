// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SkeletonNotifyDetails.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Animation/EditorSkeletonNotifyObj.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "Misc/ScopedSlowTask.h"
#include "IEditableSkeleton.h"
#include "Animation/AnimSequenceBase.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SkeletonNotifyDetails"

TSharedRef<IDetailCustomization> FSkeletonNotifyDetails::MakeInstance()
{
	return MakeShareable( new FSkeletonNotifyDetails );
}

void FSkeletonNotifyDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Skeleton Notify", LOCTEXT("SkeletonNotifyCategoryName", "Skeleton Notify") );
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	Category.AddProperty("Name").DisplayName( LOCTEXT("SkeletonNotifyName", "Notify Name") );

	TSharedPtr<IPropertyHandle> InPropertyHandle = DetailBuilder.GetProperty("AnimationNames");

	TArray< TWeakObjectPtr<UObject> > SelectedObjects =  DetailBuilder.GetSelectedObjects();

	UEditorSkeletonNotifyObj* EdObj = NULL;
	for(int i = 0; i < SelectedObjects.Num(); ++i)
	{
		UObject* Obj = SelectedObjects[i].Get();
		EdObj = Cast<UEditorSkeletonNotifyObj>(Obj);
		if(EdObj)
		{
			break;
		}
	}

	if(EdObj)
	{
		NotifyObject = EdObj;

		Category.AddCustomRow(LOCTEXT("AnimationsLabel","Animations"))
		.NameContent()
		[
			SNew(STextBlock)
			.ToolTipText(LOCTEXT("Animations_Tooltip", "List of animations that reference this notify"))
			.Text( LOCTEXT("AnimationsLabel","Animations") )
			.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Visibility_Lambda([this](){ return AnimationNames.Num() > 0 ? EVisibility::Collapsed : EVisibility::Visible; })
				.Text(LOCTEXT("ScanForAnimations", "Scan"))
				.ToolTipText(LOCTEXT("ScanForAnimationsTooltip", "Scan for animations that reference this notify"))
				.OnClicked(this, &FSkeletonNotifyDetails::CollectSequencesUsingNotify)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.MaxDesiredHeight(300.0f)
				.MaxDesiredWidth(200.0f)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					[
						SAssignNew(ListView, SListView<TSharedPtr<FString>>)
						.Visibility_Lambda([this](){ return AnimationNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
						.ListItemsSource(&AnimationNames)
						.OnGenerateRow(this, &FSkeletonNotifyDetails::MakeAnimationRow)
					]
				]
			]
		];
	}
}

TSharedRef< ITableRow > FSkeletonNotifyDetails::MakeAnimationRow( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		SNew(STextBlock)
		.ToolTipText(FText::FromString(*Item.Get()))
		.Text(FText::FromString(*Item.Get()))
	];
}

FReply FSkeletonNotifyDetails::CollectSequencesUsingNotify()
{
	if(NotifyObject.IsValid() && NotifyObject->EditableSkeleton.IsValid())
	{
		FScopedSlowTask SlowTask(1.0f, FText::Format(LOCTEXT("ScanningAnimationMessage", "Looking for animations that reference notify '{0}'."), FText::FromName(NotifyObject->Name)));
		SlowTask.MakeDialog(true);

		AnimationNames.Empty();

		TArray<FAssetData> CompatibleAnimSequences;
		NotifyObject->EditableSkeleton.Pin()->GetCompatibleAnimSequences(CompatibleAnimSequences);
		SlowTask.TotalAmountOfWork = CompatibleAnimSequences.Num();

		for( int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex )
		{
			SlowTask.EnterProgressFrame(1.0f);

			if(SlowTask.ShouldCancel())
			{
				break;
			}

			const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
			if(UObject* AnimSeqAsset = PossibleAnimSequence.GetAsset())
			{
				UAnimSequenceBase* Sequence = CastChecked<UAnimSequenceBase>(AnimSeqAsset);
				for (int32 NotifyIndex = 0; NotifyIndex < Sequence->Notifies.Num(); ++NotifyIndex)
				{
					FAnimNotifyEvent& NotifyEvent = Sequence->Notifies[NotifyIndex];
					if (NotifyEvent.NotifyName == NotifyObject->Name)
					{
						AnimationNames.Add(MakeShareable(new FString(PossibleAnimSequence.AssetName.ToString())));
						break;
					}
				}
			}
		}

		if(SlowTask.ShouldCancel())
		{
			AnimationNames.Empty();
		}

		ListView->RequestListRefresh();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

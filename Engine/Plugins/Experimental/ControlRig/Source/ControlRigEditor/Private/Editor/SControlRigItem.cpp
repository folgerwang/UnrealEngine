// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SControlRigItem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ControlRigBlueprintCommands.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ControlRigEditor.h"

#define LOCTEXT_NAMESPACE "SControlRigItem"

void SControlRigItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<SControlRig> InParentWidget, TSharedRef<FControlRigTreeNode> InControlRigTreeNode, TSharedRef<FUICommandList> InCommandList)
{
	WeakParentWidget = InParentWidget;
	WeakControlRigTreeNode = InControlRigTreeNode;
	WeakCommandList = InCommandList;

	STableRow<TSharedPtr<FControlRigTreeNode>>::Construct(
		STableRow<TSharedPtr<FControlRigTreeNode>>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer)
				.Size(FVector2D(8.0f, 1.0f))
			]
			+SHorizontalBox::Slot()
			.Padding(1.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(InControlRigTreeNode->BlueprintActionUiSpec.Icon.GetIcon())
				.ColorAndOpacity(InControlRigTreeNode->BlueprintActionUiSpec.IconTint)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer)
				.Size(FVector2D(3.0f, 1.0f))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SInlineEditableTextBlock)
/*				.HighlightText(this, &SControlRig::GetCurrentSearchString)*/		
				.Text(this, &SControlRigItem::GetItemText)
				.OnVerifyTextChanged(this, &SControlRigItem::OnVerifyNameTextChanged)
				.OnTextCommitted(this, &SControlRigItem::OnNameTextCommited)
				.IsSelected(this, &SControlRigItem::IsSelectedExclusively)
			]
		], OwnerTable);
}

FReply SControlRigItem::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FMenuBuilder MenuBuilder(true, WeakCommandList.Pin());

		MenuBuilder.BeginSection(TEXT("ControlRigItem"), LOCTEXT("ControlRigItemHeader", "Control Rig Item"));
		{
			MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().DeleteItem);
		}
		MenuBuilder.EndSection();

		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}

	return STableRow<TSharedPtr<FControlRigTreeNode>>::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FText SControlRigItem::GetItemText() const
{
	return WeakControlRigTreeNode.Pin()->BlueprintActionUiSpec.MenuName;
}

bool SControlRigItem::OnVerifyNameTextChanged(const FText& InText, FText& OutErrorText)
{
	FString CurrentName = GetItemText().ToString();
	FString NewName = InText.ToString();

	if(!NameValidator.IsValid())
	{
		NameValidator = MakeShared<FKismetNameValidator>(WeakParentWidget.Pin()->ControlRigEditor.Pin()->GetBlueprintObj(), *CurrentName);
	}

	EValidatorResult Result = NameValidator->IsValid(NewName);
	OutErrorText = FText::FromString(NameValidator->GetErrorString(NewName, Result));

	return Result == EValidatorResult::Ok;
}

void SControlRigItem::OnNameTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	NameValidator = nullptr;

	FString CurrentName = GetItemText().ToString();
	UBlueprint* Blueprint = WeakParentWidget.Pin()->ControlRigEditor.Pin()->GetBlueprintObj();
	FBlueprintEditorUtils::RenameMemberVariable(Blueprint, *CurrentName, *InText.ToString());

	// A bit 'nuke it from orbit', but does the trick
	// we need to reconstruct nodes that use the renamed variable. They cant handle this during the rename itself
	// because the variable is still using the old name a in the compiled skeleton class at the time they get called
	FBlueprintEditorUtils::ReconstructAllNodes(Blueprint);
}

bool SControlRigItem::IsSelectedExclusively() const
{
	return OwnerTablePtr.Pin()->Private_GetNumSelectedItems() == 1 && OwnerTablePtr.Pin()->Private_IsItemSelected(WeakControlRigTreeNode.Pin());
}

#undef LOCTEXT_NAMESPACE
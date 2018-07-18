// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STableRow.h"
#include "SControlRig.h"

class INameValidatorInterface;

/** A widget representing single control rig tree item */
class SControlRigItem : public STableRow<TSharedPtr<FControlRigTreeNode>>
{
	SLATE_BEGIN_ARGS(SControlRigItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<SControlRig> InParentWidget, TSharedRef<FControlRigTreeNode> InControlRigTreeNode, TSharedRef<FUICommandList> InCommandList);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FText GetItemText() const;

	bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorText);

	void OnNameTextCommited(const FText& InText, ETextCommit::Type InCommitType);

	bool IsSelectedExclusively() const;

private:
	TWeakPtr<SControlRig> WeakParentWidget;

	TWeakPtr<FControlRigTreeNode> WeakControlRigTreeNode;

	TWeakPtr<FUICommandList> WeakCommandList;

	TSharedPtr<INameValidatorInterface> NameValidator;
};
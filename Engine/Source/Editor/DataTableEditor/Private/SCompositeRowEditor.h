// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Kismet2/StructureEditorUtils.h"
#include "DataTableEditorUtils.h"
#include "Misc/NotifyHook.h"
#include "Widgets/Input/SComboBox.h"
#include "SRowEditor.h"


class SCompositeRowEditor : public SRowEditor
{
	SLATE_BEGIN_ARGS(SRowEditor) {}
	SLATE_END_ARGS()

	SCompositeRowEditor();
	virtual ~SCompositeRowEditor();

	void Construct(const FArguments& InArgs, UDataTable* Changed);

protected:
	virtual FReply OnAddClicked() override;
	virtual FReply OnRemoveClicked() override;
	virtual FReply OnMoveRowClicked(FDataTableEditorUtils::ERowMoveDirection MoveDirection) override;

	virtual bool IsMoveRowUpEnabled() const override;
	virtual bool IsMoveRowDownEnabled() const override;
	virtual bool IsAddRowEnabled() const override;
	virtual bool IsRemoveRowEnabled() const override;
	virtual EVisibility GetRenameVisibility() const override;
};

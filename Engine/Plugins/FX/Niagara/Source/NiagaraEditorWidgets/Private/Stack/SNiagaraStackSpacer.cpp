// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraStackSpacer.h"
#include "SDropTarget.h"
#include "NiagaraTypes.h"
#include "NiagaraActions.h"
#include "NiagaraConstants.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"
#include "NiagaraEditorWidgetsStyle.h"

void SNiagaraStackSpacer::Construct(const FArguments& InArgs, UNiagaraStackSpacer& InStackSpacer) 
{
	HeightOverride = InArgs._HeightOverride;

	StackSpacer = &InStackSpacer;

	ChildSlot
	[
		SNew(SDropTarget)
		.OnAllowDrop(this, &SNiagaraStackSpacer::OnSStackSpacerAllowDrop)
		.OnDrop(this, &SNiagaraStackSpacer::OnSStackSpacerDrop)
		.HorizontalImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.Stack.DropTarget.BorderHorizontal"))
		.VerticalImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.Stack.DropTarget.BorderVertical"))
		.BackgroundColor(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.DropTarget.BackgroundColor"))
		.BackgroundColorHover(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.DropTarget.BackgroundColorHover"))
		.Content()
		[
			SNew(SBox)
			.HeightOverride(HeightOverride)
		]
	];
	
}

bool SNiagaraStackSpacer::OnSStackSpacerAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if (StackSpacer)
	{
		return StackSpacer->OnStackSpacerAllowDrop(DragDropOperation);
	}
	return false;
}

FReply SNiagaraStackSpacer::OnSStackSpacerDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if (StackSpacer)
	{
		return StackSpacer->OnStackSpacerDrop(DragDropOperation);
	}
	return FReply::Unhandled();
}



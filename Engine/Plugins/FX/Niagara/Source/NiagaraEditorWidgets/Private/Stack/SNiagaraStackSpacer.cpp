// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraStackSpacer.h"
#include "SDropTarget.h"
#include "NiagaraTypes.h"
#include "NiagaraActions.h"
#include "NiagaraConstants.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"

void SNiagaraStackSpacer::Construct(const FArguments& InArgs, UNiagaraStackSpacer& InStackSpacer) 
{
	HeightOverride = InArgs._HeightOverride;

	StackSpacer = &InStackSpacer;

	ChildSlot
	[
		SNew(SDropTarget)
		.OnAllowDrop(this, &SNiagaraStackSpacer::OnSStackSpacerAllowDrop)
		.OnDrop(this, &SNiagaraStackSpacer::OnSStackSpacerDrop)
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



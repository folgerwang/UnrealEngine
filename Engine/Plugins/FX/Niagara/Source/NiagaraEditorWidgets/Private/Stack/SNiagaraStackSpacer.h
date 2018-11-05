// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"

class SNiagaraStackSpacer : public SCompoundWidget 
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackSpacer)

	// Ported HeightOverride from SBox to expose setting box size when instantiating in SNiagaraStack
	: _HeightOverride(FOptionalSize())
	{ }
	/** When specified, ignore the content's desired size and report the HeightOverride as the Box's desired height. */
	SLATE_ATTRIBUTE(FOptionalSize, HeightOverride)

	SLATE_END_ARGS(); 

	void Construct(const FArguments& InArgs, UNiagaraStackSpacer& InStackSpacer);

private:
	UNiagaraStackSpacer* StackSpacer;

	bool OnSStackSpacerAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const;
	FReply OnSStackSpacerDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const;

	/** When specified, ignore the content's desired size and report the.HeightOverride as the Box's desired height. */
	TAttribute<FOptionalSize> HeightOverride;
};
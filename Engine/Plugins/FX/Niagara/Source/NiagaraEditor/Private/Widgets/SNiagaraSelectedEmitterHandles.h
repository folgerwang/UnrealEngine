// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/GCObject.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "EditorStyleSet.h"

class FNiagaraSystemViewModel;
class UNiagaraStackViewModel;
class SNiagaraStack;
class SBox;
class SSplitter;

class SNiagaraSelectedEmitterHandles : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSelectedEmitterHandles)
	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);

	~SNiagaraSelectedEmitterHandles();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void SelectedEmitterHandlesChanged();

	void EmitterHandleViewModelsChanged();

	void RefreshEmitterWidgets();

	EVisibility GetUnsupportedSelectionTextVisibility() const;

	FText GetUnsupportedSelectionText() const;

	void OnEmitterPinnedChanged();

	void ResetWidgets();

	void ResetViewModels();

	void CollapseToHeaders();

private:
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;

	TArray<UNiagaraStackViewModel*> StackViewModels;

	TSharedPtr<SSplitter> EmitterSplitter;
};

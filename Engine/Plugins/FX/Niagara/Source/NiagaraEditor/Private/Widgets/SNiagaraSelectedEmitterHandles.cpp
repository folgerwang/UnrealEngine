// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSelectedEmitterHandles.h"
#include "NiagaraEditorStyle.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorCommands.h"
#include "Widgets/Layout/SSplitter.h"

#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NiagaraSelectedEmittersHandle"

void SNiagaraSelectedEmitterHandles::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModel = InSystemViewModel;
	SystemViewModel->OnSelectedEmitterHandlesChanged().AddRaw(this, &SNiagaraSelectedEmitterHandles::SelectedEmitterHandlesChanged);
	SystemViewModel->OnEmitterHandleViewModelsChanged().AddRaw(this, &SNiagaraSelectedEmitterHandles::EmitterHandleViewModelsChanged);
	SystemViewModel->GetOnPinnedEmittersChanged().AddRaw(this, &SNiagaraSelectedEmitterHandles::OnEmitterPinnedChanged);

	{
		TSharedPtr<FUICommandList> ToolkitCommands = SystemViewModel->GetToolkitCommands();
		ToolkitCommands->MapAction(
			FNiagaraEditorCommands::Get().CollapseStackToHeaders,
			FExecuteAction::CreateSP(this, &SNiagaraSelectedEmitterHandles::CollapseToHeaders));
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(EmitterSplitter, SSplitter)
			.MinimumSlotHeight(150.0f)
		]
		+ SOverlay::Slot()
		.Padding(0, 20, 0, 0)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SNiagaraSelectedEmitterHandles::GetUnsupportedSelectionText)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.SelectedEmitter.UnsupportedSelectionText")
			.Visibility(this, &SNiagaraSelectedEmitterHandles::GetUnsupportedSelectionTextVisibility)
		]
	];
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	RefreshEmitterWidgets();
}

SNiagaraSelectedEmitterHandles::~SNiagaraSelectedEmitterHandles()
{
	SystemViewModel->OnEmitterHandleViewModelsChanged().RemoveAll(this);
	SystemViewModel->OnSelectedEmitterHandlesChanged().RemoveAll(this);
	SystemViewModel->GetOnPinnedEmittersChanged().RemoveAll(this);

	ResetViewModels();
}

void SNiagaraSelectedEmitterHandles::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(StackViewModels);
}

void SNiagaraSelectedEmitterHandles::RefreshEmitterWidgets()
{
	ResetWidgets();
	ResetViewModels();

	TArray<TSharedPtr<FNiagaraEmitterHandleViewModel>> EmitterHandlesToDisplay;
	EmitterHandlesToDisplay.Append(SystemViewModel->GetPinnedEmitterHandles());
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	for (auto Handle : SelectedEmitterHandles)
	{
		EmitterHandlesToDisplay.AddUnique(Handle);
	}

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));

	for (auto EmitterHandleViewModel : EmitterHandlesToDisplay)
	{
		UNiagaraStackViewModel* StackModel = NewObject<UNiagaraStackViewModel>();
		StackModel->Initialize(SystemViewModel, EmitterHandleViewModel);
		StackViewModels.Add(StackModel);
		EmitterSplitter->AddSlot()
		[
			NiagaraEditorModule.CreateStackWidget(StackModel)
		];
	}
	EmitterHandlesToDisplay.Empty();
}

void SNiagaraSelectedEmitterHandles::ResetWidgets()
{
	while (EmitterSplitter->GetChildren()->Num() > 0)
	{
		int i = EmitterSplitter->GetChildren()->Num() - 1;
		EmitterSplitter->RemoveAt(i);
	}
}

void SNiagaraSelectedEmitterHandles::ResetViewModels()
{
	for (UNiagaraStackViewModel* StackViewModel : StackViewModels)
	{
		StackViewModel->Finalize();
	}
	StackViewModels.Empty();
}

void SNiagaraSelectedEmitterHandles::SelectedEmitterHandlesChanged()
{
	RefreshEmitterWidgets();
}

void SNiagaraSelectedEmitterHandles::EmitterHandleViewModelsChanged()
{
	RefreshEmitterWidgets();
}

void SNiagaraSelectedEmitterHandles::OnEmitterPinnedChanged()
{
	RefreshEmitterWidgets();
}

EVisibility SNiagaraSelectedEmitterHandles::GetUnsupportedSelectionTextVisibility() const
{
	return SystemViewModel->GetSelectedEmitterHandleIds().Num() != 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNiagaraSelectedEmitterHandles::GetUnsupportedSelectionText() const
{
	if (SystemViewModel->GetSelectedEmitterHandleIds().Num() == 0 && SystemViewModel->GetPinnedEmitterHandles().Num() == 0)
	{
		return LOCTEXT("NoSelectionMessage", "Select an emitter in the timeline.");
	}
	else
	{
		return FText();
	}
}

void SNiagaraSelectedEmitterHandles::CollapseToHeaders()
{
	for (UNiagaraStackViewModel* ViewModel : StackViewModels)
	{
		ViewModel->CollapseToHeaders();
	}
}

#undef LOCTEXT_NAMESPACE
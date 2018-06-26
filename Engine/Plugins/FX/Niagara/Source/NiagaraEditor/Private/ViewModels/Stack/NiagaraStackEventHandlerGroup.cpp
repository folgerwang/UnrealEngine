// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackEventHandlerGroup.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackEventScriptItemGroup.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/Stack/INiagaraStackItemGroupAddUtilities.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraStackEventHandlerGroup"

class FEventHandlerGroupAddUtilities : public TNiagaraStackItemGroupAddUtilities<FNiagaraEventScriptProperties>
{
public:
	FEventHandlerGroupAddUtilities(TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, FOnItemAdded InOnItemAdded)
		: TNiagaraStackItemGroupAddUtilities(LOCTEXT("ScriptGroupAddItemName", "Event Handler"), EAddMode::AddDirectly, false, InOnItemAdded)
		, EmitterViewModel(InEmitterViewModel)
	{
	}

	virtual void AddItemDirectly() override
	{
		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModelPinned = EmitterViewModel.Pin();
		if (EmitterViewModelPinned.IsValid() == false)
		{
			return;
		}

		UNiagaraEmitter* Emitter = EmitterViewModelPinned->GetEmitter();
		UNiagaraScriptSource* Source = EmitterViewModelPinned->GetSharedScriptViewModel()->GetGraphViewModel()->GetScriptSource();
		UNiagaraGraph* Graph = EmitterViewModelPinned->GetSharedScriptViewModel()->GetGraphViewModel()->GetGraph();

		// The stack should not have been created if any of these are null, so bail out if it happens somehow rather than try to handle all of these cases.
		checkf(Emitter != nullptr && Source != nullptr && Graph != nullptr, TEXT("Stack created for invalid emitter or graph."));

		FScopedTransaction ScopedTransaction(LOCTEXT("AddNewEventHandlerTransaction", "Add new event handler"));

		Emitter->Modify();
		FNiagaraEventScriptProperties EventScriptProperties;
		EventScriptProperties.Script = NewObject<UNiagaraScript>(Emitter, MakeUniqueObjectName(Emitter, UNiagaraScript::StaticClass(), "EventScript"), EObjectFlags::RF_Transactional);
		EventScriptProperties.Script->SetUsage(ENiagaraScriptUsage::ParticleEventScript);
		EventScriptProperties.Script->SetUsageId(FGuid::NewGuid());
		EventScriptProperties.Script->SetSource(Source);
		Emitter->AddEventHandler(EventScriptProperties);
		FNiagaraStackGraphUtilities::ResetGraphForOutput(*Graph, ENiagaraScriptUsage::ParticleEventScript, EventScriptProperties.Script->GetUsageId());

		// Set the emitter here so that the internal state of the view model is updated.
		// TODO: Move the logic for managing event handlers into the emitter view model or script view model.
		EmitterViewModelPinned->SetEmitter(Emitter);

		OnItemAdded.ExecuteIfBound(EventScriptProperties);
	}

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions) const override { unimplemented(); }
	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override { unimplemented(); }

private:
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModel;
};

void UNiagaraStackEventHandlerGroup::Initialize(FRequiredEntryData InRequiredEntryData)
{
	FText DisplayName = LOCTEXT("EventGroupName", "Add Event Handler");
	FText ToolTip = LOCTEXT("EventGroupTooltip", "Determines how this Emitter responds to incoming events. There can be more than one event handler script stack per Emitter.");
	AddUtilities = MakeShared<FEventHandlerGroupAddUtilities>(InRequiredEntryData.EmitterViewModel, 
		TNiagaraStackItemGroupAddUtilities<FNiagaraEventScriptProperties>::FOnItemAdded::CreateUObject(this, &UNiagaraStackEventHandlerGroup::ItemAddedFromUtilties));
	Super::Initialize(InRequiredEntryData, DisplayName, ToolTip, AddUtilities.Get());
}

void UNiagaraStackEventHandlerGroup::SetOnItemAdded(FOnItemAdded InOnItemAdded)
{
	ItemAddedDelegate = InOnItemAdded;
}

void UNiagaraStackEventHandlerGroup::ItemAddedFromUtilties(FNiagaraEventScriptProperties AddedEventHandler)
{
	ItemAddedDelegate.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE

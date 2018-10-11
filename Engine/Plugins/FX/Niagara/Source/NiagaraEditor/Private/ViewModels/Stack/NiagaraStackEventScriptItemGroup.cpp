// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackEventScriptItemGroup.h"
#include "NiagaraEmitter.h"
#include "ViewModels/Stack/NiagaraStackObject.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "NiagaraScriptSource.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraScriptMergeManager.h"
#include "NiagaraStackEditorData.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "Customizations/NiagaraEventScriptPropertiesCustomization.h"

#include "Internationalization/Internationalization.h"
#include "ScopedTransaction.h"
#include "IDetailTreeNode.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackEventScriptItemGroup"

void UNiagaraStackEventHandlerPropertiesItem::Initialize(FRequiredEntryData InRequiredEntryData, FGuid InEventScriptUsageId)
{
	FString EventStackEditorDataKey = FString::Printf(TEXT("Event-%s-Properties"), *InEventScriptUsageId.ToString(EGuidFormats::DigitsWithHyphens));
	Super::Initialize(InRequiredEntryData, EventStackEditorDataKey);

	EventScriptUsageId = InEventScriptUsageId;

	Emitter = GetEmitterViewModel()->GetEmitter();
	Emitter->OnPropertiesChanged().AddUObject(this, &UNiagaraStackEventHandlerPropertiesItem::EventHandlerPropertiesChanged);

	const UNiagaraEmitter* BaseEmitter = FNiagaraStackGraphUtilities::GetBaseEmitter(*Emitter.Get(), GetSystemViewModel()->GetSystem());
	if (BaseEmitter != nullptr && Emitter != BaseEmitter)
	{
		TSharedRef<FNiagaraScriptMergeManager> MergeManager = FNiagaraScriptMergeManager::Get();
		bHasBaseEventHandler = MergeManager->HasBaseEventHandler(*BaseEmitter, EventScriptUsageId);
	}
}

void UNiagaraStackEventHandlerPropertiesItem::FinalizeInternal()
{
	if (Emitter.IsValid())
	{
		Emitter->OnPropertiesChanged().RemoveAll(this);
	}
	Super::FinalizeInternal();
}

FText UNiagaraStackEventHandlerPropertiesItem::GetDisplayName() const
{
	return LOCTEXT("EventHandlerPropertiesDisplayName", "Event Handler Properties");
}

bool UNiagaraStackEventHandlerPropertiesItem::CanResetToBase() const
{
	if (bCanResetToBase.IsSet() == false)
	{
		if (bHasBaseEventHandler)
		{
			const UNiagaraEmitter* BaseEmitter = FNiagaraStackGraphUtilities::GetBaseEmitter(*Emitter.Get(), GetSystemViewModel()->GetSystem());
			if (BaseEmitter != nullptr && Emitter != BaseEmitter)
			{
				TSharedRef<FNiagaraScriptMergeManager> MergeManager = FNiagaraScriptMergeManager::Get();
				bCanResetToBase = MergeManager->IsEventHandlerPropertySetDifferentFromBase(*Emitter.Get(), *BaseEmitter, EventScriptUsageId);
			}
			else
			{
				bCanResetToBase = false;
			}
		}
		else
		{
			bCanResetToBase = false;
		}
	}
	return bCanResetToBase.GetValue();
}

void UNiagaraStackEventHandlerPropertiesItem::ResetToBase()
{
	if (bCanResetToBase.GetValue())
	{
		const UNiagaraEmitter* BaseEmitter = FNiagaraStackGraphUtilities::GetBaseEmitter(*Emitter.Get(), GetSystemViewModel()->GetSystem());
		TSharedRef<FNiagaraScriptMergeManager> MergeManager = FNiagaraScriptMergeManager::Get();
		MergeManager->ResetEventHandlerPropertySetToBase(*Emitter, *BaseEmitter, EventScriptUsageId);
		RefreshChildren();
	}
}

void UNiagaraStackEventHandlerPropertiesItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (EmitterObject == nullptr)
	{
		EmitterObject = NewObject<UNiagaraStackObject>(this);
		EmitterObject->Initialize(CreateDefaultChildRequiredData(), Emitter.Get(), GetStackEditorDataKey());
		EmitterObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraEventScriptProperties::StaticStruct()->GetFName(), 
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraEventScriptPropertiesCustomization::MakeInstance, 
			TWeakObjectPtr<UNiagaraSystem>(&GetSystemViewModel()->GetSystem()), TWeakObjectPtr<UNiagaraEmitter>(GetEmitterViewModel()->GetEmitter())));
		EmitterObject->SetOnSelectRootNodes(UNiagaraStackObject::FOnSelectRootNodes::CreateUObject(this, &UNiagaraStackEventHandlerPropertiesItem::SelectEmitterStackObjectRootTreeNodes));
	}

	NewChildren.Add(EmitterObject);

	bCanResetToBase.Reset();
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

void UNiagaraStackEventHandlerPropertiesItem::EventHandlerPropertiesChanged()
{
	bCanResetToBase.Reset();
}

TSharedPtr<IDetailTreeNode> GetEventHandlerArrayPropertyNode(const TArray<TSharedRef<IDetailTreeNode>>& Nodes)
{
	TArray<TSharedRef<IDetailTreeNode>> ChildrenToCheck;
	for (TSharedRef<IDetailTreeNode> Node : Nodes)
	{
		if (Node->GetNodeType() == EDetailNodeType::Item)
		{
			TSharedPtr<IPropertyHandle> NodePropertyHandle = Node->CreatePropertyHandle();
			if (NodePropertyHandle.IsValid() && NodePropertyHandle->GetProperty()->GetFName() == UNiagaraEmitter::PrivateMemberNames::EventHandlerScriptProps)
			{
				return Node;
			}
		}

		TArray<TSharedRef<IDetailTreeNode>> Children;
		Node->GetChildren(Children);
		ChildrenToCheck.Append(Children);
	}
	if (ChildrenToCheck.Num() == 0)
	{
		return nullptr;
	}
	return GetEventHandlerArrayPropertyNode(ChildrenToCheck);
}

void UNiagaraStackEventHandlerPropertiesItem::SelectEmitterStackObjectRootTreeNodes(TArray<TSharedRef<IDetailTreeNode>> Source, TArray<TSharedRef<IDetailTreeNode>>* Selected)
{
	TSharedPtr<IDetailTreeNode> EventHandlerArrayPropertyNode = GetEventHandlerArrayPropertyNode(Source);
	if (EventHandlerArrayPropertyNode.IsValid())
	{
		TArray<TSharedRef<IDetailTreeNode>> EventHandlerArrayItemNodes;
		EventHandlerArrayPropertyNode->GetChildren(EventHandlerArrayItemNodes);
		for (TSharedRef<IDetailTreeNode> EventHandlerArrayItemNode : EventHandlerArrayItemNodes)
		{
			TSharedPtr<IPropertyHandle> EventHandlerArrayItemPropertyHandle = EventHandlerArrayItemNode->CreatePropertyHandle();
			if (EventHandlerArrayItemPropertyHandle.IsValid())
			{
				UStructProperty* StructProperty = Cast<UStructProperty>(EventHandlerArrayItemPropertyHandle->GetProperty());
				if (StructProperty != nullptr && StructProperty->Struct == FNiagaraEventScriptProperties::StaticStruct())
				{
					TArray<void*> RawData;
					EventHandlerArrayItemPropertyHandle->AccessRawData(RawData);
					if (RawData.Num() == 1)
					{
						FNiagaraEventScriptProperties* EventScriptProperties = static_cast<FNiagaraEventScriptProperties*>(RawData[0]);
						if (EventScriptProperties->Script->GetUsageId() == EventScriptUsageId)
						{
							EventHandlerArrayItemNode->GetChildren(*Selected);
							return;
						}
					}
				}
			}
		}
	}
}

void UNiagaraStackEventScriptItemGroup::Initialize(
	FRequiredEntryData InRequiredEntryData,
	TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
	ENiagaraScriptUsage InScriptUsage,
	FGuid InScriptUsageId)
{
	FText ToolTip = LOCTEXT("EventGroupTooltip", "Determines how this Emitter responds to incoming events. There can be more than one event handler script stack per Emitter.");
	FText TempDisplayName = FText::Format(LOCTEXT("TempDisplayNameFormat", "Event Handler - {0}"), FText::FromString(InScriptUsageId.ToString(EGuidFormats::DigitsWithHyphens)));
	Super::Initialize(InRequiredEntryData, TempDisplayName, ToolTip, InScriptViewModel, InScriptUsage, InScriptUsageId);
}

void UNiagaraStackEventScriptItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	FName EventSpacerKey = *FString::Printf(TEXT("EventSpacer"));
	UNiagaraStackSpacer* SeparatorSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
		[=](UNiagaraStackSpacer* CurrentSpacer) { return CurrentSpacer->GetSpacerKey() == EventSpacerKey; });
	if (SeparatorSpacer == nullptr)
	{
		SeparatorSpacer = NewObject<UNiagaraStackSpacer>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(), GetExecutionCategoryName(), NAME_None, GetStackEditorData());
		SeparatorSpacer->Initialize(RequiredEntryData, EventSpacerKey);
	}
	NewChildren.Add(SeparatorSpacer);

	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();

	const FNiagaraEventScriptProperties* EventScriptProperties = Emitter->GetEventHandlers().FindByPredicate(
		[=](const FNiagaraEventScriptProperties& InEventScriptProperties) { return InEventScriptProperties.Script->GetUsageId() == GetScriptUsageId(); });

	if (EventScriptProperties != nullptr)
	{
		SetDisplayName(FText::Format(LOCTEXT("FormatEventScriptDisplayName", "Event Handler - Source: {0}"), FText::FromName(EventScriptProperties->SourceEventName)));

		const UNiagaraEmitter* BaseEmitter = FNiagaraStackGraphUtilities::GetBaseEmitter(*Emitter, GetSystemViewModel()->GetSystem());
		bHasBaseEventHandler = BaseEmitter != nullptr && FNiagaraScriptMergeManager::Get()->HasBaseEventHandler(*BaseEmitter, GetScriptUsageId());
	}
	else
	{
		SetDisplayName(LOCTEXT("UnassignedEventDisplayName", "Unassigned Event"));
		bHasBaseEventHandler = false;
	}

	if (EventHandlerProperties == nullptr)
	{
		EventHandlerProperties = NewObject<UNiagaraStackEventHandlerPropertiesItem>(this);
		EventHandlerProperties->Initialize(CreateDefaultChildRequiredData(), GetScriptUsageId());
	}
	NewChildren.Add(EventHandlerProperties);

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

bool UNiagaraStackEventScriptItemGroup::CanDelete() const
{
	return bHasBaseEventHandler == false;
}

bool UNiagaraStackEventScriptItemGroup::Delete()
{
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned = ScriptViewModel.Pin();
	checkf(ScriptViewModelPinned.IsValid(), TEXT("Can not delete when the script view model has been deleted."));

	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Emitter->GraphSource);

	if (!Source || !Source->NodeGraph)
	{
		return false;
	}

	FScopedTransaction Transaction(FText::Format(LOCTEXT("DeleteEventHandler", "Deleted {0}"), GetDisplayName()));
	Emitter->Modify();
	Source->NodeGraph->Modify();
	TArray<UNiagaraNode*> EventIndexNodes;
	Source->NodeGraph->BuildTraversal(EventIndexNodes, GetScriptUsage(), GetScriptUsageId());
	for (UNiagaraNode* Node : EventIndexNodes)
	{
		Node->Modify();
	}
	
	// First, remove the event handler script properties object.
	Emitter->RemoveEventHandlerByUsageId(GetScriptUsageId());
	
	// Now remove all graph nodes associated with the event script index.
	for (UNiagaraNode* Node : EventIndexNodes)
	{
		Node->DestroyNode();
	}

	// Set the emitter here to that the internal state of the view model is updated.
	// TODO: Move the logic for managing event handlers into the emitter view model or script view model.
	ScriptViewModelPinned->SetScripts(Emitter);
	
	OnModifiedEventHandlersDelegate.ExecuteIfBound();

	return true;
}

void UNiagaraStackEventScriptItemGroup::SetOnModifiedEventHandlers(FOnModifiedEventHandlers OnModifiedEventHandlers)
{
	OnModifiedEventHandlersDelegate = OnModifiedEventHandlers;
}

#undef LOCTEXT_NAMESPACE

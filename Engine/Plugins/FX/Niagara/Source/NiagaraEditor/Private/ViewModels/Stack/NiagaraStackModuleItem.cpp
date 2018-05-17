// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "NiagaraEditorModule.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackInputCategory.h"
#include "ViewModels/Stack/NiagaraStackModuleItemOutputCollection.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraGraph.h"
#include "NiagaraScript.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScriptMergeManager.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraCommon.h"
#include "Widgets/SWidget.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraStackModuleItem"

UNiagaraStackModuleItem::UNiagaraStackModuleItem()
	: FunctionCallNode(nullptr)
	, bCanRefresh(false)
	, InputCollection(nullptr)
{
}

const UNiagaraNodeFunctionCall& UNiagaraStackModuleItem::GetModuleNode() const
{
	return *FunctionCallNode;
}

UNiagaraNodeFunctionCall& UNiagaraStackModuleItem::GetModuleNode()
{
	return *FunctionCallNode;
}

void UNiagaraStackModuleItem::Initialize(FRequiredEntryData InRequiredEntryData, INiagaraStackItemGroupAddUtilities* InGroupAddUtilities, UNiagaraNodeFunctionCall& InFunctionCallNode)
{
	checkf(FunctionCallNode == nullptr, TEXT("Can not set the node more than once."));
	FString ModuleStackEditorDataKey = InFunctionCallNode.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
	Super::Initialize(InRequiredEntryData, ModuleStackEditorDataKey);
	GroupAddUtilities = InGroupAddUtilities;
	FunctionCallNode = &InFunctionCallNode;
	AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackModuleItem::FilterOutputCollection));

	// Update bCanMoveAndDelete
	if (GetSystemViewModel()->GetEditMode() == ENiagaraSystemViewModelEditMode::EmitterAsset)
	{
		// When editing emitters all modules can be moved and deleted.
		bCanMoveAndDelete = true;
	}
	else
	{
		// When editing systems only non-base modules can be moved and deleted.
		TSharedRef<FNiagaraScriptMergeManager> MergeManager = FNiagaraScriptMergeManager::Get();

		const UNiagaraEmitter* BaseEmitter = FNiagaraStackGraphUtilities::GetBaseEmitter(*GetEmitterViewModel()->GetEmitter(), GetSystemViewModel()->GetSystem());
		UNiagaraNodeOutput* OutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*FunctionCallNode);

		bool bIsMergeable = MergeManager->IsMergeableScriptUsage(OutputNode->GetUsage());
		bool bHasBaseModule = bIsMergeable && BaseEmitter != nullptr && MergeManager->HasBaseModule(*BaseEmitter, OutputNode->GetUsage(), OutputNode->GetUsageId(), FunctionCallNode->NodeGuid);
		bCanMoveAndDelete = bHasBaseModule == false;
	}
}

FText UNiagaraStackModuleItem::GetDisplayName() const
{
	if (FunctionCallNode != nullptr)
	{
		return FunctionCallNode->GetNodeTitle(ENodeTitleType::ListView);
	}
	else
	{
		return FText::FromName(NAME_None);
	}
}


FText UNiagaraStackModuleItem::GetTooltipText() const
{
	if (FunctionCallNode != nullptr)
	{
		return FunctionCallNode->GetTooltipText();
	}
	else
	{
		return FText();
	}
}

INiagaraStackItemGroupAddUtilities* UNiagaraStackModuleItem::GetGroupAddUtilities()
{
	return GroupAddUtilities;
}

void UNiagaraStackModuleItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	bCanRefresh = false;

	if (FunctionCallNode != nullptr && FunctionCallNode->ScriptIsValid())
	{
		// Determine if meta-data requires that we add our own refresh button here.
		if (FunctionCallNode->FunctionScript)
		{
			UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionCallNode->FunctionScript->GetSource());
			UNiagaraGraph* Graph = CastChecked<UNiagaraGraph>(Source->NodeGraph);
			const TMap<FNiagaraVariable, FNiagaraVariableMetaData>& MetaDataMap = Graph->GetAllMetaData();
			auto Iter = MetaDataMap.CreateConstIterator();
			while (Iter)
			{
				auto PropertyIter = Iter.Value().PropertyMetaData.CreateConstIterator();
				while (PropertyIter)
				{
					if (PropertyIter.Key() == (TEXT("DisplayNameArg0")))
					{
						bCanRefresh = true;
					}
					++PropertyIter;
				}
				++Iter;
			}
		}

		if (InputCollection == nullptr)
		{
			TArray<FString> InputParameterHandlePath;
			InputCollection = NewObject<UNiagaraStackFunctionInputCollection>(this);
			InputCollection->Initialize(CreateDefaultChildRequiredData(), *FunctionCallNode, *FunctionCallNode, GetStackEditorDataKey());
		}

		InputCollection->SetShouldShowInStack(GetStackEditorData().GetShowOutputs());

		if (OutputCollection == nullptr)
		{
			OutputCollection = NewObject<UNiagaraStackModuleItemOutputCollection>(this);
			OutputCollection->Initialize(CreateDefaultChildRequiredData(), *FunctionCallNode);
		}

		NewChildren.Add(InputCollection);
		NewChildren.Add(OutputCollection);
	}

	RefreshIsEnabled();

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
	
	RefreshIssues(NewIssues);
}

void UNiagaraStackModuleItem::RefreshIssues(TArray<FStackIssue>& NewIssues)
{
	if (FunctionCallNode != nullptr)
	{
		if (!FunctionCallNode->ScriptIsValid())
		{
			UNiagaraStackEntry::FStackIssue InvalidScriptError;
			InvalidScriptError.ShortDescription = LOCTEXT("MissingModule", "The referenced module's script asset is missing.");
			InvalidScriptError.LongDescription = InvalidScriptError.ShortDescription;
			InvalidScriptError.UniqueIdentifier = FName(*FString::Printf(TEXT("MissingModule-%s"), *GetStackEditorDataKey()));
			NewIssues.Add(InvalidScriptError);
		}

		TOptional<bool> IsEnabled = FNiagaraStackGraphUtilities::GetModuleIsEnabled(*FunctionCallNode);
		if (!IsEnabled.IsSet())
		{
			bIsEnabled = false;
			UNiagaraStackEntry::FStackIssue InconsistentEnabledError;
			InconsistentEnabledError.ShortDescription = LOCTEXT("InconsistentEnabledErrorSummary", "The enabled state for module is inconsistent.");
			InconsistentEnabledError.LongDescription = LOCTEXT("InconsistentEnabledError", "This module is using multiple functions and their enabled state is inconsistent.\nClick fix to make all of the functions for this module enabled.");
			InconsistentEnabledError.UniqueIdentifier = FName(*FString::Printf(TEXT("InconsistentEnabled-%s"), *GetStackEditorDataKey()));
			UNiagaraStackEntry::FStackIssueFix Fix;
			Fix.Description = LOCTEXT("EnableModule", "Enable module");
			Fix.FixDelegate.BindLambda([=]()
			{
				FScopedTransaction ScopedTransaction(Fix.Description);
				SetIsEnabled(true);
			});
			InconsistentEnabledError.Fixes.Add(Fix);
			NewIssues.Add(InconsistentEnabledError);
		}
	}
	/*// Generate dependency errors with their fixes
	TArray<UNiagaraNodeFunctionCall*> FoundCalls;
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*FunctionCallNode, StackNodeGroups);
	TArray<FNiagaraModuleDependency> DependenciesNeeded;
	UNiagaraNodeOutput* OutputNode = GetOutputNode();
	for (FNiagaraModuleDependency Dependency: FunctionCallNode->FunctionScript->RequiredDependencies)
	{
		if (Dependency.Id == NAME_None)
		{
			continue;
		}
		bool bDependencyMet = false;
		UNiagaraNodeFunctionCall* FunctionNode = nullptr;
		TArray <UNiagaraNodeFunctionCall*> DisabledDependencies;
		for (int i = 1; i < StackNodeGroups.Num() - 1; i++)
		{
			FNiagaraStackGraphUtilities::FStackNodeGroup& StackNodeGroup = StackNodeGroups[i];
			FunctionNode = Cast<UNiagaraNodeFunctionCall> (StackNodeGroup.EndNode);
			if (FunctionNode != nullptr && FunctionNode->FunctionScript->ProvidedDependencies.Contains(Dependency.Id))
			// treat disabled dependencies separately because they are easier to fix
			{
				if (FunctionNode->IsNodeEnabled() == false)
				{
					DisabledDependencies.Add(FunctionNode);
				}
				else
				{
					bDependencyMet = true;
					break;
				}
			}
		}
		if (bDependencyMet == false)
		{
			DependenciesNeeded.Add(Dependency);
			UNiagaraStackEntry::FStackIssue Error;
			Error.ShortDescription = LOCTEXT("DependencyWarning", "The module has unmet dependencies.");;
			Error.LongDescription = FText::Format(LOCTEXT("DependencyWarningLong", "The following dependency is not met: {0}"), Dependency.Description);
			Error.bCanBeDismissed = true;
			Error.UniqueIdentifier = FName(*FString::Printf(TEXT("%s-dependency-%s"), *GetStackEditorDataKey(), *Dependency.Id.ToString()));

			for ( UNiagaraNodeFunctionCall* DisabledNode : DisabledDependencies) // module exists but disabled
			{
				UNiagaraStackEntry::FStackIssueFix Fix;
				Fix.Description = FText::Format(LOCTEXT("EnableDependency", "Enable dependency module {0}"), FText::FromString(DisabledNode->GetFunctionName()));
				Fix.FixDelegate.BindLambda([=]()
				{
					FScopedTransaction ScopedTransaction(LOCTEXT("EnableDependencyModule", "Enable dependency module"));
					DisabledNode->Modify();
					FNiagaraStackGraphUtilities::SetModuleIsEnabled(*DisabledNode, true);
				});
				Error.Fixes.Add(Fix);
			}

			TArray<FAssetData> ModuleAssets;
			FNiagaraStackGraphUtilities::GetScriptAssetsByDependencyProvided(ENiagaraScriptUsage::Module, Dependency.Id, ModuleAssets);
			for (FAssetData ModuleAsset : ModuleAssets) 
			{
				auto DisabledAsset = DisabledDependencies.FindByPredicate([=](UNiagaraNodeFunctionCall* DisabledNode) {return DisabledNode->GetReferencedAsset() == ModuleAsset.GetAsset(); });
				if (DisabledAsset != nullptr)
				{
					continue;
				}
				UNiagaraStackEntry::FStackIssueFix Fix;
				Fix.Description = LOCTEXT("InsertNewModule", "Insert new module");
				int32 InsertIndex = GetModuleIndex();
				Fix.FixDelegate.BindLambda([=]()
				{
					FScopedTransaction ScopedTransaction(Fix.Description);
					UNiagaraNodeFunctionCall* NewModuleNode = nullptr;
					int32 TargetIndex = Dependency.Type == ENiagaraModuleDependencyType::PostDependency ? GetModuleIndex() + 1 : GetModuleIndex();
					NewModuleNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleAsset, *OutputNode, TargetIndex);
					checkf(NewModuleNode != nullptr, TEXT("Add module action failed"));
					FNiagaraStackGraphUtilities::InitializeStackFunctionInputs(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *NewModuleNode, *NewModuleNode);
					FNiagaraStackGraphUtilities::RelayoutGraph(*OutputNode->GetGraph());
				});
				Fix.Description = FText::Format(LOCTEXT("AddDependency", "Add dependency module {0}"), FText::FromName(ModuleAsset.AssetName));
				Error.Fixes.Add(Fix);
			}
			
			NewIssues.Add(Error);
		}
	}*/
}

bool UNiagaraStackModuleItem::FilterOutputCollection(const UNiagaraStackEntry& Child) const
{
	if (Child.IsA<UNiagaraStackModuleItemOutputCollection>() && GetStackEditorData().GetShowOutputs() == false)
	{
		return false;
	}
	return true;
}

void UNiagaraStackModuleItem::RefreshIsEnabled()
{
	TOptional<bool> IsEnabled = FNiagaraStackGraphUtilities::GetModuleIsEnabled(*FunctionCallNode);
	if (IsEnabled.IsSet())
	{
		bIsEnabled = IsEnabled.GetValue();
	}
}

bool UNiagaraStackModuleItem::CanMoveAndDelete() const
{
	return bCanMoveAndDelete;
}

bool UNiagaraStackModuleItem::CanRefresh() const
{
	return bCanRefresh;
}

void UNiagaraStackModuleItem::Refresh()
{
	if (CanRefresh())
	{
		if (FunctionCallNode->RefreshFromExternalChanges())
		{
			FunctionCallNode->GetNiagaraGraph()->NotifyGraphNeedsRecompile();
			GetSystemViewModel()->ResetSystem();
		}
		RefreshChildren();
	}
}

bool UNiagaraStackModuleItem::GetIsEnabled() const
{
	return bIsEnabled;
}

void UNiagaraStackModuleItem::SetIsEnabled(bool bInIsEnabled)
{
	FNiagaraStackGraphUtilities::SetModuleIsEnabled(*FunctionCallNode, bInIsEnabled);
	bIsEnabled = bInIsEnabled;
}

void UNiagaraStackModuleItem::Delete()
{
	checkf(CanMoveAndDelete(), TEXT("This module can't be deleted"));

	FScopedTransaction ScopedTransaction(LOCTEXT("RemoveAModuleFromTheStack", "Remove a module from the stack"));
	TArray<TWeakObjectPtr<UNiagaraNodeInput>> RemovedNodes;
	bool bRemoved = FNiagaraStackGraphUtilities::RemoveModuleFromStack(*FunctionCallNode, RemovedNodes);
	if (bRemoved)
	{
		UNiagaraGraph* Graph = FunctionCallNode->GetNiagaraGraph();
		Graph->NotifyGraphNeedsRecompile();
		FNiagaraStackGraphUtilities::RelayoutGraph(*FunctionCallNode->GetGraph());
		ModifiedGroupItemsDelegate.ExecuteIfBound();
		for (auto InputNode: RemovedNodes)
		{
			if (InputNode != nullptr && InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
			{
				GetSystemViewModel()->NotifyDataObjectChanged(InputNode->GetDataInterface());
			}
		}
	}
}

int32 UNiagaraStackModuleItem::GetModuleIndex()
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*FunctionCallNode, StackGroups);
	int32 ModuleIndex = 0;
	for (FNiagaraStackGraphUtilities::FStackNodeGroup& StackGroup : StackGroups)
	{
		if (StackGroup.EndNode == FunctionCallNode)
		{
			return ModuleIndex;
		}
		if (StackGroup.EndNode->IsA<UNiagaraNodeFunctionCall>())
		{
			ModuleIndex++;
		}
	}
	return INDEX_NONE;
}

UNiagaraNodeOutput* UNiagaraStackModuleItem::GetOutputNode() const
{
	return FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*FunctionCallNode);
}

void UNiagaraStackModuleItem::NotifyModuleMoved()
{
	ModifiedGroupItemsDelegate.ExecuteIfBound();
}

UObject* UNiagaraStackModuleItem::GetExternalAsset() const
{
	if (GetModuleNode().FunctionScript != nullptr && GetModuleNode().FunctionScript->IsAsset())
	{
		return GetModuleNode().FunctionScript;
	}
	return nullptr;
}

bool UNiagaraStackModuleItem::CanDrag() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE

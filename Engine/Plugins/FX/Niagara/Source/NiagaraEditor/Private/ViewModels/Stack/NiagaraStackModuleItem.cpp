// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "NiagaraEditorModule.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/Stack/NiagaraStackModuleItemLinkedInputCollection.h"
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
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraph.h"
#include "NiagaraScript.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraScriptMergeManager.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraCommon.h"
#include "NiagaraConstants.h"
#include "Widgets/SWidget.h"

#include "ScopedTransaction.h"

// TODO: Remove these
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "NiagaraStackModuleItem"

TArray<ENiagaraScriptUsage> UsagePriority = { // Ordered such as the highest priority has the largest index
	ENiagaraScriptUsage::ParticleUpdateScript,
	ENiagaraScriptUsage::ParticleSpawnScript,
	ENiagaraScriptUsage::EmitterUpdateScript,
	ENiagaraScriptUsage::EmitterSpawnScript,
	ENiagaraScriptUsage::SystemUpdateScript,
	ENiagaraScriptUsage::SystemSpawnScript };

UNiagaraNodeOutput* GetOutputNodeForModuleDependency(ENiagaraScriptUsage DependantUsage, UNiagaraScript* DependencyScript, UNiagaraSystem& System, const FNiagaraEmitterHandle* EmitterHandle, FNiagaraModuleDependency Dependency)
{
	UNiagaraNodeOutput* TargetOutputNode = nullptr;
	if (DependencyScript)
	{
		UNiagaraScript* OutputScript = nullptr;
		TArray<ENiagaraScriptUsage> SupportedUsages = UNiagaraScript::GetSupportedUsageContextsForBitmask(DependencyScript->ModuleUsageBitmask);

		if (Dependency.ScriptConstraint == ENiagaraModuleDependencyScriptConstraint::AllScripts)
		{
			int32 ClosestDistance = MAX_int32;
			int32 DependantIndex = UsagePriority.IndexOfByPredicate(
				[&](const ENiagaraScriptUsage CurrentUsage)
			{
				return UNiagaraScript::IsEquivalentUsage(DependantUsage, CurrentUsage);
			});

			for (ENiagaraScriptUsage PossibleUsage : SupportedUsages)
			{
				int32 PossibleIndex = UsagePriority.IndexOfByPredicate(
					[&](const ENiagaraScriptUsage CurrentUsage)
				{
					return UNiagaraScript::IsEquivalentUsage(PossibleUsage, CurrentUsage);
				});

				if (PossibleIndex == INDEX_NONE)
				{
					// This usage isn't in the execution flow so check the next one.
					continue;
				}

				int32 Distance = PossibleIndex - DependantIndex;
				bool bCorrectOrder = (Dependency.Type == ENiagaraModuleDependencyType::PreDependency && Distance >= 0) || (Dependency.Type == ENiagaraModuleDependencyType::PostDependency && Distance <= 0);
				if ((FMath::Abs(Distance) < ClosestDistance) && bCorrectOrder)
				{
					ClosestDistance = Distance;
					OutputScript = FNiagaraEditorUtilities::GetScriptFromSystem(System, EmitterHandle->GetId(), PossibleUsage, FGuid());
				}
			}
		}
		else if (Dependency.ScriptConstraint == ENiagaraModuleDependencyScriptConstraint::SameScript)
		{
			if (SupportedUsages.Contains(DependantUsage))
			{
				OutputScript = FNiagaraEditorUtilities::GetScriptFromSystem(System, EmitterHandle->GetId(), DependantUsage, FGuid());
			}
		}

		if (OutputScript != nullptr)
		{
			TargetOutputNode = FNiagaraEditorUtilities::GetScriptOutputNode(*OutputScript);
		}
	}
	return TargetOutputNode;
}

UNiagaraStackModuleItem::UNiagaraStackModuleItem()
	: FunctionCallNode(nullptr)
	, bCanRefresh(false)
	, InputCollection(nullptr)
	, bIsModuleScriptReassignmentPending(false)
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
	OutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*FunctionCallNode);

	// We do not need to include child filters for NiagaraNodeAssignments as they do not display their output or linked input collections
	if (!FunctionCallNode->IsA<UNiagaraNodeAssignment>())
	{
		AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackModuleItem::FilterOutputCollection));
		AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackModuleItem::FilterLinkedInputCollection));
	}

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

		// NiagaraNodeAssignments should not display OutputCollection and LinkedInputCollection as they effectively handle this through their InputCollection 
		if (!FunctionCallNode->IsA<UNiagaraNodeAssignment>())
		{

			if (LinkedInputCollection == nullptr)
			{
				LinkedInputCollection = NewObject<UNiagaraStackModuleItemLinkedInputCollection>(this);
				LinkedInputCollection->Initialize(CreateDefaultChildRequiredData(), *FunctionCallNode);
				LinkedInputCollection->AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackModuleItem::FilterLinkedInputCollectionChild));
			}

			if (OutputCollection == nullptr)
			{
				OutputCollection = NewObject<UNiagaraStackModuleItemOutputCollection>(this);
				OutputCollection->Initialize(CreateDefaultChildRequiredData(), *FunctionCallNode);
				OutputCollection->AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackModuleItem::FilterOutputCollectionChild));
			}

			InputCollection->SetShouldShowInStack(GetStackEditorData().GetShowOutputs() || GetStackEditorData().GetShowLinkedInputs());

			NewChildren.Add(LinkedInputCollection);
			NewChildren.Add(InputCollection);
			NewChildren.Add(OutputCollection);
		
		}
		else
		{
			// We do not show the expander arrow for InputCollections of NiagaraNodeAssignments as they only have this one collection
			InputCollection->SetShouldShowInStack(false);

			NewChildren.Add(InputCollection);
		}
	}

	RefreshIsEnabled();
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
	RefreshIssues(NewIssues);
}

void UNiagaraStackModuleItem::RefreshIssues(TArray<FStackIssue>& NewIssues)
{
	if (!GetIsEnabled())
	{
		NewIssues.Empty();
		return;
	}
	if (FunctionCallNode != nullptr)
	{
		if (FunctionCallNode->FunctionScript != nullptr && FunctionCallNode->FunctionScript->bDeprecated)
		{
			FText LongMessage = FunctionCallNode->FunctionScript->DeprecationRecommendation != nullptr ? 
				FText::Format(LOCTEXT("ModuleScriptDeprecationLong", "The script asset for the assigned module {0} has been deprecated. Suggested replacement: {1}"), FText::FromString(FunctionCallNode->GetFunctionName()),FText::FromString(FunctionCallNode->FunctionScript->DeprecationRecommendation->GetPathName())) :
				FText::Format(LOCTEXT("ModuleScriptDeprecationUnknownLong", "The script asset for the assigned module {0} has been deprecated."), FText::FromString(FunctionCallNode->GetFunctionName()));

			int32 AddIdx = NewIssues.Add(FStackIssue(
				EStackIssueSeverity::Warning,
				LOCTEXT("ModuleScriptDeprecationShort", "Deprecated module"),
				LongMessage,
				GetStackEditorDataKey(),
				false,
				{
					FStackIssueFix(
						LOCTEXT("SelectNewModuleScriptFix", "Select a new module script"),
						FStackIssueFixDelegate::CreateLambda([this]() { this->bIsModuleScriptReassignmentPending = true; })),
					FStackIssueFix(
						LOCTEXT("DeleteFix", "Delete this module"),
						FStackIssueFixDelegate::CreateLambda([this]() { this->Delete(); }))
				}));
			
			if (FunctionCallNode->FunctionScript->DeprecationRecommendation != nullptr)
			{
				NewIssues[AddIdx].InsertFix(0,
					FStackIssueFix(
						LOCTEXT("SelectNewModuleScriptFixUseRecommended", "Use recommended replacement"),
						FStackIssueFixDelegate::CreateLambda([this]() { ReassignModuleScript(FunctionCallNode->FunctionScript->DeprecationRecommendation); })));
			}
		}
		if(FunctionCallNode->FunctionScript == nullptr && FunctionCallNode->GetClass() == UNiagaraNodeFunctionCall::StaticClass())
		{
			NewIssues.Add(FStackIssue(
				EStackIssueSeverity::Error,
				LOCTEXT("ModuleScriptMissingShort", "Missing module script"),
				FText::Format(LOCTEXT("ModuleScriptMissingLong", "The script asset for the assigned module {0} is missing."), FText::FromString(FunctionCallNode->GetFunctionName())),
				GetStackEditorDataKey(),
				false,
				{
					FStackIssueFix(
						LOCTEXT("SelectNewModuleScriptFix", "Select a new module script"),
						FStackIssueFixDelegate::CreateLambda([this]() { this->bIsModuleScriptReassignmentPending = true; })),
				FStackIssueFix(
					LOCTEXT("DeleteFix", "Delete this module"),
					FStackIssueFixDelegate::CreateLambda([this]() { this->Delete(); }))
				}));
		}
		else if (!FunctionCallNode->ScriptIsValid())
		{
			FStackIssue InvalidScriptError(
				EStackIssueSeverity::Error,
				LOCTEXT("InvalidModuleScriptErrorSummary", "Invalid module script."),
				LOCTEXT("InvalidModuleScriptError", "The script this module is supposed to execute is missing or invalid for other reasons."),
				GetStackEditorDataKey(),
				false);

			NewIssues.Add(InvalidScriptError);
		}

		TOptional<bool> IsEnabled = FNiagaraStackGraphUtilities::GetModuleIsEnabled(*FunctionCallNode);
		if (!IsEnabled.IsSet())
		{
			bIsEnabled = false;
			FText FixDescription = LOCTEXT("EnableModule", "Enable module");
			FStackIssueFix EnableFix(
				FixDescription,
				FStackIssueFixDelegate::CreateLambda([this, FixDescription]()
			{
				SetIsEnabled(true);;
			}));
			FStackIssue InconsistentEnabledError(
				EStackIssueSeverity::Error,
				LOCTEXT("InconsistentEnabledErrorSummary", "The enabled state for module is inconsistent."),
				LOCTEXT("InconsistentEnabledError", "This module is using multiple functions and their enabled state is inconsistent.\nClick fix to make all of the functions for this module enabled."),
				GetStackEditorDataKey(),
				false,
				EnableFix);

			NewIssues.Add(InconsistentEnabledError);
		}

		UNiagaraNodeAssignment* AssignmentFunctionCall = Cast<UNiagaraNodeAssignment>(FunctionCallNode);
		if (AssignmentFunctionCall != nullptr)
		{
			TSet<FNiagaraVariable> FoundAssignmentTargets;
			for (const FNiagaraVariable& AssignmentTarget : AssignmentFunctionCall->GetAssignmentTargets())
			{
				if (FoundAssignmentTargets.Contains(AssignmentTarget))
				{
					FText FixDescription = LOCTEXT("RemoveDuplicate", "Remove Duplicate");
					FStackIssueFix RemoveDuplicateFix(FixDescription, FStackIssueFixDelegate::CreateLambda([AssignmentFunctionCall, AssignmentTarget]()
					{
						AssignmentFunctionCall->RemoveParameter(AssignmentTarget);
					}));
					FStackIssue DuplicateAssignmentTargetError(
						EStackIssueSeverity::Error,
						LOCTEXT("DuplicateAssignmentTargetErrorSummary", "Duplicate variables detected."),
						LOCTEXT("DuplicateAssignmentTargetError", "This 'Set Variables' module is attempting to set the same variable more than once, which is unsupported."),
						GetStackEditorDataKey(),
						false,
						RemoveDuplicateFix);

					NewIssues.Add(DuplicateAssignmentTargetError);
				}
				FoundAssignmentTargets.Add(AssignmentTarget);
			}
		}
	}
	// Generate dependency errors with their fixes
	TArray<UNiagaraNodeFunctionCall*> FoundCalls;
	TArray<FNiagaraModuleDependency> DependenciesNeeded;

	TArray<FNiagaraStackModuleData> SystemModuleData = GetSystemViewModel()->GetStackModuleDataForEmitter(GetEmitterViewModel());
	int32 ModuleIndex = INDEX_NONE;
	for (int i = 0; i < SystemModuleData.Num(); i++)
	{
		auto ModuleData = SystemModuleData[i];
		if (ModuleData.ModuleNode == FunctionCallNode)
		{
			ModuleIndex = i;
			break;
		}
	}

	if (ModuleIndex != INDEX_NONE && FunctionCallNode && FunctionCallNode->FunctionScript)
	{
		for (FNiagaraModuleDependency Dependency : FunctionCallNode->FunctionScript->RequiredDependencies)
		{
			if (Dependency.Id == NAME_None)
			{
				continue;
			}
			bool bDependencyMet = false;
			UNiagaraNodeFunctionCall* FunctionNode = nullptr;
			TArray <UNiagaraNodeFunctionCall*> DisabledDependencies;
			TArray <FNiagaraStackModuleData> DisorderedDependencies;

			int32 DependencyModuleIndex = INDEX_NONE;
			for (FNiagaraStackModuleData ModuleData : SystemModuleData)
			{
				FunctionNode = ModuleData.ModuleNode;
				DependencyModuleIndex++;
				if (FunctionNode != nullptr && FunctionNode->FunctionScript != nullptr && FunctionNode->FunctionScript->ProvidedDependencies.Contains(Dependency.Id))
				{
					auto DependencyOutputUsage = ModuleData.Usage;
					int32 PossibleIndex = UsagePriority.IndexOfByPredicate(
						[&](const ENiagaraScriptUsage CurrentUsage)
					{
						return UNiagaraScript::IsEquivalentUsage(DependencyOutputUsage, CurrentUsage);
					});
					int32 DependantIndex = UsagePriority.IndexOfByPredicate(
						[&](const ENiagaraScriptUsage CurrentUsage)
					{
						return UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), CurrentUsage);
					});
					int32 Distance = PossibleIndex - DependantIndex;

					bool bIncorrectOrder = Distance == 0 ? ((Dependency.Type == ENiagaraModuleDependencyType::PreDependency && ModuleIndex < DependencyModuleIndex)
						|| (Dependency.Type == ENiagaraModuleDependencyType::PostDependency && ModuleIndex > DependencyModuleIndex))
						: ((Dependency.Type == ENiagaraModuleDependencyType::PreDependency && Distance < 0)
							|| (Dependency.Type == ENiagaraModuleDependencyType::PostDependency && Distance > 0));

					bool bSameScriptDependencyConstraint = Dependency.ScriptConstraint == ENiagaraModuleDependencyScriptConstraint::SameScript;
					bool bEquivalentScriptUsage = UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ModuleData.Usage);

					// If the dependency is for modules in the same script, the two modules are only incorrectly ordered if they share equivalent script usages
					if (bSameScriptDependencyConstraint)
					{
						bIncorrectOrder = bEquivalentScriptUsage && bIncorrectOrder;
					}

					if (bIncorrectOrder)
					{
						DisorderedDependencies.Add(ModuleData);
					}
					else if (FunctionNode->IsNodeEnabled() == false)
					{
						DisabledDependencies.Add(FunctionNode);
					}
					else if (Dependency.ScriptConstraint == ENiagaraModuleDependencyScriptConstraint::AllScripts || 
						(bSameScriptDependencyConstraint && bEquivalentScriptUsage && OutputNode->GetUsageId() == ModuleData.UsageId))
					{
						bDependencyMet = true;
						break;
					}
				}
			}
			if (bDependencyMet == false)
			{
				TArray<FStackIssueFix> Fixes;
				DependenciesNeeded.Add(Dependency);

				FText DependencyTypeString = Dependency.Type == ENiagaraModuleDependencyType::PreDependency ? LOCTEXT("PreDependency", "pre-dependency") : LOCTEXT("PostDependency", "post-dependency");

				for (UNiagaraNodeFunctionCall* DisabledNode : DisabledDependencies) // module exists but disabled
				{
					UNiagaraStackEntry::FStackIssueFix Fix(
						FText::Format(LOCTEXT("EnableDependency", "Enable dependency module {0}"), FText::FromString(DisabledNode->GetFunctionName())),
						FStackIssueFixDelegate::CreateLambda([this, DisabledNode]()
					{
						FScopedTransaction ScopedTransaction(LOCTEXT("EnableDependencyModule", "Enable dependency module"));
						FNiagaraStackGraphUtilities::SetModuleIsEnabled(*DisabledNode, true);
						OnRequestFullRefresh().Broadcast();

					}));
					Fixes.Add(Fix);
				}

				for (FNiagaraStackModuleData DisorderedNode : DisorderedDependencies) // module exists but is not in the correct order (and possibly also disabled)
				{
					bool bNeedsEnable = !DisorderedNode.ModuleNode->IsNodeEnabled();
					FText AndEnableModule = bNeedsEnable ? FText::Format(LOCTEXT("AndEnableDependency", "And enable dependency module {0}"), FText::FromString(DisorderedNode.ModuleNode->GetFunctionName())) : FText();
					UNiagaraStackEntry::FStackIssueFix Fix(
						FText::Format(LOCTEXT("ReorderDependency", "Reposition this module in the correct order related to {0} {1}"), FText::FromString(DisorderedNode.ModuleNode->GetFunctionName()), AndEnableModule),
						FStackIssueFixDelegate::CreateLambda([this, bNeedsEnable, DisorderedNode, SystemModuleData, Dependency, ModuleIndex]()
					{
						FScopedTransaction ScopedTransaction(LOCTEXT("ReorderDependencyModule", "Reorder dependency module"));

						FunctionCallNode->Modify();
						// reorder node
						int32 CorrectIndex = Dependency.Type == ENiagaraModuleDependencyType::PostDependency ? DisorderedNode.Index : DisorderedNode.Index + 1;
						checkf(ModuleIndex != INDEX_NONE, TEXT("Module data wasn't found in system for current module!"));
						UNiagaraScript& OwningScript = *FNiagaraEditorUtilities::GetScriptFromSystem(GetSystemViewModel()->GetSystem(), SystemModuleData[ModuleIndex].EmitterHandleId, SystemModuleData[ModuleIndex].Usage, SystemModuleData[ModuleIndex].UsageId);
						FNiagaraStackGraphUtilities::MoveModule(OwningScript, *FunctionCallNode, GetSystemViewModel()->GetSystem(), DisorderedNode.EmitterHandleId, DisorderedNode.Usage, DisorderedNode.UsageId, CorrectIndex);
						// enable if needed
						if (bNeedsEnable)
						{
							FNiagaraStackGraphUtilities::SetModuleIsEnabled(*DisorderedNode.ModuleNode, true);
						}
						FNiagaraStackGraphUtilities::RelayoutGraph(*OutputNode->GetGraph());
						OnRequestFullRefresh().Broadcast();
					}));
					Fixes.Add(Fix);
				}
				if (DisorderedDependencies.Num() == 0 && DisabledDependencies.Num() == 0)
				{
					TArray<FAssetData> ModuleAssets;
					FNiagaraStackGraphUtilities::GetScriptAssetsByDependencyProvided(ENiagaraScriptUsage::Module, Dependency.Id, ModuleAssets);
					for (FAssetData ModuleAsset : ModuleAssets)
					{
						UNiagaraScript* DependencyScript = Cast<UNiagaraScript>(ModuleAsset.GetAsset());
						if (Dependency.ScriptConstraint == ENiagaraModuleDependencyScriptConstraint::SameScript)
						{
							TArray<ENiagaraScriptUsage> SupportedUsages = UNiagaraScript::GetSupportedUsageContextsForBitmask(DependencyScript->ModuleUsageBitmask);
							if (SupportedUsages.Contains(OutputNode->GetUsage()) == false)
							{
								// If the dependency requires the provider be in the same script and the usage of this module doesn't support that usage, skip it.
								continue;
							}
						}
						
						FText FixDescription = FText::Format(LOCTEXT("AddDependency", "Add new dependency module {0}"), FText::FromName(ModuleAsset.AssetName));
						UNiagaraStackEntry::FStackIssueFix Fix(
							FixDescription,
							FStackIssueFixDelegate::CreateLambda([=]()
						{
							FScopedTransaction ScopedTransaction(FixDescription);
							UNiagaraNodeFunctionCall* NewModuleNode = nullptr;
							int32 TargetIndex = 0;
							checkf(DependencyScript != nullptr, TEXT("Add module action failed"));
							// Determine the output node for the group where the added dependency module belongs
							UNiagaraNodeOutput* TargetOutputNode = nullptr;
							for (int i = ModuleIndex; i < SystemModuleData.Num() && i >= 0; i = Dependency.Type == ENiagaraModuleDependencyType::PostDependency ? i + 1 : i - 1) // moving up or down depending on type
							// starting at current module, which is a dependent
							{
								bool bRequiredDependencyFound = SystemModuleData[i].ModuleNode->FunctionScript->RequiredDependencies.ContainsByPredicate(
									[&Dependency](const FNiagaraModuleDependency& RequiredDependency)
								{
									return RequiredDependency.Id == Dependency.Id;
								});
								if (bRequiredDependencyFound) // check for multiple dependents along the way, and stop adjacent to the last one
								{
									ENiagaraScriptUsage DependencyUsage = SystemModuleData[i].Usage;
									const FNiagaraEmitterHandle* EmitterHandle = FNiagaraEditorUtilities::GetEmitterHandleForEmitter(GetSystemViewModel()->GetSystem(), *GetEmitterViewModel()->GetEmitter());
									UNiagaraNodeOutput* FoundTargetOutputNode = GetOutputNodeForModuleDependency(DependencyUsage, DependencyScript, GetSystemViewModel()->GetSystem(), EmitterHandle, Dependency);
									if (FoundTargetOutputNode != nullptr)
									{
										TargetOutputNode = FoundTargetOutputNode;
										UNiagaraNodeOutput* CurrentOutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*SystemModuleData[i].ModuleNode);
										if (TargetOutputNode == CurrentOutputNode)
										{
											TargetIndex = Dependency.Type == ENiagaraModuleDependencyType::PostDependency ? SystemModuleData[i].Index + 1 : SystemModuleData[i].Index;
										}
										else
										{
											TargetIndex = Dependency.Type == ENiagaraModuleDependencyType::PostDependency ? 0 : INDEX_NONE;
										}
									}
								}
							}

							if (TargetOutputNode == nullptr)
							{
								// If no output node was found than the dependency can't be resolved and it most likely misconfigured in data.
								// TODO: Don't show this toast here, change the fix delegate to return a fix result with whether or not the fix succeeded and any error message for the user.
								FNotificationInfo Error(LOCTEXT("FixFailedToast", "Failed to fix the dependency since\nwe could not find a compatible place to insert the module.\nPlease check the configuration of the dependency.\nSee the log for more details."));
								Error.ExpireDuration = 5.0f;
								Error.bFireAndForget = true;
								Error.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
								FSlateNotificationManager::Get().AddNotification(Error);
								FString ModuleAssetFullName;
								ModuleAsset.GetFullName(ModuleAssetFullName);
								UE_LOG(LogNiagaraEditor, Error, TEXT("Dependency fix failed, could not find a compatible place to insert the module.\nModule requiring dependency: %s\nModule providing dependency: %s\nDependency name: %s\nDependency type: %s"),
									*FunctionCallNode->FunctionScript->GetFullName(), *ModuleAssetFullName, *Dependency.Id.ToString(), Dependency.Type == ENiagaraModuleDependencyType::PreDependency ? TEXT("Pre-dependency") : TEXT("Post-dependency"));
								return;
							}

							TArray<FNiagaraStackModuleData> ScriptModuleData = SystemModuleData.FilterByPredicate([&](FNiagaraStackModuleData CurrentData) {return CurrentData.Usage == DependencyScript->GetUsage(); });
							int32 PreIndex = INDEX_NONE; // index of last pre dependency
							int32 PostIndex = INDEX_NONE; // index of fist post dependency, the module will have to be placed between these indexes
							// for now, we skip the case where the dependencies are fulfilled in other script groups as well as here, because that's extremely unlikely
							if (TargetIndex == INDEX_NONE)
							{
								TargetIndex = 0; //start at the beginning to look for potential dependencies of this dependency
							}
							for (int32 i = TargetIndex; i < ScriptModuleData.Num() && i >= 0; i = Dependency.Type == ENiagaraModuleDependencyType::PostDependency ? i + 1 : i - 1)
							{
								UNiagaraNodeFunctionCall * CurrentNode = ScriptModuleData[i].ModuleNode;
								for (FNiagaraModuleDependency Requirement : DependencyScript->RequiredDependencies)
								{
									if (Requirement.Id == NAME_None)
									{
										continue;
									}

									if (CurrentNode->FunctionScript->ProvidedDependencies.Contains(Requirement.Id))
									{
										if (Requirement.Type == ENiagaraModuleDependencyType::PreDependency)
										{
											PostIndex = i;
										}
										else if (PreIndex == INDEX_NONE) // only record the first post-dependency
										{
											PreIndex = i;
										}
									}
								}
							}
							if (PostIndex != INDEX_NONE)
							{
								TargetIndex = 0; // if it has post dependencies place it at the top
								if (PreIndex != INDEX_NONE)
								{
									TargetIndex = PostIndex; // if it also has post dependencies just add it before its first post dependency
								}
							}
							NewModuleNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleAsset, *TargetOutputNode, TargetIndex);
							checkf(NewModuleNode != nullptr, TEXT("Add module action failed"));
							FNiagaraStackGraphUtilities::InitializeStackFunctionInputs(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *NewModuleNode, *NewModuleNode);
							FNiagaraStackGraphUtilities::RelayoutGraph(*TargetOutputNode->GetGraph());
							OnRequestFullRefresh().Broadcast();
						}));
						Fixes.Add(Fix);
					}
				}
				UNiagaraStackEntry::FStackIssue Error(
					EStackIssueSeverity::Error,
					LOCTEXT("DependencyWarning", "The module has unmet dependencies."),
					FText::Format(LOCTEXT("DependencyWarningLong", "The following {0} is not met: {1}; {2}"), DependencyTypeString, FText::FromName(Dependency.Id), Dependency.Description),
					FString::Printf(TEXT("%s-dependency-%s"), *GetStackEditorDataKey(), *Dependency.Id.ToString()),
					true,
					Fixes);
				NewIssues.Add(Error);
			}
		}
	}
}

bool UNiagaraStackModuleItem::FilterOutputCollection(const UNiagaraStackEntry& Child) const
{
	if (Child.IsA<UNiagaraStackModuleItemOutputCollection>())
	{
		TArray<UNiagaraStackEntry*> FilteredChildren;
		Child.GetFilteredChildren(FilteredChildren);
		if (FilteredChildren.Num() != 0)
		{
			return true;
		}
		else if (GetStackEditorData().GetShowOutputs() == false)
		{
			return false;
		}
	}
	return true;
}

bool UNiagaraStackModuleItem::FilterOutputCollectionChild(const UNiagaraStackEntry& Child) const
{
	// Filter to only show search result matches inside collapsed collection
	if (GetStackEditorData().GetShowOutputs() == false)
	{
		return Child.GetIsSearchResult();
	}
	return true;
}

bool UNiagaraStackModuleItem::FilterLinkedInputCollection(const UNiagaraStackEntry& Child) const
{
	if (Child.IsA<UNiagaraStackModuleItemLinkedInputCollection>())
	{
		TArray<UNiagaraStackEntry*> FilteredChildren;
		Child.GetFilteredChildren(FilteredChildren);
		if (FilteredChildren.Num() != 0)
		{
 			return true;
		}
		else if (GetStackEditorData().GetShowLinkedInputs() == false && Child.GetShouldShowInStack())
		{
			return false;
		}
	}
	return true;
}

bool UNiagaraStackModuleItem::FilterLinkedInputCollectionChild(const UNiagaraStackEntry& Child) const
{
	// Filter to only show search result matches inside collapsed collection
	if (GetStackEditorData().GetShowLinkedInputs() == false)
	{
		return Child.GetIsSearchResult();
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
	FScopedTransaction ScopedTransaction(LOCTEXT("EnableDisableModule", "Enable/Disable Module"));
	FNiagaraStackGraphUtilities::SetModuleIsEnabled(*FunctionCallNode, bInIsEnabled);
	bIsEnabled = bInIsEnabled;
	OnRequestFullRefresh().Broadcast();
}

void UNiagaraStackModuleItem::Delete()
{
	checkf(CanMoveAndDelete(), TEXT("This module can't be deleted"));

	FScopedTransaction ScopedTransaction(LOCTEXT("RemoveAModuleFromTheStack", "Remove a module from the stack"));

	const FNiagaraEmitterHandle* EmitterHandle = FNiagaraEditorUtilities::GetEmitterHandleForEmitter(GetSystemViewModel()->GetSystem(), *GetEmitterViewModel()->GetEmitter());
	checkf(EmitterHandle != nullptr, TEXT("Invalid Stack - Emitter handle could not be found for module"));

	TArray<TWeakObjectPtr<UNiagaraNodeInput>> RemovedNodes;
	bool bRemoved = FNiagaraStackGraphUtilities::RemoveModuleFromStack(GetSystemViewModel()->GetSystem(), EmitterHandle->GetId(), *FunctionCallNode, RemovedNodes);
	if (bRemoved)
	{
		UNiagaraGraph* Graph = FunctionCallNode->GetNiagaraGraph();
		Graph->NotifyGraphNeedsRecompile();
		FNiagaraStackGraphUtilities::RelayoutGraph(*FunctionCallNode->GetGraph());
		for (auto InputNode: RemovedNodes)
		{
			if (InputNode != nullptr && InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
			{
				GetSystemViewModel()->NotifyDataObjectChanged(InputNode->GetDataInterface());
			}
		}
		ModifiedGroupItemsDelegate.ExecuteIfBound();
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
	return OutputNode;
}

void UNiagaraStackModuleItem::NotifyModuleMoved()
{
	ModifiedGroupItemsDelegate.ExecuteIfBound();
}

bool UNiagaraStackModuleItem::CanAddInput(FNiagaraVariable InputParameter) const
{
	UNiagaraNodeAssignment* AssignmentModule = Cast<UNiagaraNodeAssignment>(FunctionCallNode);
	return AssignmentModule != nullptr &&
		AssignmentModule->GetAssignmentTargets().Contains(InputParameter) == false &&
		FNiagaraStackGraphUtilities::ParameterIsCompatibleWithScriptUsage(InputParameter, OutputNode->GetUsage());
}

void UNiagaraStackModuleItem::AddInput(FNiagaraVariable InputParameter)
{
	if(ensureMsgf(CanAddInput(InputParameter), TEXT("This module doesn't support adding this input.")))
	{
		UNiagaraNodeAssignment* AssignmentNode = CastChecked<UNiagaraNodeAssignment>(FunctionCallNode);
		AssignmentNode->AddParameter(InputParameter, FNiagaraConstants::GetAttributeDefaultValue(InputParameter));
		FNiagaraStackGraphUtilities::InitializeStackFunctionInput(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *FunctionCallNode, *FunctionCallNode, InputParameter.GetName());
	}
}

bool UNiagaraStackModuleItem::GetIsModuleScriptReassignmentPending() const
{
	return bIsModuleScriptReassignmentPending;
}

void UNiagaraStackModuleItem::SetIsModuleScriptReassignmentPending(bool bIsPending)
{
	bIsModuleScriptReassignmentPending = bIsPending;
}

void UNiagaraStackModuleItem::ReassignModuleScript(UNiagaraScript* ModuleScript)
{
	if (ensureMsgf(FunctionCallNode != nullptr && FunctionCallNode->GetClass() == UNiagaraNodeFunctionCall::StaticClass(),
		TEXT("Can not reassign the module script when the module isn't a valid function call module.")))
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("ReassignModuleTransaction", "Reassign module script"));
		FunctionCallNode->Modify();
		FunctionCallNode->FunctionScript = ModuleScript;
		FunctionCallNode->MarkNodeRequiresSynchronization(TEXT("Module script reassigned."), true);
		RefreshChildren();
	}
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

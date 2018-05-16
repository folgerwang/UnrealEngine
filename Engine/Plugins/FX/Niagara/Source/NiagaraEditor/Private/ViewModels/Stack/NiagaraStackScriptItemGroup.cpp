// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraConstants.h"
#include "NiagaraStackEditorData.h"

#include "Internationalization/Internationalization.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackScriptItemGroup"

class FScriptGroupAddAction : public INiagaraStackItemGroupAddAction
{
public:
	static TSharedRef<FScriptGroupAddAction> CreateAssetModuleAction(FAssetData AssetData)
	{
		FText Category;
		AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraScript, Category), Category);
		if (Category.IsEmptyOrWhitespace())
		{
			Category = LOCTEXT("ModuleNotCategorized", "Uncategorized Modules");
		}

		FString DisplayNameString = FName::NameToDisplayString(AssetData.AssetName.ToString(), false);
		FText DisplayName = FText::FromString(DisplayNameString);

		FText AssetDescription;
		AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraScript, Description), AssetDescription);
		FText Description = FText::Format(LOCTEXT("AssetModuleDescriptionFormat", "Path: {0}\nDescription: {1}"), FText::FromString(AssetData.ObjectPath.ToString()), AssetDescription);

		return MakeShareable(new FScriptGroupAddAction(Category, DisplayName, Description, FNiagaraVariable(), false, AssetData, false));
	}

	static TSharedRef<FScriptGroupAddAction> CreateExistingParameterModuleAction(FNiagaraVariable ParameterVariable)
	{
		FText Category = LOCTEXT("ExistingParameterModuleCategory", "Set Specific Parameters");

		FString DisplayNameString = FName::NameToDisplayString(ParameterVariable.GetName().ToString(), false);
		FText DisplayName = FText::FromString(DisplayNameString);

		FText AttributeDescription = FNiagaraConstants::GetAttributeDescription(ParameterVariable);
		FText Description = FText::Format(LOCTEXT("ExistingParameterModuleDescriptoinFormat", "Description: Set the parameter {0}. {1}"), FText::FromName(ParameterVariable.GetName()), AttributeDescription);

		return MakeShareable(new FScriptGroupAddAction(Category, DisplayName, Description, ParameterVariable, false, FAssetData(), false));
	}

	static TSharedRef<FScriptGroupAddAction> CreateNewParameterModuleAction(FName NewParameterNamespace, FNiagaraTypeDefinition NewParameterType)
	{
		FText Category = LOCTEXT("NewParameterModuleCategory", "Create New Parameter");
		FText DisplayName = NewParameterType.GetNameText();
		FText Description = FText::Format(LOCTEXT("NewParameterModuleDescriptionFormat", "Description: Create a new {0} parameter."), DisplayName);

		FNiagaraParameterHandle NewParameterHandle(NewParameterNamespace, *(TEXT("New") + NewParameterType.GetName()));
		FNiagaraVariable NewParameter(NewParameterType, NewParameterHandle.GetParameterHandleString());

		return MakeShareable(new FScriptGroupAddAction(Category, DisplayName, Description, NewParameter, true, FAssetData(), false));
	}

	virtual FText GetCategory() const override
	{
		return Category;
	}

	virtual FText GetDisplayName() const override
	{
		return DisplayName;
	}

	virtual FText GetDescription() const override
	{
		return Description;
	}

	const FNiagaraVariable& GetModuleParameterVariable() const
	{
		return ModuleParameterVariable;
	}

	bool GetRenameParameterOnAdd() const
	{
		return bRenameParameterOnAdd;
	}

	const FAssetData& GetModuleAssetData() const
	{
		return ModuleAssetData;
	}

	bool GetIsMaterialParameterModuleAction() const
	{
		return bIsMaterialParameterModuleAction;
	}

private:
	FScriptGroupAddAction(FText InCategory, FText InDisplayName, FText InDescription, FNiagaraVariable InModuleParameterVariable, bool bInRenameParameterOnAdd, FAssetData InModuleAssetData, bool bInIsMaterialParameterModuleAction)
		: Category(InCategory)
		, DisplayName(InDisplayName)
		, Description(InDescription)
		, ModuleParameterVariable(InModuleParameterVariable)
		, bRenameParameterOnAdd(bInRenameParameterOnAdd)
		, ModuleAssetData(InModuleAssetData)
		, bIsMaterialParameterModuleAction(bInIsMaterialParameterModuleAction)
	{
	}

private:
	FText Category;
	FText DisplayName;
	FText Description;
	FNiagaraVariable ModuleParameterVariable;
	bool bRenameParameterOnAdd;
	FAssetData ModuleAssetData;
	bool bIsMaterialParameterModuleAction;
};

class FScriptItemGroupAddUtilities : public FNiagaraStackItemGroupAddUtilities
{
public:
	FScriptItemGroupAddUtilities(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, FOnItemAdded InOnItemAdded)
		: FNiagaraStackItemGroupAddUtilities(LOCTEXT("ScriptGroupAddItemName", "Module"), EAddMode::AddFromAction, false, InOnItemAdded)
		, SystemViewModel(InSystemViewModel)
		, EmitterViewModel(InEmitterViewModel)
		, StackEditorData(InStackEditorData)
	{
	}

	void SetOutputNode(UNiagaraNodeOutput* InOutputNode)
	{
		OutputNode = InOutputNode;
	}

	virtual void AddItemDirectly() override { unimplemented(); }

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions) const override
	{
		if (SystemViewModel.IsValid() == false || EmitterViewModel.IsValid() == false || OutputNode == nullptr)
		{
			return;
		}

		// Generate actions for adding script asset modules.
		TArray<FAssetData> ModuleAssets;
		FNiagaraStackGraphUtilities::GetScriptAssetsByUsage(ENiagaraScriptUsage::Module, OutputNode->GetUsage(), ModuleAssets);
		for(const FAssetData& ModuleAsset : ModuleAssets)
		{
			OutAddActions.Add(FScriptGroupAddAction::CreateAssetModuleAction(ModuleAsset));
		}

		// Generate actions for the available parameters to set.
		TArray<FNiagaraVariable> AvailableParameters;
		FNiagaraStackGraphUtilities::GetAvailableParametersForScript(*OutputNode, AvailableParameters);
		for (const FNiagaraVariable& AvailableParameter : AvailableParameters)
		{
			OutAddActions.Add(FScriptGroupAddAction::CreateExistingParameterModuleAction(AvailableParameter));
		}

		// Generate actions for setting new typed parameters.
		TOptional<FName> NewParameterNamespace = FNiagaraStackGraphUtilities::GetNamespaceForScriptUsage(OutputNode->GetUsage());
		if (NewParameterNamespace.IsSet())
		{
			TArray<FNiagaraTypeDefinition> AvailableTypes;
			FNiagaraStackGraphUtilities::GetNewParameterAvailableTypes(AvailableTypes);
			for (const FNiagaraTypeDefinition& AvailableType : AvailableTypes)
			{
				OutAddActions.Add(FScriptGroupAddAction::CreateNewParameterModuleAction(NewParameterNamespace.GetValue(), AvailableType));
			}
		}
	}

	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override
	{
		TSharedRef<FScriptGroupAddAction> ScriptGroupAddAction = StaticCastSharedRef<FScriptGroupAddAction>(AddAction);
		FScopedTransaction ScopedTransaction(LOCTEXT("InsertNewModule", "Insert new module"));
		UNiagaraNodeFunctionCall* NewModuleNode = nullptr;
		if (ScriptGroupAddAction->GetModuleAssetData().IsValid())
		{
			NewModuleNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ScriptGroupAddAction->GetModuleAssetData(), *OutputNode, TargetIndex);
		}
		else if (ScriptGroupAddAction->GetModuleParameterVariable().IsValid())
		{
			NewModuleNode = AddParameterModule(ScriptGroupAddAction->GetModuleParameterVariable(), ScriptGroupAddAction->GetRenameParameterOnAdd(), TargetIndex);
		}

		checkf(NewModuleNode != nullptr, TEXT("Add module action failed"));
		FNiagaraStackGraphUtilities::InitializeStackFunctionInputs(SystemViewModel.Pin().ToSharedRef(), EmitterViewModel.Pin().ToSharedRef(), StackEditorData, *NewModuleNode, *NewModuleNode);
		FNiagaraStackGraphUtilities::RelayoutGraph(*OutputNode->GetGraph());
		OnItemAdded.ExecuteIfBound();
	}
private:
	UNiagaraNodeFunctionCall* AddScriptAssetModule(const FAssetData& AssetData, int32 TargetIndex)
	{
		return FNiagaraStackGraphUtilities::AddScriptModuleToStack(AssetData, *OutputNode, TargetIndex);
	}

	UNiagaraNodeFunctionCall* AddParameterModule(const FNiagaraVariable& ParameterVariable, bool bRenameParameterOnAdd, int32 TargetIndex)
	{
		TArray<FNiagaraVariable> Vars;
		Vars.Add(ParameterVariable);
		TArray<FString> DefaultVals;
		DefaultVals.Add(FNiagaraConstants::GetAttributeDefaultValue(ParameterVariable));
		UNiagaraNodeAssignment* NewAssignmentModule = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Vars, *OutputNode, TargetIndex,DefaultVals );
		
		TArray<const UEdGraphPin*> InputPins;
		FNiagaraStackGraphUtilities::GetStackFunctionInputPins(*NewAssignmentModule, InputPins);
		if (InputPins.Num() == 1)
		{
			FString FunctionInputEditorDataKey = FNiagaraStackGraphUtilities::GenerateStackFunctionInputEditorDataKey(*NewAssignmentModule, InputPins[0]->PinName);
			if (bRenameParameterOnAdd)
			{
				StackEditorData.SetModuleInputIsRenamePending(FunctionInputEditorDataKey, true);
			}
		}

		return NewAssignmentModule;
	}

private:
	UNiagaraNodeOutput* OutputNode;
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModel;
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModel;
	UNiagaraStackEditorData& StackEditorData;
};

void UNiagaraStackScriptItemGroup::Initialize(
	FRequiredEntryData InRequiredEntryData,
	FText InDisplayName,
	FText InToolTip,
	TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
	ENiagaraScriptUsage InScriptUsage,
	FGuid InScriptUsageId)
{
	checkf(ScriptViewModel.IsValid() == false, TEXT("Can not set the script view model more than once."));
	AddUtilities = MakeShared<FScriptItemGroupAddUtilities>(InRequiredEntryData.SystemViewModel, InRequiredEntryData.EmitterViewModel,
		*InRequiredEntryData.StackEditorData, FNiagaraStackItemGroupAddUtilities::FOnItemAdded::CreateUObject(this, &UNiagaraStackScriptItemGroup::ItemAdded));
	Super::Initialize(InRequiredEntryData, InDisplayName, InToolTip, AddUtilities.Get());
	ScriptViewModel = InScriptViewModel;
	ScriptUsage = InScriptUsage;
	ScriptUsageId = InScriptUsageId;
}

void UNiagaraStackScriptItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned = ScriptViewModel.Pin();
	checkf(ScriptViewModelPinned.IsValid(), TEXT("Can not refresh children when the script view model has been deleted."));

	StackSpacerToModuleItemMap.Empty();

	UNiagaraGraph* Graph = ScriptViewModelPinned->GetGraphViewModel()->GetGraph();
	FText ErrorMessage;
	bIsValidForOutput = false;
	if (FNiagaraStackGraphUtilities::ValidateGraphForOutput(*Graph, ScriptUsage, ScriptUsageId, ErrorMessage) == true)
	{
		bIsValidForOutput = true;

		UNiagaraNodeOutput* MatchingOutputNode = Graph->FindOutputNode(ScriptUsage, ScriptUsageId);
		AddUtilities->SetOutputNode(MatchingOutputNode);

		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		FNiagaraStackGraphUtilities::GetOrderedModuleNodes(*MatchingOutputNode, ModuleNodes);
		int32 ModuleIndex = 0;
		for (UNiagaraNodeFunctionCall* ModuleNode : ModuleNodes)
		{
			FName ModuleSpacerKey = *FString::Printf(TEXT("Module%i"), ModuleIndex);
			UNiagaraStackSpacer* ModuleSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
				[=](UNiagaraStackSpacer* CurrentModuleSpacer) { return CurrentModuleSpacer->GetSpacerKey() == ModuleSpacerKey; });

			if (ModuleSpacer == nullptr)
			{
				ModuleSpacer = NewObject<UNiagaraStackSpacer>(this);
				ModuleSpacer->Initialize(CreateDefaultChildRequiredData(), ModuleSpacerKey, 1.4f);
			}

			NewChildren.Add(ModuleSpacer);

			UNiagaraStackModuleItem* ModuleItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackModuleItem>(CurrentChildren, 
				[=](UNiagaraStackModuleItem* CurrentModuleItem) { return &CurrentModuleItem->GetModuleNode() == ModuleNode; });

			if (ModuleItem == nullptr)
			{
				ModuleItem = NewObject<UNiagaraStackModuleItem>(this);
				ModuleItem->Initialize(CreateDefaultChildRequiredData(), GetAddUtilities(), *ModuleNode);
				ModuleItem->SetOnModifiedGroupItems(UNiagaraStackModuleItem::FOnModifiedGroupItems::CreateUObject(this, &UNiagaraStackScriptItemGroup::ChildModifiedGroupItems));
			}

			NewChildren.Add(ModuleItem);
			StackSpacerToModuleItemMap.Add(FObjectKey(ModuleSpacer), ModuleItem);

			ModuleIndex++;
		}

		// Add the post items spacer.
		FName ModuleSpacerKey = *FString::Printf(TEXT("Module%i"), ModuleIndex);
		UNiagaraStackSpacer* ModuleSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
			[=](UNiagaraStackSpacer* CurrentModuleSpacer) { return CurrentModuleSpacer->GetSpacerKey() == ModuleSpacerKey; });

		if (ModuleSpacer == nullptr)
		{
			ModuleSpacer = NewObject<UNiagaraStackSpacer>(this);
			ModuleSpacer->Initialize(CreateDefaultChildRequiredData(), ModuleSpacerKey, 1.4f);
		}

		NewChildren.Add(ModuleSpacer);
		StackSpacerToModuleItemMap.Add(FObjectKey(ModuleSpacer), nullptr);
	}
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
	RefreshIssues(NewIssues);
}

void UNiagaraStackScriptItemGroup::RefreshIssues(TArray<FStackIssue>& NewIssues)
{
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned = ScriptViewModel.Pin();
	checkf(ScriptViewModelPinned.IsValid(), TEXT("Can not refresh issues when the script view model has been deleted."));
	UNiagaraGraph* Graph = ScriptViewModelPinned->GetGraphViewModel()->GetGraph();
	FText ErrorMessage;
	
	if (FNiagaraStackGraphUtilities::ValidateGraphForOutput(*Graph, ScriptUsage, ScriptUsageId, ErrorMessage) == false)
	{
		UE_LOG(LogNiagaraEditor, Error, TEXT("Failed to Create Stack.  Message: %s"), *ErrorMessage.ToString());
		UNiagaraStackEntry::FStackIssue Error;
		Error.LongDescription = LOCTEXT("InvalidErrorText", "The data used to generate the stack has been corrupted and can not be used.\nUsing the fix option will reset this part of the stack to its default empty state.");
		Error.ShortDescription = LOCTEXT("InvalidErrorSummaryText", "The stack data is invalid");
		Error.UniqueIdentifier = FName(*FString::Printf(TEXT("StackDataInvalid-%s"), *GetStackEditorDataKey()));
		UNiagaraStackEntry::FStackIssueFix Fix;
		Fix.Description = LOCTEXT("FixStackGraph", "Fix invalid stack graph");
		Fix.FixDelegate.BindLambda([=]()
		{
			FScopedTransaction ScopedTransaction(Fix.Description);
			FNiagaraStackGraphUtilities::ResetGraphForOutput(*Graph, ScriptUsage, ScriptUsageId);
			FNiagaraStackGraphUtilities::RelayoutGraph(*Graph);
		});
		Error.Fixes.Add(Fix);
		NewIssues.Add(Error); 
	}
	else
	{
		bool bForcedError = false;
		if (ScriptUsage == ENiagaraScriptUsage::SystemUpdateScript)
		{
			// We need to make sure that System Update Scripts have the SystemLifecycle script for now.
			// The factor ensures this, but older assets may not have it or it may have been removed accidentally.
			// For now, treat this as an error and allow them to resolve.
			FString ModulePath = TEXT("/Niagara/Modules/System/SystemLifeCycle.SystemLifeCycle");
			FStringAssetReference SystemUpdateScriptRef(ModulePath);
			FAssetData ModuleScriptAsset;
			ModuleScriptAsset.ObjectPath = SystemUpdateScriptRef.GetAssetPathName();

			TArray<UNiagaraNodeFunctionCall*> FoundCalls;
			UNiagaraNodeOutput* MatchingOutputNode = Graph->FindOutputNode(ScriptUsage, ScriptUsageId);
			if (!FNiagaraStackGraphUtilities::FindScriptModulesInStack(ModuleScriptAsset, *MatchingOutputNode, FoundCalls))
			{
				bForcedError = true;
				UNiagaraStackEntry::FStackIssue Error;
				Error.LongDescription = LOCTEXT("SystemLifeCycleWarning", "The stack needs a SystemLifeCycle module.");;
				Error.ShortDescription = LOCTEXT("MissingRequiredMode", "Missing required module.");
				Error.UniqueIdentifier = FName(*FString::Printf(TEXT("MissingLifecycleModule-%s"), *GetStackEditorDataKey()));
				UNiagaraStackEntry::FStackIssueFix Fix;
				Fix.Description = LOCTEXT("AddingSystemLifecycleModule", "Adding System Lifecycle Module.");
				Fix.FixDelegate.BindLambda([=]()
				{
					FScopedTransaction ScopedTransaction(Fix.Description);
					UNiagaraNodeFunctionCall* AddedModuleNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScriptAsset, *MatchingOutputNode);
					if (AddedModuleNode == nullptr)
					{
						FNotificationInfo Info(LOCTEXT("FailedToAddSystemLifecycle", "Failed to add system life cycle module.\nCheck the log for errors."));
						Info.ExpireDuration = 5.0f;
						Info.bFireAndForget = true;
						Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
						FSlateNotificationManager::Get().AddNotification(Info);
					}
				});
				Error.Fixes.Add(Fix);
				NewIssues.Add(Error);
			}
		}

		ENiagaraScriptCompileStatus Status = ScriptViewModelPinned->GetScriptCompileStatus(GetScriptUsage(), GetScriptUsageId());
		if (!bForcedError)
		{
			if (Status == ENiagaraScriptCompileStatus::NCS_Error)
			{
				UNiagaraStackEntry::FStackIssue Error;
				Error.LongDescription = ScriptViewModelPinned->GetScriptErrors(GetScriptUsage(), GetScriptUsageId());
				Error.ShortDescription = LOCTEXT("ConpileErrorSummary", "The stack has compile errors.");
				Error.UniqueIdentifier = FName(*FString::Printf(TEXT("CompileErrors-%s"), *GetStackEditorDataKey()));
				NewIssues.Add(Error);
			}
		}
	}
}

void GenerateDragDropData(
	UNiagaraNodeFunctionCall* SourceModule,
	UNiagaraNodeFunctionCall* TargetModule, const UNiagaraGraph* TargetGraph, ENiagaraScriptUsage TargetScriptUsage, FGuid TargetScriptUsageId,
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup>& OutSourceStackGroups, int32& OutSourceGroupIndex,
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup>& OutTargetStackGroups, int32& OutTargetGroupIndex)
{
	// Find the output nodes for the source and target
	UNiagaraNodeOutput* SourceOutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*SourceModule);
	UNiagaraNodeOutput* TargetOutputNode = TargetModule != nullptr
		? FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*TargetModule)
		: TargetGraph->FindOutputNode(TargetScriptUsage, TargetScriptUsageId);

	// Collect the stack node groups for the source and target.
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*SourceOutputNode, OutSourceStackGroups);
	if (SourceOutputNode == TargetOutputNode)
	{
		OutTargetStackGroups.Append(OutSourceStackGroups);
	}
	else
	{
		FNiagaraStackGraphUtilities::GetStackNodeGroups(*TargetOutputNode, OutTargetStackGroups);
	}

	// Calculate the source and target groups indexes for the drag/drop
	OutSourceGroupIndex = INDEX_NONE;
	for (int32 GroupIndex = 0; GroupIndex < OutSourceStackGroups.Num(); GroupIndex++)
	{
		if (OutSourceStackGroups[GroupIndex].EndNode == SourceModule)
		{
			OutSourceGroupIndex = GroupIndex;
			break;
		}
	}

	OutTargetGroupIndex = INDEX_NONE;
	if (TargetModule == SourceModule)
	{
		OutTargetGroupIndex = OutSourceGroupIndex;
	}
	else if (TargetModule != nullptr)
	{
		for (int32 GroupIndex = 0; GroupIndex < OutTargetStackGroups.Num(); GroupIndex++)
		{
			if (OutTargetStackGroups[GroupIndex].EndNode == TargetModule)
			{
				OutTargetGroupIndex = GroupIndex;
				break;
			}
		}
	}
	else
	{
		// If there is no target module then we need to insert at the end.  The last group is the output node and we want to
		// insert before that.
		OutTargetGroupIndex = OutTargetStackGroups.Num() - 1;
	}
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackScriptItemGroup::ChildRequestCanDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	if (bIsValidForOutput && DraggedEntries.Num() == 1)
	{
		UNiagaraStackModuleItem* SourceModuleItem = Cast<UNiagaraStackModuleItem>(DraggedEntries[0]);
		if (SourceModuleItem != nullptr)
		{
			if (SourceModuleItem->CanMoveAndDelete() == false)
			{
				return FDropResult(false, LOCTEXT("CantMoveModuleError", "This inherited module can't be moved."));
			}

			if (SourceModuleItem->GetModuleNode().GetGraph() != ScriptViewModel.Pin()->GetGraphViewModel()->GetGraph())
			{
				return FDropResult(false, LOCTEXT("CantMoveModuleBetweenGraphsError", "This module can not be moved to this section of the stack"));
			}

			const UNiagaraStackSpacer* TargetSpacer = Cast<UNiagaraStackSpacer>(&TargetChild);
			if (TargetSpacer != nullptr)
			{
				UNiagaraStackModuleItem** TargetModuleItemPtr = StackSpacerToModuleItemMap.Find(FObjectKey(TargetSpacer));
				if (TargetModuleItemPtr != nullptr)
				{
					UNiagaraStackModuleItem* TargetModuleItem = *TargetModuleItemPtr;
					TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> SourceStackGroups;
					TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> TargetStackGroups;
					int32 SourceGroupIndex;
					int32 TargetGroupIndex;
					GenerateDragDropData(
						&SourceModuleItem->GetModuleNode(), TargetModuleItem != nullptr ? &TargetModuleItem->GetModuleNode() : nullptr,
						ScriptViewModel.Pin()->GetGraphViewModel()->GetGraph(), ScriptUsage, ScriptUsageId,
						SourceStackGroups, SourceGroupIndex,
						TargetStackGroups, TargetGroupIndex);

					// Make sure the source and target indices are within safe ranges, and make sure that the insert target isn't the source target or the spot directly
					// after the source target since that won't actually move the module.
					if (SourceGroupIndex > 0 && SourceGroupIndex < SourceStackGroups.Num() - 1 && TargetGroupIndex > 0 && TargetGroupIndex < TargetStackGroups.Num() &&
						SourceStackGroups[SourceGroupIndex].EndNode != TargetStackGroups[TargetGroupIndex].EndNode &&
						SourceStackGroups[SourceGroupIndex].EndNode != TargetStackGroups[TargetGroupIndex - 1].EndNode)
					{
						return FDropResult(true, LOCTEXT("MoveModuleResult", "Move this module here."));
					}
				}
			}
		}
	}
	return TOptional<FDropResult>();
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackScriptItemGroup::ChildRequestDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	if (bIsValidForOutput && DraggedEntries.Num() == 1)
	{
		UNiagaraStackModuleItem* SourceModuleItem = Cast<UNiagaraStackModuleItem>(DraggedEntries[0]);
		UNiagaraGraph* TargetGraph = ScriptViewModel.Pin()->GetGraphViewModel()->GetGraph();
		if (SourceModuleItem != nullptr &&
			SourceModuleItem->CanMoveAndDelete() &&
			SourceModuleItem->GetModuleNode().GetGraph() == TargetGraph)
		{
			const UNiagaraStackSpacer* TargetSpacer = Cast<UNiagaraStackSpacer>(&TargetChild);
			if (TargetSpacer != nullptr)
			{
				UNiagaraStackModuleItem** TargetModuleItemPtr = StackSpacerToModuleItemMap.Find(FObjectKey(TargetSpacer));
				if (TargetModuleItemPtr != nullptr)
				{
					UNiagaraStackModuleItem* TargetModuleItem = *TargetModuleItemPtr;
					TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> SourceStackGroups;
					TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> TargetStackGroups;
					int32 SourceGroupIndex;
					int32 TargetGroupIndex;
					GenerateDragDropData(
						&SourceModuleItem->GetModuleNode(), TargetModuleItem != nullptr ? &TargetModuleItem->GetModuleNode() : nullptr,
						TargetGraph, ScriptUsage, ScriptUsageId,
						SourceStackGroups, SourceGroupIndex,
						TargetStackGroups, TargetGroupIndex);

					// Make sure the source and target indices are within safe ranges, and make sure that the insert target isn't the source target or the spot directly
					// after the source target since that won't actually move the module.
					if (SourceGroupIndex > 0 && SourceGroupIndex < SourceStackGroups.Num() - 1 && TargetGroupIndex > 0 && TargetGroupIndex < TargetStackGroups.Num() &&
						SourceStackGroups[SourceGroupIndex].EndNode != TargetStackGroups[TargetGroupIndex].EndNode &&
						SourceStackGroups[SourceGroupIndex].EndNode != TargetStackGroups[TargetGroupIndex - 1].EndNode)
					{
						FScopedTransaction ScopedTransaction(LOCTEXT("DragAndDropModule", "Drag and drop module"));
						FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(SourceStackGroups[SourceGroupIndex], SourceStackGroups[SourceGroupIndex - 1], SourceStackGroups[SourceGroupIndex + 1]);
						FNiagaraStackGraphUtilities::ConnectStackNodeGroup(SourceStackGroups[SourceGroupIndex], TargetStackGroups[TargetGroupIndex - 1], TargetStackGroups[TargetGroupIndex]);

						FNiagaraStackGraphUtilities::RelayoutGraph(*TargetGraph);
						TargetGraph->NotifyGraphNeedsRecompile();

						SourceModuleItem->NotifyModuleMoved();
						RefreshChildren();

						return FDropResult(true);
					}
				}
			}
		}
	}
	return FDropResult(false);
}

void UNiagaraStackScriptItemGroup::ItemAdded()
{
	RefreshChildren();
}

void UNiagaraStackScriptItemGroup::ChildModifiedGroupItems()
{
	RefreshChildren();
}


#undef LOCTEXT_NAMESPACE

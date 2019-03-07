// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorUtilities.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraNodeInput.h"
#include "NiagaraDataInterface.h"
#include "NiagaraComponent.h"
#include "Modules/ModuleManager.h"
#include "UObject/StructOnScope.h"
#include "NiagaraGraph.h"
#include "NiagaraSystem.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScript.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphUtilities.h"
#include "NiagaraConstants.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "HAL/PlatformApplicationMisc.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "HAL/PlatformApplicationMisc.h"
#include "AssetRegistryModule.h"
#include "Misc/FeedbackContext.h"
#include "EdGraphSchema_Niagara.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "EdGraph/EdGraphPin.h"
#include "NiagaraNodeWriteDataSet.h"

#define LOCTEXT_NAMESPACE "FNiagaraEditorUtilities"

TSet<FName> FNiagaraEditorUtilities::GetSystemConstantNames()
{
	TSet<FName> SystemConstantNames;
	for (const FNiagaraVariable& SystemConstant : FNiagaraConstants::GetEngineConstants())
	{
		SystemConstantNames.Add(SystemConstant.GetName());
	}
	return SystemConstantNames;
}

void FNiagaraEditorUtilities::GetTypeDefaultValue(const FNiagaraTypeDefinition& Type, TArray<uint8>& DefaultData)
{
	if (const UScriptStruct* ScriptStruct = Type.GetScriptStruct())
	{
		FNiagaraVariable DefaultVariable(Type, NAME_None);
		ResetVariableToDefaultValue(DefaultVariable);

		DefaultData.SetNumUninitialized(Type.GetSize());
		DefaultVariable.CopyTo(DefaultData.GetData());
	}
}

void FNiagaraEditorUtilities::ResetVariableToDefaultValue(FNiagaraVariable& Variable)
{
	if (const UScriptStruct* ScriptStruct = Variable.GetType().GetScriptStruct())
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Variable.GetType());
		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanProvideDefaultValue())
		{
			TypeEditorUtilities->UpdateVariableWithDefaultValue(Variable);
		}
		else
		{
			Variable.AllocateData();
			ScriptStruct->InitializeDefaultValue(Variable.GetData());
		}
	}
}

void FNiagaraEditorUtilities::InitializeParameterInputNode(UNiagaraNodeInput& InputNode, const FNiagaraTypeDefinition& Type, const UNiagaraGraph* InGraph, FName InputName)
{
	InputNode.Usage = ENiagaraInputNodeUsage::Parameter;
	InputNode.bCanRenameNode = true;
	InputName = UNiagaraNodeInput::GenerateUniqueName(InGraph, InputName, ENiagaraInputNodeUsage::Parameter);
	InputNode.Input.SetName(InputName);
	InputNode.Input.SetType(Type);
	if (InGraph) // Only compute sort priority if a graph was passed in, similar to the way that GenrateUniqueName works above.
	{
		InputNode.CallSortPriority = UNiagaraNodeInput::GenerateNewSortPriority(InGraph, InputName, ENiagaraInputNodeUsage::Parameter);
	}
	if (Type.GetScriptStruct() != nullptr)
	{
		ResetVariableToDefaultValue(InputNode.Input);
		if (InputNode.GetDataInterface() != nullptr)
		{
			InputNode.SetDataInterface(nullptr);
		}
	}
	else
	{
		InputNode.Input.AllocateData(); // Frees previously used memory if we're switching from a struct to a class type.
		InputNode.SetDataInterface(NewObject<UNiagaraDataInterface>(&InputNode, const_cast<UClass*>(Type.GetClass()), NAME_None, RF_Transactional));
	}
}

void FNiagaraEditorUtilities::GetParameterVariablesFromSystem(UNiagaraSystem& System, TArray<FNiagaraVariable>& ParameterVariables,
	FNiagaraEditorUtilities::FGetParameterVariablesFromSystemOptions Options)
{
	UNiagaraScript* SystemScript = System.GetSystemSpawnScript();
	if (SystemScript != nullptr)
	{
		UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SystemScript->GetSource());
		if (ScriptSource != nullptr)
		{
			UNiagaraGraph* SystemGraph = ScriptSource->NodeGraph;
			if (SystemGraph != nullptr)
			{
				UNiagaraGraph::FFindInputNodeOptions FindOptions;
				FindOptions.bIncludeAttributes = false;
				FindOptions.bIncludeSystemConstants = false;
				FindOptions.bIncludeTranslatorConstants = false;
				FindOptions.bFilterDuplicates = true;

				TArray<UNiagaraNodeInput*> InputNodes;
				SystemGraph->FindInputNodes(InputNodes, FindOptions);
				for (UNiagaraNodeInput* InputNode : InputNodes)
				{
					bool bIsStructParameter = InputNode->Input.GetType().GetScriptStruct() != nullptr;
					bool bIsDataInterfaceParameter = InputNode->Input.GetType().GetClass() != nullptr;
					if ((bIsStructParameter && Options.bIncludeStructParameters) || (bIsDataInterfaceParameter && Options.bIncludeDataInterfaceParameters))
					{
						ParameterVariables.Add(InputNode->Input);
					}
				}
			}
		}
	}
}

// TODO: This is overly complicated.
void FNiagaraEditorUtilities::FixUpPastedInputNodes(UEdGraph* Graph, TSet<UEdGraphNode*> PastedNodes)
{
	// Collect existing inputs.
	TArray<UNiagaraNodeInput*> CurrentInputs;
	Graph->GetNodesOfClass<UNiagaraNodeInput>(CurrentInputs);
	TSet<FNiagaraVariable> ExistingInputs;
	TMap<FNiagaraVariable, UNiagaraNodeInput*> ExistingNodes;
	int32 HighestSortOrder = -1; // Set to -1 initially, so that in the event of no nodes, we still get zero.
	for (UNiagaraNodeInput* CurrentInput : CurrentInputs)
	{
		if (PastedNodes.Contains(CurrentInput) == false && CurrentInput->Usage == ENiagaraInputNodeUsage::Parameter)
		{
			ExistingInputs.Add(CurrentInput->Input);
			ExistingNodes.Add(CurrentInput->Input) = CurrentInput;
			if (CurrentInput->CallSortPriority > HighestSortOrder)
			{
				HighestSortOrder = CurrentInput->CallSortPriority;
			}
		}
	}

	// Collate pasted inputs nodes by their input for further processing.
	TMap<FNiagaraVariable, TArray<UNiagaraNodeInput*>> InputToPastedInputNodes;
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		UNiagaraNodeInput* PastedInputNode = Cast<UNiagaraNodeInput>(PastedNode);
		if (PastedInputNode != nullptr && PastedInputNode->Usage == ENiagaraInputNodeUsage::Parameter && ExistingInputs.Contains(PastedInputNode->Input) == false)
		{
			TArray<UNiagaraNodeInput*>* NodesForInput = InputToPastedInputNodes.Find(PastedInputNode->Input);
			if (NodesForInput == nullptr)
			{
				NodesForInput = &InputToPastedInputNodes.Add(PastedInputNode->Input);
			}
			NodesForInput->Add(PastedInputNode);
		}
	}

	// Fix up the nodes based on their relationship to the existing inputs.
	for (auto PastedPairIterator = InputToPastedInputNodes.CreateIterator(); PastedPairIterator; ++PastedPairIterator)
	{
		FNiagaraVariable PastedInput = PastedPairIterator.Key();
		TArray<UNiagaraNodeInput*>& PastedNodesForInput = PastedPairIterator.Value();

		// Try to find an existing input which matches the pasted input by both name and type so that the pasted nodes
		// can be assigned the same id and value, to facilitate pasting multiple times from the same source graph.
		FNiagaraVariable* MatchingInputByNameAndType = nullptr;
		UNiagaraNodeInput* MatchingNode = nullptr;
		for (FNiagaraVariable& ExistingInput : ExistingInputs)
		{
			if (PastedInput.GetName() == ExistingInput.GetName() && PastedInput.GetType() == ExistingInput.GetType())
			{
				MatchingInputByNameAndType = &ExistingInput;
				UNiagaraNodeInput** FoundNode = ExistingNodes.Find(ExistingInput);
				if (FoundNode != nullptr)
				{
					MatchingNode = *FoundNode;
				}
				break;
			}
		}

		if (MatchingInputByNameAndType != nullptr && MatchingNode != nullptr)
		{
			// Update the id and value on the matching pasted nodes.
			for (UNiagaraNodeInput* PastedNodeForInput : PastedNodesForInput)
			{
				if (nullptr == PastedNodeForInput)
				{
					continue;
				}
				PastedNodeForInput->CallSortPriority = MatchingNode->CallSortPriority;
				PastedNodeForInput->ExposureOptions = MatchingNode->ExposureOptions;
				PastedNodeForInput->Input.AllocateData();
				PastedNodeForInput->Input.SetData(MatchingInputByNameAndType->GetData());
			}
		}
		else
		{
			// Check for duplicate names
			TSet<FName> ExistingNames;
			for (FNiagaraVariable& ExistingInput : ExistingInputs)
			{
				ExistingNames.Add(ExistingInput.GetName());
			}
			if (ExistingNames.Contains(PastedInput.GetName()))
			{
				FName UniqueName = FNiagaraUtilities::GetUniqueName(PastedInput.GetName(), ExistingNames.Union(FNiagaraEditorUtilities::GetSystemConstantNames()));
				for (UNiagaraNodeInput* PastedNodeForInput : PastedNodesForInput)
				{
					PastedNodeForInput->Input.SetName(UniqueName);
				}
			}

			// Assign the pasted inputs the same new id and add them to the end of the parameters list.
			int32 NewSortOrder = ++HighestSortOrder;
			for (UNiagaraNodeInput* PastedNodeForInput : PastedNodesForInput)
			{
				PastedNodeForInput->CallSortPriority = NewSortOrder;
			}
		}
	}
}

void FNiagaraEditorUtilities::WriteTextFileToDisk(FString SaveDirectory, FString FileName, FString TextToSave, bool bAllowOverwriting)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// CreateDirectoryTree returns true if the destination
	// directory existed prior to call or has been created
	// during the call.
	if (PlatformFile.CreateDirectoryTree(*SaveDirectory))
	{
		// Get absolute file path
		FString AbsoluteFilePath = SaveDirectory + "/" + FileName;

		// Allow overwriting or file doesn't already exist
		if (bAllowOverwriting || !PlatformFile.FileExists(*AbsoluteFilePath))
		{
			if (FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath))
			{
				UE_LOG(LogNiagaraEditor, Log, TEXT("Wrote file to %s"), *AbsoluteFilePath);
				return;
			}

		}
	}
}

void FNiagaraEditorUtilities::GatherChangeIds(UNiagaraEmitter& Emitter, TMap<FGuid, FGuid>& ChangeIds, const FString& InDebugName, bool bWriteToLogDir)
{
	FString ExportText;
	ChangeIds.Empty();
	TArray<UNiagaraGraph*> Graphs;
	TArray<UNiagaraScript*> Scripts;
	Emitter.GetScripts(Scripts);

	// First gather all the graphs used by this emitter..
	for (UNiagaraScript* Script : Scripts)
	{
		if (Script != nullptr && Script->GetSource() != nullptr)
		{
			UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(Script->GetSource());
			if (ScriptSource != nullptr)
			{
				Graphs.AddUnique(ScriptSource->NodeGraph);
			}

			if (bWriteToLogDir)
			{
				FNiagaraVMExecutableDataId Id;
				Script->ComputeVMCompilationId(Id);
				FString KeyString;
				Id.AppendKeyString(KeyString);

				UEnum* FoundEnum = StaticEnum<ENiagaraScriptUsage>();

				FString ResultsEnum = TEXT("??");
				if (FoundEnum)
				{
					ResultsEnum = FoundEnum->GetNameStringByValue((int64)Script->Usage);
				}

				ExportText += FString::Printf(TEXT("Usage: %s CompileKey: %s\n"), *ResultsEnum, *KeyString );
			}
		}
	}

	
	// Now gather all the node change id's within these graphs.
	for (UNiagaraGraph* Graph : Graphs)
	{
		TArray<UNiagaraNode*> Nodes;
		Graph->GetNodesOfClass(Nodes);

		for (UNiagaraNode* Node : Nodes)
		{
			ChangeIds.Add(Node->NodeGuid, Node->GetChangeId());

			if (bWriteToLogDir)
			{
				ExportText += FString::Printf(TEXT("%40s    guid: %25s    changeId: %25s\n"), *Node->GetName(), *Node->NodeGuid.ToString(), *Node->GetChangeId().ToString());

			}
		}
	}

	if (bWriteToLogDir)
	{
		FNiagaraEditorUtilities::WriteTextFileToDisk(FPaths::ProjectLogDir(), InDebugName + TEXT(".txt"), ExportText, true);
	}
	
}

void FNiagaraEditorUtilities::GatherChangeIds(UNiagaraGraph& Graph, TMap<FGuid, FGuid>& ChangeIds, const FString& InDebugName, bool bWriteToLogDir)
{
	ChangeIds.Empty();
	TArray<UNiagaraGraph*> Graphs;
	
	FString ExportText;
	// Now gather all the node change id's within these graphs.
	{
		TArray<UNiagaraNode*> Nodes;
		Graph.GetNodesOfClass(Nodes);

		for (UNiagaraNode* Node : Nodes)
		{
			ChangeIds.Add(Node->NodeGuid, Node->GetChangeId());
			if (bWriteToLogDir)
			{
				ExportText += FString::Printf(TEXT("%40s    guid: %25s    changeId: %25s\n"), *Node->GetName(), *Node->NodeGuid.ToString(), *Node->GetChangeId().ToString());
			}
		}
	}

	if (bWriteToLogDir)
	{
		FNiagaraEditorUtilities::WriteTextFileToDisk(FPaths::ProjectLogDir(), InDebugName + TEXT(".txt"), ExportText, true);
	}
}

FText FNiagaraEditorUtilities::StatusToText(ENiagaraScriptCompileStatus Status)
{
	switch (Status)
	{
	default:
	case ENiagaraScriptCompileStatus::NCS_Unknown:
		return LOCTEXT("Recompile_Status", "Unknown status; should recompile");
	case ENiagaraScriptCompileStatus::NCS_Dirty:
		return LOCTEXT("Dirty_Status", "Dirty; needs to be recompiled");
	case ENiagaraScriptCompileStatus::NCS_Error:
		return LOCTEXT("CompileError_Status", "There was an error during compilation, see the log for details");
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return LOCTEXT("GoodToGo_Status", "Good to go");
	case ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings:
		return LOCTEXT("GoodToGoWarning_Status", "There was a warning during compilation, see the log for details");
	}
}

ENiagaraScriptCompileStatus FNiagaraEditorUtilities::UnionCompileStatus(const ENiagaraScriptCompileStatus& StatusA, const ENiagaraScriptCompileStatus& StatusB)
{
	if (StatusA != StatusB)
	{
		if (StatusA == ENiagaraScriptCompileStatus::NCS_Unknown || StatusB == ENiagaraScriptCompileStatus::NCS_Unknown)
		{
			return ENiagaraScriptCompileStatus::NCS_Unknown;
		}
		else if (StatusA >= ENiagaraScriptCompileStatus::NCS_MAX || StatusB >= ENiagaraScriptCompileStatus::NCS_MAX)
		{
			return ENiagaraScriptCompileStatus::NCS_MAX;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_Dirty || StatusB == ENiagaraScriptCompileStatus::NCS_Dirty)
		{
			return ENiagaraScriptCompileStatus::NCS_Dirty;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_Error || StatusB == ENiagaraScriptCompileStatus::NCS_Error)
		{
			return ENiagaraScriptCompileStatus::NCS_Error;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings || StatusB == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings)
		{
			return ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_BeingCreated || StatusB == ENiagaraScriptCompileStatus::NCS_BeingCreated)
		{
			return ENiagaraScriptCompileStatus::NCS_BeingCreated;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_UpToDate || StatusB == ENiagaraScriptCompileStatus::NCS_UpToDate)
		{
			return ENiagaraScriptCompileStatus::NCS_UpToDate;
		}
		else
		{
			return ENiagaraScriptCompileStatus::NCS_Unknown;
		}
	}
	else
	{
		return StatusA;
	}
}

bool FNiagaraEditorUtilities::DataMatches(const FNiagaraVariable& Variable, const FStructOnScope& StructOnScope)
{
	if (Variable.GetType().GetScriptStruct() != StructOnScope.GetStruct() ||
		Variable.IsDataAllocated() == false)
	{
		return false;
	}

	return FMemory::Memcmp(Variable.GetData(), StructOnScope.GetStructMemory(), Variable.GetSizeInBytes()) == 0;
}

bool FNiagaraEditorUtilities::DataMatches(const FNiagaraVariable& VariableA, const FNiagaraVariable& VariableB)
{
	if (VariableA.GetType() != VariableB.GetType())
	{
		return false;
	}

	if (VariableA.IsDataAllocated() != VariableB.IsDataAllocated())
	{
		return false;
	}

	if (VariableA.IsDataAllocated())
	{
		return FMemory::Memcmp(VariableA.GetData(), VariableB.GetData(), VariableA.GetSizeInBytes()) == 0;
	}

	return true;
}

bool FNiagaraEditorUtilities::DataMatches(const FStructOnScope& StructOnScopeA, const FStructOnScope& StructOnScopeB)
{
	if (StructOnScopeA.GetStruct() != StructOnScopeB.GetStruct())
	{
		return false;
	}

	return FMemory::Memcmp(StructOnScopeA.GetStructMemory(), StructOnScopeB.GetStructMemory(), StructOnScopeA.GetStruct()->GetStructureSize()) == 0;
}

TSharedPtr<SWidget> FNiagaraEditorUtilities::CreateInlineErrorText(TAttribute<FText> ErrorMessage, TAttribute<FText> ErrorTooltip)
{
	TSharedPtr<SHorizontalBox> ErrorInternalBox = SNew(SHorizontalBox);
		ErrorInternalBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(ErrorMessage)
			];

		return SNew(SHorizontalBox)
			.ToolTipText(ErrorTooltip)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Icons.Error"))
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					ErrorInternalBox.ToSharedRef()
				];
}

void FNiagaraEditorUtilities::CompileExistingEmitters(const TArray<UNiagaraEmitter*>& AffectedEmitters)
{
	TSet<UNiagaraEmitter*> CompiledEmitters;
	for (UNiagaraEmitter* Emitter : AffectedEmitters)
	{
		// If we've already compiled this emitter, or it's invalid skip it.
		if (CompiledEmitters.Contains(Emitter) || Emitter->IsPendingKillOrUnreachable())
		{
			continue;
		}

		// We only need to compile emitters referenced directly as instances by systems since emitters can now only be used in the context 
		// of a system.
		for (TObjectIterator<UNiagaraSystem> SystemIterator; SystemIterator; ++SystemIterator)
		{
			if (SystemIterator->ReferencesInstanceEmitter(*Emitter))
			{
				SystemIterator->RequestCompile(false);

				TArray<TSharedPtr<FNiagaraSystemViewModel>> ExistingSystemViewModels;
				FNiagaraSystemViewModel::GetAllViewModelsForObject(*SystemIterator, ExistingSystemViewModels);
				for (TSharedPtr<FNiagaraSystemViewModel> SystemViewModel : ExistingSystemViewModels)
				{
					SystemViewModel->RefreshAll();
				}

				for (const FNiagaraEmitterHandle& EmitterHandle : SystemIterator->GetEmitterHandles())
				{
					CompiledEmitters.Add(EmitterHandle.GetInstance());
				}
			}
		}
	}
}

bool FNiagaraEditorUtilities::TryGetEventDisplayName(UNiagaraEmitter* Emitter, FGuid EventUsageId, FText& OutEventDisplayName)
{
	if (Emitter != nullptr)
	{
		for (const FNiagaraEventScriptProperties& EventScriptProperties : Emitter->GetEventHandlers())
		{
			if (EventScriptProperties.Script->GetUsageId() == EventUsageId)
			{
				OutEventDisplayName = FText::FromName(EventScriptProperties.SourceEventName);
				return true;
			}
		}
	}
	return false;
}

bool FNiagaraEditorUtilities::IsCompilableAssetClass(UClass* AssetClass)
{
	static const TSet<UClass*> CompilableClasses = { UNiagaraScript::StaticClass(), UNiagaraEmitter::StaticClass(), UNiagaraSystem::StaticClass() };
	return CompilableClasses.Contains(AssetClass);
}

void FNiagaraEditorUtilities::MarkDependentCompilableAssetsDirty(TArray<UObject*> InObjects)
{
	const FText LoadAndMarkDirtyDisplayName = NSLOCTEXT("NiagaraEditor", "MarkDependentAssetsDirtySlowTask", "Loading and marking dependent assets dirty.");
	GWarn->BeginSlowTask(LoadAndMarkDirtyDisplayName, true, true);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetIdentifier> ReferenceNames;

	TArray<FAssetData> AssetsToLoadAndMarkDirty;
	TArray<FAssetData> AssetsToCheck;

	for (UObject* InObject : InObjects)
	{	
		AssetsToCheck.Add(FAssetData(InObject));
	}

	while (AssetsToCheck.Num() > 0)
	{
		FAssetData AssetToCheck = AssetsToCheck[0];
		AssetsToCheck.RemoveAtSwap(0);
		if (IsCompilableAssetClass(AssetToCheck.GetClass()))
		{
			if (AssetsToLoadAndMarkDirty.Contains(AssetToCheck) == false)
			{
				AssetsToLoadAndMarkDirty.Add(AssetToCheck);
				TArray<FName> Referencers;
				AssetRegistryModule.Get().GetReferencers(AssetToCheck.PackageName, Referencers);
				for (FName& Referencer : Referencers)
				{
					AssetRegistryModule.Get().GetAssetsByPackageName(Referencer, AssetsToCheck);
				}
			}
		}
	}

	int32 ItemIndex = 0;
	for (FAssetData AssetDataToLoadAndMarkDirty : AssetsToLoadAndMarkDirty)
	{	
		if (GWarn->ReceivedUserCancel())
		{
			break;
		}
		GWarn->StatusUpdate(ItemIndex++, AssetsToLoadAndMarkDirty.Num(), LoadAndMarkDirtyDisplayName);
		UObject* AssetToMarkDirty = AssetDataToLoadAndMarkDirty.GetAsset();
		AssetToMarkDirty->Modify(true);
	}

	GWarn->EndSlowTask();
}

template<typename Action>
void TraverseGraphFromOutputDepthFirst(const UEdGraphSchema_Niagara* Schema, UNiagaraNode* Node, Action& VisitAction)
{
	UNiagaraGraph* Graph = Node->GetNiagaraGraph();
	TArray<UNiagaraNode*> Nodes;
	Graph->BuildTraversal(Nodes, Node);
	for (UNiagaraNode* GraphNode : Nodes)
	{
		VisitAction(Schema, GraphNode);
	}
}

void FixUpNumericPinsVisitor(const UEdGraphSchema_Niagara* Schema, UNiagaraNode* Node)
{
	Node->ResolveNumerics(Schema, true, nullptr);
}

void FNiagaraEditorUtilities::FixUpNumericPins(const UEdGraphSchema_Niagara* Schema, UNiagaraNode* Node)
{
	auto FixUpVisitor = [&](const UEdGraphSchema_Niagara* LSchema, UNiagaraNode* LNode) { FixUpNumericPinsVisitor(LSchema, LNode); };
	TraverseGraphFromOutputDepthFirst(Schema, Node, FixUpVisitor);
}

/* Go through the graph and attempt to auto-detect the type of any numeric pins by working back from the leaves of the graph. Only change the types of pins, not FNiagaraVariables.*/
void PreprocessGraph(const UEdGraphSchema_Niagara* Schema, UNiagaraGraph* Graph, UNiagaraNodeOutput* OutputNode)
{
	check(OutputNode);
	{
		FNiagaraEditorUtilities::FixUpNumericPins(Schema, OutputNode);
	}
}

/* Go through the graph and force any input nodes with Numeric types to a hard-coded type of float. This will allow modules and functions to compile properly.*/
void PreProcessGraphForInputNumerics(const UEdGraphSchema_Niagara* Schema, UNiagaraGraph* Graph, TArray<FNiagaraVariable>& OutChangedNumericParams)
{
	// Visit all input nodes
	TArray<UNiagaraNodeInput*> InputNodes;
	Graph->FindInputNodes(InputNodes);
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		// See if any of the output pins are of Numeric type. If so, force to floats.
		TArray<UEdGraphPin*> OutputPins;
		InputNode->GetOutputPins(OutputPins);
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			FNiagaraTypeDefinition OutputPinType = Schema->PinToTypeDefinition(OutputPin);
			if (OutputPinType == FNiagaraTypeDefinition::GetGenericNumericDef())
			{
				OutputPin->PinType = Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetFloatDef());
			}
		}

		// Record that we touched this variable for later cleanup and make sure that the 
		// variable's type now matches the pin.
		if (InputNode->Input.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			OutChangedNumericParams.Add(InputNode->Input);
			InputNode->Input.SetType(FNiagaraTypeDefinition::GetFloatDef());
		}
	}
}

/* Should be called after all pins have been successfully auto-detected for type. This goes through and synchronizes any Numeric FNiagaraVarible outputs with the deduced pin type. This will allow modules and functions to compile properly.*/
void PreProcessGraphForAttributeNumerics(const UEdGraphSchema_Niagara* Schema, UNiagaraGraph* Graph, UNiagaraNodeOutput* OutputNode, TArray<FNiagaraVariable>& OutChangedNumericParams)
{
	// Visit the output node
	if (OutputNode != nullptr)
	{
		// For each pin, make sure that if it has a valid type, but the associated variable is still Numeric,
		// force the variable to match the pin's new type. Record that we touched this variable for later cleanup.
		TArray<UEdGraphPin*> InputPins;
		OutputNode->GetInputPins(InputPins);
		check(OutputNode->Outputs.Num() == InputPins.Num());
		for (int32 i = 0; i < InputPins.Num(); i++)
		{
			FNiagaraVariable& Param = OutputNode->Outputs[i];
			UEdGraphPin* InputPin = InputPins[i];

			FNiagaraTypeDefinition InputPinType = Schema->PinToTypeDefinition(InputPin);
			if (Param.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef() &&
				InputPinType != FNiagaraTypeDefinition::GetGenericNumericDef())
			{
				OutChangedNumericParams.Add(Param);
				Param.SetType(InputPinType);
			}
		}
	}
}

void FNiagaraEditorUtilities::ResolveNumerics(UNiagaraGraph* SourceGraph, bool bForceParametersToResolveNumerics, TArray<FNiagaraVariable>& ChangedNumericParams)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(SourceGraph->GetSchema());

	// In the case of functions or modules, we may not have enough information at this time to fully resolve the type. In that case,
	// we circumvent the resulting errors by forcing a type. This gives the user an appropriate level of type checking. We will, however need to clean this up in
	// the parameters that we output.
	//bool bForceParametersToResolveNumerics = InScript->IsStandaloneScript();
	if (bForceParametersToResolveNumerics)
	{
		PreProcessGraphForInputNumerics(Schema, SourceGraph, ChangedNumericParams);
	}

	// Auto-deduce the input types for numerics in the graph and overwrite the types on the pins. If PreProcessGraphForInputNumerics occurred, then
	// we will have pre-populated the inputs with valid types.
	TArray<UNiagaraNodeOutput*> OutputNodes;
	SourceGraph->FindOutputNodes(OutputNodes);

	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		PreprocessGraph(Schema, SourceGraph, OutputNode);

		// Now that we've auto-deduced the types, we need to handle any lingering Numerics in the Output's FNiagaraVariable outputs. 
		// We use the pin's deduced type to temporarily overwrite the variable's type.
		if (bForceParametersToResolveNumerics)
		{
			PreProcessGraphForAttributeNumerics(Schema, SourceGraph, OutputNode, ChangedNumericParams);
		}
	}
}

void FNiagaraEditorUtilities::PreprocessFunctionGraph(const UEdGraphSchema_Niagara* Schema, UNiagaraGraph* Graph, const TArray<UEdGraphPin*>& CallInputs, const TArray<UEdGraphPin*>& CallOutputs, ENiagaraScriptUsage ScriptUsage)
{
	// Change any numeric inputs or outputs to match the types from the call node.
	TArray<UNiagaraNodeInput*> InputNodes;

	// Only handle nodes connected to the correct output node in the event of multiple output nodes in the graph.
	UNiagaraGraph::FFindInputNodeOptions Options;
	Options.bFilterByScriptUsage = true;
	Options.TargetScriptUsage = ScriptUsage;

	Graph->FindInputNodes(InputNodes, Options);

	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		FNiagaraVariable& Input = InputNode->Input;
		if (Input.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			UEdGraphPin* const* MatchingPin = CallInputs.FindByPredicate([&](const UEdGraphPin* Pin) { return (Pin->PinName == Input.GetName()); });

			if (MatchingPin != nullptr)
			{
				FNiagaraTypeDefinition PinType = Schema->PinToTypeDefinition(*MatchingPin);
				Input.SetType(PinType);
				TArray<UEdGraphPin*> OutputPins;
				InputNode->GetOutputPins(OutputPins);
				check(OutputPins.Num() == 1);
				OutputPins[0]->PinType = (*MatchingPin)->PinType;
			}
		}
	}

	UNiagaraNodeOutput* OutputNode = Graph->FindOutputNode(ScriptUsage);
	check(OutputNode);

	TArray<UEdGraphPin*> InputPins;
	OutputNode->GetInputPins(InputPins);

	for (FNiagaraVariable& Output : OutputNode->Outputs)
	{
		if (Output.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			UEdGraphPin* const* MatchingPin = CallOutputs.FindByPredicate([&](const UEdGraphPin* Pin) { return (Pin->PinName == Output.GetName()); });

			if (MatchingPin != nullptr)
			{
				FNiagaraTypeDefinition PinType = Schema->PinToTypeDefinition(*MatchingPin);
				Output.SetType(PinType);
			}
		}
	}

	FNiagaraEditorUtilities::FixUpNumericPins(Schema, OutputNode);

}

void FNiagaraEditorUtilities::GetFilteredScriptAssets(FGetFilteredScriptAssetsOptions InFilter, TArray<FAssetData>& OutFilteredScriptAssets)
{
	FARFilter ScriptFilter;
	ScriptFilter.ClassNames.Add(UNiagaraScript::StaticClass()->GetFName());

	const UEnum* NiagaraScriptUsageEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"), true);
	const FString QualifiedScriptUsageString = NiagaraScriptUsageEnum->GetNameStringByValue(static_cast<uint8>(InFilter.ScriptUsageToInclude));
	int32 LastColonIndex;
	QualifiedScriptUsageString.FindLastChar(TEXT(':'), LastColonIndex);
	const FString UnqualifiedScriptUsageString = QualifiedScriptUsageString.RightChop(LastColonIndex + 1);
	ScriptFilter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UNiagaraScript, Usage), UnqualifiedScriptUsageString);

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssets(ScriptFilter, OutFilteredScriptAssets);

	// We remove deprecated scripts separately as FARFilter does not support filtering by non-string tags.
	if (InFilter.bIncludeDeprecatedScripts == false)
	{
		bool bScriptIsDeprecated = false;
		bool bFoundDeprecatedTag = false;
		for (int i = OutFilteredScriptAssets.Num() - 1; i >= 0; --i)
		{
			bFoundDeprecatedTag = OutFilteredScriptAssets[i].GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraScript, bDeprecated), bScriptIsDeprecated);
			// If the asset does not have the metadata tag, check if it is loaded and if so check the bDeprecated value directly
			if (bFoundDeprecatedTag == false)
			{
				if (OutFilteredScriptAssets[i].IsAssetLoaded())
				{
					UNiagaraScript* Script = static_cast<UNiagaraScript*>(OutFilteredScriptAssets[i].GetAsset());
					if (Script != nullptr)
					{
						bScriptIsDeprecated = Script->bDeprecated;
					}
				}
			}
			if (bScriptIsDeprecated)
			{
				OutFilteredScriptAssets.RemoveAt(i);
			}
		}
	}
	// We remove scripts with non matching usage bitmasks separately as FARFilter does not support filtering by non-string tags.
	if (InFilter.TargetUsageToMatch.IsSet())
	{
		FString BitfieldTagValue;
		int32 BitfieldValue, TargetBit;
		for (int i = OutFilteredScriptAssets.Num() - 1; i >= 0; --i)
		{
			BitfieldTagValue = OutFilteredScriptAssets[i].GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UNiagaraScript, ModuleUsageBitmask));
			BitfieldValue = FCString::Atoi(*BitfieldTagValue);

			TargetBit = (BitfieldValue >> (int32)InFilter.TargetUsageToMatch.GetValue()) & 1;
			if (TargetBit != 1)
			{
				OutFilteredScriptAssets.RemoveAt(i);
			}
		}
	}
}

UNiagaraNodeOutput* FNiagaraEditorUtilities::GetScriptOutputNode(UNiagaraScript& Script)
{
	UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(Script.GetSource());
	return Source->NodeGraph->FindEquivalentOutputNode(Script.GetUsage(), Script.GetUsageId());
}

UNiagaraScript* FNiagaraEditorUtilities::GetScriptFromSystem(UNiagaraSystem& System, FGuid EmitterHandleId, ENiagaraScriptUsage Usage, FGuid UsageId)
{
	if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::SystemSpawnScript))
	{
		return System.GetSystemSpawnScript();
	}
	else if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::SystemUpdateScript))
	{
		return System.GetSystemUpdateScript();
	}
	else if (EmitterHandleId.IsValid())
	{
		const FNiagaraEmitterHandle* ScriptEmitterHandle = System.GetEmitterHandles().FindByPredicate(
			[EmitterHandleId](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == EmitterHandleId; });
		if (ScriptEmitterHandle != nullptr)
		{
			if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::EmitterSpawnScript))
			{
				return ScriptEmitterHandle->GetInstance()->EmitterSpawnScriptProps.Script;
			}
			else if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::EmitterUpdateScript))
			{
				return ScriptEmitterHandle->GetInstance()->EmitterUpdateScriptProps.Script;
			}
			else if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::ParticleSpawnScript))
			{
				return ScriptEmitterHandle->GetInstance()->SpawnScriptProps.Script;
			}
			else if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::ParticleUpdateScript))
			{
				return ScriptEmitterHandle->GetInstance()->UpdateScriptProps.Script;
			}
			else if (UNiagaraScript::IsEquivalentUsage(Usage, ENiagaraScriptUsage::ParticleEventScript))
			{
				for (const FNiagaraEventScriptProperties& EventScriptProperties : ScriptEmitterHandle->GetInstance()->GetEventHandlers())
				{
					if (EventScriptProperties.Script->GetUsageId() == UsageId)
					{
						return EventScriptProperties.Script;
					}
				}
			}
		}
	}
	return nullptr;
}

const FNiagaraEmitterHandle* FNiagaraEditorUtilities::GetEmitterHandleForEmitter(UNiagaraSystem& System, UNiagaraEmitter& Emitter)
{
	return System.GetEmitterHandles().FindByPredicate(
		[&Emitter](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetInstance() == &Emitter; });
}

FText FNiagaraEditorUtilities::FormatScriptAssetDescription(FText Description, FName Path)
{
	return Description.IsEmptyOrWhitespace()
		? FText::Format(LOCTEXT("ScriptAssetDescriptionFormatPathOnly", "Path: {0}"), FText::FromName(Path))
		: FText::Format(LOCTEXT("ScriptAssetDescriptionFormat", "Description: {1}\nPath: {0}"), FText::FromName(Path), Description);
}

#undef LOCTEXT_NAMESPACE

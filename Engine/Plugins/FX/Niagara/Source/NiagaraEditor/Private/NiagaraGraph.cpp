// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraGraph.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "NiagaraConstants.h"
#include "NiagaraSystem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeWriteDataSet.h"
#include "NiagaraNodeReadDataSet.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraDataInterface.h"
#include "GraphEditAction.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeParameterMapGet.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNode.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"

DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - FindInputNodes"), STAT_NiagaraEditor_Graph_FindInputNodes, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - FindInputNodes_NotFilterUsage"), STAT_NiagaraEditor_Graph_FindInputNodes_NotFilterUsage, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - FindInputNodes_FilterUsage"), STAT_NiagaraEditor_Graph_FindInputNodes_FilterUsage, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - FindInputNodes_FilterDupes"), STAT_NiagaraEditor_Graph_FindInputNodes_FilterDupes, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - FindInputNodes_FindInputNodes_Sort"), STAT_NiagaraEditor_Graph_FindInputNodes_Sort, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - FindOutputNode"), STAT_NiagaraEditor_Graph_FindOutputNode, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("NiagaraEditor - Graph - BuildTraversalHelper"), STAT_NiagaraEditor_Graph_BuildTraversalHelper, STATGROUP_NiagaraEditor);

bool bWriteToLog = false;

#define LOCTEXT_NAMESPACE "NiagaraGraph"

FNiagaraGraphParameterReferenceCollection::FNiagaraGraphParameterReferenceCollection(const bool bInCreated)
	: bCreated(bInCreated)
{
}

bool FNiagaraGraphParameterReferenceCollection::WasCreated() const
{
	return bCreated;
}

FNiagaraGraphScriptUsageInfo::FNiagaraGraphScriptUsageInfo() 
{
	DataHash.AddZeroed(sizeof(FSHAHash));
}

UNiagaraGraph::UNiagaraGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bFindParametersAllowed(true)
	, bIsRenamingParameter(false)
{
	Schema = UEdGraphSchema_Niagara::StaticClass();
	ChangeId = FGuid::NewGuid();
}

FDelegateHandle UNiagaraGraph::AddOnGraphNeedsRecompileHandler(const FOnGraphChanged::FDelegate& InHandler)
{
	return OnGraphNeedsRecompile.Add(InHandler);
}

void UNiagaraGraph::RemoveOnGraphNeedsRecompileHandler(FDelegateHandle Handle)
{
	OnGraphNeedsRecompile.Remove(Handle);
}

void UNiagaraGraph::NotifyGraphChanged(const FEdGraphEditAction& InAction)
{
	FindParameters();
	if ((InAction.Action & GRAPHACTION_AddNode) != 0 || (InAction.Action & GRAPHACTION_RemoveNode) != 0 ||
		(InAction.Action & GRAPHACTION_GenericNeedsRecompile) != 0)
	{
		MarkGraphRequiresSynchronization(TEXT("Graph Changed"));
	}
	if ((InAction.Action & GRAPHACTION_GenericNeedsRecompile) != 0)
	{
		OnGraphNeedsRecompile.Broadcast(InAction);
		return;
	}
	Super::NotifyGraphChanged(InAction);
}

void UNiagaraGraph::NotifyGraphChanged()
{
	FindParameters();
	Super::NotifyGraphChanged();
}

void UNiagaraGraph::PostLoad()
{
	Super::PostLoad();

	// In the past, we didn't bother setting the CallSortPriority and just used lexicographic ordering.
	// In the event that we have multiple non-matching nodes with a zero call sort priority, this will
	// give every node a unique order value.
	TArray<UNiagaraNodeInput*> InputNodes;
	GetNodesOfClass(InputNodes);
	bool bAllZeroes = true;
	TArray<FName> UniqueNames;
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (InputNode->CallSortPriority != 0)
		{
			bAllZeroes = false;
		}

		if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
		{
			UniqueNames.AddUnique(InputNode->Input.GetName());
		}

		if (InputNode->Usage == ENiagaraInputNodeUsage::SystemConstant)
		{
			InputNode->Input = FNiagaraConstants::UpdateEngineConstant(InputNode->Input);
		}
	}



	if (bAllZeroes && UniqueNames.Num() > 1)
	{
		// Just do the lexicographic sort and assign the call order to their ordered index value.
		UniqueNames.Sort();
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
			{
				int32 FoundIndex = UniqueNames.Find(InputNode->Input.GetName());
				check(FoundIndex != -1);
				InputNode->CallSortPriority = FoundIndex;
			}
		}
	}

	// If this is from a prior version, enforce a valid Change Id!
	if (ChangeId.IsValid() == false)
	{
		MarkGraphRequiresSynchronization(TEXT("Graph change id was invalid"));
	}

	// Assume that all externally referenced assets have changed, so update to match. They will return true if they have changed.
	TArray<UNiagaraNode*> NiagaraNodes;
	GetNodesOfClass<UNiagaraNode>(NiagaraNodes);
	bool bAnyExternalChanges = false;
	for (UNiagaraNode* NiagaraNode : NiagaraNodes)
	{
		UObject* ReferencedAsset = NiagaraNode->GetReferencedAsset();
		if (ReferencedAsset != nullptr)
		{
			ReferencedAsset->ConditionalPostLoad();
			NiagaraNode->ConditionalPostLoad();
			if (NiagaraNode->RefreshFromExternalChanges())
			{
				bAnyExternalChanges = true;
				
			}
		}
		else
		{
			NiagaraNode->ConditionalPostLoad();
		}
	}

	RebuildCachedData();

	if (GIsEditor)
	{
		SetFlags(RF_Transactional);
	}

	Parameters.Empty();
	FindParameters();
}

void UNiagaraGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	NotifyGraphChanged();
}

class UNiagaraScriptSource* UNiagaraGraph::GetSource() const
{
	return CastChecked<UNiagaraScriptSource>(GetOuter());
}

FGuid UNiagaraGraph::GetCompileID(ENiagaraScriptUsage InUsage, const FGuid& InUsageId)
{
	RebuildCachedData();

	// Since there gpu compute script contains spawn, update, and emitter logic, and we can only return one,
	// just return the particle spawn script here.
	if (InUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		InUsage = ENiagaraScriptUsage::ParticleSpawnScript;
	}

	for (int32 j = 0; j < CachedUsageInfo.Num(); j++)
	{
		if (UNiagaraScript::IsEquivalentUsage(CachedUsageInfo[j].UsageType, InUsage) && CachedUsageInfo[j].UsageId == InUsageId)
		{
			return CachedUsageInfo[j].GeneratedCompileId;
		}
	}

	return FGuid();

}

UEdGraphPin* UNiagaraGraph::FindParameterMapDefaultValuePin(const FName VariableName, ENiagaraScriptUsage InUsage, ENiagaraScriptUsage InParentUsage) const
{
	TArray<UEdGraphPin*> MatchingDefaultPins;
	
	TArray<UNiagaraNode*> NodesTraversed;
	BuildTraversal(NodesTraversed, InUsage, FGuid());

	UEdGraphPin* DefaultInputPin = nullptr;
	for (UNiagaraNode* Node : NodesTraversed)
	{
		UNiagaraNodeParameterMapGet* GetNode = Cast<UNiagaraNodeParameterMapGet>(Node);
	
		if (GetNode)
		{
			TArray<UEdGraphPin*> OutputPins;
			GetNode->GetOutputPins(OutputPins);
			for (UEdGraphPin* OutputPin : OutputPins)
			{
				if (VariableName == OutputPin->PinName)
				{
					UEdGraphPin* Pin = GetNode->GetDefaultPin(OutputPin);
					if (Pin)
					{
						DefaultInputPin = Pin;
						break;
					}
				}
			}
		}

		if (DefaultInputPin != nullptr)
		{
			break;
		}
	}


	// There are some pins 
	if (DefaultInputPin && DefaultInputPin->LinkedTo.Num() != 0 && DefaultInputPin->LinkedTo[0] != nullptr)
	{
		UNiagaraNode* Owner = Cast<UNiagaraNode>(DefaultInputPin->LinkedTo[0]->GetOwningNode());
		UEdGraphPin* PreviousInput = DefaultInputPin;
		int32 NumIters = 0;
		while (Owner)
		{
			// Check to see if there are any reroute or choose by usage nodes involved in this..
			UEdGraphPin* InputPin = Owner->GetPassThroughPin(PreviousInput->LinkedTo[0], InParentUsage);
			if (InputPin == nullptr)
			{
				return PreviousInput;
			}
			else if (InputPin->LinkedTo.Num() == 0)
			{
				return InputPin;
			}

			check(InputPin->LinkedTo[0] != nullptr);
			Owner = Cast<UNiagaraNode>(InputPin->LinkedTo[0]->GetOwningNode());
			PreviousInput = InputPin;
			++NumIters;
			check(NumIters < Nodes.Num()); // If you hit this assert then we have a cycle in our graph somewhere.
		}
	}
	else
	{
		return DefaultInputPin;
	}

	return nullptr;
}

void UNiagaraGraph::FindOutputNodes(TArray<UNiagaraNodeOutput*>& OutputNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node))
		{
			OutputNodes.Add(OutNode);
		}
	}
}


void UNiagaraGraph::FindOutputNodes(ENiagaraScriptUsage TargetUsageType, TArray<UNiagaraNodeOutput*>& OutputNodes) const
{
	TArray<UNiagaraNodeOutput*> NodesFound;
	for (UEdGraphNode* Node : Nodes)
	{
		UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node);
		if (OutNode && OutNode->GetUsage() == TargetUsageType)
		{
			NodesFound.Add(OutNode);
		}
	}

	OutputNodes = NodesFound;
}

void UNiagaraGraph::FindEquivalentOutputNodes(ENiagaraScriptUsage TargetUsageType, TArray<UNiagaraNodeOutput*>& OutputNodes) const
{
	TArray<UNiagaraNodeOutput*> NodesFound;
	for (UEdGraphNode* Node : Nodes)
	{
		UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node);
		if (OutNode && UNiagaraScript::IsEquivalentUsage(OutNode->GetUsage(), TargetUsageType))
		{
			NodesFound.Add(OutNode);
		}
	}

	OutputNodes = NodesFound;
}

UNiagaraNodeOutput* UNiagaraGraph::FindOutputNode(ENiagaraScriptUsage TargetUsageType, FGuid TargetUsageId) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindOutputNode);
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node))
		{
			if (OutNode->GetUsage() == TargetUsageType && OutNode->GetUsageId() == TargetUsageId)
			{
				return OutNode;
			}
		}
	}
	return nullptr;
}

UNiagaraNodeOutput* UNiagaraGraph::FindEquivalentOutputNode(ENiagaraScriptUsage TargetUsageType, FGuid TargetUsageId) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindOutputNode);
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node))
		{
			if (UNiagaraScript::IsEquivalentUsage(OutNode->GetUsage(), TargetUsageType) && OutNode->GetUsageId() == TargetUsageId)
			{
				return OutNode;
			}
		}
	}
	return nullptr;
}


void BuildTraversalHelper(TArray<class UNiagaraNode*>& OutNodesTraversed, UNiagaraNode* CurrentNode)
{
	if (CurrentNode == nullptr)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_BuildTraversalHelper);

	TArray<UEdGraphPin*> Pins = CurrentNode->GetAllPins();
	for (int32 i = 0; i < Pins.Num(); i++)
	{
		if (Pins[i]->Direction == EEdGraphPinDirection::EGPD_Input && Pins[i]->LinkedTo.Num() == 1)
		{
			UNiagaraNode* Node = Cast<UNiagaraNode>(Pins[i]->LinkedTo[0]->GetOwningNode());
			if (OutNodesTraversed.Contains(Node))
			{
				continue;
			}
			BuildTraversalHelper(OutNodesTraversed, Node);
		}
	}

	OutNodesTraversed.Add(CurrentNode);
}

void UNiagaraGraph::BuildTraversal(TArray<class UNiagaraNode*>& OutNodesTraversed, ENiagaraScriptUsage TargetUsage, FGuid TargetUsageId) const
{
	UNiagaraNodeOutput* Output = FindOutputNode(TargetUsage, TargetUsageId);
	if (Output)
	{
		BuildTraversalHelper(OutNodesTraversed, Output);
	}
}

void UNiagaraGraph::BuildTraversal(TArray<class UNiagaraNode*>& OutNodesTraversed, UNiagaraNode* FinalNode) 
{
	if (FinalNode)
	{
		BuildTraversalHelper(OutNodesTraversed, FinalNode);
	}
}


void UNiagaraGraph::FindInputNodes(TArray<UNiagaraNodeInput*>& OutInputNodes, UNiagaraGraph::FFindInputNodeOptions Options) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindInputNodes);
	TArray<UNiagaraNodeInput*> InputNodes;

	if (!Options.bFilterByScriptUsage)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindInputNodes_NotFilterUsage);

		for (UEdGraphNode* Node : Nodes)
		{
			UNiagaraNodeInput* NiagaraInputNode = Cast<UNiagaraNodeInput>(Node);
			if (NiagaraInputNode != nullptr &&
				((NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Parameter && Options.bIncludeParameters) ||
				(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Attribute && Options.bIncludeAttributes) ||
				(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::SystemConstant && Options.bIncludeSystemConstants) || 
				(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::TranslatorConstant && Options.bIncludeTranslatorConstants)))
			{
				InputNodes.Add(NiagaraInputNode);
			}
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindInputNodes_FilterUsage);

		TArray<class UNiagaraNode*> Traversal;
		BuildTraversal(Traversal, Options.TargetScriptUsage, Options.TargetScriptUsageId);
		for (UNiagaraNode* Node : Traversal)
		{
			UNiagaraNodeInput* NiagaraInputNode = Cast<UNiagaraNodeInput>(Node);
			if (NiagaraInputNode != nullptr &&
				((NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Parameter && Options.bIncludeParameters) ||
					(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Attribute && Options.bIncludeAttributes) ||
					(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::SystemConstant && Options.bIncludeSystemConstants)))
			{
				InputNodes.Add(NiagaraInputNode);
			}
		}
	}

	if (Options.bFilterDuplicates)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindInputNodes_FilterDupes);

		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			auto NodeMatches = [=](UNiagaraNodeInput* UniqueInputNode)
			{
				if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
				{
					return UniqueInputNode->Input.IsEquivalent(InputNode->Input, false);
				}
				else
				{
					return UniqueInputNode->Input.IsEquivalent(InputNode->Input);
				}
			};

			if (OutInputNodes.ContainsByPredicate(NodeMatches) == false)
			{
				OutInputNodes.Add(InputNode);
			}
		}
	}
	else
	{
		OutInputNodes.Append(InputNodes);
	}

	if (Options.bSort)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Graph_FindInputNodes_Sort);

		UNiagaraNodeInput::SortNodes(OutInputNodes);
	}
}

void UNiagaraGraph::GetParameters(TArray<FNiagaraVariable>& Inputs, TArray<FNiagaraVariable>& Outputs)const
{
	Inputs.Empty();
	Outputs.Empty();

	TArray<UNiagaraNodeInput*> InputsNodes;
	FFindInputNodeOptions Options;
	Options.bSort = true;
	FindInputNodes(InputsNodes, Options);
	for (UNiagaraNodeInput* Input : InputsNodes)
	{
		Inputs.Add(Input->Input);
	}

	TArray<UNiagaraNodeOutput*> OutputNodes;
	FindOutputNodes(OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		for (FNiagaraVariable& Var : OutputNode->Outputs)
		{
			Outputs.AddUnique(Var);
		}
	}

	//Do we need to sort outputs?
	//Should leave them as they're defined in the output node?
// 	auto SortVars = [](const FNiagaraVariable& A, const FNiagaraVariable& B)
// 	{
// 		//Case insensitive lexicographical comparisons of names for sorting.
// 		return A.GetName().ToString() < B.GetName().ToString();
// 	};
// 	Outputs.Sort(SortVars);
}

const TMap<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection>& UNiagaraGraph::GetParameterMap()
{
	return Parameters;
}

void UNiagaraGraph::AddParameter(const FNiagaraVariable& Parameter)
{
	FNiagaraGraphParameterReferenceCollection* FoundParameterReferenceCollection = Parameters.Find(Parameter);
	if (!FoundParameterReferenceCollection)
	{
		FNiagaraGraphParameterReferenceCollection NewReferenceCollection = FNiagaraGraphParameterReferenceCollection(true /*bCreated*/);
		NewReferenceCollection.Graph = this;
		Parameters.Add(Parameter, NewReferenceCollection);
	}
}

void UNiagaraGraph::RemoveParameter(const FNiagaraVariable& Parameter, const bool bNotifyGraphChanged /*= true*/)
{
	FNiagaraGraphParameterReferenceCollection* ReferenceCollection = Parameters.Find(Parameter);
	if (ReferenceCollection)
	{
		// Prevent finding all parameters and metadata when renaming each pin.
		SetFindParametersAllowed(false);

		for (int32 Index = 0; Index < ReferenceCollection->ParameterReferences.Num(); Index++)
		{
			const FNiagaraGraphParameterReference& Reference = ReferenceCollection->ParameterReferences[Index];
			UNiagaraNode* Node = Reference.Value.Get();
			if (Node && Node->GetGraph() == this)
			{
				UEdGraphPin* Pin = Node->GetPinByPersistentGuid(Reference.Key);
				if (Pin)
				{
					Node->RemovePin(Pin);
				}
			}
		}

		Parameters.Remove(Parameter);

		SetFindParametersAllowed(true);

		if (bNotifyGraphChanged)
		{
			NotifyGraphChanged();
		}
	}
}

bool UNiagaraGraph::RenameParameter(const FNiagaraVariable& Parameter, FName NewName, const bool bInNotifyGraphChanged /*= true*/)
{
	// Block rename when already renaming. This prevents recursion when CommitEditablePinName is called on referenced nodes. 
	if (bIsRenamingParameter)
	{
		return false;
	}
	bIsRenamingParameter = true;

	// Prevent finding all parameters and metadata when renaming each pin.
	SetFindParametersAllowed(false);
	
	// Create the new parameter
	FNiagaraVariable NewParameter = Parameter;
	NewParameter.SetName(NewName);

	FNiagaraGraphParameterReferenceCollection* ReferenceCollection = Parameters.Find(Parameter);
	if (ReferenceCollection)
	{
		const FText NewNameText = FText::FromName(NewName);
		FNiagaraGraphParameterReferenceCollection NewReferences = *ReferenceCollection;
		for (FNiagaraGraphParameterReference& Reference : NewReferences.ParameterReferences)
		{
			UNiagaraNode* Node = Reference.Value.Get();
			if (Node && Node->GetGraph() == this)
			{
				UEdGraphPin* Pin = Node->GetPinByPersistentGuid(Reference.Key);
				if (Pin)
				{
					Node->CommitEditablePinName(NewNameText, Pin);
				}
			}
		}

		Parameters.Remove(Parameter);
		Parameters.Add(NewParameter, NewReferences);
	}

	// Swap metadata to the new parameter
	FNiagaraVariableMetaData* Metadata = GetMetaData(Parameter);
	if (Metadata)
	{
		FNiagaraVariableMetaData MetadataCopy = *Metadata;
		VariableToMetaData.Remove(Parameter);
		VariableToMetaData.Add(NewParameter, MetadataCopy);
	}

	SetFindParametersAllowed(true);
	bIsRenamingParameter = false;

	if (bInNotifyGraphChanged)
	{
		NotifyGraphChanged();
	}
	return true;
}

int32 UNiagaraGraph::GetOutputNodeVariableIndex(const FNiagaraVariable& Variable)const
{
	TArray<FNiagaraVariable> Variables;
	GetOutputNodeVariables(Variables);
	return Variables.Find(Variable);
}

void UNiagaraGraph::GetOutputNodeVariables(TArray< FNiagaraVariable >& OutVariables)const
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	FindOutputNodes(OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		for (FNiagaraVariable& Var : OutputNode->Outputs)
		{
			OutVariables.AddUnique(Var);
		}
	}
}

void UNiagaraGraph::GetOutputNodeVariables(ENiagaraScriptUsage InScriptUsage, TArray< FNiagaraVariable >& OutVariables)const
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	FindOutputNodes(InScriptUsage, OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		for (FNiagaraVariable& Var : OutputNode->Outputs)
		{
			OutVariables.AddUnique(Var);
		}
	}
}

bool UNiagaraGraph::HasParameterMapParameters()const
{
	TArray<FNiagaraVariable> Inputs;
	TArray<FNiagaraVariable> Outputs;

	GetParameters(Inputs, Outputs);

	for (FNiagaraVariable& Var : Inputs)
	{
		if (Var.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			return true;
		}
	}
	for (FNiagaraVariable& Var : Outputs)
	{
		if (Var.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			return true;
		}
	}

	return false;
}

bool UNiagaraGraph::HasNumericParameters()const
{
	TArray<FNiagaraVariable> Inputs;
	TArray<FNiagaraVariable> Outputs;
	
	GetParameters(Inputs, Outputs);
	
	for (FNiagaraVariable& Var : Inputs)
	{
		if (Var.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			return true;
		}
	}
	for (FNiagaraVariable& Var : Outputs)
	{
		if (Var.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			return true;
		}
	}

	return false;
}

void UNiagaraGraph::NotifyGraphNeedsRecompile()
{
	FEdGraphEditAction Action;
	Action.Action = (EEdGraphActionType)GRAPHACTION_GenericNeedsRecompile;
	NotifyGraphChanged(Action);
}


void UNiagaraGraph::NotifyGraphDataInterfaceChanged()
{
	OnDataInterfaceChangedDelegate.Broadcast();
}

void UNiagaraGraph::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	TArray<UNiagaraNode*> NiagaraNodes;
	GetNodesOfClass<UNiagaraNode>(NiagaraNodes);
	for (UNiagaraNode* NiagaraNode : NiagaraNodes)
	{
		NiagaraNode->SubsumeExternalDependencies(ExistingConversions);
	}
}

void UNiagaraGraph::RebuildCachedData(bool bForce)
{
	// If the graph hasn't changed since last rebuild, then do nothing.
	if (!bForce && ChangeId == LastBuiltTraversalDataChangeId && LastBuiltTraversalDataChangeId.IsValid())
	{
		return;
	}

	// First find all the output nodes
	TArray<UNiagaraNodeOutput*> NiagaraOutputNodes;
	GetNodesOfClass<UNiagaraNodeOutput>(NiagaraOutputNodes);

	// Now build the new cache..
	TArray<FNiagaraGraphScriptUsageInfo> NewUsageCache;
	NewUsageCache.AddDefaulted(NiagaraOutputNodes.Num());

	UEnum* FoundEnum = nullptr;
	bool bNeedsAnyNewCompileIds = false;

	for (int32 i = 0; i < NiagaraOutputNodes.Num(); i++)
	{
		UNiagaraNodeOutput* OutputNode = NiagaraOutputNodes[i];
		NewUsageCache[i].UsageType = OutputNode->GetUsage();
		NewUsageCache[i].UsageId = OutputNode->GetUsageId();

		BuildTraversal(NewUsageCache[i].Traversal, OutputNode);

		int32 FoundMatchIdx = INDEX_NONE;
		for (int32 j = 0; j < CachedUsageInfo.Num(); j++)
		{
			if (UNiagaraScript::IsEquivalentUsage(CachedUsageInfo[j].UsageType, NewUsageCache[i].UsageType) && CachedUsageInfo[j].UsageId == NewUsageCache[i].UsageId)
			{
				FoundMatchIdx = j;
				break;
			}
		}

		// Now compare the change id's of all the nodes in the traversal by hashing them up and comparing the hash
		// now with the hash from previous runs.
		FSHA1 HashState;
		for (UNiagaraNode* Node : NewUsageCache[i].Traversal)
		{
			FGuid Guid = Node->GetChangeId();
			HashState.Update((const uint8*)&Guid, sizeof(FGuid));
		}
		HashState.Final();

		// We can't store in a FShaHash struct directly because you can't UProperty it. Using a standin of the same size.
		check(sizeof(uint8)*NewUsageCache[i].DataHash.Num() == sizeof(FSHAHash));
		HashState.GetHash(&NewUsageCache[i].DataHash[0]);

		bool bNeedsNewCompileId = true;

		// Now compare the hashed data. If it is the same as before, then leave the compile ID as-is. If it is different, generate a new guid.
		if (FoundMatchIdx != INDEX_NONE)
		{
			if (NewUsageCache[i].DataHash == CachedUsageInfo[FoundMatchIdx].DataHash)
			{
				NewUsageCache[i].GeneratedCompileId = CachedUsageInfo[FoundMatchIdx].GeneratedCompileId;
				bNeedsNewCompileId = false;
			}
		}

		if (bNeedsNewCompileId)
		{
			NewUsageCache[i].GeneratedCompileId = FGuid::NewGuid();
			bNeedsAnyNewCompileIds = true;
		}

		// TODO sckime debug logic... should be disabled or put under a cvar in the future
		{

			if (FoundEnum == nullptr)
			{
				FoundEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"), true);
			}

			FString ResultsEnum = TEXT("??");
			if (FoundEnum)
			{
				ResultsEnum = FoundEnum->GetNameStringByValue((int64)NewUsageCache[i].UsageType);
			}

			if (bNeedsNewCompileId)
			{
				//UE_LOG(LogNiagaraEditor, Log, TEXT("'%s' changes detected in %s .. new guid: %s"), *GetFullName(), *ResultsEnum, *NewUsageCache[i].GeneratedCompileId.ToString());
			}
			else
			{
				//UE_LOG(LogNiagaraEditor, Log, TEXT("'%s' changes NOT detected in %s .. keeping guid: %s"), *GetFullName(), *ResultsEnum, *NewUsageCache[i].GeneratedCompileId.ToString());
			}
		}
	}

	// Debug logic, usually disabled at top of file.
	if (bNeedsAnyNewCompileIds && bWriteToLog)
	{
		TMap<FGuid, FGuid> ComputeChangeIds;
		FNiagaraEditorUtilities::GatherChangeIds(*this, ComputeChangeIds, GetName());
	}

	// Now update the cache with the newly computed results.
	CachedUsageInfo = NewUsageCache;
	LastBuiltTraversalDataChangeId = ChangeId;

}

void UNiagaraGraph::SynchronizeInternalCacheWithGraph(UNiagaraGraph* Other)
{
	// Force us to rebuild the cache, note that this builds traversals and everything else, keeping it in sync if nothing changed from the current version.
	RebuildCachedData(true);
	
	UEnum* FoundEnum = nullptr;

	// Now go through all of the other graph's usage info. If we find a match for its usage and our data hashes match, use the generated compile id from
	// the other graph.
	for (int32 i = 0; i < CachedUsageInfo.Num(); i++)
	{
		int32 FoundMatchIdx = INDEX_NONE;
		for (int32 j = 0; j < Other->CachedUsageInfo.Num(); j++)
		{
			if (UNiagaraScript::IsEquivalentUsage(Other->CachedUsageInfo[j].UsageType, CachedUsageInfo[i].UsageType) && Other->CachedUsageInfo[j].UsageId == CachedUsageInfo[i].UsageId)
			{
				FoundMatchIdx = j;
				break;
			}
		}

		if (FoundMatchIdx != INDEX_NONE)
		{
			if (CachedUsageInfo[i].DataHash == Other->CachedUsageInfo[FoundMatchIdx].DataHash)
			{
				CachedUsageInfo[i].GeneratedCompileId = Other->CachedUsageInfo[FoundMatchIdx].GeneratedCompileId;		

				// TODO sckime debug logic... should be disabled or put under a cvar in the future
				{
					if (FoundEnum == nullptr)
					{
						FoundEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"), true);
					}

					FString ResultsEnum = TEXT("??");
					if (FoundEnum)
					{
						ResultsEnum = FoundEnum->GetNameStringByValue((int64)CachedUsageInfo[i].UsageType);
					}
					UE_LOG(LogNiagaraEditor, Log, TEXT("'%s' changes synchronized with master script in %s .. synced guid: %s"), *GetFullName(), *ResultsEnum, *CachedUsageInfo[i].GeneratedCompileId.ToString());
				}
			}
		}
	}

	if (bWriteToLog)
	{
		TMap<FGuid, FGuid> ComputeChangeIds;
		FNiagaraEditorUtilities::GatherChangeIds(*this, ComputeChangeIds, GetName() + TEXT(".Synced"));
	}
}


void UNiagaraGraph::InvalidateCachedCompileIds()
{
	Modify();
	CachedUsageInfo.Empty();
	MarkGraphRequiresSynchronization(__FUNCTION__);
}

void UNiagaraGraph::GatherExternalDependencyIDs(ENiagaraScriptUsage InUsage, const FGuid& InUsageId, TArray<FGuid>& InReferencedIDs, TArray<UObject*>& InReferencedObjs)
{
	RebuildCachedData();
	
	// Particle compute scripts get all particle scripts baked into their dependency chain. 
	if (InUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		for (int32 i = 0; i < CachedUsageInfo.Num(); i++)
		{
			// Add all chains that we depend on.
			if (UNiagaraScript::IsUsageDependentOn(InUsage, CachedUsageInfo[i].UsageType)) 
			{
				// Skip adding to list because we already did it in GetCompileId above if spawn script.
				if (CachedUsageInfo[i].UsageType != ENiagaraScriptUsage::ParticleSpawnScript)
				{
					InReferencedIDs.Add(CachedUsageInfo[i].GeneratedCompileId);
					InReferencedObjs.Add(CachedUsageInfo[i].Traversal.Last());
				}

				for (UNiagaraNode* Node : CachedUsageInfo[i].Traversal)
				{
					Node->GatherExternalDependencyIDs(InUsage, InUsageId, InReferencedIDs, InReferencedObjs);
				}
			}
		}
	}
	// Otherwise, just add downstream dependencies for the specific usage type we're on.
	else
	{
		for (int32 i = 0; i < CachedUsageInfo.Num(); i++)
		{
			// First add our direct dependency chain...
			if (UNiagaraScript::IsEquivalentUsage(CachedUsageInfo[i].UsageType, InUsage) && CachedUsageInfo[i].UsageId == InUsageId)
			{
				// Skip adding to list because we already did it in GetCompileId above.
				for (UNiagaraNode* Node : CachedUsageInfo[i].Traversal)
				{
					Node->GatherExternalDependencyIDs(InUsage, InUsageId, InReferencedIDs, InReferencedObjs);
				}
			}
			// Now add any other dependency chains that we might have...
			else if (UNiagaraScript::IsUsageDependentOn(InUsage, CachedUsageInfo[i].UsageType))
			{
				InReferencedIDs.Add(CachedUsageInfo[i].GeneratedCompileId);
				InReferencedObjs.Add(CachedUsageInfo[i].Traversal.Last());

				for (UNiagaraNode* Node : CachedUsageInfo[i].Traversal)
				{
					Node->GatherExternalDependencyIDs(InUsage, InUsageId, InReferencedIDs, InReferencedObjs);
				}
			}
		}
	}
}


void UNiagaraGraph::GetAllReferencedGraphs(TArray<const UNiagaraGraph*>& Graphs) const
{
	Graphs.AddUnique(this);
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNode* InNode = Cast<UNiagaraNode>(Node))
		{
			UObject* AssetRef = InNode->GetReferencedAsset();
			if (AssetRef != nullptr && AssetRef->IsA(UNiagaraScript::StaticClass()))
			{
				if (UNiagaraScript* FunctionScript = Cast<UNiagaraScript>(AssetRef))
				{
					if (FunctionScript->GetSource())
					{
						UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
						if (Source != nullptr)
						{
							UNiagaraGraph* FunctionGraph = CastChecked<UNiagaraGraph>(Source->NodeGraph);
							if (FunctionGraph != nullptr)
							{
								if (!Graphs.Contains(FunctionGraph))
								{
									FunctionGraph->GetAllReferencedGraphs(Graphs);
								}
							}
						}
					}
				}
				else if (UNiagaraGraph* FunctionGraph = Cast<UNiagaraGraph>(AssetRef))
				{
					if (!Graphs.Contains(FunctionGraph))
					{
						FunctionGraph->GetAllReferencedGraphs(Graphs);
					}
				}
			}
		}
	}
}

/** Determine if another item has been synchronized with this graph.*/
bool UNiagaraGraph::IsOtherSynchronized(const FGuid& InChangeId) const
{
	if (ChangeId.IsValid() && ChangeId == InChangeId)
	{
		return true;
	}
	return false;
}

/** Identify that this graph has undergone changes that will require synchronization with a compiled script.*/
void UNiagaraGraph::MarkGraphRequiresSynchronization(FString Reason)
{
	Modify();
	ChangeId = FGuid::NewGuid();
	//UE_LOG(LogNiagaraEditor, Verbose, TEXT("Graph %s was marked requires synchronization.  Reason: %s"), *GetPathName(), *Reason);
}

/** Get the meta-data associated with this variable, if it exists.*/
FNiagaraVariableMetaData* UNiagaraGraph::GetMetaData(const FNiagaraVariable& InVar)
{
	return VariableToMetaData.Find(InVar);
}

const FNiagaraVariableMetaData* UNiagaraGraph::GetMetaData(const FNiagaraVariable& InVar) const
{
	return VariableToMetaData.Find(InVar);
}

/** Return the meta-data associated with this variable. This should only be called on variables defined within this Graph, otherwise meta-data may leak.*/
FNiagaraVariableMetaData& UNiagaraGraph::FindOrAddMetaData(const FNiagaraVariable& InVar)
{
	FNiagaraVariableMetaData* FoundMetaData = VariableToMetaData.Find(InVar);
	if (FoundMetaData)
	{
		return *FoundMetaData;
	}
	// We shouldn't add constants to the graph's meta-data list. Those are stored globally.
	ensure(FNiagaraConstants::IsNiagaraConstant(InVar) == false);
	return VariableToMetaData.Add(InVar);
}

void UNiagaraGraph::PurgeUnreferencedMetaData()
{
	TArray<FNiagaraVariable> VarsToRemove;
	for (auto It = VariableToMetaData.CreateConstIterator(); It; ++It)
	{
		int32 NumValid = 0;
		for (TWeakObjectPtr<UObject> WeakPtr : It.Value().ReferencerNodes)
		{
			if (WeakPtr.IsValid())
			{
				NumValid++;
			}
		}

		if (NumValid == 0)
		{
			VarsToRemove.Add(It.Key());
		}
	}

	for (FNiagaraVariable& Var : VarsToRemove)
	{
		VariableToMetaData.Remove(Var);
	}
}

void UNiagaraGraph::PurgeUnreferencedParameters()
{
	TArray<FNiagaraVariable> VarsToRemove;
	for (auto& ParameterEntry : Parameters)
	{
		if (!ParameterEntry.Value.WasCreated() && ParameterEntry.Value.ParameterReferences.Num() == 0)
		{
			VarsToRemove.Add(ParameterEntry.Key);
		}
	}

	for (FNiagaraVariable& Var : VarsToRemove)
	{
		Parameters.Remove(Var);
	}
}

UNiagaraGraph::FOnDataInterfaceChanged& UNiagaraGraph::OnDataInterfaceChanged()
{
	return OnDataInterfaceChangedDelegate;
}

void UNiagaraGraph::FindParameters()
{
	if (!bFindParametersAllowed)
	{
		return;
	}

	for (auto& ParameterEntry : Parameters)
	{
		ParameterEntry.Value.ParameterReferences.Empty();
	}

	for (auto& MetadataEntry : VariableToMetaData)
	{
		MetadataEntry.Value.ReferencerNodes.Empty();
	}

	auto AddParameterReference = [&](const FNiagaraVariable& Parameter, const UEdGraphPin* Pin, FNiagaraGraphParameterReferenceCollection*& ReferenceCollection)
	{
		if (Pin->PinType.PinSubCategory == UNiagaraNodeParameterMapBase::ParameterPinSubCategory)
		{
			const FNiagaraGraphParameterReference Reference(Pin->PersistentGuid, Cast<UNiagaraNode>(Pin->GetOwningNode()));
			bool bNewReference = true;
			if (ReferenceCollection)
			{
				ReferenceCollection->ParameterReferences.AddUnique(Reference);
				bNewReference = false;
			}
			else
			{
				FNiagaraGraphParameterReferenceCollection* FoundReferenceCollection = Parameters.Find(Parameter);
				if (FoundReferenceCollection)
				{
					ReferenceCollection = FoundReferenceCollection;
					FoundReferenceCollection->ParameterReferences.AddUnique(Reference);
					bNewReference = false;
				}
			}

			if (bNewReference)
			{
				FNiagaraGraphParameterReferenceCollection NewReferenceCollection;
				NewReferenceCollection.ParameterReferences.Add(Reference);
				NewReferenceCollection.Graph = this;
				Parameters.Add(Parameter, NewReferenceCollection);
			}
		}
	};

	const TArray<FNiagaraParameterMapHistory> Histories = UNiagaraNodeParameterMapBase::GetParameterMaps(this);
	for (const FNiagaraParameterMapHistory& History : Histories)
	{
		for (int32 Index = 0; Index < History.VariablesWithOriginalAliasesIntact.Num(); Index++)
		{
			const FNiagaraVariable& Parameter = History.VariablesWithOriginalAliasesIntact[Index];

			FNiagaraGraphParameterReferenceCollection* FoundReferences = nullptr;
			for (const UEdGraphPin* WritePin : History.PerVariableWriteHistory[Index])
			{
				AddParameterReference(Parameter, WritePin, FoundReferences);
			}

			for (const TTuple<const UEdGraphPin*, const UEdGraphPin*>& ReadPinTuple : History.PerVariableReadHistory[Index])
			{
				AddParameterReference(Parameter, ReadPinTuple.Key, FoundReferences);
			}
		}
	}

	// Find all the parameters in the graph that have no connection and won't be picked up by the parameter map history.
	const UEdGraphSchema_Niagara* NiagaraSchema = Cast<UEdGraphSchema_Niagara>(Schema);
	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->PinType.PinSubCategory == UNiagaraNodeParameterMapBase::ParameterPinSubCategory)
			{
				const FNiagaraVariable Parameter = NiagaraSchema->PinToNiagaraVariable(Pin, false);
				const FNiagaraParameterHandle Handle = FNiagaraParameterHandle(Parameter.GetName());

				if (Handle.IsModuleHandle() && !FNiagaraConstants::IsNiagaraConstant(Parameter))
				{
					FNiagaraVariableMetaData* MetaData = VariableToMetaData.Find(Parameter);
					if (MetaData)
					{
						MetaData->ReferencerNodes.AddUnique(Node);
					}
					else
					{
						FNiagaraVariableMetaData NewVariableMetadata;
						NewVariableMetadata.ReferencerNodes.Add(Node);
						VariableToMetaData.Add(Parameter, NewVariableMetadata);
					}
				}
			
				const FNiagaraGraphParameterReference Reference(Pin->PersistentGuid, Cast<UNiagaraNode>(Pin->GetOwningNode()));
				FNiagaraGraphParameterReferenceCollection* FoundParameterReferenceCollection = Parameters.Find(Parameter);
				if (FoundParameterReferenceCollection)
				{
					FoundParameterReferenceCollection->ParameterReferences.AddUnique(Reference);
				}
				else
				{
					FNiagaraGraphParameterReferenceCollection NewReferenceCollection;
					NewReferenceCollection.ParameterReferences.Add(Reference);
					NewReferenceCollection.Graph = this;
					Parameters.Add(Parameter, NewReferenceCollection);
				}
			}
		}
	}

	// Clean up all parameters and metadata that do not have a reference
	PurgeUnreferencedParameters();
	PurgeUnreferencedMetaData();
}


void UNiagaraGraph::SetFindParametersAllowed(const bool bAllowed)
{
	bFindParametersAllowed = bAllowed;
}

#undef LOCTEXT_NAMESPACE
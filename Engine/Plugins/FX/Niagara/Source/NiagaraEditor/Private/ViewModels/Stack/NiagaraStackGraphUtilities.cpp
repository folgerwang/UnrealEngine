// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraParameterMapHistory.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraScriptSource.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "ViewModels/Stack/NiagaraStackInputCategory.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "NiagaraSystem.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraConstants.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "AssetRegistryModule.h"

DECLARE_CYCLE_STAT(TEXT("Niagara - StackGraphUtilities - RelayoutGraph"), STAT_NiagaraEditor_StackGraphUtilities_RelayoutGraph, STATGROUP_NiagaraEditor);

#define LOCTEXT_NAMESPACE "NiagaraStackGraphUtilities"

void FNiagaraStackGraphUtilities::RelayoutGraph(UEdGraph& Graph)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_StackGraphUtilities_RelayoutGraph);
	TArray<TArray<TArray<UEdGraphNode*>>> OutputNodeTraversalStacks;
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph.GetNodesOfClass(OutputNodes);
	TSet<UEdGraphNode*> AllTraversedNodes;
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		TSet<UEdGraphNode*> TraversedNodes;
		TArray<TArray<UEdGraphNode*>> TraversalStack;
		TArray<UEdGraphNode*> CurrentNodesToTraverse;
		CurrentNodesToTraverse.Add(OutputNode);
		while (CurrentNodesToTraverse.Num() > 0)
		{
			TArray<UEdGraphNode*> TraversedNodesThisLevel;
			TArray<UEdGraphNode*> NextNodesToTraverse;
			for (UEdGraphNode* CurrentNodeToTraverse : CurrentNodesToTraverse)
			{
				if (TraversedNodes.Contains(CurrentNodeToTraverse))
				{
					continue;
				}
				
				for (UEdGraphPin* Pin : CurrentNodeToTraverse->GetAllPins())
				{
					if (Pin->Direction == EGPD_Input)
					{
						for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
						{
							if (LinkedPin->GetOwningNode() != nullptr)
							{
								NextNodesToTraverse.Add(LinkedPin->GetOwningNode());
							}
						}
					}
				}
				TraversedNodes.Add(CurrentNodeToTraverse);
				TraversedNodesThisLevel.Add(CurrentNodeToTraverse);
			}
			TraversalStack.Add(TraversedNodesThisLevel);
			CurrentNodesToTraverse.Empty();
			CurrentNodesToTraverse.Append(NextNodesToTraverse);
		}
		OutputNodeTraversalStacks.Add(TraversalStack);
		AllTraversedNodes = AllTraversedNodes.Union(TraversedNodes);
	}

	// Find all nodes which were not traversed and put them them in a separate traversal stack.
	TArray<UEdGraphNode*> UntraversedNodes;
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (AllTraversedNodes.Contains(Node) == false)
		{
			UntraversedNodes.Add(Node);
		}
	}
	TArray<TArray<UEdGraphNode*>> UntraversedNodeStack;
	for (UEdGraphNode* UntraversedNode : UntraversedNodes)
	{
		TArray<UEdGraphNode*> UntraversedStackItem;
		UntraversedStackItem.Add(UntraversedNode);
		UntraversedNodeStack.Add(UntraversedStackItem);
	}
	OutputNodeTraversalStacks.Add(UntraversedNodeStack);

	// Layout the traversed node stacks.
	float YOffset = 0;
	float XDistance = 400;
	float YDistance = 50;
	float YPinDistance = 50;
	for (const TArray<TArray<UEdGraphNode*>>& TraversalStack : OutputNodeTraversalStacks)
	{
		float CurrentXOffset = 0;
		float MaxYOffset = YOffset;
		for (const TArray<UEdGraphNode*> TraversalLevel : TraversalStack)
		{
			float CurrentYOffset = YOffset;
			for (UEdGraphNode* Node : TraversalLevel)
			{
				Node->Modify();
				Node->NodePosX = CurrentXOffset;
				Node->NodePosY = CurrentYOffset;
				int NumInputPins = 0;
				int NumOutputPins = 0;
				for (UEdGraphPin* Pin : Node->GetAllPins())
				{
					if (Pin->Direction == EGPD_Input)
					{
						NumInputPins++;
					}
					else
					{
						NumOutputPins++;
					}
				}
				int MaxPins = FMath::Max(NumInputPins, NumOutputPins);
				CurrentYOffset += YDistance + (MaxPins * YPinDistance);
			}
			MaxYOffset = FMath::Max(MaxYOffset, CurrentYOffset);
			CurrentXOffset -= XDistance;
		}
		YOffset = MaxYOffset + YDistance;
	}

	Graph.NotifyGraphChanged();
}

void FNiagaraStackGraphUtilities::GetWrittenVariablesForGraph(UEdGraph& Graph, TArray<FNiagaraVariable>& OutWrittenVariables)
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph.GetNodesOfClass<UNiagaraNodeOutput>(OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		TArray<UEdGraphPin*> InputPins;
		OutputNode->GetInputPins(InputPins);
		if (InputPins.Num() == 1)
		{
			FNiagaraParameterMapHistoryBuilder Builder;
			Builder.BuildParameterMaps(OutputNode, true);
			check(Builder.Histories.Num() == 1);
			for (int32 i = 0; i < Builder.Histories[0].Variables.Num(); i++)
			{
				if (Builder.Histories[0].PerVariableWriteHistory[i].Num() > 0)
				{
					OutWrittenVariables.Add(Builder.Histories[0].Variables[i]);
				}
			}
		}
	}
}

void FNiagaraStackGraphUtilities::ConnectPinToInputNode(UEdGraphPin& Pin, UNiagaraNodeInput& InputNode)
{
	TArray<UEdGraphPin*> InputPins;
	InputNode.GetOutputPins(InputPins);
	if (InputPins.Num() == 1)
	{
		Pin.MakeLinkTo(InputPins[0]);
	}
}

UEdGraphPin* GetParameterMapPin(const TArray<UEdGraphPin*>& Pins)
{
	auto IsParameterMapPin = [](const UEdGraphPin* Pin)
	{
		const UEdGraphSchema_Niagara* NiagaraSchema = CastChecked<UEdGraphSchema_Niagara>(Pin->GetSchema());
		FNiagaraTypeDefinition PinDefinition = NiagaraSchema->PinToTypeDefinition(Pin);
		return PinDefinition == FNiagaraTypeDefinition::GetParameterMapDef();
	};

	UEdGraphPin*const* ParameterMapPinPtr = Pins.FindByPredicate(IsParameterMapPin);

	return ParameterMapPinPtr != nullptr ? *ParameterMapPinPtr : nullptr;
}

UEdGraphPin* FNiagaraStackGraphUtilities::GetParameterMapInputPin(UNiagaraNode& Node)
{
	TArray<UEdGraphPin*> InputPins;
	Node.GetInputPins(InputPins);
	return GetParameterMapPin(InputPins);
}

UEdGraphPin* FNiagaraStackGraphUtilities::GetParameterMapOutputPin(UNiagaraNode& Node)
{
	TArray<UEdGraphPin*> OutputPins;
	Node.GetOutputPins(OutputPins);
	return GetParameterMapPin(OutputPins);
}

void FNiagaraStackGraphUtilities::GetOrderedModuleNodes(UNiagaraNodeOutput& OutputNode, TArray<UNiagaraNodeFunctionCall*>& ModuleNodes)
{
	UNiagaraNode* PreviousNode = &OutputNode;
	while (PreviousNode != nullptr)
	{
		UEdGraphPin* PreviousNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*PreviousNode);
		if (PreviousNodeInputPin != nullptr && PreviousNodeInputPin->LinkedTo.Num() == 1)
		{
			UNiagaraNode* CurrentNode = Cast<UNiagaraNode>(PreviousNodeInputPin->LinkedTo[0]->GetOwningNode());
			UNiagaraNodeFunctionCall* ModuleNode = Cast<UNiagaraNodeFunctionCall>(CurrentNode);
			if (ModuleNode != nullptr)
			{
				ModuleNodes.Insert(ModuleNode, 0);
			}
			PreviousNode = CurrentNode;
		}
		else
		{
			PreviousNode = nullptr;
		}
	}
}

UNiagaraNodeFunctionCall* FNiagaraStackGraphUtilities::GetPreviousModuleNode(UNiagaraNodeFunctionCall& CurrentNode)
{
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(CurrentNode);
	if (OutputNode != nullptr)
	{
		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		GetOrderedModuleNodes(*OutputNode, ModuleNodes);

		int32 ModuleIndex;
		ModuleNodes.Find(&CurrentNode, ModuleIndex);
		return ModuleIndex > 0 ? ModuleNodes[ModuleIndex - 1] : nullptr;
	}
	return nullptr;
}

UNiagaraNodeFunctionCall* FNiagaraStackGraphUtilities::GetNextModuleNode(UNiagaraNodeFunctionCall& CurrentNode)
{
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(CurrentNode);
	if (OutputNode != nullptr)
	{
		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		GetOrderedModuleNodes(*OutputNode, ModuleNodes);

		int32 ModuleIndex;
		ModuleNodes.Find(&CurrentNode, ModuleIndex);
		return ModuleIndex < ModuleNodes.Num() - 2 ? ModuleNodes[ModuleIndex + 1] : nullptr;
	}
	return nullptr;
}

UNiagaraNodeOutput* FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(UNiagaraNode& StackNode)
{
	TArray<UNiagaraNode*> NodesToCheck;
	TSet<UNiagaraNode*> NodesChecked;
	NodesToCheck.Add(&StackNode);
	while (NodesToCheck.Num() > 0)
	{
		UNiagaraNode* NodeToCheck = NodesToCheck[0];
		NodesToCheck.RemoveAt(0);
		NodesChecked.Add(NodeToCheck);

		if (NodeToCheck->GetClass() == UNiagaraNodeOutput::StaticClass())
		{
			return CastChecked<UNiagaraNodeOutput>(NodeToCheck);
		}
		
		TArray<UEdGraphPin*> OutputPins;
		NodeToCheck->GetOutputPins(OutputPins);
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
			{
				UNiagaraNode* LinkedNiagaraNode = Cast<UNiagaraNode>(LinkedPin->GetOwningNode());
				if (LinkedNiagaraNode != nullptr && NodesChecked.Contains(LinkedNiagaraNode) == false)
				{
					NodesToCheck.Add(LinkedNiagaraNode);
				}
			}
		}
	}
	return nullptr;
}

UNiagaraNodeInput* FNiagaraStackGraphUtilities::GetEmitterInputNodeForStackNode(UNiagaraNode& StackNode)
{
	// Since the stack graph can have arbitrary branches when traversing inputs, the only safe way to get the initial input
	// is to start at the output node and then trace only parameter map inputs.
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(StackNode);

	UNiagaraNode* PreviousNode = OutputNode;
	while (PreviousNode != nullptr)
	{
		UEdGraphPin* PreviousNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*PreviousNode);
		if (PreviousNodeInputPin != nullptr && PreviousNodeInputPin->LinkedTo.Num() == 1)
		{
			UNiagaraNode* CurrentNode = Cast<UNiagaraNode>(PreviousNodeInputPin->LinkedTo[0]->GetOwningNode());
			UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(CurrentNode);
			if (InputNode != nullptr)
			{
				return InputNode;
			}
			PreviousNode = CurrentNode;
		}
		else
		{
			PreviousNode = nullptr;
		}
	}
	return nullptr;
}

void GetGroupNodesRecursive(const TArray<UNiagaraNode*>& CurrentStartNodes, UNiagaraNode* EndNode, TArray<UNiagaraNode*>& OutAllNodes)
{
	for (UNiagaraNode* CurrentStartNode : CurrentStartNodes)
	{
		if (OutAllNodes.Contains(CurrentStartNode) == false)
		{
			OutAllNodes.Add(CurrentStartNode);
			if (CurrentStartNode != EndNode)
			{
				TArray<UNiagaraNode*> LinkedNodes;
				TArray<UEdGraphPin*> OutputPins;
				CurrentStartNode->GetOutputPins(OutputPins);
				for (UEdGraphPin* OutputPin : OutputPins)
				{
					for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
					{
						UNiagaraNode* LinkedNode = Cast<UNiagaraNode>(LinkedPin->GetOwningNode());
						if (LinkedNode != nullptr)
						{
							LinkedNodes.Add(LinkedNode);
						}
					}
				}
				GetGroupNodesRecursive(LinkedNodes, EndNode, OutAllNodes);
			}
		}
	}
}

void FNiagaraStackGraphUtilities::FStackNodeGroup::GetAllNodesInGroup(TArray<UNiagaraNode*>& OutAllNodes) const
{
	GetGroupNodesRecursive(StartNodes, EndNode, OutAllNodes);
}

void FNiagaraStackGraphUtilities::GetStackNodeGroups(UNiagaraNode& StackNode, TArray<FNiagaraStackGraphUtilities::FStackNodeGroup>& OutStackNodeGroups)
{
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(StackNode);
	if (OutputNode != nullptr)
	{
		UNiagaraNodeInput* InputNode = GetEmitterInputNodeForStackNode(*OutputNode);
		if (InputNode != nullptr)
		{
			FStackNodeGroup InputGroup;
			InputGroup.StartNodes.Add(InputNode);
			InputGroup.EndNode = InputNode;
			OutStackNodeGroups.Add(InputGroup);

			TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
			GetOrderedModuleNodes(*OutputNode, ModuleNodes);
			for (UNiagaraNodeFunctionCall* ModuleNode : ModuleNodes)
			{
				FStackNodeGroup ModuleGroup;
				UEdGraphPin* PreviousOutputPin = GetParameterMapOutputPin(*OutStackNodeGroups.Last().EndNode);
				for (UEdGraphPin* PreviousOutputPinLinkedPin : PreviousOutputPin->LinkedTo)
				{
					ModuleGroup.StartNodes.Add(CastChecked<UNiagaraNode>(PreviousOutputPinLinkedPin->GetOwningNode()));
				}
				ModuleGroup.EndNode = ModuleNode;
				OutStackNodeGroups.Add(ModuleGroup);
			}

			FStackNodeGroup OutputGroup;
			UEdGraphPin* PreviousOutputPin = GetParameterMapOutputPin(*OutStackNodeGroups.Last().EndNode);
			for (UEdGraphPin* PreviousOutputPinLinkedPin : PreviousOutputPin->LinkedTo)
			{
				OutputGroup.StartNodes.Add(CastChecked<UNiagaraNode>(PreviousOutputPinLinkedPin->GetOwningNode()));
			}
			OutputGroup.EndNode = OutputNode;
			OutStackNodeGroups.Add(OutputGroup);
		}
	}
}

void FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(const FStackNodeGroup& DisconnectGroup, const FStackNodeGroup& PreviousGroup, const FStackNodeGroup& NextGroup)
{
	UEdGraphPin* PreviousOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*PreviousGroup.EndNode);
	PreviousOutputPin->BreakAllPinLinks();

	UEdGraphPin* DisconnectOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*DisconnectGroup.EndNode);
	DisconnectOutputPin->BreakAllPinLinks();

	for (UNiagaraNode* NextStartNode : NextGroup.StartNodes)
	{
		UEdGraphPin* NextStartInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*NextStartNode);
		PreviousOutputPin->MakeLinkTo(NextStartInputPin);
	}
}

void FNiagaraStackGraphUtilities::ConnectStackNodeGroup(const FStackNodeGroup& ConnectGroup, const FStackNodeGroup& NewPreviousGroup, const FStackNodeGroup& NewNextGroup)
{
	UEdGraphPin* NewPreviousOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*NewPreviousGroup.EndNode);
	NewPreviousOutputPin->BreakAllPinLinks();

	for (UNiagaraNode* ConnectStartNode : ConnectGroup.StartNodes)
	{
		UEdGraphPin* ConnectInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*ConnectStartNode);
		NewPreviousOutputPin->MakeLinkTo(ConnectInputPin);
	}

	UEdGraphPin* ConnectOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*ConnectGroup.EndNode);

	for (UNiagaraNode* NewNextStartNode : NewNextGroup.StartNodes)
	{
		UEdGraphPin* NewNextStartInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*NewNextStartNode);
		ConnectOutputPin->MakeLinkTo(NewNextStartInputPin);
	}
}

DECLARE_DELEGATE_RetVal_OneParam(bool, FInputSelector, UNiagaraStackFunctionInput*);

void InitializeStackFunctionInputsInternal(TSharedRef<FNiagaraSystemViewModel> SystemViewModel, TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel, UNiagaraStackEditorData& StackEditorData, UNiagaraNodeFunctionCall& ModuleNode, UNiagaraNodeFunctionCall& InputFunctionCallNode, FInputSelector InputSelector)
{
	UNiagaraStackFunctionInputCollection* FunctionInputCollection = NewObject<UNiagaraStackFunctionInputCollection>(GetTransientPackage()); 
	UNiagaraStackEntry::FRequiredEntryData RequiredEntryData(SystemViewModel, EmitterViewModel, NAME_None, NAME_None, StackEditorData);
	FunctionInputCollection->Initialize(RequiredEntryData, ModuleNode, InputFunctionCallNode, FString());
	FunctionInputCollection->RefreshChildren();

	// Reset all direct inputs on this function to initialize data interfaces and default dynamic inputs.
	TArray<UNiagaraStackEntry*> Children;
	FunctionInputCollection->GetUnfilteredChildren(Children);
	for (UNiagaraStackEntry* Child : Children)
	{
		UNiagaraStackInputCategory* InputCategory = Cast<UNiagaraStackInputCategory>(Child);
		if (InputCategory != nullptr)
		{
			TArray<UNiagaraStackEntry*> CategoryChildren;
			InputCategory->GetUnfilteredChildren(CategoryChildren);
			for (UNiagaraStackEntry* CategoryChild : CategoryChildren)
			{
				UNiagaraStackFunctionInput* FunctionInput = Cast<UNiagaraStackFunctionInput>(CategoryChild);
				if (FunctionInput != nullptr && (InputSelector.IsBound() == false || InputSelector.Execute(FunctionInput)) && FunctionInput->CanReset())
				{
					FunctionInput->Reset();
				}
			}
		}
	}

	FunctionInputCollection->Finalize();
	SystemViewModel->NotifyDataObjectChanged(nullptr);
}

void FNiagaraStackGraphUtilities::InitializeStackFunctionInputs(TSharedRef<FNiagaraSystemViewModel> SystemViewModel, TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel, UNiagaraStackEditorData& StackEditorData, UNiagaraNodeFunctionCall& ModuleNode, UNiagaraNodeFunctionCall& InputFunctionCallNode)
{
	InitializeStackFunctionInputsInternal(SystemViewModel, EmitterViewModel, StackEditorData, ModuleNode, InputFunctionCallNode, FInputSelector());
}

void FNiagaraStackGraphUtilities::InitializeStackFunctionInput(TSharedRef<FNiagaraSystemViewModel> SystemViewModel, TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel, UNiagaraStackEditorData& StackEditorData, UNiagaraNodeFunctionCall& ModuleNode, UNiagaraNodeFunctionCall& InputFunctionCallNode, FName InputName)
{
	FInputSelector InputSelector;
	InputSelector.BindLambda([&InputName](UNiagaraStackFunctionInput* Input)
	{
		return Input->GetInputParameterHandle().GetName() == InputName;
	});
	InitializeStackFunctionInputsInternal(SystemViewModel, EmitterViewModel, StackEditorData, ModuleNode, InputFunctionCallNode, InputSelector);
}

FString FNiagaraStackGraphUtilities::GenerateStackFunctionInputEditorDataKey(UNiagaraNodeFunctionCall& FunctionCallNode, FNiagaraParameterHandle InputParameterHandle)
{
	return FunctionCallNode.GetFunctionName() + InputParameterHandle.GetParameterHandleString().ToString();
}

FString FNiagaraStackGraphUtilities::GenerateStackModuleEditorDataKey(UNiagaraNodeFunctionCall& ModuleNode)
{
	return ModuleNode.GetFunctionName();
}

void FNiagaraStackGraphUtilities::GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TArray<const UEdGraphPin*>& OutInputPins, ENiagaraGetStackFunctionInputPinsOptions Options, bool bIgnoreDisabled)
{
	FNiagaraParameterMapHistoryBuilder Builder;
	Builder.SetIgnoreDisabled(bIgnoreDisabled);
	FunctionCallNode.BuildParameterMapHistory(Builder, false);
	
	if (Builder.Histories.Num() == 1)
	{
		for (int32 i = 0; i < Builder.Histories[0].Variables.Num(); i++)
		{
			FNiagaraVariable& Variable = Builder.Histories[0].Variables[i];
			TArray<TTuple<const UEdGraphPin*, const UEdGraphPin*>>& ReadHistory = Builder.Histories[0].PerVariableReadHistory[i];

			// A read is only really exposed if it's the first read and it has no corresponding write.
			if (ReadHistory.Num() > 0 && ReadHistory[0].Get<1>() == nullptr)
			{
				const UEdGraphPin* InputPin = ReadHistory[0].Get<0>();

				// Make sure that the module input is from the called graph, and not a nested graph.
				if (InputPin->GetOwningNode()->GetGraph() == FunctionCallNode.GetCalledGraph() &&
					(Options == ENiagaraGetStackFunctionInputPinsOptions::AllInputs || FNiagaraParameterHandle(InputPin->PinName).IsModuleHandle()))
				{
					OutInputPins.Add(InputPin);
				}
			}
		}
	}
}

UNiagaraNodeParameterMapSet* FNiagaraStackGraphUtilities::GetStackFunctionOverrideNode(UNiagaraNodeFunctionCall& FunctionCallNode)
{
	UEdGraphPin* ParameterMapInput = FNiagaraStackGraphUtilities::GetParameterMapInputPin(FunctionCallNode);
	if (ParameterMapInput != nullptr && ParameterMapInput->LinkedTo.Num() == 1)
	{
		return Cast<UNiagaraNodeParameterMapSet>(ParameterMapInput->LinkedTo[0]->GetOwningNode());
	}
	return nullptr;
}

UNiagaraNodeParameterMapSet& FNiagaraStackGraphUtilities::GetOrCreateStackFunctionOverrideNode(UNiagaraNodeFunctionCall& StackFunctionCall, const FGuid& PreferredOverrideNodeGuid)
{
	UNiagaraNodeParameterMapSet* OverrideNode = GetStackFunctionOverrideNode(StackFunctionCall);
	if (OverrideNode == nullptr)
	{
		UEdGraph* Graph = StackFunctionCall.GetGraph();
		FGraphNodeCreator<UNiagaraNodeParameterMapSet> ParameterMapSetNodeCreator(*Graph);
		OverrideNode = ParameterMapSetNodeCreator.CreateNode();
		ParameterMapSetNodeCreator.Finalize();
		if (PreferredOverrideNodeGuid.IsValid())
		{
			OverrideNode->NodeGuid = PreferredOverrideNodeGuid;
		}
		OverrideNode->SetEnabledState(StackFunctionCall.GetDesiredEnabledState(), StackFunctionCall.HasUserSetTheEnabledState());

		UEdGraphPin* OverrideNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*OverrideNode);
		UEdGraphPin* OverrideNodeOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*OverrideNode);

		UEdGraphPin* OwningFunctionCallInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(StackFunctionCall);
		UEdGraphPin* PreviousStackNodeOutputPin = OwningFunctionCallInputPin->LinkedTo[0];

		OwningFunctionCallInputPin->BreakAllPinLinks();
		OwningFunctionCallInputPin->MakeLinkTo(OverrideNodeOutputPin);
		for (UEdGraphPin* PreviousStackNodeOutputLinkedPin : PreviousStackNodeOutputPin->LinkedTo)
		{
			PreviousStackNodeOutputLinkedPin->MakeLinkTo(OverrideNodeOutputPin);
		}
		PreviousStackNodeOutputPin->BreakAllPinLinks();
		PreviousStackNodeOutputPin->MakeLinkTo(OverrideNodeInputPin);
	}
	return *OverrideNode;
}

UEdGraphPin* FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin(UNiagaraNodeFunctionCall& StackFunctionCall, FNiagaraParameterHandle AliasedInputParameterHandle)
{
	UNiagaraNodeParameterMapSet* OverrideNode = GetStackFunctionOverrideNode(StackFunctionCall);
	if (OverrideNode != nullptr)
	{
		TArray<UEdGraphPin*> InputPins;
		OverrideNode->GetInputPins(InputPins);
		UEdGraphPin** OverridePinPtr = InputPins.FindByPredicate([&](const UEdGraphPin* Pin) { return Pin->PinName == AliasedInputParameterHandle.GetParameterHandleString(); });
		if (OverridePinPtr != nullptr)
		{
			return *OverridePinPtr;
		}
	}
	return nullptr;
}

UEdGraphPin& FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(UNiagaraNodeFunctionCall& StackFunctionCall, FNiagaraParameterHandle AliasedInputParameterHandle, FNiagaraTypeDefinition InputType, const FGuid& PreferredOverrideNodeGuid)
{
	UEdGraphPin* OverridePin = GetStackFunctionInputOverridePin(StackFunctionCall, AliasedInputParameterHandle);
	if (OverridePin == nullptr)
	{
		UNiagaraNodeParameterMapSet& OverrideNode = GetOrCreateStackFunctionOverrideNode(StackFunctionCall, PreferredOverrideNodeGuid);
		OverrideNode.Modify();

		TArray<UEdGraphPin*> OverrideInputPins;
		OverrideNode.GetInputPins(OverrideInputPins);

		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
		FEdGraphPinType PinType = NiagaraSchema->TypeDefinitionToPinType(InputType);
		OverridePin = OverrideNode.CreatePin(EEdGraphPinDirection::EGPD_Input, PinType, AliasedInputParameterHandle.GetParameterHandleString(), OverrideInputPins.Num() - 1);
	}
	return *OverridePin;
}

void FNiagaraStackGraphUtilities::RemoveNodesForStackFunctionInputOverridePin(UEdGraphPin& StackFunctionInputOverridePin)
{
	TArray<TWeakObjectPtr<UNiagaraDataInterface>> RemovedDataObjects;
	RemoveNodesForStackFunctionInputOverridePin(StackFunctionInputOverridePin, RemovedDataObjects);
}

void FNiagaraStackGraphUtilities::RemoveNodesForStackFunctionInputOverridePin(UEdGraphPin& StackFunctionInputOverridePin, TArray<TWeakObjectPtr<UNiagaraDataInterface>>& OutRemovedDataObjects)
{
	if (StackFunctionInputOverridePin.LinkedTo.Num() == 1)
	{
		UEdGraphNode* OverrideValueNode = StackFunctionInputOverridePin.LinkedTo[0]->GetOwningNode();
		UEdGraph* Graph = OverrideValueNode->GetGraph();
		if (OverrideValueNode->IsA<UNiagaraNodeInput>() || OverrideValueNode->IsA<UNiagaraNodeParameterMapGet>())
		{
			UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(OverrideValueNode);
			if (InputNode != nullptr && InputNode->GetDataInterface() != nullptr)
			{
				OutRemovedDataObjects.Add(InputNode->GetDataInterface());
			}
			Graph->RemoveNode(OverrideValueNode);
		}
		else if (OverrideValueNode->IsA<UNiagaraNodeFunctionCall>())
		{
			UNiagaraNodeFunctionCall* DynamicInputNode = CastChecked<UNiagaraNodeFunctionCall>(OverrideValueNode);
			UEdGraphPin* DynamicInputNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*DynamicInputNode);
			if (DynamicInputNodeInputPin && DynamicInputNodeInputPin->LinkedTo.Num() > 0 && DynamicInputNodeInputPin->LinkedTo[0] != nullptr)
			{
				UNiagaraNodeParameterMapSet* DynamicInputNodeOverrideNode = Cast<UNiagaraNodeParameterMapSet>(DynamicInputNodeInputPin->LinkedTo[0]->GetOwningNode());
				if (DynamicInputNodeOverrideNode != nullptr)
				{
					TArray<UEdGraphPin*> InputPins;
					DynamicInputNodeOverrideNode->GetInputPins(InputPins);
					for (UEdGraphPin* InputPin : InputPins)
					{
						FNiagaraParameterHandle InputHandle(InputPin->PinName);
						if (InputHandle.GetNamespace().ToString() == DynamicInputNode->GetFunctionName())
						{
							RemoveNodesForStackFunctionInputOverridePin(*InputPin, OutRemovedDataObjects);
							DynamicInputNodeOverrideNode->RemovePin(InputPin);
						}
					}

					TArray<UEdGraphPin*> NewInputPins;
					DynamicInputNodeOverrideNode->GetInputPins(NewInputPins);
					if (NewInputPins.Num() == 2)
					{
						// If there are only 2 input pins left, they are the parameter map input and the add pin, so the dynamic input's override node 
						// can be removed.  This not always be the case when removing dynamic input nodes because they share the same override node.
						UEdGraphPin* InputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*DynamicInputNodeOverrideNode);
						UEdGraphPin* OutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*DynamicInputNodeOverrideNode);

						if (ensureMsgf(InputPin != nullptr && InputPin->LinkedTo.Num() == 1 && OutputPin != nullptr &&
							OutputPin->LinkedTo.Num() >= 2, TEXT("Invalid Stack - Dynamic input node override node not connected correctly.")))
						{
							// The DynamicInputOverrideNode will have a single input which is the previous module or override map set, and
							// two or more output links, one to the dynamic input node, one to the next override map set, and 0 or more links
							// to other dynamic inputs on sibling inputs.  Collect these linked pins to reconnect after removing the override node.
							UEdGraphPin* LinkedInputPin = InputPin->LinkedTo[0];
							TArray<UEdGraphPin*> LinkedOutputPins;
							for (UEdGraphPin* LinkedOutputPin : OutputPin->LinkedTo)
							{
								if (LinkedOutputPin->GetOwningNode() != DynamicInputNode)
								{
									LinkedOutputPins.Add(LinkedOutputPin);
								}
							}

							// Disconnect the override node and remove it.
							InputPin->BreakAllPinLinks();
							OutputPin->BreakAllPinLinks();
							Graph->RemoveNode(DynamicInputNodeOverrideNode);

							// Reconnect the pins which were connected to the removed override node.
							for (UEdGraphPin* LinkedOutputPin : LinkedOutputPins)
							{
								LinkedInputPin->MakeLinkTo(LinkedOutputPin);
							}
						}
					}
				}
			}

			Graph->RemoveNode(DynamicInputNode);
		}
	}
}

void FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(UEdGraphPin& OverridePin, FNiagaraParameterHandle LinkedParameterHandle, const FGuid& NewNodePersistentId)
{
	checkf(OverridePin.LinkedTo.Num() == 0, TEXT("Can't set a linked value handle when the override pin already has a value."));

	UNiagaraNodeParameterMapSet* OverrideNode = CastChecked<UNiagaraNodeParameterMapSet>(OverridePin.GetOwningNode());
	UEdGraph* Graph = OverrideNode->GetGraph();
	FGraphNodeCreator<UNiagaraNodeParameterMapGet> GetNodeCreator(*Graph);
	UNiagaraNodeParameterMapGet* GetNode = GetNodeCreator.CreateNode();
	GetNodeCreator.Finalize();
	GetNode->SetEnabledState(OverrideNode->GetDesiredEnabledState(), OverrideNode->HasUserSetTheEnabledState());

	UEdGraphPin* GetInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*GetNode);
	checkf(GetInputPin != nullptr, TEXT("Parameter map get node was missing it's parameter map input pin."));

	UEdGraphPin* OverrideNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*OverrideNode);
	UEdGraphPin* PreviousStackNodeOutputPin = OverrideNodeInputPin->LinkedTo[0];
	checkf(PreviousStackNodeOutputPin != nullptr, TEXT("Invalid Stack Graph - No previous stack node."));

	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition InputType = NiagaraSchema->PinToTypeDefinition(&OverridePin);
	UEdGraphPin* GetOutputPin = GetNode->RequestNewTypedPin(EGPD_Output, InputType, LinkedParameterHandle.GetParameterHandleString());
	GetInputPin->MakeLinkTo(PreviousStackNodeOutputPin);
	GetOutputPin->MakeLinkTo(&OverridePin);

	if (NewNodePersistentId.IsValid())
	{
		GetNode->NodeGuid = NewNodePersistentId;
	}
}

void FNiagaraStackGraphUtilities::SetDataValueObjectForFunctionInput(UEdGraphPin& OverridePin, UClass* DataObjectType, FString DataObjectName, UNiagaraDataInterface*& OutDataObject, const FGuid& NewNodePersistentId)
{
	checkf(OverridePin.LinkedTo.Num() == 0, TEXT("Can't set a data value when the override pin already has a value."));
	checkf(DataObjectType->IsChildOf<UNiagaraDataInterface>(), TEXT("Can only set a function input to a data interface value object"));

	UNiagaraNodeParameterMapSet* OverrideNode = CastChecked<UNiagaraNodeParameterMapSet>(OverridePin.GetOwningNode());
	UEdGraph* Graph = OverrideNode->GetGraph();
	FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*Graph);
	UNiagaraNodeInput* InputNode = InputNodeCreator.CreateNode();
	FNiagaraEditorUtilities::InitializeParameterInputNode(*InputNode, FNiagaraTypeDefinition(DataObjectType), CastChecked<UNiagaraGraph>(Graph), *DataObjectName);

	OutDataObject = NewObject<UNiagaraDataInterface>(InputNode, DataObjectType, *DataObjectName, RF_Transactional | RF_Public);
	InputNode->SetDataInterface(OutDataObject);

	InputNodeCreator.Finalize();
	FNiagaraStackGraphUtilities::ConnectPinToInputNode(OverridePin, *InputNode);

	if (NewNodePersistentId.IsValid())
	{
		InputNode->NodeGuid = NewNodePersistentId;
	}
}

void FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(UEdGraphPin& OverridePin, UNiagaraScript* DynamicInput, UNiagaraNodeFunctionCall*& OutDynamicInputFunctionCall, const FGuid& NewNodePersistentId, FString SuggestedName)
{
	checkf(OverridePin.LinkedTo.Num() == 0, TEXT("Can't set a data value when the override pin already has a value."));

	UNiagaraNodeParameterMapSet* OverrideNode = CastChecked<UNiagaraNodeParameterMapSet>(OverridePin.GetOwningNode());
	UEdGraph* Graph = OverrideNode->GetGraph();
	FGraphNodeCreator<UNiagaraNodeFunctionCall> FunctionCallNodeCreator(*Graph);
	UNiagaraNodeFunctionCall* FunctionCallNode = FunctionCallNodeCreator.CreateNode();
	FunctionCallNode->FunctionScript = DynamicInput;
	FunctionCallNodeCreator.Finalize();
	FunctionCallNode->SetEnabledState(OverrideNode->GetDesiredEnabledState(), OverrideNode->HasUserSetTheEnabledState());

	UEdGraphPin* FunctionCallInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*FunctionCallNode);
	TArray<UEdGraphPin*> FunctionCallOutputPins;
	FunctionCallNode->GetOutputPins(FunctionCallOutputPins);

	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();

	FNiagaraTypeDefinition InputType = NiagaraSchema->PinToTypeDefinition(&OverridePin);


	UEdGraphPin* OverrideNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*OverrideNode);
	UEdGraphPin* PreviousStackNodeOutputPin = nullptr;
	if (OverrideNodeInputPin != nullptr)
	{
		PreviousStackNodeOutputPin = OverrideNodeInputPin->LinkedTo[0];
	}
	
	if (FunctionCallInputPin != nullptr && PreviousStackNodeOutputPin != nullptr)
	{
		FunctionCallInputPin->MakeLinkTo(PreviousStackNodeOutputPin);
	}
	
	if (FunctionCallOutputPins.Num() >= 1 && FunctionCallOutputPins[0] != nullptr)
	{
		FunctionCallOutputPins[0]->MakeLinkTo(&OverridePin);
	}

	OutDynamicInputFunctionCall = FunctionCallNode;

	if (NewNodePersistentId.IsValid())
	{
		FunctionCallNode->NodeGuid = NewNodePersistentId;
	}

	if (SuggestedName.IsEmpty() == false)
	{
		FunctionCallNode->SuggestName(SuggestedName);
	}
}

void FNiagaraStackGraphUtilities::SetCustomExpressionForFunctionInput(UEdGraphPin& OverridePin, UNiagaraNodeCustomHlsl*& OutDynamicInputFunctionCall, const FGuid& NewNodePersistentId)
{
	checkf(OverridePin.LinkedTo.Num() == 0, TEXT("Can't set a data value when the override pin already has a value."));

	UNiagaraNodeParameterMapSet* OverrideNode = CastChecked<UNiagaraNodeParameterMapSet>(OverridePin.GetOwningNode());
	UEdGraph* Graph = OverrideNode->GetGraph();
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(OverrideNode->GetSchema());

	FGraphNodeCreator<UNiagaraNodeCustomHlsl> FunctionCallNodeCreator(*Graph);
	UNiagaraNodeCustomHlsl* FunctionCallNode = FunctionCallNodeCreator.CreateNode();
	FunctionCallNode->InitAsCustomHlslDynamicInput(Schema->PinToTypeDefinition(&OverridePin));
	FunctionCallNodeCreator.Finalize();
	FunctionCallNode->SetEnabledState(OverrideNode->GetDesiredEnabledState(), OverrideNode->HasUserSetTheEnabledState());

	UEdGraphPin* FunctionCallInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*FunctionCallNode);
	TArray<UEdGraphPin*> FunctionCallOutputPins;
	FunctionCallNode->GetOutputPins(FunctionCallOutputPins);

	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();

	FNiagaraTypeDefinition InputType = NiagaraSchema->PinToTypeDefinition(&OverridePin);
	checkf(FunctionCallInputPin != nullptr, TEXT("Dynamic Input function call did not have a parameter map input pin."));
	checkf(FunctionCallOutputPins.Num() == 2 && NiagaraSchema->PinToTypeDefinition(FunctionCallOutputPins[0]) == InputType, TEXT("Invalid Stack Graph - Dynamic Input function did not have the correct typed output pin"));

	UEdGraphPin* OverrideNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*OverrideNode);
	UEdGraphPin* PreviousStackNodeOutputPin = OverrideNodeInputPin->LinkedTo[0];
	checkf(PreviousStackNodeOutputPin != nullptr, TEXT("Invalid Stack Graph - No previous stack node."));

	FunctionCallInputPin->MakeLinkTo(PreviousStackNodeOutputPin);
	FunctionCallOutputPins[0]->MakeLinkTo(&OverridePin);

	OutDynamicInputFunctionCall = FunctionCallNode;

	if (NewNodePersistentId.IsValid())
	{
		FunctionCallNode->NodeGuid = NewNodePersistentId;
	}
}

bool FNiagaraStackGraphUtilities::RemoveModuleFromStack(UNiagaraSystem& OwningSystem, FGuid OwningEmitterId, UNiagaraNodeFunctionCall& ModuleNode)
{
	TArray<TWeakObjectPtr<UNiagaraNodeInput>> RemovedNodes;
	return RemoveModuleFromStack(OwningSystem, OwningEmitterId, ModuleNode, RemovedNodes);
}

bool FNiagaraStackGraphUtilities::RemoveModuleFromStack(UNiagaraSystem& OwningSystem, FGuid OwningEmitterId, UNiagaraNodeFunctionCall& ModuleNode, TArray<TWeakObjectPtr<UNiagaraNodeInput>>& OutRemovedInputNodes)
{
	// Find the owning script so it can be modified as part of the transaction so that rapid iteration parameters values are retained upon undo.
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(ModuleNode);
	checkf(OutputNode != nullptr, TEXT("Invalid Stack - Output node could not be found for module"));

	UNiagaraScript* OwningScript = FNiagaraEditorUtilities::GetScriptFromSystem(
		OwningSystem, OwningEmitterId, OutputNode->GetUsage(), OutputNode->GetUsageId());
	checkf(OwningScript != nullptr, TEXT("Invalid Stack - Owning script could not be found for module"));

	return RemoveModuleFromStack(*OwningScript, ModuleNode, OutRemovedInputNodes);
}

bool FNiagaraStackGraphUtilities::RemoveModuleFromStack(UNiagaraScript& OwningScript, UNiagaraNodeFunctionCall& ModuleNode)
{
	TArray<TWeakObjectPtr<UNiagaraNodeInput>> RemovedNodes;
	return RemoveModuleFromStack(OwningScript, ModuleNode, RemovedNodes);
}

bool FNiagaraStackGraphUtilities::RemoveModuleFromStack(UNiagaraScript& OwningScript, UNiagaraNodeFunctionCall& ModuleNode, TArray<TWeakObjectPtr<UNiagaraNodeInput>>& OutRemovedInputNodes)
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(ModuleNode, StackNodeGroups);

	int32 ModuleStackIndex = StackNodeGroups.IndexOfByPredicate([&](const FNiagaraStackGraphUtilities::FStackNodeGroup& StackNodeGroup) { return StackNodeGroup.EndNode == &ModuleNode; });
	if (ModuleStackIndex == INDEX_NONE)
	{
		return false;
	}

	OwningScript.Modify();

	// Disconnect the group from the stack first to make collecting the nodes to remove easier.
	FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(StackNodeGroups[ModuleStackIndex], StackNodeGroups[ModuleStackIndex - 1], StackNodeGroups[ModuleStackIndex + 1]);

	// Traverse all of the nodes in the group to find the nodes to remove.
	FNiagaraStackGraphUtilities::FStackNodeGroup ModuleGroup = StackNodeGroups[ModuleStackIndex];
	TArray<UNiagaraNode*> NodesToRemove;
	TArray<UNiagaraNode*> NodesToCheck;
	NodesToCheck.Add(ModuleGroup.EndNode);
	while (NodesToCheck.Num() > 0)
	{
		UNiagaraNode* NodeToRemove = NodesToCheck[0];
		NodesToCheck.RemoveAt(0);
		NodesToRemove.AddUnique(NodeToRemove);

		TArray<UEdGraphPin*> InputPins;
		NodeToRemove->GetInputPins(InputPins);
		for (UEdGraphPin* InputPin : InputPins)
		{
			if (InputPin->LinkedTo.Num() == 1)
			{
				UNiagaraNode* LinkedNode = Cast<UNiagaraNode>(InputPin->LinkedTo[0]->GetOwningNode());
				if (LinkedNode != nullptr)
				{
					NodesToCheck.Add(LinkedNode);
				}
			}
		}
	}

	// Remove the nodes in the group from the graph.
	UNiagaraGraph* Graph = ModuleNode.GetNiagaraGraph();
	for (UNiagaraNode* NodeToRemove : NodesToRemove)
	{
		NodeToRemove->Modify();
		Graph->RemoveNode(NodeToRemove);
		UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(NodeToRemove);
		if (InputNode != nullptr)
		{
			OutRemovedInputNodes.Add(InputNode);
		}
	}

	return true;
}

void ConnectModuleNode(UNiagaraNodeFunctionCall& ModuleNode, UNiagaraNodeOutput& TargetOutputNode, int32 TargetIndex)
{
	FNiagaraStackGraphUtilities::FStackNodeGroup ModuleGroup;
	ModuleGroup.StartNodes.Add(&ModuleNode);
	ModuleGroup.EndNode = &ModuleNode;

	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(TargetOutputNode, StackNodeGroups);
	checkf(StackNodeGroups.Num() >= 2, TEXT("Stack graph is invalid, can not connect module"));

	int32 InsertIndex;
	if (TargetIndex != INDEX_NONE)
	{
		// The first stack node group is always the input node so we add one to the target module index to get the insertion index.
		InsertIndex = FMath::Clamp(TargetIndex + 1, 1, StackNodeGroups.Num() - 1);
	}
	else
	{
		// If no insert index was specified, add the module at the end.
		InsertIndex = StackNodeGroups.Num() - 1;
	}

	FNiagaraStackGraphUtilities::FStackNodeGroup& TargetInsertGroup = StackNodeGroups[InsertIndex];
	FNiagaraStackGraphUtilities::FStackNodeGroup& TargetInsertPreviousGroup = StackNodeGroups[InsertIndex - 1];
	FNiagaraStackGraphUtilities::ConnectStackNodeGroup(ModuleGroup, TargetInsertPreviousGroup, TargetInsertGroup);
}

bool FNiagaraStackGraphUtilities::FindScriptModulesInStack(FAssetData ModuleScriptAsset, UNiagaraNodeOutput& TargetOutputNode, TArray<UNiagaraNodeFunctionCall*> OutFunctionCalls)
{
	UNiagaraGraph* Graph = TargetOutputNode.GetNiagaraGraph();
	TArray<UNiagaraNode*> Nodes;
	Graph->BuildTraversal(Nodes, &TargetOutputNode);

	OutFunctionCalls.Empty();
	FString ModuleObjectName = ModuleScriptAsset.ObjectPath.ToString();
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeFunctionCall* FunctionCallNode = Cast<UNiagaraNodeFunctionCall>(Node))
		{
			if (FunctionCallNode->FunctionScriptAssetObjectPath == ModuleScriptAsset.ObjectPath ||
				(FunctionCallNode->FunctionScript != nullptr && FunctionCallNode->FunctionScript->GetPathName() == ModuleObjectName))
			{
				OutFunctionCalls.Add(FunctionCallNode);
			}
		}
	}

	return OutFunctionCalls.Num() > 0;
}

UNiagaraNodeFunctionCall* FNiagaraStackGraphUtilities::AddScriptModuleToStack(FAssetData ModuleScriptAsset, UNiagaraNodeOutput& TargetOutputNode, int32 TargetIndex)
{
	UEdGraph* Graph = TargetOutputNode.GetGraph();
	Graph->Modify();

	FGraphNodeCreator<UNiagaraNodeFunctionCall> ModuleNodeCreator(*Graph);
	UNiagaraNodeFunctionCall* NewModuleNode = ModuleNodeCreator.CreateNode();
	NewModuleNode->FunctionScriptAssetObjectPath = ModuleScriptAsset.ObjectPath;
	ModuleNodeCreator.Finalize();

	ConnectModuleNode(*NewModuleNode, TargetOutputNode, TargetIndex);
	return NewModuleNode;
}

UNiagaraNodeAssignment* FNiagaraStackGraphUtilities::AddParameterModuleToStack(const TArray<FNiagaraVariable>& ParameterVariables, UNiagaraNodeOutput& TargetOutputNode, int32 TargetIndex, const TArray<FString>& InDefaultValues)
{
	UEdGraph* Graph = TargetOutputNode.GetGraph();
	Graph->Modify();

	FGraphNodeCreator<UNiagaraNodeAssignment> AssignmentNodeCreator(*Graph);
	UNiagaraNodeAssignment* NewAssignmentNode = AssignmentNodeCreator.CreateNode();

	check(ParameterVariables.Num() == InDefaultValues.Num());
	for (int32 i = 0; i < ParameterVariables.Num(); i++)
	{
		NewAssignmentNode->AddAssignmentTarget(ParameterVariables[i], &InDefaultValues[i]);
	}
	AssignmentNodeCreator.Finalize();

	ConnectModuleNode(*NewAssignmentNode, TargetOutputNode, TargetIndex);
	NewAssignmentNode->UpdateUsageBitmaskFromOwningScript();

	return NewAssignmentNode;
}

void GetAllNodesForModule(UNiagaraNodeFunctionCall& ModuleFunctionCall, TArray<UNiagaraNode*>& ModuleNodes)
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(ModuleFunctionCall, StackNodeGroups);

	int32 ThisGroupIndex = StackNodeGroups.IndexOfByPredicate([&](const FNiagaraStackGraphUtilities::FStackNodeGroup& Group) { return Group.EndNode == &ModuleFunctionCall; });
	checkf(ThisGroupIndex > 0 && ThisGroupIndex < StackNodeGroups.Num() - 1, TEXT("Stack graph is invalid"));

	TArray<UNiagaraNode*> AllGroupNodes;
	StackNodeGroups[ThisGroupIndex].GetAllNodesInGroup(ModuleNodes);
}

TOptional<bool> FNiagaraStackGraphUtilities::GetModuleIsEnabled(UNiagaraNodeFunctionCall& FunctionCallNode)
{
	TArray<UNiagaraNode*> AllModuleNodes;
	GetAllNodesForModule(FunctionCallNode, AllModuleNodes);
	bool bIsEnabled = AllModuleNodes[0]->IsNodeEnabled();
	for (int32 i = 1; i < AllModuleNodes.Num(); i++)
	{
		if (AllModuleNodes[i]->IsNodeEnabled() != bIsEnabled)
		{
			return TOptional<bool>();
		}
	}
	return TOptional<bool>(bIsEnabled);
}

void FNiagaraStackGraphUtilities::SetModuleIsEnabled(UNiagaraNodeFunctionCall& FunctionCallNode, bool bIsEnabled)
{
	FunctionCallNode.Modify();
	TArray<UNiagaraNode*> ModuleNodes;
	GetAllNodesForModule(FunctionCallNode, ModuleNodes);
	for (UNiagaraNode* ModuleNode : ModuleNodes)
	{
		ModuleNode->Modify();
		ModuleNode->SetEnabledState(bIsEnabled ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled, true);
		ModuleNode->MarkNodeRequiresSynchronization(__FUNCTION__, false);
	}
	FunctionCallNode.GetNiagaraGraph()->NotifyGraphNeedsRecompile();
}

bool FNiagaraStackGraphUtilities::ValidateGraphForOutput(UNiagaraGraph& NiagaraGraph, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, FText& ErrorMessage)
{
	UNiagaraNodeOutput* OutputNode = NiagaraGraph.FindEquivalentOutputNode(ScriptUsage, ScriptUsageId);
	if (OutputNode == nullptr)
	{
		ErrorMessage = LOCTEXT("ValidateNoOutputMessage", "Output node doesn't exist for script.");
		return false;
	}

	TArray<FStackNodeGroup> NodeGroups;
	GetStackNodeGroups(*OutputNode, NodeGroups);
	
	if (NodeGroups.Num() < 2 || NodeGroups[0].EndNode->IsA<UNiagaraNodeInput>() == false || NodeGroups.Last().EndNode->IsA<UNiagaraNodeOutput>() == false)
	{
		ErrorMessage = LOCTEXT("ValidateInvalidStackMessage", "Stack graph does not include an input node connected to an output node.");
		return false;
	}

	return true;
}

UNiagaraNodeOutput* FNiagaraStackGraphUtilities::ResetGraphForOutput(UNiagaraGraph& NiagaraGraph, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, const FGuid& PreferredOutputNodeGuid, const FGuid& PreferredInputNodeGuid)
{
	NiagaraGraph.Modify();
	UNiagaraNodeOutput* OutputNode = NiagaraGraph.FindOutputNode(ScriptUsage, ScriptUsageId);
	UEdGraphPin* OutputNodeInputPin = OutputNode != nullptr ? GetParameterMapInputPin(*OutputNode) : nullptr;
	if (OutputNode != nullptr && OutputNodeInputPin == nullptr)
	{
		NiagaraGraph.RemoveNode(OutputNode);
		OutputNode = nullptr;
	}

	if (OutputNode == nullptr)
	{
		FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(NiagaraGraph);
		OutputNode = OutputNodeCreator.CreateNode();
		OutputNode->SetUsage(ScriptUsage);
		OutputNode->SetUsageId(ScriptUsageId);
		OutputNode->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out")));
		OutputNodeCreator.Finalize();

		if (PreferredOutputNodeGuid.IsValid())
		{
			OutputNode->NodeGuid = PreferredOutputNodeGuid;
		}

		OutputNodeInputPin = GetParameterMapInputPin(*OutputNode);
	}
	else
	{
		OutputNode->Modify();
	}

	FNiagaraVariable InputVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
	FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(NiagaraGraph);
	UNiagaraNodeInput* InputNode = InputNodeCreator.CreateNode();
	InputNode->Input = FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
	InputNode->Usage = ENiagaraInputNodeUsage::Parameter;
	InputNodeCreator.Finalize();

	if (PreferredInputNodeGuid.IsValid())
	{
		InputNode->NodeGuid = PreferredInputNodeGuid;
	}

	UEdGraphPin* InputNodeOutputPin = GetParameterMapOutputPin(*InputNode);
	OutputNodeInputPin->BreakAllPinLinks();
	OutputNodeInputPin->MakeLinkTo(InputNodeOutputPin);

	if (ScriptUsage == ENiagaraScriptUsage::SystemSpawnScript || ScriptUsage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		// TODO: Move the emitter node wrangling to a utility function instead of getting the typed outer here and creating a view model.
		UNiagaraSystem* System = NiagaraGraph.GetTypedOuter<UNiagaraSystem>();
		if (System != nullptr && System->GetEmitterHandles().Num() != 0)
		{
			TSharedRef<FNiagaraSystemScriptViewModel> SystemScriptViewModel = MakeShared<FNiagaraSystemScriptViewModel>(*System, nullptr);
			SystemScriptViewModel->RebuildEmitterNodes();
		}
	}

	return OutputNode;
}

const UNiagaraEmitter* FNiagaraStackGraphUtilities::GetBaseEmitter(UNiagaraEmitter& Emitter, UNiagaraSystem& OwningSystem)
{
	for(const FNiagaraEmitterHandle& Handle : OwningSystem.GetEmitterHandles())
	{ 
		if (Handle.GetInstance() == &Emitter)
		{
			if (Handle.GetSource() != nullptr && Handle.GetSource() != &Emitter)
			{
				return Handle.GetSource();
			}
			else
			{
				// If the source is null then it was deleted and if the source is the same as the emitter the owning
				// system is transient and the emitter doesn't have base.
				return nullptr;
			}
		}
	}
	return nullptr;
}

void GetFunctionNamesRecursive(UNiagaraNode* CurrentNode, TArray<UNiagaraNode*>& VisitedNodes, TArray<FString>& FunctionNames)
{
	if (VisitedNodes.Contains(CurrentNode) == false)
	{
		VisitedNodes.Add(CurrentNode);
		UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(CurrentNode);
		if (FunctionCall != nullptr)
		{
			FunctionNames.Add(FunctionCall->GetFunctionName());
		}
		TArray<UEdGraphPin*> InputPins;
		CurrentNode->GetInputPins(InputPins);
		for (UEdGraphPin* InputPin : InputPins)
		{
			for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
			{
				UNiagaraNode* LinkedNode = Cast<UNiagaraNode>(LinkedPin->GetOwningNode());
				if (LinkedNode != nullptr)
				{
					GetFunctionNamesRecursive(LinkedNode, VisitedNodes, FunctionNames);
				}
			}
		}
	}
}

void GetFunctionNamesForOutputNode(UNiagaraNodeOutput& OutputNode, TArray<FString>& FunctionNames)
{
	TArray<UNiagaraNode*> VisitedNodes;
	GetFunctionNamesRecursive(&OutputNode, VisitedNodes, FunctionNames);
}

bool FNiagaraStackGraphUtilities::IsRapidIterationType(const FNiagaraTypeDefinition& InputType)
{
	checkf(InputType.IsValid(), TEXT("Type is invalid."));
	return InputType != FNiagaraTypeDefinition::GetBoolDef() && !InputType.IsEnum() &&
		InputType != FNiagaraTypeDefinition::GetParameterMapDef() && !InputType.IsDataInterface();
}

FNiagaraVariable FNiagaraStackGraphUtilities::CreateRapidIterationParameter(const FString& UniqueEmitterName, ENiagaraScriptUsage ScriptUsage, const FName& AliasedInputName, const FNiagaraTypeDefinition& InputType)
{
	FNiagaraVariable InputVariable(InputType, AliasedInputName);
	FNiagaraVariable RapidIterationVariable;
	if (ScriptUsage == ENiagaraScriptUsage::SystemSpawnScript || ScriptUsage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		RapidIterationVariable = FNiagaraParameterMapHistory::ConvertVariableToRapidIterationConstantName(InputVariable, nullptr, ScriptUsage); // These names *should* have the emitter baked in...
	}
	else
	{
		RapidIterationVariable = FNiagaraParameterMapHistory::ConvertVariableToRapidIterationConstantName(InputVariable, *UniqueEmitterName, ScriptUsage);
	}

	return RapidIterationVariable;
}

void FNiagaraStackGraphUtilities::CleanUpStaleRapidIterationParameters(UNiagaraScript& Script, UNiagaraEmitter& OwningEmitter)
{
	UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(Script.GetSource());
	UNiagaraNodeOutput* OutputNode = Source->NodeGraph->FindOutputNode(Script.GetUsage(), Script.GetUsageId());
	if (OutputNode != nullptr)
	{
		TArray<FString> ValidFunctionCallNames;
		GetFunctionNamesForOutputNode(*OutputNode, ValidFunctionCallNames);
		TArray<FNiagaraVariable> RapidIterationParameters;
		Script.RapidIterationParameters.GetParameters(RapidIterationParameters);
		for (const FNiagaraVariable& RapidIterationParameter : RapidIterationParameters)
		{
			FString EmitterName;
			FString FunctionCallName;
			FString InputName;
			if (FNiagaraParameterMapHistory::SplitRapidIterationParameterName(RapidIterationParameter, EmitterName, FunctionCallName, InputName))
			{
				if (EmitterName != OwningEmitter.GetUniqueEmitterName() || ValidFunctionCallNames.Contains(FunctionCallName) == false)
				{
					Script.RapidIterationParameters.RemoveParameter(RapidIterationParameter);
				}
			}
		}
	}
}

void FNiagaraStackGraphUtilities::CleanUpStaleRapidIterationParameters(UNiagaraEmitter& Emitter)
{
	TArray<UNiagaraScript*> Scripts;
	Emitter.GetScripts(Scripts, false);
	for (UNiagaraScript* Script : Scripts)
	{
		CleanUpStaleRapidIterationParameters(*Script, Emitter);
	}
}

void FNiagaraStackGraphUtilities::GetNewParameterAvailableTypes(TArray<FNiagaraTypeDefinition>& OutAvailableTypes)
{
	for (const FNiagaraTypeDefinition& RegisteredParameterType : FNiagaraTypeRegistry::GetRegisteredParameterTypes())
	{
		if (RegisteredParameterType != FNiagaraTypeDefinition::GetGenericNumericDef() && RegisteredParameterType != FNiagaraTypeDefinition::GetParameterMapDef())
		{
			OutAvailableTypes.Add(RegisteredParameterType);
		}
	}
}

void FNiagaraStackGraphUtilities::GetScriptAssetsByDependencyProvided(ENiagaraScriptUsage AssetUsage, FName DependencyName, TArray<FAssetData>& OutAssets)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> ScriptAssets;
	AssetRegistryModule.Get().GetAssetsByClass(UNiagaraScript::StaticClass()->GetFName(), ScriptAssets);
	
	for (const FAssetData& ScriptAsset : ScriptAssets)
	{
		auto TagName = GET_MEMBER_NAME_CHECKED(UNiagaraScript, ProvidedDependencies);

		FString ProvidedDependenciesString;
		if(ScriptAsset.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraScript, ProvidedDependencies), ProvidedDependenciesString) && ProvidedDependenciesString.IsEmpty() == false)
		{
			TArray<FString> DependencyStrings;
			ProvidedDependenciesString.ParseIntoArray(DependencyStrings, TEXT(","));
			for (FString DependencyString: DependencyStrings)
			{
				if (FName(*DependencyString) == DependencyName)
				{
					OutAssets.Add(ScriptAsset);
					break;
				}
			}
		}
	}
}

void FNiagaraStackGraphUtilities::GetAvailableParametersForScript(UNiagaraNodeOutput& ScriptOutputNode, TArray<FNiagaraVariable>& OutAvailableParameters)
{
	TArray<FNiagaraParameterMapHistory> Histories = UNiagaraNodeParameterMapBase::GetParameterMaps(ScriptOutputNode.GetNiagaraGraph());

	if (ScriptOutputNode.GetUsage() == ENiagaraScriptUsage::ParticleSpawnScript ||
		ScriptOutputNode.GetUsage() == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated ||
		ScriptOutputNode.GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript ||
		ScriptOutputNode.GetUsage() == ENiagaraScriptUsage::ParticleEventScript)
	{
		OutAvailableParameters.Append(FNiagaraConstants::GetCommonParticleAttributes());
	}

	for (FNiagaraParameterMapHistory& History : Histories)
	{
		for (FNiagaraVariable& Variable : History.Variables)
		{
			if (History.IsPrimaryDataSetOutput(Variable, ScriptOutputNode.GetUsage()))
			{
				OutAvailableParameters.AddUnique(Variable);
			}
		}
	}

	TOptional<FName> UsageNamespace = FNiagaraStackGraphUtilities::GetNamespaceForScriptUsage(ScriptOutputNode.GetUsage());
	if (UsageNamespace.IsSet())
	{
		for (const TPair<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection>& Entry : ScriptOutputNode.GetNiagaraGraph()->GetParameterMap())
		{
			// Pick up any params with 0 references from the Parameters window
			bool bDoesParamHaveNoReferences = Entry.Value.ParameterReferences.Num() == 0;
			bool bIsParamInUsageNamespace = Entry.Key.IsInNameSpace(UsageNamespace.GetValue().ToString());

			if (bDoesParamHaveNoReferences && bIsParamInUsageNamespace)
			{
				OutAvailableParameters.AddUnique(Entry.Key);
			}
		}
	}
}

TOptional<FName> FNiagaraStackGraphUtilities::GetNamespaceForScriptUsage(ENiagaraScriptUsage ScriptUsage)
{
	switch (ScriptUsage)
	{
	case ENiagaraScriptUsage::ParticleSpawnScript:
	case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
	case ENiagaraScriptUsage::ParticleUpdateScript:
	case ENiagaraScriptUsage::ParticleEventScript:
		return FNiagaraParameterHandle::ParticleAttributeNamespace;
	case ENiagaraScriptUsage::EmitterSpawnScript:
	case ENiagaraScriptUsage::EmitterUpdateScript:
		return FNiagaraParameterHandle::EmitterNamespace;
	case ENiagaraScriptUsage::SystemSpawnScript:
	case ENiagaraScriptUsage::SystemUpdateScript:
		return FNiagaraParameterHandle::SystemNamespace;
	default:
		return TOptional<FName>();
	}
}

void FNiagaraStackGraphUtilities::GetOwningEmitterAndScriptForStackNode(UNiagaraNode& StackNode, UNiagaraSystem& OwningSystem, UNiagaraEmitter*& OutEmitter, UNiagaraScript*& OutScript)
{
	OutEmitter = nullptr;
	OutScript = nullptr;
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(StackNode);
	if (OutputNode != nullptr)
	{
		switch (OutputNode->GetUsage())
		{
			case ENiagaraScriptUsage::SystemSpawnScript:
				OutScript = OwningSystem.GetSystemSpawnScript();
				break;
			case ENiagaraScriptUsage::SystemUpdateScript:
				OutScript = OwningSystem.GetSystemUpdateScript();
				break;
			case ENiagaraScriptUsage::EmitterSpawnScript:
			case ENiagaraScriptUsage::EmitterUpdateScript:
			case ENiagaraScriptUsage::ParticleSpawnScript:
			case ENiagaraScriptUsage::ParticleUpdateScript:
			case ENiagaraScriptUsage::ParticleEventScript:
				for (const FNiagaraEmitterHandle& EmitterHandle : OwningSystem.GetEmitterHandles())
				{
					UNiagaraScriptSource* EmitterSource = CastChecked<UNiagaraScriptSource>(EmitterHandle.GetInstance()->GraphSource);
					if (EmitterSource->NodeGraph == StackNode.GetNiagaraGraph())
					{
						OutEmitter = EmitterHandle.GetInstance();
						OutScript = OutEmitter->GetScript(OutputNode->GetUsage(), OutputNode->GetUsageId());
						break;
					}
				}
				break;
		}
	}
}

struct FRapidIterationParameterContext
{
	FRapidIterationParameterContext()
		: UniqueEmitterName()
		, OwningFunctionCall(nullptr)
	{
	}

	FRapidIterationParameterContext(FString InUniqueEmitterName, UNiagaraNodeFunctionCall& InOwningFunctionCall)
		: UniqueEmitterName(InUniqueEmitterName)
		, OwningFunctionCall(&InOwningFunctionCall)
	{
	}

	bool IsValid()
	{
		return UniqueEmitterName.IsEmpty() == false && OwningFunctionCall != nullptr;
	}

	FNiagaraVariable GetValue(UNiagaraScript& OwningScript, FName InputName, FNiagaraTypeDefinition Type)
	{
		FNiagaraParameterHandle ModuleHandle = FNiagaraParameterHandle::CreateModuleParameterHandle(InputName);
		FNiagaraParameterHandle AliasedFunctionHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, OwningFunctionCall);
		FNiagaraVariable RapidIterationParameter = FNiagaraStackGraphUtilities::CreateRapidIterationParameter(UniqueEmitterName, OwningScript.GetUsage(), AliasedFunctionHandle.GetParameterHandleString(), Type);
		const uint8* ValueData = OwningScript.RapidIterationParameters.GetParameterData(RapidIterationParameter);
		if (ValueData != nullptr)
		{
			RapidIterationParameter.SetData(ValueData);
			return RapidIterationParameter;
		}
		return FNiagaraVariable();
	}

	const FString UniqueEmitterName;
	UNiagaraNodeFunctionCall* const OwningFunctionCall;
};

struct FStackFunctionInputValue
{
	FNiagaraTypeDefinition Type;
	bool bIsOverride;
	TOptional<FNiagaraVariable> LocalValue;
	TOptional<FName> LinkedValue;
	TOptional<UNiagaraDataInterface*> DataValue;
	TOptional<UNiagaraNodeFunctionCall*> DynamicValue;
	TMap<FName, TSharedRef<FStackFunctionInputValue>> DynamicValueInputs;

	bool Matches(const FStackFunctionInputValue& Other) const
	{
		if (Type != Other.Type)
		{
			return false;
		}
		if (LocalValue.IsSet() && Other.LocalValue.IsSet())
		{
			return LocalValue.GetValue().GetType() == Other.LocalValue.GetValue().GetType() &&
				FMemory::Memcmp(LocalValue.GetValue().GetData(), Other.LocalValue.GetValue().GetData(), LocalValue.GetValue().GetSizeInBytes()) == 0;
		}
		else if (LinkedValue.IsSet() && Other.LinkedValue.IsSet())
		{
			return LinkedValue.GetValue() == Other.LinkedValue.GetValue();
		}
		else if (DataValue.IsSet() && Other.DataValue.IsSet())
		{
			return (DataValue.GetValue() == nullptr && Other.DataValue.GetValue() == nullptr) ||
				(DataValue.GetValue() != nullptr && Other.DataValue.GetValue() != nullptr && DataValue.GetValue()->Equals(Other.DataValue.GetValue()));
		}
		else if (DynamicValue.IsSet() && Other.DynamicValue.IsSet())
		{
			if (DynamicValue.GetValue()->FunctionScript == Other.DynamicValue.GetValue()->FunctionScript)
			{
				for (auto It = DynamicValueInputs.CreateConstIterator(); It; ++It)
				{
					FName InputName = It.Key();
					TSharedRef<FStackFunctionInputValue> InputValue = It.Value();
					const TSharedRef<FStackFunctionInputValue>* OtherInputValue = Other.DynamicValueInputs.Find(InputName);
					if (OtherInputValue == nullptr || InputValue->Matches(**OtherInputValue) == false)
					{
						return false;
					}
				}
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
};

bool TryGetStackFunctionInputValue(UNiagaraScript& OwningScript, const UEdGraphPin* OverridePin, const UEdGraphPin& DefaultPin, FName InputName, FRapidIterationParameterContext RapidIterationParameterContext, FStackFunctionInputValue& OutStackFunctionInputValue)
{
	OutStackFunctionInputValue.Type = GetDefault<UEdGraphSchema_Niagara>()->PinToTypeDefinition(&DefaultPin);
	OutStackFunctionInputValue.bIsOverride = OverridePin != nullptr;
	const UEdGraphPin& InputPin = OverridePin != nullptr ? *OverridePin : DefaultPin;
	if (RapidIterationParameterContext.IsValid() && DefaultPin.LinkedTo.Num() == 0 && OverridePin == nullptr)
	{
		OutStackFunctionInputValue.LocalValue = RapidIterationParameterContext.GetValue(OwningScript, InputName, OutStackFunctionInputValue.Type);
	}
	else if (InputPin.LinkedTo.Num() == 0)
	{
		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
		if (NiagaraSchema->PinToTypeDefinition(&InputPin).IsDataInterface())
		{
			OutStackFunctionInputValue.DataValue = nullptr;
		}
		else
		{
			OutStackFunctionInputValue.LocalValue = NiagaraSchema->PinToNiagaraVariable(&InputPin, true);
		}
	}
	else if (InputPin.LinkedTo.Num() == 1)
	{
		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
		UEdGraphNode* PreviousOwningNode = InputPin.LinkedTo[0]->GetOwningNode();

		
		if (PreviousOwningNode->IsA<UNiagaraNodeParameterMapGet>())
		{
			OutStackFunctionInputValue.LinkedValue = InputPin.LinkedTo[0]->GetFName();
		}
		else if (PreviousOwningNode->IsA<UNiagaraNodeInput>())
		{
			OutStackFunctionInputValue.DataValue = CastChecked<UNiagaraNodeInput>(InputPin.LinkedTo[0]->GetOwningNode())->GetDataInterface();
		}
		else if (PreviousOwningNode->IsA<UNiagaraNodeFunctionCall>() && 
			FNiagaraStackGraphUtilities::GetParameterMapInputPin(*(static_cast<UNiagaraNodeFunctionCall*>(PreviousOwningNode))) != nullptr)
		{
			UNiagaraNodeFunctionCall* DynamicInputFunctionCall = CastChecked<UNiagaraNodeFunctionCall>(InputPin.LinkedTo[0]->GetOwningNode());
			OutStackFunctionInputValue.DynamicValue = DynamicInputFunctionCall;
			TArray<const UEdGraphPin*> DynamicValueInputPins;
			FNiagaraStackGraphUtilities::GetStackFunctionInputPins(*DynamicInputFunctionCall,DynamicValueInputPins,
				FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);

			FRapidIterationParameterContext InputRapidIterationParameterContext = RapidIterationParameterContext.IsValid() 
				? FRapidIterationParameterContext(RapidIterationParameterContext.UniqueEmitterName, *DynamicInputFunctionCall)
				: FRapidIterationParameterContext();

			for (const UEdGraphPin* DynamicValueInputPin : DynamicValueInputPins)
			{
				FNiagaraParameterHandle ModuleHandle(DynamicValueInputPin->GetFName());
				UEdGraphPin* DynamicValueOverridePin = FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin(*DynamicInputFunctionCall,
					FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, DynamicInputFunctionCall));

				UEdGraphPin* DynamicValueInputDefaultPin = DynamicInputFunctionCall->FindParameterMapDefaultValuePin(DynamicValueInputPin->PinName, OwningScript.GetUsage());

				FStackFunctionInputValue InputValue;
				if (TryGetStackFunctionInputValue(OwningScript, DynamicValueOverridePin, *DynamicValueInputDefaultPin, ModuleHandle.GetName(), InputRapidIterationParameterContext, InputValue))
				{
					OutStackFunctionInputValue.DynamicValueInputs.Add(ModuleHandle.GetName(), MakeShared<FStackFunctionInputValue>(InputValue));
				}
				else
				{
					return false;
				}
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	return true;
}

bool FNiagaraStackGraphUtilities::IsValidDefaultDynamicInput(UNiagaraScript& OwningScript, UEdGraphPin& DefaultPin)
{
	FStackFunctionInputValue InputValue;
	return TryGetStackFunctionInputValue(OwningScript, nullptr, DefaultPin, NAME_None, FRapidIterationParameterContext(), InputValue) && InputValue.DynamicValue.IsSet();
}

bool FNiagaraStackGraphUtilities::ParameterIsCompatibleWithScriptUsage(FNiagaraVariable Parameter, ENiagaraScriptUsage Usage)
{
	const FNiagaraParameterHandle ParameterHandle(Parameter.GetName());
	switch (Usage)
	{
	case ENiagaraScriptUsage::SystemSpawnScript:
	case ENiagaraScriptUsage::SystemUpdateScript:
		return ParameterHandle.IsSystemHandle();
	case ENiagaraScriptUsage::EmitterSpawnScript:
	case ENiagaraScriptUsage::EmitterUpdateScript:
		return ParameterHandle.IsEmitterHandle();
	case ENiagaraScriptUsage::ParticleSpawnScript:
	case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
	case ENiagaraScriptUsage::ParticleUpdateScript:
	case ENiagaraScriptUsage::ParticleEventScript:
		return ParameterHandle.IsParticleAttributeHandle();
	default:
		return false;
	}
}

bool FNiagaraStackGraphUtilities::DoesDynamicInputMatchDefault(
	FString EmitterUniqueName, 
	UNiagaraScript& OwningScript,
	UNiagaraNodeFunctionCall& OwningFunctionCallNode,
	UEdGraphPin& OverridePin,
	FName InputName,
	UEdGraphPin& DefaultPin)
{
	FStackFunctionInputValue CurrentValue;
	FStackFunctionInputValue DefaultValue;
	return
		TryGetStackFunctionInputValue(OwningScript, &OverridePin, DefaultPin, InputName, FRapidIterationParameterContext(EmitterUniqueName, OwningFunctionCallNode), CurrentValue) &&
		TryGetStackFunctionInputValue(OwningScript, nullptr, DefaultPin, NAME_None, FRapidIterationParameterContext(), DefaultValue) &&
		CurrentValue.Matches(DefaultValue);
}

void SetInputValue(
	TSharedRef<FNiagaraSystemViewModel> SystemViewModel,
	TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel,
	UNiagaraStackEditorData& StackEditorData,
	UNiagaraScript& SourceScript,
	const TArray<TWeakObjectPtr<UNiagaraScript>> AffectedScripts,
	UNiagaraNodeFunctionCall& ModuleNode,
	UNiagaraNodeFunctionCall& InputFunctionCallNode,
	FName InputName,
	const FStackFunctionInputValue& Value)
{
	FNiagaraParameterHandle ModuleHandle = FNiagaraParameterHandle::CreateModuleParameterHandle(InputName);
	FNiagaraParameterHandle AliasedFunctionHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, &InputFunctionCallNode);
	if (Value.LocalValue.IsSet())
	{
		bool bRapidIterationParameterSet = false;
		if (FNiagaraStackGraphUtilities::IsRapidIterationType(Value.Type))
		{
			UEdGraphPin* DefaultPin = InputFunctionCallNode.FindParameterMapDefaultValuePin(ModuleHandle.GetParameterHandleString(), SourceScript.GetUsage());
			if (DefaultPin->LinkedTo.Num() == 0)
			{
				FNiagaraVariable RapidIterationParameter = FNiagaraStackGraphUtilities::CreateRapidIterationParameter(
					EmitterViewModel->GetEmitter()->GetUniqueEmitterName(), SourceScript.GetUsage(), AliasedFunctionHandle.GetParameterHandleString(), Value.Type);

				for (TWeakObjectPtr<UNiagaraScript> AffectedScript : AffectedScripts)
				{
					AffectedScript->Modify();
					AffectedScript->RapidIterationParameters.SetParameterData(Value.LocalValue.GetValue().GetData(), RapidIterationParameter, true);
				}
				bRapidIterationParameterSet = true;
			}
		}

		if (bRapidIterationParameterSet == false)
		{
			const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
			FString PinDefaultValue;
			if (ensureMsgf(NiagaraSchema->TryGetPinDefaultValueFromNiagaraVariable(Value.LocalValue.GetValue(), PinDefaultValue),
				TEXT("Could not generate default value string for non-rapid iteration parameter.")))
			{
				UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(InputFunctionCallNode, AliasedFunctionHandle, Value.Type);
				OverridePin.Modify();
				OverridePin.DefaultValue = PinDefaultValue;
				Cast<UNiagaraNode>(OverridePin.GetOwningNode())->MarkNodeRequiresSynchronization(TEXT("OverridePin Default Value Changed"), true);
			}
		}
	}
	else if (Value.LinkedValue.IsSet())
	{
		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(InputFunctionCallNode, AliasedFunctionHandle, Value.Type);
		FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(OverridePin, FNiagaraParameterHandle(Value.LinkedValue.GetValue()));
	}
	else if (Value.DataValue.IsSet())
	{
		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(InputFunctionCallNode, AliasedFunctionHandle, Value.Type);
		FString DataObjectName = Value.DataValue.GetValue() != nullptr ? Value.DataValue.GetValue()->GetName() : Value.Type.GetName();
		UNiagaraDataInterface* NewDataObject;
		FNiagaraStackGraphUtilities::SetDataValueObjectForFunctionInput(OverridePin, Value.Type.GetClass(), DataObjectName, NewDataObject);
		if (Value.DataValue.GetValue() != nullptr)
		{
			Value.DataValue.GetValue()->CopyTo(NewDataObject);
		}
	}
	else if (Value.DynamicValue.IsSet())
	{
		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(InputFunctionCallNode, AliasedFunctionHandle, Value.Type);
		UNiagaraNodeFunctionCall* NewDynamicInputFunctionCall;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(OverridePin, Value.DynamicValue.GetValue()->FunctionScript, NewDynamicInputFunctionCall);
		FNiagaraStackGraphUtilities::InitializeStackFunctionInputs(SystemViewModel, EmitterViewModel, StackEditorData, ModuleNode, *NewDynamicInputFunctionCall);
		for (auto It = Value.DynamicValueInputs.CreateConstIterator(); It; ++It)
		{
			FName DynamicValueInputName = It.Key();
			TSharedRef<FStackFunctionInputValue> DynamicValueInputValue = It.Value();
			if (DynamicValueInputValue->bIsOverride)
			{
				SetInputValue(SystemViewModel, EmitterViewModel, StackEditorData, SourceScript, AffectedScripts, ModuleNode, *NewDynamicInputFunctionCall, DynamicValueInputName, *DynamicValueInputValue);
			}
		}
	}
}

void FNiagaraStackGraphUtilities::ResetToDefaultDynamicInput(
	TSharedRef<FNiagaraSystemViewModel> SystemViewModel,
	TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel,
	UNiagaraStackEditorData& StackEditorData,
	UNiagaraScript& SourceScript,
	const TArray<TWeakObjectPtr<UNiagaraScript>> AffectedScripts,
	UNiagaraNodeFunctionCall& ModuleNode,
	UNiagaraNodeFunctionCall& InputFunctionCallNode,
	FName InputName,
	UEdGraphPin& DefaultPin)
{
	FStackFunctionInputValue DefaultValue;
	if (ensureMsgf(TryGetStackFunctionInputValue(SourceScript, nullptr, DefaultPin, NAME_None, FRapidIterationParameterContext(), DefaultValue), TEXT("Default dynamic input was not valid")))
	{
		SetInputValue(SystemViewModel, EmitterViewModel, StackEditorData, SourceScript, AffectedScripts, ModuleNode, InputFunctionCallNode, InputName, DefaultValue);
	}
}

bool FNiagaraStackGraphUtilities::GetStackIssuesRecursively(const UNiagaraStackEntry* const Entry, TArray<UNiagaraStackErrorItem*>& OutIssues)
{
	TArray<UNiagaraStackEntry*> Entries;
	Entry->GetUnfilteredChildren(Entries);
	while (Entries.Num() > 0)
	{
		UNiagaraStackEntry* EntryToProcess = Entries[0];
		UNiagaraStackErrorItem* ErrorItem = Cast<UNiagaraStackErrorItem>(EntryToProcess);
		if (ErrorItem != nullptr)
		{
			OutIssues.Add(ErrorItem);
		}
		else // don't process error items, errors don't have errors
		{
			EntryToProcess->GetUnfilteredChildren(Entries);
		}
		Entries.RemoveAtSwap(0); 
	}
	return OutIssues.Num() > 0;
}

void FNiagaraStackGraphUtilities::MoveModule(UNiagaraScript& SourceScript, UNiagaraNodeFunctionCall& ModuleToMove, UNiagaraSystem& TargetSystem, FGuid TargetEmitterHandleId, ENiagaraScriptUsage TargetUsage, FGuid TargetUsageId, int32 TargetModuleIndex)
{
	UNiagaraScript* TargetScript = FNiagaraEditorUtilities::GetScriptFromSystem(TargetSystem, TargetEmitterHandleId, TargetUsage, TargetUsageId);
	checkf(TargetScript != nullptr, TEXT("Target script not found"));

	UNiagaraNodeOutput* TargetOutputNode = FNiagaraEditorUtilities::GetScriptOutputNode(*TargetScript);
	checkf(TargetOutputNode != nullptr, TEXT("Target stack is invalid"));

	TArray<FStackNodeGroup> SourceGroups;
	GetStackNodeGroups(ModuleToMove, SourceGroups);
	int32 SourceGroupIndex = SourceGroups.IndexOfByPredicate([&ModuleToMove](const FStackNodeGroup& SourceGroup) { return SourceGroup.EndNode == &ModuleToMove; });
	TArray<UNiagaraNode*> SourceGroupNodes;
	SourceGroups[SourceGroupIndex].GetAllNodesInGroup(SourceGroupNodes);

	UNiagaraGraph* SourceGraph = ModuleToMove.GetNiagaraGraph();
	UNiagaraGraph* TargetGraph = TargetOutputNode->GetNiagaraGraph();

	// If the source and target scripts don't match we need to collect the rapid iteration parameter values for each function in the source group
	// so we can restore them after moving.
	TMap<FGuid, TArray<FNiagaraVariable>> SourceFunctionIdToRapidIterationParametersMap;
	if (&SourceScript != TargetScript)
	{
		TMap<FString, FGuid> FunctionCallNameToNodeIdMap;
		for (UNiagaraNode* SourceGroupNode : SourceGroupNodes)
		{
			UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(SourceGroupNode);
			if (FunctionNode != nullptr)
			{
				FunctionCallNameToNodeIdMap.Add(FunctionNode->GetFunctionName(), FunctionNode->NodeGuid);
			}
		}

		TArray<FNiagaraVariable> ScriptRapidIterationParameters;
		SourceScript.RapidIterationParameters.GetParameters(ScriptRapidIterationParameters);
		for (FNiagaraVariable& ScriptRapidIterationParameter : ScriptRapidIterationParameters)
		{
			FString EmitterName;
			FString FunctionCallName;
			FString InputName;
			if (FNiagaraParameterMapHistory::SplitRapidIterationParameterName(
				ScriptRapidIterationParameter, EmitterName, FunctionCallName, InputName))
			{
				FGuid* NodeIdPtr = FunctionCallNameToNodeIdMap.Find(FunctionCallName);
				if (NodeIdPtr != nullptr)
				{
					TArray<FNiagaraVariable>& RapidIterationParameters = SourceFunctionIdToRapidIterationParametersMap.FindOrAdd(*NodeIdPtr);
					RapidIterationParameters.Add(ScriptRapidIterationParameter);
					RapidIterationParameters.Last().SetData(SourceScript.RapidIterationParameters.GetParameterData(ScriptRapidIterationParameter));
				}
			}
		}
	}

	FStackNodeGroup TargetGroup;
	TArray<UNiagaraNode*> TargetGroupNodes;
	TMap<FGuid, FGuid> OldNodeIdToNewIdMap;
	if (SourceGraph == TargetGraph)
	{
		TargetGroup = SourceGroups[SourceGroupIndex];
		TargetGroupNodes = SourceGroupNodes;
	}
	else 
	{
		// If the module is being inserted into a different graph all of the nodes need to be copied into the target graph.
		FStackNodeGroup SourceGroup = SourceGroups[SourceGroupIndex];
		
		TSet<UObject*> NodesToCopy;
		for (UNiagaraNode* SourceGroupNode : SourceGroupNodes)
		{
			SourceGroupNode->PrepareForCopying();
			NodesToCopy.Add(SourceGroupNode);
		}

		FString ExportedText;
		FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);

		TSet<UEdGraphNode*> CopiedNodesSet;
		FEdGraphUtilities::ImportNodesFromText(TargetGraph, ExportedText, CopiedNodesSet);
		TArray<UEdGraphNode*> CopiedNodes = CopiedNodesSet.Array();

		// Collect the start and end nodes for the group by ID before assigning the copied nodes new ids.
		UEdGraphNode** CopiedEndNode = CopiedNodes.FindByPredicate([SourceGroup](UEdGraphNode* CopiedNode)
			{ return CopiedNode->NodeGuid == SourceGroup.EndNode->NodeGuid; });
		checkf(CopiedEndNode != nullptr, TEXT("Group copy failed"));
		TargetGroup.EndNode = CastChecked<UNiagaraNode>(*CopiedEndNode);

		for (UNiagaraNode* StartNode : SourceGroup.StartNodes)
		{
			UEdGraphNode** CopiedStartNode = CopiedNodes.FindByPredicate([StartNode](UEdGraphNode* CopiedNode)
				{ return CopiedNode->NodeGuid == StartNode->NodeGuid; });
			checkf(CopiedStartNode != nullptr, TEXT("Group copy failed"));
			TargetGroup.StartNodes.Add(CastChecked<UNiagaraNode>(*CopiedStartNode));
		}

		TargetGroup.GetAllNodesInGroup(TargetGroupNodes);

		// Assign all of the new nodes new ids and mark them as requiring synchronization.
		for (UEdGraphNode* CopiedNode : CopiedNodes)
		{
			FGuid OldId = CopiedNode->NodeGuid;
			CopiedNode->CreateNewGuid();
			OldNodeIdToNewIdMap.Add(OldId, CopiedNode->NodeGuid);
			UNiagaraNode* CopiedNiagaraNode = Cast<UNiagaraNode>(CopiedNode);
			if (CopiedNiagaraNode != nullptr)
			{
				CopiedNiagaraNode->MarkNodeRequiresSynchronization(__FUNCTION__, false);
			}
		}
	}

	TArray<FStackNodeGroup> TargetGroups;
	GetStackNodeGroups(*TargetOutputNode, TargetGroups);

	// The first group is the output node, so to get the group index from module index we need to add 1, but 
	// if a valid index wasn't supplied, than we insert at the end.
	int32 TargetGroupIndex = TargetModuleIndex != INDEX_NONE ? TargetModuleIndex + 1 : TargetGroups.Num() - 1;

	// Remove the source module group from it's stack, and insert the source or copied nodes into the target stack.
	DisconnectStackNodeGroup(SourceGroups[SourceGroupIndex], SourceGroups[SourceGroupIndex - 1], SourceGroups[SourceGroupIndex + 1]);
	if (SourceGraph != TargetGraph)
	{
		// If the graphs were different also remove the nodes from the source graph.
		for (UNiagaraNode* SourceGroupNode : SourceGroupNodes)
		{
			SourceGraph->RemoveNode(SourceGroupNode);
		}
	}

	ConnectStackNodeGroup(TargetGroup, TargetGroups[TargetGroupIndex - 1], TargetGroups[TargetGroupIndex]);

	// Copy any rapid iteration parameters cached earlier into the target script.
	if (SourceFunctionIdToRapidIterationParametersMap.Num() != 0)
	{
		SourceScript.Modify();
		TargetScript->Modify();
		if (SourceGraph == TargetGraph)
		{
			// If the module was dropped in the same graph than neither the emitter or function call name could have changed
			// so we can just add them directly to the target script.
			for (auto It = SourceFunctionIdToRapidIterationParametersMap.CreateIterator(); It; ++It)
			{
				FGuid& FunctionId = It.Key();
				TArray<FNiagaraVariable>& RapidIterationParameters = It.Value();
				for (FNiagaraVariable& RapidIterationParameter : RapidIterationParameters)
				{
					TargetScript->RapidIterationParameters.SetParameterData(RapidIterationParameter.GetData(), RapidIterationParameter, true);
				}
			}
		}
		else
		{
			// If the module was moved to a different graph it's possible that the emitter name or function call name could have
			// changed so we need to construct new rapid iteration parameters.
			FString EmitterName;
			if (TargetEmitterHandleId.IsValid())
			{
				const FNiagaraEmitterHandle* TargetEmitterHandle = TargetSystem.GetEmitterHandles().FindByPredicate(
					[TargetEmitterHandleId](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == TargetEmitterHandleId; });
				EmitterName = TargetEmitterHandle->GetUniqueInstanceName();
			}

			TArray<FNiagaraVariable> TargetRapidIterationParameters;
			for (auto It = SourceFunctionIdToRapidIterationParametersMap.CreateIterator(); It; ++It)
			{
				FGuid& FunctionId = It.Key();
				TArray<FNiagaraVariable>& RapidIterationParameters = It.Value();

				if (SourceGraph == TargetGraph)
				{
					TargetRapidIterationParameters.Append(RapidIterationParameters);
				}
				else
				{
					FGuid TargetNodeId = OldNodeIdToNewIdMap[FunctionId];
					UNiagaraNode** TargetFunctionNodePtr = TargetGroupNodes.FindByPredicate(
						[TargetNodeId](UNiagaraNode* TargetGroupNode) { return TargetGroupNode->NodeGuid == TargetNodeId; });
					checkf(TargetFunctionNodePtr != nullptr, TEXT("Target nodes not copied correctly"));
					UNiagaraNodeFunctionCall* TargetFunctionNode = CastChecked<UNiagaraNodeFunctionCall>(*TargetFunctionNodePtr);
					for (FNiagaraVariable& RapidIterationParameter : RapidIterationParameters)
					{
						FString OldEmitterName;
						FString OldFunctionCallName;
						FString InputName;
						FNiagaraParameterMapHistory::SplitRapidIterationParameterName(RapidIterationParameter, OldEmitterName, OldFunctionCallName, InputName);
						FNiagaraParameterHandle ModuleHandle = FNiagaraParameterHandle::CreateModuleParameterHandle(*InputName);
						FNiagaraParameterHandle AliasedModuleHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, TargetFunctionNode);
						FNiagaraVariable TargetRapidIterationParameter = CreateRapidIterationParameter(EmitterName, TargetUsage, AliasedModuleHandle.GetParameterHandleString(), RapidIterationParameter.GetType());
						TargetScript->RapidIterationParameters.SetParameterData(RapidIterationParameter.GetData(), TargetRapidIterationParameter, true);
					}
				}
			}
		}
	}
}

bool FNiagaraStackGraphUtilities::ParameterAllowedInExecutionCategory(const FName InParameterName, const FName ExecutionCategory)
{
	FNiagaraParameterHandle Handle = FNiagaraParameterHandle(InParameterName);
	if (Handle.IsSystemHandle())
	{
		return ExecutionCategory == UNiagaraStackEntry::FExecutionCategoryNames::System
			|| ExecutionCategory == UNiagaraStackEntry::FExecutionCategoryNames::Emitter
			|| ExecutionCategory == UNiagaraStackEntry::FExecutionCategoryNames::Particle;
	}
	else if (Handle.IsEmitterHandle())
	{
		return ExecutionCategory == UNiagaraStackEntry::FExecutionCategoryNames::Emitter
			|| ExecutionCategory == UNiagaraStackEntry::FExecutionCategoryNames::Particle;
	}
	else if (Handle.IsParticleAttributeHandle())
	{
		return ExecutionCategory == UNiagaraStackEntry::FExecutionCategoryNames::Particle;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

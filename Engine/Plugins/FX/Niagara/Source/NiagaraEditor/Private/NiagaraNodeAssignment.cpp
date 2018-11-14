// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeAssignment.h"
#include "UObject/UnrealType.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScript.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphSchema_Niagara.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "NiagaraComponent.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraConstants.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "ViewModels/Stack/INiagaraStackItemGroupAddUtilities.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeAssigment"

void UNiagaraNodeAssignment::AllocateDefaultPins()
{
	GenerateScript();
	Super::AllocateDefaultPins();
}

FText UNiagaraNodeAssignment::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Set Variables");
}

bool UNiagaraNodeAssignment::RefreshFromExternalChanges()
{
	FunctionScript = nullptr;
	GenerateScript();
	ReallocatePins();
	return true;
}

void UNiagaraNodeAssignment::PostLoad()
{
	Super::PostLoad();

	// Handle the case where we moved towards an array of assignment targets...
	if (AssignmentTarget_DEPRECATED.IsValid() && AssignmentTargets.Num() == 0)
	{
		AssignmentTargets.Add(AssignmentTarget_DEPRECATED);
		AssignmentDefaultValues.Add(AssignmentDefaultValue_DEPRECATED);
		OldFunctionCallName = FunctionDisplayName;
		FunctionDisplayName.Empty();
		RefreshFromExternalChanges();

		UE_LOG(LogNiagaraEditor, Log, TEXT("Found old Assignment Node, converting variable \"%s\" in \"%s\""), *AssignmentTarget_DEPRECATED.GetName().ToString(), *GetFullName());

		MarkNodeRequiresSynchronization(__FUNCTION__, true);
		
		// Deduce what rapid iteration variable we would have previously been and prepare to change
		// any instances of it.
		TMap<FNiagaraVariable, FNiagaraVariable> Converted;
		FNiagaraParameterHandle TargetHandle(AssignmentTarget_DEPRECATED.GetName());
		FString VarNamespace = TargetHandle.GetNamespace().ToString();
		TMap<FString, FString> AliasMap;
		AliasMap.Add(OldFunctionCallName, FunctionDisplayName + TEXT(".") + VarNamespace);
		FNiagaraVariable RemapVar = FNiagaraVariable(AssignmentTarget_DEPRECATED.GetType(), *(OldFunctionCallName + TEXT(".") + TargetHandle.GetName().ToString()));
		FNiagaraVariable NewVar = FNiagaraParameterMapHistory::ResolveAliases(RemapVar, AliasMap, TEXT("."));
		Converted.Add(RemapVar, NewVar);

		bool bConvertedAnything = false;

		// Now clean up the input set node going into us...
		UEdGraphPin* Pin = GetInputPin(0);
		if (Pin != nullptr && Pin->LinkedTo.Num() == 1)
		{
			// Likely we have a set node going into us, check to see if it has any variables that need to be cleaned up.
			UNiagaraNodeParameterMapSet* SetNode = Cast<UNiagaraNodeParameterMapSet>(Pin->LinkedTo[0]->GetOwningNode());
			if (SetNode)
			{
				SetNode->ConditionalPostLoad();

				TArray<UEdGraphPin*> InputPins;
				SetNode->GetInputPins(InputPins);

				const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
				for (UEdGraphPin* InputPin : InputPins)
				{
					FNiagaraVariable Var = NiagaraSchema->PinToNiagaraVariable(InputPin, false);
					if (Var.GetName() == RemapVar.GetName())
					{
						if (NewVar != Var)
						{
							SetNode->SetPinName(InputPin, NewVar.GetName());
							UE_LOG(LogNiagaraEditor, Log, TEXT("Converted Set pin variable \"%s\" to \"%s\" in \"%s\""), *Var.GetName().ToString(), *NewVar.GetName().ToString(), *GetFullName());
							bConvertedAnything = true;
						}
					}
				}
			}
			else if (Pin->LinkedTo[0]->GetOwningNode())
			{
				// Sometimes we don't automatically have set nodes between modules in the stack... just skip over these.
				UE_LOG(LogNiagaraEditor, Log, TEXT("Found node \"%s\" attached to assignment \"%s\" variable %s"), *Pin->LinkedTo[0]->GetOwningNode()->GetFullName(), *GetFullName(), *NewVar.GetName().ToString());
			}
		}

		// Now we need to find the scripts affecting this node... we cheat and walk up our ownership hierarchy until we find a system or emitter.
		if (Converted.Num() != 0)
		{
			UNiagaraEmitter* Emitter = nullptr;
			UNiagaraSystem* System = nullptr;
			UObject* OuterObj = GetOuter();
			while (OuterObj != nullptr)
			{
				if (Emitter == nullptr)
				{
					Emitter = Cast<UNiagaraEmitter>(OuterObj);
				}
				if (System == nullptr)
				{
					System = Cast<UNiagaraSystem>(OuterObj);
				}

				OuterObj = OuterObj->GetOuter();
			}

			// Gather up the affected scripts from the relevant owner...
			TArray<UNiagaraScript*> Scripts;
			if (Emitter)
			{
				Emitter->GetScripts(Scripts, false);
			}
			if (System)
			{
				Scripts.Add(System->GetSystemSpawnScript());
				Scripts.Add(System->GetSystemUpdateScript());
			}

			for (UNiagaraScript* Script : Scripts)
			{
				if (Script->HandleVariableRenames(Converted, Emitter ? Emitter->GetUniqueEmitterName() : FString()))
				{
					bConvertedAnything = true;
				}
			}
		}

		if (!bConvertedAnything)
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("Found old Assignment Node, nothing was attached???? variable \"%s\" in \"%s\""), *AssignmentTarget_DEPRECATED.GetName().ToString(), *GetFullName());
		}
	}
	else
	{
		const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
		if (NiagaraVer < FNiagaraCustomVersion::AssignmentNodeUsesBeginDefaults)
		{
			FunctionScript = nullptr;
			GenerateScript();
		}
		if (NiagaraVer < FNiagaraCustomVersion::AssignmentNodeHasCorrectUsageBitmask)
		{
			if (FunctionScript != nullptr)
			{
				UpdateUsageBitmaskFromOwningScript();
			}
		}
	}
}

void UNiagaraNodeAssignment::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	Super::BuildParameterMapHistory(OutHistory, bRecursive);
}

void UNiagaraNodeAssignment::GenerateScript()
{
	if (FunctionScript == nullptr)
	{
		FunctionScript = NewObject<UNiagaraScript>(this, FName(*(TEXT("SetVariables_") + NodeGuid.ToString())), RF_Transactional);
		FunctionScript->SetUsage(ENiagaraScriptUsage::Module);
		FunctionScript->Description = LOCTEXT("AssignmentNodeDesc", "Sets one or more variables in the stack.");
		InitializeScript(FunctionScript);
		UpdateUsageBitmaskFromOwningScript();
		ComputeNodeName();
	}
}

void UNiagaraNodeAssignment::MergeUp()
{
	//NiagaraStackUtilities::
}

void UNiagaraNodeAssignment::BuildAddParameterMenu(FMenuBuilder& MenuBuilder, ENiagaraScriptUsage InUsage, UNiagaraNodeOutput* InGraphOutputNode)
{
	TArray<FNiagaraVariable> AvailableParameters;
	FNiagaraStackGraphUtilities::GetAvailableParametersForScript(*InGraphOutputNode, AvailableParameters);

	for (const FNiagaraVariable& AvailableParameter : AvailableParameters)
	{
		FString DisplayNameString = FName::NameToDisplayString(AvailableParameter.GetName().ToString(), false);
		const FText NameText = FText::FromString(DisplayNameString);
		FText VarDesc = FNiagaraConstants::GetAttributeDescription(AvailableParameter);
		FString VarDefaultValue = FNiagaraConstants::GetAttributeDefaultValue(AvailableParameter);
		const FText TooltipDesc = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Description: Set the parameter {0}. {1}"), FText::FromName(AvailableParameter.GetName()), VarDesc);
		FText CategoryName = LOCTEXT("ModuleSetCategory", "Set Specific Parameters");
		bool bCanExecute = AssignmentTargets.Contains(AvailableParameter) == false; 

		MenuBuilder.AddMenuEntry(
			NameText,
			TooltipDesc,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateUObject(this, &UNiagaraNodeAssignment::AddParameter, AvailableParameter, VarDefaultValue),
				FCanExecuteAction::CreateLambda([bCanExecute] { return bCanExecute; })));
	}

}

void UNiagaraNodeAssignment::BuildCreateParameterMenu(FMenuBuilder& MenuBuilder, ENiagaraScriptUsage InUsage, UNiagaraNodeOutput* InGraphOutputNode)
{
	// Generate actions for creating new typed parameters.
	TOptional<FName> NewParameterNamespace = FNiagaraStackGraphUtilities::GetNamespaceForScriptUsage(InGraphOutputNode->GetUsage());
	if (NewParameterNamespace.IsSet())
	{
		// Collect all parameter names for ensuring new param has unique name
		TArray<const UNiagaraGraph*> Graphs;
		InGraphOutputNode->GetNiagaraGraph()->GetAllReferencedGraphs(Graphs);
		TSet<FName> Names;
		for (const UNiagaraGraph* Graph : Graphs)
		{
			for (const TPair<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection>& ParameterElement : Graph->GetParameterMap())
			{
				Names.Add(ParameterElement.Key.GetName());
			}
		}

		TArray<FNiagaraTypeDefinition> AvailableTypes;
		FNiagaraStackGraphUtilities::GetNewParameterAvailableTypes(AvailableTypes);
		for (const FNiagaraTypeDefinition& AvailableType : AvailableTypes)
		{
			// Make generic new parameter name
			const FString NewParameterNameString = NewParameterNamespace.GetValue().ToString() + ".New" + AvailableType.GetName();
			const FName NewParameterName = FName(*NewParameterNameString);

			// Make NewParameterName unique  
			const FName UniqueNewParameterName = FNiagaraUtilities::GetUniqueName(NewParameterName, Names);

			// Create the new param
			FNiagaraVariable NewParameter = FNiagaraVariable(AvailableType, UniqueNewParameterName);
			FString VarDefaultValue = FNiagaraConstants::GetAttributeDefaultValue(NewParameter);
			
			// Tooltip and menu entry Text
			FText VarDesc = FNiagaraConstants::GetAttributeDescription(NewParameter);
			const FText TypeText = AvailableType.GetNameText();
			const FText TooltipDesc = FText::Format(LOCTEXT("NewParameterModuleDescriptionFormat", "Description: Create a new {0} parameter. {1}"), TypeText, VarDesc);
			FText Category = LOCTEXT("NewParameterModuleCategory", "Create New Parameter");

			MenuBuilder.AddMenuEntry(
				TypeText,
				TooltipDesc,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UNiagaraNodeAssignment::AddParameter, NewParameter, VarDefaultValue)));
		}
	}
}

void UNiagaraNodeAssignment::AddParameter(FNiagaraVariable InVar, FString InDefaultValue)
{
	const FText TransactionDesc = FText::Format(LOCTEXT("SetFunctionTransactionDesc", "Add the parameter {0}."), FText::FromName(InVar.GetName()));
	FScopedTransaction ScopedTransaction(TransactionDesc);

	// Since we blow away the graph, we need to cache *everything* we create potentially.
	Modify();
	FunctionScript->Modify();
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(FunctionScript->GetSource());
	Source->Modify();
	Source->NodeGraph->Modify();
	for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
	{
		Node->Modify();
	}

	AddAssignmentTarget(InVar, &InDefaultValue);

	RefreshFromExternalChanges();
	MarkNodeRequiresSynchronization(__FUNCTION__, true);
	OnInputsChangedDelegate.Broadcast();
}

void UNiagaraNodeAssignment::RemoveParameter(const FNiagaraVariable& InVar)
{
	const FText TransactionDesc = FText::Format(LOCTEXT("RemoveFunctionTransactionDesc", "Remove the parameter {0}."), FText::FromName(InVar.GetName()));
	FScopedTransaction ScopedTransaction(TransactionDesc);

	// Since we blow away the graph, we need to cache *everything* we create potentially.
	Modify();
	FunctionScript->Modify();
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(FunctionScript->GetSource());
	Source->Modify();
	Source->NodeGraph->Modify();
	for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
	{
		Node->Modify();
	}

	int32 Index = INDEX_NONE;
	if (AssignmentTargets.Find(InVar, Index))
	{
		AssignmentTargets.RemoveAt(Index);
		AssignmentDefaultValues.RemoveAt(Index);
	}

	RefreshFromExternalChanges();
	MarkNodeRequiresSynchronization(__FUNCTION__, true);
	OnInputsChangedDelegate.Broadcast();
}

void UNiagaraNodeAssignment::UpdateUsageBitmaskFromOwningScript()
{
	FunctionScript->ModuleUsageBitmask = CalculateScriptUsageBitmask();
}

void UNiagaraNodeAssignment::InitializeScript(UNiagaraScript* NewScript)
{
	if (NewScript != NULL)
	{		
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(NewScript->GetSource());

		if (nullptr == Source)
		{
			Source = NewObject<UNiagaraScriptSource>(NewScript, NAME_None, RF_Transactional);
			NewScript->SetSource(Source);
		}

		UNiagaraGraph* CreatedGraph = Source->NodeGraph;
		if (nullptr == CreatedGraph)
		{
			CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;
		}
		
		TArray<UNiagaraNodeInput*> InputNodes;
		CreatedGraph->FindInputNodes(InputNodes);

		UNiagaraNodeInput* InputMapInputNode;
		UNiagaraNodeInput** InputMapInputNodePtr = InputNodes.FindByPredicate([](UNiagaraNodeInput* InputNode)
		{ 
			return InputNode->Usage == ENiagaraInputNodeUsage::Parameter && InputNode->Input.GetType() == FNiagaraTypeDefinition::GetParameterMapDef() && InputNode->Input.GetName() == "InputMap"; 
		});
		if (InputMapInputNodePtr == nullptr)
		{
			FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*CreatedGraph);
			InputMapInputNode = InputNodeCreator.CreateNode();
			InputMapInputNode->Input = FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
			InputMapInputNode->Usage = ENiagaraInputNodeUsage::Parameter;
			InputNodeCreator.Finalize();
		}
		else
		{
			InputMapInputNode = *InputMapInputNodePtr;
		}

		UNiagaraNodeInput* BeginDefaultsInputNode;
		UNiagaraNodeInput** BeginDefaultsInputNodePtr = InputNodes.FindByPredicate([](UNiagaraNodeInput* InputNode)
		{
			return InputNode->Usage == ENiagaraInputNodeUsage::TranslatorConstant && InputNode->Input == TRANSLATOR_PARAM_BEGIN_DEFAULTS;
		});
		if (BeginDefaultsInputNodePtr == nullptr)
		{
			FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*CreatedGraph);
			BeginDefaultsInputNode = InputNodeCreator.CreateNode();
			BeginDefaultsInputNode->Input = TRANSLATOR_PARAM_BEGIN_DEFAULTS;
			BeginDefaultsInputNode->Usage = ENiagaraInputNodeUsage::TranslatorConstant;
			BeginDefaultsInputNode->ExposureOptions.bCanAutoBind = true;
			BeginDefaultsInputNode->ExposureOptions.bHidden = true;
			BeginDefaultsInputNode->ExposureOptions.bRequired = false;
			BeginDefaultsInputNode->ExposureOptions.bExposed = false;
			InputNodeCreator.Finalize();
		}
		else
		{
			BeginDefaultsInputNode = *BeginDefaultsInputNodePtr;
		}

		UNiagaraNodeOutput* OutputNode = CreatedGraph->FindOutputNode(ENiagaraScriptUsage::Module);
		if (OutputNode == nullptr)
		{
			FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*CreatedGraph);
			OutputNode = OutputNodeCreator.CreateNode();
			FNiagaraVariable ParamMapAttrib(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("OutputMap"));
			OutputNode->SetUsage(ENiagaraScriptUsage::Module);
			OutputNode->Outputs.Add(ParamMapAttrib);
			OutputNodeCreator.Finalize();
		}

		TArray<UNiagaraNodeParameterMapGet*> GetNodes;
		CreatedGraph->GetNodesOfClass(GetNodes);

		TArray<UNiagaraNodeParameterMapSet*> SetNodes;
		CreatedGraph->GetNodesOfClass(SetNodes);

		if (SetNodes.Num() == 0)
		{
			FGraphNodeCreator<UNiagaraNodeParameterMapSet> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeParameterMapSet* InputNode = InputNodeCreator.CreateNode();
			InputNodeCreator.Finalize();
			SetNodes.Add(InputNode);

			InputMapInputNode->GetOutputPin(0)->MakeLinkTo(SetNodes[0]->GetInputPin(0));
			SetNodes[0]->GetOutputPin(0)->MakeLinkTo(OutputNode->GetInputPin(0));
		}

		// We create two get nodes. The first is for the direct values.
		// The second is in the case of referencing other parameters that we want to use as defaults.
		if (GetNodes.Num() == 0)
		{
			FGraphNodeCreator<UNiagaraNodeParameterMapGet> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeParameterMapGet* InputNode = InputNodeCreator.CreateNode();
			InputNodeCreator.Finalize();
			GetNodes.Add(InputNode);

			InputMapInputNode->GetOutputPin(0)->MakeLinkTo(GetNodes[0]->GetInputPin(0));
		}
		if (GetNodes.Num() == 1)
		{
			FGraphNodeCreator<UNiagaraNodeParameterMapGet> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeParameterMapGet* InputNode = InputNodeCreator.CreateNode();
			InputNodeCreator.Finalize();
			GetNodes.Add(InputNode);

			BeginDefaultsInputNode->GetOutputPin(0)->MakeLinkTo(GetNodes[1]->GetInputPin(0));
		}

		// Clean out existing pins
		while (!SetNodes[0]->IsAddPin(SetNodes[0]->GetInputPin(1)))
		{
			SetNodes[0]->RemovePin(SetNodes[0]->GetInputPin(1));
		}

		while (!GetNodes[0]->IsAddPin(GetNodes[0]->GetOutputPin(0)))
		{
			GetNodes[0]->RemovePin(GetNodes[0]->GetInputPin(0));
		}

		while (!GetNodes[1]->IsAddPin(GetNodes[1]->GetOutputPin(0)))
		{
			GetNodes[1]->RemovePin(GetNodes[1]->GetInputPin(0));
		}

		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();

		for (int32 i = 0; i < AssignmentTargets.Num(); i++)
		{
			// Now create the proper new pins and connect them.
			FName Name = AssignmentTargets[i].GetName();
			FNiagaraTypeDefinition Type = AssignmentTargets[i].GetType();
			FString DefaultValue = AssignmentDefaultValues[i];

			if (Name != NAME_None)
			{
				FNiagaraParameterHandle TargetHandle(Name);
				UEdGraphPin* SetPin = SetNodes[0]->RequestNewTypedPin(EGPD_Input, Type, Name);
				FString ModuleVarName = FString::Printf(TEXT("Module.%s"), *TargetHandle.GetParameterHandleString().ToString());
				UEdGraphPin* GetPin = GetNodes[0]->RequestNewTypedPin(EGPD_Output, Type, *ModuleVarName);
				FNiagaraVariable TargetVar = NiagaraSchema->PinToNiagaraVariable(GetPin, false);
				GetPin->MakeLinkTo(SetPin);

				if (!DefaultValue.IsEmpty())
				{
					UEdGraphPin* DefaultInputPin = GetNodes[0]->GetDefaultPin(GetPin);

					bool isEngineConstant = false;
					FNiagaraVariable SeekVar = FNiagaraVariable(Type, FName(*DefaultValue));
					const FNiagaraVariable* FoundVar = FNiagaraConstants::FindEngineConstant(SeekVar);
					if (FoundVar != nullptr)
					{
						UEdGraphPin* DefaultGetPin = GetNodes[1]->RequestNewTypedPin(EGPD_Output, Type, FoundVar->GetName());
						DefaultGetPin->MakeLinkTo(DefaultInputPin);
					}
					else
					{
						DefaultInputPin->bDefaultValueIsIgnored = false;
						DefaultInputPin->DefaultValue = DefaultValue;
					}
				}

				if (FNiagaraConstants::IsNiagaraConstant(AssignmentTargets[i]))
				{
					const FNiagaraVariableMetaData* FoundMetaData = FNiagaraConstants::GetConstantMetaData(AssignmentTargets[i]);
					if (FoundMetaData)
					{
						FNiagaraVariableMetaData& MetaData = CreatedGraph->FindOrAddMetaData(TargetVar);
						MetaData.Description = FoundMetaData->Description;
						MetaData.ReferencerNodes.Empty();
						MetaData.ReferencerNodes.Add(GetNodes[0]);
					}
				}
			}
		}

		CreatedGraph->PurgeUnreferencedMetaData();
	}
}

int32 UsageToBitmask(ENiagaraScriptUsage Usage)
{
	return 1 << (int32)Usage;
}

int32 UNiagaraNodeAssignment::CalculateScriptUsageBitmask()
{
	int32 UsageBitmask = 0;
	UNiagaraNodeOutput* OutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*this);
	if (OutputNode != nullptr)
	{
		if (UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::SystemSpawnScript) ||
			UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::SystemUpdateScript))
		{
			UsageBitmask =
				UsageToBitmask(ENiagaraScriptUsage::SystemSpawnScript) |
				UsageToBitmask(ENiagaraScriptUsage::SystemUpdateScript);
		}
		if (UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::EmitterSpawnScript) ||
			UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::EmitterUpdateScript))
		{
			UsageBitmask =
				UsageToBitmask(ENiagaraScriptUsage::EmitterSpawnScript) |
				UsageToBitmask(ENiagaraScriptUsage::EmitterUpdateScript);
		}
		if (UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::ParticleSpawnScript) ||
			UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::ParticleUpdateScript) ||
			UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), ENiagaraScriptUsage::ParticleEventScript))
		{
			UsageBitmask =
				UsageToBitmask(ENiagaraScriptUsage::ParticleSpawnScript) |
				UsageToBitmask(ENiagaraScriptUsage::ParticleUpdateScript) |
				UsageToBitmask(ENiagaraScriptUsage::ParticleEventScript);
		}
	}
	return UsageBitmask;
}

int32 UNiagaraNodeAssignment::FindAssignmentTarget(const FName& InName, const FNiagaraTypeDefinition& InType)
{
	for (int32 i = 0; i < AssignmentTargets.Num(); i++)
	{
		FName Name = AssignmentTargets[i].GetName();
		FNiagaraTypeDefinition Type = AssignmentTargets[i].GetType();
	
		if (InName == Name && InType == Type)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

int32 UNiagaraNodeAssignment::FindAssignmentTarget(const FName& InName)
{
	for (int32 i = 0; i < AssignmentTargets.Num(); i++)
	{
		FName Name = AssignmentTargets[i].GetName();
		FNiagaraTypeDefinition Type = AssignmentTargets[i].GetType();

		if (InName == Name)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

int32 UNiagaraNodeAssignment::AddAssignmentTarget(const FNiagaraVariable& InVar, const FString* InDefaultValue)
{
	int32 IdxA = AssignmentTargets.AddDefaulted();
	int32 IdxB = AssignmentDefaultValues.AddDefaulted();
	check(IdxA == IdxB);
	SetAssignmentTarget(IdxA, InVar, InDefaultValue);
	return IdxA;
}

bool UNiagaraNodeAssignment::SetAssignmentTarget(int32 Idx, const FNiagaraVariable& InVar, const FString* InDefaultValue)
{
	check(Idx < AssignmentTargets.Num());

	bool bRetValue = false;
	if (InVar != AssignmentTargets[Idx])
	{
		AssignmentTargets[Idx] = InVar;
		MarkNodeRequiresSynchronization(__FUNCTION__, true);
		bRetValue = true;
	}
	
	if (InDefaultValue != nullptr && AssignmentDefaultValues[Idx] != *InDefaultValue)
	{ 
		AssignmentDefaultValues[Idx] = *InDefaultValue;
		MarkNodeRequiresSynchronization(__FUNCTION__, true);
		bRetValue = true;
	}
	return bRetValue;
}

bool UNiagaraNodeAssignment::SetAssignmentTargetName(int32 Idx, const FName& InName)
{
	check(Idx < AssignmentTargets.Num());
	if (AssignmentTargets[Idx].GetName() != InName)
	{
		AssignmentTargets[Idx].SetName(InName);
		MarkNodeRequiresSynchronization(__FUNCTION__, true);
		return true;
	}
	return false;
}


#undef LOCTEXT_NAMESPACE

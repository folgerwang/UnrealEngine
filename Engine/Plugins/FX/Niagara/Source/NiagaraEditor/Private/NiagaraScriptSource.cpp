// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptSource.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "NiagaraGraph.h"
#include "NiagaraConstants.h"
#include "NiagaraSystem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeWriteDataSet.h"
#include "NiagaraNodeReadDataSet.h"
#include "NiagaraScript.h"
#include "NiagaraDataInterface.h"
#include "GraphEditAction.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_Niagara.h"
#include "Logging/TokenizedMessage.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraCustomVersion.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeEmitter.h"
#include "Logging/LogMacros.h"

DECLARE_CYCLE_STAT(TEXT("Niagara - ScriptSource - Compile"), STAT_NiagaraEditor_ScriptSource_Compile, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("Niagara - ScriptSource - InitializeNewRapidIterationParameters"), STAT_NiagaraEditor_ScriptSource_InitializeNewRapidIterationParameters, STATGROUP_NiagaraEditor);

UNiagaraScriptSource::UNiagaraScriptSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraScriptSource::ComputeVMCompilationId(FNiagaraVMExecutableDataId& Id, ENiagaraScriptUsage InUsage, const FGuid& InUsageId) const
{
	Id.ScriptUsageType = InUsage;
	Id.ScriptUsageTypeID = InUsageId;
	Id.CompilerVersionID = FNiagaraCustomVersion::LatestScriptCompileVersion;
	if (NodeGraph)
	{
		Id.BaseScriptID = NodeGraph->GetCompileID(InUsage, InUsageId);
		NodeGraph->GatherExternalDependencyIDs(InUsage, InUsageId, Id.ReferencedDependencyIds, Id.ReferencedObjects);
	}
}

void UNiagaraScriptSource::InvalidateCachedCompileIds() 
{
	NodeGraph->InvalidateCachedCompileIds();
}


void UNiagaraScriptSource::PostLoad()
{
	Super::PostLoad();

	if (NodeGraph)
	{
		// We need to make sure that the node-graph is already resolved b/c we may be asked IsSyncrhonized later...
		NodeGraph->ConditionalPostLoad();

		// Hook up event handlers so the on changed handler can be called correctly.
		NodeGraph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateUObject(this, &UNiagaraScriptSource::OnGraphChanged));
		NodeGraph->AddOnGraphNeedsRecompileHandler(FOnGraphChanged::FDelegate::CreateUObject(this, &UNiagaraScriptSource::OnGraphChanged));
		NodeGraph->OnDataInterfaceChanged().AddUObject(this, &UNiagaraScriptSource::OnGraphDataInterfaceChanged);
	}
}

UNiagaraScriptSourceBase* UNiagaraScriptSource::MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const
{
	check(GetOuter() != DestOuter);
	EObjectFlags Flags = RF_AllFlags & ~RF_Standalone & ~RF_Public; // Remove Standalone and Public flags.
	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	GetTransientPackage()->LinkerCustomVersion.Empty();

	UNiagaraScriptSource*	ScriptSource = CastChecked<UNiagaraScriptSource>(StaticDuplicateObject(this, GetTransientPackage(), NAME_None, Flags));
	check(ScriptSource->HasAnyFlags(RF_Standalone) == false);
	check(ScriptSource->HasAnyFlags(RF_Public) == false);

	ScriptSource->Rename(nullptr, DestOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	UE_LOG(LogNiagaraEditor, Warning, TEXT("MakeRecursiveDeepCopy %s"), *ScriptSource->GetFullName());
	ExistingConversions.Add(this, ScriptSource);

	ScriptSource->SubsumeExternalDependencies(ExistingConversions);
	return ScriptSource;
}

void UNiagaraScriptSource::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	if (NodeGraph)
	{
		NodeGraph->SubsumeExternalDependencies(ExistingConversions);
	}
}

bool UNiagaraScriptSource::IsSynchronized(const FGuid& InChangeId)
{
	if (NodeGraph)
	{
		return NodeGraph->IsOtherSynchronized(InChangeId);
	}
	else
	{
		return false;
	}
}

void UNiagaraScriptSource::MarkNotSynchronized(FString Reason)
{
	if (NodeGraph)
	{
		NodeGraph->MarkGraphRequiresSynchronization(Reason);
	}
}

void UNiagaraScriptSource::PostLoadFromEmitter(UNiagaraEmitter& OwningEmitter)
{
	const int32 NiagaraCustomVersion = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraCustomVersion < FNiagaraCustomVersion::ScriptsNowUseAGuidForIdentificationInsteadOfAnIndex)
	{
		TArray<UNiagaraNodeOutput*> OutputNodes;
		NodeGraph->GetNodesOfClass<UNiagaraNodeOutput>(OutputNodes);
		for (int32 i = 0; i < OwningEmitter.GetEventHandlers().Num(); i++)
		{
			const FNiagaraEventScriptProperties& EventScriptProperties = OwningEmitter.GetEventHandlers()[i];
			EventScriptProperties.Script->SetUsageId(FGuid::NewGuid());

			auto FindOutputNodeByUsageIndex = [=](UNiagaraNodeOutput* OutputNode) 
			{ 
				return OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleEventScript && OutputNode->ScriptTypeIndex_DEPRECATED == EventScriptProperties.Script->UsageIndex_DEPRECATED; 
			};
			UNiagaraNodeOutput** MatchingOutputNodePtr = OutputNodes.FindByPredicate(FindOutputNodeByUsageIndex);
			if (MatchingOutputNodePtr != nullptr)
			{
				UNiagaraNodeOutput* MatchingOutputNode = *MatchingOutputNodePtr;
				MatchingOutputNode->SetUsageId(EventScriptProperties.Script->GetUsageId());
			}
		}
		NodeGraph->MarkGraphRequiresSynchronization("Modified while handling a change to the niagara custom version.");
	}
}

bool UNiagaraScriptSource::AddModuleIfMissing(FString ModulePath, ENiagaraScriptUsage Usage, bool& bOutFoundModule)
{
	FSoftObjectPath SystemUpdateScriptRef(ModulePath);
	FAssetData ModuleScriptAsset;
	ModuleScriptAsset.ObjectPath = SystemUpdateScriptRef.GetAssetPathName();
	bOutFoundModule = false;

	if (ModuleScriptAsset.IsValid())
	{
		bOutFoundModule = true;
		if (UNiagaraNodeOutput* OutputNode = NodeGraph->FindOutputNode(Usage))
		{
			TArray<UNiagaraNodeFunctionCall*> FoundCalls;
			if (!FNiagaraStackGraphUtilities::FindScriptModulesInStack(ModuleScriptAsset, *OutputNode, FoundCalls))
			{
				FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScriptAsset, *OutputNode);
				return true;
			}
		}
	}

	return false;
}

void InitializeNewRapidIterationParametersForNode(const UEdGraphSchema_Niagara* Schema, UNiagaraNode* Node, const FString& UniqueEmitterName, ENiagaraScriptUsage ScriptUsage, FNiagaraParameterStore& RapidIterationParameters, TSet<FName>& ValidRapidIterationParameterNames)
{
	UNiagaraNodeFunctionCall* FunctionCallNode = Cast<UNiagaraNodeFunctionCall>(Node);
	if (FunctionCallNode != nullptr)
	{
		TArray<const UEdGraphPin*> FunctionInputPins;
		FNiagaraStackGraphUtilities::GetStackFunctionInputPins(*FunctionCallNode, FunctionInputPins, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly, false);
		for (const UEdGraphPin* FunctionInputPin : FunctionInputPins)
		{
			FNiagaraTypeDefinition InputType = Schema->PinToTypeDefinition(FunctionInputPin);
			if (InputType.IsValid() == false)
			{
				UE_LOG(LogNiagaraEditor, Error, TEXT("Invalid input type found while attempting initialize new rapid iteration parameters. Function Node: %s %s Input Name: %s"),
					*FunctionCallNode->GetPathName(), *FunctionCallNode->GetFunctionName(), *FunctionInputPin->GetName());
				continue;
			}

			if (FNiagaraStackGraphUtilities::IsRapidIterationType(InputType))
			{
				FNiagaraParameterHandle AliasedFunctionInputHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(FunctionInputPin->PinName), FunctionCallNode);
				FNiagaraVariable RapidIterationParameter = FNiagaraStackGraphUtilities::CreateRapidIterationParameter(UniqueEmitterName, ScriptUsage, AliasedFunctionInputHandle.GetParameterHandleString(), InputType);
				ValidRapidIterationParameterNames.Add(RapidIterationParameter.GetName());
				int32 ParameterIndex = RapidIterationParameters.IndexOf(RapidIterationParameter);
				// Only set a value for the parameter if it's not already set.
				if (ParameterIndex == INDEX_NONE)
				{
					UEdGraphPin* DefaultPin = FunctionCallNode->FindParameterMapDefaultValuePin(FunctionInputPin->PinName, ScriptUsage);
					// Only set values for inputs which don't have a default wired in the script graph, since inputs with wired defaults can't currently use rapid iteration parameters.
					if (DefaultPin != nullptr && DefaultPin->LinkedTo.Num() == 0)
					{
						// Only set values for inputs without override pins, since and override pin means it's being read from a different value.
						UEdGraphPin* OverridePin = FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin(*FunctionCallNode, AliasedFunctionInputHandle);
						if (OverridePin == nullptr)
						{
							FNiagaraVariable DefaultVariable = Schema->PinToNiagaraVariable(DefaultPin, true);
							check(DefaultVariable.GetData() != nullptr);
							bool bAddParameterIfMissing = true;
							RapidIterationParameters.SetParameterData(DefaultVariable.GetData(), RapidIterationParameter, bAddParameterIfMissing);
						}
					}
				}
			}
		}
	}
}

void UNiagaraScriptSource::CleanUpOldAndInitializeNewRapidIterationParameters(FString UniqueEmitterName, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, FNiagaraParameterStore& RapidIterationParameters) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_ScriptSource_InitializeNewRapidIterationParameters);
	TArray<UNiagaraNodeOutput*> OutputNodes;
	if (ScriptUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		TArray<UNiagaraNodeOutput*> TempOutputNodes;
		NodeGraph->FindOutputNodes(TempOutputNodes);
		for (UNiagaraNodeOutput* OutputNode : TempOutputNodes)
		{
			if (UNiagaraScript::IsParticleScript(OutputNode->GetUsage()))
			{
				OutputNodes.AddUnique(OutputNode);
			}
		}
	}
	else
	{
		UNiagaraNodeOutput* OutputNode = NodeGraph->FindEquivalentOutputNode(ScriptUsage, ScriptUsageId);
		OutputNodes.Add(OutputNode);
	}

	TSet<FName> ValidRapidIterationParameterNames;
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		if (OutputNode != nullptr)
		{
			TArray<UNiagaraNode*> Nodes;
			NodeGraph->BuildTraversal(Nodes, OutputNode);
			const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

			for(UNiagaraNode* Node : Nodes)
			{	
				InitializeNewRapidIterationParametersForNode(Schema, Node, UniqueEmitterName, OutputNode->GetUsage(), RapidIterationParameters, ValidRapidIterationParameterNames);
			}
		}
	}

	// Clean up old rapid iteration parameters.
	TArray<FNiagaraVariable> CurrentRapidIterationVariables;
	RapidIterationParameters.GetParameters(CurrentRapidIterationVariables);
	for (const FNiagaraVariable& CurrentRapidIterationVariable : CurrentRapidIterationVariables)
	{
		if (ValidRapidIterationParameterNames.Contains(CurrentRapidIterationVariable.GetName()) == false)
		{
			RapidIterationParameters.RemoveParameter(CurrentRapidIterationVariable);
		}
	}
}

/*
ENiagaraScriptCompileStatus UNiagaraScriptSource::Compile(UNiagaraScript* ScriptOwner, FString& OutGraphLevelErrorMessages)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_ScriptSource_Compile);
	bool bDoPostCompile = false;
	if (!bIsPrecompiled)
	{
		PreCompile(nullptr, TArray<FNiagaraVariable>());
		bDoPostCompile = true;
	}

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
	ENiagaraScriptCompileStatus Status = NiagaraEditorModule.CompileScript(ScriptOwner, OutGraphLevelErrorMessages);
	check(ScriptOwner != nullptr && IsSynchronized(ScriptOwner->GetChangeID()));
	
	if (bDoPostCompile)
	{
		PostCompile();
	}
	return Status;

// 	FNiagaraConstants& ExternalConsts = ScriptOwner->ConstantData.GetExternalConstants();
// 
// 	//Build the constant list. 
// 	//This is mainly just jumping through some hoops for the custom UI. Should be removed and have the UI just read directly from the constants stored in the UScript.
// 	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(NodeGraph->GetSchema());
// 	ExposedVectorConstants.Empty();
// 	for (int32 ConstIdx = 0; ConstIdx < ExternalConsts.GetNumVectorConstants(); ConstIdx++)
// 	{
// 		FNiagaraVariableInfo Info;
// 		FVector4 Value;
// 		ExternalConsts.GetVectorConstant(ConstIdx, Value, Info);
// 		if (Schema->IsSystemConstant(Info))
// 		{
// 			continue;//System constants are "external" but should not be exposed to the editor.
// 		}
// 			
// 		EditorExposedVectorConstant *Const = new EditorExposedVectorConstant();
// 		Const->ConstName = Info.Name;
// 		Const->Value = Value;
// 		ExposedVectorConstants.Add(MakeShareable(Const));
// 	}

}
*/

void UNiagaraScriptSource::OnGraphChanged(const FEdGraphEditAction &Action)
{
	OnChangedDelegate.Broadcast();
}

void UNiagaraScriptSource::OnGraphDataInterfaceChanged()
{
	OnChangedDelegate.Broadcast();
}

FGuid UNiagaraScriptSource::GetChangeID() 
{ 
	return NodeGraph->GetChangeID(); 
}

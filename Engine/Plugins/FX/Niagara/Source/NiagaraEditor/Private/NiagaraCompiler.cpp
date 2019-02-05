// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraCompiler.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraEditorModule.h"
#include "NiagaraComponent.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IShaderFormat.h"
#include "ShaderFormatVectorVM.h"
#include "NiagaraConstants.h"
#include "NiagaraSystem.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeInput.h"
#include "NiagaraScriptSource.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceStaticMesh.h"
#include "NiagaraDataInterfaceCurlNoise.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeOutput.h"
#include "ShaderCore.h"
#include "EdGraphSchema_Niagara.h"
#include "Misc/FileHelper.h"
#include "ShaderCompiler.h"
#include "NiagaraShader.h"

#define LOCTEXT_NAMESPACE "NiagaraCompiler"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraCompiler, All, All);

DECLARE_CYCLE_STAT(TEXT("Niagara - Module - CompileScript"), STAT_NiagaraEditor_Module_CompileScript, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("Niagara - HlslCompiler - CompileScript"), STAT_NiagaraEditor_HlslCompiler_CompileScript, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("Niagara - HlslCompiler - CompileShader_VectorVM"), STAT_NiagaraEditor_HlslCompiler_CompileShader_VectorVM, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("Niagara - Module - CompileShader_VectorVMSucceeded"), STAT_NiagaraEditor_HlslCompiler_CompileShader_VectorVMSucceeded, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("Niagara - ScriptSource - PreCompile"), STAT_NiagaraEditor_ScriptSource_PreCompile, STATGROUP_NiagaraEditor);
DECLARE_CYCLE_STAT(TEXT("Niagara - HlslCompiler - TestCompileShader_VectorVM"), STAT_NiagaraEditor_HlslCompiler_TestCompileShader_VectorVM, STATGROUP_NiagaraEditor);

static int32 GbForceNiagaraTranslatorSingleThreaded = 1;
static FAutoConsoleVariableRef CVarForceNiagaraTranslatorSingleThreaded(
	TEXT("fx.ForceNiagaraTranslatorSingleThreaded"),
	GbForceNiagaraTranslatorSingleThreaded,
	TEXT("If > 0 all translation will occur one at a time, useful for debugging. \n"),
	ECVF_Default
);


static FCriticalSection TranslationCritSec;

void FNiagaraCompileRequestData::VisitReferencedGraphs(UNiagaraGraph* InSrcGraph, UNiagaraGraph* InDupeGraph, ENiagaraScriptUsage InUsage)
{
	if (!InDupeGraph && !InSrcGraph)
	{
		return;
	}
	FunctionData Data;
	Data.ClonedScript = nullptr;
	Data.ClonedGraph = InDupeGraph;
	Data.Usage = InUsage;
	Data.bHasNumericInputs = false;
	TArray<FunctionData> MadeArray;
	MadeArray.Add(Data);
	PreprocessedFunctions.Add(InSrcGraph, MadeArray);

	bool bStandaloneScript = false;

	TArray<UNiagaraNodeOutput*> OutputNodes;
	InDupeGraph->FindOutputNodes(OutputNodes);
	if (OutputNodes.Num() == 1 && UNiagaraScript::IsStandaloneScript(OutputNodes[0]->GetUsage()))
	{
		bStandaloneScript = true;
	}
	FNiagaraEditorUtilities::ResolveNumerics(InDupeGraph, bStandaloneScript, ChangedFromNumericVars);
	ClonedGraphs.AddUnique(InDupeGraph);

	VisitReferencedGraphsRecursive(InDupeGraph);
}

void FNiagaraCompileRequestData::VisitReferencedGraphsRecursive(UNiagaraGraph* InGraph)
{
	if (!InGraph)
	{
		return;
	}
	UPackage* OwningPackage = InGraph->GetOutermost();

	TArray<UNiagaraNode*> Nodes;
	InGraph->GetNodesOfClass(Nodes);
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNode* InNode = Cast<UNiagaraNode>(Node))
		{
			UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(InNode);
			if (InputNode)
			{
				if (InputNode->Input.IsDataInterface())
				{
					UNiagaraDataInterface* DataInterface = InputNode->GetDataInterface();
					FName DIName = InputNode->Input.GetName();
					UNiagaraDataInterface* Dupe = DuplicateObject<UNiagaraDataInterface>(DataInterface, GetTransientPackage());
					CopiedDataInterfacesByName.Add(DIName, Dupe);
				}
				continue;
			}

			ENiagaraScriptUsage ScriptUsage = ENiagaraScriptUsage::Function;

			UNiagaraNodeFunctionCall* FunctionCallNode = Cast<UNiagaraNodeFunctionCall>(InNode);
			if (FunctionCallNode)
			{
				UNiagaraScript* FunctionScript = FunctionCallNode->FunctionScript;
				ScriptUsage = FunctionCallNode->GetCalledUsage();

				if (FunctionScript != nullptr)
				{
					UNiagaraGraph* FunctionGraph = FunctionCallNode->GetCalledGraph();
					{
						bool bHasNumericParams = FunctionGraph->HasNumericParameters();
						bool bHasNumericInputs = false;

						UPackage* FunctionPackage = FunctionGraph->GetOutermost();
						bool bFromDifferentPackage = OwningPackage != FunctionPackage;

						TArray<UEdGraphPin*> CallOutputs;
						TArray<UEdGraphPin*> CallInputs;
						InNode->GetOutputPins(CallOutputs);
						InNode->GetInputPins(CallInputs);

						TArray<UNiagaraNodeInput*> InputNodes;
						UNiagaraGraph::FFindInputNodeOptions Options;
						Options.bFilterDuplicates = true;
						Options.bIncludeParameters = true;
						Options.bIncludeAttributes = false;
						Options.bIncludeSystemConstants = false;
						Options.bIncludeTranslatorConstants = false;
						FunctionGraph->FindInputNodes(InputNodes, Options);

						for (UNiagaraNodeInput* Input : InputNodes)
						{
							if (Input->Input.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
							{
								bHasNumericInputs = true;
							}
						}

						/*UE_LOG(LogNiagaraEditor, Log, TEXT("FunctionGraph = %p SrcScriptName = %s DiffPackage? %s Numerics? %s NumericInputs? %s"), FunctionGraph, *FunctionGraph->GetPathName(),
							bFromDifferentPackage ? TEXT("yes") : TEXT("no"), bHasNumericParams ? TEXT("yes") : TEXT("no"), bHasNumericInputs ? TEXT("yes") : TEXT("no"));*/

						UNiagaraGraph* ProcessedGraph = nullptr;
						// We only need to clone a non-numeric graph once.

						if (!PreprocessedFunctions.Contains(FunctionGraph))
						{
							UNiagaraScript* DupeScript = nullptr;
							if (!bFromDifferentPackage && !bHasNumericInputs && !bHasNumericParams)
							{
								DupeScript = FunctionScript;
								ProcessedGraph = FunctionGraph;
							}
							else
							{
								DupeScript = DuplicateObject<UNiagaraScript>(FunctionScript, InNode, FunctionScript->GetFName());
								ProcessedGraph = Cast<UNiagaraScriptSource>(DupeScript->GetSource())->NodeGraph;
								FEdGraphUtilities::MergeChildrenGraphsIn(ProcessedGraph, ProcessedGraph, /*bRequireSchemaMatch=*/ true);
								FNiagaraEditorUtilities::PreprocessFunctionGraph(Schema, ProcessedGraph, CallInputs, CallOutputs, ScriptUsage);
								FunctionCallNode->FunctionScript = DupeScript;

								/*UE_LOG(LogNiagaraEditor, Log, TEXT("Duplicating script %s OriginalGraph %p NewGraph %p"), *FunctionScript->GetFName().ToString(),
									Cast<UNiagaraScriptSource>(FunctionScript->GetSource())->NodeGraph,
									Cast<UNiagaraScriptSource>(DupeScript->GetSource())->NodeGraph);*/
							}

							FunctionData Data;
							Data.ClonedScript = DupeScript;
							Data.ClonedGraph = ProcessedGraph;
							Data.CallInputs = CallInputs;
							Data.CallOutputs = CallOutputs;
							Data.Usage = ScriptUsage;
							Data.bHasNumericInputs = bHasNumericInputs;
							TArray<FunctionData> MadeArray;
							MadeArray.Add(Data);
							PreprocessedFunctions.Add(FunctionGraph, MadeArray);
							VisitReferencedGraphsRecursive(ProcessedGraph);		
							ClonedGraphs.AddUnique(ProcessedGraph);

						}
						else if (bHasNumericParams)
						{
							UNiagaraScript* DupeScript = DuplicateObject<UNiagaraScript>(FunctionScript, InNode, FunctionScript->GetFName());
							ProcessedGraph = Cast<UNiagaraScriptSource>(DupeScript->GetSource())->NodeGraph;
							FEdGraphUtilities::MergeChildrenGraphsIn(ProcessedGraph, ProcessedGraph, /*bRequireSchemaMatch=*/ true);
							FNiagaraEditorUtilities::PreprocessFunctionGraph(Schema, ProcessedGraph, CallInputs, CallOutputs, ScriptUsage);
							FunctionCallNode->FunctionScript = DupeScript;

							/*UE_LOG(LogNiagaraEditor, Log, TEXT("Duplicating script %s OriginalGraph %p NewGraph %p"), *FunctionScript->GetFName().ToString(),
								Cast<UNiagaraScriptSource>(FunctionScript->GetSource())->NodeGraph,
								Cast<UNiagaraScriptSource>(DupeScript->GetSource())->NodeGraph);*/

							TArray<FunctionData>* FoundArray = PreprocessedFunctions.Find(FunctionGraph);
							ClonedGraphs.AddUnique(ProcessedGraph);

							FunctionData Data;
							Data.ClonedScript = DupeScript;
							Data.ClonedGraph = ProcessedGraph;
							Data.CallInputs = CallInputs;
							Data.CallOutputs = CallOutputs;
							Data.Usage = ScriptUsage;
							Data.bHasNumericInputs = bHasNumericInputs;

							FoundArray->Add(Data);
							VisitReferencedGraphsRecursive(ProcessedGraph);
						}
						else if (bFromDifferentPackage)
						{
							TArray<FunctionData>* FoundArray = PreprocessedFunctions.Find(FunctionGraph);
							check(FoundArray != nullptr && FoundArray->Num() != 0);
							FunctionCallNode->FunctionScript = (*FoundArray)[0].ClonedScript;
						}
					}
				}
			}

			UNiagaraNodeEmitter* EmitterNode = Cast<UNiagaraNodeEmitter>(InNode);
			if (EmitterNode)
			{
				for (TSharedPtr<FNiagaraCompileRequestData, ESPMode::ThreadSafe>& Ptr : EmitterData)
				{
					if (Ptr->EmitterUniqueName == EmitterNode->GetEmitterUniqueName())
					{
						EmitterNode->SetOwnerSystem(nullptr);
						EmitterNode->SetCachedVariablesForCompilation(*Ptr->EmitterUniqueName, Ptr->NodeGraphDeepCopy, Ptr->Source);
					}
				}
			}
		}
	}
}

const TMap<FName, UNiagaraDataInterface*>& FNiagaraCompileRequestData::GetObjectNameMap()
{
	return CopiedDataInterfacesByName;
}

void FNiagaraCompileRequestData::MergeInEmitterPrecompiledData(FNiagaraCompileRequestDataBase* InEmitterDataBase)
{
//	check(EmitterData.Contains(InEmitterDataBase));

	FNiagaraCompileRequestData* InEmitterData = (FNiagaraCompileRequestData*)InEmitterDataBase;
	if (InEmitterData)
	{
		{
			auto It = InEmitterData->CopiedDataInterfacesByName.CreateIterator();
			while (It)
			{
				FName Name = It.Key();
				Name = FNiagaraParameterMapHistory::ResolveEmitterAlias(Name, InEmitterData->GetUniqueEmitterName());
				CopiedDataInterfacesByName.Add(Name, It.Value());
				++It;
			}
		}
	}
}

FName FNiagaraCompileRequestData::ResolveEmitterAlias(FName VariableName) const
{
	return FNiagaraParameterMapHistory::ResolveEmitterAlias(VariableName, EmitterUniqueName);
}

void FNiagaraCompileRequestData::GetReferencedObjects(TArray<UObject*>& Objects)
{
	Objects.Add(NodeGraphDeepCopy);
	TArray<UNiagaraDataInterface*> DIs;
	CopiedDataInterfacesByName.GenerateValueArray(DIs);
	for (UNiagaraDataInterface* DI : DIs)
	{
		Objects.Add(DI);
	}

	{
		auto Iter = CDOs.CreateIterator();
		while (Iter)
		{
			Objects.Add(Iter.Value());
			++Iter;
		}
	}

	{
		auto Iter = PreprocessedFunctions.CreateIterator();
		while (Iter)
		{
			for (int32 i = 0; i < Iter.Value().Num(); i++)
			{
				Objects.Add(Iter.Value()[i].ClonedScript);
				Objects.Add(Iter.Value()[i].ClonedGraph);
			}
			++Iter;
		}
	}
}

bool FNiagaraCompileRequestData::GatherPreCompiledVariables(const FString& InNamespaceFilter, TArray<FNiagaraVariable>& OutVars)
{
	if (PrecompiledHistories.Num() == 0)
	{
		return false;
	}

	for (const FNiagaraParameterMapHistory& History : PrecompiledHistories)
	{
		for (const FNiagaraVariable& Var : History.Variables)
		{
			if (FNiagaraParameterMapHistory::IsInNamespace(Var, InNamespaceFilter))
			{
				FNiagaraVariable NewVar = Var;
				if (NewVar.IsDataAllocated() == false && !Var.IsDataInterface())
				{
					FNiagaraEditorUtilities::ResetVariableToDefaultValue(NewVar);
				}
				OutVars.AddUnique(NewVar);
			}
		}
	}
	return true;
}

void FNiagaraCompileRequestData::DeepCopyGraphs(UNiagaraScriptSource* ScriptSource, ENiagaraScriptUsage InUsage)
{
	// Clone the source graph so we can modify it as needed; merging in the child graphs
	//NodeGraphDeepCopy = CastChecked<UNiagaraGraph>(FEdGraphUtilities::CloneGraph(ScriptSource->NodeGraph, GetTransientPackage(), 0));
	Source = DuplicateObject<UNiagaraScriptSource>(ScriptSource, GetTransientPackage());
	NodeGraphDeepCopy = Source->NodeGraph;
	FEdGraphUtilities::MergeChildrenGraphsIn(NodeGraphDeepCopy, NodeGraphDeepCopy, /*bRequireSchemaMatch=*/ true);
	VisitReferencedGraphs(ScriptSource->NodeGraph, NodeGraphDeepCopy, InUsage /*InEmitter ? ENiagaraScriptUsage::EmitterSpawnScript : ENiagaraScriptUsage::SystemSpawnScript*/);
}


void FNiagaraCompileRequestData::FinishPrecompile(UNiagaraScriptSource* ScriptSource, const TArray<FNiagaraVariable>& EncounterableVariables, ENiagaraScriptUsage InUsage)
{
	{
		ENiagaraScriptCompileStatusEnum = StaticEnum<ENiagaraScriptCompileStatus>();
		ENiagaraScriptUsageEnum = StaticEnum<ENiagaraScriptUsage>();

		PrecompiledHistories.Empty();

		TArray<UNiagaraNodeOutput*> OutputNodes;
		NodeGraphDeepCopy->FindOutputNodes(OutputNodes);
		PrecompiledHistories.Empty();

		for (UNiagaraNodeOutput* FoundOutputNode : OutputNodes)
		{
			// Map all for this output node
			FNiagaraParameterMapHistoryBuilder Builder;
			Builder.RegisterEncounterableVariables(EncounterableVariables);

			FString TranslationName = TEXT("Emitter");
			Builder.BeginTranslation(TranslationName);
			Builder.EnableScriptWhitelist(true, FoundOutputNode->GetUsage());
			Builder.BuildParameterMaps(FoundOutputNode, true);
			TArray<FNiagaraParameterMapHistory> Histories = Builder.Histories;
			ensure(Histories.Num() <= 1);

			for (FNiagaraParameterMapHistory& History : Histories)
			{
				for (FNiagaraVariable& Var : History.Variables)
				{
					check(Var.GetType() != FNiagaraTypeDefinition::GetGenericNumericDef());
				}
			}

			PrecompiledHistories.Append(Histories);
			Builder.EndTranslation(TranslationName);
		}

		// Generate CDO's for any referenced data interfaces...
		for (int32 i = 0; i < PrecompiledHistories.Num(); i++)
		{
			for (const FNiagaraVariable& Var : PrecompiledHistories[i].Variables)
			{
				if (Var.IsDataInterface())
				{
					UClass* Class = const_cast<UClass*>(Var.GetType().GetClass());
					UObject* Obj = DuplicateObject(Class->GetDefaultObject(true), GetTransientPackage());
					CDOs.Add(Class, Obj);
				}
			}
		}

		// Generate CDO's for data interfaces that are passed in to function or dynamic input scripts compiled standalone as we do not have a history
		if (InUsage == ENiagaraScriptUsage::Function || InUsage == ENiagaraScriptUsage::DynamicInput)
		{
			for (const auto ReferencedGraph : ClonedGraphs)
			{
				TArray<UNiagaraNodeInput*> InputNodes;
				TArray<FNiagaraVariable*> InputVariables;
				ReferencedGraph->FindInputNodes(InputNodes);
				for (const auto InputNode : InputNodes)
				{
					InputVariables.Add(&InputNode->Input);
				}

				for (const auto InputVariable : InputVariables)
				{
					if (InputVariable->IsDataInterface())
					{
						UClass* Class = const_cast<UClass*>(InputVariable->GetType().GetClass());
						UObject* Obj = DuplicateObject(Class->GetDefaultObject(true), GetTransientPackage());
						CDOs.Add(Class, Obj);
					}
				}
			}
		}

	}
}

TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> FNiagaraEditorModule::Precompile(UObject* InObj)
{
	UNiagaraScript* Script = Cast<UNiagaraScript>(InObj);
	UNiagaraSystem* System = Cast<UNiagaraSystem>(InObj);

	if (!Script && !System)
	{
		TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> InvalidPtr;
		return InvalidPtr;
	}

	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_ScriptSource_PreCompile);
	double StartTime = FPlatformTime::Seconds();

	TSharedPtr<FNiagaraCompileRequestData, ESPMode::ThreadSafe> BasePtr = MakeShared<FNiagaraCompileRequestData, ESPMode::ThreadSafe>();
	TArray<TSharedPtr<FNiagaraCompileRequestData, ESPMode::ThreadSafe>> DependentRequests;

	BasePtr->SourceName = InObj->GetName();

	if (Script)
	{
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetSource());
		BasePtr->DeepCopyGraphs(Source, Script->GetUsage());
		const TArray<FNiagaraVariable> EncounterableVariables;
		BasePtr->FinishPrecompile(Source, EncounterableVariables, Script->GetUsage());
	}
	else if (System)
	{
		// Store off the current variables in the exposed parameters list.
		TArray<FNiagaraVariable> OriginalExposedParams;
		System->GetExposedParameters().GetParameters(OriginalExposedParams);

		// Create an array of variables that we might encounter when traversing the graphs (include the originally exposed vars above)
		TArray<FNiagaraVariable> EncounterableVars(OriginalExposedParams);

		// Create an array of variables that we *did* encounter when traversing the graphs.
		TArray<FNiagaraVariable> EncounteredExposedVars;
		check(System->GetSystemSpawnScript()->GetSource() == System->GetSystemUpdateScript()->GetSource());

		// First deep copy all the emitter graphs referenced by the system so that we can later hook up emitter handles in the system traversal.
		BasePtr->EmitterData.Empty();
		for (int32 i = 0; i < System->GetEmitterHandles().Num(); i++)
		{
			const FNiagaraEmitterHandle& Handle = System->GetEmitterHandle(i);
			TSharedPtr<FNiagaraCompileRequestData, ESPMode::ThreadSafe> EmitterPtr = MakeShared<FNiagaraCompileRequestData, ESPMode::ThreadSafe>();
			EmitterPtr->DeepCopyGraphs(Cast<UNiagaraScriptSource>(Handle.GetInstance()->GraphSource), ENiagaraScriptUsage::EmitterSpawnScript);
			EmitterPtr->EmitterUniqueName = Handle.GetInstance()->GetUniqueEmitterName();
			EmitterPtr->SourceName = BasePtr->SourceName;
			BasePtr->EmitterData.Add(EmitterPtr);
		}

		// Now deep copy the system graphs, skipping traversal into any emitter references.
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(System->GetSystemSpawnScript()->GetSource());
		BasePtr->DeepCopyGraphs(Source, ENiagaraScriptUsage::SystemSpawnScript);
		BasePtr->FinishPrecompile(Source, EncounterableVars, ENiagaraScriptUsage::SystemSpawnScript);

		// Add the User and System variables that we did encounter to the list that emitters might also encounter.
		BasePtr->GatherPreCompiledVariables(TEXT("User"), EncounterableVars);
		BasePtr->GatherPreCompiledVariables(TEXT("System"), EncounterableVars);

		// Now we can finish off the emitters.
		for (int32 i = 0; i < System->GetEmitterHandles().Num(); i++)
		{
			const FNiagaraEmitterHandle& Handle = System->GetEmitterHandle(i);
			BasePtr->EmitterData[i]->FinishPrecompile(Cast<UNiagaraScriptSource>(Handle.GetInstance()->GraphSource), EncounterableVars, ENiagaraScriptUsage::EmitterSpawnScript);
			BasePtr->MergeInEmitterPrecompiledData(BasePtr->EmitterData[i].Get());
		}

	}

	UE_LOG(LogNiagaraEditor, Log, TEXT("'%s' Precompile took %f sec."), *InObj->GetOutermost()->GetName(),
		(float)(FPlatformTime::Seconds() - StartTime));

	return BasePtr;
}

TSharedPtr<FNiagaraVMExecutableData> FNiagaraEditorModule::CompileScript(const FNiagaraCompileRequestDataBase* InCompileRequest, const FNiagaraCompileOptions& InCompileOptions)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_Module_CompileScript);

	double StartTime = FPlatformTime::Seconds();

	check(InCompileRequest != NULL);
	const FNiagaraCompileRequestData* CompileRequest = (const FNiagaraCompileRequestData*)InCompileRequest;

	UE_LOG(LogNiagaraEditor, Log, TEXT("Compiling System %s ..................................................................."), *InCompileOptions.FullName);

	FNiagaraCompileResults Results;
	FHlslNiagaraCompiler Compiler;
	FNiagaraTranslateResults TranslateResults;
	FHlslNiagaraTranslator Translator;

	float TranslationTime = 0.0f;
	float VMCompilationTime = 0.0f;
	{
		FHlslNiagaraTranslatorOptions TranslateOptions;

		if (InCompileOptions.TargetUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
		{
			TranslateOptions.SimTarget = ENiagaraSimTarget::GPUComputeSim;
		}
		else
		{
			TranslateOptions.SimTarget = ENiagaraSimTarget::CPUSim;
		}

		double TranslationStartTime = FPlatformTime::Seconds();
		if (GbForceNiagaraTranslatorSingleThreaded > 0)
		{
			FScopeLock Lock(&TranslationCritSec);
			TranslateResults = Translator.Translate(CompileRequest, InCompileOptions, TranslateOptions);
		}
		else
		{
			TranslateResults = Translator.Translate(CompileRequest, InCompileOptions, TranslateOptions);
		}
		TranslationTime = (float)(FPlatformTime::Seconds() - TranslationStartTime);

		Results = Compiler.CompileScript(CompileRequest, InCompileOptions, &Translator.GetTranslateOutput(), Translator.GetTranslatedHLSL());
		VMCompilationTime = Results.CompileTime;
	}
	
	TArray<FNiagaraCompileEvent> Messages;
	if (TranslateResults.CompileEvents.Num() > 0)
	{
		Messages.Append(TranslateResults.CompileEvents);
	}
	if (Results.CompileEvents.Num() > 0)
	{
		Messages.Append(Results.CompileEvents);
	}

	FString OutGraphLevelErrorMessages;
	for (const FNiagaraCompileEvent& Message : Messages)
	{
		if (Message.Type == FNiagaraCompileEventType::Log)
		{
		#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
			UE_LOG(LogNiagaraCompiler, Log, TEXT("%s"), *Message.Message);
		#endif
		}
		else if (Message.Type == FNiagaraCompileEventType::Warning )
		{
		#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
			UE_LOG(LogNiagaraCompiler, Warning, TEXT("%s"), *Message.Message);
		#endif
		}
		else if (Message.Type == FNiagaraCompileEventType::Error)
		{
		#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
			UE_LOG(LogNiagaraCompiler, Error, TEXT("%s"), *Message.Message);
		#endif
			// Write the error messages to the string as well so that they can be echoed up the chain.
			if (OutGraphLevelErrorMessages.Len() > 0)
			{
				OutGraphLevelErrorMessages += "\n";
			}
			OutGraphLevelErrorMessages += Message.Message;
		}
	}
	
	Results.Data->ErrorMsg = OutGraphLevelErrorMessages;
	Results.Data->LastCompileStatus = (FNiagaraCompileResults::CompileResultsToSummary(&Results));
	
	UEnum* FoundEnum = CompileRequest->ENiagaraScriptCompileStatusEnum;
	FString ResultsEnum = TEXT("??");
	if (FoundEnum)
	{
		ResultsEnum = FoundEnum->GetNameStringByValue((int64)Results.Data->LastCompileStatus);
	}
	Results.Data->CompileTime = (float)(FPlatformTime::Seconds() - StartTime);

	UE_LOG(LogNiagaraEditor, Log, TEXT("Compiling System %s took %f sec (%f/%f)... Status %s"), *InCompileOptions.FullName, Results.Data->CompileTime,
		TranslationTime, VMCompilationTime, *ResultsEnum);
	return Results.Data;
}


void FNiagaraEditorModule::TestCompileScriptFromConsole(const TArray<FString>& Arguments)
{
	if (Arguments.Num() == 1)
	{
		FString TranslatedHLSL;
		FFileHelper::LoadFileToString(TranslatedHLSL, *Arguments[0]);
		if (TranslatedHLSL.Len() != 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_HlslCompiler_TestCompileShader_VectorVM);
			FShaderCompilerInput Input;
			Input.VirtualSourceFilePath = TEXT("/Engine/Private/NiagaraEmitterInstanceShader.usf");
			Input.EntryPointName = TEXT("SimulateMain");
			Input.Environment.SetDefine(TEXT("VM_SIMULATION"), 1);
			Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/NiagaraEmitterInstance.ush"), TranslatedHLSL);

			FShaderCompilerOutput Output;
			FVectorVMCompilationOutput CompilationOutput;
			double StartTime = FPlatformTime::Seconds();
			bool bSucceeded = CompileShader_VectorVM(Input, Output, FString(FPlatformProcess::ShaderDir()), 0, CompilationOutput, GNiagaraSkipVectorVMBackendOptimizations);
			float DeltaTime = (float)(FPlatformTime::Seconds() - StartTime);

			if (bSucceeded)
			{
				UE_LOG(LogNiagaraCompiler, Log, TEXT("Test compile of %s took %f seconds and succeeded."), *Arguments[0], DeltaTime);
			}
			else
			{
				UE_LOG(LogNiagaraCompiler, Error, TEXT("Test compile of %s took %f seconds and failed.  Errors: %s"), *Arguments[0], DeltaTime, *CompilationOutput.Errors);
			}
		}
		else
		{
			UE_LOG(LogNiagaraCompiler, Error, TEXT("Test compile of %s failed, the file could not be loaded or it was empty."), *Arguments[0]);
		}
	}
	else
	{
		UE_LOG(LogNiagaraCompiler, Error, TEXT("Test compile failed, file name argument was missing."));
	}
}


ENiagaraScriptCompileStatus FNiagaraCompileResults::CompileResultsToSummary(const FNiagaraCompileResults* CompileResults)
{
	ENiagaraScriptCompileStatus SummaryStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
	if (CompileResults != nullptr)
	{
		if (CompileResults->NumErrors > 0)
		{
			SummaryStatus = ENiagaraScriptCompileStatus::NCS_Error;
		}
		else
		{
			if (CompileResults->bVMSucceeded)
			{
				if (CompileResults->NumWarnings)
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
				}
				else
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
				}
			}

			if (CompileResults->bComputeSucceeded)
			{
				if (CompileResults->NumWarnings)
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;
				}
				else
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
				}
			}
		}
	}
	return SummaryStatus;
}

FNiagaraCompileResults FHlslNiagaraCompiler::CompileScript(const FNiagaraCompileRequestData* InCompileRequest, const FNiagaraCompileOptions& InOptions, FNiagaraTranslatorOutput *TranslatorOutput, FString &TranslatedHLSL)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_HlslCompiler_CompileScript);

	CompileResults.Data = MakeShared<FNiagaraVMExecutableData>();

	//TODO: This should probably be done via the same route that other shaders take through the shader compiler etc.
	//But that adds the complexity of a new shader type, new shader class and a new shader map to contain them etc.
	//Can do things simply for now.
	
	CompileResults.Data->LastHlslTranslation = TEXT("");

	FShaderCompilerInput Input;
	Input.VirtualSourceFilePath = TEXT("/Engine/Private/NiagaraEmitterInstanceShader.usf");
	Input.EntryPointName = TEXT("SimulateMain");
	Input.Environment.SetDefine(TEXT("VM_SIMULATION"), 1);
	Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/NiagaraEmitterInstance.ush"), TranslatedHLSL);
	Input.bGenerateDirectCompileFile = false;
	Input.DumpDebugInfoRootPath = GShaderCompilingManager->GetAbsoluteShaderDebugInfoDirectory() / TEXT("VM");
	FString UsageIdStr = !InOptions.TargetUsageId.IsValid() ? TEXT("") : (TEXT("_") + InOptions.TargetUsageId.ToString());
	Input.DebugGroupName = InCompileRequest->SourceName / InCompileRequest->EmitterUniqueName / InCompileRequest->ENiagaraScriptUsageEnum->GetNameStringByValue((int64)InOptions.TargetUsage) + UsageIdStr;
	Input.DumpDebugInfoPath = Input.DumpDebugInfoRootPath / Input.DebugGroupName;

	if (GShaderCompilingManager->GetDumpShaderDebugInfo())
	{
		// Sanitize the name to be used as a path
		// List mostly comes from set of characters not allowed by windows in a path.  Just try to rename a file and type one of these for the list.
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("<"), TEXT("("));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT(">"), TEXT(")"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("::"), TEXT("=="));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("|"), TEXT("_"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("*"), TEXT("-"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("?"), TEXT("!"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("\""), TEXT("\'"));

		if (!IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath))
		{
			verifyf(IFileManager::Get().MakeDirectory(*Input.DumpDebugInfoPath, true), TEXT("Failed to create directory for shader debug info '%s'"), *Input.DumpDebugInfoPath);
		}
	}

	bool bGPUScript = false;
	if (InOptions.TargetUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		bGPUScript = true;
		CompileResults.bComputeSucceeded = false;
		if (TranslatorOutput != nullptr && TranslatorOutput->Errors.Len() > 0)
		{
			Error(FText::Format(LOCTEXT("HlslTranslateErrorMessageFormat", "The HLSL Translator failed.  Errors:\n{0}"), FText::FromString(TranslatorOutput->Errors)));
			CompileResults.bVMSucceeded = false;
		}
		else if (TranslatedHLSL.Len() == 0)
		{
			Error(LOCTEXT("HlslTranslateErrorMessageFailed", "The HLSL Translator failed to generate HLSL!"));
			CompileResults.bVMSucceeded = false;
		}
		else
		{
			*(CompileResults.Data) = TranslatorOutput->ScriptData;
			CompileResults.Data->ByteCode.Empty();
			CompileResults.bComputeSucceeded = true;
		}
		CompileResults.Data->LastHlslTranslationGPU = TranslatedHLSL;
	}
	else
	{
		FShaderCompilerOutput Output;

		FVectorVMCompilationOutput CompilationOutput;

		if (TranslatorOutput != nullptr && TranslatorOutput->Errors.Len() > 0)
		{
			//TODO: Map Lines of HLSL to their source Nodes and flag those nodes with errors associated with their lines.
			Error(FText::Format(LOCTEXT("HlslTranslateErrorMessageFormat", "The HLSL Translator failed.  Errors:\n{0}"), FText::FromString(TranslatorOutput->Errors)));
			CompileResults.bVMSucceeded = false;
		}
		else if (TranslatedHLSL.Len() == 0)
		{
			Error(LOCTEXT("HlslTranslateErrorMessageFailed", "The HLSL Translator failed to generate HLSL!"));
			CompileResults.bVMSucceeded = false;
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_HlslCompiler_CompileShader_VectorVM);
			static FCriticalSection CritSec;
			
			CritSec.Lock();
			double StartTime = FPlatformTime::Seconds();
			CompileResults.bVMSucceeded = CompileShader_VectorVM(Input, Output, FString(FPlatformProcess::ShaderDir()), 0, CompilationOutput, GNiagaraSkipVectorVMBackendOptimizations);
			CompileResults.CompileTime = (float)(FPlatformTime::Seconds() - StartTime);
			CritSec.Unlock();
		}

		if (CompilationOutput.Errors.Len() > 0)
		{
			//TODO: Map Lines of HLSL to their source Nodes and flag those nodes with errors associated with their lines.
			Error(FText::Format(LOCTEXT("VectorVMCompileErrorMessageFormat", "The Vector VM compile failed.  Errors:\n{0}"), FText::FromString(CompilationOutput.Errors)));
			CompileResults.bVMSucceeded = false;
		}

		//For now we just copy the shader code over into the script. 
		//Eventually Niagara will have all the shader plumbing and do things like materials.
		if (CompileResults.bVMSucceeded)
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_HlslCompiler_CompileShader_VectorVMSucceeded);
			check(TranslatorOutput);
			*CompileResults.Data.Get() = TranslatorOutput->ScriptData;
			CompileResults.Data->ByteCode = CompilationOutput.ByteCode;
			CompileResults.Data->LastAssemblyTranslation = CompilationOutput.AssemblyAsString;
			CompileResults.Data->LastOpCount = CompilationOutput.NumOps;
			//Build internal parameters
			CompileResults.Data->InternalParameters.Empty();
			for (int32 i = 0; i < CompilationOutput.InternalConstantOffsets.Num(); ++i)
			{
				EVectorVMBaseTypes Type = CompilationOutput.InternalConstantTypes[i];
				int32 Offset = CompilationOutput.InternalConstantOffsets[i];
				switch (Type)
				{
				case EVectorVMBaseTypes::Float:
				{
					float Val = *(float*)(CompilationOutput.InternalConstantData.GetData() + Offset);
					CompileResults.Data->InternalParameters.SetOrAdd(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *LexToString(Val)))->SetValue(Val);
				}
				break;
				case EVectorVMBaseTypes::Int:
				{
					int32 Val = *(int32*)(CompilationOutput.InternalConstantData.GetData() + Offset);
					CompileResults.Data->InternalParameters.SetOrAdd(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), *LexToString(Val)))->SetValue(Val);
				}
				break;
				case EVectorVMBaseTypes::Bool:
				{
					int32 Val = *(int32*)(CompilationOutput.InternalConstantData.GetData() + Offset);
					CompileResults.Data->InternalParameters.SetOrAdd(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), Val == 0 ? TEXT("FALSE") : TEXT("TRUE")))->SetValue(Val);
				}
				break;
				}
			}

			//Extract the external function call table binding info.
			CompileResults.Data->CalledVMExternalFunctions.Empty(CompilationOutput.CalledVMFunctionTable.Num());
			for (FVectorVMCompilationOutput::FCalledVMFunction& FuncInfo : CompilationOutput.CalledVMFunctionTable)
			{
				//Find the interface corresponding to this call.
				const FNiagaraFunctionSignature* Sig = nullptr;
				for (FNiagaraScriptDataInterfaceCompileInfo& NDIInfo : TranslatorOutput->ScriptData.DataInterfaceInfo)
				{
					Sig = NDIInfo.RegisteredFunctions.FindByPredicate([&](const FNiagaraFunctionSignature& CheckSig)
					{
						FString SigSymbol = FHlslNiagaraTranslator::GetFunctionSignatureSymbol(CheckSig);
						return SigSymbol == FuncInfo.Name;
					});
					if (Sig)
					{
						break;
					}
				}

				if (Sig)
				{
					int32 NewBindingIdx = CompileResults.Data->CalledVMExternalFunctions.AddDefaulted();
					CompileResults.Data->CalledVMExternalFunctions[NewBindingIdx].Name = *Sig->GetName();
					CompileResults.Data->CalledVMExternalFunctions[NewBindingIdx].OwnerName = Sig->OwnerName;

					CompileResults.Data->CalledVMExternalFunctions[NewBindingIdx].InputParamLocations = FuncInfo.InputParamLocations;
					CompileResults.Data->CalledVMExternalFunctions[NewBindingIdx].NumOutputs = FuncInfo.NumOutputs;
				}
				else
				{
					Error(FText::Format(LOCTEXT("VectorVMExternalFunctionBindingError", "Failed to bind the exernal function call:  {0}"), FText::FromString(FuncInfo.Name)));
					CompileResults.bVMSucceeded = false;
				}
			}
		}

		CompileResults.Data->LastHlslTranslation = TranslatedHLSL;

		if (CompileResults.bVMSucceeded == false)
		{
			//Some error. Clear script and exit.
			CompileResults.Data->ByteCode.Empty();
			CompileResults.Data->Attributes.Empty();
			CompileResults.Data->Parameters.Empty();
			CompileResults.Data->InternalParameters.Empty();
			CompileResults.Data->DataInterfaceInfo.Empty();
			//		Script->NumUserPtrs = 0;
		}
	}

	if (GShaderCompilingManager->GetDumpShaderDebugInfo() && CompileResults.Data.IsValid())
	{
		FString ExportText = CompileResults.Data->LastHlslTranslation;
		FString ExportTextAsm = CompileResults.Data->LastAssemblyTranslation;
		if (bGPUScript)
		{
			ExportText = CompileResults.Data->LastHlslTranslationGPU;
			ExportTextAsm = "";
		}
		FString ExportTextParams;
		for (const FNiagaraVariable& Var : CompileResults.Data->Parameters.Parameters)
		{
			ExportTextParams += Var.ToString();
			ExportTextParams += "\n";
		}
		
		FNiagaraEditorUtilities::WriteTextFileToDisk(Input.DumpDebugInfoPath, TEXT("NiagaraEmitterInstance.ush"), ExportText, true);
		FNiagaraEditorUtilities::WriteTextFileToDisk(Input.DumpDebugInfoPath, TEXT("NiagaraEmitterInstance.asm"), ExportTextAsm, true);
		FNiagaraEditorUtilities::WriteTextFileToDisk(Input.DumpDebugInfoPath, TEXT("NiagaraEmitterInstance.params"), ExportTextParams, true);
	}
	return CompileResults;
}


FHlslNiagaraCompiler::FHlslNiagaraCompiler()
	: CompileResults()
{
}



void FHlslNiagaraCompiler::Error(FText ErrorText)
{
	FString ErrorString = FString::Printf(TEXT("%s"), *ErrorText.ToString());
	CompileResults.CompileEvents.Add(FNiagaraCompileEvent(FNiagaraCompileEventType::Error, ErrorString));
	CompileResults.NumErrors++;
}

void FHlslNiagaraCompiler::Warning(FText WarningText)
{
	FString WarnString = FString::Printf(TEXT("%s"), *WarningText.ToString());
	CompileResults.CompileEvents.Add(FNiagaraCompileEvent(FNiagaraCompileEventType::Warning, WarnString));
	CompileResults.NumWarnings++;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

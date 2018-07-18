// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraParameters.h"

class Error;
class UEdGraphPin;
class UNiagaraNode;
class UNiagaraGraph;
class UEdGraphPin;
class FCompilerResultsLog;
class UNiagaraDataInterface;
struct FNiagaraTranslatorOutput;
struct FNiagaraVMExecutableData;
class FNiagaraCompileRequestData;
class FNiagaraCompileOptions;

/** Defines the compile event types for translation/compilation.*/
enum FNiagaraCompileEventType
{
	Log = 0,
	Warning = 1,
	Error = 2
};

/** Records necessary information to give UI cues for errors/logs/warnings during compile.*/
struct FNiagaraCompileEvent
{
public:
	FNiagaraCompileEvent(FNiagaraCompileEventType InType, const FString& InMessage, FGuid InNodeGuid = FGuid(), FGuid InPinGuid = FGuid(), const TArray<FGuid>& InCallstackGuids = TArray<FGuid>())
	: Type(InType), Message(InMessage), NodeGuid(InNodeGuid), PinGuid(InPinGuid), StackGuids(InCallstackGuids){}

	/** Whether or not this is an error, warning, or info*/
	FNiagaraCompileEventType Type;
	/* The message itself*/
	FString Message;
	/** The node guid that generated the compile event*/
	FGuid NodeGuid;
	/** The pin persistent id that generated the compile event*/
	FGuid PinGuid;
	/** The compile stack frame of node id's*/
	TArray<FGuid> StackGuids;
};


/** Defines information about the results of a Niagara script compile. */
struct FNiagaraCompileResults
{
	/** Whether or not the script compiled successfully for VectorVM */
	bool bVMSucceeded;

	/** Whether or not the script compiled successfully for GPU compute */
	bool bComputeSucceeded;
	
	/** The actual final compiled data.*/
	TSharedPtr<FNiagaraVMExecutableData> Data;

	float CompileTime;

	/** Tracking any compilation warnings or errors that occur.*/
	TArray<FNiagaraCompileEvent> CompileEvents;
	uint32 NumErrors;
	uint32 NumWarnings;

	FNiagaraCompileResults()
		: CompileTime(0.0f), NumErrors(0), NumWarnings(0)
	{
	}

	static ENiagaraScriptCompileStatus CompileResultsToSummary(const FNiagaraCompileResults* CompileResults);
};

//Interface for Niagara compilers.
// NOTE: the graph->hlsl translation step is now in FNiagaraHlslTranslator
//
class INiagaraCompiler
{
public:
	/** Compiles a script. */
	virtual FNiagaraCompileResults CompileScript(const FNiagaraCompileRequestData* InCompileRequest, const FNiagaraCompileOptions& InOptions, FNiagaraTranslatorOutput *TranslatorOutput, FString& TranslatedHLSL) = 0;

	/** Adds an error to be reported to the user. Any error will lead to compilation failure. */
	virtual void Error(FText ErrorText) = 0 ;

	/** Adds a warning to be reported to the user. Warnings will not cause a compilation failure. */
	virtual void Warning(FText WarningText) = 0;
};

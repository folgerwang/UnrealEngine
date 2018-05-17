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


/** Defines information about the results of a Niagara script compile. */
struct FNiagaraCompileResults
{
	/** Whether or not the script compiled successfully for VectorVM */
	bool bVMSucceeded;

	/** Whether or not the script compiled successfully for GPU compute */
	bool bComputeSucceeded;

	/** A results log with messages, warnings, and errors which occurred during the compile. */
	FCompilerResultsLog* MessageLog;

	/** The actual final compiled data.*/
	TSharedPtr<FNiagaraVMExecutableData> Data;

	float CompileTime;

	FNiagaraCompileResults(FCompilerResultsLog* InMessageLog = nullptr)
		: MessageLog(InMessageLog), CompileTime(0.0f)
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

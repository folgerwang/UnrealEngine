// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PythonScriptCommandlet.h"
#include "PythonScriptPlugin.h"

DEFINE_LOG_CATEGORY_STATIC(LogPythonScriptCommandlet, Log, All);

int32 UPythonScriptCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Find the Python script to run
	FString PythonScript;
	if (const FString* ParamVal = ParamVals.Find(TEXT("Script")))
	{
		PythonScript = *ParamVal;
	}
	else
	{
		UE_LOG(LogPythonScriptCommandlet, Error, TEXT("-Script argument not specified"));
		return -1;
	}

#if WITH_PYTHON
	UE_LOG(LogPythonScriptCommandlet, Display, TEXT("Running Python script: %s"), *PythonScript);
	FPythonScriptPlugin::Get()->HandlePythonExecCommand(*PythonScript);
#else	// WITH_PYTHON
	UE_LOG(LogPythonScriptCommandlet, Error, TEXT("Python script cannot run as the plugin was built as a stub!"));
	return -1;
#endif	// WITH_PYTHON

	return 0;
}

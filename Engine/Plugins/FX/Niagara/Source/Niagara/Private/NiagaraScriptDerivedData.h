// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraScript.h"
#include "NiagaraModule.h"
#include "NiagaraScriptSourceBase.h"

#if WITH_EDITOR
#include "DerivedDataPluginInterface.h"
#endif


#if WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// FNiagaraScriptDerivedData
class FNiagaraScriptDerivedData : public FDerivedDataPluginInterface
{
private:
	FString ScriptFullName;

	// The compile options to use in compiling this script.
	FNiagaraCompileOptions CompileOptions;

	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> PrecompiledData;

	// Niagara script id requested.
	FNiagaraVMExecutableDataId GeneratedVMId;

	// Was this created to be async?
	bool bAsynchronous;

	// The compiler
	INiagaraModule* NiagaraModule;


public:
	FNiagaraScriptDerivedData(const FString& InScriptFullName,const TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>& InPrecompiledData, const FNiagaraCompileOptions& InCompileOptions, const FNiagaraVMExecutableDataId& InGeneratedVMId, bool bInAsync);
	virtual ~FNiagaraScriptDerivedData();

	const FNiagaraVMExecutableDataId& GetGeneratedId() const {
		return GeneratedVMId;
	}

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("NiagaraScriptDerivedData");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		return TEXT("B19605DB7417452D85E5BD2E13EE370B");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override;


	virtual bool IsBuildThreadsafe() const override
	{
		return bAsynchronous;
	}

	/** Helper to convert the struct from its binary data out of the DDC to it's actual in-memory version.
		Do not call this on anything other than the game thread as it depends on the FObjectAndNameAsStringProxyArchive, 
		which calls FindStaticObject which can fail when used in any other thread!*/
	static bool BinaryToExecData(const TArray<uint8>& InBinaryData, FNiagaraVMExecutableData& OutExecData);

	/** Indicated that this plugin generates deterministic data. This is used for DDC verification */
	virtual bool IsDeterministic() const override { return true; }

	virtual FString GetDebugContextString() const override { return TEXT("Unknown Context"); }

	virtual bool Build( TArray<uint8>& OutData ) override;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return PrecompiledData.IsValid();
	}
};

#endif	//WITH_EDITOR

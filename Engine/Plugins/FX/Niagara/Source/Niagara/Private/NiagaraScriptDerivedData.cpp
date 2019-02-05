// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "NiagaraScriptDerivedData.h"
#include "NiagaraStats.h"
#include "Serialization/MemoryWriter.h"
#include "NiagaraScript.h"
#include "UObject/Package.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraScriptSourceBase.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "UObject/PropertyPortFlags.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR

DECLARE_CYCLE_STAT(TEXT("Niagara - DerivedData - Compile"), STAT_Niagara_VM_Compile, STATGROUP_Niagara);

FNiagaraScriptDerivedData::FNiagaraScriptDerivedData(const FString& InScriptFullName, const TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>& InPrecompiledData, const FNiagaraCompileOptions& InCompileOptions, const FNiagaraVMExecutableDataId& InGeneratedVMId, bool bInAsync)
	: ScriptFullName(InScriptFullName),  CompileOptions(InCompileOptions), PrecompiledData(InPrecompiledData), GeneratedVMId(InGeneratedVMId), bAsynchronous(bInAsync)
{
	check(InPrecompiledData.IsValid());

	CompileOptions.AdditionalDefines = InGeneratedVMId.AdditionalDefines;
	NiagaraModule = &FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
}

FNiagaraScriptDerivedData::~FNiagaraScriptDerivedData()
{
}

FString FNiagaraScriptDerivedData::GetPluginSpecificCacheKeySuffix() const
{
	enum { UE_NIAGARA_COMPILATION_DERIVEDDATA_VER = 1 };

	FString Ret = FString::Printf(TEXT("%i_%i"),
		(int32)UE_NIAGARA_COMPILATION_DERIVEDDATA_VER, GNiagaraSkipVectorVMBackendOptimizations);

	GeneratedVMId.AppendKeyString(Ret);

	//UE_LOG(LogNiagara, Log, TEXT("Cache Key Gen Niagara VM Script DDC data for %s key %s"), *ScriptFullName, *Ret);

	return Ret;
}

bool FNiagaraScriptDerivedData::BinaryToExecData(const TArray<uint8>& InBinaryData, FNiagaraVMExecutableData& OutExecData)
{
	// Do not call this on anything other than the game thread as it depends on the FObjectAndNameAsStringProxyArchive, which 
	// calls FindStaticObject which can fail when used in any other thread!
	check(IsInGameThread());
	if (InBinaryData.Num() == 0)
	{
		return false;
	}
	FMemoryReader Ar(InBinaryData);
	FObjectAndNameAsStringProxyArchive SafeAr(Ar, false);
	OutExecData.SerializeData(SafeAr, true);
	SafeAr.Close();
	Ar.Close();

	return !SafeAr.IsError();
}


bool FNiagaraScriptDerivedData::Build(TArray<uint8>& OutData)
{
	SCOPE_CYCLE_COUNTER(STAT_Niagara_VM_Compile);
	//UE_LOG(LogNiagara, Log, TEXT("Building Niagara VM Script DDC data for %s"), *ScriptFullName);

	bool bCompilationSuccessful = false;
	{
		FString OutGraphLevelErrorMessages;
		TSharedPtr<FNiagaraVMExecutableData> ExeData = NiagaraModule->CompileScript(PrecompiledData.Get(), CompileOptions);
		bCompilationSuccessful = ExeData.IsValid();

		if (bCompilationSuccessful)
		{
			//const bool bTestSuccessfulReadWrite = false; // Set to true to debug serialization issues.
			{
				FMemoryWriter Ar(OutData, true);
				FObjectAndNameAsStringProxyArchive SafeAr(Ar, false);
				ExeData->SerializeData(SafeAr, true);
			}

			/*if (bTestSuccessfulReadWrite && IsInGameThread())
			{
				FNiagaraVMExecutableData TestExeData;
				bool bGenerated = BinaryToExecData(OutData, TestExeData);
				check(bGenerated);

				bool bIdentical = FNiagaraVMExecutableData::StaticStruct()->CompareScriptStruct(&TestExeData, ExeData.Get(), PPF_None);
				check(bIdentical);
			}*/
		}
	}

	return bCompilationSuccessful;
}
#endif	//WITH_EDITOR

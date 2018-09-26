// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraComponent.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraStats.h"
#include "UObject/Linker.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"

DECLARE_CYCLE_STAT(TEXT("Niagara - Utilities - PrepareRapidIterationParameters"), STAT_Niagara_Utilities_PrepareRapidIterationParameters, STATGROUP_Niagara);

//////////////////////////////////////////////////////////////////////////

FString FNiagaraTypeHelper::ToString(const uint8* ValueData, const UScriptStruct* Struct)
{
	FString Ret;
	if (Struct == FNiagaraTypeDefinition::GetFloatStruct())
	{
		Ret += FString::Printf(TEXT("%g "), *(float*)ValueData);
	}
	else if (Struct == FNiagaraTypeDefinition::GetIntStruct())
	{
		Ret += FString::Printf(TEXT("%d "), *(int32*)ValueData);
	}
	else if (Struct == FNiagaraTypeDefinition::GetBoolStruct())
	{
		int32 Val = *(int32*)ValueData;
		Ret += Val == 0xFFFFFFFF ? (TEXT("True")) : ( Val == 0x0 ? TEXT("False") : TEXT("Invalid"));		
	}
	else
	{
		for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			const UProperty* Property = *PropertyIt;
			const uint8* PropPtr = ValueData + PropertyIt->GetOffset_ForInternal();
			if (Property->IsA(UFloatProperty::StaticClass()))
			{
				Ret += FString::Printf(TEXT("%s: %g "), *Property->GetNameCPP(), *(float*)PropPtr);
			}
			else if (Property->IsA(UIntProperty::StaticClass()))
			{
				Ret += FString::Printf(TEXT("%s: %d "), *Property->GetNameCPP(), *(int32*)PropPtr);
			}
			else if (Property->IsA(UBoolProperty::StaticClass()))
			{
				int32 Val = *(int32*)ValueData;
				FString BoolStr = Val == 0xFFFFFFFF ? (TEXT("True")) : (Val == 0x0 ? TEXT("False") : TEXT("Invalid"));
				Ret += FString::Printf(TEXT("%s: %d "), *Property->GetNameCPP(), *BoolStr);
			}
			else if (const UStructProperty* StructProp = CastChecked<UStructProperty>(Property))
			{
				Ret += FString::Printf(TEXT("%s: (%s) "), *Property->GetNameCPP(), *FNiagaraTypeHelper::ToString(PropPtr, StructProp->Struct));
			}
			else
			{
				check(false);
				Ret += TEXT("Unknown Type");
			}
		}
	}
	return Ret;
}
//////////////////////////////////////////////////////////////////////////

FNiagaraSystemUpdateContext::~FNiagaraSystemUpdateContext()
{
	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	for (UNiagaraSystem* Sys : SystemSimsToDestroy)
	{
		NiagaraModule.DestroyAllSystemSimulations(Sys);
	}

	for (UNiagaraComponent* Comp : ComponentsToReInit)
	{
		Comp->ReinitializeSystem();
	}
	for (UNiagaraComponent* Comp : ComponentsToReset)
	{
		Comp->ResetSystem();
	}
}

void FNiagaraSystemUpdateContext::AddAll(bool bReInit)
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Comp = *It;
		check(Comp);
		AddInternal(Comp, bReInit);
	}
}

void FNiagaraSystemUpdateContext::Add(const UNiagaraSystem* System, bool bReInit)
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Comp = *It;
		check(Comp);
		if (Comp->GetAsset() == System)
		{
			AddInternal(Comp, bReInit);
		}
	}
}
#if WITH_EDITORONLY_DATA

void FNiagaraSystemUpdateContext::Add(const UNiagaraEmitter* Emitter, bool bReInit)
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Comp = *It;
		check(Comp);
		FNiagaraSystemInstance* SystemInst = Comp->GetSystemInstance();
		if (SystemInst && SystemInst->UsesEmitter(Emitter))
		{
			AddInternal(Comp, bReInit);
		}		
	}
}

void FNiagaraSystemUpdateContext::Add(const UNiagaraScript* Script, bool bReInit)
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Comp = *It;
		check(Comp);
		UNiagaraSystem* System = Comp->GetAsset();
		if (System && System->UsesScript(Script))
		{
			AddInternal(Comp, bReInit);
		}
	}
}

// void FNiagaraSystemUpdateContext::Add(UNiagaraDataInterface* Interface, bool bReInit)
// {
// 	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
// 	{
// 		UNiagaraComponent* Comp = *It;
// 		check(Comp);		
// 		if (FNiagaraSystemInstance* SystemInst = Comp->GetSystemInstance())
// 		{
// 			if (SystemInst->ContainsDataInterface(Interface))
// 			{
// 				AddInternal(SystemInst, bReInit);
// 			}
// 		}
// 	}
// }

void FNiagaraSystemUpdateContext::Add(const UNiagaraParameterCollection* Collection, bool bReInit)
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Comp = *It;
		check(Comp);
		FNiagaraSystemInstance* SystemInst = Comp->GetSystemInstance();
		if (SystemInst && SystemInst->UsesCollection(Collection))
		{
			AddInternal(Comp, bReInit);
		}
	}
}
#endif

void FNiagaraSystemUpdateContext::AddInternal(UNiagaraComponent* Comp, bool bReInit)
{
	if (bReInit)
	{
		ComponentsToReInit.AddUnique(Comp);
		SystemSimsToDestroy.AddUnique(Comp->GetAsset());
	}
	else
	{
		ComponentsToReset.AddUnique(Comp);
	}
}


//////////////////////////////////////////////////////////////////////////

FName NIAGARA_API FNiagaraUtilities::GetUniqueName(FName CandidateName, const TSet<FName>& ExistingNames)
{
	if (ExistingNames.Contains(CandidateName) == false)
	{
		return CandidateName;
	}

	FString CandidateNameString = CandidateName.ToString();
	FString BaseNameString = CandidateNameString;
	if (CandidateNameString.Len() >= 3 && CandidateNameString.Right(3).IsNumeric())
	{
		BaseNameString = CandidateNameString.Left(CandidateNameString.Len() - 3);
	}

	FName UniqueName = FName(*BaseNameString);
	int32 NameIndex = 1;
	while (ExistingNames.Contains(UniqueName))
	{
		UniqueName = FName(*FString::Printf(TEXT("%s%03i"), *BaseNameString, NameIndex));
		NameIndex++;
	}

	return UniqueName;
}

FNiagaraVariable FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(FNiagaraVariable InVar, const TCHAR* InEmitterName, ENiagaraScriptUsage InUsage)
{
	FNiagaraVariable Var = InVar;

	TArray<FString> SplitName;
	Var.GetName().ToString().ParseIntoArray(SplitName, TEXT("."));
	int32 NumSlots = SplitName.Num();
	if (InEmitterName != nullptr)
	{
		for (int32 i = 0; i < NumSlots; i++)
		{
			if (SplitName[i] == TEXT("Emitter"))
			{
				SplitName[i] = InEmitterName;
			}
		}

		if (NumSlots >= 3 && SplitName[0] == InEmitterName)
		{
			// Do nothing
			UE_LOG(LogNiagara, Log, TEXT("ConvertVariableToRapidIterationConstantName Got here!"));
		}
		else
		{
			SplitName.Insert(InEmitterName, 0);
		}
		SplitName.Insert(TEXT("Constants"), 0);
	}
	else
	{
		SplitName.Insert(TEXT("Constants"), 0);
	}

	FString OutVarStrName = FString::Join(SplitName, TEXT("."));
	Var.SetName(*OutVarStrName);
	return Var;
}

void FNiagaraUtilities::CollectScriptDataInterfaceParameters(const UObject& Owner, const TArray<UNiagaraScript*>& Scripts, FNiagaraParameterStore& OutDataInterfaceParameters)
{
	for (UNiagaraScript* Script : Scripts)
	{
		for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : Script->GetCachedDefaultDataInterfaces())
		{
			if (DataInterfaceInfo.RegisteredParameterMapWrite != NAME_None)
			{
				FNiagaraVariable DataInterfaceParameter(DataInterfaceInfo.Type, DataInterfaceInfo.RegisteredParameterMapWrite);
				if (OutDataInterfaceParameters.AddParameter(DataInterfaceParameter, false, false))
				{
					OutDataInterfaceParameters.SetDataInterface(DataInterfaceInfo.DataInterface, DataInterfaceParameter);
				}
				else
				{
					UE_LOG(LogNiagara, Error, TEXT("Duplicate data interface parameter writes found, simulation will be incorrect.  Owner: %s Parameter: %s"),
						*Owner.GetPathName(), *DataInterfaceInfo.RegisteredParameterMapWrite.ToString());
				}
			}
		}
	}
}

bool FNiagaraScriptDataInterfaceCompileInfo::CanExecuteOnTarget(ENiagaraSimTarget SimTarget) const
{
	check(IsInGameThread());
	UNiagaraDataInterface* Obj = GetDefaultDataInterface();
	if (Obj)
	{
		return Obj->CanExecuteOnTarget(SimTarget);
	}
	check(false);
	return false;
}

bool FNiagaraScriptDataInterfaceCompileInfo::IsSystemSolo() const
{
	check(IsInGameThread());
	if (Name.ToString().StartsWith("User."))
	{
		return true;
	}

	UNiagaraDataInterface* Obj = GetDefaultDataInterface();
	if (Obj && Obj->PerInstanceDataSize() > 0)
	{
		return true;
	}
	return false;
}

UNiagaraDataInterface* FNiagaraScriptDataInterfaceCompileInfo::GetDefaultDataInterface() const
{
	check(IsInGameThread());
	UNiagaraDataInterface* Obj = CastChecked<UNiagaraDataInterface>(const_cast<UClass*>(Type.GetClass())->GetDefaultObject(true));
	return Obj;
}
#if WITH_EDITORONLY_DATA
void FNiagaraUtilities::PrepareRapidIterationParameters(const TArray<UNiagaraScript*>& Scripts, const TMap<UNiagaraScript*, UNiagaraScript*>& ScriptDependencyMap, const TMap<UNiagaraScript*, FString>& ScriptToEmitterNameMap)
{
	SCOPE_CYCLE_COUNTER(STAT_Niagara_Utilities_PrepareRapidIterationParameters);

	TMap<UNiagaraScript*, FNiagaraParameterStore> ScriptToPreparedParameterStoreMap;

	// Remove old and initialize new parameters.
	for (UNiagaraScript* Script : Scripts)
	{
		FNiagaraParameterStore& ParameterStoreToPrepare = ScriptToPreparedParameterStoreMap.FindOrAdd(Script);
		Script->RapidIterationParameters.CopyParametersTo(ParameterStoreToPrepare, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
		const FString* EmitterName = ScriptToEmitterNameMap.Find(Script);
		checkf(EmitterName != nullptr, TEXT("Script to emitter name map must have an entry for each script to be processed."));
		Script->GetSource()->CleanUpOldAndInitializeNewRapidIterationParameters(*EmitterName, Script->GetUsage(), Script->GetUsageId(), ParameterStoreToPrepare);
	}

	// Copy parameters for dependencies.
	for (auto It = ScriptToPreparedParameterStoreMap.CreateIterator(); It; ++It)
	{
		UNiagaraScript* Script = It.Key();
		FNiagaraParameterStore& PreparedParameterStore = It.Value();
		UNiagaraScript*const* DependentScriptPtr = ScriptDependencyMap.Find(Script);
		if (DependentScriptPtr != nullptr)
		{
			UNiagaraScript* DependentScript = *DependentScriptPtr;
			FNiagaraParameterStore* DependentPreparedParameterStore = ScriptToPreparedParameterStoreMap.Find(DependentScript);
			checkf(DependentPreparedParameterStore != nullptr, TEXT("Dependent scripts must be one of the scripts being processed."));
			PreparedParameterStore.CopyParametersTo(*DependentPreparedParameterStore, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
		}
	}

	// Resolve prepared parameters with the source parameters.
	for (auto It = ScriptToPreparedParameterStoreMap.CreateIterator(); It; ++It)
	{
		UNiagaraScript* Script = It.Key();
		FNiagaraParameterStore& PreparedParameterStore = It.Value();

		bool bOverwriteParameters = false;
		if (Script->RapidIterationParameters.GetNumParameters() != PreparedParameterStore.GetNumParameters())
		{
			bOverwriteParameters = true;
		}
		else
		{
			const TMap<FNiagaraVariable, int32>& SourceParameterOffsets = Script->RapidIterationParameters.GetParameterOffests();
			for (auto ParameterOffsetIt = SourceParameterOffsets.CreateConstIterator(); ParameterOffsetIt; ++ParameterOffsetIt)
			{
				const FNiagaraVariable& SourceParameter = ParameterOffsetIt.Key();
				int32 SourceOffset = ParameterOffsetIt.Value();

				int32 PreparedOffset = PreparedParameterStore.IndexOf(SourceParameter);
				if (PreparedOffset == INDEX_NONE)
				{
					bOverwriteParameters = true;
					break;
				}
				else
				{
					if (FMemory::Memcmp(
						Script->RapidIterationParameters.GetParameterData(SourceOffset),
						PreparedParameterStore.GetParameterData(PreparedOffset),
						SourceParameter.GetSizeInBytes()) != 0)
					{
						bOverwriteParameters = true;
						break;
					}
				}
			}
		}

		if (bOverwriteParameters)
		{
			Script->RapidIterationParameters = PreparedParameterStore;
		}
	}
}
#endif
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptExecutionContext.h"
#include "NiagaraStats.h"
#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraWorldManager.h"

DECLARE_CYCLE_STAT(TEXT("Register Setup"), STAT_NiagaraSimRegisterSetup, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Context Ticking"), STAT_NiagaraScriptExecContextTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Rebind DInterface Func Table"), STAT_NiagaraRebindDataInterfaceFunctionTable, STATGROUP_Niagara);
	//Add previous frame values if we're interpolated spawn.
	
	//Internal constants - only needed for non-GPU sim

uint32 FNiagaraScriptExecutionContext::TickCounter = 0;

FNiagaraScriptExecutionContext::FNiagaraScriptExecutionContext()
	: Script(nullptr)
{

}

FNiagaraScriptExecutionContext::~FNiagaraScriptExecutionContext()
{
}

bool FNiagaraScriptExecutionContext::Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget)
{
	Script = InScript;

	Parameters.InitFromOwningContext(Script, InTarget, true);

	return true;//TODO: Error cases?
}

bool FNiagaraScriptExecutionContext::Tick(FNiagaraSystemInstance* ParentSystemInstance, ENiagaraSimTarget SimTarget)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraScriptExecContextTick);

	if (Script && Script->IsReadyToRun(ENiagaraSimTarget::CPUSim))//TODO: Remove. Script can only be null for system instances that currently don't have their script exec context set up correctly.
	{
		const TArray<UNiagaraDataInterface*>& DataInterfaces = GetDataInterfaces();
		
		//Bind data interfaces if needed.
		if (Parameters.GetInterfacesDirty())
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraRebindDataInterfaceFunctionTable);
			// UE_LOG(LogNiagara, Log, TEXT("Updating data interfaces for script %s"), *Script->GetFullName());

			// We must make sure that the data interfaces match up between the original script values and our overrides...
			if (Script->GetVMExecutableData().DataInterfaceInfo.Num() != DataInterfaces.Num())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Mismatch between Niagara Exectuion Context data interfaces and those in it's script!"));
				return false;
			}

			//Fill the instance data table.
			if (ParentSystemInstance)
			{
				DataInterfaceInstDataTable.SetNumZeroed(Script->GetVMExecutableData().NumUserPtrs, false);
				for (int32 i = 0; i < DataInterfaces.Num(); i++)
				{
					UNiagaraDataInterface* Interface = DataInterfaces[i];

					int32 UserPtrIdx = Script->GetVMExecutableData().DataInterfaceInfo[i].UserPtrIdx;
					if (UserPtrIdx != INDEX_NONE)
					{
						void* InstData = ParentSystemInstance->FindDataInterfaceInstanceData(Interface);
						DataInterfaceInstDataTable[UserPtrIdx] = InstData;
					}
				}
			}
			else
			{
				check(Script->GetVMExecutableData().NumUserPtrs == 0);//Can't have user ptrs if we have no parent instance.
			}

			FunctionTable.Reset(Script->GetVMExecutableData().CalledVMExternalFunctions.Num());

			bool bSuccessfullyMapped = true;
			for (FVMExternalFunctionBindingInfo& BindingInfo : Script->GetVMExecutableData().CalledVMExternalFunctions)
			{
				for (int32 i = 0; i < Script->GetVMExecutableData().DataInterfaceInfo.Num(); i++)
				{
					FNiagaraScriptDataInterfaceCompileInfo& ScriptInfo = Script->GetVMExecutableData().DataInterfaceInfo[i];
					UNiagaraDataInterface* ExternalInterface = DataInterfaces[i];
					if (ScriptInfo.Name == BindingInfo.OwnerName)
					{
						void* InstData = ScriptInfo.UserPtrIdx == INDEX_NONE ? nullptr : DataInterfaceInstDataTable[ScriptInfo.UserPtrIdx];
						int32 AddedIdx = FunctionTable.Add(FVMExternalFunction());
						if (ExternalInterface != nullptr)
						{
							ExternalInterface->GetVMExternalFunction(BindingInfo, InstData, FunctionTable[AddedIdx]);
						}

						if (AddedIdx != INDEX_NONE && !FunctionTable[AddedIdx].IsBound())
						{
							UE_LOG(LogNiagara, Error, TEXT("Could not Get VMExternalFunction '%s'.. emitter will not run!"), *BindingInfo.Name.ToString());
							bSuccessfullyMapped = false;
						}
					}
				}
			}

			if (!bSuccessfullyMapped)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Error building data interface function table!"));
				FunctionTable.Empty();
				return false;
			}
		}
	}

	Parameters.Tick();

	return true;
}

void FNiagaraScriptExecutionContext::PostTick()
{
	//If we're for interpolated spawn, copy over the previous frame's parameters into the Prev parameters.
	if (Script && Script->GetComputedVMCompilationId().HasInterpolatedParameters())
	{
		Parameters.CopyCurrToPrev();
	}
}

bool FNiagaraScriptExecutionContext::Execute(uint32 NumInstances, TArray<FNiagaraDataSetExecutionInfo, TInlineAllocator<8>>& DataSetInfos)
{
	if (NumInstances == 0)
	{
		return true;
	}

	++TickCounter;//Should this be per execution?

	int32 NumInputRegisters = 0;
	int32 NumOutputRegisters = 0;
	uint8* InputRegisters[VectorVM::MaxInputRegisters];
	uint8* OutputRegisters[VectorVM::MaxOutputRegisters];

	DataSetMetaTable.Reset();

	bool bRegisterSetupCompleted = true;
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimRegisterSetup);
		for (FNiagaraDataSetExecutionInfo& DataSetInfo : DataSetInfos)
		{
#if NIAGARA_NAN_CHECKING
			DataSetInfo.DataSet->CheckForNaNs();
#endif
			check(DataSetInfo.DataSet);
			FDataSetMeta SetMeta(DataSetInfo.DataSet->GetSizeBytes(), &InputRegisters[NumInputRegisters], NumInputRegisters, DataSetInfo.StartInstance, 
				&DataSetInfo.DataSet->CurrIDTable(), &DataSetInfo.DataSet->GetFreeIDTable(), &DataSetInfo.DataSet->GetNumFreeIDs(), &DataSetInfo.DataSet->GetMaxUsedID(), DataSetInfo.DataSet->GetIDAcquireTag());
			DataSetMetaTable.Add(SetMeta);
			if (DataSetInfo.bAllocate)
			{
				DataSetInfo.DataSet->Allocate(NumInstances);
				DataSetInfo.DataSet->SetNumInstances(NumInstances);
			}
			
			bRegisterSetupCompleted &= DataSetInfo.DataSet->AppendToRegisterTable(InputRegisters, NumInputRegisters, OutputRegisters, NumOutputRegisters, DataSetInfo.StartInstance);
		}
	}

	if (bRegisterSetupCompleted)
	{
		VectorVM::Exec(
			Script->GetVMExecutableData().ByteCode.GetData(),
			InputRegisters,
			NumInputRegisters,
			OutputRegisters,
			NumOutputRegisters,
			Parameters.GetParameterDataArray().GetData(),
			DataSetMetaTable,
			FunctionTable.GetData(),
			DataInterfaceInstDataTable.GetData(),
			NumInstances
#if STATS
			, Script->GetStatScopeIDs()
#endif
		);
	}

	// Tell the datasets we wrote how many instances were actually written.
	for (int Idx = 0; Idx < DataSetInfos.Num(); Idx++)
	{
		FNiagaraDataSetExecutionInfo& Info = DataSetInfos[Idx];

#if NIAGARA_NAN_CHECKING
		Info.DataSet->CheckForNaNs();
#endif

		if (Info.bUpdateInstanceCount)
		{
			Info.DataSet->SetNumInstances(Info.StartInstance + DataSetMetaTable[Idx].DataSetAccessIndex + 1);
		}
	}

	return true;//TODO: Error cases?
}

void FNiagaraScriptExecutionContext::DirtyDataInterfaces()
{
	Parameters.MarkInterfacesDirty();
}

bool FNiagaraScriptExecutionContext::CanExecute()const
{
	return Script && Script->GetVMExecutableData().IsValid() && Script->GetVMExecutableData().ByteCode.Num() > 0;
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemSimulation.h"
#include "NiagaraModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraTypes.h"
#include "NiagaraEvents.h"
#include "NiagaraSettings.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraConstants.h"
#include "NiagaraStats.h"
#include "Async/ParallelFor.h"
#include "NiagaraComponent.h"
#include "NiagaraWorldManager.h"

DECLARE_CYCLE_STAT(TEXT("System Simulation"), STAT_NiagaraSystemSim, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Pre Simulate"), STAT_NiagaraSystemSim_PreSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Prepare For Simulate"), STAT_NiagaraSystemSim_PrepareForSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Update"), STAT_NiagaraSystemSim_Update, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Spawn"), STAT_NiagaraSystemSim_Spawn, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Transfer Parameters"), STAT_NiagaraSystemSim_TransferParameters, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Post Simulate"), STAT_NiagaraSystemSim_PostSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Mark Component Dirty"), STAT_NiagaraSystemSim_MarkComponentDirty, STATGROUP_Niagara);


static int32 GbDumpSystemData = 0;
static FAutoConsoleVariableRef CVarNiagaraDumpSystemData(
	TEXT("fx.DumpSystemData"),
	GbDumpSystemData,
	TEXT("If > 0, results of system simulations will be dumped to the log. \n"),
	ECVF_Default
);

static int32 GbSystemUpdateOnSpawn = 1;
static FAutoConsoleVariableRef CVarSystemUpdateOnSpawn(
	TEXT("fx.SystemUpdateOnSpawn"),
	GbSystemUpdateOnSpawn,
	TEXT("If > 0, system simulations are given a small update after spawn. \n"),
	ECVF_Default
);

//Pretick can no longer be run in parallel. Will likely remain this way.
// static int32 GbParallelSystemPreTick = 0;
// static FAutoConsoleVariableRef CVarNiagaraParallelSystemPreTick(
// 	TEXT("fx.ParallelSystemPreTick"),
// 	GbParallelSystemPreTick,
// 	TEXT("If > 0, system pre tick is parallelized. \n"),
// 	ECVF_Default
// );

static int32 GbParallelSystemPostTick = 1;
static FAutoConsoleVariableRef CVarNiagaraParallelSystemPostTick(
	TEXT("fx.ParallelSystemPostTick"),
	GbParallelSystemPostTick,
	TEXT("If > 0, system post tick is parallelized. \n"),
	ECVF_Default
);

//TODO: Experiment with parallel param transfer.
//static int32 GbParallelSystemParamTransfer = 1;
//static FAutoConsoleVariableRef CVarNiagaraParallelSystemParamTransfer(
//	TEXT("fx.ParallelSystemParamTransfer"),
//	GbParallelSystemParamTransfer,
//	TEXT("If > 0, system param transfer is parallelized. \n"),
//	ECVF_Default
//);

//////////////////////////////////////////////////////////////////////////

FNiagaraSystemSimulation::~FNiagaraSystemSimulation()
{
	Destroy();
}

bool FNiagaraSystemSimulation::Init(UNiagaraSystem* InSystem, UWorld* InWorld, FNiagaraSystemInstance* InSoloSystemInstance)
{
	UNiagaraSystem* System = InSystem;
	WeakSystem = System;
	SoloSystemInstance = InSoloSystemInstance;

	World = InWorld;

	FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(InWorld);
	check(WorldMan);

	bCanExecute = System->GetSystemSpawnScript()->GetVMExecutableData().IsValid() && System->GetSystemUpdateScript()->GetVMExecutableData().IsValid();
	UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();

	if (bCanExecute)
	{
		DataSet.Init(FNiagaraDataSetID(), ENiagaraSimTarget::CPUSim);
		DataSet.AddVariables(System->GetSystemSpawnScript()->GetVMExecutableData().Attributes);
		DataSet.AddVariables(System->GetSystemUpdateScript()->GetVMExecutableData().Attributes);
		DataSet.Finalize();

		{
			SpawnInstanceParameterDataSet.Init(FNiagaraDataSetID(), ENiagaraSimTarget::CPUSim);
			FNiagaraParameters* EngineParamsSpawn = System->GetSystemSpawnScript()->GetVMExecutableData().DataSetToParameters.Find(TEXT("Engine"));
			if (EngineParamsSpawn != nullptr)
			{
				SpawnInstanceParameterDataSet.AddVariables(EngineParamsSpawn->Parameters);
			}
			SpawnInstanceParameterDataSet.Finalize();
			UpdateInstanceParameterDataSet.Init(FNiagaraDataSetID(), ENiagaraSimTarget::CPUSim);
			FNiagaraParameters* EngineParamsUpdate = System->GetSystemUpdateScript()->GetVMExecutableData().DataSetToParameters.Find(TEXT("Engine"));
			if (EngineParamsUpdate != nullptr)
			{
				UpdateInstanceParameterDataSet.AddVariables(EngineParamsUpdate->Parameters);
			}
			UpdateInstanceParameterDataSet.Finalize();
		}

		UNiagaraScript* SpawnScript = System->GetSystemSpawnScript();
		UNiagaraScript* UpdateScript = System->GetSystemUpdateScript();

		SpawnExecContext.Init(SpawnScript, ENiagaraSimTarget::CPUSim);
		UpdateExecContext.Init(UpdateScript, ENiagaraSimTarget::CPUSim);

		//Bind parameter collections.
		for (UNiagaraParameterCollection* Collection : SpawnScript->GetCachedParameterCollectionReferences())
		{
			GetParameterCollectionInstance(Collection)->GetParameterStore().Bind(&SpawnExecContext.Parameters);
		}
		for (UNiagaraParameterCollection* Collection : UpdateScript->GetCachedParameterCollectionReferences())
		{
			GetParameterCollectionInstance(Collection)->GetParameterStore().Bind(&UpdateExecContext.Parameters);
		}

		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(SpawnScript);
		Scripts.Add(UpdateScript);
		FNiagaraUtilities::CollectScriptDataInterfaceParameters(*System, Scripts, ScriptDefinedDataInterfaceParameters);

		ScriptDefinedDataInterfaceParameters.Bind(&SpawnExecContext.Parameters);
		ScriptDefinedDataInterfaceParameters.Bind(&UpdateExecContext.Parameters);

		SpawnScript->RapidIterationParameters.Bind(&SpawnExecContext.Parameters);
		UpdateScript->RapidIterationParameters.Bind(&UpdateExecContext.Parameters);

		SystemExecutionStateAccessor.Create(&DataSet, FNiagaraVariable(EnumPtr, TEXT("System.ExecutionState")));
		EmitterSpawnInfoAccessors.Reset();
		EmitterExecutionStateAccessors.Reset();
		EmitterSpawnInfoAccessors.SetNum(System->GetNumEmitters());

		for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
		{
			FNiagaraEmitterHandle& EmitterHandle = System->GetEmitterHandle(EmitterIdx);
			UNiagaraEmitter* Emitter = EmitterHandle.GetInstance();
			FString EmitterName = Emitter->GetUniqueEmitterName();
			check(Emitter);
			EmitterExecutionStateAccessors.Emplace(DataSet, FNiagaraVariable(EnumPtr, *(EmitterName + TEXT(".ExecutionState"))));
			const TArray<FNiagaraEmitterSpawnAttributes>& EmitterSpawnAttrNames = System->GetEmitterSpawnAttributes();
			
			check(EmitterSpawnAttrNames.Num() == System->GetNumEmitters());
			for (FName AttrName : EmitterSpawnAttrNames[EmitterIdx].SpawnAttributes)
			{
				EmitterSpawnInfoAccessors[EmitterIdx].Emplace(DataSet, FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()), AttrName));
			}
		}

		SpawnDeltaTimeParam.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_DELTA_TIME);
		UpdateDeltaTimeParam.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_DELTA_TIME);
		SpawnInvDeltaTimeParam.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_INV_DELTA_TIME);
		UpdateInvDeltaTimeParam.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_INV_DELTA_TIME);
		SpawnNumSystemInstancesParam.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_NUM_SYSTEM_INSTANCES);
		UpdateNumSystemInstancesParam.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_NUM_SYSTEM_INSTANCES);
		SpawnGlobalSpawnCountScaleParam.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_GLOBAL_SPAWN_COUNT_SCALE);
		UpdateGlobalSpawnCountScaleParam.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_GLOBAL_SPAWN_COUNT_SCALE);
		SpawnGlobalSystemCountScaleParam.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_GLOBAL_SYSTEM_COUNT_SCALE);
		UpdateGlobalSystemCountScaleParam.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_GLOBAL_SYSTEM_COUNT_SCALE);
	}

	return true;
}

void FNiagaraSystemSimulation::Destroy()
{
	while (SystemInstances.Num())
	{
		SystemInstances.Last()->Deactivate(true);
	}
	while (PendingSystemInstances.Num())
	{
		PendingSystemInstances.Last()->Deactivate(true);
	}

	FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World);
	check(WorldMan);
	SpawnExecContext.Parameters.UnbindFromSourceStores();
	UpdateExecContext.Parameters.UnbindFromSourceStores();
}

UNiagaraParameterCollectionInstance* FNiagaraSystemSimulation::GetParameterCollectionInstance(UNiagaraParameterCollection* Collection)
{
	UNiagaraSystem* System = WeakSystem.Get();
	check(System != nullptr);
	UNiagaraParameterCollectionInstance* Ret = System->GetParameterCollectionOverride(Collection);

	//If no explicit override from the system, just get the current instance set on the world.
	if (!Ret)
	{
		FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World);
		Ret = WorldMan->GetParameterCollection(Collection);
	}

	return Ret;
}

FNiagaraParameterStore& FNiagaraSystemSimulation::GetScriptDefinedDataInterfaceParameters()
{
	return ScriptDefinedDataInterfaceParameters;
}

void FNiagaraSystemSimulation::TransferInstance(FNiagaraSystemSimulation* SourceSimulation, FNiagaraSystemInstance* SystemInst)
{
	check(SourceSimulation->GetSystem() == GetSystem());
	check(SystemInst);

	int32 SystemInstIdx = SystemInst->SystemInstanceIndex;
	int32 NewDataIndex = INDEX_NONE;
	if (!SystemInst->IsPendingSpawn())
	{
// 		UE_LOG(LogNiagara, Log, TEXT("== Dataset Transfer ========================"));
// 		UE_LOG(LogNiagara, Log, TEXT(" ----- Existing values in src. Idx: %d -----"), SystemInstIdx);
// 		SourceSimulation->DataSet.Dump(true, SystemInstIdx, 1);

		//If we're not pending then the system actually has data to pull over.
		NewDataIndex = DataSet.TransferInstance(SourceSimulation->DataSet, SystemInstIdx);

// 		UE_LOG(LogNiagara, Log, TEXT(" ----- Transfered values in dest. Idx: %d -----"), NewDataIndex);
// 		DataSet.Dump(true, NewDataIndex, 1);
	
		SourceSimulation->RemoveInstance(SystemInst);
	
		//Move the system direct to the new sim's 
		SystemInst->SystemInstanceIndex = SystemInstances.Add(SystemInst);

		check(NewDataIndex == SystemInst->SystemInstanceIndex);
	}
	else
	{
		SourceSimulation->RemoveInstance(SystemInst);

		AddInstance(SystemInst);			
	}
}

void FNiagaraSystemSimulation::DumpInstance(const FNiagaraSystemInstance* Inst)const
{
	UE_LOG(LogNiagara, Log, TEXT("==  %s (%d) ========"), *Inst->GetSystem()->GetFullName(), Inst->SystemInstanceIndex);
	UE_LOG(LogNiagara, Log, TEXT(".................Spawn................."));
	SpawnExecContext.Parameters.DumpParameters(false);
	SpawnInstanceParameterDataSet.Dump(false, Inst->SystemInstanceIndex, 1);
	UE_LOG(LogNiagara, Log, TEXT(".................Update................."));
	UpdateExecContext.Parameters.DumpParameters(false);
	UpdateInstanceParameterDataSet.Dump(false, Inst->SystemInstanceIndex, 1);
	UE_LOG(LogNiagara, Log, TEXT("................. System Instance ................."));
	DataSet.Dump(false, Inst->SystemInstanceIndex, 1);
	DataSet.Dump(true, Inst->SystemInstanceIndex, 1);
}

bool FNiagaraSystemSimulation::Tick(float DeltaSeconds)
{
	UNiagaraSystem* System = WeakSystem.Get();
	if (System == nullptr || bCanExecute == false)
	{
		// TODO: evaluate whether or not we should have removed this from the world manager instead?
		return false;
	}

	UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript();
	UNiagaraScript* SystemUpdateScript = System->GetSystemUpdateScript();
#if WITH_EDITOR
	SystemSpawnScript->RapidIterationParameters.Tick();
	SystemUpdateScript->RapidIterationParameters.Tick();
#endif

	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim);

	int32 OrigNum = SystemInstances.Num();
	int32 SpawnNum = PendingSystemInstances.Num();
	int32 NewNum = OrigNum + SpawnNum;

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_PreSimulate);

		{
			int32 SystemIndex = 0;
			while (SystemIndex < SystemInstances.Num())
			{
				FNiagaraSystemInstance* Inst = SystemInstances[SystemIndex];

				Inst->TickDataInterfaces(DeltaSeconds, false);
				if (!Inst->IsComplete())
				{
					++SystemIndex;
				}
				else
				{
					checkSlow(Inst->SystemInstanceIndex == INDEX_NONE);
				}
			}
			OrigNum = SystemIndex;

			//Pre tick and gather any still valid pending instances for spawn.
			SystemInstances.Reserve(NewNum);
			SpawnNum = 0;
			while (PendingSystemInstances.Num())
			{
				FNiagaraSystemInstance* Inst = PendingSystemInstances.Pop();

				Inst->TickDataInterfaces(DeltaSeconds, false);
				Inst->SetPendingSpawn(false);
				if (!Inst->IsComplete())
				{
					Inst->SystemInstanceIndex = SystemInstances.Add(Inst);
					++SpawnNum;
				}
				else
				{
					checkSlow(Inst->SystemInstanceIndex == INDEX_NONE);
				}
			}
		}

		NewNum = OrigNum + SpawnNum;
		check(NewNum == SystemInstances.Num());
	}

	if (bCanExecute && NewNum > 0)
	{
		if (GbDumpSystemData || System->bDumpDebugSystemInfo)
		{
			UE_LOG(LogNiagara, Log, TEXT("=========================================================="));
			UE_LOG(LogNiagara, Log, TEXT("Niagara System Sim Tick: %s"), *System->GetName());
			UE_LOG(LogNiagara, Log, TEXT("=========================================================="));
		}

		TArray<FNiagaraDataSetExecutionInfo, TInlineAllocator<8>> DataSetExecInfos;
		DataSetExecInfos.SetNum(2);
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_PrepareForSimulate);
			auto PreSimulateAndTransferParams = [&](int32 SystemIndex)
			{
				FNiagaraSystemInstance* Inst = SystemInstances[SystemIndex];
				Inst->PreSimulateTick(DeltaSeconds);

				if (Inst->GetParameters().GetParametersDirty() && bCanExecute)
				{
					SpawnInstanceParameterToDataSetBinding.ParameterStoreToDataSet(Inst->GetParameters(), SpawnInstanceParameterDataSet, SystemIndex);
					UpdateInstanceParameterToDataSetBinding.ParameterStoreToDataSet(Inst->GetParameters(), UpdateInstanceParameterDataSet, SystemIndex);
				}

				//TODO: Find good way to check that we're not using any instance parameter data interfaces in the system scripts here.
				//In that case we need to solo and will never get here.

				TArray<TSharedRef<FNiagaraEmitterInstance>>& Emitters = Inst->GetEmitters();
				for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
				{
					FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();
					if (EmitterExecutionStateAccessors.Num() > EmitterIdx && EmitterExecutionStateAccessors[EmitterIdx].BaseIsValid())
					{
						EmitterExecutionStateAccessors[EmitterIdx].Set(SystemIndex, (int32)EmitterInst.GetExecutionState());
					}
				}
			};

			InitBindings(SystemInstances[0]);

			SpawnInstanceParameterDataSet.Allocate(NewNum);
			UpdateInstanceParameterDataSet.Allocate(NewNum);

			for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
			{
				EmitterExecutionStateAccessors[EmitterIdx].InitForAccess(true);
			}

			//Transfer any values like execution state from the system instance into the dataset for simulation.
			ParallelFor(SystemInstances.Num(), [&](int32 SystemIndex)
			{
				PreSimulateAndTransferParams(SystemIndex);
			});

			SpawnInstanceParameterDataSet.SetNumInstances(NewNum);
			UpdateInstanceParameterDataSet.SetNumInstances(NewNum);
			SpawnInstanceParameterDataSet.Tick();
			UpdateInstanceParameterDataSet.Tick();

			//Setup the few real constants like delta time.
			float InvDt = 1.0f / DeltaSeconds;
			float GlobalSpawnCountScale = INiagaraModule::GetGlobalSpawnCountScale();
			float GlobalSystemCountScale = INiagaraModule::GetGlobalSystemCountScale();
			SpawnDeltaTimeParam.SetValue(DeltaSeconds);
			UpdateDeltaTimeParam.SetValue(DeltaSeconds);
			SpawnInvDeltaTimeParam.SetValue(InvDt);
			UpdateInvDeltaTimeParam.SetValue(InvDt);
			SpawnNumSystemInstancesParam.SetValue(NewNum);
			UpdateNumSystemInstancesParam.SetValue(NewNum);
			SpawnGlobalSpawnCountScaleParam.SetValue(GlobalSpawnCountScale);
			UpdateGlobalSpawnCountScaleParam.SetValue(GlobalSpawnCountScale);
			SpawnGlobalSystemCountScaleParam.SetValue(GlobalSystemCountScale);
			UpdateGlobalSystemCountScaleParam.SetValue(GlobalSystemCountScale);
		}

		//TODO: JIRA - UE-60096 - Remove.
		//We're having to allocate and spawn before update here so we have to do needless copies.			
		//Ideally this should be compiled directly into the script similarly to interpolated particle spawning.
		if (SpawnNum)
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_Spawn);
			DataSet.Allocate(NewNum, true);

			DataSet.SetNumInstances(NewNum);
			//SpawnInstanceParameterDataSet.SetNumInstances(NewNum);

			//Run Spawn
			SpawnExecContext.Tick(SoloSystemInstance);//We can't require a specific instance here as these are for all instances.
			DataSetExecInfos[0] = FNiagaraDataSetExecutionInfo(&DataSet, OrigNum, false, false);
			DataSetExecInfos[1] = FNiagaraDataSetExecutionInfo(&SpawnInstanceParameterDataSet, OrigNum, false, false);
			SpawnExecContext.Execute(SpawnNum, DataSetExecInfos);

			if (GbDumpSystemData || System->bDumpDebugSystemInfo)
			{
				UE_LOG(LogNiagara, Log, TEXT("=== Spwaned %d Systems ==="), SpawnNum);
				DataSet.Dump(true, OrigNum, SpawnNum);
				SpawnInstanceParameterDataSet.Dump(false, OrigNum, SpawnNum);
			}

#if WITH_EDITORONLY_DATA
			if (SoloSystemInstance && SoloSystemInstance->ShouldCaptureThisFrame())
			{
				FNiagaraScriptDebuggerInfo* DebugInfo = SoloSystemInstance->GetActiveCaptureWrite(NAME_None, ENiagaraScriptUsage::SystemSpawnScript, FGuid());
				if (DebugInfo)
				{
					DataSet.Dump(DebugInfo->Frame, true, OrigNum, SpawnNum);
					//DebugInfo->Frame.Dump(true, 0, SpawnNum);
					DebugInfo->Parameters = SpawnExecContext.Parameters;
				}
			}
#endif
		}

		DataSet.Tick();
		DataSet.Allocate(NewNum);

		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_Update);
			DataSet.SetNumInstances(NewNum);
			//UpdateInstanceParameterDataSet.SetNumInstances(OrigNum);

			//Run update.
			UpdateExecContext.Tick(SystemInstances[0]);
			DataSetExecInfos[0] = FNiagaraDataSetExecutionInfo(&DataSet, 0, false, false);
			DataSetExecInfos[1] = FNiagaraDataSetExecutionInfo(&UpdateInstanceParameterDataSet, 0, false, false);

// 			if (GbDumpSystemData || System->bDumpDebugSystemInfo)
// 			{
// 				UE_LOG(LogNiagara, Log, TEXT("=== PreUpdated %d Systems ==="), OrigNum);
// 				DataSet.Dump(false, 0, OrigNum);
// 				UpdateInstanceParameterDataSet.Dump(false, 0, OrigNum);
// 			}

			UpdateExecContext.Execute(OrigNum, DataSetExecInfos);

			if (GbDumpSystemData || System->bDumpDebugSystemInfo)
			{
				UE_LOG(LogNiagara, Log, TEXT("=== Updated %d Systems ==="), OrigNum);
				DataSet.Dump(true, 0, OrigNum);
				UpdateInstanceParameterDataSet.Dump(false, 0, OrigNum);
			}

 			//Also run the update script on the newly spawned systems too.
			//TODO: JIRA - UE-60096 - Remove.
			//Ideally this should be compiled directly into the script similarly to interpolated particle spawning.
			if (SpawnNum && GbSystemUpdateOnSpawn)
			{
				DataSet.SetNumInstances(NewNum);

				//Run update.
				UpdateExecContext.Tick(SystemInstances[0]);
				DataSetExecInfos[0] = FNiagaraDataSetExecutionInfo(&DataSet, OrigNum, false, false);
				DataSetExecInfos[1] = FNiagaraDataSetExecutionInfo(&UpdateInstanceParameterDataSet, OrigNum, false, false);

				UpdateExecContext.Parameters.SetParameterValue(0.0001f, SYS_PARAM_ENGINE_DELTA_TIME);
				UpdateExecContext.Parameters.SetParameterValue(10000.0f, SYS_PARAM_ENGINE_INV_DELTA_TIME);

				UpdateExecContext.Execute(SpawnNum, DataSetExecInfos);

				if (GbDumpSystemData || System->bDumpDebugSystemInfo)
				{
					UE_LOG(LogNiagara, Log, TEXT("=== Spawn Updated %d Systems ==="), SpawnNum);
					DataSet.Dump(true, OrigNum, SpawnNum);
					UpdateInstanceParameterDataSet.Dump(false, OrigNum, SpawnNum);
				}
			}

#if WITH_EDITORONLY_DATA
			if (SoloSystemInstance && SoloSystemInstance->ShouldCaptureThisFrame())
			{
				FNiagaraScriptDebuggerInfo* DebugInfo = SoloSystemInstance->GetActiveCaptureWrite(NAME_None, ENiagaraScriptUsage::SystemUpdateScript, FGuid());
				if (DebugInfo)
				{
					DataSet.Dump(DebugInfo->Frame, true, 0, NewNum);
					//DebugInfo->Frame.Dump(true, 0, OrigNum);
					DebugInfo->Parameters = UpdateExecContext.Parameters;
				}
			}
#endif
		}

		SystemExecutionStateAccessor.InitForAccess(true);
		for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
		{
			EmitterExecutionStateAccessors[EmitterIdx].InitForAccess(true);
			for (int32 SpawnInfoIdx = 0; SpawnInfoIdx < EmitterSpawnInfoAccessors[EmitterIdx].Num(); ++SpawnInfoIdx)
			{
				EmitterSpawnInfoAccessors[EmitterIdx][SpawnInfoIdx].InitForAccess(true);
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_TransferParameters);
			int32 SystemIndex = 0;
			while (SystemIndex < SystemInstances.Num())
			{
				ENiagaraExecutionState ExecutionState = (ENiagaraExecutionState)SystemExecutionStateAccessor.GetSafe(SystemIndex, (int32)ENiagaraExecutionState::Disabled);
				FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];

				//Apply the systems requested execution state to it's actual execution state.
				SystemInst->SetActualExecutionState(ExecutionState);

				if (!SystemInst->IsDisabled() && !SystemInst->HandleCompletion())
				{
					//Now pull data out of the simulation and drive the emitters with it.
					TArray<TSharedRef<FNiagaraEmitterInstance>>& Emitters = SystemInst->GetEmitters();
					for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
					{
						FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();

						//Early exit before we set the state as if we're complete or disabled we should never let the emitter turn itself back. It needs to be reset/reinited manually.
						if (EmitterInst.IsComplete())
						{
							continue;
						}

						ENiagaraExecutionState State = (ENiagaraExecutionState)EmitterExecutionStateAccessors[EmitterIdx].GetSafe(SystemIndex, (int32)ENiagaraExecutionState::Disabled);
						EmitterInst.SetExecutionState(State);
												
						TArray<FNiagaraSpawnInfo>& EmitterInstSpawnInfos = EmitterInst.GetSpawnInfo();
						for (int32 SpawnInfoIdx=0; SpawnInfoIdx < EmitterSpawnInfoAccessors[EmitterIdx].Num(); ++SpawnInfoIdx)
						{
							if (SpawnInfoIdx < EmitterInstSpawnInfos.Num())
							{
								EmitterInstSpawnInfos[SpawnInfoIdx] = EmitterSpawnInfoAccessors[EmitterIdx][SpawnInfoIdx].Get(SystemIndex);
							}
							else
							{
								ensure(SpawnInfoIdx < EmitterInstSpawnInfos.Num());
							}
						}


						//TODO: Any other fixed function stuff like this?

						FNiagaraScriptExecutionContext& SpawnContext = EmitterInst.GetSpawnExecutionContext();
						DataSetToEmitterSpawnParameters[EmitterIdx].DataSetToParameterStore(SpawnContext.Parameters, DataSet, SystemIndex);

						FNiagaraScriptExecutionContext& UpdateContext = EmitterInst.GetUpdateExecutionContext();
						DataSetToEmitterUpdateParameters[EmitterIdx].DataSetToParameterStore(UpdateContext.Parameters, DataSet, SystemIndex);

						TArray<FNiagaraScriptExecutionContext>& EventContexts = EmitterInst.GetEventExecutionContexts();
						for (int32 EventIdx = 0; EventIdx < EventContexts.Num(); ++EventIdx)
						{
							FNiagaraScriptExecutionContext& EventContext = EventContexts[EventIdx];
							if (DataSetToEmitterEventParameters[EmitterIdx].Num() > EventIdx)
							{
								DataSetToEmitterEventParameters[EmitterIdx][EventIdx].DataSetToParameterStore(EventContext.Parameters, DataSet, SystemIndex);
							}
							else
							{
								UE_LOG(LogNiagara, Log, TEXT("Skipping DataSetToEmitterEventParameters because EventIdx is out-of-bounds. %d of %d"), EventIdx, DataSetToEmitterEventParameters[EmitterIdx].Num());
							}
						}
					}

					//System is still enabled. 
					++SystemIndex;
				}
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_PostSimulate);

		if (GbParallelSystemPostTick)
		{
			ParallelFor(SystemInstances.Num(),
				[&](int32 SystemIndex)
			{
				FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
				SystemInst->PostSimulateTick(DeltaSeconds);
			});
		}
		else
		{
			//Now actually tick emitters.
			for (int32 SystemIndex = 0; SystemIndex < SystemInstances.Num(); ++SystemIndex)
			{
				FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
				SystemInst->PostSimulateTick(DeltaSeconds);
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_MarkComponentDirty);
		//This is not a small amount of the update time. 
		//Annoyingly these can't be done in parallel it seems.
		//TODO: Find some way to parallelize this. Especially UpdateComponentToWorld();
		int32 SystemIndex = 0;
		while (SystemIndex < SystemInstances.Num())
		{
			FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
			++SystemIndex;
			if (SystemIndex < SystemInstances.Num())
			{
				FPlatformMisc::Prefetch(SystemInstances[SystemIndex]->GetComponent());
			}
			SystemInst->FinalizeTick(DeltaSeconds);
		}
	}

#if WITH_EDITORONLY_DATA
	if (SoloSystemInstance != nullptr)
	{
		SoloSystemInstance->FinishCapture();
	}
#endif

	INC_DWORD_STAT_BY(STAT_NiagaraNumSystems, SystemInstances.Num());

	return true;
}

void FNiagaraSystemSimulation::RemoveInstance(FNiagaraSystemInstance* Instance)
{
	if (Instance->SystemInstanceIndex == INDEX_NONE)
	{
		return;
	}

	UNiagaraSystem* System = WeakSystem.Get();
	if (Instance->IsPendingSpawn())
	{
		int32 SystemIndex = Instance->SystemInstanceIndex;
		check(Instance == PendingSystemInstances[SystemIndex]);
		PendingSystemInstances.RemoveAtSwap(SystemIndex);
		Instance->SystemInstanceIndex = INDEX_NONE;
		Instance->SetPendingSpawn(false);
		if (PendingSystemInstances.IsValidIndex(SystemIndex))
		{
			PendingSystemInstances[SystemIndex]->SystemInstanceIndex = SystemIndex;
		}
	}
	else if (SystemInstances.IsValidIndex(Instance->SystemInstanceIndex))
	{
		if (GbDumpSystemData || (System && System->bDumpDebugSystemInfo))
		{
			UE_LOG(LogNiagara, Log, TEXT("=== Removing System %d ==="), Instance->SystemInstanceIndex);
			DataSet.Dump(true, Instance->SystemInstanceIndex, 1);
		}

		int32 NumInstances = DataSet.GetNumInstances();
		check(SystemInstances.Num() == NumInstances);

		int32 SystemIndex = Instance->SystemInstanceIndex;
		check(Instance == SystemInstances[SystemIndex]);
		check(SystemInstances.IsValidIndex(SystemIndex));
		DataSet.KillInstance(SystemIndex);
		SystemInstances.RemoveAtSwap(SystemIndex);
		Instance->SystemInstanceIndex = INDEX_NONE;

		if (SystemInstances.IsValidIndex(SystemIndex))
		{
			SystemInstances[SystemIndex]->SystemInstanceIndex = SystemIndex;
		}
	}

#if NIAGARA_NAN_CHECKING
	DataSet.CheckForNaNs();
#endif
}

void FNiagaraSystemSimulation::AddInstance(FNiagaraSystemInstance* Instance)
{
	Instance->SetPendingSpawn(true);
	Instance->SystemInstanceIndex = PendingSystemInstances.Add(Instance);
}

void FNiagaraSystemSimulation::InitBindings(FNiagaraSystemInstance* SystemInst)
{
	//Have to init here as we need an actual parameter store to pull the layout info from.
	//TODO: Pull the layout stuff out of each data set and store. So much duplicated data.
	//This assumes that all layouts for all emitters is the same. Which it should be.
	//Ideally we can store all this layout info in the systm/emitter assets so we can just generate this in Init()
	if (DataSetToEmitterSpawnParameters.Num() == 0 && SystemInst != nullptr)
	{
		SpawnInstanceParameterToDataSetBinding.Init(SpawnInstanceParameterDataSet, SystemInst->GetInstanceParameters());
		UpdateInstanceParameterToDataSetBinding.Init(UpdateInstanceParameterDataSet, SystemInst->GetInstanceParameters());

		TArray<TSharedRef<FNiagaraEmitterInstance>>& Emitters = SystemInst->GetEmitters();
		check(DataSetToEmitterUpdateParameters.Num() == 0);
		check(DataSetToEmitterEventParameters.Num() == 0);
		DataSetToEmitterSpawnParameters.SetNum(Emitters.Num());
		DataSetToEmitterUpdateParameters.SetNum(Emitters.Num());
		DataSetToEmitterEventParameters.SetNum(Emitters.Num());
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
		{
			FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();
			FNiagaraScriptExecutionContext& SpawnContext = EmitterInst.GetSpawnExecutionContext();
			DataSetToEmitterSpawnParameters[EmitterIdx].Init(DataSet, SpawnContext.Parameters);

			FNiagaraScriptExecutionContext& UpdateContext = EmitterInst.GetUpdateExecutionContext();
			DataSetToEmitterUpdateParameters[EmitterIdx].Init(DataSet, UpdateContext.Parameters);

			TArray<FNiagaraScriptExecutionContext>& EventContexts = EmitterInst.GetEventExecutionContexts();
			DataSetToEmitterEventParameters[EmitterIdx].SetNum(EventContexts.Num());
			for (int32 EventIdx = 0; EventIdx < EventContexts.Num(); ++EventIdx)
			{
				FNiagaraScriptExecutionContext& EventContext = EventContexts[EventIdx];
				DataSetToEmitterEventParameters[EmitterIdx][EventIdx].Init(DataSet, EventContext.Parameters);
			}
		}
	}

	// If we have data interfaces (say user ones) that need to be pushed to the 
	// system spawn and update scripts, it needs to happen here.
	if (SystemInst)
	{
		SystemInst->GetInstanceParameters().Bind(&SpawnExecContext.Parameters);
		SystemInst->GetInstanceParameters().Bind(&UpdateExecContext.Parameters);

	}
}

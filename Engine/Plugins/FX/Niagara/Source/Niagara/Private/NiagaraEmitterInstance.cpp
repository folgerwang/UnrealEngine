// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterInstance.h"
#include "Materials/Material.h"
#include "VectorVM.h"
#include "NiagaraStats.h"
#include "NiagaraConstants.h"
#include "NiagaraRenderer.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraDataInterface.h"
#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraScriptExecutionContext.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraScriptExecutionContext.h"
#include "NiagaraWorldManager.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Custom Events"), STAT_NiagaraNumCustomEvents, STATGROUP_Niagara);

//DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_NiagaraTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Simulate"), STAT_NiagaraSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Spawn"), STAT_NiagaraSpawn, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Spawn"), STAT_NiagaraEvents, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Kill"), STAT_NiagaraKill, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Event Handling"), STAT_NiagaraEventHandle, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Error Check"), STAT_NiagaraEmitterErrorCheck, STATGROUP_Niagara);

static int32 GbDumpParticleData = 0;
static FAutoConsoleVariableRef CVarNiagaraDumpParticleData(
	TEXT("fx.DumpParticleData"),
	GbDumpParticleData,
	TEXT("If > 0 current frame particle data will be dumped after simulation. \n"),
	ECVF_Default
	);

/**
TODO: This is mainly to avoid hard limits in our storage/alloc code etc rather than for perf reasons.
We should improve our hard limit/safety code and possibly add a max for perf reasons.
*/
static int32 GMaxNiagaraCPUParticlesPerEmitter = 1000000;
static FAutoConsoleVariableRef CVarMaxNiagaraCPUParticlesPerEmitter(
	TEXT("fx.MaxNiagaraCPUParticlesPerEmitter"),
	GMaxNiagaraCPUParticlesPerEmitter,
	TEXT("The max number of supported CPU particles per emitter in Niagara. \n"),
	ECVF_Default
);
//////////////////////////////////////////////////////////////////////////

const FName FNiagaraEmitterInstance::PositionName(TEXT("Position"));
const FName FNiagaraEmitterInstance::SizeName(TEXT("SpriteSize"));
const FName FNiagaraEmitterInstance::MeshScaleName(TEXT("Scale"));

FNiagaraEmitterInstance::FNiagaraEmitterInstance(FNiagaraSystemInstance* InParentSystemInstance)
: CPUTimeMS(0.0f)
, ExecutionState(ENiagaraExecutionState::Inactive)
, CachedBounds(ForceInit)
, ParentSystemInstance(InParentSystemInstance)
, CachedEmitter(nullptr)
#if !UE_BUILD_SHIPPING
, bEncounteredNaNs(false)
#endif
{
	bDumpAfterEvent = false;
	ParticleDataSet = new FNiagaraDataSet();
}

FNiagaraEmitterInstance::~FNiagaraEmitterInstance()
{
	//UE_LOG(LogNiagara, Warning, TEXT("~Simulator %p"), this);
	ClearRenderer();
	CachedBounds.Init();
	UnbindParameters();

	if (CachedEmitter != nullptr && CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		NiagaraEmitterInstanceBatcher::Get()->Remove(&GPUExecContext);
	}

	/** We defer the deletion of the particle dataset to the RT to be sure all in-flight RT commands have finished using it.*/
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(FDeleteParticleDataSetCommand,
		FNiagaraDataSet*, DataSet, ParticleDataSet,
		{
			delete DataSet;
		});
}

void FNiagaraEmitterInstance::ClearRenderer()
{
	for (int32 i = 0; i < EmitterRenderer.Num(); i++)
	{
		if (EmitterRenderer[i])
		{
			//UE_LOG(LogNiagara, Warning, TEXT("ClearRenderer %p"), EmitterRenderer);
			// This queues up the renderer for deletion on the render thread..
			EmitterRenderer[i]->Release();
			EmitterRenderer[i] = nullptr;
		}
	}
}

FBox FNiagaraEmitterInstance::GetBounds()
{
	return CachedBounds;
}

bool FNiagaraEmitterInstance::IsReadyToRun() const
{
	if (!CachedEmitter->IsReadyToRun())
	{
		return false;
	}

	return true;
}

void FNiagaraEmitterInstance::Dump()const
{
	UE_LOG(LogNiagara, Log, TEXT("==  %s ========"), *CachedEmitter->GetUniqueEmitterName());
	UE_LOG(LogNiagara, Log, TEXT(".................Spawn................."));
	SpawnExecContext.Parameters.DumpParameters(true);
	UE_LOG(LogNiagara, Log, TEXT(".................Update................."));
	UpdateExecContext.Parameters.DumpParameters(true);
	UE_LOG(LogNiagara, Log, TEXT("................. %s Combined Parameters ................."), TEXT("GPU Script"));
	GPUExecContext.CombinedParamStore.DumpParameters();
	UE_LOG(LogNiagara, Log, TEXT("................. Particles ................."));
	ParticleDataSet->Dump(false);
	ParticleDataSet->Dump(true);
}

void FNiagaraEmitterInstance::Init(int32 InEmitterIdx, FName InSystemInstanceName)
{
	check(ParticleDataSet);
	FNiagaraDataSet& Data = *ParticleDataSet;
	EmitterIdx = InEmitterIdx;
	OwnerSystemInstanceName = InSystemInstanceName;
	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	CachedEmitter = EmitterHandle.GetInstance();
	checkSlow(CachedEmitter);
	CachedIDName = EmitterHandle.GetIdName();

	if (!EmitterHandle.GetIsEnabled()
		|| !CachedEmitter->IsAllowedByDetailLevel()
		|| (GMaxRHIFeatureLevel != ERHIFeatureLevel::SM5 && GMaxRHIFeatureLevel != ERHIFeatureLevel::ES3_1 && CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)  // skip if GPU sim and <SM5. TODO: fall back to CPU sim instead once we have scalability functionality to do so
		)
	{
		ExecutionState = ENiagaraExecutionState::Disabled;
		return;
	}

#if !UE_BUILD_SHIPPING
	bEncounteredNaNs = false;
#endif

	Data.Init(FNiagaraDataSetID(CachedIDName, ENiagaraDataSetType::ParticleData), CachedEmitter->SimTarget);

	//Init the spawn infos to the correct number for this system.
	const TArray<FNiagaraEmitterSpawnAttributes>& EmitterSpawnInfoAttrs = ParentSystemInstance->GetSystem()->GetEmitterSpawnAttributes();
	if (EmitterSpawnInfoAttrs.IsValidIndex(EmitterIdx))
	{
		SpawnInfos.SetNum(EmitterSpawnInfoAttrs[EmitterIdx].SpawnAttributes.Num());
	}

	CheckForErrors();

	if (IsDisabled())
	{
		return;
	}

	ResetSimulation();

	DataSetMap.Empty();

	//Add the particle data to the data set map.
	//Currently just used for the tick loop but will also allow access directly to the particle data from other emitters.
	DataSetMap.Add(Data.GetID(), &Data);
	//Warn the user if there are any attributes used in the update script that are not initialized in the spawn script.
	//TODO: We need some window in the System editor and possibly the graph editor for warnings and errors.

	const bool bVerboseAttributeLogging = false;

	if (bVerboseAttributeLogging)
	{
		for (FNiagaraVariable& Attr : CachedEmitter->UpdateScriptProps.Script->GetVMExecutableData().Attributes)
		{
			int32 FoundIdx;
			if (!CachedEmitter->SpawnScriptProps.Script->GetVMExecutableData().Attributes.Find(Attr, FoundIdx))
			{
				UE_LOG(LogNiagara, Warning, TEXT("Attribute %s is used in the Update script for %s but it is not initialised in the Spawn script!"), *Attr.GetName().ToString(), *EmitterHandle.GetName().ToString());
			}
			for (int32 i = 0; i < CachedEmitter->GetEventHandlers().Num(); i++)
			{
				if (CachedEmitter->GetEventHandlers()[i].Script && !CachedEmitter->GetEventHandlers()[i].Script->GetVMExecutableData().Attributes.Find(Attr, FoundIdx))
				{
					UE_LOG(LogNiagara, Warning, TEXT("Attribute %s is used in the event handler script for %s but it is not initialised in the Spawn script!"), *Attr.GetName().ToString(), *EmitterHandle.GetName().ToString());
				}
			}
		}
	}
	Data.AddVariables(CachedEmitter->UpdateScriptProps.Script->GetVMExecutableData().Attributes);
	Data.AddVariables(CachedEmitter->SpawnScriptProps.Script->GetVMExecutableData().Attributes);

	//if we use persistent IDs then add that here too.
	if (RequiredPersistentID())
	{
		Data.SetNeedsPersistentIDs(true);
	}

	Data.Finalize();

	ensure(CachedEmitter->UpdateScriptProps.DataSetAccessSynchronized());
	UpdateScriptEventDataSets.Empty();
	for (const FNiagaraEventGeneratorProperties &GeneratorProps : CachedEmitter->UpdateScriptProps.EventGenerators)
	{
		FNiagaraDataSet *Set = FNiagaraEventDataSetMgr::CreateEventDataSet(ParentSystemInstance->GetIDName(), EmitterHandle.GetIdName(), GeneratorProps.SetProps.ID.Name);
		Set->Init(FNiagaraDataSetID(), ENiagaraSimTarget::CPUSim);
		Set->AddVariables(GeneratorProps.SetProps.Variables);
		Set->Finalize();
		UpdateScriptEventDataSets.Add(Set);
	}

	ensure(CachedEmitter->SpawnScriptProps.DataSetAccessSynchronized());
	SpawnScriptEventDataSets.Empty();
	for (const FNiagaraEventGeneratorProperties &GeneratorProps : CachedEmitter->SpawnScriptProps.EventGenerators)
	{
		FNiagaraDataSet *Set = FNiagaraEventDataSetMgr::CreateEventDataSet(ParentSystemInstance->GetIDName(), EmitterHandle.GetIdName(), GeneratorProps.SetProps.ID.Name);
		Set->Init(FNiagaraDataSetID(), ENiagaraSimTarget::CPUSim);
		Set->AddVariables(GeneratorProps.SetProps.Variables);
		Set->Finalize();
		SpawnScriptEventDataSets.Add(Set);
	}

	SpawnExecContext.Init(CachedEmitter->SpawnScriptProps.Script, CachedEmitter->SimTarget);
	UpdateExecContext.Init(CachedEmitter->UpdateScriptProps.Script, CachedEmitter->SimTarget);

	// setup the parameer store for the GPU execution context; since spawn and update are combined here, we build one with params from both script props
	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		GPUExecContext.InitParams(CachedEmitter->GetGPUComputeScript(), CachedEmitter->SpawnScriptProps.Script, CachedEmitter->UpdateScriptProps.Script, CachedEmitter->SimTarget);
		SpawnExecContext.Parameters.Bind(&GPUExecContext.CombinedParamStore);
		UpdateExecContext.Parameters.Bind(&GPUExecContext.CombinedParamStore);
	}

	EventExecContexts.SetNum(CachedEmitter->GetEventHandlers().Num());
	int32 NumEvents = CachedEmitter->GetEventHandlers().Num();
	for (int32 i = 0; i < NumEvents; i++)
	{
		ensure(CachedEmitter->GetEventHandlers()[i].DataSetAccessSynchronized());

		UNiagaraScript* EventScript = CachedEmitter->GetEventHandlers()[i].Script;

		//This is cpu explicitly? Are we doing event handlers on GPU?
		EventExecContexts[i].Init(EventScript, ENiagaraSimTarget::CPUSim);
	}

	//Setup direct bindings for setting parameter values.
	SpawnIntervalBinding.Init(SpawnExecContext.Parameters, CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_SPAWN_INTERVAL));
	InterpSpawnStartBinding.Init(SpawnExecContext.Parameters, CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT));
	SpawnGroupBinding.Init(SpawnExecContext.Parameters, CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_SPAWN_GROUP));

	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		SpawnIntervalBindingGPU.Init(GPUExecContext.CombinedParamStore, CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_SPAWN_INTERVAL));
		InterpSpawnStartBindingGPU.Init(GPUExecContext.CombinedParamStore, CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT));
		SpawnGroupBindingGPU.Init(GPUExecContext.CombinedParamStore, CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_SPAWN_GROUP));
	}

	FNiagaraVariable EmitterAgeParam = CachedEmitter->ToEmitterParameter(SYS_PARAM_EMITTER_AGE);
	SpawnEmitterAgeBinding.Init(SpawnExecContext.Parameters, EmitterAgeParam);
	UpdateEmitterAgeBinding.Init(UpdateExecContext.Parameters, EmitterAgeParam);
	EventEmitterAgeBindings.SetNum(NumEvents);
	for (int32 i = 0; i < NumEvents; i++)
	{
		EventEmitterAgeBindings[i].Init(EventExecContexts[i].Parameters, EmitterAgeParam);
	}

	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		EmitterAgeBindingGPU.Init(GPUExecContext.CombinedParamStore, EmitterAgeParam);
	}

	SpawnExecCountBinding.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_EXEC_COUNT);
	UpdateExecCountBinding.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_EXEC_COUNT);
	EventExecCountBindings.SetNum(NumEvents);
	for (int32 i = 0; i < NumEvents; i++)
	{
		EventExecCountBindings[i].Init(EventExecContexts[i].Parameters, SYS_PARAM_ENGINE_EXEC_COUNT);
	}

	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		//Just ensure we've generated the singleton here on the GT as it throws a wobbler if we do this later in parallel.
		NiagaraEmitterInstanceBatcher::Get();
	}
	else
	{
		//Init accessors for PostProcessParticles
		PositionAccessor = FNiagaraDataSetAccessor<FVector>(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), PositionName));
		SizeAccessor = FNiagaraDataSetAccessor<FVector2D>(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), SizeName));
		MeshScaleAccessor = FNiagaraDataSetAccessor<FVector>(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), MeshScaleName));
	}

	// Collect script defined data interface parameters.
	TArray<UNiagaraScript*> Scripts;
	Scripts.Add(CachedEmitter->SpawnScriptProps.Script);
	Scripts.Add(CachedEmitter->UpdateScriptProps.Script);
	for (const FNiagaraEventScriptProperties& EventHandler : CachedEmitter->GetEventHandlers())
	{
		Scripts.Add(EventHandler.Script);
	}
	FNiagaraUtilities::CollectScriptDataInterfaceParameters(*CachedEmitter, Scripts, ScriptDefinedDataInterfaceParameters);
}

void FNiagaraEmitterInstance::ResetSimulation()
{
	bResetPending = true;
	Age = 0;
	Loops = 0;
	TickCount = 0;
	CachedBounds.Init();

	ParticleDataSet->ResetBuffers();
	for (FNiagaraDataSet* SpawnScriptEventDataSet : SpawnScriptEventDataSets)
	{
		SpawnScriptEventDataSet->ResetBuffers();
	}
	for (FNiagaraDataSet* UpdateScriptEventDataSet : UpdateScriptEventDataSets)
	{
		UpdateScriptEventDataSet->ResetBuffers();
	}
	
	GPUExecContext.Reset();

	SetExecutionState(ENiagaraExecutionState::Active);
}

void FNiagaraEmitterInstance::CheckForErrors()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEmitterErrorCheck);
	
	checkSlow(CachedEmitter);

	//Check for various failure conditions and bail.
	if (!CachedEmitter->UpdateScriptProps.Script || !CachedEmitter->SpawnScriptProps.Script )
	{
		//TODO - Arbitrary named scripts. Would need some base functionality for Spawn/Udpate to be called that can be overriden in BPs for emitters with custom scripts.
		UE_LOG(LogNiagara, Error, TEXT("Emitter cannot be enabled because it's doesn't have both an update and spawn script."), *CachedEmitter->GetFullName());
		SetExecutionState(ENiagaraExecutionState::Disabled);
		return;
	}

	if (!CachedEmitter->UpdateScriptProps.Script->IsReadyToRun(ENiagaraSimTarget::CPUSim) || !CachedEmitter->SpawnScriptProps.Script->IsReadyToRun(ENiagaraSimTarget::CPUSim))
	{
		//TODO - Arbitrary named scripts. Would need some base functionality for Spawn/Udpate to be called that can be overriden in BPs for emitters with custom scripts.
		UE_LOG(LogNiagara, Error, TEXT("Emitter cannot be enabled because it's doesn't have both an update and spawn script ready to run CPU scripts."), *CachedEmitter->GetFullName());
		SetExecutionState(ENiagaraExecutionState::Disabled);
		return;
	}

	if (CachedEmitter->SpawnScriptProps.Script->GetVMExecutableData().DataUsage.bReadsAttributeData)
	{
		UE_LOG(LogNiagara, Error, TEXT("%s reads attribute data and so cannot be used as a spawn script. The data being read would be invalid."), *CachedEmitter->SpawnScriptProps.Script->GetName());
		SetExecutionState(ENiagaraExecutionState::Disabled);
		return;
	}
	if (CachedEmitter->UpdateScriptProps.Script->GetVMExecutableData().Attributes.Num() == 0 || CachedEmitter->SpawnScriptProps.Script->GetVMExecutableData().Attributes.Num() == 0)
	{
		UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's spawn or update script doesn't have any attriubtes.."));
		SetExecutionState(ENiagaraExecutionState::Disabled);
		return;
	}

	if (CachedEmitter->SimTarget == ENiagaraSimTarget::CPUSim || CachedEmitter->SimTarget == ENiagaraSimTarget::DynamicLoadBalancedSim)
	{
		bool bFailed = false;
		if (!CachedEmitter->SpawnScriptProps.Script->DidScriptCompilationSucceed(false))
		{
			bFailed = true;
			UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's CPU Spawn script failed to compile."));
		}

		if (!CachedEmitter->UpdateScriptProps.Script->DidScriptCompilationSucceed(false))
		{
			bFailed = true;
			UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's CPU Update script failed to compile."));
		}

		if (CachedEmitter->GetEventHandlers().Num() != 0)
		{
			for (int32 i = 0; i < CachedEmitter->GetEventHandlers().Num(); i++)
			{
				if (!CachedEmitter->GetEventHandlers()[i].Script->DidScriptCompilationSucceed(false))
				{
					bFailed = true;
					UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because one of it's CPU Event scripts failed to compile."));
				}
			}
		}

		if (bFailed)
		{
			SetExecutionState(ENiagaraExecutionState::Disabled);
			return;
		}
	}

	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim || CachedEmitter->SimTarget == ENiagaraSimTarget::DynamicLoadBalancedSim)
	{
		if (CachedEmitter->GetGPUComputeScript()->IsScriptCompilationPending(true))
		{
			UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's GPU script hasn't been compiled.."));
			SetExecutionState(ENiagaraExecutionState::Disabled);
			return;
		}
		if (!CachedEmitter->GetGPUComputeScript()->DidScriptCompilationSucceed(true))
		{
			UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's GPU script failed to compile."));
			SetExecutionState(ENiagaraExecutionState::Disabled);
			return;
		}
	}
}

void FNiagaraEmitterInstance::DirtyDataInterfaces()
{
	// Make sure that our function tables need to be regenerated...
	SpawnExecContext.DirtyDataInterfaces();
	UpdateExecContext.DirtyDataInterfaces();
	GPUExecContext.DirtyDataInterfaces();

	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		EventContext.DirtyDataInterfaces();
	}
}

//Unsure on usage of this atm. Possibly useful in future.
// void FNiagaraEmitterInstance::RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance)
// {
// 	OldInstance->GetParameterStore().Unbind(&SpawnExecContext.Parameters);
// 	NewInstance->GetParameterStore().Bind(&SpawnExecContext.Parameters);
// 
// 	OldInstance->GetParameterStore().Unbind(&UpdateExecContext.Parameters);
// 	NewInstance->GetParameterStore().Bind(&UpdateExecContext.Parameters);
// 
// 	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
// 	{
// 		OldInstance->GetParameterStore().Unbind(&EventContext.Parameters);
// 		NewInstance->GetParameterStore().Bind(&EventContext.Parameters);
// 	}
// }

void FNiagaraEmitterInstance::UnbindParameters()
{
	SpawnExecContext.Parameters.UnbindFromSourceStores();
	UpdateExecContext.Parameters.UnbindFromSourceStores();

	for (int32 EventIdx = 0; EventIdx < EventExecContexts.Num(); ++EventIdx)
	{
		EventExecContexts[EventIdx].Parameters.UnbindFromSourceStores();
	}
}

void FNiagaraEmitterInstance::BindParameters()
{
	if (IsDisabled())
	{
		return;
	}

	FNiagaraWorldManager* WorldMan = ParentSystemInstance->GetWorldManager();
	check(WorldMan);

	for (UNiagaraParameterCollection* Collection : SpawnExecContext.Script->GetCachedParameterCollectionReferences())
	{
		ParentSystemInstance->GetParameterCollectionInstance(Collection)->GetParameterStore().Bind(&SpawnExecContext.Parameters);
	}
	for (UNiagaraParameterCollection* Collection : UpdateExecContext.Script->GetCachedParameterCollectionReferences())
	{
		ParentSystemInstance->GetParameterCollectionInstance(Collection)->GetParameterStore().Bind(&UpdateExecContext.Parameters);
	}

	for (int32 EventIdx = 0; EventIdx < EventExecContexts.Num(); ++EventIdx)
	{
		for (UNiagaraParameterCollection* Collection : EventExecContexts[EventIdx].Script->GetCachedParameterCollectionReferences())
		{
			ParentSystemInstance->GetParameterCollectionInstance(Collection)->GetParameterStore().Bind(&EventExecContexts[EventIdx].Parameters);
		}
	}

	//Now bind parameters from the component and system.
	FNiagaraParameterStore& InstanceParams = ParentSystemInstance->GetParameters();
	FNiagaraParameterStore& SystemScriptDefinedDataInterfaceParameters = ParentSystemInstance->GetSystemSimulation()->GetScriptDefinedDataInterfaceParameters();
	
	InstanceParams.Bind(&SpawnExecContext.Parameters);
	SystemScriptDefinedDataInterfaceParameters.Bind(&SpawnExecContext.Parameters);
	ScriptDefinedDataInterfaceParameters.Bind(&SpawnExecContext.Parameters);

	InstanceParams.Bind(&UpdateExecContext.Parameters);
	SystemScriptDefinedDataInterfaceParameters.Bind(&UpdateExecContext.Parameters);
	ScriptDefinedDataInterfaceParameters.Bind(&UpdateExecContext.Parameters);

	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		InstanceParams.Bind(&EventContext.Parameters);
		SystemScriptDefinedDataInterfaceParameters.Bind(&EventContext.Parameters);
		ScriptDefinedDataInterfaceParameters.Bind(&EventContext.Parameters);
	}

#if WITH_EDITORONLY_DATA
	CachedEmitter->SpawnScriptProps.Script->RapidIterationParameters.Bind(&SpawnExecContext.Parameters);
	CachedEmitter->UpdateScriptProps.Script->RapidIterationParameters.Bind(&UpdateExecContext.Parameters);
	ensure(CachedEmitter->GetEventHandlers().Num() == EventExecContexts.Num());
	for (int32 i = 0; i < CachedEmitter->GetEventHandlers().Num(); i++)
	{
		CachedEmitter->GetEventHandlers()[i].Script->RapidIterationParameters.Bind(&EventExecContexts[i].Parameters);
	}
#endif
}

void FNiagaraEmitterInstance::PostInitSimulation()
{
	if (!IsDisabled())
	{
		check(ParentSystemInstance);

		//Go through all our receivers and grab their generator sets so that the source emitters can do any init work they need to do.
		for (const FNiagaraEventReceiverProperties& Receiver : CachedEmitter->SpawnScriptProps.EventReceivers)
		{
			//FNiagaraDataSet* ReceiverSet = ParentSystemInstance->GetDataSet(FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event), Receiver.SourceEmitter);
			const FNiagaraDataSet* ReceiverSet = FNiagaraEventDataSetMgr::GetEventDataSet(ParentSystemInstance->GetIDName(), Receiver.SourceEmitter, Receiver.SourceEventGenerator);

		}

		for (const FNiagaraEventReceiverProperties& Receiver : CachedEmitter->UpdateScriptProps.EventReceivers)
		{
			//FNiagaraDataSet* ReceiverSet = ParentSystemInstance->GetDataSet(FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event), Receiver.SourceEmitter);
			const FNiagaraDataSet* ReceiverSet = FNiagaraEventDataSetMgr::GetEventDataSet(ParentSystemInstance->GetIDName(), Receiver.SourceEmitter, Receiver.SourceEventGenerator);
		}
	}
}

FNiagaraDataSet* FNiagaraEmitterInstance::GetDataSet(FNiagaraDataSetID SetID)
{
	FNiagaraDataSet** SetPtr = DataSetMap.Find(SetID);
	FNiagaraDataSet* Ret = NULL;
	if (SetPtr)
	{
		Ret = *SetPtr;
	}
	else
	{
		// TODO: keep track of data sets generated by the scripts (event writers) and find here
	}

	return Ret;
}

const FNiagaraEmitterHandle& FNiagaraEmitterInstance::GetEmitterHandle() const
{
	UNiagaraSystem* Sys = ParentSystemInstance->GetSystem();
	checkSlow(Sys->GetEmitterHandles().Num() > EmitterIdx);
	return Sys->GetEmitterHandles()[EmitterIdx];
}

float FNiagaraEmitterInstance::GetTotalCPUTime()
{
	float Total = CPUTimeMS;
	for (int32 i = 0; i < EmitterRenderer.Num(); i++)
	{
		if (EmitterRenderer[i])
		{
			Total += EmitterRenderer[i]->GetCPUTimeMS();

		}
	}

	return Total;
}

int FNiagaraEmitterInstance::GetTotalBytesUsed()
{
	check(ParticleDataSet);
	int32 BytesUsed = ParticleDataSet->GetSizeBytes();
	/*
	for (FNiagaraDataSet& Set : DataSets)
	{
		BytesUsed += Set.GetSizeBytes();
	}
	*/
	return BytesUsed;
}

TOptional<FBox> FNiagaraEmitterInstance::CalculateDynamicBounds()
{
	checkSlow(ParticleDataSet);
	FNiagaraDataSet& Data = *ParticleDataSet;
	int32 NumInstances = Data.GetNumInstances();
	FBox Ret;
	Ret.Init();

	if (IsComplete() || NumInstances == 0 || CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)//TODO: Pull data back from gpu buffers to get bounds for GPU sims.
	{
		return TOptional<FBox>();
	}

	PositionAccessor.InitForAccess(true);

	if (PositionAccessor.IsValid() == false)
	{
		return TOptional<FBox>();
	}

	SizeAccessor.InitForAccess(true);
	MeshScaleAccessor.InitForAccess(true);

	FVector MaxSize(ForceInitToZero);

	if (SizeAccessor.IsValid() == false && MeshScaleAccessor.IsValid() == false)
	{
		MaxSize = FVector(50.0f, 50.0f, 50.0f);
	}

	for (int32 InstIdx = 0; InstIdx < NumInstances && PositionAccessor.IsValid(); ++InstIdx)
	{
		FVector Position;
		PositionAccessor.Get(InstIdx, Position);

		// Some graphs have a tendency to divide by zero. This ContainsNaN has been added prophylactically
		// to keep us safe during GDC. It should be removed as soon as we feel safe that scripts are appropriately warned.
		if (!Position.ContainsNaN())
		{
			Ret += Position;

			// We advance the scale or size depending of if we use either.
			if (MeshScaleAccessor.IsValid())
			{
				MaxSize = MaxSize.ComponentMax(MeshScaleAccessor.Get(InstIdx));
			}
			else if (SizeAccessor.IsValid())
			{
				MaxSize = MaxSize.ComponentMax(FVector(SizeAccessor.Get(InstIdx).GetMax()));
			}
		}
		else
		{
#if !UE_BUILD_SHIPPING
			if (bEncounteredNaNs == false && ParentSystemInstance != nullptr && CachedEmitter != nullptr && ParentSystemInstance->GetSystem() != nullptr)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Particle position data contains NaNs. Likely a divide by zero somewhere in your modules. Emitter \"%s\" in System \"%s\""),
					*CachedEmitter->GetName(), *ParentSystemInstance->GetSystem()->GetName());
				bEncounteredNaNs = true;
				ParentSystemInstance->Dump();
			}
#endif
		}
	}

	float MaxBaseSize = 0.0001f;
	if (MaxSize.IsNearlyZero())
	{
		MaxSize = FVector(1.0f, 1.0f, 1.0f);
	}

	for (int32 i = 0; i < EmitterRenderer.Num(); i++)
	{
		if (EmitterRenderer[i])
		{
			FVector BaseExtents = EmitterRenderer[i]->GetBaseExtents();
			FVector ComponentMax;
			MaxBaseSize = BaseExtents.ComponentMax(FVector(MaxBaseSize, MaxBaseSize, MaxBaseSize)).GetMax();
		}
	}

	Ret = Ret.ExpandBy(MaxSize*MaxBaseSize);

	return Ret;
}

/** Look for dead particles and move from the end of the list to the dead location, compacting in the process
  * Also calculates bounds; Kill will be removed from this once we do conditional write
  */
void FNiagaraEmitterInstance::PostProcessParticles()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraKill);

	checkSlow(CachedEmitter);
	CachedBounds.Init();
	if (CachedEmitter->bFixedBounds || CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		CachedBounds = CachedEmitter->FixedBounds;
	}
	else
	{
		TOptional<FBox> DynamicBounds = CalculateDynamicBounds();
		if (DynamicBounds.IsSet())
		{
			if (CachedEmitter->bLocalSpace)
			{
				CachedBounds = DynamicBounds.GetValue();
			}
			else
			{
				CachedBounds = DynamicBounds.GetValue().TransformBy(ParentSystemInstance->GetComponent()->GetComponentToWorld().Inverse());
			}
		}
		else
		{
			CachedBounds = CachedEmitter->FixedBounds;
		}
	}
}

bool FNiagaraEmitterInstance::HandleCompletion(bool bForce)
{
	if (bForce)
	{
		SetExecutionState(ENiagaraExecutionState::Complete);
	}

	if (IsComplete())
	{
		//If we have any particles then clear out the buffers.
		if (ParticleDataSet->GetNumInstances() > 0 || ParticleDataSet->GetPrevNumInstances() > 0)
		{
			ParticleDataSet->ResetBuffers();
		}
		return true;
	}

	return false;
}

bool FNiagaraEmitterInstance::RequiredPersistentID()const
{
	//TODO: can we have this be enabled at runtime from outside the system?
	return GetEmitterHandle().GetInstance()->RequiresPersistantIDs() || ParticleDataSet->HasVariable(SYS_PARAM_PARTICLES_ID);
}

/** 
  * PreTick - handles killing dead particles, emitter death, and buffer swaps
  */
void FNiagaraEmitterInstance::PreTick()
{
	if (IsComplete())
	{
		return;
	}

	checkSlow(ParticleDataSet);
	FNiagaraDataSet& Data = *ParticleDataSet;

#if WITH_EDITOR
	CachedEmitter->SpawnScriptProps.Script->RapidIterationParameters.Tick();
	CachedEmitter->UpdateScriptProps.Script->RapidIterationParameters.Tick();
	ensure(CachedEmitter->GetEventHandlers().Num() == EventExecContexts.Num());
	for (int32 i = 0; i < CachedEmitter->GetEventHandlers().Num(); i++)
	{
		CachedEmitter->GetEventHandlers()[i].Script->RapidIterationParameters.Tick();
	}
#endif


	bool bOk = true;
	bOk &= SpawnExecContext.Tick(ParentSystemInstance);
	bOk &= UpdateExecContext.Tick(ParentSystemInstance);
	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		bOk &= GPUExecContext.Tick(ParentSystemInstance);
	}
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		bOk &= EventContext.Tick(ParentSystemInstance);
	}

	if (!bOk)
	{
		ResetSimulation();
		SetExecutionState(ENiagaraExecutionState::Disabled);
		return;
	}

	if (TickCount == 0)
	{
		//On our very first frame we prime any previous params (for interpolation).
		SpawnExecContext.PostTick();
		UpdateExecContext.PostTick();
		for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
		{
			EventContext.PostTick();
		}
	}

	checkSlow(Data.GetNumVariables() > 0);
	checkSlow(CachedEmitter->SpawnScriptProps.Script);
	checkSlow(CachedEmitter->UpdateScriptProps.Script);

	if (bResetPending)
	{
		Data.ResetCurrentBuffers();
		for (FNiagaraDataSet* SpawnScriptEventDataSet : SpawnScriptEventDataSets)
		{
			SpawnScriptEventDataSet->ResetCurrentBuffers();
		}
		for (FNiagaraDataSet* UpdateScriptEventDataSet : UpdateScriptEventDataSets)
		{
			UpdateScriptEventDataSet->ResetCurrentBuffers();
		}
		bResetPending = false;
	}

	//Swap all data set buffers before doing the main tick on any simulation.
	if (CachedEmitter->SimTarget == ENiagaraSimTarget::CPUSim)
	{
		for (TPair<FNiagaraDataSetID, FNiagaraDataSet*> SetPair : DataSetMap)
		{
			SetPair.Value->Tick();
		}

		for (FNiagaraDataSet* Set : UpdateScriptEventDataSets)
		{
			Set->Tick();
		}

		for (FNiagaraDataSet* Set : SpawnScriptEventDataSets)
		{
			Set->Tick();
		}
	}

	++TickCount;
	ParticleDataSet->SetIDAcquireTag(TickCount);
}


void FNiagaraEmitterInstance::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraTick);
	SimpleTimer TickTime;

	if (HandleCompletion())
	{
		CPUTimeMS = TickTime.GetElapsedMilliseconds();
		return;
	}

	checkSlow(ParticleDataSet);
	FNiagaraDataSet& Data = *ParticleDataSet;
	Age += DeltaSeconds;

	if (ExecutionState == ENiagaraExecutionState::InactiveClear)
	{
		Data.ResetBuffers();
		ExecutionState = ENiagaraExecutionState::Inactive;
		CPUTimeMS = TickTime.GetElapsedMilliseconds();
		return;
	}

	int32 OrigNumParticles = Data.GetPrevNumInstances();
	if (OrigNumParticles == 0 && ExecutionState != ENiagaraExecutionState::Active)
	{
		//Clear out curr buffer in case it had some data in previously.
		if (CachedEmitter->SimTarget == ENiagaraSimTarget::CPUSim)
		{
			Data.Allocate(0);
		}
		CPUTimeMS = TickTime.GetElapsedMilliseconds();
		return;
	}

	UNiagaraSystem* System = ParentSystemInstance->GetSystem();

	check(Data.GetNumVariables() > 0);
	check(CachedEmitter->SpawnScriptProps.Script);
	check(CachedEmitter->UpdateScriptProps.Script);
	
	//TickEvents(DeltaSeconds);

	// add system constants
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraConstants);
		float InvDT = 1.0f / DeltaSeconds;

		//TODO: Create a binding helper object for these to avoid the search.
		SpawnEmitterAgeBinding.SetValue(Age);
		UpdateEmitterAgeBinding.SetValue(Age);
		for (FNiagaraParameterDirectBinding<float>& Binding : EventEmitterAgeBindings)
		{
			Binding.SetValue(Age);
		}
		
		if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			EmitterAgeBindingGPU.SetValue(Age);
		}
	}
	
	// Calculate number of new particles from regular spawning 
	uint32 SpawnTotal = 0;
	if (ExecutionState == ENiagaraExecutionState::Active)
	{
		for (FNiagaraSpawnInfo& Info : SpawnInfos)
		{
			if (Info.Count > 0)
			{
				SpawnTotal += Info.Count;
			}
		}
	}

	// Calculate number of new particles from all event related spawns
	TArray<TArray<int32, TInlineAllocator<16>>, TInlineAllocator<16>> EventSpawnCounts;
	EventSpawnCounts.AddDefaulted(CachedEmitter->GetEventHandlers().Num());
	TArray<int32, TInlineAllocator<16>> EventHandlerSpawnCounts;
	EventHandlerSpawnCounts.AddDefaulted(CachedEmitter->GetEventHandlers().Num());
	uint32 EventSpawnTotal = 0;
	TArray<FNiagaraDataSet*, TInlineAllocator<16>> EventSet;
	EventSet.AddZeroed(CachedEmitter->GetEventHandlers().Num());
	TArray<FGuid, TInlineAllocator<16>> SourceEmitterGuid;
	SourceEmitterGuid.AddDefaulted(CachedEmitter->GetEventHandlers().Num());
	TArray<FName, TInlineAllocator<16>> SourceEmitterName;
	SourceEmitterName.AddDefaulted(CachedEmitter->GetEventHandlers().Num());
	TArray<bool, TInlineAllocator<16>> bPerformEventSpawning;
	bPerformEventSpawning.AddDefaulted(CachedEmitter->GetEventHandlers().Num());

	for (int32 i = 0; i < CachedEmitter->GetEventHandlers().Num(); i++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = CachedEmitter->GetEventHandlers()[i];
		SourceEmitterGuid[i] = EventHandlerProps.SourceEmitterID;
		SourceEmitterName[i] = SourceEmitterGuid[i].IsValid() ? *SourceEmitterGuid[i].ToString() : CachedIDName;
		EventSet[i] = FNiagaraEventDataSetMgr::GetEventDataSet(ParentSystemInstance->GetIDName(), SourceEmitterName[i], EventHandlerProps.SourceEventName);
		bPerformEventSpawning[i] = (ExecutionState == ENiagaraExecutionState::Active && EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::SpawnedParticles);
		if (bPerformEventSpawning[i])
		{
			uint32 EventSpawnNum = CalculateEventSpawnCount(EventHandlerProps, EventSpawnCounts[i], EventSet[i]);
			EventSpawnTotal += EventSpawnNum;
			EventHandlerSpawnCounts[i] = EventSpawnNum;
		}
	}


	/* GPU simulation -  we just create an FNiagaraComputeExecutionContext, queue it, and let the batcher take care of the rest
	 */
	if (CachedEmitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)	
	{
		//FNiagaraComputeExecutionContext *ComputeContext = new FNiagaraComputeExecutionContext();
		GPUExecContext.MainDataSet = &Data;
		GPUExecContext.RTGPUScript = CachedEmitter->GetGPUComputeScript()->GetRenderThreadScript();
		GPUExecContext.RTSpawnScript = CachedEmitter->SpawnScriptProps.Script->GetRenderThreadScript();
		GPUExecContext.RTUpdateScript = CachedEmitter->UpdateScriptProps.Script->GetRenderThreadScript();
		GPUExecContext.SpawnRateInstances = SpawnTotal;
		GPUExecContext.EventSpawnTotal = EventSpawnTotal;
		GPUExecContext.NumIndicesPerInstance = CachedEmitter->GetRenderers()[0]->GetNumIndicesPerInstance();

		bool bOnlySetOnce = false;
		for (FNiagaraSpawnInfo& Info : SpawnInfos)
		{
			if (Info.Count > 0 && !bOnlySetOnce)
			{
				SpawnIntervalBindingGPU.SetValue(Info.IntervalDt);
				InterpSpawnStartBindingGPU.SetValue(Info.InterpStartDt);
				SpawnGroupBindingGPU.SetValue(Info.SpawnGroup);
				bOnlySetOnce = true;
			}
			else if (Info.Count > 0)
			{
				UE_LOG(LogNiagara, Log, TEXT("Multiple spawns are happening this frame. Only doing the first!"));
				break;
			}
		}

		//GPUExecContext.UpdateInterfaces = CachedEmitter->UpdateScriptProps.Script->GetCachedDefaultDataInterfaces();

		// copy over the constants for the render thread
		//
		//UE_LOG(LogNiagara, Log, TEXT(".................Spawn................."));
		//SpawnExecContext.Parameters.DumpParameters();
		//UE_LOG(LogNiagara, Log, TEXT(".................Update................."));
		//UpdateExecContext.Parameters.DumpParameters();
		//UE_LOG(LogNiagara, Log, TEXT(".................GPU................."));
		//GPUExecContext.CombinedParamStore.DumpParameters();
		//UE_LOG(LogNiagara, Log, TEXT(".................END.................")); 
		if (GbDumpParticleData || System->bDumpDebugEmitterInfo)
		{
			UE_LOG(LogNiagara, Log, TEXT(".................Spawn................."));
			SpawnExecContext.Parameters.DumpParameters(true);
			UE_LOG(LogNiagara, Log, TEXT(".................Update................."));
			UpdateExecContext.Parameters.DumpParameters(true);
			UE_LOG(LogNiagara, Log, TEXT("................. %s Combined Parameters (%d Spawned )................."), TEXT("GPU Script"), SpawnTotal);
			GPUExecContext.CombinedParamStore.DumpParameters();
		}

		int32 ParmSize = GPUExecContext.CombinedParamStore.GetPaddedParameterSizeInBytes();
		
		GPUExecContext.ParamData_RT.SetNumZeroed(ParmSize);
		GPUExecContext.CombinedParamStore.CopyParameterDataToPaddedBuffer(GPUExecContext.ParamData_RT.GetData(), ParmSize);
		// Because each context is only ran once each frame, the CBuffer layout stays constant for the lifetime duration of the CBuffer (one frame).
		GPUExecContext.CBufferLayout.ConstantBufferSize = ParmSize;
		GPUExecContext.CBufferLayout.ComputeHash();

		// push event data sets to the context
		for (FNiagaraDataSet *Set : UpdateScriptEventDataSets)
		{
			GPUExecContext.UpdateEventWriteDataSets.Add(Set);
		}

		GPUExecContext.EventHandlerScriptProps = CachedEmitter->GetEventHandlers();
		GPUExecContext.EventSets = EventSet;
		GPUExecContext.EventSpawnCounts = EventHandlerSpawnCounts;
		NiagaraEmitterInstanceBatcher::Get()->Queue(&GPUExecContext);

		// Need to call post-tick, which calls the copy to previous for interpolated spawning
		SpawnExecContext.PostTick();
		UpdateExecContext.PostTick();
		for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
		{
			EventContext.PostTick();
		}

		CachedBounds = CachedEmitter->FixedBounds;

		CPUTimeMS = TickTime.GetElapsedMilliseconds();

		/*if (CachedEmitter->SpawnScriptProps.Script->GetComputedVMCompilationId().HasInterpolatedParameters())
		{
			GPUExecContext.CombinedParamStore.CopyCurrToPrev();
		}*/

		return;
	}

	int32 AllocationSize = OrigNumParticles + SpawnTotal + EventSpawnTotal;

	//Ensure we don't blow our current hard limits on cpu particle count.
	//TODO: These current limits can be improved relatively easily. Though perf in at these counts will obviously be an issue anyway.
	if (CachedEmitter->SimTarget == ENiagaraSimTarget::CPUSim && AllocationSize > GMaxNiagaraCPUParticlesPerEmitter)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Emitter %s has attemted to exceed the max CPU particle count! | Max: %d | Requested: %u"), *CachedEmitter->GetUniqueEmitterName(), GMaxNiagaraCPUParticlesPerEmitter, AllocationSize);
		//For now we completely bail out of spawning new particles. Possibly should improve this in future.
		AllocationSize = OrigNumParticles;
		SpawnTotal = 0;
		EventSpawnTotal = 0;
	}

	//Allocate space for prev frames particles and any new one's we're going to spawn.
	Data.Allocate(AllocationSize);
	for (FNiagaraDataSet* SpawnEventDataSet : SpawnScriptEventDataSets)
	{
		SpawnEventDataSet->Allocate(SpawnTotal + EventSpawnTotal);
	}
	for (FNiagaraDataSet* UpdateEventDataSet : UpdateScriptEventDataSets)
	{
		UpdateEventDataSet->Allocate(OrigNumParticles);
	}
	TArray<FNiagaraDataSetExecutionInfo, TInlineAllocator<8>> DataSetExecInfos;
	DataSetExecInfos.Emplace(&Data, 0, false, true);

	// Simulate existing particles forward by DeltaSeconds.
	if (OrigNumParticles > 0)
	{
		/*
		if (bDumpAfterEvent)
		{
			Data.Dump(false);
			bDumpAfterEvent = false;
		}
		*/

		Data.SetNumInstances(OrigNumParticles);
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimulate);

		UpdateExecCountBinding.SetValue(OrigNumParticles);
		DataSetExecInfos.SetNum(1, false);
		DataSetExecInfos[0].StartInstance = 0;
		for (FNiagaraDataSet* EventDataSet : UpdateScriptEventDataSets)
		{
			DataSetExecInfos.Emplace(EventDataSet, 0, false, true);
			EventDataSet->SetNumInstances(OrigNumParticles);
		}
		UpdateExecContext.Execute(OrigNumParticles, DataSetExecInfos);
		int32 DeltaParticles = Data.GetNumInstances() - OrigNumParticles;

		ensure(DeltaParticles <= 0); // We either lose particles or stay the same, we should never add particles in update!

		if (GbDumpParticleData || System->bDumpDebugEmitterInfo)
		{
			UE_LOG(LogNiagara, Log, TEXT("=== Update Parameters ===") );
			UpdateExecContext.Parameters.Dump();

			UE_LOG(LogNiagara, Log, TEXT("=== Updated %d Particles (%d Died) ==="), OrigNumParticles, -DeltaParticles);
			Data.Dump(true, 0, OrigNumParticles);
		}
	}
	
#if WITH_EDITORONLY_DATA
	if (ParentSystemInstance->ShouldCaptureThisFrame())
	{
		FNiagaraScriptDebuggerInfo* DebugInfo = ParentSystemInstance->GetActiveCaptureWrite(CachedIDName, ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
		if (DebugInfo)
		{
			Data.Dump(DebugInfo->Frame, true, 0, OrigNumParticles);
			//DebugInfo->Frame.Dump(true, 0, OrigNumParticles);
			DebugInfo->Parameters = UpdateExecContext.Parameters;
		}
	}
#endif

	uint32 EventSpawnStart = Data.GetNumInstances();
	int32 NumBeforeSpawn = Data.GetNumInstances();

	//Init new particles with the spawn script.
	if (SpawnTotal + EventSpawnTotal > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSpawn);

		//Handle main spawn rate spawning
		auto SpawnParticles = [&](int32 Num, FString DumpLabel)
		{
			if (Num > 0)
			{
				int32 OrigNum = Data.GetNumInstances();
				Data.SetNumInstances(OrigNum + Num);

				SpawnExecCountBinding.SetValue(Num);
				DataSetExecInfos.SetNum(1, false);
				DataSetExecInfos[0].StartInstance = OrigNum;

				//UE_LOG(LogNiagara, Log, TEXT("SpawnScriptEventDataSets: %d"), SpawnScriptEventDataSets.Num());
				for (FNiagaraDataSet* EventDataSet : SpawnScriptEventDataSets)
				{
					//UE_LOG(LogNiagara, Log, TEXT("SpawnScriptEventDataSets.. %d"), EventDataSet->GetNumVariables());
					int32 EventOrigNum = EventDataSet->GetNumInstances();
					EventDataSet->SetNumInstances(EventOrigNum + Num);
					DataSetExecInfos.Emplace(EventDataSet, EventOrigNum, false, true);
				}
				
				SpawnExecContext.Execute(Num, DataSetExecInfos);

				if (GbDumpParticleData || System->bDumpDebugEmitterInfo)
				{
					UE_LOG(LogNiagara, Log, TEXT("=== %s Spawn Parameters ==="), *DumpLabel);
					SpawnExecContext.Parameters.Dump();
					UE_LOG(LogNiagara, Log, TEXT("===  %s Spawned %d Particles==="), *DumpLabel, Num);
					Data.Dump(true, OrigNum, Num);
				}
			}
		};

		//Perform all our regular spawning that's driven by our emitter script.
		for (FNiagaraSpawnInfo& Info : SpawnInfos)
		{
			SpawnIntervalBinding.SetValue(Info.IntervalDt);
			InterpSpawnStartBinding.SetValue(Info.InterpStartDt);
			SpawnGroupBinding.SetValue(Info.SpawnGroup);

			SpawnParticles(Info.Count, TEXT("Regular Spawn"));
		};

		EventSpawnStart = Data.GetNumInstances();

		for (int32 EventScriptIdx = 0; EventScriptIdx < CachedEmitter->GetEventHandlers().Num(); EventScriptIdx++)
		{
			//Spawn particles coming from events.
			for (int32 i = 0; i < EventSpawnCounts[EventScriptIdx].Num(); i++)
			{
				int32 EventNumToSpawn = EventSpawnCounts[EventScriptIdx][i];

				//Event spawns are instantaneous at the middle of the frame?
				SpawnIntervalBinding.SetValue(0.0f);
				InterpSpawnStartBinding.SetValue(DeltaSeconds * 0.5f);
				SpawnGroupBinding.SetValue(0);

				SpawnParticles(EventNumToSpawn, TEXT("Event Spawn"));
			}
		}
	}


#if WITH_EDITORONLY_DATA
	int32 NumAfterSpawn = Data.GetNumInstances();
	int32 TotalNumSpawned = NumAfterSpawn - NumBeforeSpawn;
	if (ParentSystemInstance->ShouldCaptureThisFrame())
	{
		FNiagaraScriptDebuggerInfo* DebugInfo = ParentSystemInstance->GetActiveCaptureWrite(CachedIDName, ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
		if (DebugInfo)
		{
			Data.Dump(DebugInfo->Frame, true, NumBeforeSpawn, TotalNumSpawned);
			//DebugInfo->Frame.Dump(true, 0, Num);
			DebugInfo->Parameters = SpawnExecContext.Parameters;
		}
	}
#endif
	/*else if (SpawnTotal + EventSpawnTotal > 0)
	{
		UE_LOG(LogNiagara, Log, TEXT("Skipping spawning due to execution state! %d"), (uint32)ExecutionState)
	}*/

	// Events are all working from the same set of data generated during spawn they they only need 1 copy to have updated data.
	if (CachedEmitter->GetEventHandlers().Num())
	{
		Data.CopyCurToPrev();
	}
	int32 SpawnEventScriptStartIndex = EventSpawnStart;
	for (int32 EventScriptIdx = 0;  EventScriptIdx < CachedEmitter->GetEventHandlers().Num(); EventScriptIdx++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = CachedEmitter->GetEventHandlers()[EventScriptIdx];

		if (bPerformEventSpawning[EventScriptIdx] && EventSet[EventScriptIdx] && EventSpawnCounts[EventScriptIdx].Num())
		{
			uint32 NumParticles = Data.GetNumInstances();
			SCOPE_CYCLE_COUNTER(STAT_NiagaraEventHandle);

			for (int32 i = 0; i < EventSpawnCounts[EventScriptIdx].Num(); i++)
			{
				// When using interpolated spawn it's possible for the interpolated update script to kill a particle the same frame that it's spawned.
				// In this case we have to decrease the number of instances to run the event script on.
				int32 EventNumToSpawn = EventSpawnCounts[EventScriptIdx][i];
				int32 ActualEventNumToSpawn = FMath::Min(EventNumToSpawn, (int32)Data.GetNumInstances() - (int32)EventSpawnStart);
				
				if (ActualEventNumToSpawn > 0)
				{
					EventExecCountBindings[EventScriptIdx].SetValue(EventNumToSpawn);

					DataSetExecInfos.SetNum(1, false);
					DataSetExecInfos[0].StartInstance = EventSpawnStart;
					DataSetExecInfos[0].bUpdateInstanceCount = false;
					DataSetExecInfos.Emplace(EventSet[EventScriptIdx], i, false, false);
					EventExecContexts[EventScriptIdx].Execute(ActualEventNumToSpawn, DataSetExecInfos);

					if (GbDumpParticleData)
					{
						UE_LOG(LogNiagara, Log, TEXT("=== Event %d Parameters ==="), EventScriptIdx);
						EventExecContexts[EventScriptIdx].Parameters.Dump();
						UE_LOG(LogNiagara, Log, TEXT("=== Event %d %d Particles ==="), EventScriptIdx, ActualEventNumToSpawn);
						Data.Dump(true, EventSpawnStart, ActualEventNumToSpawn);
					}

#if WITH_EDITORONLY_DATA
					if (ParentSystemInstance->ShouldCaptureThisFrame())
					{
						FGuid EventGuid = EventExecContexts[EventScriptIdx].Script->GetUsageId();
						FNiagaraScriptDebuggerInfo* DebugInfo = ParentSystemInstance->GetActiveCaptureWrite(CachedIDName, ENiagaraScriptUsage::ParticleEventScript, EventGuid);
						if (DebugInfo)
						{
							Data.Dump(DebugInfo->Frame, true, EventSpawnStart, ActualEventNumToSpawn);		
							//DebugInfo->Frame.Dump(true, 0, ActualEventNumToSpawn);
							DebugInfo->Parameters = EventExecContexts[EventScriptIdx].Parameters;
						}
					}
#endif

					ensure(Data.GetNumInstances() == NumParticles);

					EventSpawnStart += ActualEventNumToSpawn;
				}
			}
		}
	}

	// Update events need a copy per event so that the previous event's data can be used.
	for (int32 EventScriptIdx = 0; EventScriptIdx < CachedEmitter->GetEventHandlers().Num(); EventScriptIdx++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = CachedEmitter->GetEventHandlers()[EventScriptIdx];

		// handle all-particle events
		if (EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::EveryParticle && EventSet[EventScriptIdx])
		{
			uint32 NumParticles = Data.GetNumInstances();
			if (EventSet[EventScriptIdx]->GetPrevNumInstances())
			{
				SCOPE_CYCLE_COUNTER(STAT_NiagaraEventHandle);
				
				for (uint32 i = 0; i < EventSet[EventScriptIdx]->GetPrevNumInstances(); i++)
				{
					// Copy the current to previous so that the event script has access to the new values from the update
					// script and any values updated in previous events.
					Data.CopyCurToPrev();

					uint32 NumInstancesPrev = Data.GetPrevNumInstances();
					EventExecCountBindings[EventScriptIdx].SetValue(NumInstancesPrev);
					DataSetExecInfos.SetNum(1, false);
					DataSetExecInfos[0].StartInstance = 0;
					DataSetExecInfos.Emplace(EventSet[EventScriptIdx], i, false, false);
					/*if (GbDumpParticleData || System->bDumpDebugEmitterInfo)
					{
						UE_LOG(LogNiagara, Log, TEXT("=== Event %d [%d] Payload ==="), EventScriptIdx, i);
						DataSetExecInfos[1].DataSet->Dump(true, 0, 1);
					}*/

					EventExecContexts[EventScriptIdx].Execute(NumInstancesPrev, DataSetExecInfos);

					if (GbDumpParticleData || System->bDumpDebugEmitterInfo)
					{
						UE_LOG(LogNiagara, Log, TEXT("=== Event %d [%d] Parameters ==="), EventScriptIdx, i);
						EventExecContexts[EventScriptIdx].Parameters.Dump();
						UE_LOG(LogNiagara, Log, TEXT("=== Event %d %d Particles ==="), EventScriptIdx, NumInstancesPrev);
						Data.Dump(true, 0, NumInstancesPrev);
					}


#if WITH_EDITORONLY_DATA
					if (ParentSystemInstance->ShouldCaptureThisFrame())
					{
						FGuid EventGuid = EventExecContexts[EventScriptIdx].Script->GetUsageId();
						FNiagaraScriptDebuggerInfo* DebugInfo = ParentSystemInstance->GetActiveCaptureWrite(CachedIDName, ENiagaraScriptUsage::ParticleEventScript, EventGuid);
						if (DebugInfo)
						{
							Data.Dump(DebugInfo->Frame, true, 0, NumInstancesPrev);
							//DebugInfo->Frame.Dump(true, 0, NumInstancesPrev);
							DebugInfo->Parameters = EventExecContexts[EventScriptIdx].Parameters;
						}
					}
#endif

					ensure(NumParticles == Data.GetNumInstances());
				}
			}
		}

		//TODO: Disabling this event mode for now until it can be reworked. Currently it uses index directly with can easily be invalid and cause undefined behavior.
		//
//		// handle single-particle events
//		// TODO: we'll need a way to either skip execution of the VM if an index comes back as invalid, or we'll have to pre-process
//		// event/particle arrays; this is currently a very naive (and comparatively slow) implementation, until full indexed reads work
// 		if (EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::SingleParticle && EventSet[EventScriptIdx])
// 		{
// 
// 			SCOPE_CYCLE_COUNTER(STAT_NiagaraEventHandle);
// 			FNiagaraVariable IndexVar(FNiagaraTypeDefinition::GetIntDef(), "ParticleIndex");
// 			FNiagaraDataSetIterator<int32> IndexItr(*EventSet[EventScriptIdx], IndexVar, 0, false);
// 			if (IndexItr.IsValid() && EventSet[EventScriptIdx]->GetPrevNumInstances() > 0)
// 			{
// 				EventExecCountBindings[EventScriptIdx].SetValue(1);
// 
// 				Data.CopyCurToPrev();
// 				uint32 NumParticles = Data.GetNumInstances();
// 
// 				for (uint32 i = 0; i < EventSet[EventScriptIdx]->GetPrevNumInstances(); i++)
// 				{
// 					int32 Index = *IndexItr;
// 					IndexItr.Advance();
// 					DataSetExecInfos.SetNum(1, false);
// 					DataSetExecInfos[0].StartInstance = Index;
// 					DataSetExecInfos[0].bUpdateInstanceCount = false;
// 					DataSetExecInfos.Emplace(EventSet[EventScriptIdx], i, false, false);
// 					EventExecContexts[EventScriptIdx].Execute(1, DataSetExecInfos);
// 
// 					if (GbDumpParticleData || System->bDumpDebugEmitterInfo)
// 					{
// 						ensure(EventHandlerProps.Script->RapidIterationParameters.VerifyBinding(&EventExecContexts[EventScriptIdx].Parameters));
// 						UE_LOG(LogNiagara, Log, TEXT("=== Event %d Src Parameters ==="), EventScriptIdx);
// 						EventHandlerProps.Script->RapidIterationParameters.Dump();
// 						UE_LOG(LogNiagara, Log, TEXT("=== Event %d Context Parameters ==="), EventScriptIdx);
// 						EventExecContexts[EventScriptIdx].Parameters.Dump();
// 						UE_LOG(LogNiagara, Log, TEXT("=== Event %d Particles (%d index written, %d total) ==="), EventScriptIdx, Index, Data.GetNumInstances());
// 						Data.Dump(true, Index, 1);
// 					}
// 
// 
// #if WITH_EDITORONLY_DATA
// 					if (ParentSystemInstance->ShouldCaptureThisFrame())
// 					{
// 						FGuid EventGuid = EventExecContexts[EventScriptIdx].Script->GetUsageId();
// 						FNiagaraScriptDebuggerInfo* DebugInfo = ParentSystemInstance->GetActiveCaptureWrite(CachedIDName, ENiagaraScriptUsage::ParticleEventScript, EventGuid);
// 						if (DebugInfo)
// 						{
// 							Data.Dump(DebugInfo->Frame, true, Index, 1);
// 							//DebugInfo->Frame.Dump(true, 0, 1);
// 							DebugInfo->Parameters = EventExecContexts[EventScriptIdx].Parameters;
// 						}
// 					}
// #endif
// 					ensure(NumParticles == Data.GetNumInstances());
// 				}
// 			}
// 		}
	}

	PostProcessParticles();

	SpawnExecContext.PostTick();
	UpdateExecContext.PostTick();
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		EventContext.PostTick();
	}

	CPUTimeMS = TickTime.GetElapsedMilliseconds();

	INC_DWORD_STAT_BY(STAT_NiagaraNumParticles, Data.GetNumInstances());
}


/** Calculate total number of spawned particles from events; these all come from event handler script with the SpawnedParticles execution mode
 *  We get the counts ahead of event processing time so we only have to allocate new particles once
 *  TODO: augment for multiple spawning event scripts
 */
uint32 FNiagaraEmitterInstance::CalculateEventSpawnCount(const FNiagaraEventScriptProperties &EventHandlerProps, TArray<int32, TInlineAllocator<16>>& EventSpawnCounts, FNiagaraDataSet *EventSet)
{
	uint32 EventSpawnTotal = 0;
	int32 NumEventsToProcess = 0;

	if (EventSet)
	{
		NumEventsToProcess = EventSet->GetPrevNumInstances();
		if(EventHandlerProps.MaxEventsPerFrame > 0)
		{
			NumEventsToProcess = FMath::Min<int32>(EventSet->GetPrevNumInstances(), EventHandlerProps.MaxEventsPerFrame);
		}

		const bool bUseRandom = EventHandlerProps.bRandomSpawnNumber && EventHandlerProps.MinSpawnNumber < EventHandlerProps.SpawnNumber;
		for (int32 i = 0; i < NumEventsToProcess; i++)
		{
			const uint32 SpawnNumber = bUseRandom ? FMath::RandRange((int32)EventHandlerProps.MinSpawnNumber, (int32)EventHandlerProps.SpawnNumber) : EventHandlerProps.SpawnNumber;
			if (ExecutionState == ENiagaraExecutionState::Active && SpawnNumber > 0)
			{
				EventSpawnCounts.Add(SpawnNumber);
				EventSpawnTotal += SpawnNumber;
			}
		}
	}

	return EventSpawnTotal;
}

void FNiagaraEmitterInstance::SetExecutionState(ENiagaraExecutionState InState)
{
	/*if (InState != ExecutionState)
	{
		const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();
		UE_LOG(LogNiagara, Log, TEXT("Emitter \"%s\" change state: %s to %s"), *GetEmitterHandle().GetName().ToString(), *EnumPtr->GetNameStringByValue((int64)ExecutionState),
			*EnumPtr->GetNameStringByValue((int64)InState));
	}*/

	/*if (InState == ENiagaraExecutionState::Active && ExecutionState == ENiagaraExecutionState::Inactive)
	{
		UE_LOG(LogNiagara, Log, TEXT("Emitter \"%s\" change state N O O O O O "), *GetEmitterHandle().GetName().ToString());
	}*/
	check(InState >= ENiagaraExecutionState::Active && InState < ENiagaraExecutionState::Num);
	//We can't move out of disabled without a proper reinit.
	if (ExecutionState != ENiagaraExecutionState::Disabled)
	{
		ExecutionState = InState;
	}
}


#if WITH_EDITORONLY_DATA

bool FNiagaraEmitterInstance::CheckAttributesForRenderer(int32 Index)
{
	if (Index > EmitterRenderer.Num())
	{
		return false;
	}

	bool bOk = true;
	checkSlow(ParticleDataSet);
	FNiagaraDataSet& Data = *ParticleDataSet;
	if (EmitterRenderer[Index])
	{
		
		const TArray<FNiagaraVariable>& RequiredAttrs = EmitterRenderer[Index]->GetRequiredAttributes();

		for (FNiagaraVariable Attr : RequiredAttrs)
		{
			// TODO .. should we always be namespaced?
			FString AttrName = Attr.GetName().ToString();
			if (AttrName.RemoveFromStart(TEXT("Particles.")))
			{
				Attr.SetName(*AttrName);
			}

			if (!Data.HasVariable(Attr))
			{
				bOk = false;
				UE_LOG(LogNiagara, Error, TEXT("Cannot render %s because it does not define attribute %s %s."), *GetEmitterHandle().GetName().ToString(), *Attr.GetType().GetNameText().ToString() , *Attr.GetName().ToString());
			}
		}

		if (bOk && !EmitterRenderer[Index]->GetRendererProperties()->IsSimTargetSupported(CachedEmitter->SimTarget))
		{
			UE_LOG(LogNiagara, Error, TEXT("Cannot render %s because it is not compatible with this SimTarget mode."), *GetEmitterHandle().GetName().ToString());
			bOk = false;
		}

		EmitterRenderer[Index]->SetEnabled(bOk);
	}
	return bOk;
}

#endif

/** Replace the current System renderer with a new one of Type.
Don't forget to call RenderModuleUpdate on the SceneProxy after calling this! 
 */
void FNiagaraEmitterInstance::UpdateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, TArray<NiagaraRenderer*>& ToBeAddedList, TArray<NiagaraRenderer*>& ToBeRemovedList)
{
	checkSlow(CachedEmitter);

	// Add all the old to be purged..
	for (int32 SubIdx = 0; SubIdx < EmitterRenderer.Num(); SubIdx++)
	{
		if (EmitterRenderer[SubIdx] != nullptr)
		{
			ToBeRemovedList.Add(EmitterRenderer[SubIdx]);
			EmitterRenderer[SubIdx] = nullptr;
		}
	}

	if (!IsComplete())
	{
		EmitterRenderer.Empty();
		EmitterRenderer.AddZeroed(CachedEmitter->GetRenderers().Num());
		for (int32 SubIdx = 0; SubIdx < CachedEmitter->GetRenderers().Num(); SubIdx++)
		{
			UMaterialInterface *Material = nullptr;

			TArray<UMaterialInterface*> UsedMats;
			if (CachedEmitter->GetRenderers()[SubIdx] != nullptr)
			{
				CachedEmitter->GetRenderers()[SubIdx]->GetUsedMaterials(UsedMats);
				if (UsedMats.Num() != 0)
				{
					Material = UsedMats[0];
				}
			}

			if (Material == nullptr)
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			if (CachedEmitter->GetRenderers()[SubIdx] != nullptr)
			{
				EmitterRenderer[SubIdx] = CachedEmitter->GetRenderers()[SubIdx]->CreateEmitterRenderer(FeatureLevel);
				EmitterRenderer[SubIdx]->SetMaterial(Material, FeatureLevel);
				EmitterRenderer[SubIdx]->SetLocalSpace(CachedEmitter->bLocalSpace);
				ToBeAddedList.Add(EmitterRenderer[SubIdx]);

				//UE_LOG(LogNiagara, Warning, TEXT("CreateRenderer %p"), EmitterRenderer);
#if WITH_EDITORONLY_DATA
				CheckAttributesForRenderer(SubIdx);
#endif
			}
			else
			{
				EmitterRenderer[SubIdx] = nullptr;
			}
		}
	}
}

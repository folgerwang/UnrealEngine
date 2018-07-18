// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemInstance.h"
#include "NiagaraConstants.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraStats.h"
#include "Async/ParallelFor.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraWorldManager.h"
#include "NiagaraComponent.h"
#include "NiagaraRenderer.h"
#include "GameFramework/PlayerController.h"
#include "Templates/AlignmentTemplates.h"


DECLARE_CYCLE_STAT(TEXT("System Activate (GT)"), STAT_NiagaraSystemActivate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Deactivate (GT)"), STAT_NiagaraSystemDeactivate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Complete (GT)"), STAT_NiagaraSystemComplete, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Parallel Tick"), STAT_NiagaraParallelTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Reset (GT)"), STAT_NiagaraSystemReset, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Reinit (GT)"), STAT_NiagaraSystemReinit, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Init Emitters (GT)"), STAT_NiagaraSystemInitEmitters, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Advance Simulation "), STAT_NiagaraSystemAdvanceSim, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System SetSolo "), STAT_NiagaraSystemSetSolo, STATGROUP_Niagara); 
DECLARE_CYCLE_STAT(TEXT("System PreSimulateTick "), STAT_NiagaraSystemPreSimulateTick, STATGROUP_Niagara); 

 
/** Safety time to allow for the LastRenderTime coming back from the RT. */
static float GLastRenderTimeSafetyBias = 0.1f;
static FAutoConsoleVariableRef CVarLastRenderTimeSafetyBias(
	TEXT("fx.LastRenderTimeSafetyBias"),
	GLastRenderTimeSafetyBias,
	TEXT("The time to bias the LastRenderTime value to allow for the delay from it being written by the RT."),
	ECVF_Default
);

FNiagaraSystemInstance::FNiagaraSystemInstance(UNiagaraComponent* InComponent)
	: SystemInstanceIndex(INDEX_NONE)
	, Component(InComponent)
	, Age(0.0f)
	, ID(FGuid::NewGuid())
	, IDName(*ID.ToString())
	, InstanceParameters(Component)
	, bSolo(false)
	, bForceSolo(false)
	, bPendingSpawn(false)
	, bHasTickingEmitters(true)
	, RequestedExecutionState(ENiagaraExecutionState::Complete)
	, ActualExecutionState(ENiagaraExecutionState::Complete)
{
	SystemBounds.Init();
}

void FNiagaraSystemInstance::Init(UNiagaraSystem* InSystem, bool bInForceSolo)
{
	bForceSolo = bInForceSolo;
	ActualExecutionState = ENiagaraExecutionState::Inactive;
	RequestedExecutionState = ENiagaraExecutionState::Inactive;

	//InstanceParameters = GetSystem()->GetInstanceParameters();
	// In order to get user data interface parameters in the component to work properly,
	// we need to bind here, otherwise the instances when we init data interfaces during reset will potentially
	// be the defaults (i.e. null) for things like static mesh data interfaces.
	Reset(EResetMode::ReInit, true);

#if WITH_EDITORONLY_DATA
	InstanceParameters.DebugName = *FString::Printf(TEXT("SystemInstance %p"), this);
#endif
	OnInitializedDelegate.Broadcast();
}

void FNiagaraSystemInstance::SetRequestedExecutionState(ENiagaraExecutionState InState)
{
	//Once in disabled state we can never get out except on Reinit.
	if (RequestedExecutionState != InState && RequestedExecutionState != ENiagaraExecutionState::Disabled)
	{
		/*const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();
		UE_LOG(LogNiagara, Log, TEXT("Component \"%s\" System \"%s\" requested change state: %s to %s, actual %s"), *GetComponent()->GetName(), *GetSystem()->GetName(), *EnumPtr->GetNameStringByValue((int64)RequestedExecutionState),
			*EnumPtr->GetNameStringByValue((int64)InState), *EnumPtr->GetNameStringByValue((int64)ActualExecutionState));
		*/
		if (InState == ENiagaraExecutionState::Disabled)
		{
			//Really move to disabled straight away.
			ActualExecutionState = ENiagaraExecutionState::Disabled;
			Cleanup();
		}
		RequestedExecutionState = InState;
	}
}

void FNiagaraSystemInstance::SetActualExecutionState(ENiagaraExecutionState InState)
{

	//Once in disabled state we can never get out except on Reinit.
	if (ActualExecutionState != InState && ActualExecutionState != ENiagaraExecutionState::Disabled)
	{
		/*const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();
		UE_LOG(LogNiagara, Log, TEXT("Component \"%s\" System \"%s\" actual change state: %s to %s"), *GetComponent()->GetName(), *GetSystem()->GetName(), *EnumPtr->GetNameStringByValue((int64)ActualExecutionState),
			*EnumPtr->GetNameStringByValue((int64)InState));
		*/
		ActualExecutionState = InState;

		if (ActualExecutionState == ENiagaraExecutionState::Active)
		{
			// We only need to notify completion once after each successful active.
			// Here's when we know that we just became active.
			bNotifyOnCompletion = true;

			// We may also end up calling HandleCompletion on each emitter.
			// This may happen *before* we've successfully pulled data off of a 
			// simulation run. This means that we need to synchronize the execution
			// states upon activation.
			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
			{
				FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();
				EmitterInst.SetExecutionState(ENiagaraExecutionState::Active);
			}
		}
	}
}

void FNiagaraSystemInstance::Dump()const
{
	GetSystemSimulation()->DumpInstance(this);
	for (auto& Emitter : Emitters)
	{
		Emitter->Dump();
	}
}

#if WITH_EDITORONLY_DATA
bool FNiagaraSystemInstance::RequestCapture(const FGuid& RequestId)
{
	if (IsComplete() || CurrentCapture.IsValid())
	{
		return false;
	}

	bWasSoloPriorToCaptureRequest = bSolo;
	SetSolo(true);

	// Go ahead and populate the shared array so that we don't have to do this on the game thread and potentially race.
	TSharedRef<TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>, ESPMode::ThreadSafe> TempCaptureHolder = 
		MakeShared<TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>, ESPMode::ThreadSafe>();
	
	TempCaptureHolder->Add(MakeShared<FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>(NAME_None, ENiagaraScriptUsage::SystemSpawnScript, FGuid()));
	TempCaptureHolder->Add(MakeShared<FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>(NAME_None, ENiagaraScriptUsage::SystemUpdateScript, FGuid()));

	for (const FNiagaraEmitterHandle& Handle : GetSystem()->GetEmitterHandles())
	{
		TArray<UNiagaraScript*> Scripts;
		Handle.GetInstance()->GetScripts(Scripts, false);

		for (UNiagaraScript* Script : Scripts)
		{
			TempCaptureHolder->Add(MakeShared<FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>(Handle.GetIdName(), Script->GetUsage(), Script->GetUsageId()));
		}
	}
	CapturedFrames.Add(RequestId, TempCaptureHolder);
	CurrentCapture = TempCaptureHolder;
	CurrentCaptureGuid = MakeShared<FGuid, ESPMode::ThreadSafe>(RequestId);
	return true;
}

void FNiagaraSystemInstance::FinishCapture()
{
	if (!CurrentCapture.IsValid())
	{
		return;
	}

	SetSolo(bWasSoloPriorToCaptureRequest);
	CurrentCapture.Reset();
	CurrentCaptureGuid.Reset();
}

bool FNiagaraSystemInstance::QueryCaptureResults(const FGuid& RequestId, TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>& OutCaptureResults)
{
	if (CurrentCaptureGuid.IsValid() && RequestId == *CurrentCaptureGuid.Get())
	{
		return false;
	}

	const TSharedPtr<TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>, ESPMode::ThreadSafe>* FoundEntry = CapturedFrames.Find(RequestId);
	if (FoundEntry != nullptr)
	{
		TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>* Array = FoundEntry->Get();
		OutCaptureResults.SetNum(Array->Num());

		for (int32 i = 0; i < FoundEntry->Get()->Num(); i++)
		{
			OutCaptureResults[i] = (*Array)[i];
		}
		CapturedFrames.Remove(RequestId);
		return true;
	}

	return false;
}

TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>* FNiagaraSystemInstance::GetActiveCaptureResults()
{
	return CurrentCapture.Get();
}

FNiagaraScriptDebuggerInfo* FNiagaraSystemInstance::GetActiveCaptureWrite(const FName& InHandleName, ENiagaraScriptUsage InUsage, const FGuid& InUsageId)
{
	if (CurrentCapture.IsValid())
	{
		TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>* FoundEntry = CurrentCapture->FindByPredicate([&](const TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>& Entry)
		{
			return Entry->HandleName == InHandleName && UNiagaraScript::IsEquivalentUsage(Entry->Usage, InUsage) && Entry->UsageId == InUsageId;
		});

		if (FoundEntry != nullptr)
		{
			return FoundEntry->Get();
		}
	}
	return nullptr;
}

bool FNiagaraSystemInstance::ShouldCaptureThisFrame() const
{
	return CurrentCapture.IsValid();
}
#endif

void FNiagaraSystemInstance::SetSolo(bool bInSolo)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSetSolo);
	if (bSolo == bInSolo)
	{
		return;
	}

	UNiagaraSystem* System = GetSystem();
	if (bInSolo)
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> NewSoloSim = MakeShared<FNiagaraSystemSimulation, ESPMode::ThreadSafe>();
		NewSoloSim->Init(System, Component->GetWorld(), this);

		NewSoloSim->TransferInstance(SystemSimulation.Get(), this);	

		SystemSimulation = NewSoloSim;
		bSolo = true;
	}
	else
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> NewSim = GetWorldManager()->GetSystemSimulation(System);

		NewSim->TransferInstance(SystemSimulation.Get(), this);
		
		SystemSimulation = NewSim;
		bSolo = false;
	}
}

void FNiagaraSystemInstance::Activate(EResetMode InResetMode)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemActivate);
	
	UNiagaraSystem* System = GetSystem();
	if (System && System->IsValid() && IsReadyToRun())
	{
		Reset(InResetMode, true);
	}
	else
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Disabled);
	}
}

void FNiagaraSystemInstance::Deactivate(bool bImmediate)
{

	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemDeactivate);
	if (IsComplete())
	{
		return;
	}

	if (bImmediate)
	{
		Complete();
	}
	else
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Inactive);
	}
}

void FNiagaraSystemInstance::Complete()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemComplete);
	
	// Only notify others if have yet to complete
	bool bNeedToNotifyOthers = bNotifyOnCompletion;

	if (SystemInstanceIndex != INDEX_NONE)
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = GetSystemSimulation();
		SystemSim->RemoveInstance(this);

		SetActualExecutionState(ENiagaraExecutionState::Complete);
		SetRequestedExecutionState(ENiagaraExecutionState::Complete);

		for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
		{
			Simulation->HandleCompletion(true);
		}
	}
	else
	{
		SetActualExecutionState(ENiagaraExecutionState::Complete);
		SetRequestedExecutionState(ENiagaraExecutionState::Complete);
	}

	DestroyDataInterfaceInstanceData();

	UnbindParameters();

	if (bNeedToNotifyOthers)
	{
		OnCompleteDelegate.Broadcast(this);

		if (Component)
		{
			Component->OnSystemComplete();
		}
		
		// We've already notified once, no need to do so again.
		bNotifyOnCompletion = false;
	}
}

void FNiagaraSystemInstance::Reset(FNiagaraSystemInstance::EResetMode Mode, bool bBindParams)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemReset);

	FNiagaraSystemSimulation* SystemSim = GetSystemSimulation().Get();
	if (Mode == EResetMode::None)
	{
		// Right now we don't support binding with reset mode none.
		/*if (Mode == EResetMode::None && bBindParams)
		{
			BindParameters();
		}*/
		return;
	}

	Component->LastRenderTime = Component->GetWorld()->GetTimeSeconds();

	if (SystemSim)
	{
		SystemSim->RemoveInstance(this);
	}
	else
	{
		Mode = EResetMode::ReInit;
	}

	//If we were disabled, try to reinit on reset.
	if (IsDisabled())
	{
		Mode = EResetMode::ReInit;
	}
		
	if (Mode == EResetMode::ResetSystem)
	{
		//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::Reset false"));
		ResetInternal(false);
	}
	else if (Mode == EResetMode::ResetAll)
	{
		//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::Reset true"));
		ResetInternal(true);
	}
	else if (Mode == EResetMode::ReInit)
	{
		//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::ReInit"));
		ReInitInternal();
	}
	
	if (bBindParams)
	{
		BindParameters();
	}

	SystemSim = GetSystemSimulation().Get();
	SetRequestedExecutionState(ENiagaraExecutionState::Active);
	SetActualExecutionState(ENiagaraExecutionState::Active);

	InitDataInterfaces();

	//Interface init can disable the system.
	if (!IsComplete())
	{
		bPendingSpawn = true;
		SystemSim->AddInstance(this);

		UNiagaraSystem* System = GetSystem();
		if (System->NeedsWarmup())
		{
			int32 WarmupTicks = System->GetWarmupTickCount();
			float WarmupDt = System->GetWarmupTickDelta();
			
			AdvanceSimulation(WarmupTicks, WarmupDt);

			//Reset age to zero.
			Age = 0.0f;
		}
	}

	// This system may not tick again immediately so we mark the render state dirty here so that
	// the renderers will be reset this frame.
	Component->MarkRenderDynamicDataDirty();
}

void FNiagaraSystemInstance::ResetInternal(bool bResetSimulations)
{
	Age = 0;
	UNiagaraSystem* System = GetSystem();
	if (System == nullptr || Component == nullptr || IsDisabled())
	{
		return;
	}

#if WITH_EDITOR
	if (Component->GetWorld() != nullptr && Component->GetWorld()->WorldType == EWorldType::Editor)
	{
		Component->GetOverrideParameters().Tick();
	}
#endif

	bool bAllReadyToRun = IsReadyToRun();

	if (!bAllReadyToRun)
	{
		return;
	}

	if (!System->IsValid())
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Disabled);
		UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara System due to invalid asset!"));
		return;
	}

	if (bResetSimulations)
	{
		for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
		{
			Simulation->ResetSimulation();
		}
	}

#if WITH_EDITOR
	//UE_LOG(LogNiagara, Log, TEXT("OnResetInternal %p"), this);
	OnResetDelegate.Broadcast();
#endif
}

UNiagaraParameterCollectionInstance* FNiagaraSystemInstance::GetParameterCollectionInstance(UNiagaraParameterCollection* Collection)
{
	return SystemSimulation->GetParameterCollectionInstance(Collection);
}

void FNiagaraSystemInstance::AdvanceSimulation(int32 TickCount, float TickDeltaSeconds)
{
	if (TickCount > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemAdvanceSim);
		bool bWasSolo = bSolo;
		SetSolo(true);

		for (int32 TickIdx = 0; TickIdx < TickCount; ++TickIdx)
		{
			ComponentTick(TickDeltaSeconds);
		}
		SetSolo(bWasSolo);
	}
}

bool FNiagaraSystemInstance::IsReadyToRun() const
{
	bool bAllReadyToRun = true;

	UNiagaraSystem* System = GetSystem();

	if (!System || !System->IsReadyToRun())
	{
		return false;
	}

	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
	{
		if (!Simulation->IsReadyToRun())
		{
			bAllReadyToRun = false;
		}
	}
	return bAllReadyToRun;
}

void FNiagaraSystemInstance::ReInitInternal()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemReinit);
	Age = 0;
	UNiagaraSystem* System = GetSystem();
	if (System == nullptr || Component == nullptr)
	{
		return;
	}

	//Bypass the SetExecutionState() and it's check for disabled.
	RequestedExecutionState = ENiagaraExecutionState::Inactive;
	ActualExecutionState = ENiagaraExecutionState::Inactive;

	bool bAllReadyToRun = IsReadyToRun();

	if (!bAllReadyToRun)
	{
		return;
	}
	
	if (!System->IsValid())
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Disabled);
		UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara System due to invalid asset!"));
		return;
	}

	/** Do we need to run in solo mode? */
	bSolo = bForceSolo || System->IsSolo();
	if (bSolo)
	{
		if (!SystemSimulation.IsValid())
		{
			SystemSimulation = MakeShared<FNiagaraSystemSimulation, ESPMode::ThreadSafe>();
			SystemSimulation->Init(System, Component->GetWorld(), this);
		}
	}
	else
	{
		SystemSimulation = GetWorldManager()->GetSystemSimulation(System);
	}

	//When re initializing, throw away old emitters and init new ones.
	Emitters.Reset();
	InitEmitters();
	
	InstanceParameters.Reset();
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_POSITION, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_SCALE, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_VELOCITY, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_X_AXIS, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_Y_AXIS, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_Z_AXIS, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_LOCAL_TO_WORLD, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_WORLD_TO_LOCAL, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_NO_SCALE, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_NO_SCALE, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_DELTA_TIME, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_TIME, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_REAL_TIME, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_INV_DELTA_TIME, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_TIME_SINCE_RENDERED, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_EXECUTION_STATE, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE, true, false);
	InstanceParameters.AddParameter(SYS_PARAM_ENGINE_SYSTEM_AGE);

	// This is required for user default data interface's (like say static meshes) to be set up properly.
	// Additionally, it must happen here for data to be properly found below.
	const bool bOnlyAdd = false;
	System->GetExposedParameters().CopyParametersTo(InstanceParameters, bOnlyAdd, FNiagaraParameterStore::EDataInterfaceCopyMethod::Reference);

	TArray<FNiagaraVariable> NumParticleVars;
	for (int32 i = 0; i < Emitters.Num(); i++)
	{
		TSharedRef<FNiagaraEmitterInstance> Simulation = Emitters[i];
		FString EmitterName = Simulation->GetEmitterHandle().GetInstance()->GetUniqueEmitterName();
		FNiagaraVariable Var = SYS_PARAM_ENGINE_EMITTER_NUM_PARTICLES;
		FString ParamName = Var.GetName().ToString().Replace(TEXT("Emitter"), *EmitterName);
		Var.SetName(*ParamName);
		InstanceParameters.AddParameter(Var, true, false);
		NumParticleVars.Add(Var);
	}

	// Make sure all parameters are added before initializing the bindings, otherwise parameter store layout changes might invalidate the bindings.
	OwnerPositionParam.Init(InstanceParameters, SYS_PARAM_ENGINE_POSITION);
	OwnerScaleParam.Init(InstanceParameters, SYS_PARAM_ENGINE_SCALE);
	OwnerVelocityParam.Init(InstanceParameters, SYS_PARAM_ENGINE_VELOCITY);
	OwnerXAxisParam.Init(InstanceParameters, SYS_PARAM_ENGINE_X_AXIS);
	OwnerYAxisParam.Init(InstanceParameters, SYS_PARAM_ENGINE_Y_AXIS);
	OwnerZAxisParam.Init(InstanceParameters, SYS_PARAM_ENGINE_Z_AXIS);

	OwnerTransformParam.Init(InstanceParameters, SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
	OwnerInverseParam.Init(InstanceParameters, SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
	OwnerTransposeParam.Init(InstanceParameters, SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
	OwnerInverseTransposeParam.Init(InstanceParameters, SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);
	OwnerTransformNoScaleParam.Init(InstanceParameters, SYS_PARAM_ENGINE_LOCAL_TO_WORLD_NO_SCALE);
	OwnerInverseNoScaleParam.Init(InstanceParameters, SYS_PARAM_ENGINE_WORLD_TO_LOCAL_NO_SCALE);

	OwnerDeltaSecondsParam.Init(InstanceParameters, SYS_PARAM_ENGINE_DELTA_TIME);
	OwnerInverseDeltaSecondsParam.Init(InstanceParameters, SYS_PARAM_ENGINE_INV_DELTA_TIME);

	SystemAgeParam.Init(InstanceParameters, SYS_PARAM_ENGINE_SYSTEM_AGE);
	OwnerEngineTimeParam.Init(InstanceParameters, SYS_PARAM_ENGINE_TIME);
	OwnerEngineRealtimeParam.Init(InstanceParameters, SYS_PARAM_ENGINE_REAL_TIME);

	OwnerMinDistanceToCameraParam.Init(InstanceParameters, SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA);
	SystemNumEmittersParam.Init(InstanceParameters, SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS);
	SystemNumEmittersAliveParam.Init(InstanceParameters, SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE);

	SystemTimeSinceRenderedParam.Init(InstanceParameters, SYS_PARAM_ENGINE_TIME_SINCE_RENDERED);

	OwnerExecutionStateParam.Init(InstanceParameters, SYS_PARAM_ENGINE_EXECUTION_STATE);

	ParameterNumParticleBindings.SetNum(NumParticleVars.Num());
	for (int32 i = 0; i < NumParticleVars.Num(); i++)
	{
		ParameterNumParticleBindings[i].Init(InstanceParameters, NumParticleVars[i]);
	}

	// rebind now after all parameters have been added
	InstanceParameters.Rebind();

	TickInstanceParameters(0.01f);

	// This gets a little tricky, but we want to delete any renderers that are no longer in use on the rendering thread, but
	// first (to be safe), we want to update the proxy to point to the new renderer objects.

	// Step 1: Recreate the renderers on the simulations, we keep the old and new renderers.
	TArray<NiagaraRenderer*> NewRenderers;
	TArray<NiagaraRenderer*> OldRenderers;

	UpdateRenderModules(Component->GetWorld()->FeatureLevel, NewRenderers, OldRenderers);

	// Step 2: Update the proxy with the new renderers that were created.
	UpdateProxy(NewRenderers);
	Component->MarkRenderStateDirty();

	// Step 3: Queue up the old renderers for deletion on the render thread.
	for (NiagaraRenderer* Renderer : OldRenderers)
	{
		if (Renderer != nullptr)
		{
			Renderer->Release();
		}
	}

#if WITH_EDITOR
	//UE_LOG(LogNiagara, Log, TEXT("OnResetInternal %p"), this);
	OnResetDelegate.Broadcast();
#endif

}

FNiagaraSystemInstance::~FNiagaraSystemInstance()
{
	//UE_LOG(LogNiagara, Warning, TEXT("~FNiagaraSystemInstance %p"), this);

	//FlushRenderingCommands();

	Cleanup();

// #if WITH_EDITOR
// 	OnDestroyedDelegate.Broadcast();
// #endif
}

void FNiagaraSystemInstance::Cleanup()
{
	if (SystemInstanceIndex != INDEX_NONE)
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = GetSystemSimulation();
		SystemSim->RemoveInstance(this);
	}

	DestroyDataInterfaceInstanceData();

	// Clear out the System renderer from the proxy.
	TArray<NiagaraRenderer*> NewRenderers;
	UpdateProxy(NewRenderers);

	// Clear out the System renderer from the simulation.
	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
	{
		Simulation->ClearRenderer();
	}

	UnbindParameters();

	// Clear out the emitters.
	Emitters.Empty(0);
}

//Unsure on usage of this atm. Possibly useful in future.
// void FNiagaraSystemInstance::RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance)
// {
// 	OldInstance->GetParameterStore().Unbind(&InstanceParameters);
// 	NewInstance->GetParameterStore().Bind(&InstanceParameters);
// 
// 	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
// 	{
// 		Simulation->RebindParameterCollection(OldInstance, NewInstance);
// 	}
// 
// 	//Have to re init the instance data for data interfaces.
// 	//This is actually lots more work than absolutely needed in some cases so we can improve it a fair bit.
// 	InitDataInterfaces();
// }

void FNiagaraSystemInstance::BindParameters()
{
	Component->GetOverrideParameters().Bind(&InstanceParameters);

	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
	{
		Simulation->BindParameters();
	}
}

void FNiagaraSystemInstance::UnbindParameters()
{
	Component->GetOverrideParameters().Unbind(&InstanceParameters);

	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
	{
		Simulation->UnbindParameters();
	}
}

FNiagaraWorldManager* FNiagaraSystemInstance::GetWorldManager()const
{
	return Component ? FNiagaraWorldManager::Get(Component->GetWorld()) : nullptr; 
}

void FNiagaraSystemInstance::InitDataInterfaces()
{
	// If either the System or the component is invalid, it is possible that our cached data interfaces
	// are now bogus and could point to invalid memory. Only the UNiagaraComponent or UNiagaraSystem
	// can hold onto GC references to the DataInterfaces.
	if (GetSystem() == nullptr || IsDisabled())
	{
		return;
	}

	if (Component == nullptr)
	{
		return;
	}

	Component->GetOverrideParameters().Tick();
	
	DestroyDataInterfaceInstanceData();

	//Now the interfaces in the simulations are all correct, we can build the per instance data table.
	int32 InstanceDataSize = 0;
	DataInterfaceInstanceDataOffsets.Empty();
	auto CalcInstDataSize = [&](const TArray<UNiagaraDataInterface*>& Interfaces)
	{
		for (UNiagaraDataInterface* Interface : Interfaces)
		{
			if (!Interface)
			{
				continue;
			}

			if (int32 Size = Interface->PerInstanceDataSize())
			{
				int32* ExistingInstanceDataOffset = DataInterfaceInstanceDataOffsets.Find(Interface);
				if (!ExistingInstanceDataOffset)//Don't add instance data for interfaces we've seen before.
				{
					DataInterfaceInstanceDataOffsets.Add(Interface) = InstanceDataSize;
					// Assume that some of our data is going to be 16 byte aligned, so enforce that 
					// all per-instance data is aligned that way.
					InstanceDataSize += Align(Size, 16);
				}
			}
		}
	};

	CalcInstDataSize(InstanceParameters.GetDataInterfaces());//This probably should be a proper exec context. 

	//Iterate over interfaces to get size for table and clear their interface bindings.
	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
	{
		FNiagaraEmitterInstance& Sim = Simulation.Get();
		CalcInstDataSize(Sim.GetSpawnExecutionContext().GetDataInterfaces());
		CalcInstDataSize(Sim.GetUpdateExecutionContext().GetDataInterfaces());
		for (int32 i = 0; i < Sim.GetEventExecutionContexts().Num(); i++)
		{
			CalcInstDataSize(Sim.GetEventExecutionContexts()[i].GetDataInterfaces());
		}

		//Also force a rebind while we're here.
		Sim.DirtyDataInterfaces();
	}

	DataInterfaceInstanceData.SetNumUninitialized(InstanceDataSize);

	bool bOk = true;
	for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
	{
		if (UNiagaraDataInterface* Interface = Pair.Key.Get())
		{
			check(IsAligned(&DataInterfaceInstanceData[Pair.Value], 16));

			//Ideally when we make the batching changes, we can keep the instance data in big single type blocks that can all be updated together with a single virtual call.
			bool bResult = Pair.Key->InitPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
			bOk &= bResult;
			if (!bResult)
			{
				UE_LOG(LogNiagara, Error, TEXT("Error initializing data interface \"%s\" for system. %u | %s"), *Interface->GetPathName(), Component, *Component->GetAsset()->GetName());
			}
		}
		else
		{
			UE_LOG(LogNiagara, Error, TEXT("A data interface currently in use by an System has been destroyed."));
			bOk = false;
		}
	}

	if (!bOk && (!IsComplete() && !IsPendingSpawn()))
	{
		//Some error initializing the data interfaces so disable until we're explicitly reinitialized.
		UE_LOG(LogNiagara, Error, TEXT("Error initializing data interfaces. Completing system. %u | %s"), Component, *Component->GetAsset()->GetName());
		Complete();
	}
}

void FNiagaraSystemInstance::TickDataInterfaces(float DeltaSeconds, bool bPostSimulate)
{
	if (!GetSystem() || !Component || IsDisabled())
	{
		return;
	}

	bool bReInitDataInterfaces = false;
	if (bPostSimulate)
	{
		for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
		{
			if (UNiagaraDataInterface* Interface = Pair.Key.Get())
			{
				//Ideally when we make the batching changes, we can keep the instance data in big single type blocks that can all be updated together with a single virtual call.
				bReInitDataInterfaces |= Interface->PerInstanceTickPostSimulate(&DataInterfaceInstanceData[Pair.Value], this, DeltaSeconds);
			}
		}
	}
	else
	{
		for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
		{
			if (UNiagaraDataInterface* Interface = Pair.Key.Get())
			{
				//Ideally when we make the batching changes, we can keep the instance data in big single type blocks that can all be updated together with a single virtual call.
				bReInitDataInterfaces |= Interface->PerInstanceTick(&DataInterfaceInstanceData[Pair.Value], this, DeltaSeconds);
			}
		}
	}

	if (bReInitDataInterfaces)
	{
		InitDataInterfaces();
	}
}

void FNiagaraSystemInstance::TickInstanceParameters(float DeltaSeconds)
{
	//TODO: Create helper binding objects to avoid the search in set parameter value.
	//Set System params.
	FTransform ComponentTrans = Component->GetComponentTransform();
	FVector OldPos = OwnerPositionParam.GetValue();// ComponentTrans.GetLocation();
	FVector CurrPos = ComponentTrans.GetLocation();
	OwnerPositionParam.SetValue(CurrPos);
	OwnerScaleParam.SetValue(ComponentTrans.GetScale3D());
	OwnerVelocityParam.SetValue((CurrPos - OldPos) / DeltaSeconds);
	OwnerXAxisParam.SetValue(ComponentTrans.GetRotation().GetAxisX());
	OwnerYAxisParam.SetValue(ComponentTrans.GetRotation().GetAxisY());
	OwnerZAxisParam.SetValue(ComponentTrans.GetRotation().GetAxisZ());

	FMatrix Transform = ComponentTrans.ToMatrixWithScale();
	FMatrix Inverse = Transform.Inverse();
	FMatrix Transpose = Transform.GetTransposed();
	FMatrix InverseTranspose = Inverse.GetTransposed();
	OwnerTransformParam.SetValue(Transform);
	OwnerInverseParam.SetValue(Inverse);
	OwnerTransposeParam.SetValue(Transpose);
	OwnerInverseTransposeParam.SetValue(InverseTranspose);

	FMatrix TransformNoScale = ComponentTrans.ToMatrixNoScale();
	FMatrix InverseNoScale = TransformNoScale.Inverse();
	OwnerTransformNoScaleParam.SetValue(TransformNoScale);
	OwnerInverseNoScaleParam.SetValue(InverseNoScale);

	OwnerDeltaSecondsParam.SetValue(DeltaSeconds);
	OwnerInverseDeltaSecondsParam.SetValue(1.0f / DeltaSeconds);

	//Calculate the min distance to a camera.
	UWorld* World = Component->GetWorld();
	if (World != NULL)
	{
		TArray<FVector, TInlineAllocator<8> > PlayerViewLocations;
		if (World->GetPlayerControllerIterator())
		{
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PlayerController = Iterator->Get();
				if (PlayerController && PlayerController->IsLocalPlayerController())
				{
					FVector* POVLoc = new(PlayerViewLocations) FVector;
					FRotator POVRotation;
					PlayerController->GetPlayerViewPoint(*POVLoc, POVRotation);
				}
			}
		}
		else
		{
			PlayerViewLocations.Append(World->ViewLocationsRenderedLastFrame);
		}

		float LODDistanceSqr = (PlayerViewLocations.Num() ? FMath::Square(WORLD_MAX) : 0.0f);
		for (const FVector& ViewLocation : PlayerViewLocations)
		{
			const float DistanceToEffectSqr = FVector(ViewLocation - CurrPos).SizeSquared();
			if (DistanceToEffectSqr < LODDistanceSqr)
			{
				LODDistanceSqr = DistanceToEffectSqr;
			}
		}
		OwnerMinDistanceToCameraParam.SetValue(FMath::Sqrt(LODDistanceSqr));

		OwnerEngineTimeParam.SetValue(World->TimeSeconds);
		OwnerEngineRealtimeParam.SetValue(World->RealTimeSeconds);
	}
	else
	{
		OwnerEngineTimeParam.SetValue(Age);
		OwnerEngineRealtimeParam.SetValue(Age);
	}
	SystemAgeParam.SetValue(Age);

	int32 NumAlive = 0;
	for (int32 i = 0; i < Emitters.Num(); i++)
	{
		int32 NumParticles = Emitters[i]->GetNumParticles();
		if (!Emitters[i]->IsComplete())
		{
			NumAlive++;
		}
		ParameterNumParticleBindings[i].SetValue(NumParticles);
	}
	SystemNumEmittersParam.SetValue(Emitters.Num());
	SystemNumEmittersAliveParam.SetValue(NumAlive);

	check(World);
	float SafeTimeSinceRendererd = FMath::Max(0.0f, World->GetTimeSeconds() - Component->LastRenderTime - GLastRenderTimeSafetyBias);
	SystemTimeSinceRenderedParam.SetValue(SafeTimeSinceRendererd);
	
	OwnerExecutionStateParam.SetValue((int32)RequestedExecutionState);
	
	Component->GetOverrideParameters().Tick();
	InstanceParameters.Tick();
	InstanceParameters.MarkParametersDirty();
}

#if WITH_EDITORONLY_DATA

bool FNiagaraSystemInstance::UsesEmitter(const UNiagaraEmitter* Emitter)const
{
	if (GetSystem())
	{
		return GetSystem()->UsesEmitter(Emitter);
	}
	return false;
}

bool FNiagaraSystemInstance::UsesScript(const UNiagaraScript* Script)const
{
	if (GetSystem())
	{
		for (FNiagaraEmitterHandle EmitterHandle : GetSystem()->GetEmitterHandles())
		{
			if ((EmitterHandle.GetSource() && EmitterHandle.GetSource()->UsesScript(Script)) || (EmitterHandle.GetInstance() && EmitterHandle.GetInstance()->UsesScript(Script)))
			{
				return true;
			}
		}
	}
	return false;
}

// bool FNiagaraSystemInstance::UsesDataInterface(UNiagaraDataInterface* Interface)
// {
// 
// }

bool FNiagaraSystemInstance::UsesCollection(const UNiagaraParameterCollection* Collection)const
{
	if (UNiagaraSystem* System = GetSystem())
	{
		if (System->UsesCollection(Collection))
		{
			return true;
		}
	}
	return false;
}

#endif

void FNiagaraSystemInstance::InitEmitters()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInitEmitters);
	if (Component)
	{
		Component->MarkRenderStateDirty();
	}

	//STovey: This is done after InitEmitters so removing this duplication.
// 	// Just in case this ends up being called more than in Init, we need to 
// 	// clear out the update proxy of any renderers that will be destroyed when Emitters.Empty occurs..
// 	TArray<NiagaraRenderer*> NewRenderers;
// 	UpdateProxy(NewRenderers);
// 
// 	// Clear out the System renderer from the simulation.
// 	for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
// 	{
// 		Simulation->ClearRenderer();
// 	}

	Emitters.Empty();
	if (GetSystem() != nullptr)
	{
		const TArray<FNiagaraEmitterHandle>& EmitterHandles = GetSystem()->GetEmitterHandles();
		for (int32 EmitterIdx=0; EmitterIdx < GetSystem()->GetEmitterHandles().Num(); ++EmitterIdx)
		{
			const FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[EmitterIdx];
			TSharedRef<FNiagaraEmitterInstance> Sim = MakeShareable(new FNiagaraEmitterInstance(this));
			Sim->Init(EmitterIdx, IDName);
			Emitters.Add(Sim);
		}

		for (TSharedRef<FNiagaraEmitterInstance> Simulation : Emitters)
		{
			Simulation->PostInitSimulation();
		}
	}
}

void FNiagaraSystemInstance::UpdateRenderModules(ERHIFeatureLevel::Type InFeatureLevel, TArray<NiagaraRenderer*>& OutNewRenderers, TArray<NiagaraRenderer*>& OutOldRenderers)
{
	for (TSharedPtr<FNiagaraEmitterInstance> Sim : Emitters)
	{
		Sim->UpdateEmitterRenderer(InFeatureLevel, OutNewRenderers, OutOldRenderers);
	}
}

void FNiagaraSystemInstance::UpdateProxy(TArray<NiagaraRenderer*>& InRenderers)
{
	if (!Component)
	{
		return;
	}

	FNiagaraSceneProxy *NiagaraProxy = static_cast<FNiagaraSceneProxy*>(Component->SceneProxy);
	if (NiagaraProxy)
	{
		if (Component->GetWorld() != nullptr)
		{
			// Tell the scene proxy on the render thread to update its System renderers.
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FChangeNiagaraRenderModule,
				FNiagaraSceneProxy*, InProxy, NiagaraProxy,
				TArray<NiagaraRenderer*>, InRendererArray, InRenderers,
				{
					InProxy->UpdateEmitterRenderers(InRendererArray);
				}
			);
		}
	}
}

void FNiagaraSystemInstance::ComponentTick(float DeltaSeconds)
{
	if (IsDisabled())
	{
		return;
	}

	TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> Sim = GetSystemSimulation();
	check(Sim.IsValid());
	check(IsInGameThread());
	check(bSolo);
	check(Component);
	
	TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = GetSystemSimulation();
	SystemSim->Tick(DeltaSeconds);
}

void FNiagaraSystemInstance::FinalizeTick(float DeltaSeconds)
{
	//Post tick our interfaces.
	TickDataInterfaces(DeltaSeconds, true);

	if (HasTickingEmitters())
	{
		Component->UpdateComponentToWorld();//Needed for bounds updates. Can probably skip if using fixed bounds
		Component->MarkRenderDynamicDataDirty();
	}
}

bool FNiagaraSystemInstance::HandleCompletion()
{
	bool bEmittersCompleteOrDisabled = true;
	bHasTickingEmitters = false;
	for (TSharedRef<FNiagaraEmitterInstance>&it : Emitters)
	{
		FNiagaraEmitterInstance& Inst = *it;
		bEmittersCompleteOrDisabled &= Inst.HandleCompletion();
		bHasTickingEmitters |= Inst.ShouldTick();
	}

	bool bCompletedAlready = IsComplete();
	if (bCompletedAlready || bEmittersCompleteOrDisabled)
	{
		//UE_LOG(LogNiagara, Log, TEXT("Completion Achieved"));
		Complete();
		return true;
	}
	return false;
}

void FNiagaraSystemInstance::PreSimulateTick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemPreSimulateTick);
	TickInstanceParameters(DeltaSeconds);
}

void FNiagaraSystemInstance::PostSimulateTick(float DeltaSeconds)
{
	if (IsComplete() || !bHasTickingEmitters || GetSystem() == nullptr || Component == nullptr || DeltaSeconds < SMALL_NUMBER)
	{
		return;
	}

	// pass the constants down to the emitter
	// TODO: should probably just pass a pointer to the table
	for (TPair<FNiagaraDataSetID, FNiagaraDataSet>& EventSetPair : ExternalEvents)
	{
		EventSetPair.Value.Tick();
	}

	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		FNiagaraEmitterInstance& Inst = Emitters[EmitterIdx].Get();
		Inst.PreTick();
	}

	// now tick all emitters
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		FNiagaraEmitterInstance& Inst = Emitters[EmitterIdx].Get();
		Inst.Tick(DeltaSeconds);
	}

	Age += DeltaSeconds;
}

#if WITH_EDITORONLY_DATA
bool FNiagaraSystemInstance::GetIsolateEnabled() const
{
	UNiagaraSystem* System = GetSystem();
	if (System)
	{
		return System->GetIsolateEnabled();
	}
	return false;
}
#endif

void FNiagaraSystemInstance::DestroyDataInterfaceInstanceData()
{
	for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
	{
		if (UNiagaraDataInterface* Interface = Pair.Key.Get())
		{
			Interface->DestroyPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
		}
	}
	DataInterfaceInstanceDataOffsets.Empty();
	DataInterfaceInstanceData.Empty();
}

TSharedPtr<FNiagaraEmitterInstance> FNiagaraSystemInstance::GetSimulationForHandle(const FNiagaraEmitterHandle& EmitterHandle)
{
	for (TSharedPtr<FNiagaraEmitterInstance> Sim : Emitters)
	{
		if(Sim->GetEmitterHandle().GetId() == EmitterHandle.GetId())
		{
			return Sim;
		}
	}
	return nullptr;
}

UNiagaraSystem* FNiagaraSystemInstance::GetSystem()const
{
	return Component->GetAsset();
}

FNiagaraEmitterInstance* FNiagaraSystemInstance::GetEmitterByID(FGuid InID)
{
	for (TSharedRef<FNiagaraEmitterInstance>& Emitter : Emitters)
	{
		if (Emitter->GetEmitterHandle().GetId() == InID)
		{
			return &Emitter.Get();
		}
	}
	return nullptr;
}

FNiagaraDataSet* FNiagaraSystemInstance::GetDataSet(FNiagaraDataSetID SetID, FName EmitterName)
{
	if (EmitterName == NAME_None)
	{
		if (FNiagaraDataSet* ExternalSet = ExternalEvents.Find(SetID))
		{
			return ExternalSet;
		}
	}
	for (TSharedPtr<FNiagaraEmitterInstance> Emitter : Emitters)
	{
		check(Emitter.IsValid());
		if (!Emitter->IsComplete())
		{
			if (Emitter->GetCachedIDName() == EmitterName)
			{
				return Emitter->GetDataSet(SetID);
			}
		}
	}

	return NULL;
}

FNiagaraSystemInstance::FOnInitialized& FNiagaraSystemInstance::OnInitialized()
{
	return OnInitializedDelegate;
}

FNiagaraSystemInstance::FOnComplete& FNiagaraSystemInstance::OnComplete()
{
	return OnCompleteDelegate;
}

#if WITH_EDITOR
FNiagaraSystemInstance::FOnReset& FNiagaraSystemInstance::OnReset()
{
	return OnResetDelegate;
}

FNiagaraSystemInstance::FOnDestroyed& FNiagaraSystemInstance::OnDestroyed()
{
	return OnDestroyedDelegate;
}
#endif

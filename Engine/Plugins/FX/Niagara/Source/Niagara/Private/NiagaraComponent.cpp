// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponent.h"
#include "VectorVM.h"
#include "NiagaraRenderer.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "MeshBatch.h"
#include "SceneUtils.h"
#include "ComponentReregisterContext.h"
#include "NiagaraConstants.h"
#include "NiagaraStats.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceStaticMesh.h"
#include "UObject/NameTypes.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraWorldManager.h"
#include "EngineUtils.h"

DECLARE_CYCLE_STAT(TEXT("Sceneproxy create (GT)"), STAT_NiagaraCreateSceneProxy, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Component Tick (GT)"), STAT_NiagaraComponentTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Activate (GT)"), STAT_NiagaraComponentActivate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Deactivate (GT)"), STAT_NiagaraComponentDeactivate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Send Render Data (GT)"), STAT_NiagaraComponentSendRenderData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Get Dynamic Mesh Elements (RT)"), STAT_NiagaraComponentGetDynamicMeshElements, STATGROUP_Niagara);

DEFINE_LOG_CATEGORY(LogNiagara);

static int32 GbSuppressNiagaraSystems = 0;
static FAutoConsoleVariableRef CVarSuppressNiagaraSystems(
	TEXT("fx.SuppressNiagaraSystems"),
	GbSuppressNiagaraSystems,
	TEXT("If > 0 Niagara particle systems will not be activated. \n"),
	ECVF_Default
);


void DumpNiagaraComponents(UWorld* World)
{
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		TArray<UNiagaraComponent*> Components;
		ActorItr->GetComponents<UNiagaraComponent>(Components, true);
		if (Components.Num() != 0)
		{
			UE_LOG(LogNiagara, Log, TEXT("Actor: \"%s\" ... %d Components"), *ActorItr->GetName(), Components.Num());
		}

		for (UNiagaraComponent* Component : Components)
		{
			if (Component != nullptr)
			{
				UNiagaraSystem* Sys = Component->GetAsset();
				FNiagaraSystemInstance* SysInst = Component->GetSystemInstance();
				if (!Sys)
				{
					UE_LOG(LogNiagara, Log, TEXT("Component: \"%s\" ... no system"), *Component->GetName());

				}
				else if (Sys && !SysInst)
				{
					UE_LOG(LogNiagara, Log, TEXT("Component: \"%s\" System: \"%s\" ... no instance"), *Component->GetName(), *Sys->GetName());

				}
				else
				{
					UE_LOG(LogNiagara, Log, TEXT("Component: \"%s\" System: \"%s\" | ReqExecState: %d | ExecState: %d | bIsActive: %d"), *Component->GetName(), *Sys->GetName(),
						(int32)SysInst->GetRequestedExecutionState(), (int32)SysInst->GetActualExecutionState(), Component->bIsActive);

					if (!SysInst->IsComplete())
					{
						for (TSharedRef<FNiagaraEmitterInstance> Emitter : SysInst->GetEmitters())
						{
							UE_LOG(LogNiagara, Log, TEXT("    Emitter: \"%s\" | ExecState: %d | NumParticles: %d | CPUTime: %f"), *Emitter->GetEmitterHandle().GetUniqueInstanceName(),
								(int32)Emitter->GetExecutionState(), Emitter->GetNumParticles(), Emitter->GetTotalCPUTime());
						}
					}
				}
			}
		}
	}
}

FAutoConsoleCommandWithWorld DumpNiagaraComponentsCommand(
	TEXT("DumpNiagaraComponents"),
	TEXT("Dump Existing Niagara Components"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DumpNiagaraComponents)
);


FNiagaraSceneProxy::FNiagaraSceneProxy(const UNiagaraComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, bRenderingEnabled(true)
{
	// In this case only, update the System renderers on the game thread.
	check(IsInGameThread());
	FNiagaraSystemInstance* SystemInst = InComponent->GetSystemInstance();
	if (SystemInst)
	{
		TArray<TSharedRef<FNiagaraEmitterInstance> > &Sims = SystemInst->GetEmitters();
		TArray<NiagaraRenderer*> RenderersFromSims;
		for (TSharedRef<FNiagaraEmitterInstance> Sim : Sims)
		{
			for (int32 i = 0; i < Sim->GetEmitterRendererNum(); i++)
			{
				RenderersFromSims.Add(Sim->GetEmitterRenderer(i));
			}
		}
		UpdateEmitterRenderers(RenderersFromSims);
		//UE_LOG(LogNiagara, Warning, TEXT("FNiagaraSceneProxy %p"), this);

		bAlwaysHasVelocity = true;
	}
}

SIZE_T FNiagaraSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}
void FNiagaraSceneProxy::UpdateEmitterRenderers(TArray<NiagaraRenderer*>& InRenderers)
{
	EmitterRenderers.Empty();
	for (NiagaraRenderer* EmitterRenderer : InRenderers)
	{
		if (EmitterRenderer)
		{
			EmitterRenderers.Add(EmitterRenderer);
		}
	}

	// We sort by the sort hint in order to guarantee that we submit according to the preferred sort order..
	EmitterRenderers.Sort([&](const NiagaraRenderer& A, const NiagaraRenderer& B)
	{
		check(A.GetRendererProperties());
		check(B.GetRendererProperties());
		int32 AIndex = A.GetRendererProperties()->SortOrderHint;
		int32 BIndex = B.GetRendererProperties()->SortOrderHint;
		return AIndex < BIndex;
	});
}

FNiagaraSceneProxy::~FNiagaraSceneProxy()
{
	//UE_LOG(LogNiagara, Warning, TEXT("~FNiagaraSceneProxy %p"), this);
	ReleaseRenderThreadResources();
}

/** Called on render thread to assign new dynamic data */
void FNiagaraSceneProxy::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->SetDynamicData_RenderThread(NewDynamicData);
		}
	}
	return;
}


void FNiagaraSceneProxy::ReleaseRenderThreadResources()
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->ReleaseRenderThreadResources();
		}
	}
	return;
}

// FPrimitiveSceneProxy interface.
void FNiagaraSceneProxy::CreateRenderThreadResources()
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->CreateRenderThreadResources();
		}
	}
	return;
}

void FNiagaraSceneProxy::OnTransformChanged()
{
	//WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

FPrimitiveViewRelevance FNiagaraSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;

	if (bRenderingEnabled == false)
	{
		return Relevance;
	}
	Relevance.bDynamicRelevance = true;

	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer && Renderer->GetRendererProperties()->GetIsEnabled())
		{
			Relevance |= Renderer->GetViewRelevance(View, this);
		}
	}
	return Relevance;
}


uint32 FNiagaraSceneProxy::GetMemoryFootprint() const
{ 
	return (sizeof(*this) + GetAllocatedSize()); 
}

uint32 FNiagaraSceneProxy::GetAllocatedSize() const
{ 
	uint32 DynamicDataSize = 0;
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			DynamicDataSize += Renderer->GetDynamicDataSize();
		}
	}
	return FPrimitiveSceneProxy::GetAllocatedSize() + DynamicDataSize;
}

bool FNiagaraSceneProxy::GetRenderingEnabled() const
{
	return bRenderingEnabled;
}

void FNiagaraSceneProxy::SetRenderingEnabled(bool bInRenderingEnabled)
{
	bRenderingEnabled = bInRenderingEnabled;
}

void FNiagaraSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraComponentGetDynamicMeshElements);
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, this);
		}
	}

	if (ViewFamily.EngineShowFlags.Particles)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
				if (HasCustomOcclusionBounds())
				{
					RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetCustomOcclusionBounds(), IsSelected());
				}
			}
		}
	}
}



void FNiagaraSceneProxy::GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const
{
	NiagaraRendererLights *LightRenderer = nullptr;
	FNiagaraDynamicDataLights *DynamicData = nullptr;
	for (int32 Idx = 0; Idx < EmitterRenderers.Num(); Idx++)
	{
		NiagaraRenderer *Renderer = EmitterRenderers[Idx];
		if (Renderer && Renderer->GetPropertiesClass() == UNiagaraLightRendererProperties::StaticClass())
		{
			LightRenderer = static_cast<NiagaraRendererLights*>(Renderer);
			DynamicData = static_cast<FNiagaraDynamicDataLights*>(Renderer->GetDynamicData());
			break;
		}
	}


	if (DynamicData)
	{
		int32 LightCount = DynamicData->LightArray.Num();
		
		OutParticleLights.InstanceData.Reserve(LightCount);
		OutParticleLights.PerViewData.Reserve(LightCount);

		for (NiagaraRendererLights::SimpleLightData &LightData : DynamicData->LightArray)
		{
			// When not using camera-offset, output one position for all views to share. 
			OutParticleLights.PerViewData.Add(LightData.PerViewEntry);

			// Add an entry for the light instance.
			OutParticleLights.InstanceData.Add(LightData.LightEntry);
		}
	}

}


//////////////////////////////////////////////////////////////////////////

UNiagaraComponent::UNiagaraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OverrideParameters(this)
	, bForceSolo(false)
	, AgeUpdateMode(ENiagaraAgeUpdateMode::TickDeltaTime)
	, DesiredAge(0.0f)
	, bCanRenderWhileSeeking(true)
	, SeekDelta(1 / 30.0f)
	, MaxSimTime(33.0f / 1000.0f)
	, bIsSeeking(false)
	, bAutoDestroy(false)
#if WITH_EDITOR
	, bWaitForCompilationOnActivate(false)
#endif
	, bAwaitingActivationDueToNotReady(false)
	//, bIsChangingAutoAttachment(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	bTickInEditor = true;
	bAutoActivate = true;
	bRenderingEnabled = true;
	SavedAutoAttachRelativeScale3D = FVector(1.f, 1.f, 1.f);
}


void UNiagaraComponent::TickComponent(float DeltaSeconds, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraComponentTick);

	if (bAwaitingActivationDueToNotReady)
	{
		Activate(bActivateShouldResetWhenReady);
		return;
	}

	if (!bIsActive && bAutoActivate)
	{
		Activate();
	}

	if (!SystemInstance)
	{
		return;
	}

	check(SystemInstance->IsSolo());
	if (bIsActive && SystemInstance.Get() && !SystemInstance->IsComplete())
	{
		// If the interfaces have changed in a meaningful way, we need to potentially rebind and update the values.
		if (OverrideParameters.GetInterfacesDirty())
		{
			SystemInstance->Reset(FNiagaraSystemInstance::EResetMode::ReInit);
		}

		if (AgeUpdateMode == ENiagaraAgeUpdateMode::TickDeltaTime)
		{
			SystemInstance->ComponentTick(DeltaSeconds);
		}
		else
		{
			float AgeDiff = FMath::Max(DesiredAge, 0.0f) - SystemInstance->GetAge();
			int32 TicksToProcess = 0;
			if (FMath::Abs(AgeDiff) < KINDA_SMALL_NUMBER)
			{
				AgeDiff = 0.0f;
			}
			else
			{
				if (AgeDiff < 0.0f)
				{
					SystemInstance->Reset(FNiagaraSystemInstance::EResetMode::ResetAll);
					AgeDiff = DesiredAge - SystemInstance->GetAge();
				}

				if (AgeDiff > 0.0f)
				{
					FNiagaraSystemSimulation* SystemSim = GetSystemSimulation().Get();
					if (SystemSim)
					{
						double StartTime = FPlatformTime::Seconds();
						double CurrentTime = StartTime;

						TicksToProcess = FMath::FloorToInt(AgeDiff / SeekDelta);
						for (; TicksToProcess > 0 && CurrentTime - StartTime < MaxSimTime; --TicksToProcess)
						{
							SystemInstance->ComponentTick(SeekDelta);
							CurrentTime = FPlatformTime::Seconds();
						}
					}
				}
			}

			if (TicksToProcess == 0)
			{
				bIsSeeking = false;
			}
		}

		if (SceneProxy != nullptr)
		{
			FNiagaraSceneProxy* NiagaraProxy = static_cast<FNiagaraSceneProxy*>(SceneProxy);
			NiagaraProxy->SetRenderingEnabled(bRenderingEnabled && (bCanRenderWhileSeeking || bIsSeeking == false));
		}
	}
}

const UObject* UNiagaraComponent::AdditionalStatObject() const
{
	return Asset;
}

void UNiagaraComponent::ResetSystem()
{
	Activate(true);
}

void UNiagaraComponent::ReinitializeSystem()
{
	DestroyInstance();
	Activate();
}

bool UNiagaraComponent::GetRenderingEnabled() const
{
	return bRenderingEnabled;
}

void UNiagaraComponent::SetRenderingEnabled(bool bInRenderingEnabled)
{
	bRenderingEnabled = bInRenderingEnabled;
}

void UNiagaraComponent::AdvanceSimulation(int32 TickCount, float TickDeltaSeconds)
{
	if (SystemInstance.IsValid() && TickDeltaSeconds > SMALL_NUMBER)
	{
		SystemInstance->AdvanceSimulation(TickCount, TickDeltaSeconds);
	}
}

void UNiagaraComponent::AdvanceSimulationByTime(float SimulateTime, float TickDeltaSeconds)
{
	if (SystemInstance.IsValid() && TickDeltaSeconds > SMALL_NUMBER)
	{
		int32 TickCount = SimulateTime / TickDeltaSeconds;
		SystemInstance->AdvanceSimulation(TickCount, TickDeltaSeconds);
	}
}

bool UNiagaraComponent::InitializeSystem()
{
	if (SystemInstance.IsValid() == false)
	{
		//UE_LOG(LogNiagara, Warning, TEXT("Create System: %0X | %s\n"), this, *GetAsset()->GetFullName());
		SystemInstance = MakeUnique<FNiagaraSystemInstance>(this);
#if WITH_EDITORONLY_DATA
		OnSystemInstanceChangedDelegate.Broadcast();
#endif
		SystemInstance->Init(GetAsset(), bForceSolo);
		return true;
	}
	return false;
}

void UNiagaraComponent::Activate(bool bReset /* = false */)
{
	bAwaitingActivationDueToNotReady = false;

	if (GbSuppressNiagaraSystems != 0)
	{
		OnSystemComplete();
		return;
	}

	if (IsSwitchPlatform(GMaxRHIShaderPlatform))
	{
		UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara component as Niagara is not yet supported on this platform: %s"), *LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString());
		OnSystemComplete();
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_NiagaraComponentActivate);
	if (Asset == nullptr)
	{
		DestroyInstance();
		if (!HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject | RF_ClassDefaultObject))
		{
			UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara Component due to missing or invalid asset!"));
		}
		SetComponentTickEnabled(false);
		return;
	}
	
	// If the particle system can't ever render (ie on dedicated server or in a commandlet) than do not activate...
	if (!FApp::CanEverRender())
	{
		return;
	}

	if (!IsRegistered())
	{
		return;
	}

	// On the off chance that the user changed the asset, we need to clear out the existing data.
	if (SystemInstance.IsValid() && SystemInstance->GetSystem() != Asset)
	{
		OnSystemComplete();
	}

#if WITH_EDITOR
	// In case we're not yet ready to run due to compilation requests, go ahead and keep polling there..
	if (Asset->HasOutstandingCompilationRequests())
	{
		if (bWaitForCompilationOnActivate)
		{
			Asset->WaitForCompilationComplete();
		}
		Asset->PollForCompilationComplete();
	}
#endif

	if (!Asset->IsReadyToRun())
	{
		bAwaitingActivationDueToNotReady = true;
		bActivateShouldResetWhenReady = bReset;
		SetComponentTickEnabled(true);
		return;
	}

	
	Super::Activate(bReset);

	//UE_LOG(LogNiagara, Log, TEXT("Activate: %u - %s"), this, *Asset->GetName());
	
	// Auto attach if requested
	const bool bWasAutoAttached = bDidAutoAttach;
	bDidAutoAttach = false;
	if (bAutoManageAttachment)
	{
		USceneComponent* NewParent = AutoAttachParent.Get();
		if (NewParent)
		{
			const bool bAlreadyAttached = GetAttachParent() && (GetAttachParent() == NewParent) && (GetAttachSocketName() == AutoAttachSocketName) && GetAttachParent()->GetAttachChildren().Contains(this);
			if (!bAlreadyAttached)
			{
				bDidAutoAttach = bWasAutoAttached;
				CancelAutoAttachment(true);
				SavedAutoAttachRelativeLocation = RelativeLocation;
				SavedAutoAttachRelativeRotation = RelativeRotation;
				SavedAutoAttachRelativeScale3D = RelativeScale3D;
				//bIsChangingAutoAttachment = true;
				AttachToComponent(NewParent, FAttachmentTransformRules(AutoAttachLocationRule, AutoAttachRotationRule, AutoAttachScaleRule, false), AutoAttachSocketName);
				//bIsChangingAutoAttachment = false;
			}

			bDidAutoAttach = true;
			//bFlagAsJustAttached = true;
		}
		else
		{
			CancelAutoAttachment(true);
		}
	}

	FNiagaraSystemInstance::EResetMode ResetMode = bReset ? FNiagaraSystemInstance::EResetMode::ResetAll : FNiagaraSystemInstance::EResetMode::ResetSystem;
	if (InitializeSystem())
	{
		ResetMode = FNiagaraSystemInstance::EResetMode::None;//Already done a reinit sete
	}

	if (!SystemInstance)
	{
		return;
	}

	SystemInstance->Activate(ResetMode);

	/** We only need to tick the component if we require solo mode. */
	SetComponentTickEnabled(SystemInstance->IsSolo());
}

void UNiagaraComponent::Deactivate()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraComponentDeactivate);
	Super::Deactivate();

	//UE_LOG(LogNiagara, Log, TEXT("Deactivate: %u - %s"), this, *Asset->GetName());

	bIsActive = false;

	if (SystemInstance)
	{
		SystemInstance->Deactivate();
	}
}

void UNiagaraComponent::DeactivateImmediate()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraComponentDeactivate);
	Super::Deactivate();

	//UE_LOG(LogNiagara, Log, TEXT("DeactivateImmediate: %u - %s"), this, *Asset->GetName());

	//UE_LOG(LogNiagara, Log, TEXT("Deactivate %s"), *GetName());

	bIsActive = false;

	if (SystemInstance)
	{
		SystemInstance->Deactivate(true);
	}
}

void UNiagaraComponent::OnSystemComplete()
{
	//UE_LOG(LogNiagara, Log, TEXT("OnSystemComplete: %u - %s"), this, *Asset->GetName());

	SetComponentTickEnabled(false);
	bIsActive = false;

	MarkRenderDynamicDataDirty();
		
	OnSystemFinished.Broadcast(this);

	if (bAutoDestroy)
	{
		DestroyComponent();
	}
	else if (bAutoManageAttachment)
	{
		CancelAutoAttachment(/*bDetachFromParent=*/ true);
	}
}

void UNiagaraComponent::DestroyInstance()
{
	//UE_LOG(LogNiagara, Warning, TEXT("Destroy System: %s\n"), *GetAsset()->GetFullName());
	//UE_LOG(LogNiagara, Log, TEXT("DestroyInstance: %u - %s"), this, *Asset->GetName());
	bIsActive = false;
	SystemInstance = nullptr;
#if WITH_EDITORONLY_DATA
	OnSystemInstanceChangedDelegate.Broadcast();
#endif
}

void UNiagaraComponent::OnRegister()
{
	if (bAutoManageAttachment && !IsActive())
	{
		// Detach from current parent, we are supposed to wait for activation.
		if (GetAttachParent())
		{
			// If no auto attach parent override, use the current parent when we activate
			if (!AutoAttachParent.IsValid())
			{
				AutoAttachParent = GetAttachParent();
			}
			// If no auto attach socket override, use current socket when we activate
			if (AutoAttachSocketName == NAME_None)
			{
				AutoAttachSocketName = GetAttachSocketName();
			}

			// Prevent attachment before Super::OnRegister() tries to attach us, since we only attach when activated.
			if (GetAttachParent()->GetAttachChildren().Contains(this))
			{
				// Only detach if we are not about to auto attach to the same target, that would be wasteful.
				if (!bAutoActivate || (AutoAttachLocationRule != EAttachmentRule::KeepRelative && AutoAttachRotationRule != EAttachmentRule::KeepRelative && AutoAttachScaleRule != EAttachmentRule::KeepRelative) || (AutoAttachSocketName != GetAttachSocketName()) || (AutoAttachParent != GetAttachParent()))
				{
					//bIsChangingAutoAttachment = true;
					DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, /*bCallModify=*/ false));
					//bIsChangingAutoAttachment = false;
				}
			}
			else
			{
				SetupAttachment(nullptr, NAME_None);
			}
		}

		SavedAutoAttachRelativeLocation = RelativeLocation;
		SavedAutoAttachRelativeRotation = RelativeRotation;
		SavedAutoAttachRelativeScale3D = RelativeScale3D;
	}
	Super::OnRegister();
}

void UNiagaraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	//UE_LOG(LogNiagara, Log, TEXT("OnComponentDestroyed %p %p"), this, SystemInstance.Get());
	//DestroyInstance();//Can't do this here as we can call this from inside the system instance currently during completion 

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UNiagaraComponent::OnUnregister()
{
	Super::OnUnregister();

	bIsActive = false;

	if (SystemInstance)
	{
		SystemInstance->Deactivate(true);
	}
}

void UNiagaraComponent::BeginDestroy()
{
	DestroyInstance();

	Super::BeginDestroy();
}

// Uncertain about this. 
// void UNiagaraComponent::OnAttachmentChanged()
// {
// 	Super::OnAttachmentChanged();
// 	if (bIsActive && !bIsChangingAutoAttachment && !GetOwner()->IsPendingKillPending())
// 	{
// 		ResetSystem();
// 	}
// }

TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> UNiagaraComponent::GetSystemSimulation()
{
	if (SystemInstance)
	{
		return SystemInstance->GetSystemSimulation();
	}

	return nullptr;
}

void UNiagaraComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();
	// The emitter instance may not tick again next frame so we send the dynamic data here so that the current state
	// renders.  This can happen when while editing, or any time the age update mode is set to desired age.
	SendRenderDynamicData_Concurrent();
}

void UNiagaraComponent::SendRenderDynamicData_Concurrent()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraComponentSendRenderData);
	if (SystemInstance.IsValid() && SceneProxy)
	{
		SystemInstance->GetSystemBounds().Init();
		
		FNiagaraSceneProxy* NiagaraProxy = static_cast<FNiagaraSceneProxy*>(SceneProxy);

		for (int32 i = 0; i < SystemInstance->GetEmitters().Num(); i++)
		{
			FNiagaraEmitterInstance* Emitter = &SystemInstance->GetEmitters()[i].Get();
			for (int32 EmitterIdx = 0; EmitterIdx < Emitter->GetEmitterRendererNum(); EmitterIdx++)
			{
				NiagaraRenderer* Renderer = Emitter->GetEmitterRenderer(EmitterIdx);
				if (Renderer)
				{
					bool bRendererEditorEnabled = true;
#if WITH_EDITORONLY_DATA
					bRendererEditorEnabled = (!SystemInstance->GetIsolateEnabled() || Emitter->GetEmitterHandle().IsIsolated());
#endif
					bRendererEditorEnabled &= Renderer->GetRendererProperties()->GetIsEnabled();
					if (bRendererEditorEnabled && !Emitter->IsComplete() && !SystemInstance->IsComplete())
					{
						FNiagaraDynamicDataBase* DynamicData = Renderer->GenerateVertexData(NiagaraProxy, Emitter->GetData(), Emitter->GetEmitterHandle().GetInstance()->SimTarget);

						ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
							FSendNiagaraDynamicData,
							NiagaraRenderer*, EmitterRenderer, Emitter->GetEmitterRenderer(EmitterIdx),
							FNiagaraDynamicDataBase*, DynamicData, DynamicData,
							{
								EmitterRenderer->SetDynamicData_RenderThread(DynamicData);
							});
					}
					else
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
							FSendNiagaraDynamicData,
							NiagaraRenderer*, EmitterRenderer, Emitter->GetEmitterRenderer(EmitterIdx),
							{
								EmitterRenderer->SetDynamicData_RenderThread(nullptr);
							});
					}
				}
			}
		}
	}

}

int32 UNiagaraComponent::GetNumMaterials() const
{
	return 0;
}


FBoxSphereBounds UNiagaraComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox SimBounds(ForceInit);
	if (SystemInstance.IsValid())
	{
		SystemInstance->GetSystemBounds().Init();
		for (int32 i = 0; i < SystemInstance->GetEmitters().Num(); i++)
		{
			FNiagaraEmitterInstance &Sim = *(SystemInstance->GetEmitters()[i]);
			SystemInstance->GetSystemBounds() += Sim.GetBounds();
		}
		FBox BoundingBox = SystemInstance->GetSystemBounds();
		const FVector ExpandAmount = FVector(0.0f, 0.0f, 0.0f);// BoundingBox.GetExtent() * 0.1f;
		BoundingBox = FBox(BoundingBox.Min - ExpandAmount, BoundingBox.Max + ExpandAmount);

		FBoxSphereBounds BSBounds(BoundingBox);
		return BSBounds;
	}
	return FBoxSphereBounds(SimBounds);
}

FPrimitiveSceneProxy* UNiagaraComponent::CreateSceneProxy()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraCreateSceneProxy);
	// The constructor will set up the System renderers from the component.
	FNiagaraSceneProxy* Proxy = new FNiagaraSceneProxy(this);
	return Proxy;
}

void UNiagaraComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (!SystemInstance.IsValid())
	{
		return;
	}

	for (TSharedRef<FNiagaraEmitterInstance> Sim : SystemInstance->GetEmitters())	
	{
		if (UNiagaraEmitter* Props = Sim->GetEmitterHandle().GetInstance())
		{	
			for (int32 i = 0; i < Props->GetRenderers().Num(); i++)
			{
				if (UNiagaraRendererProperties* Renderer = Props->GetRenderers()[i])
				{
					Renderer->GetUsedMaterials(OutMaterials);
				}
			}
		}
	}
}

FNiagaraSystemInstance* UNiagaraComponent::GetSystemInstance() const
{
	return SystemInstance.Get();
}

void UNiagaraComponent::SetNiagaraVariableLinearColor(const FString& InVariableName, const FLinearColor& InValue)
{
	FName VarName = FName(*InVariableName);

	OverrideParameters.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), VarName), true);
}

void UNiagaraComponent::SetNiagaraVariableQuat(const FString& InVariableName, const FQuat& InValue)
{
	FName VarName = FName(*InVariableName);

	OverrideParameters.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), VarName), true);
}

void UNiagaraComponent::SetNiagaraVariableVec4(const FString& InVariableName, const FVector4& InValue)
{
	FName VarName = FName(*InVariableName);

	OverrideParameters.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), VarName), true);
}

void UNiagaraComponent::SetNiagaraVariableVec3(const FString& InVariableName, FVector InValue)
{
	FName VarName = FName(*InVariableName);
	
	OverrideParameters.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), VarName), true);
}

void UNiagaraComponent::SetNiagaraVariableVec2(const FString& InVariableName, FVector2D InValue)
{
	FName VarName = FName(*InVariableName);

	OverrideParameters.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(),VarName), true);
}

void UNiagaraComponent::SetNiagaraVariableFloat(const FString& InVariableName, float InValue)
{
	FName VarName = FName(*InVariableName);

	OverrideParameters.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), VarName), true);
}

void UNiagaraComponent::SetNiagaraVariableBool(const FString& InVariableName, bool InValue)
{
	FName VarName = FName(*InVariableName);

	OverrideParameters.SetParameterValue(InValue ? FNiagaraBool::True : FNiagaraBool::False, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), VarName), true);
}

TArray<FVector> UNiagaraComponent::GetNiagaraParticlePositions_DebugOnly(const FString& InEmitterName)
{
	return GetNiagaraParticleValueVec3_DebugOnly(InEmitterName, TEXT("Position"));
}

TArray<FVector> UNiagaraComponent::GetNiagaraParticleValueVec3_DebugOnly(const FString& InEmitterName, const FString& InValueName)
{
	TArray<FVector> Positions;
	FName EmitterName = FName(*InEmitterName);
	if (SystemInstance.IsValid())
	{
		for (TSharedRef<FNiagaraEmitterInstance>& Sim : SystemInstance->GetEmitters())
		{
			if (Sim->GetEmitterHandle().GetName() == EmitterName)
			{
				int32 NumParticles = Sim->GetData().GetNumInstances();
				Positions.SetNum(NumParticles);
				FNiagaraDataSetIterator<FVector> PosItr(Sim->GetData(), FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), *InValueName));
				int32 i = 0;
				while (PosItr.IsValid())
				{
					FVector Position;
					PosItr.Get(Position);
					Positions[i] = Position;
					i++;
					PosItr.Advance();
				}
			}
		}
	}
	return Positions;

}

TArray<float> UNiagaraComponent::GetNiagaraParticleValues_DebugOnly(const FString& InEmitterName, const FString& InValueName)
{

	TArray<float> Values;
	FName EmitterName = FName(*InEmitterName);
	if (SystemInstance.IsValid())
	{
		for (TSharedRef<FNiagaraEmitterInstance>& Sim : SystemInstance->GetEmitters())
		{
			if (Sim->GetEmitterHandle().GetName() == EmitterName)
			{
				int32 NumParticles = Sim->GetData().GetNumInstances();
				Values.SetNum(NumParticles);
				FNiagaraDataSetIterator<float> ValueItr(Sim->GetData(), FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *InValueName));
				int32 i = 0;
				while (ValueItr.IsValid())
				{
					float Value;
					ValueItr.Get(Value);
					Values[i] = Value;
					i++;
					ValueItr.Advance();
				}
			}
		}
	}
	return Values;
}

void UNiagaraComponent::PostLoad()
{
	Super::PostLoad();
	if (Asset)
	{
		Asset->ConditionalPostLoad();
#if WITH_EDITOR
		SynchronizeWithSourceSystem();
		AssetExposedParametersChangedHandle = Asset->GetExposedParameters().AddOnChangedHandler(
			FNiagaraParameterStore::FOnChanged::FDelegate::CreateUObject(this, &UNiagaraComponent::AssetExposedParametersChanged));
#endif
	}
}

#if WITH_EDITOR

void UNiagaraComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange != nullptr && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraComponent, Asset) && Asset != nullptr)
	{
		Asset->GetExposedParameters().RemoveOnChangedHandler(AssetExposedParametersChangedHandle);
	}
}

void UNiagaraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraComponent, Asset))
	{
		SynchronizeWithSourceSystem();
		if (Asset != nullptr)
		{
			AssetExposedParametersChangedHandle = Asset->GetExposedParameters().AddOnChangedHandler(
				FNiagaraParameterStore::FOnChanged::FDelegate::CreateUObject(this, &UNiagaraComponent::AssetExposedParametersChanged));
		}
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraComponent, OverrideParameters))
	{
		SynchronizeWithSourceSystem();
	}

	ReinitializeSystem();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UNiagaraComponent::SynchronizeWithSourceSystem()
{
	// Synchronizing parameters will create new data interface objects and if the old data
	// interface objects are currently being used by a simulation they may be destroyed due to garbage
	// collection, so preemptively kill the instance here.
	DestroyInstance();

	//TODO: Look through params in system in "Owner" namespace and add to our parameters.
	if (Asset == nullptr)
	{
		OverrideParameters.Empty();
		EditorOverridesValue.Empty();
		return;
	}

	TArray<FNiagaraVariable> SourceVars;
	Asset->GetExposedParameters().GetParameters(SourceVars);

	for (FNiagaraVariable& Param : SourceVars)
	{
		OverrideParameters.AddParameter(Param, true);
	}

	TArray<FNiagaraVariable> ExistingVars;
	OverrideParameters.GetParameters(ExistingVars);

	for (FNiagaraVariable ExistingVar : ExistingVars)
	{
		if (!SourceVars.Contains(ExistingVar))
		{
			OverrideParameters.RemoveParameter(ExistingVar);
			EditorOverridesValue.Remove(ExistingVar.GetName());
		}
	}

	for (FNiagaraVariable ExistingVar : ExistingVars)
	{
		bool* FoundVar = EditorOverridesValue.Find(ExistingVar.GetName());

		if (!IsParameterValueOverriddenLocally(ExistingVar.GetName()))
		{
			Asset->GetExposedParameters().CopyParameterData(OverrideParameters, ExistingVar);
		}
	}

	OverrideParameters.Rebind();

#if WITH_EDITORONLY_DATA
	OnSynchronizedWithAssetParametersDelegate.Broadcast();
#endif
}

void UNiagaraComponent::AssetExposedParametersChanged()
{
	SynchronizeWithSourceSystem();
}
#endif

ENiagaraAgeUpdateMode UNiagaraComponent::GetAgeUpdateMode() const
{
	return AgeUpdateMode;
}

void UNiagaraComponent::SetAgeUpdateMode(ENiagaraAgeUpdateMode InAgeUpdateMode)
{
	AgeUpdateMode = InAgeUpdateMode;
}

float UNiagaraComponent::GetDesiredAge() const
{
	return DesiredAge;
}

void UNiagaraComponent::SetDesiredAge(float InDesiredAge)
{
	DesiredAge = InDesiredAge;
	bIsSeeking = false;
}

void UNiagaraComponent::SeekToDesiredAge(float InDesiredAge)
{
	DesiredAge = InDesiredAge;
	bIsSeeking = true;
}

void UNiagaraComponent::SetCanRenderWhileSeeking(bool bInCanRenderWhileSeeking)
{
	bCanRenderWhileSeeking = bInCanRenderWhileSeeking;
}

float UNiagaraComponent::GetSeekDelta() const
{
	return SeekDelta;
}

void UNiagaraComponent::SetSeekDelta(float InSeekDelta)
{
	SeekDelta = InSeekDelta;
}

float UNiagaraComponent::GetMaxSimTime() const
{
	return MaxSimTime;
}

void UNiagaraComponent::SetMaxSimTime(float InMaxTime)
{
	MaxSimTime = InMaxTime;
}

#if WITH_EDITOR
bool UNiagaraComponent::IsParameterValueOverriddenLocally(const FName& InParamName)
{
	bool* FoundVar = EditorOverridesValue.Find(InParamName);

	if (FoundVar != nullptr && *(FoundVar))
	{
		return true;
	}
	return false;
}

void UNiagaraComponent::SetParameterValueOverriddenLocally(const FNiagaraVariable& InParam, bool bInOverriden)
{
	bool* FoundVar = EditorOverridesValue.Find(InParam.GetName());

	if (FoundVar != nullptr && bInOverriden) 
	{
		*(FoundVar) = bInOverriden;
	}
	else if (FoundVar == nullptr && bInOverriden)			
	{
		EditorOverridesValue.Add(InParam.GetName(), true);
	}
	else
	{
		EditorOverridesValue.Remove(InParam.GetName());
		Asset->GetExposedParameters().CopyParameterData(OverrideParameters, InParam);
	}
}


#endif // WITH_EDITOR



void UNiagaraComponent::SetAsset(UNiagaraSystem* InAsset)
{
	if (Asset != InAsset)
	{
#if WITH_EDITOR
		if (Asset != nullptr)
		{
			Asset->GetExposedParameters().RemoveOnChangedHandler(AssetExposedParametersChangedHandle);
		}
#endif
		Asset = InAsset;

#if WITH_EDITOR
		SynchronizeWithSourceSystem();
		AssetExposedParametersChangedHandle = Asset->GetExposedParameters().AddOnChangedHandler(
			FNiagaraParameterStore::FOnChanged::FDelegate::CreateUObject(this, &UNiagaraComponent::AssetExposedParametersChanged));
#endif

		//Force a reinit.
		DestroyInstance();
	}
}

void UNiagaraComponent::SetForceSolo(bool bInForceSolo) 
{ 
	if (bForceSolo != bInForceSolo)
	{
		bForceSolo = bInForceSolo;
		DestroyInstance();
		SetComponentTickEnabled(bInForceSolo);
	}
}

void UNiagaraComponent::SetAutoAttachmentParameters(USceneComponent* Parent, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule)
{
	AutoAttachParent = Parent;
	AutoAttachSocketName = SocketName;
	AutoAttachLocationRule = LocationRule;
	AutoAttachRotationRule = RotationRule;
	AutoAttachScaleRule = ScaleRule;
}


void UNiagaraComponent::CancelAutoAttachment(bool bDetachFromParent)
{
	if (bAutoManageAttachment)
	{
		if (bDidAutoAttach)
		{
			// Restore relative transform from before attachment. Actual transform will be updated as part of DetachFromParent().
			RelativeLocation = SavedAutoAttachRelativeLocation;
			RelativeRotation = SavedAutoAttachRelativeRotation;
			RelativeScale3D = SavedAutoAttachRelativeScale3D;
			bDidAutoAttach = false;
		}

		if (bDetachFromParent)
		{
			//bIsChangingAutoAttachment = true;
			DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			//bIsChangingAutoAttachment = false;
		}
	}
}

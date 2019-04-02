// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraObjectSelection.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScriptSource.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraParameterEditMode.h"
#include "NiagaraGraph.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EmitterEditorViewModel"

template<> TMap<UNiagaraEmitter*, TArray<FNiagaraEmitterViewModel*>> TNiagaraViewModelManager<UNiagaraEmitter, FNiagaraEmitterViewModel>::ObjectsToViewModels{};

const FText FNiagaraEmitterViewModel::StatsFormat = NSLOCTEXT("NiagaraEmitterViewModel", "StatsFormat", "{0} Particles | {1} ms | {2} MB | {3}");
const FText FNiagaraEmitterViewModel::StatsParticleCountFormat = NSLOCTEXT("NiagaraEmitterViewModel", "StatsParticleCountFormat", "{0} Particles");

namespace NiagaraCommands
{
	static FAutoConsoleVariable EmitterStatsFormat(TEXT("Niagara.EmitterStatsFormat"), 1, TEXT("0 shows the particles count, ms, mb and state. 1 shows particles count."));
}

const float Megabyte = 1024.0f * 1024.0f;

FNiagaraEmitterViewModel::FNiagaraEmitterViewModel(UNiagaraEmitter* InEmitter, TWeakPtr<FNiagaraEmitterInstance> InSimulation)
	: Emitter(InEmitter)
	, Simulation(InSimulation)
	, SharedScriptViewModel(MakeShareable(new FNiagaraScriptViewModel(InEmitter, LOCTEXT("SharedDisplayName", "Graph"), ENiagaraParameterEditMode::EditAll)))
	, bUpdatingSelectionInternally(false)
	, ExecutionStateEnum(StaticEnum<ENiagaraExecutionState>())
{	
	SetEmitter(InEmitter);
}

void FNiagaraEmitterViewModel::Cleanup()
{
	if (Emitter.IsValid())
	{
		Emitter->OnEmitterVMCompiled().RemoveAll(this);
		Emitter->OnPropertiesChanged().RemoveAll(this);
	}

	if (SharedScriptViewModel.IsValid())
	{
		SharedScriptViewModel->GetGraphViewModel()->GetSelection()->OnSelectedObjectsChanged().RemoveAll(this);
		SharedScriptViewModel.Reset();
	}

	RemoveScriptEventHandlers();
}

bool FNiagaraEmitterViewModel::Set(UNiagaraEmitter* InEmitter, TWeakPtr<FNiagaraEmitterInstance> InSimulation)
{
	SetEmitter(InEmitter);
	SetSimulation(InSimulation);
	return true;
}

FNiagaraEmitterViewModel::~FNiagaraEmitterViewModel()
{
	Cleanup();
	UnregisterViewModelWithMap(RegisteredHandle);

	//UE_LOG(LogNiagaraEditor, Warning, TEXT("Deleting Emitter view model %p"), this);
}


void FNiagaraEmitterViewModel::SetEmitter(UNiagaraEmitter* InEmitter)
{
	if (Emitter.IsValid())
	{
		Emitter->OnEmitterVMCompiled().RemoveAll(this);
		Emitter->OnPropertiesChanged().RemoveAll(this);
	}

	UnregisterViewModelWithMap(RegisteredHandle);

	RemoveScriptEventHandlers();

	Emitter = InEmitter;

	if (Emitter.IsValid())
	{
		Emitter->OnEmitterVMCompiled().AddRaw(this, &FNiagaraEmitterViewModel::OnVMCompiled);
		Emitter->OnPropertiesChanged().AddRaw(this, &FNiagaraEmitterViewModel::OnEmitterPropertiesChanged);
	}

	AddScriptEventHandlers();

	RegisteredHandle = RegisterViewModelWithMap(InEmitter, this);

	check(SharedScriptViewModel.IsValid());
	SharedScriptViewModel->SetScripts(InEmitter);

	OnEmitterChanged().Broadcast();
}


void FNiagaraEmitterViewModel::SetSimulation(TWeakPtr<FNiagaraEmitterInstance> InSimulation)
{
	Simulation = InSimulation;
}


UNiagaraEmitter* FNiagaraEmitterViewModel::GetEmitter()
{
	return Emitter.Get();
}


FText FNiagaraEmitterViewModel::GetStatsText() const
{
	if (Simulation.IsValid())
	{
		TSharedPtr<FNiagaraEmitterInstance> SimInstance = Simulation.Pin();
		if (SimInstance.IsValid())
		{
			static const FNumberFormattingOptions FractionalFormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(3)
				.SetMaximumFractionalDigits(3);

			if (!SimInstance->IsReadyToRun() || SimInstance->GetParentSystemInstance()->GetSystem()->HasOutstandingCompilationRequests())
			{
				return LOCTEXT("PendingCompile", "Compilation in progress...");
			}

			const FNiagaraEmitterHandle& Handle = SimInstance->GetEmitterHandle();
			if (Handle.GetInstance())
			{
				if (Handle.IsValid() == false)
				{
					return LOCTEXT("InvalidHandle", "Invalid handle");
				}

				UNiagaraEmitter* HandleEmitter = Handle.GetInstance();
				if (HandleEmitter == nullptr)
				{
					return LOCTEXT("NullInstance", "Invalid instance");
				}

				if (!HandleEmitter->IsValid())
				{
					return LOCTEXT("InvalidInstance", "Invalid Emitter! May have compile errors.");
				}

				if (Handle.GetIsEnabled() == false)
				{
					return LOCTEXT("DisabledSimulation", "Simulation is not enabled.");
				}

				if (NiagaraCommands::EmitterStatsFormat->GetInt() == 1)
				{
					return FText::Format(StatsParticleCountFormat, FText::AsNumber(SimInstance->GetNumParticles()));
				}
				else
				{
					return FText::Format(StatsFormat,
						FText::AsNumber(SimInstance->GetNumParticles()),
						FText::AsNumber(SimInstance->GetTotalCPUTime(), &FractionalFormatOptions),
						FText::AsNumber(SimInstance->GetTotalBytesUsed() / Megabyte, &FractionalFormatOptions),
						ExecutionStateEnum->GetDisplayNameTextByValue((int32)SimInstance->GetExecutionState()));
				}
			}
		}
	}
	else if(!Emitter->IsReadyToRun())
	{
		return LOCTEXT("SimulationNotReady", "Preparing simulation...");
	}
	
	return LOCTEXT("InvalidSimulation", "Simulation is invalid.");
}

TSharedRef<FNiagaraScriptViewModel> FNiagaraEmitterViewModel::GetSharedScriptViewModel()
{
	return SharedScriptViewModel.ToSharedRef();
}

const UNiagaraEmitterEditorData& FNiagaraEmitterViewModel::GetEditorData() const
{
	check(Emitter.IsValid());

	const UNiagaraEmitterEditorData* EditorData = Cast<UNiagaraEmitterEditorData>(Emitter->EditorData);
	if (EditorData == nullptr)
	{
		EditorData = GetDefault<UNiagaraEmitterEditorData>();
	}
	return *EditorData;
}

UNiagaraEmitterEditorData& FNiagaraEmitterViewModel::GetOrCreateEditorData()
{
	check(Emitter.IsValid());
	UNiagaraEmitterEditorData* EditorData = Cast<UNiagaraEmitterEditorData>(Emitter->EditorData);
	if (EditorData == nullptr)
	{
		EditorData = NewObject<UNiagaraEmitterEditorData>(Emitter.Get(), NAME_None, RF_Transactional);
		Emitter->Modify();
		Emitter->EditorData = EditorData;
	}
	return *EditorData;
}

void FNiagaraEmitterViewModel::OnVMCompiled(UNiagaraEmitter* InEmitter)
{
	if (Emitter.IsValid() && InEmitter == Emitter.Get())
	{
		TArray<ENiagaraScriptCompileStatus> CompileStatuses;
		TArray<FString> CompileErrors;
		TArray<FString> CompilePaths;
		TArray<TPair<ENiagaraScriptUsage, int32> > Usages;

		ENiagaraScriptCompileStatus AggregateStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
		FString AggregateErrors;

		TArray<UNiagaraScript*> Scripts;
		Emitter->GetScripts(Scripts, true);

		int32 EventsFound = 0;
		for (int32 i = 0; i < Scripts.Num(); i++)
		{
			UNiagaraScript* Script = Scripts[i];
			if (Script != nullptr && Script->GetVMExecutableData().IsValid())
			{
				CompileStatuses.Add(Script->GetVMExecutableData().LastCompileStatus);
				CompileErrors.Add(Script->GetVMExecutableData().ErrorMsg);
				CompilePaths.Add(Script->GetPathName());

				if (Script->GetUsage() == ENiagaraScriptUsage::ParticleEventScript)
				{
					Usages.Add(TPair<ENiagaraScriptUsage, int32>(Script->GetUsage(), EventsFound));
					EventsFound++;
				}
				else
				{
					Usages.Add(TPair<ENiagaraScriptUsage, int32>(Script->GetUsage(), 0));
				}
			}
			else
			{
				CompileStatuses.Add(ENiagaraScriptCompileStatus::NCS_Unknown);
				CompileErrors.Add(TEXT("Invalid script pointer!"));
				CompilePaths.Add(TEXT("Unknown..."));
				Usages.Add(TPair<ENiagaraScriptUsage, int32>(ENiagaraScriptUsage::Function, 0));
			}

		}

		for (int32 i = 0; i < CompileStatuses.Num(); i++)
		{
			AggregateStatus = FNiagaraEditorUtilities::UnionCompileStatus(AggregateStatus, CompileStatuses[i]);
			AggregateErrors += CompilePaths[i] + TEXT(" ") + FNiagaraEditorUtilities::StatusToText(CompileStatuses[i]).ToString() + TEXT("\n");
			AggregateErrors += CompileErrors[i] + TEXT("\n");
		}
		check(SharedScriptViewModel.IsValid());
		SharedScriptViewModel->UpdateCompileStatus(AggregateStatus, AggregateErrors, CompileStatuses, CompileErrors, CompilePaths, Scripts);
	}
	OnScriptCompiled().Broadcast();
}

ENiagaraScriptCompileStatus FNiagaraEmitterViewModel::GetLatestCompileStatus()
{
	check(SharedScriptViewModel.IsValid());
	ENiagaraScriptCompileStatus UnionStatus = SharedScriptViewModel->GetLatestCompileStatus();

	if (Emitter->SimTarget == ENiagaraSimTarget::GPUComputeSim || Emitter->SimTarget == ENiagaraSimTarget::DynamicLoadBalancedSim)
	{
		if (UnionStatus != ENiagaraScriptCompileStatus::NCS_Dirty)
		{
			if (!Emitter->GetGPUComputeScript()->AreScriptAndSourceSynchronized())
			{
				UnionStatus = ENiagaraScriptCompileStatus::NCS_Dirty;
			}
		}
	}
	return UnionStatus;
}


FNiagaraEmitterViewModel::FOnEmitterChanged& FNiagaraEmitterViewModel::OnEmitterChanged()
{
	return OnEmitterChangedDelegate;
}

FNiagaraEmitterViewModel::FOnPropertyChanged& FNiagaraEmitterViewModel::OnPropertyChanged()
{
	return OnPropertyChangedDelegate;
}

FNiagaraEmitterViewModel::FOnScriptCompiled& FNiagaraEmitterViewModel::OnScriptCompiled()
{
	return OnScriptCompiledDelegate;
}

FNiagaraEmitterViewModel::FOnScriptGraphChanged& FNiagaraEmitterViewModel::OnScriptGraphChanged()
{
	return OnScriptGraphChangedDelegate;
}

FNiagaraEmitterViewModel::FOnScriptParameterStoreChanged& FNiagaraEmitterViewModel::OnScriptParameterStoreChanged()
{
	return OnScriptParameterStoreChangedDelegate;
}

void FNiagaraEmitterViewModel::AddScriptEventHandlers()
{
	if (Emitter.IsValid())
	{
		TArray<UNiagaraScript*> Scripts;
		Emitter->GetScripts(Scripts, false);
		for (UNiagaraScript* Script : Scripts)
		{
			UNiagaraScriptSource* ScriptSource = CastChecked<UNiagaraScriptSource>(Script->GetSource());
			FDelegateHandle OnGraphChangedHandle = ScriptSource->NodeGraph->AddOnGraphChangedHandler(
				FOnGraphChanged::FDelegate::CreateRaw<FNiagaraEmitterViewModel, const UNiagaraScript&>(this, &FNiagaraEmitterViewModel::ScriptGraphChanged, *Script));
			FDelegateHandle OnGraphNeedRecompileHandle = ScriptSource->NodeGraph->AddOnGraphNeedsRecompileHandler(
				FOnGraphChanged::FDelegate::CreateRaw<FNiagaraEmitterViewModel, const UNiagaraScript&>(this, &FNiagaraEmitterViewModel::ScriptGraphChanged, *Script));

			ScriptToOnGraphChangedHandleMap.Add(FObjectKey(Script), OnGraphChangedHandle);
			ScriptToRecompileHandleMap.Add(FObjectKey(Script), OnGraphNeedRecompileHandle);

			FDelegateHandle OnParameterStoreChangedHandle = Script->RapidIterationParameters.AddOnChangedHandler(
				FNiagaraParameterStore::FOnChanged::FDelegate::CreateRaw<FNiagaraEmitterViewModel, const FNiagaraParameterStore&, const UNiagaraScript&>(
					this, &FNiagaraEmitterViewModel::ScriptParameterStoreChanged, Script->RapidIterationParameters, *Script));
			ScriptToOnParameterStoreChangedHandleMap.Add(FObjectKey(Script), OnParameterStoreChangedHandle);
		}
	}
}

void FNiagaraEmitterViewModel::RemoveScriptEventHandlers()
{
	if (Emitter.IsValid())
	{
		TArray<UNiagaraScript*> Scripts;
		Emitter->GetScripts(Scripts, false);
		for (UNiagaraScript* Script : Scripts)
		{
			FDelegateHandle* OnGraphChangedHandle = ScriptToOnGraphChangedHandleMap.Find(FObjectKey(Script));
			if (OnGraphChangedHandle != nullptr)
			{
				UNiagaraScriptSource* ScriptSource = CastChecked<UNiagaraScriptSource>(Script->GetSource());
				ScriptSource->NodeGraph->RemoveOnGraphChangedHandler(*OnGraphChangedHandle);
			}

			FDelegateHandle* OnGraphRecompileHandle = ScriptToRecompileHandleMap.Find(FObjectKey(Script));
			if (OnGraphRecompileHandle != nullptr)
			{
				UNiagaraScriptSource* ScriptSource = CastChecked<UNiagaraScriptSource>(Script->GetSource());
				ScriptSource->NodeGraph->RemoveOnGraphNeedsRecompileHandler(*OnGraphRecompileHandle);
			}

			FDelegateHandle* OnParameterStoreChangedHandle = ScriptToOnParameterStoreChangedHandleMap.Find(FObjectKey(Script));
			if (OnParameterStoreChangedHandle != nullptr)
			{
				Script->RapidIterationParameters.RemoveOnChangedHandler(*OnParameterStoreChangedHandle);
			}
		}
	}

	ScriptToOnGraphChangedHandleMap.Empty();
	ScriptToRecompileHandleMap.Empty();
	ScriptToOnParameterStoreChangedHandleMap.Empty();
}

void FNiagaraEmitterViewModel::ScriptGraphChanged(const FEdGraphEditAction& InAction, const UNiagaraScript& OwningScript)
{
	OnScriptGraphChangedDelegate.Broadcast(InAction, OwningScript);
}

void FNiagaraEmitterViewModel::ScriptParameterStoreChanged(const FNiagaraParameterStore& ChangedParameterStore, const UNiagaraScript& OwningScript)
{
	OnScriptParameterStoreChangedDelegate.Broadcast(ChangedParameterStore, OwningScript);
}

void FNiagaraEmitterViewModel::OnEmitterPropertiesChanged()
{
	// When the properties change we reset the scripts on the script view model because gpu/cpu or interpolation may have changed.
	SharedScriptViewModel->SetScripts(Emitter.Get());
	OnPropertyChangedDelegate.Broadcast();
}

#undef LOCTEXT_NAMESPACE

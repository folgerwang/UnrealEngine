// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/NiagaraSystemViewModel.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraScriptSource.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraSequence.h"
#include "MovieSceneNiagaraEmitterTrack.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeInput.h"
#include "NiagaraEditorModule.h"
#include "NiagaraNodeFunctionCall.h"

#include "Editor.h"

#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "ISequencerModule.h"
#include "EditorSupportDelegates.h"
#include "Modules/ModuleManager.h"
#include "ViewModels/TNiagaraViewModelManager.h"
#include "Math/NumericLimits.h"
#include "NiagaraComponent.h"
#include "MovieSceneFolder.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Framework/Commands/UICommandList.h"

DECLARE_CYCLE_STAT(TEXT("Niagara - SystemViewModel - CompileSystem"), STAT_NiagaraEditor_SystemViewModel_CompileSystem, STATGROUP_NiagaraEditor);

#define LOCTEXT_NAMESPACE "NiagaraSystemViewModel"

template<> TMap<UNiagaraSystem*, TArray<FNiagaraSystemViewModel*>> TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::ObjectsToViewModels{};

FNiagaraSystemViewModelOptions::FNiagaraSystemViewModelOptions()
	: bCanAutoCompile(true)
	, bCanSimulate(true)
{
}

FNiagaraSystemViewModel::FNiagaraSystemViewModel(UNiagaraSystem& InSystem, FNiagaraSystemViewModelOptions InOptions)
	: System(InSystem)
	, PreviewComponent(nullptr)
	, SystemInstance(nullptr)
	, SystemScriptViewModel(MakeShareable(new FNiagaraSystemScriptViewModel(System, this)))
	, NiagaraSequence(nullptr)
	, bSettingSequencerTimeDirectly(false)
	, bCanModifyEmittersFromTimeline(InOptions.bCanModifyEmittersFromTimeline)
	, bCanAutoCompile(InOptions.bCanAutoCompile)
	, bForceAutoCompileOnce(false)
	, bCanSimulate(InOptions.bCanSimulate)
	, EditMode(InOptions.EditMode)
	, OnGetSequencerAddMenuContent(InOptions.OnGetSequencerAddMenuContent)
	, bUpdatingEmittersFromSequencerDataChange(false)
	, bUpdatingSequencerFromEmitterDataChange(false)
	, bUpdatingSystemSelectionFromSequencer(false)
	, bUpdatingSequencerSelectionFromSystem(false)
	, EditorSettings(GetMutableDefault<UNiagaraEditorSettings>())
	, bResetRequestPending(false)
	, bCompilePendingCompletion(false)
{
	SetupPreviewComponentAndInstance();
	SetupSequencer();
	RefreshAll();
	GEditor->RegisterForUndo(this);
	RegisteredHandle = RegisterViewModelWithMap(&InSystem, this);
	AddSystemEventHandlers();
}

void FNiagaraSystemViewModel::DumpToText(FString& ExportText)
{
	TSet<UObject*> ExportObjs;
	ExportObjs.Add(&System);
	FEdGraphUtilities::ExportNodesToText(ExportObjs, ExportText);
}

void FNiagaraSystemViewModel::Cleanup()
{
	UE_LOG(LogNiagaraEditor, Warning, TEXT("Cleanup System view model %p"), this);

	if (PreviewComponent)
	{
		PreviewComponent->OnSystemInstanceChanged().RemoveAll(this);
	}

	if (SystemInstance)
	{
		SystemInstance->OnInitialized().RemoveAll(this);
		SystemInstance->OnReset().RemoveAll(this);
	}

	CurveOwner.EmptyCurves();

	GEditor->UnregisterForUndo(this);

	// Make sure that we clear out all of our event handlers
	UnregisterViewModelWithMap(RegisteredHandle);

	for (TSharedRef<FNiagaraEmitterHandleViewModel>& HandleRef : EmitterHandleViewModels)
	{
		HandleRef->OnPropertyChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnPropertyChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
		HandleRef->Cleanup();
	}
	EmitterHandleViewModels.Empty();

	if (Sequencer.IsValid())
	{
		Sequencer->OnMovieSceneDataChanged().RemoveAll(this);
		Sequencer->OnGlobalTimeChanged().RemoveAll(this);
		Sequencer->GetSelectionChangedTracks().RemoveAll(this);
		Sequencer->GetSelectionChangedSections().RemoveAll(this);
		Sequencer.Reset();
	}

	PreviewComponent = nullptr;
	RemoveSystemEventHandlers();
	SystemScriptViewModel.Reset();
}

FNiagaraSystemViewModel::~FNiagaraSystemViewModel()
{
	Cleanup();

	UE_LOG(LogNiagaraEditor, Warning, TEXT("Deleting System view model %p"), this);
}

const TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& FNiagaraSystemViewModel::GetEmitterHandleViewModels()
{
	return EmitterHandleViewModels;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> FNiagaraSystemViewModel::GetEmitterHandleViewModelById(FGuid InEmitterHandleId)
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (EmitterHandleViewModel->GetId() == InEmitterHandleId)
		{
			return EmitterHandleViewModel;
		}
	}
	return TSharedPtr<FNiagaraEmitterHandleViewModel>();
}

TSharedPtr<FNiagaraSystemScriptViewModel> FNiagaraSystemViewModel::GetSystemScriptViewModel()
{
	return SystemScriptViewModel;
}

void FNiagaraSystemViewModel::CompileSystem(bool bForce)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_SystemViewModel_CompileSystem);
	KillSystemInstances();
	check(SystemScriptViewModel.IsValid());
	SystemScriptViewModel->CompileSystem(bForce);
	bCompilePendingCompletion = true;
}

ENiagaraScriptCompileStatus FNiagaraSystemViewModel::GetLatestCompileStatus()
{
	check(SystemScriptViewModel.IsValid());
	return SystemScriptViewModel->GetLatestCompileStatus();
}

const TArray<FGuid>& FNiagaraSystemViewModel::GetSelectedEmitterHandleIds()
{
	return SelectedEmitterHandleIds;
}

void FNiagaraSystemViewModel::SetSelectedEmitterHandlesById(TArray<FGuid> InSelectedEmitterHandleIds)
{
	bool bSelectionChanged = false;
	if (SelectedEmitterHandleIds.Num() == InSelectedEmitterHandleIds.Num())
	{
		for (FGuid InSelectedEmitterHandleId : InSelectedEmitterHandleIds)
		{
			if (SelectedEmitterHandleIds.Contains(InSelectedEmitterHandleId) == false)
			{
				bSelectionChanged = true;
				break;
			}
		}
	}
	else
	{
		bSelectionChanged = true;
	}

	SelectedEmitterHandleIds.Empty();
	SelectedEmitterHandleIds.Append(InSelectedEmitterHandleIds);
	if (bSelectionChanged)
	{
		if (bUpdatingSystemSelectionFromSequencer == false)
		{
			UpdateSequencerFromEmitterHandleSelection();
		}
		OnSelectedEmitterHandlesChangedDelegate.Broadcast();
	}
}

void FNiagaraSystemViewModel::SetSelectedEmitterHandleById(FGuid InSelectedEmitterHandleId)
{
	TArray<FGuid> SingleSelectedEmitterHandleId;
	SingleSelectedEmitterHandleId.Add(InSelectedEmitterHandleId);
	SetSelectedEmitterHandlesById(SingleSelectedEmitterHandleId);
}

void FNiagaraSystemViewModel::GetSelectedEmitterHandles(TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& OutSelectedEmitterHanldles)
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (SelectedEmitterHandleIds.Contains(EmitterHandleViewModel->GetId()))
		{
			OutSelectedEmitterHanldles.Add(EmitterHandleViewModel);
		}
	}
}

const UNiagaraSystemEditorData& FNiagaraSystemViewModel::GetEditorData() const
{
	const UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System.GetEditorData());
	if (EditorData == nullptr)
	{
		EditorData = GetDefault<UNiagaraSystemEditorData>();
	}
	return *EditorData;
}

UNiagaraSystemEditorData& FNiagaraSystemViewModel::GetOrCreateEditorData()
{
	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System.GetEditorData());
	if (EditorData == nullptr)
	{
		EditorData = NewObject<UNiagaraSystemEditorData>(&System, NAME_None, RF_Transactional);
		System.Modify();
		System.SetEditorData(EditorData);
	}
	return *EditorData;
}

UNiagaraComponent* FNiagaraSystemViewModel::GetPreviewComponent()
{
	return PreviewComponent;
}

TSharedPtr<ISequencer> FNiagaraSystemViewModel::GetSequencer()
{
	return Sequencer;
}

FNiagaraCurveOwner& FNiagaraSystemViewModel::GetCurveOwner()
{
	return CurveOwner;
}

bool FNiagaraSystemViewModel::GetCanModifyEmittersFromTimeline() const
{
	return bCanModifyEmittersFromTimeline;
}

/** Gets the current editing mode for this system view model. */
ENiagaraSystemViewModelEditMode FNiagaraSystemViewModel::GetEditMode() const
{
	return EditMode;
}

void FNiagaraSystemViewModel::AddEmitterFromAssetData(const FAssetData& AssetData)
{
	UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(AssetData.GetAsset());
	if (Emitter != nullptr)
	{
		AddEmitter(*Emitter);
	}
}

void FNiagaraSystemViewModel::AddEmitter(UNiagaraEmitter& Emitter)
{
	// Kill all system instances before modifying the emitter handle list to prevent accessing deleted data.
	KillSystemInstances();

	// When editing an emitter asset the system is a placeholder and we don't want to make adding an emitter to it
	// undoable.
	if (EditMode != ENiagaraSystemViewModelEditMode::EmitterAsset)
	{
		GEditor->BeginTransaction(LOCTEXT("AddEmitter", "Add emitter"));
	}

	TSet<FName> EmitterHandleNames;
	for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
	{
		EmitterHandleNames.Add(EmitterHandle.GetName());
	}

	FNiagaraEmitterHandle EmitterHandle;
	if (EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		System.Modify();
		EmitterHandle = System.AddEmitterHandle(Emitter, FNiagaraUtilities::GetUniqueName(Emitter.GetFName(), EmitterHandleNames));
	}
	else
	{
		EmitterHandle = System.AddEmitterHandleWithoutCopying(Emitter);
	}

	check(SystemScriptViewModel.IsValid());
	SystemScriptViewModel->RebuildEmitterNodes();

	if (EditMode == ENiagaraSystemViewModelEditMode::SystemAsset)
	{
		GEditor->EndTransaction();
	}

	if (System.GetNumEmitters() == 1 && EditorSettings->GetAutoPlay())
	{
		// When adding a new emitter to an empty system start playing.
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
	}
		
	RefreshAll();

	TRange<float> SystemPlaybackRange = GetEditorData().GetPlaybackRange();
	TRange<float> EmitterPlaybackRange = GetEmitterHandleViewModelById(EmitterHandle.GetId())->GetEmitterViewModel()->GetEditorData().GetPlaybackRange();
	TRange<float> NewSystemPlaybackRange = TRange<float>(
		FMath::Min(SystemPlaybackRange.GetLowerBoundValue(), EmitterPlaybackRange.GetLowerBoundValue()),
		FMath::Max(SystemPlaybackRange.GetUpperBoundValue(), EmitterPlaybackRange.GetUpperBoundValue()));

	GetOrCreateEditorData().Modify();
	GetOrCreateEditorData().SetPlaybackRange(NewSystemPlaybackRange);

	TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);

	FFrameTime NewStartFrame = NewSystemPlaybackRange.GetLowerBoundValue() * NiagaraSequence->GetMovieScene()->GetTickResolution();
	int32 NewDuration = (NewSystemPlaybackRange.Size<float>() * NiagaraSequence->GetMovieScene()->GetTickResolution()).FrameNumber.Value;

	NiagaraSequence->GetMovieScene()->SetPlaybackRange(NewStartFrame.RoundToFrame(), NewDuration);
		
	SetSelectedEmitterHandleById(EmitterHandle.GetId());

	bForceAutoCompileOnce = true;
}

void FNiagaraSystemViewModel::DuplicateEmitter(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToDuplicate)
{
	if (EmitterHandleToDuplicate->GetEmitterHandle() == nullptr)
	{
		return;
	}
	TSet<FGuid> HandlesToDuplicate;
	HandlesToDuplicate.Add(EmitterHandleToDuplicate->GetEmitterHandle()->GetId());
	DuplicateEmitters(HandlesToDuplicate);
	bForceAutoCompileOnce = true;
}

void FNiagaraSystemViewModel::DuplicateEmitters(TSet<FGuid> EmitterHandleIdsToDuplicate)
{
	if (EmitterHandleIdsToDuplicate.Num() <= 0)
	{
		return;
	}
	// Kill all system instances before modifying the emitter handle list to prevent accessing deleted data.
	KillSystemInstances();
	const FScopedTransaction DeleteTransaction(EmitterHandleIdsToDuplicate.Num() == 1
		? LOCTEXT("DuplicateEmitter", "Duplicate emitter")
		: LOCTEXT("DuplicateEmitters", "Duplicate emitters"));


	TSet<FName> EmitterHandleNames;
	for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
	{
		EmitterHandleNames.Add(EmitterHandle.GetName());
	}

	System.Modify();
	for (FGuid OriginalEmitterHandleId : EmitterHandleIdsToDuplicate)
	{
		FNiagaraEmitterHandle OriginalEmitterHandle;
		for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
		{
			if (EmitterHandle.GetId() == OriginalEmitterHandleId)
			{
				OriginalEmitterHandle = EmitterHandle;
				break;
			}
		}
		if (OriginalEmitterHandle.IsValid())
		{
			FNiagaraEmitterHandle EmitterHandle = System.DuplicateEmitterHandle(OriginalEmitterHandle, FNiagaraUtilities::GetUniqueName(OriginalEmitterHandle.GetName(), EmitterHandleNames));
		}
	}

	check(SystemScriptViewModel.IsValid());
	SystemScriptViewModel->RebuildEmitterNodes();
	RefreshAll();
	bForceAutoCompileOnce = true;
}

void FNiagaraSystemViewModel::DeleteEmitter(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToDelete)
{
	TSet<FGuid> IdsToDelete;
	IdsToDelete.Add(EmitterHandleToDelete->GetId());
	DeleteEmitters(IdsToDelete);
	bForceAutoCompileOnce = true;
}

void FNiagaraSystemViewModel::DeleteEmitters(TSet<FGuid> EmitterHandleIdsToDelete)
{
	if (EmitterHandleIdsToDelete.Num() > 0)
	{
		// Kill all system instances before modifying the emitter handle list to prevent accessing deleted data.
		KillSystemInstances();

		const FScopedTransaction DeleteTransaction(EmitterHandleIdsToDelete.Num() == 1 
			? LOCTEXT("DeleteEmitter", "Delete emitter")
			: LOCTEXT("DeleteEmitters", "Delete emitters"));

		System.Modify();
		System.RemoveEmitterHandlesById(EmitterHandleIdsToDelete);

		check(SystemScriptViewModel.IsValid());
		SystemScriptViewModel->RebuildEmitterNodes();

		RefreshAll();
		bForceAutoCompileOnce = true;
	}
}

TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> FNiagaraSystemViewModel::GetPinnedEmitterHandles()
{
	return PinnedEmitterHandles;
}

void FNiagaraSystemViewModel::SetEmitterPinnedState(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel, bool bPinnedState)
{
	if (bPinnedState)
	{
		PinnedEmitterHandles.AddUnique(EmitterHandleModel);
	}
	else
	{
		PinnedEmitterHandles.Remove(EmitterHandleModel);
	}
	OnPinnedChangedDelegate.Broadcast();
}

FNiagaraSystemViewModel::FOnPinnedEmittersChanged& FNiagaraSystemViewModel::GetOnPinnedEmittersChanged()
{
	return OnPinnedChangedDelegate;
}

bool FNiagaraSystemViewModel::GetIsEmitterPinned(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel)
{
	return PinnedEmitterHandles.ContainsByPredicate([=](TSharedRef<FNiagaraEmitterHandleViewModel> Model) {return Model==EmitterHandleModel;});
}


FNiagaraSystemViewModel::FOnEmitterHandleViewModelsChanged& FNiagaraSystemViewModel::OnEmitterHandleViewModelsChanged()
{
	return OnEmitterHandleViewModelsChangedDelegate;
}

FNiagaraSystemViewModel::FOnCurveOwnerChanged& FNiagaraSystemViewModel::OnCurveOwnerChanged()
{
	return OnCurveOwnerChangedDelegate;
}

FNiagaraSystemViewModel::FOnSelectedEmitterHandlesChanged& FNiagaraSystemViewModel::OnSelectedEmitterHandlesChanged()
{
	return OnSelectedEmitterHandlesChangedDelegate;
}

FNiagaraSystemViewModel::FOnPostSequencerTimeChange& FNiagaraSystemViewModel::OnPostSequencerTimeChanged()
{
	return OnPostSequencerTimeChangeDelegate;
}

FNiagaraSystemViewModel::FOnSystemCompiled& FNiagaraSystemViewModel::OnSystemCompiled()
{
	return OnSystemCompiledDelegate;
}

void FNiagaraSystemViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (PreviewComponent != nullptr)
	{
		Collector.AddReferencedObject(PreviewComponent);
	}
	if (NiagaraSequence != nullptr)
	{
		Collector.AddReferencedObject(NiagaraSequence);
	}
}

void FNiagaraSystemViewModel::PostUndo(bool bSuccess)
{
	RefreshAll();
}

void FNiagaraSystemViewModel::Tick(float DeltaTime)
{
	if (bForceAutoCompileOnce || (GetDefault<UNiagaraEditorSettings>()->GetAutoCompile() && bCanAutoCompile))
	{
		bool bRecompile = false;

		check(SystemScriptViewModel.IsValid());
		if (SystemScriptViewModel->GetLatestCompileStatus() == ENiagaraScriptCompileStatus::NCS_Dirty)
		{
			//SystemScriptViewModel->CompileSystem();
			//UE_LOG(LogNiagaraEditor, Log, TEXT("Compiling %s due to dirty scripts."), *System.GetName());
			bRecompile |= true;
		}

		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
		{
			if (EmitterHandleViewModel->GetEmitterViewModel()->GetLatestCompileStatus() == ENiagaraScriptCompileStatus::NCS_Dirty)
			{
				bRecompile |= true;
				//EmitterHandleViewModel->GetEmitterViewModel()->CompileScripts();
				//UE_LOG(LogNiagaraEditor, Log, TEXT("Compiling %s - %s due to dirty scripts."), *System.GetName(), *EmitterHandleViewModel->GetName().ToString());
			}
		}

		if (System.HasOutstandingCompilationRequests() == false)
		{
			if (bCompilePendingCompletion)
			{
				bCompilePendingCompletion = false;
				OnSystemCompiled().Broadcast();
			}

			if (bRecompile || bForceAutoCompileOnce)
			{
				CompileSystem(false);
				bForceAutoCompileOnce = false;
			}

			if (bResetRequestPending)
			{
				ResetSystem();
			}
		}
	}

	if (EmitterIdsRequiringSequencerTrackUpdate.Num() > 0)
	{
		UpdateSequencerTracksForEmitters(EmitterIdsRequiringSequencerTrackUpdate);
		EmitterIdsRequiringSequencerTrackUpdate.Empty();
	}
}

void FNiagaraSystemViewModel::OnPreSave()
{
	if (System.HasOutstandingCompilationRequests())
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("System %s has pending compile jobs. Waiting for that code to complete before Saving.."), *System.GetName());
		System.WaitForCompilationComplete();
	}
}

void FNiagaraSystemViewModel::OnPreClose()
{
	if (System.HasOutstandingCompilationRequests())
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("System %s has pending compile jobs. Waiting for that code to complete before Closing.."), *System.GetName());
		System.WaitForCompilationComplete();
	}
}

TSharedPtr<FUICommandList> FNiagaraSystemViewModel::GetToolkitCommands()
{
	return ToolkitCommands.Pin();
}

FNiagaraSystemViewModel::FOnPinnedCurvesChanged& FNiagaraSystemViewModel::GetOnPinnedCurvesChanged()
{
	return OnPinnedCurvesChangedDelegate;
}

void FNiagaraSystemViewModel::SetToolkitCommands(const TSharedRef<FUICommandList>& InToolkitCommands)
{
	ToolkitCommands = InToolkitCommands;
}

const TArray<FNiagaraStackModuleData>& FNiagaraSystemViewModel::GetStackModuleDataForEmitter(TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel)
{
	FGuid EmitterHandleId;
	TSharedRef<FNiagaraEmitterHandleViewModel>* FoundModel = EmitterHandleViewModels.FindByPredicate([EmitterViewModel](TSharedRef<FNiagaraEmitterHandleViewModel> CurrentViewModel)
	{ return CurrentViewModel->GetEmitterViewModel() == EmitterViewModel; });
	checkf(FoundModel != nullptr, TEXT("Couldn't get stack module data for emitter"));
	if (FoundModel)
	{
		EmitterHandleId = (*FoundModel)->GetEmitterHandle()->GetId();
		if (!EmitterToCachedStackModuleData.Contains(EmitterHandleId))
		{
			// If not cached, rebuild
			UNiagaraEmitter* Emitter = (*FoundModel)->GetEmitterViewModel()->GetEmitter();
			TArray<FNiagaraStackModuleData> StackModuleData;
			BuildStackModuleData(GetSystem().GetSystemSpawnScript(), EmitterHandleId, StackModuleData);
			BuildStackModuleData(GetSystem().GetSystemUpdateScript(), EmitterHandleId, StackModuleData);
			BuildStackModuleData(Emitter->EmitterSpawnScriptProps.Script, EmitterHandleId, StackModuleData);
			BuildStackModuleData(Emitter->EmitterUpdateScriptProps.Script, EmitterHandleId, StackModuleData);
			BuildStackModuleData(Emitter->SpawnScriptProps.Script, EmitterHandleId, StackModuleData);
			BuildStackModuleData(Emitter->UpdateScriptProps.Script, EmitterHandleId, StackModuleData);
			EmitterToCachedStackModuleData.Add(EmitterHandleId) = StackModuleData;
		}
	}
	return EmitterToCachedStackModuleData[EmitterHandleId];
}

TStatId FNiagaraSystemViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraSystemViewModel, STATGROUP_Tickables);
}

void FNiagaraSystemViewModel::SetupPreviewComponentAndInstance()
{
	if (bCanSimulate)
	{
		PreviewComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		PreviewComponent->CastShadow = 1;
		PreviewComponent->bCastDynamicShadow = 1;
		PreviewComponent->SetAsset(&System);
		PreviewComponent->SetForceSolo(true);
		PreviewComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
		PreviewComponent->SetCanRenderWhileSeeking(false);
		PreviewComponent->Activate(true);

		UNiagaraSystemEditorData& EditorData = GetOrCreateEditorData();
		FTransform OwnerTransform = EditorData.GetOwnerTransform();
		PreviewComponent->SetRelativeTransform(OwnerTransform);

		PreviewComponent->OnSystemInstanceChanged().AddRaw(this, &FNiagaraSystemViewModel::PreviewComponentSystemInstanceChanged);
		PreviewComponentSystemInstanceChanged();
	}
}

void FNiagaraSystemViewModel::RefreshAll()
{
	ReInitializeSystemInstances();
	RefreshEmitterHandleViewModels();
	RefreshSequencerTracks();
	ResetCurveData();
}

void FNiagaraSystemViewModel::NotifyDataObjectChanged(UObject* ChangedObject)
{
	UNiagaraDataInterface* ChangedDataInterface = Cast<UNiagaraDataInterface>(ChangedObject);
	if (ChangedDataInterface)
	{
		UpdateCompiledDataInterfaces(ChangedDataInterface);
	}

	UNiagaraDataInterfaceCurveBase* ChangedDataInterfaceCurve = Cast<UNiagaraDataInterfaceCurveBase>(ChangedDataInterface);
	if (ChangedDataInterfaceCurve || ChangedObject == nullptr)
	{
		TArray<UNiagaraDataInterfaceCurveBase*> OldShownCurveDataInterfaces = ShownCurveDataInterfaces;
		ResetCurveData();
		if(ChangedDataInterfaceCurve != nullptr && ChangedDataInterfaceCurve->ShowInCurveEditor && OldShownCurveDataInterfaces.Contains(ChangedDataInterfaceCurve) == false)
		{
			NotifyPinnedCurvesChanged();
		}
	}

	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::IsolateEmitters(TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmitterHandlesToIsolate)
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		EmitterHandle->GetEmitterHandle()->SetIsolated(false);
	}

	bool bAnyEmitterIsolated = false;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToIsolate : EmitterHandlesToIsolate)
	{
		bAnyEmitterIsolated = true;
		EmitterHandleToIsolate->GetEmitterHandle()->SetIsolated(true);
	}

	System.SetIsolateEnabled(bAnyEmitterIsolated);
}

void FNiagaraSystemViewModel::ToggleEmitterIsolation(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandle)
{
	InEmitterHandle->GetEmitterHandle()->SetIsolated(!InEmitterHandle->GetEmitterHandle()->IsIsolated());
	
	bool bAnyEmitterIsolated = false;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandle : EmitterHandleViewModels)
	{
		if (EmitterHandle->GetEmitterHandle()->IsIsolated())
		{
			bAnyEmitterIsolated = true;
			break;
		}
	}

	System.SetIsolateEnabled(bAnyEmitterIsolated);
}

bool FNiagaraSystemViewModel::IsEmitterIsolated(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandle)
{
	return InEmitterHandle->GetEmitterHandle()->IsIsolated();
}

void FNiagaraSystemViewModel::RefreshEmitterHandleViewModels()
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> OldViewModels = EmitterHandleViewModels;
	EmitterHandleViewModels.Empty();
	EmitterToCachedStackModuleData.Empty();

	// Map existing view models to the real instances that now exist. Reuse if we can. Create a new one if we cannot.
	TArray<FGuid> ValidEmitterHandleIds;
	int32 i;
	for (i = 0; i < System.GetNumEmitters(); ++i)
	{
		FNiagaraEmitterHandle* EmitterHandle = &System.GetEmitterHandle(i);
		TSharedPtr<FNiagaraEmitterInstance> Simulation = SystemInstance ? SystemInstance->GetSimulationForHandle(*EmitterHandle) : nullptr;
		ValidEmitterHandleIds.Add(EmitterHandle->GetId());

		bool bAdd = OldViewModels.Num() <= i;
		if (bAdd)
		{
			TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = MakeShareable(new FNiagaraEmitterHandleViewModel(EmitterHandle, Simulation, System));
			// Since we're adding fresh, we need to register all the event handlers.
			ViewModel->OnPropertyChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterHandlePropertyChanged, ViewModel);
			ViewModel->OnNameChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterHandleNameChanged, ViewModel);
			ViewModel->GetEmitterViewModel()->OnPropertyChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterPropertyChanged, ViewModel);
			ViewModel->GetEmitterViewModel()->OnScriptCompiled().AddRaw(this, &FNiagaraSystemViewModel::ScriptCompiled);
			ViewModel->GetEmitterViewModel()->OnScriptGraphChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterScriptGraphChanged, ViewModel);
			ViewModel->GetEmitterViewModel()->OnScriptParameterStoreChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterParameterStoreChanged, ViewModel);
			EmitterHandleViewModels.Add(ViewModel);
		}
		else
		{
			TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = OldViewModels[i];
			ViewModel->Set(EmitterHandle, Simulation, System);
			EmitterHandleViewModels.Add(ViewModel);
		}

	}

	check(EmitterHandleViewModels.Num() == System.GetNumEmitters());

	// Clear out any old view models that may still be left around.
	for (; i < OldViewModels.Num(); i++)
	{
		TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = OldViewModels[i];
		ViewModel->OnPropertyChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnPropertyChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptGraphChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptParameterStoreChanged().RemoveAll(this);
		ViewModel->Set(nullptr, nullptr, System);
	}

	// Remove any invalid ids from the handle selection.
	auto EmitterHandleIdIsInvalid = [&](FGuid& EmitterHandleId) { return ValidEmitterHandleIds.Contains(EmitterHandleId) == false; };
	int32 NumRemoved = SelectedEmitterHandleIds.RemoveAll(EmitterHandleIdIsInvalid);

	OnEmitterHandleViewModelsChangedDelegate.Broadcast();
	if (NumRemoved > 0)
	{
		OnSelectedEmitterHandlesChangedDelegate.Broadcast();
	}
}

void PopulateChildMovieSceneFoldersFromNiagaraFolders(const UNiagaraSystemEditorFolder* NiagaraFolder, UMovieSceneFolder* MovieSceneFolder, const TMap<FGuid, UMovieSceneNiagaraEmitterTrack*>& EmitterHandleIdToTrackMap)
{
	for (const UNiagaraSystemEditorFolder* ChildNiagaraFolder : NiagaraFolder->GetChildFolders())
	{
		UMovieSceneFolder* MatchingMovieSceneFolder = nullptr;
		for (UMovieSceneFolder* ChildMovieSceneFolder : MovieSceneFolder->GetChildFolders())
		{
			if (ChildMovieSceneFolder->GetFolderName() == ChildNiagaraFolder->GetFolderName())
			{
				MatchingMovieSceneFolder = ChildMovieSceneFolder;
			}
		}

		if (MatchingMovieSceneFolder == nullptr)
		{
			MatchingMovieSceneFolder = NewObject<UMovieSceneFolder>(MovieSceneFolder, ChildNiagaraFolder->GetFolderName(), RF_Transactional);
			MatchingMovieSceneFolder->SetFolderName(ChildNiagaraFolder->GetFolderName());
			MovieSceneFolder->AddChildFolder(MatchingMovieSceneFolder);
		}

		PopulateChildMovieSceneFoldersFromNiagaraFolders(ChildNiagaraFolder, MatchingMovieSceneFolder, EmitterHandleIdToTrackMap);
	}

	for (const FGuid& ChildEmitterHandleId : NiagaraFolder->GetChildEmitterHandleIds())
	{
		UMovieSceneNiagaraEmitterTrack* const* TrackPtr = EmitterHandleIdToTrackMap.Find(ChildEmitterHandleId);
		if (TrackPtr != nullptr && MovieSceneFolder->GetChildMasterTracks().Contains(*TrackPtr) == false)
		{
			MovieSceneFolder->AddChildMasterTrack(*TrackPtr);
		}
	}
}

void FNiagaraSystemViewModel::RefreshSequencerTracks()
{
	TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);

	TArray<UMovieSceneTrack*> MasterTracks = NiagaraSequence->GetMovieScene()->GetMasterTracks();
	for (UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (MasterTrack != nullptr)
		{
			NiagaraSequence->GetMovieScene()->RemoveMasterTrack(*MasterTrack);
		}
	}

	TMap<FGuid, UMovieSceneNiagaraEmitterTrack*> EmitterHandleIdToTrackMap;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		UMovieSceneNiagaraEmitterTrack* EmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(NiagaraSequence->GetMovieScene()->AddMasterTrack(UMovieSceneNiagaraEmitterTrack::StaticClass()));
		EmitterTrack->Initialize(*this, EmitterHandleViewModel, NiagaraSequence->GetMovieScene()->GetTickResolution());
		EmitterHandleIdToTrackMap.Add(EmitterHandleViewModel->GetId(), EmitterTrack);
	}

	TArray<UMovieSceneFolder*>& MovieSceneRootFolders = NiagaraSequence->GetMovieScene()->GetRootFolders();
	MovieSceneRootFolders.Empty();

	const UNiagaraSystemEditorData& SystemEditorData = GetEditorData();
	UNiagaraSystemEditorFolder& RootFolder = SystemEditorData.GetRootFolder();
	for (const UNiagaraSystemEditorFolder* RootChildFolder : RootFolder.GetChildFolders())
	{
		UMovieSceneFolder* MovieSceneRootFolder = NewObject<UMovieSceneFolder>(NiagaraSequence->GetMovieScene(), RootChildFolder->GetFolderName(), RF_Transactional);
		MovieSceneRootFolder->SetFolderName(RootChildFolder->GetFolderName());
		MovieSceneRootFolders.Add(MovieSceneRootFolder);
		PopulateChildMovieSceneFoldersFromNiagaraFolders(RootChildFolder, MovieSceneRootFolder, EmitterHandleIdToTrackMap);
	}

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

	// Since we just rebuilt all of the sequencer tracks, these updates don't need to be done.
	EmitterIdsRequiringSequencerTrackUpdate.Empty();
}

void FNiagaraSystemViewModel::UpdateSequencerTracksForEmitters(const TArray<FGuid>& EmitterIdsRequiringUpdate)
{
	TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);
	for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
	{
		UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
		if (EmitterIdsRequiringUpdate.Contains(EmitterTrack->GetEmitterHandleViewModel()->GetId()))
		{
			EmitterTrack->UpdateTrackFromEmitterGraphChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
		}
	}
	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

UMovieSceneNiagaraEmitterTrack* FNiagaraSystemViewModel::GetTrackForHandleViewModel(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
	{
		UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
		if (EmitterTrack->GetEmitterHandleViewModel() == EmitterHandleViewModel)
		{
			return EmitterTrack;
		}
	}
	return nullptr;
}

void FNiagaraSystemViewModel::SetupSequencer()
{
	NiagaraSequence = NewObject<UNiagaraSequence>(GetTransientPackage());
	UMovieScene* MovieScene = NewObject<UMovieScene>(NiagaraSequence, FName("Niagara System MovieScene"), RF_Transactional);
	MovieScene->SetDisplayRate(FFrameRate(240, 1));

	NiagaraSequence->Initialize(this, MovieScene);

	FFrameTime StartTime = GetEditorData().GetPlaybackRange().GetLowerBoundValue() * MovieScene->GetTickResolution();
	int32      Duration  = (GetEditorData().GetPlaybackRange().Size<float>() * MovieScene->GetTickResolution()).FrameNumber.Value;

	MovieScene->SetPlaybackRange(StartTime.RoundToFrame(), Duration);

	FMovieSceneEditorData& EditorData = NiagaraSequence->GetMovieScene()->GetEditorData();
	float ViewTimeOffset = .1f;
	EditorData.WorkStart = GetEditorData().GetPlaybackRange().GetLowerBoundValue() - ViewTimeOffset;
	EditorData.WorkEnd = GetEditorData().GetPlaybackRange().GetUpperBoundValue() + ViewTimeOffset;
	EditorData.ViewStart = EditorData.WorkStart;
	EditorData.ViewEnd = EditorData.WorkEnd;

	FSequencerViewParams ViewParams(TEXT("NiagaraSequencerSettings"));
	{
		ViewParams.UniqueName = "NiagaraSequenceEditor";
		ViewParams.OnGetAddMenuContent = OnGetSequencerAddMenuContent;
	}

	FSequencerInitParams SequencerInitParams;
	{
		SequencerInitParams.ViewParams = ViewParams;
		SequencerInitParams.RootSequence = NiagaraSequence;
		SequencerInitParams.bEditWithinLevelEditor = false;
		SequencerInitParams.ToolkitHost = nullptr;
	}

	ISequencerModule &SequencerModule = FModuleManager::LoadModuleChecked< ISequencerModule >("Sequencer");
	Sequencer = SequencerModule.CreateSequencer(SequencerInitParams);

	Sequencer->OnMovieSceneDataChanged().AddRaw(this, &FNiagaraSystemViewModel::SequencerDataChanged);
	Sequencer->OnGlobalTimeChanged().AddRaw(this, &FNiagaraSystemViewModel::SequencerTimeChanged);
	Sequencer->GetSelectionChangedTracks().AddRaw(this, &FNiagaraSystemViewModel::SequencerTrackSelectionChanged);
	Sequencer->GetSelectionChangedSections().AddRaw(this, &FNiagaraSystemViewModel::SequencerSectionSelectionChanged);
	Sequencer->SetPlaybackStatus(System.GetNumEmitters() > 0 && EditorSettings->GetAutoPlay()
		? EMovieScenePlayerStatus::Playing
		: EMovieScenePlayerStatus::Stopped);
}

void FNiagaraSystemViewModel::ResetSystem()
{
	ResetSystemInternal(true);
}

void FNiagaraSystemViewModel::ResetSystemInternal(bool bCanResetTime)
{
	bool bResetAge = bCanResetTime && (Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing || EditorSettings->GetResimulateOnChangeWhilePaused() == false);
	if (bResetAge)
	{
		TGuardValue<bool> Guard(bSettingSequencerTimeDirectly, true);
		if (Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
		{
			Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
			Sequencer->SetGlobalTime(0);
			Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
		}
		else
		{
			Sequencer->SetGlobalTime(0);
		}
	}
	
	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		if (Component->GetAsset() == &System)
		{
			Component->ResetSystem();
			if (bResetAge && Component->GetAgeUpdateMode() == ENiagaraAgeUpdateMode::DesiredAge)
			{
				Component->SetDesiredAge(0);
			}
		}
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	bResetRequestPending = false;
}

void FNiagaraSystemViewModel::RequestResetSystem()
{
	bResetRequestPending = true;
}

void FNiagaraSystemViewModel::KillSystemInstances()
{
	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		if (Component->GetAsset() == &System)
		{
			Component->DestroyInstance();
		}
	}
}

void FNiagaraSystemViewModel::ReInitializeSystemInstances()
{
	if (Sequencer.IsValid() && Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		Sequencer->SetGlobalTime(0);
	}

	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		if (Component->GetAsset() == &System)
		{
			Component->ReinitializeSystem();
		}
	}
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

struct FNiagaraSystemCurveData
{
	FRichCurve* Curve;
	FName Name;
	FLinearColor Color;
	UObject* Owner;
};

void GetCurveDataFromInterface(UNiagaraDataInterfaceCurveBase* CurveDataInterface, FString CurveSource, FString DefaultName,  
	TArray<FNiagaraSystemCurveData>& OutCurveData, TArray<UNiagaraDataInterfaceCurveBase*>& OutCurveDataInterfaces)
{
	if (!CurveDataInterface->ShowInCurveEditor)
	{
		return;
	}
	OutCurveDataInterfaces.Add(CurveDataInterface);
	TArray<UNiagaraDataInterfaceCurveBase::FCurveData> CurveData;
	CurveDataInterface->GetCurveData(CurveData);
	for (UNiagaraDataInterfaceCurveBase::FCurveData CurveDataItem : CurveData)
	{
		FNiagaraSystemCurveData SystemCurveData;
		SystemCurveData.Curve = CurveDataItem.Curve;
		SystemCurveData.Color = CurveDataItem.Color;
		SystemCurveData.Owner = CurveDataInterface;
		FString ParameterName = CurveDataItem.Name == NAME_None
			? DefaultName
			: DefaultName + ".";
		FString DataName = CurveDataItem.Name == NAME_None
			? FString()
			: CurveDataItem.Name.ToString();
		SystemCurveData.Name = *(CurveSource + ParameterName + DataName);
		OutCurveData.Add(SystemCurveData);
	}
}

void GetCurveData(FString CurveSource, UNiagaraGraph* SourceGraph, TArray<FNiagaraSystemCurveData>& OutCurveData, TArray<UNiagaraDataInterfaceCurveBase*>& OutCurveDataInterfaces)
{
	TArray<UNiagaraNodeInput*> InputNodes;
	SourceGraph->GetNodesOfClass<UNiagaraNodeInput>(InputNodes);
	TSet<FName> HandledInputs;
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (HandledInputs.Contains(InputNode->Input.GetName()) == false)
		{
			if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
			{
				UNiagaraDataInterfaceCurveBase* CurveDataInterface = Cast<UNiagaraDataInterfaceCurveBase>(InputNode->GetDataInterface());
				if (CurveDataInterface != nullptr)
				{
					FString DefaultName = InputNode->Input.GetName().ToString();
					GetCurveDataFromInterface(CurveDataInterface, CurveSource, DefaultName, OutCurveData, OutCurveDataInterfaces);
				}
			}
			HandledInputs.Add(InputNode->Input.GetName());
		}
	}
}

void FNiagaraSystemViewModel::ResetCurveData()
{
	CurveOwner.EmptyCurves();
	ShownCurveDataInterfaces.Empty();

	TArray<FNiagaraSystemCurveData> CurveData;


	check(SystemScriptViewModel.IsValid()); 
	GetCurveData(
		TEXT("System"),
		SystemScriptViewModel->GetGraphViewModel()->GetGraph(),
		CurveData,
		ShownCurveDataInterfaces);
	// Get curves from user variables
	for (UNiagaraDataInterface* DataInterface : System.GetExposedParameters().GetDataInterfaces())
	{
		UNiagaraDataInterfaceCurveBase* CurveDataInterface = Cast<UNiagaraDataInterfaceCurveBase>(DataInterface);
		if (CurveDataInterface != nullptr)
		{
			GetCurveDataFromInterface(CurveDataInterface, TEXT("System"), TEXT("User"), CurveData, ShownCurveDataInterfaces);
		}
	}
	
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		GetCurveData(
			EmitterHandleViewModel->GetName().ToString(),
			EmitterHandleViewModel->GetEmitterViewModel()->GetSharedScriptViewModel()->GetGraphViewModel()->GetGraph(),
			CurveData, 
			ShownCurveDataInterfaces);
	}

	for (FNiagaraSystemCurveData& CurveDataItem : CurveData)
	{
		CurveOwner.AddCurve(
			*CurveDataItem.Curve,
			CurveDataItem.Name,
			CurveDataItem.Color,
			*CurveDataItem.Owner,
			FNiagaraCurveOwner::FNotifyCurveChanged::CreateRaw(this, &FNiagaraSystemViewModel::CurveChanged));
	}

	OnCurveOwnerChangedDelegate.Broadcast();
}

void UpdateCompiledDataInterfacesForScript(UNiagaraScript& TargetScript, FName TargetDataInterfaceName, UNiagaraDataInterface& SourceDataInterface)
{
	for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : TargetScript.GetCachedDefaultDataInterfaces())
	{
		if (DataInterfaceInfo.Name == TargetDataInterfaceName)
		{
			SourceDataInterface.CopyTo(DataInterfaceInfo.DataInterface);
			break;
		}
	}
}

void FNiagaraSystemViewModel::UpdateCompiledDataInterfaces(UNiagaraDataInterface* ChangedDataInterface)
{
	UNiagaraNodeInput* OuterInputNode = ChangedDataInterface->GetTypedOuter<UNiagaraNodeInput>();
	if (OuterInputNode != nullptr)
	{
		// If the data interface's owning node has been removed from it's graph then it's not valid so early out here.
		bool bIsValidInputNode = OuterInputNode->GetGraph()->Nodes.Contains(OuterInputNode);
		if (bIsValidInputNode == false)
		{
			return;
		}

		// If the data interface was owned by an input node, then we need to try to update the compiled version.
		UNiagaraEmitter* OwningEmitter;
		UNiagaraScript* OwningScript;
		FNiagaraStackGraphUtilities::GetOwningEmitterAndScriptForStackNode(*OuterInputNode, System, OwningEmitter, OwningScript);
		if (ensureMsgf(OwningScript != nullptr, TEXT("Could not find owning script for data interface input node.")))
		{
			switch (OwningScript->GetUsage())
			{
			case ENiagaraScriptUsage::SystemSpawnScript:
			case ENiagaraScriptUsage::SystemUpdateScript:
			case ENiagaraScriptUsage::ParticleSpawnScript:
			case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
			case ENiagaraScriptUsage::ParticleUpdateScript:
			case ENiagaraScriptUsage::ParticleEventScript:
				UpdateCompiledDataInterfacesForScript(*OwningScript, OuterInputNode->Input.GetName(), *ChangedDataInterface);
				break;
			case ENiagaraScriptUsage::EmitterSpawnScript:
			case ENiagaraScriptUsage::EmitterUpdateScript:
				if (ensureMsgf(OwningEmitter != nullptr, TEXT("Could not find owning emitter for data interface input node.")))
				{
					UNiagaraScript& TargetScript = OwningScript->GetUsage() == ENiagaraScriptUsage::EmitterSpawnScript ? *System.GetSystemSpawnScript() : *System.GetSystemUpdateScript();
					FName AliasedInputNodeName = FNiagaraParameterMapHistory::ResolveEmitterAlias(OuterInputNode->Input.GetName(), OwningEmitter->GetUniqueEmitterName());
					UpdateCompiledDataInterfacesForScript(TargetScript, AliasedInputNodeName, *ChangedDataInterface);
				}
				break;
			}
		}
	}
	else
	{
		// If the data interface wasn't owned by a script, try to find it in the exposed parameter data interfaces.
		const FNiagaraVariable* FoundExposedDataInterface = System.GetExposedParameters().FindVariable(ChangedDataInterface);
		if (FoundExposedDataInterface != nullptr)
		{
			System.GetExposedParameters().OnInterfaceChange();
		}
	}
}

void FNiagaraSystemViewModel::EmitterHandlePropertyChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	// When the emitter handle changes, refresh the System scripts emitter nodes and the sequencer tracks just in case the
	// property that changed was the handles emitter.
	if (bUpdatingEmittersFromSequencerDataChange == false)
	{
		TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);
		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			if (EmitterTrack->GetEmitterHandleViewModel() == EmitterHandleViewModel)
			{
				EmitterTrack->UpdateTrackFromEmitterGraphChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
			}
		}
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::EmitterHandleNameChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	CompileSystem(false);
}

void FNiagaraSystemViewModel::EmitterPropertyChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::ScriptCompiled()
{
	//ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::SystemParameterStoreChanged(const FNiagaraParameterStore& ChangedParameterStore, const UNiagaraScript* OwningScript)
{
	UpdateSimulationFromParameterChange();
}

void FNiagaraSystemViewModel::EmitterScriptGraphChanged(const FEdGraphEditAction& InAction, const UNiagaraScript& OwningScript, const TSharedRef<FNiagaraEmitterHandleViewModel> OwningEmitterHandleViewModel)
{
	if (bUpdatingEmittersFromSequencerDataChange == false)
	{
		EmitterIdsRequiringSequencerTrackUpdate.AddUnique(OwningEmitterHandleViewModel->GetId());
	}
	// Remove from cache on graph change 
	EmitterToCachedStackModuleData.Remove(OwningEmitterHandleViewModel->GetId());
}

void FNiagaraSystemViewModel::SystemScriptGraphChanged(const FEdGraphEditAction& InAction)
{
	EmitterToCachedStackModuleData.Empty();
}

void FNiagaraSystemViewModel::EmitterParameterStoreChanged(const FNiagaraParameterStore& ChangedParameterStore, const UNiagaraScript& OwningScript, const TSharedRef<FNiagaraEmitterHandleViewModel> OwningEmitterHandleViewModel)
{
	if (bUpdatingEmittersFromSequencerDataChange == false)
	{
		TGuardValue<bool> UpdateGuard(bUpdatingSequencerFromEmitterDataChange, true);
		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			EmitterTrack->UpdateTrackFromEmitterParameterChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
		}
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
	UpdateSimulationFromParameterChange();
}

void FNiagaraSystemViewModel::UpdateSimulationFromParameterChange()
{
	if (EditorSettings->GetResetSimulationOnChange())
	{
		RequestResetSystem();
	}
	else
	{
		if (Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped)
		{
			// TODO: Update the view when paused and reset on change is turned off.
		}
	}
}

void FNiagaraSystemViewModel::CurveChanged(FRichCurve* ChangedCurve, UObject* InCurveOwner)
{
	UNiagaraDataInterfaceCurveBase* CurveDataInterface = Cast<UNiagaraDataInterfaceCurveBase>(InCurveOwner);
	if (CurveDataInterface != nullptr)
	{
		CurveDataInterface->UpdateLUT();
		UpdateCompiledDataInterfaces(CurveDataInterface);
	}
	ResetSystem();
}

void PopulateNiagaraFoldersFromMovieSceneFolders(const TArray<UMovieSceneFolder*>& MovieSceneFolders, const TArray<UMovieSceneTrack*>& MovieSceneTracks, UNiagaraSystemEditorFolder* ParentFolder)
{
	TArray<FName> ValidFolderNames;
	for (UMovieSceneFolder* MovieSceneFolder : MovieSceneFolders)
	{
		ValidFolderNames.Add(MovieSceneFolder->GetFolderName());
		UNiagaraSystemEditorFolder* MatchingNiagaraFolder = nullptr;
		for (UNiagaraSystemEditorFolder* ChildNiagaraFolder : ParentFolder->GetChildFolders())
		{
			CA_ASSUME(ChildNiagaraFolder != nullptr);
			if (ChildNiagaraFolder->GetFolderName() == MovieSceneFolder->GetFolderName())
			{
				MatchingNiagaraFolder = ChildNiagaraFolder;
				break;
			}
		}

		if (MatchingNiagaraFolder == nullptr)
		{
			MatchingNiagaraFolder = NewObject<UNiagaraSystemEditorFolder>(ParentFolder, MovieSceneFolder->GetFolderName(), RF_Transactional);
			MatchingNiagaraFolder->SetFolderName(MovieSceneFolder->GetFolderName());
			ParentFolder->AddChildFolder(MatchingNiagaraFolder);
		}

		PopulateNiagaraFoldersFromMovieSceneFolders(MovieSceneFolder->GetChildFolders(), MovieSceneFolder->GetChildMasterTracks(), MatchingNiagaraFolder);
	}

	TArray<UNiagaraSystemEditorFolder*> ChildNiagaraFolders = ParentFolder->GetChildFolders();
	for (UNiagaraSystemEditorFolder* ChildNiagaraFolder : ChildNiagaraFolders)
	{
		if (ValidFolderNames.Contains(ChildNiagaraFolder->GetFolderName()) == false)
		{
			ParentFolder->RemoveChildFolder(ChildNiagaraFolder);
		}
	}

	TArray<FGuid> ValidEmitterHandleIds;
	for (UMovieSceneTrack* MovieSceneTrack : MovieSceneTracks)
	{
		UMovieSceneNiagaraEmitterTrack* NiagaraEmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(MovieSceneTrack);
		if (NiagaraEmitterTrack != nullptr)
		{
			FGuid EmitterHandleId = NiagaraEmitterTrack->GetEmitterHandleViewModel()->GetId();
			ValidEmitterHandleIds.Add(EmitterHandleId);
			if (ParentFolder->GetChildEmitterHandleIds().Contains(EmitterHandleId) == false)
			{
				ParentFolder->AddChildEmitterHandleId(EmitterHandleId);
			}
		}
	}

	TArray<FGuid> ChildEmitterHandleIds = ParentFolder->GetChildEmitterHandleIds();
	for (FGuid& ChildEmitterHandleId : ChildEmitterHandleIds)
	{
		if (ValidEmitterHandleIds.Contains(ChildEmitterHandleId) == false)
		{
			ParentFolder->RemoveChildEmitterHandleId(ChildEmitterHandleId);
		}
	}
}

void FNiagaraSystemViewModel::SequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	if (bUpdatingSequencerFromEmitterDataChange == false && GIsTransacting == false)
	{
		TGuardValue<bool> UpdateGuard(bUpdatingEmittersFromSequencerDataChange, true);

		GetOrCreateEditorData().Modify();
		TRange<FFrameNumber> FramePlaybackRange = NiagaraSequence->GetMovieScene()->GetPlaybackRange();
		float StartTimeSeconds = NiagaraSequence->GetMovieScene()->GetTickResolution().AsSeconds(FramePlaybackRange.GetLowerBoundValue());
		float EndTimeSeconds = NiagaraSequence->GetMovieScene()->GetTickResolution().AsSeconds(FramePlaybackRange.GetUpperBoundValue());
		GetOrCreateEditorData().SetPlaybackRange(TRange<float>(StartTimeSeconds, EndTimeSeconds));

		TSet<FGuid> VaildTrackEmitterHandleIds;
		TSet<FGuid> EmittersToDuplicate;
		TArray<TTuple<TSharedPtr<FNiagaraEmitterHandleViewModel>, FName>> EmitterHandlesToRename;

		for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
			if (EmitterTrack->GetEmitterHandleViewModel().IsValid())
			{
				VaildTrackEmitterHandleIds.Add(EmitterTrack->GetEmitterHandleViewModel()->GetId());
				EmitterTrack->UpdateEmitterHandleFromTrackChange(NiagaraSequence->GetMovieScene()->GetTickResolution());
				EmitterTrack->GetEmitterHandleViewModel()->GetEmitterViewModel()->GetOrCreateEditorData().Modify();
				EmitterTrack->GetEmitterHandleViewModel()->GetEmitterViewModel()->GetOrCreateEditorData().SetPlaybackRange(GetEditorData().GetPlaybackRange());
				if (EmitterTrack->GetDisplayName().ToString() != EmitterTrack->GetEmitterHandleViewModel()->GetNameText().ToString())
				{
					EmitterHandlesToRename.Add(TTuple<TSharedPtr<FNiagaraEmitterHandleViewModel>, FName>(EmitterTrack->GetEmitterHandleViewModel(), *EmitterTrack->GetDisplayName().ToString()));
				}
			}
			else
			{
				if (EmitterTrack->GetEmitterHandleId().IsValid())
				{
					// The emitter handle is invalid, but the track has a valid Id, most probably because of a copy/paste event
					EmittersToDuplicate.Add(EmitterTrack->GetEmitterHandleId());
				}
			}
		}

		bool bRefreshAllTracks = EmitterHandlesToRename.Num() > 0;

		for (TTuple<TSharedPtr<FNiagaraEmitterHandleViewModel>, FName>& EmitterHandletoRename : EmitterHandlesToRename)
		{
			EmitterHandletoRename.Get<0>()->SetName(EmitterHandletoRename.Get<1>());
		}

		TSet<FGuid> AllEmitterHandleIds;
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
		{
			AllEmitterHandleIds.Add(EmitterHandleViewModel->GetId());
		}

		TSet<FGuid> RemovedEmitterHandleIds = AllEmitterHandleIds.Difference(VaildTrackEmitterHandleIds);
		if (RemovedEmitterHandleIds.Num() > 0)
		{
			if (bCanModifyEmittersFromTimeline)
			{
				DeleteEmitters(RemovedEmitterHandleIds);
				// When deleting emitters from sequencer, select a new one if one is available.
				if (SelectedEmitterHandleIds.Num() == 0 && EmitterHandleViewModels.Num() > 0)
				{
					SetSelectedEmitterHandleById(EmitterHandleViewModels[0]->GetId());
				}
			}
			else
			{
				bRefreshAllTracks = true;
			}
		}

		if (EmittersToDuplicate.Num() > 0)
		{
			if (bCanModifyEmittersFromTimeline)
			{
				DuplicateEmitters(EmittersToDuplicate);
			}
			else
			{
				bRefreshAllTracks = true;
			}
		}

		TArray<UMovieSceneTrack*> RootTracks;
		TArray<UMovieSceneFolder*> RootFolders = NiagaraSequence->GetMovieScene()->GetRootFolders();
		if (RootFolders.Num() != 0 || GetEditorData().GetRootFolder().GetChildFolders().Num() != 0)
		{
			PopulateNiagaraFoldersFromMovieSceneFolders(RootFolders, RootTracks, &GetOrCreateEditorData().GetRootFolder());
		}

		if (bRefreshAllTracks)
		{
			RefreshSequencerTracks();
		}
	}
}

void FNiagaraSystemViewModel::SequencerTimeChanged()
{
	if (!PreviewComponent)
	{
		return;
	}
	EMovieScenePlayerStatus::Type CurrentStatus = Sequencer->GetPlaybackStatus();
	float CurrentSequencerTime = Sequencer->GetGlobalTime().AsSeconds();
	if (SystemInstance != nullptr)
	{
		// Avoid reentrancy if we're setting the time directly.
		if (bSettingSequencerTimeDirectly == false && CurrentSequencerTime != PreviousSequencerTime)
		{
			// Skip the first update after going from stopped to playing or from playing to stopped because snapping in sequencer may have made
			// the time reverse by a small amount, and sending that update to the System will reset it unnecessarily.
			bool bStartedPlaying = CurrentStatus == EMovieScenePlayerStatus::Playing && PreviousSequencerStatus != EMovieScenePlayerStatus::Playing;
			bool bEndedPlaying = CurrentStatus != EMovieScenePlayerStatus::Playing && PreviousSequencerStatus == EMovieScenePlayerStatus::Playing;

			bool bUpdateDesiredAge = bStartedPlaying == false;
			bool bResetSystemInstance = SystemInstance->IsComplete();

			if (bUpdateDesiredAge)
			{
				if (CurrentStatus == EMovieScenePlayerStatus::Playing)
				{
					PreviewComponent->SetDesiredAge(FMath::Max(CurrentSequencerTime, 0.0f));
				}
				else
				{
					PreviewComponent->SeekToDesiredAge(FMath::Max(CurrentSequencerTime, 0.0f));
				}
			}

			if (bResetSystemInstance)
			{
				// We don't want to reset the current time if we're scrubbing.
				bool bCanResetTime = CurrentStatus == EMovieScenePlayerStatus::Playing;
				ResetSystemInternal(bCanResetTime);
			}
		}
	}

	PreviousSequencerStatus = CurrentStatus;
	PreviousSequencerTime = CurrentSequencerTime;

	OnPostSequencerTimeChangeDelegate.Broadcast();
}

void FNiagaraSystemViewModel::SequencerTrackSelectionChanged(TArray<UMovieSceneTrack*> SelectedTracks)
{
	if (bUpdatingSequencerSelectionFromSystem == false)
	{
		UpdateEmitterHandleSelectionFromSequencer();
	}
}

void FNiagaraSystemViewModel::SequencerSectionSelectionChanged(TArray<UMovieSceneSection*> SelectedSections)
{
	if (bUpdatingSequencerSelectionFromSystem == false)
	{
		UpdateEmitterHandleSelectionFromSequencer();
	}
}

void FNiagaraSystemViewModel::UpdateEmitterHandleSelectionFromSequencer()
{
	TArray<FGuid> NewSelectedEmitterHandleIds;

	TArray<UMovieSceneTrack*> SelectedTracks;
	Sequencer->GetSelectedTracks(SelectedTracks);
	for (UMovieSceneTrack* SelectedTrack : SelectedTracks)
	{
		UMovieSceneNiagaraEmitterTrack* SelectedEmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(SelectedTrack);
		if (SelectedEmitterTrack != nullptr && SelectedEmitterTrack->GetEmitterHandleViewModel().IsValid())
		{
			NewSelectedEmitterHandleIds.AddUnique(SelectedEmitterTrack->GetEmitterHandleViewModel()->GetId());
		}
	}

	TArray<UMovieSceneSection*> SelectedSections;
	Sequencer->GetSelectedSections(SelectedSections);
	for (UMovieSceneSection* SelectedSection : SelectedSections)
	{
		UMovieSceneNiagaraEmitterSectionBase* SelectedEmitterSection = Cast<UMovieSceneNiagaraEmitterSectionBase>(SelectedSection);
		if (SelectedEmitterSection != nullptr && SelectedEmitterSection->GetEmitterHandleViewModel().IsValid())
		{
			NewSelectedEmitterHandleIds.AddUnique(SelectedEmitterSection->GetEmitterHandleViewModel()->GetId());
		}
	}

	TGuardValue<bool> UpdateGuard(bUpdatingSystemSelectionFromSequencer, true);
	SetSelectedEmitterHandlesById(NewSelectedEmitterHandleIds);
}

void FNiagaraSystemViewModel::UpdateSequencerFromEmitterHandleSelection()
{
	TGuardValue<bool> UpdateGuard(bUpdatingSequencerSelectionFromSystem, true);
	Sequencer->EmptySelection();
	for (FGuid SelectedEmitterHandleId : SelectedEmitterHandleIds)
	{
		for (UMovieSceneTrack* MasterTrack : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(MasterTrack);
			if (EmitterTrack != nullptr && EmitterTrack->GetEmitterHandleViewModel()->GetId() == SelectedEmitterHandleId)
			{
				Sequencer->SelectTrack(EmitterTrack);
			}
		}
	}
}

void FNiagaraSystemViewModel::SystemInstanceReset()
{
	SystemInstanceInitialized();
}

void FNiagaraSystemViewModel::PreviewComponentSystemInstanceChanged()
{
	FNiagaraSystemInstance* OldSystemInstance = SystemInstance;
	SystemInstance = PreviewComponent->GetSystemInstance();
	if (SystemInstance != OldSystemInstance)
	{
		if (SystemInstance != nullptr)
		{
			SystemInstance->OnInitialized().AddRaw(this, &FNiagaraSystemViewModel::SystemInstanceInitialized);
			SystemInstance->OnReset().AddRaw(this, &FNiagaraSystemViewModel::SystemInstanceReset);
		}
		else
		{
			for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
			{
				if (EmitterHandleViewModel->GetEmitterHandle())
				{
					EmitterHandleViewModel->SetSimulation(nullptr);
				}
			}
		}
	}
}

void FNiagaraSystemViewModel::SystemInstanceInitialized()
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (EmitterHandleViewModel->GetEmitterHandle())
		{
			EmitterHandleViewModel->SetSimulation(SystemInstance->GetSimulationForHandle(*EmitterHandleViewModel->GetEmitterHandle()));
		}
	}
}

void FNiagaraSystemViewModel::UpdateEmitterFixedBounds()
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	GetSelectedEmitterHandles(SelectedEmitterHandles);

	for (TSharedRef<FNiagaraEmitterHandleViewModel>& SelectedEmitterHandleViewModel : SelectedEmitterHandles)
	{
		FNiagaraEmitterHandle* SelectedEmitterHandle = SelectedEmitterHandleViewModel->GetEmitterHandle();
		check(SelectedEmitterHandle);
		UNiagaraEmitter* Emitter = SelectedEmitterHandle->GetInstance();
		for (TSharedRef<FNiagaraEmitterInstance>& EmitterInst : PreviewComponent->GetSystemInstance()->GetEmitters())
		{
			if (&EmitterInst->GetEmitterHandle() == SelectedEmitterHandle && !EmitterInst->IsComplete())
			{
				FBox EmitterBounds = EmitterInst->CalculateDynamicBounds();
				Emitter->Modify();
				Emitter->bFixedBounds = true;
				//Dynamic bounds are in world space. Transform back to local.
				Emitter->FixedBounds = EmitterBounds.TransformBy(PreviewComponent->GetComponentToWorld().Inverse());
			}
		}
	}
}

void FNiagaraSystemViewModel::AddSystemEventHandlers()
{
	if (System.IsValid())
	{
		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(System.GetSystemSpawnScript());
		Scripts.Add(System.GetSystemUpdateScript());
		
		for (UNiagaraScript* Script : Scripts)
		{
			FDelegateHandle OnParameterStoreChangedHandle = Script->RapidIterationParameters.AddOnChangedHandler(
				FNiagaraParameterStore::FOnChanged::FDelegate::CreateRaw<FNiagaraSystemViewModel, const FNiagaraParameterStore&, const UNiagaraScript*>(
					this, &FNiagaraSystemViewModel::SystemParameterStoreChanged, Script->RapidIterationParameters, Script));
			ScriptToOnParameterStoreChangedHandleMap.Add(FObjectKey(Script), OnParameterStoreChangedHandle);
		}

		UserParameterStoreChangedHandle = System.GetExposedParameters().AddOnChangedHandler(
			FNiagaraParameterStore::FOnChanged::FDelegate::CreateRaw<FNiagaraSystemViewModel, const FNiagaraParameterStore&, const UNiagaraScript*>(
				this, &FNiagaraSystemViewModel::SystemParameterStoreChanged, System.GetExposedParameters(), nullptr));

		SystemScriptGraphChangedHandler = SystemScriptViewModel->GetGraphViewModel()->GetGraph()->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraSystemViewModel::SystemScriptGraphChanged));
	}
}

void FNiagaraSystemViewModel::RemoveSystemEventHandlers()
{
	if (System.IsValid())
	{
		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(System.GetSystemSpawnScript());
		Scripts.Add(System.GetSystemUpdateScript());

		for (UNiagaraScript* Script : Scripts)
		{
			FDelegateHandle* OnParameterStoreChangedHandle = ScriptToOnParameterStoreChangedHandleMap.Find(FObjectKey(Script));
			if (OnParameterStoreChangedHandle != nullptr)
			{
				Script->RapidIterationParameters.RemoveOnChangedHandler(*OnParameterStoreChangedHandle);
			}
		}

		System.GetExposedParameters().RemoveOnChangedHandler(UserParameterStoreChangedHandle);
		if (SystemScriptViewModel.IsValid())
		{
			SystemScriptViewModel->GetGraphViewModel()->GetGraph()->RemoveOnGraphChangedHandler(SystemScriptGraphChangedHandler);
		}
	}

	ScriptToOnParameterStoreChangedHandleMap.Empty();
	UserParameterStoreChangedHandle.Reset();
}

void FNiagaraSystemViewModel::NotifyPinnedCurvesChanged()
{
	OnPinnedCurvesChangedDelegate.Broadcast();
}

void FNiagaraSystemViewModel::BuildStackModuleData(UNiagaraScript* Script, FGuid InEmitterHandleId, TArray<FNiagaraStackModuleData>& OutStackModuleData)
{
	UNiagaraNodeOutput* OutputNode = FNiagaraEditorUtilities::GetScriptOutputNode(*Script);
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups((UNiagaraNode&)*OutputNode, StackGroups);

	int StackIndex = 0;
	if (StackGroups.Num() > 2)
	{
		for (int i = 1; i < StackGroups.Num() - 1; i++)
		{
			FNiagaraStackGraphUtilities::FStackNodeGroup& StackGroup = StackGroups[i];
			StackIndex = i - 1;
			TArray<UNiagaraNode*> GroupNodes;
			StackGroup.GetAllNodesInGroup(GroupNodes);
			UNiagaraNodeFunctionCall * ModuleNode = Cast<UNiagaraNodeFunctionCall>(StackGroup.EndNode);
			if (ModuleNode != nullptr)
			{
				ENiagaraScriptUsage Usage = Script->GetUsage();
				FGuid UsageId = Script->GetUsageId();
				int32 Index = StackIndex;
				FNiagaraStackModuleData ModuleData = { ModuleNode, Usage, UsageId, Index, InEmitterHandleId };
				OutStackModuleData.Add(ModuleData);
			}
		}
	}

}

#undef LOCTEXT_NAMESPACE // NiagaraSystemViewModel
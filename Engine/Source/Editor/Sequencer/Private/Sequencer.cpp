// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sequencer.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/MessageDialog.h"
#include "Containers/ArrayBuilder.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/MetaData.h"
#include "UObject/PropertyPortFlags.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Editor.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScenePossessable.h"
#include "MovieScene.h"
#include "Widgets/Layout/SBorder.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Exporters/Exporter.h"
#include "Editor/UnrealEdEngine.h"
#include "Camera/CameraActor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "EditorModeManager.h"
#include "UnrealEdMisc.h"
#include "EditorDirectories.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "SequencerCommands.h"
#include "DisplayNodes/SequencerFolderNode.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "ISequencerSection.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "MovieSceneClipboard.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "SSequencerSection.h"
#include "SequencerKeyCollection.h"
#include "SequencerSettings.h"
#include "SequencerLog.h"
#include "SequencerEdMode.h"
#include "MovieSceneSequence.h"
#include "MovieSceneFolder.h"
#include "PropertyEditorModule.h"
#include "EditorWidgetsModule.h"
#include "ILevelViewport.h"
#include "EditorSupportDelegates.h"
#include "SSequencerTreeView.h"
#include "ScopedTransaction.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieSceneCameraAnimTrack.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneToolHelpers.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "MovieSceneObjectBindingIDCustomization.h"
#include "ISettingsModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"
#include "MovieSceneCaptureDialogModule.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneCommonHelpers.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PackageTools.h"
#include "VirtualTrackArea.h"
#include "SequencerUtilities.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "ISequenceRecorder.h"
#include "CineCameraActor.h"
#include "CameraRig_Rail.h"
#include "CameraRig_Crane.h"
#include "Components/SplineComponent.h"
#include "DesktopPlatformModule.h"
#include "Factories.h"
#include "FbxExporter.h"
#include "UnrealExporter.h"
#include "ISequencerEditorObjectBinding.h"
#include "LevelSequence.h"
#include "IVREditorModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Compilation/MovieSceneCompiler.h"
#include "SequencerKeyActor.h"
#include "MovieSceneCopyableBinding.h"
#include "MovieSceneCopyableTrack.h"
#include "ISequencerChannelInterface.h"
#include "SequencerKeyCollection.h"
#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "CurveDataAbstraction.h"
#include "Fonts/FontMeasure.h"
#include "MovieSceneTimeHelpers.h"
#include "FrameNumberNumericInterface.h"
#include "UObject/StrongObjectPtr.h"
#include "SequencerExportTask.h"
#include "LevelUtils.h"
#include "Engine/Blueprint.h"
#include "MovieSceneSequenceEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ISerializedRecorder.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "Sequencer"

DEFINE_LOG_CATEGORY(LogSequencer);

static TAutoConsoleVariable<float> CVarAutoScrubSpeed(
	TEXT("Sequencer.AutoScrubSpeed"),
	6.0f,
	TEXT("How fast to scrub forward/backward when auto-scrubbing"));

static TAutoConsoleVariable<float> CVarAutoScrubCurveExponent(
	TEXT("Sequencer.AutoScrubCurveExponent"),
	2.0f,
	TEXT("How much to ramp in and out the scrub speed when auto-scrubbing"));

struct FSequencerTemplateStore : IMovieSceneSequenceTemplateStore
{
	void Reset()
	{
		Templates.Reset();
	}

	void PurgeStaleTracks()
	{
		for (auto& Pair : Templates)
		{
			Pair.Value->PurgeStaleTracks();
		}
	}

	virtual FMovieSceneEvaluationTemplate& AccessTemplate(UMovieSceneSequence& Sequence)
	{
		FObjectKey SequenceKey(&Sequence);
		if (TUniquePtr<FMovieSceneEvaluationTemplate>* ExistingTemplate = Templates.Find(SequenceKey))
		{
			FMovieSceneEvaluationTemplate* Template = ExistingTemplate->Get();
			return *Template;
		}
		else
		{
			FMovieSceneEvaluationTemplate* NewTemplate = new FMovieSceneEvaluationTemplate;
			Templates.Add(SequenceKey, TUniquePtr<FMovieSceneEvaluationTemplate>(NewTemplate));
			return *NewTemplate;
		}
	}

	// Store templates as unique ptrs to ensure that external pointers don't become invalid when the array is reallocated
	TMap<FObjectKey, TUniquePtr<FMovieSceneEvaluationTemplate>> Templates;
};


struct FSequencerCurveEditorBounds : ICurveEditorBounds
{
	FSequencerCurveEditorBounds(TSharedRef<FSequencer> InSequencer)
		: OutputMin(0), OutputMax(1), WeakSequencer(InSequencer)
	{}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const override
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			TRange<double> Bounds = Sequencer->GetViewRange();
			OutMin = Bounds.GetLowerBoundValue();
			OutMax = Bounds.GetUpperBoundValue();
		}
	}
	virtual void GetOutputBounds(double& OutMin, double& OutMax) const override
	{
		OutMin = OutputMin;
		OutMax = OutputMax;
	}

	virtual void SetInputBounds(double InMin, double InMax) override
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

			if (InMin * TickResolution > TNumericLimits<int32>::Lowest() && InMax * TickResolution < TNumericLimits<int32>::Max())
			{
				Sequencer->SetViewRange(TRange<double>(InMin, InMax), EViewRangeInterpolation::Immediate);
			}
		}
	}
	virtual void SetOutputBounds(double InMin, double InMax) override
	{
		if (InMax - InMin < TNumericLimits<float>::Max())
		{
			OutputMin = InMin;
			OutputMax = InMax;
		}
	}

	double OutputMin, OutputMax;
	TWeakPtr<FSequencer> WeakSequencer;
};

class FSequencerCurveEditor : public FCurveEditor
{
public:
	TWeakPtr<FSequencer> WeakSequencer;

	FSequencerCurveEditor(TWeakPtr<FSequencer> InSequencer)
		: WeakSequencer(InSequencer)
	{}

	virtual void GetGridLinesX(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>& MajorGridLabels) const override
	{
		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();

		FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();

		double MajorGridStep  = 0.0;
		int32  MinorDivisions = 0;

		if (Sequencer.IsValid() && Sequencer->GetGridMetrics(ScreenSpace.GetPhysicalWidth(), MajorGridStep, MinorDivisions))
		{
			const double FirstMajorLine = FMath::FloorToDouble(ScreenSpace.GetInputMin() / MajorGridStep) * MajorGridStep;
			const double LastMajorLine  = FMath::CeilToDouble(ScreenSpace.GetInputMax() / MajorGridStep) * MajorGridStep;

			for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
			{
				MajorGridLines.Add( ScreenSpace.SecondsToScreen(CurrentMajorLine) );
				MajorGridLabels.Add(FText());
				 
				for (int32 Step = 1; Step < MinorDivisions; ++Step)
				{
					MinorGridLines.Add( ScreenSpace.SecondsToScreen(CurrentMajorLine + Step*MajorGridStep/MinorDivisions) );
				}
			}
		}
	}
};

void FSequencer::InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates, const TArray<FOnCreateEditorObjectBinding>& EditorObjectBindingDelegates)
{
	bIsEditingWithinLevelEditor = InitParams.bEditWithinLevelEditor;
	ScrubStyle = InitParams.ViewParams.ScrubberStyle;

	SilentModeCount = 0;
	bReadOnly = InitParams.ViewParams.bReadOnly;

	PreAnimatedState.EnableGlobalCapture();

	if (InitParams.SpawnRegister.IsValid())
	{
		SpawnRegister = InitParams.SpawnRegister;
	}
	else
	{
		// Spawnables not supported
		SpawnRegister = MakeShareable(new FNullMovieSceneSpawnRegister);
	}

	EventContextsAttribute = InitParams.EventContexts;
	if (EventContextsAttribute.IsSet())
	{
		CachedEventContexts.Reset();
		for (UObject* Object : EventContextsAttribute.Get())
		{
			CachedEventContexts.Add(Object);
		}
	}

	PlaybackContextAttribute = InitParams.PlaybackContext;
	CachedPlaybackContext = PlaybackContextAttribute.Get(nullptr);

	Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(*InitParams.ViewParams.UniqueName);

	Settings->GetOnEvaluateSubSequencesInIsolationChanged().AddSP(this, &FSequencer::RestorePreAnimatedState);
	Settings->GetOnShowSelectedNodesOnlyChanged().AddSP(this, &FSequencer::OnSelectedNodesOnlyChanged);
	Settings->GetOnCurveEditorCurveVisibilityChanged().AddSP(this, &FSequencer::SyncCurveEditorToSelection, false);

	{
		CurveEditorModel = MakeShared<FSequencerCurveEditor>(SharedThis(this));
		CurveEditorModel->SetBounds(MakeUnique<FSequencerCurveEditorBounds>(SharedThis(this)));

		CurveEditorModel->InputSnapEnabledAttribute   = MakeAttributeLambda([this]{ return Settings->GetIsSnapEnabled(); });
		CurveEditorModel->OnInputSnapEnabledChanged   = FOnSetBoolean::CreateLambda([this](bool NewValue){ Settings->SetIsSnapEnabled(NewValue); });

		CurveEditorModel->OutputSnapEnabledAttribute  = MakeAttributeLambda([this]{ return Settings->GetSnapCurveValueToInterval(); });
		CurveEditorModel->OnOutputSnapEnabledChanged  = FOnSetBoolean::CreateLambda([this](bool NewValue){ Settings->SetSnapCurveValueToInterval(NewValue); });

		CurveEditorModel->OutputSnapIntervalAttribute = MakeAttributeLambda([this]{ return (double)Settings->GetCurveValueSnapInterval(); });
		CurveEditorModel->InputSnapRateAttribute      = MakeAttributeSP(this, &FSequencer::GetFocusedDisplayRate);

		CurveEditorModel->DefaultKeyAttributes        = MakeAttributeSP(this, &FSequencer::GetDefaultKeyAttributes);
	}

	{
		FDelegateHandle OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddLambda([&]{ State.InvalidateExpiredObjects(); });
		AcquiredResources.Add([=]{ GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledHandle); });
	}

	{
		ISequenceRecorder* Recorder = FModuleManager::Get().GetModulePtr<ISequenceRecorder>("SequenceRecorder");
		
		Recorder->OnRecordingStarted().AddSP(this, &FSequencer::HandleRecordingStarted);
		Recorder->OnRecordingFinished().AddSP(this, &FSequencer::HandleRecordingFinished);
	}
	ToolkitHost = InitParams.ToolkitHost;

	PlaybackSpeed = 1.f;
	ShuttleMultiplier = 0;
	ObjectChangeListener = InObjectChangeListener;

	check( ObjectChangeListener.IsValid() );
	
	RootSequence = InitParams.RootSequence;

	UpdateTimeBases();
	PlayPosition.Reset(FFrameTime(0));

	TemplateStore = MakeShared<FSequencerTemplateStore>();

	ActiveTemplateIDs.Add(MovieSceneSequenceID::Root);
	ActiveTemplateStates.Add(true);
	RootTemplateInstance.Initialize(*InitParams.RootSequence, *this, TemplateStore.ToSharedRef());

	ResetTimeController();

	// Make internal widgets
	SequencerWidget = SNew( SSequencer, SharedThis( this ) )
		.ViewRange( this, &FSequencer::GetViewRange )
		.ClampRange( this, &FSequencer::GetClampRange )
		.PlaybackRange( this, &FSequencer::GetPlaybackRange )
		.PlaybackStatus( this, &FSequencer::GetPlaybackStatus )
		.SelectionRange( this, &FSequencer::GetSelectionRange )
		.VerticalFrames(this, &FSequencer::GetVerticalFrames)
		.MarkedFrames(this, &FSequencer::GetMarkedFrames)
		.OnMarkedFrameChanged(this, &FSequencer::SetMarkedFrame )
		.OnClearAllMarkedFrames(this, &FSequencer::ClearAllMarkedFrames )
		.SubSequenceRange( this, &FSequencer::GetSubSequenceRange )
		.OnPlaybackRangeChanged( this, &FSequencer::SetPlaybackRange )
		.OnPlaybackRangeBeginDrag( this, &FSequencer::OnPlaybackRangeBeginDrag )
		.OnPlaybackRangeEndDrag( this, &FSequencer::OnPlaybackRangeEndDrag )
		.OnSelectionRangeChanged( this, &FSequencer::SetSelectionRange )
		.OnSelectionRangeBeginDrag( this, &FSequencer::OnSelectionRangeBeginDrag )
		.OnSelectionRangeEndDrag( this, &FSequencer::OnSelectionRangeEndDrag )
		.IsPlaybackRangeLocked( this, &FSequencer::IsPlaybackRangeLocked )
		.OnTogglePlaybackRangeLocked( this, &FSequencer::TogglePlaybackRangeLocked )
		.ScrubPosition( this, &FSequencer::GetLocalFrameTime )
		.OnBeginScrubbing( this, &FSequencer::OnBeginScrubbing )
		.OnEndScrubbing( this, &FSequencer::OnEndScrubbing )
		.OnScrubPositionChanged( this, &FSequencer::OnScrubPositionChanged )
		.OnViewRangeChanged( this, &FSequencer::SetViewRange )
		.OnClampRangeChanged( this, &FSequencer::OnClampRangeChanged )
		.OnGetNearestKey( this, &FSequencer::OnGetNearestKey )
		.OnGetAddMenuContent(InitParams.ViewParams.OnGetAddMenuContent)
		.OnBuildCustomContextMenuForGuid(InitParams.ViewParams.OnBuildCustomContextMenuForGuid)
		.OnReceivedFocus(InitParams.ViewParams.OnReceivedFocus)
		.AddMenuExtender(InitParams.ViewParams.AddMenuExtender)
		.ToolbarExtender(InitParams.ViewParams.ToolbarExtender);

	// When undo occurs, get a notification so we can make sure our view is up to date
	GEditor->RegisterForUndo(this);

	// Create tools and bind them to this sequencer
	for( int32 DelegateIndex = 0; DelegateIndex < TrackEditorDelegates.Num(); ++DelegateIndex )
	{
		check( TrackEditorDelegates[DelegateIndex].IsBound() );
		// Tools may exist in other modules, call a delegate that will create one for us 
		TSharedRef<ISequencerTrackEditor> TrackEditor = TrackEditorDelegates[DelegateIndex].Execute( SharedThis( this ) );
		TrackEditors.Add( TrackEditor );
	}

	for (int32 DelegateIndex = 0; DelegateIndex < EditorObjectBindingDelegates.Num(); ++DelegateIndex)
	{
		check(EditorObjectBindingDelegates[DelegateIndex].IsBound());
		// Object bindings may exist in other modules, call a delegate that will create one for us 
		TSharedRef<ISequencerEditorObjectBinding> ObjectBinding = EditorObjectBindingDelegates[DelegateIndex].Execute(SharedThis(this));
		ObjectBindings.Add(ObjectBinding);
	}

	FMovieSceneObjectBindingIDCustomization::BindTo(AsShared());

	ZoomAnimation = FCurveSequence();
	ZoomCurve = ZoomAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);
	OverlayAnimation = FCurveSequence();
	OverlayCurve = OverlayAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);

	// Update initial movie scene data
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::ActiveMovieSceneChanged );
	UpdateTimeBoundsToFocusedMovieScene();

	FQualifiedFrameTime CurrentTime = GetLocalTime();
	if (!TargetViewRange.Contains(CurrentTime.AsSeconds()))
	{
		SetLocalTimeDirectly(LastViewRange.GetLowerBoundValue() * CurrentTime.Rate);
		OnGlobalTimeChangedDelegate.Broadcast();
	}

	// NOTE: Could fill in asset editor commands here!

	BindCommands();

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnInitialize();
	}

	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs[0]);
}


FSequencer::FSequencer()
	: SequencerCommandBindings( new FUICommandList )
	, SequencerSharedBindings( new FUICommandList )
	, TargetViewRange(0.f, 5.f)
	, LastViewRange(0.f, 5.f)
	, ViewRangeBeforeZoom(TRange<double>::Empty())
	, PlaybackState( EMovieScenePlayerStatus::Stopped )
	, bPerspectiveViewportPossessionEnabled( true )
	, bPerspectiveViewportCameraCutEnabled( false )
	, bIsEditingWithinLevelEditor( false )
	, bShowCurveEditor( false )
	, bNeedTreeRefresh( false )
	, StoredPlaybackState( EMovieScenePlayerStatus::Stopped )
	, NodeTree( MakeShareable( new FSequencerNodeTree( *this ) ) )
	, bUpdatingSequencerSelection( false )
	, bUpdatingExternalSelection( false )
	, OldMaxTickRate(GEngine->GetMaxFPS())
	, bNeedsEvaluate(false)
{
	Selection.GetOnOutlinerNodeSelectionChanged().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
	Selection.GetOnNodesWithSelectedKeysOrSectionsChanged().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
	Selection.GetOnOutlinerNodeSelectionChangedObjectGuids().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
}


FSequencer::~FSequencer()
{
	RootTemplateInstance.Finish(*this);
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnRelease();
	}

	AcquiredResources.Release();
	SequencerWidget.Reset();
	TrackEditors.Empty();
}


void FSequencer::Close()
{
	RootTemplateInstance.Finish(*this);
	RestorePreAnimatedState();

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnRelease();
	}

	SequencerWidget.Reset();
	TrackEditors.Empty();

	GUnrealEd->UpdatePivotLocationForSelection();
	
	OnCloseEventDelegate.Broadcast(AsShared());
}


void FSequencer::Tick(float InDeltaTime)
{
	static bool bEnableRefCountCheck = true;
	if (bEnableRefCountCheck && !FSlateApplication::Get().AnyMenusVisible())
	{
		const int32 SequencerRefCount = AsShared().GetSharedReferenceCount() - 1;
		ensureAlwaysMsgf(SequencerRefCount == 1, TEXT("Multiple persistent shared references detected for Sequencer. There should only be one persistent authoritative reference. Found %d additional references which will result in FSequencer not being released correctly."), SequencerRefCount - 1);
	}

	// Ensure the time bases for our playback position are kept up to date with the root sequence
	UpdateTimeBases();

	Selection.Tick();

	if (PlaybackContextAttribute.IsBound())
	{
		TWeakObjectPtr<UObject> NewPlaybackContext = PlaybackContextAttribute.Get();

		if (CachedPlaybackContext != NewPlaybackContext)
		{
			PrePossessionViewTargets.Reset();
			State.ClearObjectCaches(*this);
			RestorePreAnimatedState();
			NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshAllImmediately);
			CachedPlaybackContext = NewPlaybackContext;
		}
	}

	{
		TSet<UMovieSceneSequence*> DirtySequences;
		bool bSequenceIsDirty = RootTemplateInstance.IsDirty(&DirtySequences);

		// If we only dirtied a single sequence, and this is the sequence that has a supression assigned, and the signature is the same, we don't auto-evaluate
		if (bSequenceIsDirty && SuppressAutoEvalSignature.IsSet() && DirtySequences.Num() == 1)
		{
			UMovieSceneSequence* SuppressSequence = SuppressAutoEvalSignature->Get<0>().Get();
			const FGuid& SuppressSignature = SuppressAutoEvalSignature->Get<1>();

			if (SuppressSequence && DirtySequences.Contains(SuppressSequence) && SuppressSequence->GetSignature() == SuppressSignature)
			{
				// Suppress auto evaluation
				bSequenceIsDirty = false;
			}
		}

		if (bSequenceIsDirty)
		{
			bNeedsEvaluate = true;
		}
	}

	if (bNeedTreeRefresh)
	{
		SelectionPreview.Empty();

		RefreshTree();

		SetPlaybackStatus(StoredPlaybackState);
	}


	UObject* PlaybackContext = GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
	float Dilation = World ? World->GetWorldSettings()->MatineeTimeDilation : 1.f;

	TimeController->Tick(InDeltaTime, PlaybackSpeed * Dilation);

	FQualifiedFrameTime GlobalTime = GetGlobalTime();
	FFrameTime NewGlobalTime = TimeController->RequestCurrentTime(GlobalTime, PlaybackSpeed * Dilation);

	static const float AutoScrollFactor = 0.1f;

	// Animate the autoscroll offset if it's set
	if (AutoscrollOffset.IsSet())
	{
		float Offset = AutoscrollOffset.GetValue() * AutoScrollFactor;
		SetViewRange(TRange<double>(TargetViewRange.GetLowerBoundValue() + Offset, TargetViewRange.GetUpperBoundValue() + Offset), EViewRangeInterpolation::Immediate);
	}

	// Animate the autoscrub offset if it's set
	if (AutoscrubOffset.IsSet())
	{
		FQualifiedFrameTime CurrentTime = GetLocalTime();
		FFrameTime Offset = (AutoscrubOffset.GetValue() * AutoScrollFactor) * CurrentTime.Rate;
		SetLocalTimeDirectly(CurrentTime.Time + Offset);
	}

	// override max frame rate
	if (PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		if (PlayPosition.GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked)
		{
			GEngine->SetMaxFPS(1.f / PlayPosition.GetInputRate().AsInterval());
		}
		else
		{
			GEngine->SetMaxFPS(OldMaxTickRate);
		}
	}

	if (GetSelectionRange().IsEmpty() && GetLoopMode() == SLM_LoopSelectionRange)
	{
		Settings->SetLoopMode(SLM_Loop);
	}

	if (PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		// Put the time into local space
		SetLocalTimeLooped(NewGlobalTime * RootToLocalTransform);

		if (IsAutoScrollEnabled() && GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
		{
			const float ThresholdPercentage = 0.15f;
			UpdateAutoScroll(GetLocalTime().Time / GetFocusedTickResolution(), ThresholdPercentage);
		}
	}
	else
	{
		PlayPosition.Reset(GlobalTime.ConvertTo(PlayPosition.GetInputRate()));
	}

	if (AutoScrubTarget.IsSet())
	{
		const double ScrubSpeed = CVarAutoScrubSpeed->GetFloat();		// How fast to scrub at peak curve speed
		const double AutoScrubExp = CVarAutoScrubCurveExponent->GetFloat();	// How long to ease in and out.  Bigger numbers allow for longer easing.

		const double SecondsPerFrame = GetFocusedTickResolution().AsInterval() / ScrubSpeed;
		const int32 TotalFrames = FMath::Abs(AutoScrubTarget.GetValue().DestinationTime.GetFrame().Value - AutoScrubTarget.GetValue().SourceTime.GetFrame().Value);
		const double TargetSeconds = (double)TotalFrames * SecondsPerFrame;

		double ElapsedSeconds = FPlatformTime::Seconds() - AutoScrubTarget.GetValue().StartTime;
		float Alpha = ElapsedSeconds / TargetSeconds;
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
		int32 NewFrameNumber = FMath::InterpEaseInOut(AutoScrubTarget.GetValue().SourceTime.GetFrame().Value, AutoScrubTarget.GetValue().DestinationTime.GetFrame().Value, Alpha, AutoScrubExp);

		FAutoScrubTarget CachedTarget = AutoScrubTarget.GetValue();

		SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
		PlayPosition.SetTimeBase(GetRootTickResolution(), GetRootTickResolution(), EMovieSceneEvaluationType::WithSubFrames);
		SetLocalTimeDirectly(FFrameNumber(NewFrameNumber));

		AutoScrubTarget = CachedTarget;

		if (FMath::IsNearlyEqual(Alpha, 1.f, KINDA_SMALL_NUMBER))
		{
			SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
			AutoScrubTarget.Reset();
		}
	}

	UpdateSubSequenceData();

	// Tick all the tools we own as well
	for (int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex)
	{
		TrackEditors[EditorIndex]->Tick(InDeltaTime);
	}

	if (!IsInSilentMode())
	{
		if (bNeedsEvaluate)
		{
			EvaluateInternal(PlayPosition.GetCurrentPositionAsRange());
		}
	}

	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
	if(SequenceRecorder.IsRecording())
	{
		UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
		if(Section != nullptr && Section->HasStartFrame())
		{
			FFrameRate   SectionResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
			FFrameNumber RecordingLength   = SequenceRecorder.GetCurrentRecordingLength().ConvertTo(SectionResolution).CeilToFrame();

			if (RecordingLength > 0)
			{
				FFrameNumber EndFrame = Section->GetInclusiveStartFrame() + RecordingLength;
				Section->SetRange(TRange<FFrameNumber>(Section->GetInclusiveStartFrame(), TRangeBound<FFrameNumber>::Exclusive(EndFrame)));
			}
		}
	}

	// Reset any player controllers that we were possessing, if we're not possessing them any more
	if (!IsPerspectiveViewportCameraCutEnabled() && PrePossessionViewTargets.Num())
	{
		for (const FCachedViewTarget& CachedView : PrePossessionViewTargets)
		{
			APlayerController* PlayerController = CachedView.PlayerController.Get();
			AActor* ViewTarget = CachedView.ViewTarget.Get();

			if (PlayerController && ViewTarget)
			{
				PlayerController->SetViewTarget(ViewTarget);
			}
		}
		PrePossessionViewTargets.Reset();
	}
}


TSharedRef<SWidget> FSequencer::GetSequencerWidget() const
{
	return SequencerWidget.ToSharedRef();
}


UMovieSceneSequence* FSequencer::GetRootMovieSceneSequence() const
{
	return RootSequence.Get();
}


UMovieSceneSequence* FSequencer::GetFocusedMovieSceneSequence() const
{
	// the last item is the focused movie scene
	if (ActiveTemplateIDs.Num())
	{
		return RootTemplateInstance.GetSequence(ActiveTemplateIDs.Last());
	}

	return nullptr;
}

UMovieSceneSubSection* FSequencer::FindSubSection(FMovieSceneSequenceID SequenceID) const
{
	if (SequenceID == MovieSceneSequenceID::Root)
	{
		return nullptr;
	}

	const FMovieSceneSequenceHierarchy&     Hierarchy    = RootTemplateInstance.GetHierarchy();
	const FMovieSceneSequenceHierarchyNode* SequenceNode = Hierarchy.FindNode(SequenceID);
	const FMovieSceneSubSequenceData*       SubData      = Hierarchy.FindSubData(SequenceID);

	if (SubData && SequenceNode)
	{
		UMovieSceneSequence* ParentSequence   = RootTemplateInstance.GetSequence(SequenceNode->ParentID);
		UMovieScene*         ParentMovieScene = ParentSequence ? ParentSequence->GetMovieScene() : nullptr;

		if (ParentMovieScene)
		{
			return FindObject<UMovieSceneSubSection>(ParentMovieScene, *SubData->SectionPath.ToString());
		}
	}

	return nullptr;
}


void FSequencer::ResetToNewRootSequence(UMovieSceneSequence& NewSequence)
{
	RootSequence = &NewSequence;
	RestorePreAnimatedState();

	RootTemplateInstance.Finish(*this);

	TemplateStore->Reset();

	ActiveTemplateIDs.Reset();
	ActiveTemplateIDs.Add(MovieSceneSequenceID::Root);
	ActiveTemplateStates.Reset();
	ActiveTemplateStates.Add(true);

	RootTemplateInstance.Initialize(NewSequence, *this);

	RootToLocalTransform = FMovieSceneSequenceTransform();

	ResetPerMovieSceneData();
	SequencerWidget->ResetBreadcrumbs();

	PlayPosition.Reset(GetPlaybackRange().GetLowerBoundValue());
	TimeController->Reset(FQualifiedFrameTime(PlayPosition.GetCurrentPosition(), GetRootTickResolution()));

	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());
}


void FSequencer::FocusSequenceInstance(UMovieSceneSubSection& InSubSection)
{
	FMovieSceneRootOverridePath Path;
	Path.Set(ActiveTemplateIDs.Last(), RootTemplateInstance.GetHierarchy());

	// Root out the SequenceID for the sub section
	FMovieSceneSequenceID SequenceID = Path.Remap(InSubSection.GetSequenceID());

	// Ensure the hierarchy is up to date for this level
	int32 MaxDepth = 1;
	FMovieSceneCompiler::CompileHierarchy(*GetFocusedMovieSceneSequence(), RootTemplateInstance.GetHierarchy(), ActiveTemplateIDs.Last(), MaxDepth);

	if (!ensure(RootTemplateInstance.GetHierarchy().FindSubData(SequenceID)))
	{
		return;
	}

	ActiveTemplateIDs.Push(SequenceID);
	ActiveTemplateStates.Push(InSubSection.IsActive());

	if (Settings->ShouldEvaluateSubSequencesInIsolation())
	{
		RestorePreAnimatedState();
	}

	UpdateSubSequenceData();

	// Reset data that is only used for the previous movie scene
	ResetPerMovieSceneData();
	SequencerWidget->UpdateBreadcrumbs();

	if (!State.FindSequence(SequenceID))
	{
		State.AssignSequence(SequenceID, *GetFocusedMovieSceneSequence(), *this);
	}

	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());

	bNeedsEvaluate = true;
}


void FSequencer::SuppressAutoEvaluation(UMovieSceneSequence* Sequence, const FGuid& InSequenceSignature)
{
	SuppressAutoEvalSignature = MakeTuple(MakeWeakObjectPtr(Sequence), InSequenceSignature);
}

FGuid FSequencer::CreateBinding(UObject& InObject, const FString& InName)
{
	const FScopedTransaction Transaction(LOCTEXT("CreateBinding", "Create New Binding"));

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	OwnerSequence->Modify();
	OwnerMovieScene->Modify();
		
	const FGuid PossessableGuid = OwnerMovieScene->AddPossessable(InName, InObject.GetClass());

	// Attempt to use the parent as a context if necessary
	UObject* ParentObject = OwnerSequence->GetParentObject(&InObject);
	UObject* BindingContext = GetPlaybackContext();

	if (ParentObject)
	{
		// Ensure we have possessed the outer object, if necessary
		FGuid ParentGuid = GetHandleToObject(ParentObject);
		
		if (OwnerSequence->AreParentContextsSignificant())
		{
			BindingContext = ParentObject;
		}

		// Set up parent/child guids for possessables within spawnables
		if (ParentGuid.IsValid())
		{
			FMovieScenePossessable* ChildPossessable = OwnerMovieScene->FindPossessable(PossessableGuid);
			if (ensure(ChildPossessable))
			{
				ChildPossessable->SetParent(ParentGuid);
			}

			FMovieSceneSpawnable* ParentSpawnable = OwnerMovieScene->FindSpawnable(ParentGuid);
			if (ParentSpawnable)
			{
				ParentSpawnable->AddChildPossessable(PossessableGuid);
			}
		}
	}

	OwnerSequence->BindPossessableObject(PossessableGuid, InObject, BindingContext);

	return PossessableGuid;
}


UObject* FSequencer::GetPlaybackContext() const
{
	return CachedPlaybackContext.Get();
}

TArray<UObject*> FSequencer::GetEventContexts() const
{
	TArray<UObject*> Temp;
	CopyFromWeakArray(Temp, CachedEventContexts);
	return Temp;
}

void FSequencer::GetKeysFromSelection(TUniquePtr<FSequencerKeyCollection>& KeyCollection, float DuplicateThresholdSeconds)
{
	if (!KeyCollection.IsValid())
	{
		KeyCollection.Reset(new FSequencerKeyCollection);
	}

	TArray<FSequencerDisplayNode*> SelectedNodes;
	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		SelectedNodes.Add(&Node.Get());
	}

	FFrameNumber ThresholdFrames = (DuplicateThresholdSeconds * GetFocusedTickResolution()).FloorToFrame();
	KeyCollection->Update(FSequencerKeyCollectionSignature::FromNodesRecursive(SelectedNodes, ThresholdFrames));
}


void FSequencer::GetAllKeys(TUniquePtr<FSequencerKeyCollection>& KeyCollection, float DuplicateThresholdSeconds)
{
	if (!KeyCollection.IsValid())
	{
		KeyCollection.Reset(new FSequencerKeyCollection);
	}

	TArray<FSequencerDisplayNode*> AllNodes;
	for (TSharedRef<FSequencerDisplayNode> Node : NodeTree->GetAllNodes())
	{
		AllNodes.Add(&Node.Get());
	}

	FFrameNumber ThresholdFrames = (DuplicateThresholdSeconds * GetFocusedTickResolution()).FloorToFrame();
	KeyCollection->Update(FSequencerKeyCollectionSignature::FromNodesRecursive(AllNodes, ThresholdFrames));
}


void FSequencer::PopToSequenceInstance(FMovieSceneSequenceIDRef SequenceID)
{
	if( ActiveTemplateIDs.Num() > 1 )
	{
		// Pop until we find the movie scene to focus
		while( SequenceID != ActiveTemplateIDs.Last() )
		{
			ActiveTemplateIDs.Pop();
			ActiveTemplateStates.Pop();
		}

		check( ActiveTemplateIDs.Num() > 0 );
		UpdateSubSequenceData();

		// Pop out of any potentially locked cameras from the shot and toggle on camera cuts
		for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{		
			if (LevelVC && LevelVC->AllowsCinematicControl() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}

		ResetPerMovieSceneData();
		SequencerWidget->UpdateBreadcrumbs();

		OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());

		bNeedsEvaluate = true;
	}
}

void FSequencer::UpdateSubSequenceData()
{
	SubSequenceRange = TRange<FFrameNumber>::Empty();
	RootToLocalTransform = FMovieSceneSequenceTransform();

	// Find the parent sub section and set up the sub sequence range, if necessary
	if (ActiveTemplateIDs.Num() <= 1)
	{
		return;
	}

	const FMovieSceneSubSequenceData* SubSequenceData = RootTemplateInstance.GetHierarchy().FindSubData(ActiveTemplateIDs.Top());

	if (SubSequenceData)
	{
		SubSequenceRange = SubSequenceData->PlayRange.Value;
		RootToLocalTransform = SubSequenceData->RootToSequenceTransform;
	}
}

void FSequencer::RerunConstructionScripts()
{
	TSet<TWeakObjectPtr<AActor> > BoundActors;

	FMovieSceneRootEvaluationTemplateInstance& RootTemplate = GetEvaluationTemplate();
		
	UMovieSceneSequence* Sequence = RootTemplate.GetSequence(MovieSceneSequenceID::Root);

	GetConstructionScriptActors(Sequence->GetMovieScene(), MovieSceneSequenceID::Root, BoundActors);

	for (FMovieSceneSequenceIDRef SequenceID : RootTemplateInstance.GetThisFrameMetaData().ActiveSequences)
	{
		UMovieSceneSequence* SubSequence = RootTemplateInstance.GetSequence(SequenceID);
		if (SubSequence)
		{
			GetConstructionScriptActors(SubSequence->GetMovieScene(), SequenceID, BoundActors);
		}
	}

	for (TWeakObjectPtr<AActor> BoundActor : BoundActors)
	{
		if (BoundActor.IsValid())
		{
			BoundActor.Get()->RerunConstructionScripts();
		}
	}
}

void FSequencer::GetConstructionScriptActors(UMovieScene* MovieScene, FMovieSceneSequenceIDRef SequenceID, TSet<TWeakObjectPtr<AActor> >& BoundActors)
{
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetPossessable(Index).GetGuid();

		for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ThisGuid, SequenceID))
		{
			if (WeakObject.IsValid())
			{
				AActor* Actor = Cast<AActor>(WeakObject.Get());
	
				if (Actor)
				{
					UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
					if (Blueprint && Blueprint->bRunConstructionScriptInSequencer)
					{
						BoundActors.Add(Actor);
					}
				}
			}
		}
	}

	for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetSpawnable(Index).GetGuid();

		for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ThisGuid, SequenceID))
		{
			if (WeakObject.IsValid())
			{
				AActor* Actor = Cast<AActor>(WeakObject.Get());

				if (Actor)
				{
					UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
					if (Blueprint && Blueprint->bRunConstructionScriptInSequencer)
					{
						BoundActors.Add(Actor);
					}
				}
			}
		}
	}
}

void FSequencer::DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections)
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	bool bAnythingRemoved = false;

	FScopedTransaction DeleteSectionTransaction( NSLOCTEXT("Sequencer", "DeleteSection_Transaction", "Delete Section") );

	for (const auto Section : Sections)
	{
		if (!Section.IsValid() || Section->IsLocked())
		{
			continue;
		}

		// if this check fails then the section is outered to a type that doesnt know about the section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(Section->GetOuter());
		{
			Track->SetFlags(RF_Transactional);
			Track->Modify();
			Track->RemoveSection(*Section);
		}

		bAnythingRemoved = true;
	}

	if (bAnythingRemoved)
	{
		// Full refresh required just in case the last section was removed from any track.
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemRemoved );
	}

	Selection.EmptySelectedSections();
	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


void FSequencer::DeleteSelectedKeys()
{
	FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteSelectedKeys_Transaction", "Delete Selected Keys") );
	bool bAnythingRemoved = false;

	FSelectedKeysByChannel KeysByChannel(Selection.GetSelectedKeys().Array());
	TSet<UMovieSceneSection*> ModifiedSections;

	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* Channel = ChannelInfo.Channel.Get();
		if (Channel)
		{
			if (!ModifiedSections.Contains(ChannelInfo.OwningSection))
			{
				ChannelInfo.OwningSection->Modify();
				ModifiedSections.Add(ChannelInfo.OwningSection);
			}

			Channel->DeleteKeys(ChannelInfo.KeyHandles);
			bAnythingRemoved = true;
		}
	}

	if (bAnythingRemoved)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}

	Selection.EmptySelectedKeys();
	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


void FSequencer::SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	if (SelectedKeysArray.Num() == 0)
	{
		return;
	}

	FScopedTransaction SetInterpTangentModeTransaction(NSLOCTEXT("Sequencer", "SetInterpTangentMode_Transaction", "Set Interpolation and Tangent Mode"));
	bool bAnythingChanged = false;

	FSelectedKeysByChannel KeysByChannel(SelectedKeysArray);
	TSet<UMovieSceneSection*> ModifiedSections;

	const FName FloatChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetFName();

	// @todo: sequencer-timecode: move this float-specific logic elsewhere to make it extensible for any channel type
	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* ChannelPtr = ChannelInfo.Channel.Get();
		if (ChannelInfo.Channel.GetChannelTypeName() == FloatChannelTypeName && ChannelPtr)
		{
			if (!ModifiedSections.Contains(ChannelInfo.OwningSection))
			{
				ChannelInfo.OwningSection->Modify();
				ModifiedSections.Add(ChannelInfo.OwningSection);
			}

			FMovieSceneFloatChannel* Channel = static_cast<FMovieSceneFloatChannel*>(ChannelPtr);
			TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();

			TArrayView<FMovieSceneFloatValue> Values = ChannelData.GetValues();

			for (FKeyHandle Handle : ChannelInfo.KeyHandles)
			{
				const int32 KeyIndex = ChannelData.GetIndex(Handle);
				if (KeyIndex != INDEX_NONE)
				{
					Values[KeyIndex].InterpMode = InterpMode;
					Values[KeyIndex].TangentMode = TangentMode;
					bAnythingChanged = true;
				}
			}

			Channel->AutoSetTangents();
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

void FSequencer::ToggleInterpTangentWeightMode()
{
	// @todo: sequencer-timecode: move this float-specific logic elsewhere to make it extensible for any channel type

	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	if (SelectedKeysArray.Num() == 0)
	{
		return;
	}

	FScopedTransaction SetInterpTangentWeightModeTransaction(NSLOCTEXT("Sequencer", "ToggleInterpTangentWeightMode_Transaction", "Toggle Tangent Weight Mode"));
	bool bAnythingChanged = false;

	FSelectedKeysByChannel KeysByChannel(SelectedKeysArray);
	TSet<UMovieSceneSection*> ModifiedSections;

	const FName FloatChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetFName();

	// Remove all tangent weights unless we find a compatible key that does not have weights yet
	ERichCurveTangentWeightMode WeightModeToApply = RCTWM_WeightedNone;

	// First off iterate all the current keys and find any that don't have weights
	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* ChannelPtr = ChannelInfo.Channel.Get();
		if (ChannelInfo.Channel.GetChannelTypeName() == FloatChannelTypeName && ChannelPtr)
		{
			FMovieSceneFloatChannel* Channel = static_cast<FMovieSceneFloatChannel*>(ChannelPtr);
			TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();

			TArrayView<FMovieSceneFloatValue> Values = ChannelData.GetValues();

			for (FKeyHandle Handle : ChannelInfo.KeyHandles)
			{
				const int32 KeyIndex = ChannelData.GetIndex(Handle);
				if (KeyIndex != INDEX_NONE && Values[KeyIndex].InterpMode == RCIM_Cubic && Values[KeyIndex].Tangent.TangentWeightMode == RCTWM_WeightedNone)
				{
					WeightModeToApply = RCTWM_WeightedBoth;
					goto assign_weights;
				}
			}
		}
	}

assign_weights:

	// Assign the new weight mode for all cubic keys
	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* ChannelPtr = ChannelInfo.Channel.Get();
		if (ChannelInfo.Channel.GetChannelTypeName() == FloatChannelTypeName && ChannelPtr)
		{
			if (!ModifiedSections.Contains(ChannelInfo.OwningSection))
			{
				ChannelInfo.OwningSection->Modify();
				ModifiedSections.Add(ChannelInfo.OwningSection);
			}

			FMovieSceneFloatChannel* Channel = static_cast<FMovieSceneFloatChannel*>(ChannelPtr);
			TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();

			TArrayView<FMovieSceneFloatValue> Values = ChannelData.GetValues();

			for (FKeyHandle Handle : ChannelInfo.KeyHandles)
			{
				const int32 KeyIndex = ChannelData.GetIndex(Handle);
				if (KeyIndex != INDEX_NONE && Values[KeyIndex].InterpMode == RCIM_Cubic)
				{
					Values[KeyIndex].Tangent.TangentWeightMode = WeightModeToApply;
					bAnythingChanged = true;
				}
			}

			Channel->AutoSetTangents();
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

void FSequencer::SnapToFrame()
{
	FScopedTransaction SnapToFrameTransaction(NSLOCTEXT("Sequencer", "SnapToFrame_Transaction", "Snap Selected Keys to Frame"));
	bool bAnythingChanged = false;

	FSelectedKeysByChannel KeysByChannel(Selection.GetSelectedKeys().Array());
	TSet<UMovieSceneSection*> ModifiedSections;

	TArray<FFrameNumber> KeyTimesScratch;
	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* Channel = ChannelInfo.Channel.Get();
		if (Channel)
		{
			if (!ModifiedSections.Contains(ChannelInfo.OwningSection))
			{
				ChannelInfo.OwningSection->Modify();
				ModifiedSections.Add(ChannelInfo.OwningSection);
			}

			const int32 NumKeys = ChannelInfo.KeyHandles.Num();
			KeyTimesScratch.Reset(NumKeys);
			KeyTimesScratch.SetNum(NumKeys);

			Channel->GetKeyTimes(ChannelInfo.KeyHandles, KeyTimesScratch);

			FFrameRate TickResolution  = GetFocusedTickResolution();
			FFrameRate DisplayRate     = GetFocusedDisplayRate();

			for (FFrameNumber& Time : KeyTimesScratch)
			{
				// Convert to frame
				FFrameNumber PlayFrame    = FFrameRate::TransformTime(Time,      TickResolution, DisplayRate).RoundToFrame();
				FFrameNumber SnappedFrame = FFrameRate::TransformTime(PlayFrame, DisplayRate, TickResolution).RoundToFrame();

				Time = SnappedFrame;
			}

			Channel->SetKeyTimes(ChannelInfo.KeyHandles, KeyTimesScratch);
			bAnythingChanged = true;
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}


bool FSequencer::CanSnapToFrame() const
{
	const bool bKeysSelected = Selection.GetSelectedKeys().Num() > 0;

	return bKeysSelected;
}

void FSequencer::TransformSelectedKeysAndSections(FFrameTime InDeltaTime, float InScale)
{
	FScopedTransaction TransformKeysAndSectionsTransaction(NSLOCTEXT("Sequencer", "TransformKeysandSections_Transaction", "Transform Keys and Sections"));
	bool bAnythingChanged = false;

	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	TArray<TWeakObjectPtr<UMovieSceneSection>> SelectedSectionsArray = Selection.GetSelectedSections().Array();

	const FFrameTime OriginTime = GetLocalTime().Time;

	FSelectedKeysByChannel KeysByChannel(SelectedKeysArray);
	TMap<UMovieSceneSection*, TRange<FFrameNumber>> SectionToNewBounds;

	TArray<FFrameNumber> KeyTimesScratch;
	if (InScale != 0.f)
	{
		// Dilate the keys
		for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
		{
			FMovieSceneChannel* Channel = ChannelInfo.Channel.Get();
			if (Channel)
			{
				const int32 NumKeys = ChannelInfo.KeyHandles.Num();
				KeyTimesScratch.Reset(NumKeys);
				KeyTimesScratch.SetNum(NumKeys);

				// Populate the key times scratch buffer with the times for these handles
				Channel->GetKeyTimes(ChannelInfo.KeyHandles, KeyTimesScratch);

				// We have to find the lowest key time and the highest key time. They're added based on selection order so we can't rely on their order in the array.
				FFrameTime LowestFrameTime = KeyTimesScratch[0];
				FFrameTime HighestFrameTime = KeyTimesScratch[0];

				// Perform the transformation
				for (FFrameNumber& Time : KeyTimesScratch)
				{
					FFrameTime KeyTime = Time;
					Time = (OriginTime + InDeltaTime + (KeyTime - OriginTime) * InScale).FloorToFrame();

					if (Time < LowestFrameTime)
					{
						LowestFrameTime = Time;
					}

					if (Time > HighestFrameTime)
					{
						HighestFrameTime = Time;
					}
				}

				TRange<FFrameNumber>* NewSectionBounds = SectionToNewBounds.Find(ChannelInfo.OwningSection);
				if (!NewSectionBounds)
				{
					// Call Modify on the owning section before we call SetKeyTimes so that our section bounds/key times stay in sync.
					ChannelInfo.OwningSection->Modify();
					NewSectionBounds = &SectionToNewBounds.Add(ChannelInfo.OwningSection, ChannelInfo.OwningSection->GetRange());
				}


				// Expand the range by ensuring the new range contains the range our keys are in. We add one because the highest time is exclusive
				// for sections, but HighestFrameTime is measuring only the key's time.
				*NewSectionBounds = TRange<FFrameNumber>::Hull(*NewSectionBounds, TRange<FFrameNumber>(LowestFrameTime.GetFrame(), HighestFrameTime.GetFrame() + 1));

				// Apply the new, transformed key times
				Channel->SetKeyTimes(ChannelInfo.KeyHandles, KeyTimesScratch);
				bAnythingChanged = true;
			}
		}

		// Dilate the sections
		for (TWeakObjectPtr<UMovieSceneSection> Section : SelectedSectionsArray)
		{
			if (!Section.IsValid())
			{
				continue;
			}

			TRangeBound<FFrameNumber> LowerBound = Section->GetRange().GetLowerBound();
			TRangeBound<FFrameNumber> UpperBound = Section->GetRange().GetUpperBound();

			if (Section->HasStartFrame())
			{
				FFrameTime StartTime = Section->GetInclusiveStartFrame();
				FFrameNumber StartFrame = (OriginTime + InDeltaTime + (StartTime - OriginTime) * InScale).FloorToFrame();
				LowerBound = TRangeBound<FFrameNumber>::Inclusive(StartFrame);
			}

			if (Section->HasEndFrame())
			{
				FFrameTime EndTime = Section->GetExclusiveEndFrame();
				FFrameNumber EndFrame = (OriginTime + InDeltaTime + (EndTime - OriginTime) * InScale).FloorToFrame();
				UpperBound = TRangeBound<FFrameNumber>::Exclusive(EndFrame);
			}

			TRange<FFrameNumber>* NewSectionBounds = SectionToNewBounds.Find(Section.Get());
			if (!NewSectionBounds)
			{
				NewSectionBounds = &SectionToNewBounds.Add( Section.Get(), TRange<FFrameNumber>(LowerBound, UpperBound) );
			}

			// If keys have already modified the section, we're applying the same modification to the section so we can
			// overwrite the (possibly) existing bound, so it's okay to just overwrite the range without a TRange::Hull.
			*NewSectionBounds = TRange<FFrameNumber>(LowerBound, UpperBound);
			bAnythingChanged = true;
		}
	}
	
	// Remove any null sections so we don't need a null check inside the loop.
	SectionToNewBounds.Remove(nullptr);
	for (TTuple<UMovieSceneSection*, TRange<FFrameNumber>>& Pair : SectionToNewBounds)
	{
		// Set the range of each section that has been modified to their new bounds.
		Pair.Key->SetRange(Pair.Value);
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

void FSequencer::TranslateSelectedKeysAndSections(bool bTranslateLeft)
{
	int32 Shift = bTranslateLeft ? -1 : 1;
	FFrameTime Delta = FQualifiedFrameTime(Shift, GetFocusedDisplayRate()).ConvertTo(GetFocusedTickResolution());
	TransformSelectedKeysAndSections(Delta, 1.f);
}

void FSequencer::BakeTransform()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	FScopedTransaction BakeTransform(NSLOCTEXT("Sequencer", "BakeTransform", "Bake Transform"));

	FocusedMovieScene->Modify();

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() != ESequencerNode::Object)
		{
			continue;
		}

		FFrameTime ResetTime = PlayPosition.GetCurrentPosition();
		auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

		FGuid Guid = ObjectBindingNode->GetObjectBinding();
		for (auto RuntimeObject : FindBoundObjects(Guid, ActiveTemplateIDs.Top()) )
		{
			AActor* Actor = Cast<AActor>(RuntimeObject.Get());

			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(RuntimeObject.Get());

			FVector Location = Actor->GetActorLocation();
			FVector Rotation = Actor->GetActorRotation().Euler();
			FVector Scale = Actor->GetActorScale();

			// Cache transforms
			TArray<FVector> Locations;
			TArray<FRotator> Rotations;
			TArray<FFrameNumber> KeyTimes;

			FFrameRate   Resolution  = FocusedMovieScene->GetTickResolution();
			FFrameRate   SnapRate    = FocusedMovieScene->GetDisplayRate();

			FFrameNumber InFrame     = MovieScene::DiscreteInclusiveLower(GetPlaybackRange());
			FFrameNumber OutFrame    = MovieScene::DiscreteExclusiveUpper(GetPlaybackRange());

			FFrameTime Interval = FFrameRate::TransformTime(1, SnapRate, Resolution);
			for (FFrameTime EvalTime = InFrame; EvalTime < OutFrame; EvalTime += Interval)
			{
				FFrameNumber KeyTime = FFrameRate::Snap(EvalTime, Resolution, SnapRate).FloorToFrame();
				FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(KeyTime * RootToLocalTransform.Inverse());
				EvaluateInternal(Range);

				if (CameraComponent)
				{
					FTransform AdditiveOffset;
					float AdditiveFOVOffset;
					CameraComponent->GetAdditiveOffset(AdditiveOffset, AdditiveFOVOffset);

					FTransform Transform(Actor->GetActorRotation(), Actor->GetActorLocation());
					FTransform TransformWithAdditiveOffset = Transform * AdditiveOffset;
					FVector LocalTranslation = TransformWithAdditiveOffset.GetTranslation();
					FRotator LocalRotation = TransformWithAdditiveOffset.GetRotation().Rotator();

					Locations.Add(LocalTranslation);
					Rotations.Add(LocalRotation);
				}
				else
				{
					Locations.Add(Actor->GetActorLocation());
					Rotations.Add(Actor->GetActorRotation());
				}

				KeyTimes.Add(KeyTime);
			}

			// Delete any attach tracks
			// cbb: this only operates on a single attach section.
			AActor* AttachParentActor = nullptr;
			UMovieScene3DAttachTrack* AttachTrack = Cast<UMovieScene3DAttachTrack>(FocusedMovieScene->FindTrack(UMovieScene3DAttachTrack::StaticClass(), Guid));
			if (AttachTrack)
			{
				for (auto AttachSection : AttachTrack->GetAllSections())
				{
					FMovieSceneObjectBindingID ConstraintBindingID = (Cast<UMovieScene3DAttachSection>(AttachSection))->GetConstraintBindingID();
					for (auto ParentObject : FindBoundObjects(ConstraintBindingID.GetGuid(), ConstraintBindingID.GetSequenceID()) )
					{
						AttachParentActor = Cast<AActor>(ParentObject.Get());
						break;
					}
				}

				FocusedMovieScene->RemoveTrack(*AttachTrack);
			}

			// Delete any transform tracks
			UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(FocusedMovieScene->FindTrack(UMovieScene3DTransformTrack::StaticClass(), Guid, "Transform"));
			if (TransformTrack)
			{
				FocusedMovieScene->RemoveTrack(*TransformTrack);
			}

			// Delete any camera anim tracks
			UMovieSceneCameraAnimTrack* CameraAnimTrack = Cast<UMovieSceneCameraAnimTrack>(FocusedMovieScene->FindTrack(UMovieSceneCameraAnimTrack::StaticClass(), Guid));
			if (CameraAnimTrack)
			{
				FocusedMovieScene->RemoveTrack(*CameraAnimTrack);
			}

			// Delete any camera shake tracks
			UMovieSceneCameraShakeTrack* CameraShakeTrack = Cast<UMovieSceneCameraShakeTrack>(FocusedMovieScene->FindTrack(UMovieSceneCameraShakeTrack::StaticClass(), Guid));
			if (CameraShakeTrack)
			{
				FocusedMovieScene->RemoveTrack(*CameraShakeTrack);
			}

			// Reset position
			EvaluateInternal(PlayPosition.JumpTo(ResetTime));

			// Always detach from any existing parent
			Actor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);

			// If there was an attach track that was the parent, detach and attach to that actor's parent if it exists
			FTransform ParentInverseTransform;
			ParentInverseTransform.SetIdentity();
			if (AttachParentActor)
			{
				AActor* ExistingParentActor = AttachParentActor->GetAttachParentActor();
				if (ExistingParentActor)
				{
					Actor->AttachToActor(ExistingParentActor, FAttachmentTransformRules::KeepRelativeTransform);
					ParentInverseTransform = ExistingParentActor->GetActorTransform().Inverse();
				}
			}

			// Create new transform track and section
			TransformTrack = Cast<UMovieScene3DTransformTrack>(FocusedMovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), Guid));

			if (TransformTrack)
			{
				UMovieScene3DTransformSection* TransformSection = CastChecked<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
				TransformTrack->AddSection(*TransformSection);
			
				TransformSection->SetRange(TRange<FFrameNumber>::All());

				TArrayView<FMovieSceneFloatChannel*> FloatChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
				FloatChannels[0]->SetDefault(Location.X);
				FloatChannels[1]->SetDefault(Location.Y);
				FloatChannels[2]->SetDefault(Location.Z);
				FloatChannels[3]->SetDefault(Rotation.X);
				FloatChannels[4]->SetDefault(Rotation.Y);
				FloatChannels[5]->SetDefault(Rotation.Z);
				FloatChannels[6]->SetDefault(Scale.X);
				FloatChannels[7]->SetDefault(Scale.Y);
				FloatChannels[8]->SetDefault(Scale.Z);

				for (int32 Counter = 0; Counter < KeyTimes.Num(); ++Counter)
				{
					FFrameNumber KeyTime = KeyTimes[Counter];

					FTransform Transform(Rotations[Counter], Locations[Counter]);
					FTransform LocalTransform = ParentInverseTransform * Transform;
					FVector LocalTranslation = LocalTransform.GetTranslation();
					FVector LocalRotation = LocalTransform.GetRotation().Euler();

					FloatChannels[0]->AddLinearKey(KeyTime, LocalTranslation.X);
					FloatChannels[1]->AddLinearKey(KeyTime, LocalTranslation.Y);
					FloatChannels[2]->AddLinearKey(KeyTime, LocalTranslation.Z);
					FloatChannels[3]->AddLinearKey(KeyTime, LocalRotation.X);
					FloatChannels[4]->AddLinearKey(KeyTime, LocalRotation.Y);
					FloatChannels[5]->AddLinearKey(KeyTime, LocalRotation.Z);
					FloatChannels[6]->AddLinearKey(KeyTime, Scale.X);
					FloatChannels[7]->AddLinearKey(KeyTime, Scale.Y);
					FloatChannels[8]->AddLinearKey(KeyTime, Scale.Z);
				}
			}
		}
	}
	
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
}


void FSequencer::SyncSectionsUsingSourceTimecode()
{
	FScopedTransaction SyncSectionsUsingSourceTimecodeTransaction( LOCTEXT("SyncSectionsUsingSourceTimecode_Transaction", "Sync Sections Using Source Timecode") );
	bool bAnythingChanged = false;

	TArray<UMovieSceneSection*> Sections;
	for (auto Section : GetSelection().GetSelectedSections())
	{
		if (Section.IsValid() && Section->HasStartFrame())
		{
			Sections.Add(Section.Get());
		}
	}

	if (Sections.Num() < 2) 
	{
		return;
	}

	const UMovieSceneSection* FirstSection = Sections[0];
	FFrameNumber FirstSectionSourceTimecode = FirstSection->TimecodeSource.Timecode.ToFrameNumber(GetFocusedTickResolution());
	FFrameNumber FirstSectionCurrentStartFrame = FirstSection->GetInclusiveStartFrame();// - FirstSection->TimecodeSource.DeltaFrame;
	Sections.RemoveAt(0);

	for (auto Section : Sections)
	{
		if (Section->HasStartFrame())
		{
			FFrameNumber SectionSourceTimecode = Section->TimecodeSource.Timecode.ToFrameNumber(GetFocusedTickResolution());
			FFrameNumber SectionCurrentStartFrame = Section->GetInclusiveStartFrame();// - Section->TimecodeSource.DeltaFrame;

			FFrameNumber TimecodeDelta = SectionSourceTimecode - FirstSectionSourceTimecode;
			FFrameNumber CurrentDelta = SectionCurrentStartFrame - FirstSectionCurrentStartFrame;
			FFrameNumber Delta = -CurrentDelta + TimecodeDelta;

			Section->MoveSection(Delta);

			bAnythingChanged = bAnythingChanged || (Delta.Value != 0);
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}


void FSequencer::OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors )
{
	AddActors(Actors);
}


void FSequencer::NotifyMovieSceneDataChangedInternal()
{
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::Unknown );
}


void FSequencer::NotifyMovieSceneDataChanged( EMovieSceneDataChangeType DataChangeType )
{
	if (!GetFocusedMovieSceneSequence()->GetMovieScene())
	{
		if (RootSequence.IsValid())
		{
			ResetToNewRootSequence(*RootSequence.Get());
		}
		else
		{
			UE_LOG(LogSequencer, Error, TEXT("Fatal error, focused movie scene no longer valid and there is no root sequence to default to."));
		}
	}

	if ( DataChangeType == EMovieSceneDataChangeType::ActiveMovieSceneChanged ||
		DataChangeType == EMovieSceneDataChangeType::Unknown )
	{
		LabelManager.SetMovieScene( GetFocusedMovieSceneSequence()->GetMovieScene() );
	}

	StoredPlaybackState = GetPlaybackStatus();

	if ( DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved ||
		DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemsChanged ||
		DataChangeType == EMovieSceneDataChangeType::Unknown )
	{
		// When structure items are removed, or we don't know what may have changed, refresh the tree and instances immediately so that the data
		// is in a consistent state when the UI is updated during the next tick.
		SetPlaybackStatus( EMovieScenePlayerStatus::Stopped );
		SelectionPreview.Empty();
		RefreshTree();
		SetPlaybackStatus( StoredPlaybackState );
	}
	else if (DataChangeType == EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately)
	{
		// Evaluate now
		EvaluateInternal(PlayPosition.GetCurrentPositionAsRange());
	}
	else if (DataChangeType == EMovieSceneDataChangeType::RefreshAllImmediately)
	{
		RefreshTree();

		// Evaluate now
		EvaluateInternal(PlayPosition.GetCurrentPositionAsRange());
	}
	else
	{
		if ( DataChangeType != EMovieSceneDataChangeType::TrackValueChanged )
		{
			// All changes types except for track value changes require refreshing the outliner tree.
			SetPlaybackStatus( EMovieScenePlayerStatus::Stopped );
			bNeedTreeRefresh = true;
		}
	}

	if (DataChangeType == EMovieSceneDataChangeType::TrackValueChanged || 
		DataChangeType == EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately || 
		DataChangeType == EMovieSceneDataChangeType::Unknown ||
		DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved)
	{
		FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(GLevelEditorModeTools().GetActiveMode(FSequencerEdMode::EM_SequencerMode));
		if (SequencerEdMode != nullptr)
		{
			SequencerEdMode->CleanUpMeshTrails();
		}
	}

	bNeedsEvaluate = true;
	State.ClearObjectCaches(*this);

	UpdatePlaybackRange();
	OnMovieSceneDataChangedDelegate.Broadcast(DataChangeType);
}

void FSequencer::RefreshTree()
{
	SequencerWidget->UpdateLayoutTree();
	bNeedTreeRefresh = false;
}

FAnimatedRange FSequencer::GetViewRange() const
{
	FAnimatedRange AnimatedRange(FMath::Lerp(LastViewRange.GetLowerBoundValue(), TargetViewRange.GetLowerBoundValue(), ZoomCurve.GetLerp()),
		FMath::Lerp(LastViewRange.GetUpperBoundValue(), TargetViewRange.GetUpperBoundValue(), ZoomCurve.GetLerp()));

	if (ZoomAnimation.IsPlaying())
	{
		AnimatedRange.AnimationTarget = TargetViewRange;
	}

	return AnimatedRange;
}


FAnimatedRange FSequencer::GetClampRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().GetWorkingRange();
}


void FSequencer::SetClampRange(TRange<double> InNewClampRange)
{
	FMovieSceneEditorData& EditorData = GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData();
	EditorData.WorkStart = InNewClampRange.GetLowerBoundValue();
	EditorData.WorkEnd   = InNewClampRange.GetUpperBoundValue();
}


TOptional<TRange<FFrameNumber>> FSequencer::GetSubSequenceRange() const
{
	if (Settings->ShouldEvaluateSubSequencesInIsolation() || ActiveTemplateIDs.Num() == 1)
	{
		return TOptional<TRange<FFrameNumber>>();
	}
	return SubSequenceRange;
}


TRange<FFrameNumber> FSequencer::GetSelectionRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetSelectionRange();
}


void FSequencer::SetSelectionRange(TRange<FFrameNumber> Range)
{
	const FScopedTransaction Transaction(LOCTEXT("SetSelectionRange_Transaction", "Set Selection Range"));
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	FocusedMovieScene->Modify();
	FocusedMovieScene->SetSelectionRange(Range);
}


void FSequencer::SetSelectionRangeEnd()
{
	const FFrameNumber LocalTime = GetLocalTime().Time.FrameNumber;

	if (GetSelectionRange().GetLowerBoundValue() >= LocalTime)
	{
		SetSelectionRange(TRange<FFrameNumber>(LocalTime));
	}
	else
	{
		SetSelectionRange(TRange<FFrameNumber>(GetSelectionRange().GetLowerBound(), LocalTime));
	}
}


void FSequencer::SetSelectionRangeStart()
{
	const FFrameNumber LocalTime = GetLocalTime().Time.FrameNumber;

	if (GetSelectionRange().GetUpperBoundValue() <= LocalTime)
	{
		SetSelectionRange(TRange<FFrameNumber>(LocalTime));
	}
	else
	{
		SetSelectionRange(TRange<FFrameNumber>(LocalTime, GetSelectionRange().GetUpperBound()));
	}
}


void FSequencer::SelectInSelectionRange(const TSharedRef<FSequencerDisplayNode>& DisplayNode, const TRange<FFrameNumber>& SelectionRange, bool bSelectKeys, bool bSelectSections)
{
	if (DisplayNode->GetType() == ESequencerNode::Track)
	{
		if (bSelectKeys)
		{
			TArray<FKeyHandle> HandlesScratch;

			TSet<TSharedPtr<IKeyArea>> KeyAreas;
			SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);

			for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
			{
				UMovieSceneSection* Section = KeyArea->GetOwningSection();

				if (Section)
				{
					HandlesScratch.Reset();
					KeyArea->GetKeyHandles(HandlesScratch, SelectionRange);

					for (int32 Index = 0; Index < HandlesScratch.Num(); ++Index)
					{
						Selection.AddToSelection(FSequencerSelectedKey(*Section, KeyArea, HandlesScratch[Index]));
					}
				}
			}
		}

		if (bSelectSections)
		{
			TSet<TWeakObjectPtr<UMovieSceneSection>> OutSections;
			SequencerHelpers::GetAllSections(DisplayNode, OutSections);

			for (auto Section : OutSections)
			{
				if (Section.IsValid() && Section->GetRange().Overlaps(SelectionRange))
				{
					Selection.AddToSelection(Section.Get());
				}
			}
		}
	}

	for (const auto& ChildNode : DisplayNode->GetChildNodes())
	{
		SelectInSelectionRange(ChildNode, SelectionRange, bSelectKeys, bSelectSections);
	}
}

void FSequencer::ResetSelectionRange()
{
	SetSelectionRange(TRange<FFrameNumber>::Empty());
}

void FSequencer::SelectInSelectionRange(bool bSelectKeys, bool bSelectSections)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	TRange<FFrameNumber> SelectionRange = MovieScene->GetSelectionRange();

	Selection.Empty();

	for (const TSharedRef<FSequencerDisplayNode>& DisplayNode : NodeTree->GetRootNodes())
	{
		SelectInSelectionRange(DisplayNode, SelectionRange, bSelectKeys, bSelectSections);
	}
}


TRange<FFrameNumber> FSequencer::GetPlaybackRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
}


void FSequencer::SetPlaybackRange(TRange<FFrameNumber> Range)
{
	if (ensure(Range.HasLowerBound() && Range.HasUpperBound()))
	{
		if (!IsPlaybackRangeLocked())
		{
			const FScopedTransaction Transaction(LOCTEXT("SetPlaybackRange_Transaction", "Set Playback Range"));

			UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

			FocusedMovieScene->SetPlaybackRange(Range);

			bNeedsEvaluate = true;
			NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
		}
	}
}

UMovieSceneSection* FSequencer::FindNextOrPreviousShot(UMovieSceneSequence* Sequence, FFrameNumber SearchFromTime, const bool bNextShot) const
{
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	UMovieSceneTrack* CinematicShotTrack = OwnerMovieScene->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass());
	if (!CinematicShotTrack)
	{
		return nullptr;
	}

	FFrameNumber MinTime = TNumericLimits<FFrameNumber>::Max();

	TMap<FFrameNumber, int32> StartTimeMap;
	for (int32 SectionIndex = 0; SectionIndex < CinematicShotTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneSection* ShotSection = CinematicShotTrack->GetAllSections()[SectionIndex];

		if (ShotSection && ShotSection->HasStartFrame())
		{
			StartTimeMap.Add(ShotSection->GetInclusiveStartFrame(), SectionIndex);
		}
	}

	StartTimeMap.KeySort(TLess<FFrameNumber>());

	int32 MinShotIndex = -1;
	for (auto StartTimeIt = StartTimeMap.CreateIterator(); StartTimeIt; ++StartTimeIt)
	{
		FFrameNumber StartTime = StartTimeIt->Key;
		if (bNextShot)
		{
			if (StartTime > SearchFromTime)
			{
				FFrameNumber DiffTime = FMath::Abs(StartTime - SearchFromTime);
				if (DiffTime < MinTime)
				{
					MinTime = DiffTime;
					MinShotIndex = StartTimeIt->Value;
				}
			}
		}
		else
		{
			if (SearchFromTime >= StartTime)
			{
				FFrameNumber DiffTime = FMath::Abs(StartTime - SearchFromTime);
				if (DiffTime < MinTime)
				{
					MinTime = DiffTime;
					MinShotIndex = StartTimeIt->Value;
				}
			}
		}
	}

	int32 TargetShotIndex = -1;

	if (bNextShot)
	{
		TargetShotIndex = MinShotIndex;
	}
	else
	{
		int32 PreviousShotIndex = -1;
		for (auto StartTimeIt = StartTimeMap.CreateIterator(); StartTimeIt; ++StartTimeIt)
		{
			if (StartTimeIt->Value == MinShotIndex)
			{
				if (PreviousShotIndex != -1)
				{
					TargetShotIndex = PreviousShotIndex;
				}
				break;
			}
			PreviousShotIndex = StartTimeIt->Value;
		}
	}

	if (TargetShotIndex == -1)
	{
		return nullptr;
	}	

	return CinematicShotTrack->GetAllSections()[TargetShotIndex];
}

void FSequencer::SetSelectionRangeToShot(const bool bNextShot)
{
	UMovieSceneSection* TargetShotSection = FindNextOrPreviousShot(GetFocusedMovieSceneSequence(), GetLocalTime().Time.FloorToFrame(), bNextShot);

	TRange<FFrameNumber> NewSelectionRange = TargetShotSection ? TargetShotSection->GetRange() : TRange<FFrameNumber>::All();
	if (NewSelectionRange.GetLowerBound().IsClosed() && NewSelectionRange.GetUpperBound().IsClosed())
	{
		SetSelectionRange(NewSelectionRange);
	}
}

void FSequencer::SetPlaybackRangeToAllShots()
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	UMovieSceneTrack* CinematicShotTrack = OwnerMovieScene->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass());
	if (!CinematicShotTrack || CinematicShotTrack->GetAllSections().Num() == 0)
	{
		return;
	}

	TRange<FFrameNumber> NewRange = CinematicShotTrack->GetAllSections()[0]->GetRange();

	for (UMovieSceneSection* ShotSection : CinematicShotTrack->GetAllSections())
	{
		if (ShotSection && ShotSection->HasStartFrame() && ShotSection->HasEndFrame())
		{
			NewRange = TRange<FFrameNumber>::Hull(ShotSection->GetRange(), NewRange);
		}
	}

	SetPlaybackRange(NewRange);
}

bool FSequencer::IsPlaybackRangeLocked() const
{
	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSceneSequence != nullptr)
	{
		UMovieScene* MovieScene = FocusedMovieSceneSequence->GetMovieScene();

		if (MovieScene->IsReadOnly())
		{
			return true;
		}
	
		return MovieScene->IsPlaybackRangeLocked();
	}

	return false;
}

void FSequencer::TogglePlaybackRangeLocked()
{
	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	if ( FocusedMovieSceneSequence != nullptr )
	{
		UMovieScene* MovieScene = FocusedMovieSceneSequence->GetMovieScene();

		if (MovieScene->IsReadOnly())
		{
			return;
		}

		FScopedTransaction TogglePlaybackRangeLockTransaction( NSLOCTEXT( "Sequencer", "TogglePlaybackRangeLocked", "Toggle playback range lock" ) );
		MovieScene->Modify();
		MovieScene->SetPlaybackRangeLocked( !MovieScene->IsPlaybackRangeLocked() );
	}
}

void FSequencer::ResetViewRange()
{
	TRange<double> PlayRangeSeconds = GetPlaybackRange() / GetFocusedTickResolution();
	const double OutputViewSize = PlayRangeSeconds.Size<double>();
	const double OutputChange = OutputViewSize * 0.1f;

	if (OutputChange > 0)
	{
		PlayRangeSeconds = MovieScene::ExpandRange(PlayRangeSeconds, OutputChange);

		SetClampRange(PlayRangeSeconds);
		SetViewRange(PlayRangeSeconds, EViewRangeInterpolation::Animated);
	}
}


void FSequencer::ZoomViewRange(float InZoomDelta)
{
	float LocalViewRangeMax = TargetViewRange.GetUpperBoundValue();
	float LocalViewRangeMin = TargetViewRange.GetLowerBoundValue();

	const double CurrentTime = GetLocalTime().AsSeconds();
	const double OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const double OutputChange = OutputViewSize * InZoomDelta;

	float CurrentPositionFraction = (CurrentTime - LocalViewRangeMin) / OutputViewSize;

	double NewViewOutputMin = LocalViewRangeMin - (OutputChange * CurrentPositionFraction);
	double NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - CurrentPositionFraction));

	if (NewViewOutputMin < NewViewOutputMax)
	{
		SetViewRange(TRange<double>(NewViewOutputMin, NewViewOutputMax), EViewRangeInterpolation::Animated);
	}
}


void FSequencer::ZoomInViewRange()
{
	ZoomViewRange(-0.1f);
}


void FSequencer::ZoomOutViewRange()
{
	ZoomViewRange(0.1f);
}

void FSequencer::UpdatePlaybackRange()
{
	if (Settings->ShouldKeepPlayRangeInSectionBounds())
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		TArray<UMovieSceneSection*> AllSections = FocusedMovieScene->GetAllSections();

		if (AllSections.Num() > 0 && !IsPlaybackRangeLocked())
		{
			TRange<FFrameNumber> NewBounds = TRange<FFrameNumber>::Empty();
			for (UMovieSceneSection* Section : AllSections)
			{
				NewBounds = TRange<FFrameNumber>::Hull(Section->ComputeEffectiveRange(), NewBounds);
			}

			// When the playback range is determined by the section bounds, don't mark the change in the playback range otherwise the scene will be marked dirty
			if (!NewBounds.IsDegenerate())
			{
				const bool bAlwaysMarkDirty = false;
				FocusedMovieScene->SetPlaybackRange(NewBounds, bAlwaysMarkDirty);
			}
		}
	}
}


EAutoChangeMode FSequencer::GetAutoChangeMode() const 
{
	return Settings->GetAutoChangeMode();
}


void FSequencer::SetAutoChangeMode(EAutoChangeMode AutoChangeMode)
{
	Settings->SetAutoChangeMode(AutoChangeMode);
}


EAllowEditsMode FSequencer::GetAllowEditsMode() const 
{
	return Settings->GetAllowEditsMode();
}


void FSequencer::SetAllowEditsMode(EAllowEditsMode AllowEditsMode)
{
	Settings->SetAllowEditsMode(AllowEditsMode);
}


EKeyGroupMode FSequencer::GetKeyGroupMode() const
{
	return Settings->GetKeyGroupMode();
}


void FSequencer::SetKeyGroupMode(EKeyGroupMode Mode)
{
	Settings->SetKeyGroupMode(Mode);
}


bool FSequencer::GetKeyInterpPropertiesOnly() const 
{
	return Settings->GetKeyInterpPropertiesOnly();
}


void FSequencer::SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) 
{
	Settings->SetKeyInterpPropertiesOnly(bKeyInterpPropertiesOnly);
}


EMovieSceneKeyInterpolation FSequencer::GetKeyInterpolation() const
{
	return Settings->GetKeyInterpolation();
}


void FSequencer::SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation)
{
	Settings->SetKeyInterpolation(InKeyInterpolation);
}


bool FSequencer::GetInfiniteKeyAreas() const
{
	return Settings->GetInfiniteKeyAreas();
}


void FSequencer::SetInfiniteKeyAreas(bool bInfiniteKeyAreas)
{
	Settings->SetInfiniteKeyAreas(bInfiniteKeyAreas);
}


bool FSequencer::GetAutoSetTrackDefaults() const
{
	return Settings->GetAutoSetTrackDefaults();
}


FQualifiedFrameTime FSequencer::GetLocalTime() const
{
	FFrameRate FocusedResolution = GetFocusedTickResolution();
	FFrameTime CurrentPosition   = PlayPosition.GetCurrentPosition();

	FFrameTime RootTime = ConvertFrameTime(CurrentPosition, PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());
	return FQualifiedFrameTime(RootTime * RootToLocalTransform, FocusedResolution);
}


FQualifiedFrameTime FSequencer::GetGlobalTime() const
{
	FFrameTime RootTime = ConvertFrameTime(PlayPosition.GetCurrentPosition(), PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());
	return FQualifiedFrameTime(RootTime, PlayPosition.GetOutputRate());
}

void FSequencer::SetLocalTime( FFrameTime NewTime, ESnapTimeMode SnapTimeMode )
{
	FFrameRate LocalResolution = GetFocusedTickResolution();

	// Ensure the time is in the current view
	ScrollIntoView(NewTime / LocalResolution);

	// Perform snapping
	if ((SnapTimeMode & ESnapTimeMode::STM_Interval) && Settings->GetIsSnapEnabled())
	{
		FFrameRate LocalDisplayRate = GetFocusedDisplayRate();

		NewTime = FFrameRate::TransformTime(FFrameRate::TransformTime(NewTime, LocalResolution, LocalDisplayRate).RoundToFrame(), LocalDisplayRate, LocalResolution);
	}

	if ((SnapTimeMode & ESnapTimeMode::STM_Keys) && (Settings->GetSnapPlayTimeToKeys() || FSlateApplication::Get().GetModifierKeys().IsShiftDown()))
	{
		NewTime = OnGetNearestKey(NewTime, true);
	}

	SetLocalTimeDirectly(NewTime);
}


void FSequencer::SetLocalTimeDirectly(FFrameTime NewTime)
{
	// Transform the time to the root time-space
	SetGlobalTime(NewTime * RootToLocalTransform.Inverse());
}


void FSequencer::SetGlobalTime(FFrameTime NewTime)
{
	TSharedPtr<SWidget> PreviousFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();

	// Clear focus before setting time in case there's a key editor value selected that gets committed to a newly selected key on UserMovedFocus
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

	NewTime = ConvertFrameTime(NewTime, GetRootTickResolution(), PlayPosition.GetInputRate());
	if (PlayPosition.GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked)
	{
		NewTime = NewTime.FloorToFrame();
	}

	// Don't update the sequence if the time hasn't changed as this will cause duplicate events and the like to fire.
	// If we need to reevaluate the sequence at the same time for whetever reason, we should call ForceEvaluate()
	TOptional<FFrameTime> CurrentPosition = PlayPosition.GetCurrentPosition();
	if (PlayPosition.GetCurrentPosition() != NewTime)
	{
		EvaluateInternal(PlayPosition.JumpTo(NewTime));
	}

	if (AutoScrubTarget.IsSet())
	{
		SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		AutoScrubTarget.Reset();
	}

	if (PreviousFocusedWidget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(PreviousFocusedWidget);
	}
}

void FSequencer::ForceEvaluate()
{
	EvaluateInternal(PlayPosition.GetCurrentPositionAsRange());
}

void FSequencer::EvaluateInternal(FMovieSceneEvaluationRange InRange, bool bHasJumped)
{
	if (Settings->ShouldCompileDirectorOnEvaluate())
	{
		RecompileDirtyDirectors();
	}

	bNeedsEvaluate = false;

	if (PlaybackContextAttribute.IsBound())
	{
		CachedPlaybackContext = PlaybackContextAttribute.Get();
	}
	
	if (EventContextsAttribute.IsBound())
	{
		CachedEventContexts.Reset();
		for (UObject* Object : EventContextsAttribute.Get())
		{
			CachedEventContexts.Add(Object);
		}
	}

	FMovieSceneContext Context = FMovieSceneContext(InRange, PlaybackState).SetIsSilent(SilentModeCount != 0);
	Context.SetHasJumped(bHasJumped);

	FMovieSceneSequenceID RootOverride = MovieSceneSequenceID::Root;
	if (Settings->ShouldEvaluateSubSequencesInIsolation())
	{
		RootOverride = ActiveTemplateIDs.Top();
	}
	
	RootTemplateInstance.Evaluate(Context, *this, RootOverride);

	TemplateStore->PurgeStaleTracks();
	SuppressAutoEvalSignature.Reset();

	if (Settings->ShouldRerunConstructionScripts())
	{
		RerunConstructionScripts();
	}

	if (!IsInSilentMode())
	{
		OnGlobalTimeChangedDelegate.Broadcast();
	}
}

void FSequencer::ScrollIntoView(float InLocalTime)
{
	if (IsAutoScrollEnabled())
	{
		float RangeOffset = CalculateAutoscrollEncroachment(InLocalTime).Get(0.f);
		
		// When not scrubbing, we auto scroll the view range immediately
		if (RangeOffset != 0.f)
		{
			TRange<double> WorkingRange = GetClampRange();

			// Adjust the offset so that the target range will be within the working range.
			if (TargetViewRange.GetLowerBoundValue() + RangeOffset < WorkingRange.GetLowerBoundValue())
			{
				RangeOffset = WorkingRange.GetLowerBoundValue() - TargetViewRange.GetLowerBoundValue();
			}
			else if (TargetViewRange.GetUpperBoundValue() + RangeOffset > WorkingRange.GetUpperBoundValue())
			{
				RangeOffset = WorkingRange.GetUpperBoundValue() - TargetViewRange.GetUpperBoundValue();
			}

			SetViewRange(TRange<double>(TargetViewRange.GetLowerBoundValue() + RangeOffset, TargetViewRange.GetUpperBoundValue() + RangeOffset), EViewRangeInterpolation::Immediate);
		}
	}
}

void FSequencer::UpdateAutoScroll(double NewTime, float ThresholdPercentage)
{
	AutoscrollOffset = CalculateAutoscrollEncroachment(NewTime, ThresholdPercentage);

	if (!AutoscrollOffset.IsSet())
	{
		AutoscrubOffset.Reset();
		return;
	}

	TRange<double> ViewRange = GetViewRange();
	const double Threshold = (ViewRange.GetUpperBoundValue() - ViewRange.GetLowerBoundValue()) * ThresholdPercentage;

	const FQualifiedFrameTime LocalTime = GetLocalTime();

	// If we have no autoscrub offset yet, we move the scrub position to the boundary of the autoscroll threasdhold, then autoscrub from there
	if (!AutoscrubOffset.IsSet())
	{
		if (AutoscrollOffset.GetValue() < 0 && LocalTime.AsSeconds() > ViewRange.GetLowerBoundValue() + Threshold)
		{
			SetLocalTimeDirectly( (ViewRange.GetLowerBoundValue() + Threshold) * LocalTime.Rate );
		}
		else if (AutoscrollOffset.GetValue() > 0 && LocalTime.AsSeconds() < ViewRange.GetUpperBoundValue() - Threshold)
		{
			SetLocalTimeDirectly( (ViewRange.GetUpperBoundValue() - Threshold) * LocalTime.Rate );
		}
	}

	// Don't autoscrub if we're at the extremes of the movie scene range
	const FMovieSceneEditorData& EditorData = GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData();
	if (NewTime < EditorData.WorkStart + Threshold ||
		NewTime > EditorData.WorkEnd - Threshold
		)
	{
		AutoscrubOffset.Reset();
		return;
	}

	// Scrub at the same rate we scroll
	AutoscrubOffset = AutoscrollOffset;
}


TOptional<float> FSequencer::CalculateAutoscrollEncroachment(double NewTime, float ThresholdPercentage) const
{
	enum class EDirection { Positive, Negative };
	const EDirection Movement = NewTime - GetLocalTime().AsSeconds() >= 0 ? EDirection::Positive : EDirection::Negative;

	const TRange<double> CurrentRange = GetViewRange();
	const double RangeMin = CurrentRange.GetLowerBoundValue(), RangeMax = CurrentRange.GetUpperBoundValue();
	const double AutoScrollThreshold = (RangeMax - RangeMin) * ThresholdPercentage;

	if (Movement == EDirection::Negative && NewTime < RangeMin + AutoScrollThreshold)
	{
		// Scrolling backwards in time, and have hit the threshold
		return NewTime - (RangeMin + AutoScrollThreshold);
	}
	
	if (Movement == EDirection::Positive && NewTime > RangeMax - AutoScrollThreshold)
	{
		// Scrolling forwards in time, and have hit the threshold
		return NewTime - (RangeMax - AutoScrollThreshold);
	}

	return TOptional<float>();
}


void FSequencer::AutoScrubToTime(FFrameTime DestinationTime)
{
	AutoScrubTarget = FAutoScrubTarget(DestinationTime, GetLocalTime().Time, FPlatformTime::Seconds());
}

void FSequencer::SetPerspectiveViewportPossessionEnabled(bool bEnabled)
{
	bPerspectiveViewportPossessionEnabled = bEnabled;
}


void FSequencer::SetPerspectiveViewportCameraCutEnabled(bool bEnabled)
{
	bPerspectiveViewportCameraCutEnabled = bEnabled;
}

void FSequencer::RenderMovie(UMovieSceneSection* InSection) const
{
	RenderMovieInternal(InSection->GetRange(), true);
}

void FSequencer::RenderMovieInternal(TRange<FFrameNumber> Range, bool bSetFrameOverrides) const
{
	if (Range.GetLowerBound().IsOpen() || Range.GetUpperBound().IsOpen())
	{
		Range = TRange<FFrameNumber>::Hull(Range, GetPlaybackRange());
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	// Create a new movie scene capture object for an automated level sequence, and open the tab
	UAutomatedLevelSequenceCapture* MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), UMovieSceneCapture::MovieSceneCaptureUIName, RF_Transient);
	MovieSceneCapture->LoadFromConfig();

	MovieSceneCapture->LevelSequenceAsset = GetCurrentAsset()->GetPathName();

	FFrameRate DisplayRate = GetFocusedDisplayRate();
	FFrameRate TickResolution = GetFocusedTickResolution();

	MovieSceneCapture->Settings.FrameRate = DisplayRate;
	MovieSceneCapture->Settings.ZeroPadFrameNumbers = Settings->GetZeroPadFrames();
	MovieSceneCapture->Settings.bUseRelativeFrameNumbers = false;

	FFrameNumber StartFrame = MovieScene::DiscreteInclusiveLower(Range);
	FFrameNumber EndFrame = MovieScene::DiscreteExclusiveUpper(Range);

	FFrameNumber RoundedStartFrame = FFrameRate::TransformTime(StartFrame, TickResolution, DisplayRate).CeilToFrame();
	FFrameNumber RoundedEndFrame = FFrameRate::TransformTime(EndFrame, TickResolution, DisplayRate).CeilToFrame();

	if (bSetFrameOverrides)
	{
		MovieSceneCapture->SetFrameOverrides(RoundedStartFrame, RoundedEndFrame);
	}
	else
	{
		if (!MovieSceneCapture->bUseCustomStartFrame)
		{
			MovieSceneCapture->CustomStartFrame = RoundedStartFrame;
		}

		if (!MovieSceneCapture->bUseCustomEndFrame)
		{
			MovieSceneCapture->CustomEndFrame = RoundedEndFrame;
		}
	}

	// We create a new Numeric Type Interface that ties it's Capture/Resolution rates to the Capture Object so that it converts UI entries
	// to the correct resolution for the capture, and not for the original sequence.
	USequencerSettings* LocalSettings = Settings;

	TAttribute<EFrameNumberDisplayFormats> GetDisplayFormatAttr = MakeAttributeLambda(
		[LocalSettings]
		{
			if (LocalSettings)
			{
				return LocalSettings->GetTimeDisplayFormat();
			}
			return EFrameNumberDisplayFormats::Frames;
		}
	);

	TAttribute<uint8> GetZeroPadFramesAttr = MakeAttributeLambda(
		[LocalSettings]()->uint8
		{
			if (LocalSettings)
			{
				return LocalSettings->GetZeroPadFrames();
			}
			return 0;
		}
	);

	// By using a TickResolution/DisplayRate that match the numbers entered via the numeric interface don't change frames of reference.
	// This is used here because the movie scene capture works entirely on play rate resolution and has no knowledge of the internal resolution
	// so we don't need to convert the user's input into internal resolution.
	TAttribute<FFrameRate> GetFrameRateAttr = MakeAttributeLambda(
		[MovieSceneCapture]
		{
			if (MovieSceneCapture)
			{
				return MovieSceneCapture->GetSettings().FrameRate;
			}
			return FFrameRate(30, 1);
		}
	);

	// Create our numeric type interface so we can pass it to the time slider below.
	TSharedPtr<INumericTypeInterface<double>> MovieSceneCaptureNumericInterface = MakeShareable(new FFrameNumberInterface(GetDisplayFormatAttr, GetZeroPadFramesAttr, GetFrameRateAttr, GetFrameRateAttr));

	IMovieSceneCaptureDialogModule::Get().OpenDialog(LevelEditorModule.GetLevelEditorTabManager().ToSharedRef(), MovieSceneCapture, MovieSceneCaptureNumericInterface);
}

ISequencer::FOnActorAddedToSequencer& FSequencer::OnActorAddedToSequencer()
{
	return OnActorAddedToSequencerEvent;
}

ISequencer::FOnPreSave& FSequencer::OnPreSave()
{
	return OnPreSaveEvent;
}

ISequencer::FOnPostSave& FSequencer::OnPostSave()
{
	return OnPostSaveEvent;
}

ISequencer::FOnActivateSequence& FSequencer::OnActivateSequence()
{
	return OnActivateSequenceEvent;
}

ISequencer::FOnCameraCut& FSequencer::OnCameraCut()
{
	return OnCameraCutEvent;
}

TSharedRef<INumericTypeInterface<double>> FSequencer::GetNumericTypeInterface() const
{
	return SequencerWidget->GetNumericTypeInterface();
}

TSharedRef<SWidget> FSequencer::MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange)
{
	return SequencerWidget->MakeTimeRange(InnerContent, bShowWorkingRange, bShowViewRange, bShowPlaybackRange);
}

/** Attempt to find an object binding ID that relates to an unspawned spawnable object */
FGuid FindUnspawnedObjectGuid(UObject& InObject, UMovieSceneSequence& Sequence)
{
	UMovieScene* MovieScene = Sequence.GetMovieScene();

	// If the object is an archetype, the it relates to an unspawned spawnable.
	UObject* ParentObject = Sequence.GetParentObject(&InObject);
	if (ParentObject && FMovieSceneSpawnable::IsSpawnableTemplate(*ParentObject))
	{
		FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable([&](FMovieSceneSpawnable& InSpawnable){
			return InSpawnable.GetObjectTemplate() == ParentObject;
		});

		if (ParentSpawnable)
		{
			UObject* ParentContext = ParentSpawnable->GetObjectTemplate();

			// The only way to find the object now is to resolve all the child bindings, and see if they are the same
			for (const FGuid& ChildGuid : ParentSpawnable->GetChildPossessables())
			{
				const bool bHasObject = Sequence.LocateBoundObjects(ChildGuid, ParentContext).Contains(&InObject);
				if (bHasObject)
				{
					return ChildGuid;
				}
			}
		}
	}
	else if (FMovieSceneSpawnable::IsSpawnableTemplate(InObject))
	{
		FMovieSceneSpawnable* SpawnableByArchetype = MovieScene->FindSpawnable([&](FMovieSceneSpawnable& InSpawnable){
			return InSpawnable.GetObjectTemplate() == &InObject;
		});

		if (SpawnableByArchetype)
		{
			return SpawnableByArchetype->GetGuid();
		}
	}

	return FGuid();
}

UMovieSceneFolder* FSequencer::CreateFoldersRecursively(const TArray<FString>& FolderPaths, int32 FolderPathIndex, UMovieScene* OwningMovieScene, UMovieSceneFolder* ParentFolder, const TArray<UMovieSceneFolder*>& FoldersToSearch)
{
	// An empty folder path won't create a folder
	if (FolderPaths.Num() == 0)
	{
		return nullptr;
	}

	check(FolderPathIndex < FolderPaths.Num());

	// Look to see if there's already a folder with the right name
	UMovieSceneFolder* FolderToUse = nullptr;
	FName DesiredFolderName = FName(*FolderPaths[FolderPathIndex]);

	for (UMovieSceneFolder* Folder : FoldersToSearch)
	{
		if (Folder->GetFolderName() == DesiredFolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	// If we didn't find a folder with the desired name then we create a new folder as a sibling of the existing folders.
	if (FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(OwningMovieScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(DesiredFolderName);
		if (ParentFolder)
		{
			// Add the new folder as a sibling of the folders we were searching in.
			ParentFolder->AddChildFolder(FolderToUse);
		}
		else
		{
			// If we have no parent folder then we must be at the root so we add it to the root of the movie scene
			OwningMovieScene->GetRootFolders().Add(FolderToUse);
		}
	}

	// Increment which part of the path we're searching in and then recurse inside of the folder we found (or created).
	FolderPathIndex++;
	if (FolderPathIndex < FolderPaths.Num())
	{
		return CreateFoldersRecursively(FolderPaths, FolderPathIndex, OwningMovieScene, FolderToUse, FolderToUse->GetChildFolders());
	}

	// We return the tail folder created so that the user can add things to it.
	return FolderToUse;
}

FGuid FSequencer::GetHandleToObject( UObject* Object, bool bCreateHandleIfMissing, const FName& CreatedFolderName )
{
	if (Object == nullptr)
	{
		return FGuid();
	}

	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	UMovieScene* FocusedMovieScene = FocusedMovieSceneSequence->GetMovieScene();
	
	if (FocusedMovieScene->IsReadOnly())
	{
		return FGuid();
	}

	// Attempt to resolve the object through the movie scene instance first, 
	FGuid ObjectGuid = FindObjectId(*Object, ActiveTemplateIDs.Top());

	if (ObjectGuid.IsValid())
	{
		// Check here for spawnable otherwise spawnables get recreated as possessables, which doesn't make sense
		FMovieSceneSpawnable* Spawnable = FocusedMovieScene->FindSpawnable(ObjectGuid);
		if (Spawnable)
		{
			return ObjectGuid;
		}

		// Make sure that the possessable is still valid, if it's not remove the binding so new one 
		// can be created.  This can happen due to undo.
		FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(ObjectGuid);
		if(Possessable == nullptr)
		{
			FocusedMovieSceneSequence->UnbindPossessableObjects(ObjectGuid);
			ObjectGuid.Invalidate();
		}
	}
	else
	{
		ObjectGuid = FindUnspawnedObjectGuid(*Object, *FocusedMovieSceneSequence);
	}

	if (ObjectGuid.IsValid() || IsReadOnly())
	{
		return ObjectGuid;
	}

	UObject* PlaybackContext = PlaybackContextAttribute.Get(nullptr);

	// If the object guid was not found attempt to add it
	// Note: Only possessed actors can be added like this
	if (FocusedMovieSceneSequence->CanPossessObject(*Object, PlaybackContext) && bCreateHandleIfMissing)
	{
		AActor* PossessedActor = Cast<AActor>(Object);

		ObjectGuid = CreateBinding(*Object, PossessedActor != nullptr ? PossessedActor->GetActorLabel() : Object->GetName());

		AActor* OwningActor = PossessedActor;
		FGuid OwningObjectGuid = ObjectGuid;
		if (!OwningActor)
		{
			// We can only add Object Bindings for actors to folders, but this function can be called on a component of an Actor.
			// In this case, we attempt to find the Actor who owns the component and then look up the Binding Guid for that actor
			// so that we add that actor to the folder as expected.
			OwningActor = Object->GetTypedOuter<AActor>();
			if (OwningActor)
			{
				OwningObjectGuid = FocusedMovieSceneSequence->FindPossessableObjectId(*OwningActor, PlaybackContext);
			}
		}

		if (OwningActor)
		{
			FGuid ActorAddedGuid = GetHandleToObject(OwningActor);
			if (ActorAddedGuid.IsValid())
			{
				OnActorAddedToSequencerEvent.Broadcast(OwningActor, ActorAddedGuid);
			}
		}

		// Some sources that create object bindings may want to group all of these objects together for organizations sake.
		if (OwningActor && CreatedFolderName != NAME_None)
		{
			TArray<FString> SubfolderHierarchy;
			if (OwningActor->GetFolderPath() != NAME_None)
			{
				OwningActor->GetFolderPath().ToString().ParseIntoArray(SubfolderHierarchy, TEXT("/"));
			}

			// Add the desired sub-folder as the root of the hierarchy so that the Actor's World Outliner folder structure is replicated inside of the desired folder name.
			// This has to come after the ParseIntoArray call as that will wipe the array.
			SubfolderHierarchy.Insert(CreatedFolderName.ToString(), 0); 

			UMovieSceneFolder* TailFolder = FSequencer::CreateFoldersRecursively(SubfolderHierarchy, 0, FocusedMovieScene, nullptr, FocusedMovieScene->GetRootFolders());
			if (TailFolder)
			{
				TailFolder->AddChildObjectBinding(OwningObjectGuid);
			}

			// We have to build a new expansion state path since we created them in sub-folders.
			// We have to recursively build an expansion state as well so that nestled objects get auto-expanded.
			FString NewPath; 
			for (int32 Index = 0; Index < SubfolderHierarchy.Num(); Index++)
			{
				NewPath += SubfolderHierarchy[Index];
				FocusedMovieScene->GetEditorData().ExpansionStates.FindOrAdd(NewPath) = FMovieSceneExpansionState(true);
				
				// Expansion States are delimited by periods.
				NewPath += TEXT(".");
			}

		}

		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
	
	return ObjectGuid;
}


ISequencerObjectChangeListener& FSequencer::GetObjectChangeListener()
{ 
	return *ObjectChangeListener;
}

void FSequencer::PossessPIEViewports(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	UWorld* World = Cast<UWorld>(CachedPlaybackContext.Get());
	if (!World || World->WorldType != EWorldType::PIE)
	{
		return;
	}
	
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
	if (PC == nullptr)
	{
		return;
	}

	TWeakObjectPtr<APlayerController> WeakPC = PC;
	auto FindViewTarget = [=](const FCachedViewTarget& In){ return In.PlayerController == WeakPC; };

	// skip same view target
	AActor* ViewTarget = PC->GetViewTarget();

	// save the last view target so that it can be restored when the camera object is null
	if (!PrePossessionViewTargets.ContainsByPredicate(FindViewTarget))
	{
		PrePossessionViewTargets.Add(FCachedViewTarget{ PC, ViewTarget });
	}

	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	if (CameraComponent && CameraComponent->GetOwner() != CameraObject)
	{
		CameraObject = CameraComponent->GetOwner();
	}

	if (CameraObject == ViewTarget)
	{
		if ( bJumpCut )
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
			}

			if (CameraComponent)
			{
				CameraComponent->NotifyCameraCut();
			}
		}
		return;
	}

	// skip unlocking if the current view target differs
	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	// if unlockIfCameraActor is valid, release lock if currently locked to object
	if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
	{
		return;
	}

	// override the player controller's view target
	AActor* CameraActor = Cast<AActor>(CameraObject);

	// if the camera object is null, use the last view target so that it is restored to the state before the sequence takes control
	if (CameraActor == nullptr)
	{
		if (const FCachedViewTarget* CachedTarget = PrePossessionViewTargets.FindByPredicate(FindViewTarget))
		{
			CameraActor = CachedTarget->ViewTarget.Get();
		}
	}

	FViewTargetTransitionParams TransitionParams;
	PC->SetViewTarget(CameraActor, TransitionParams);

	if (CameraComponent)
	{
		CameraComponent->NotifyCameraCut();
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);
		PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
	}
}

TSharedPtr<class ITimeSlider> FSequencer::GetTopTimeSliderWidget() const
{
	return SequencerWidget->GetTopTimeSliderWidget();
}

void FSequencer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	OnCameraCutEvent.Broadcast(CameraObject, bJumpCut);

	if (!IsPerspectiveViewportCameraCutEnabled())
	{
		return;
	}

	PossessPIEViewports(CameraObject, UnlockIfCameraObject, bJumpCut);

	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if ((LevelVC == nullptr) || !LevelVC->AllowsCinematicControl())
		{
			continue;
		}

		if ((CameraObject != nullptr) || LevelVC->IsLockedToActor(UnlockIfCameraActor))
		{
			UpdatePreviewLevelViewportClientFromCameraCut(*LevelVC, CameraObject, bJumpCut);
		}
	}
}

void FSequencer::NotifyBindingsChanged()
{
	ISequencer::NotifyBindingsChanged();

	OnMovieSceneBindingsChangedDelegate.Broadcast();
}


void FSequencer::SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap)
{
	if (!IsPerspectiveViewportPossessionEnabled())
	{
		return;
	}

	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC)
		{
			if (LevelVC->AllowsCinematicControl())
			{
				if (ViewportParamsMap.Contains(LevelVC))
				{
					const EMovieSceneViewportParams* ViewportParams = ViewportParamsMap.Find(LevelVC);
					if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeAmount)
					{
						LevelVC->FadeAmount = ViewportParams->FadeAmount;
						LevelVC->bEnableFading = true;
					}
					if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeColor)
					{
						LevelVC->FadeColor = ViewportParams->FadeColor.ToFColor(/*bSRGB=*/ true);
						LevelVC->bEnableFading = true;
					}
					if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_ColorScaling)
					{
						LevelVC->bEnableColorScaling = ViewportParams->bEnableColorScaling;
						LevelVC->ColorScale = ViewportParams->ColorScale;
					}
				}
			}
			else
			{
				LevelVC->bEnableFading = false;
				LevelVC->bEnableColorScaling = false;
			}
		}
	}
}


void FSequencer::GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const
{
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->AllowsCinematicControl())
		{
			EMovieSceneViewportParams ViewportParams;
			ViewportParams.FadeAmount = LevelVC->FadeAmount;
			ViewportParams.FadeColor = FLinearColor(LevelVC->FadeColor);
			ViewportParams.ColorScale = LevelVC->ColorScale;

			ViewportParamsMap.Add(LevelVC, ViewportParams);
		}
	}
}


EMovieScenePlayerStatus::Type FSequencer::GetPlaybackStatus() const
{
	return PlaybackState;
}


void FSequencer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlaybackState = InPlaybackStatus;

	// Inform the renderer when Sequencer is in a 'paused' state for the sake of inter-frame effects
	const bool bIsPaused = (InPlaybackStatus == EMovieScenePlayerStatus::Stopped || InPlaybackStatus == EMovieScenePlayerStatus::Scrubbing || InPlaybackStatus == EMovieScenePlayerStatus::Stepping);

	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->AllowsCinematicControl())
		{
			LevelVC->ViewState.GetReference()->SetSequencerState(bIsPaused);
		}
	}

	// backup or restore tick rate
	if (InPlaybackStatus == EMovieScenePlayerStatus::Playing)
	{
		OldMaxTickRate = GEngine->GetMaxFPS();
	}
	else
	{
		StopAutoscroll();

		GEngine->SetMaxFPS(OldMaxTickRate);

		ShuttleMultiplier = 0;
	}

	TimeController->PlayerStatusChanged(PlaybackState, GetGlobalTime());
}


void FSequencer::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( Settings );

	if (UMovieSceneSequence* RootSequencePtr = RootSequence.Get())
	{
		Collector.AddReferencedObject( RootSequencePtr );
	}

	FMovieSceneRootEvaluationTemplateInstance::StaticStruct()->SerializeBin(Collector.GetVerySlowReferenceCollectorArchive(), &RootTemplateInstance);
}


void FSequencer::ResetPerMovieSceneData()
{
	//@todo Sequencer - We may want to preserve selections when moving between movie scenes
	Selection.Empty();

	RefreshTree();

	UpdateTimeBoundsToFocusedMovieScene();

	LabelManager.SetMovieScene( GetFocusedMovieSceneSequence()->GetMovieScene() );

	SuppressAutoEvalSignature.Reset();

	// @todo run through all tracks for new movie scene changes
	//  needed for audio track decompression
}


void FSequencer::RecordSelectedActors()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
	if (SequenceRecorder.IsRecording())
	{
		FNotificationInfo Info(LOCTEXT("UnableToRecord_AlreadyRecording", "Cannot start a new recording while one is already in progress."));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	if (Settings->ShouldRewindOnRecord())
	{
		JumpToStart();
	}
	
	TArray<ACameraActor*> SelectedCameras;
	TArray<AActor*> EntireSelection;

	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedCameras);
	GEditor->GetSelectedActors()->GetSelectedObjects(EntireSelection);

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Figure out what we're recording into - a sub track, or a camera cut track, or a shot track
	UMovieSceneTrack* DestinationTrack = nullptr;
	if (SelectedCameras.Num())
	{
		DestinationTrack = MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
		if (!DestinationTrack)
		{
			DestinationTrack = MovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
		}
	}
	else if (EntireSelection.Num())
	{
		DestinationTrack = MovieScene->FindMasterTrack<UMovieSceneSubTrack>();
		if (!DestinationTrack)
		{
			DestinationTrack = MovieScene->AddMasterTrack<UMovieSceneSubTrack>();
		}
	}
	else
	{
		FNotificationInfo Info(LOCTEXT("UnableToRecordNoSelection", "Unable to start recording because no actors are selected"));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	if (!DestinationTrack)
	{
		FNotificationInfo Info(LOCTEXT("UnableToRecord", "Unable to start recording because a valid sub track could not be found or created"));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	int32 MaxRow = -1;
	for (UMovieSceneSection* Section : DestinationTrack->GetAllSections())
	{
		MaxRow = FMath::Max(Section->GetRowIndex(), MaxRow);
	}
	// @todo: Get row at current time
	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(DestinationTrack->CreateNewSection());
	NewSection->SetRowIndex(MaxRow + 1);
	DestinationTrack->AddSection(*NewSection);
	NewSection->SetAsRecording(true);

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

	if (UMovieSceneSubSection::IsSetAsRecording())
	{
		TArray<AActor*> ActorsToRecord;
		for (AActor* Actor : EntireSelection)
		{
			AActor* CounterpartActor = EditorUtilities::GetSimWorldCounterpartActor(Actor);
			ActorsToRecord.Add(CounterpartActor ? CounterpartActor : Actor);
		}

		const FString& PathToRecordTo = UMovieSceneSubSection::GetRecordingSection()->GetTargetPathToRecordTo();
		const FString& SequenceName = UMovieSceneSubSection::GetRecordingSection()->GetTargetSequenceName();
		SequenceRecorder.StartRecording(
			ActorsToRecord,
			PathToRecordTo,
			SequenceName);
	}
}

TSharedRef<SWidget> FSequencer::MakeTransportControls(bool bExtended)
{
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::Get().LoadModuleChecked<FEditorWidgetsModule>( "EditorWidgets" );

	FTransportControlArgs TransportControlArgs;
	{
		TransportControlArgs.OnBackwardEnd.BindSP( this, &FSequencer::OnJumpToStart );
		TransportControlArgs.OnBackwardStep.BindSP( this, &FSequencer::OnStepBackward );
		TransportControlArgs.OnForwardPlay.BindSP( this, &FSequencer::OnPlayForward, true );
		TransportControlArgs.OnBackwardPlay.BindSP( this, &FSequencer::OnPlayBackward, true );
		TransportControlArgs.OnForwardStep.BindSP( this, &FSequencer::OnStepForward );
		TransportControlArgs.OnForwardEnd.BindSP( this, &FSequencer::OnJumpToEnd );
		TransportControlArgs.OnGetPlaybackMode.BindSP( this, &FSequencer::GetPlaybackMode );

		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportSetPlaybackStart)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardEnd));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportJumpToPreviousKey)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardStep));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardPlay));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardPlay));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportRecord)));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardStep));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportJumpToNextKey)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardEnd));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportSetPlaybackEnd)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportLoopMode)));
		TransportControlArgs.bAreButtonsFocusable = false;
	}

	return EditorWidgetsModule.CreateTransportControl( TransportControlArgs );
}

TSharedRef<SWidget> FSequencer::OnCreateTransportSetPlaybackStart()
{
	FText SetPlaybackStartToolTip = FText::Format(LOCTEXT("SetPlayStart_Tooltip", "Set playback start to the current position ({0})"), FSequencerCommands::Get().SetStartPlaybackRange->GetInputText());

	return SNew(SButton)
		.OnClicked(this, &FSequencer::SetPlaybackStart)
		.ToolTipText(SetPlaybackStartToolTip)
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.SetPlayStart")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportJumpToPreviousKey()
{
	FText JumpToPreviousKeyToolTip = FText::Format(LOCTEXT("JumpToPreviousKey_Tooltip", "Jump to the previous key in the selected track(s) ({0})"), FSequencerCommands::Get().StepToPreviousKey->GetInputText());

	return SNew(SButton)
		.OnClicked(this, &FSequencer::JumpToPreviousKey)
		.ToolTipText(JumpToPreviousKeyToolTip)
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.JumpToPreviousKey")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportJumpToNextKey()
{
	FText JumpToNextKeyToolTip = FText::Format(LOCTEXT("JumpToNextKey_Tooltip", "Jump to the next key in the selected track(s) ({0})"), FSequencerCommands::Get().StepToNextKey->GetInputText());

	return SNew(SButton)
		.OnClicked(this, &FSequencer::JumpToNextKey)
		.ToolTipText(JumpToNextKeyToolTip)
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.JumpToNextKey")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportSetPlaybackEnd()
{
	FText SetPlaybackEndToolTip = FText::Format(LOCTEXT("SetPlayEnd_Tooltip", "Set playback end to the current position ({0})"), FSequencerCommands::Get().SetEndPlaybackRange->GetInputText());

	return SNew(SButton)
		.OnClicked(this, &FSequencer::SetPlaybackEnd)
		.ToolTipText(SetPlaybackEndToolTip)
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.SetPlayEnd")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportLoopMode()
{
	TSharedRef<SButton> LoopButton = SNew(SButton)
		.OnClicked(this, &FSequencer::OnCycleLoopMode)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ToolTipText_Lambda([&]()
		{ 
			if (GetLoopMode() == ESequencerLoopMode::SLM_NoLoop)
			{
				return LOCTEXT("LoopModeNoLoop_Tooltip", "No looping");
			}
			else if (GetLoopMode() == ESequencerLoopMode::SLM_Loop)
			{
				return LOCTEXT("LoopModeLoop_Tooltip", "Loop playback range");
			}
			else
			{
				return LOCTEXT("LoopModeLoopSelectionRange_Tooltip", "Loop selection range");
			}
		})
		.ContentPadding(2.0f);

	TWeakPtr<SButton> WeakButton = LoopButton;

	LoopButton->SetContent(SNew(SImage)
		.Image_Lambda([&, WeakButton]()
		{
			if (GetLoopMode() == ESequencerLoopMode::SLM_NoLoop)
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Disabled").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Disabled").Normal;
			}
			else if (GetLoopMode() == ESequencerLoopMode::SLM_Loop)
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Enabled").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Enabled").Normal;
			}
			else
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.SelectionRange").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.SelectionRange").Normal;
			}
		})
	);

	return LoopButton;
}

TSharedRef<SWidget> FSequencer::OnCreateTransportRecord()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	TSharedRef<SButton> RecordButton = SNew(SButton)
		.OnClicked(this, &FSequencer::OnRecord)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ToolTipText_Lambda([&](){ return SequenceRecorder.IsRecording() ? LOCTEXT("StopRecord_Tooltip", "Stop recording current sub-track.") : LOCTEXT("Record_Tooltip", "Record the primed sequence sub-track."); })
		.Visibility(this, &FSequencer::GetRecordButtonVisibility)
		.ContentPadding(2.0f);

	TWeakPtr<SButton> WeakButton = RecordButton;

	RecordButton->SetContent(SNew(SImage)
		.Image_Lambda([&SequenceRecorder, WeakButton]()
		{
			if (SequenceRecorder.IsRecording())
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Recording").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Recording").Normal;
			}

			return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
				&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Record").Pressed : 
				&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Record").Normal;
		})
	);

	return RecordButton;
}


UObject* FSequencer::FindSpawnedObjectOrTemplate(const FGuid& BindingId)
{
	TArrayView<TWeakObjectPtr<>> Objects = FindObjectsInCurrentSequence(BindingId);
	if (Objects.Num())
	{
		return Objects[0].Get();
	}

	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return nullptr;
	}

	UMovieScene* FocusedMovieScene = Sequence->GetMovieScene();

	FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(BindingId);
	// If we're a possessable with a parent spawnable and we don't have the object, we look the object up within the default object of the spawnable
	if (Possessable && Possessable->GetParent().IsValid())
	{
		// If we're a spawnable and we don't have the object, use the default object to build up the track menu
		FMovieSceneSpawnable* ParentSpawnable = FocusedMovieScene->FindSpawnable(Possessable->GetParent());
		if (ParentSpawnable)
		{
			UObject* ParentObject = ParentSpawnable->GetObjectTemplate();
			if (ParentObject)
			{
				for (UObject* Obj : Sequence->LocateBoundObjects(BindingId, ParentObject))
				{
					return Obj;
				}
			}
		}
	}
	// If we're a spawnable and we don't have the object, use the default object to build up the track menu
	else if (FMovieSceneSpawnable* Spawnable = FocusedMovieScene->FindSpawnable(BindingId))
	{
		return Spawnable->GetObjectTemplate();
	}

	return nullptr;
}

FReply FSequencer::OnPlay(bool bTogglePlay)
{
	if( PlaybackState == EMovieScenePlayerStatus::Playing && bTogglePlay )
	{
		Pause();
	}
	else
	{
		SetPlaybackStatus(EMovieScenePlayerStatus::Playing);

		// Make sure Slate ticks during playback
		SequencerWidget->RegisterActiveTimerForPlayback();

		OnPlayDelegate.Broadcast();
	}

	return FReply::Handled();
}


EVisibility FSequencer::GetRecordButtonVisibility() const
{
	return UMovieSceneSubSection::IsSetAsRecording() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply FSequencer::OnRecord()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	if(UMovieSceneSubSection::IsSetAsRecording() && !SequenceRecorder.IsRecording())
	{
		AActor* ActorToRecord = UMovieSceneSubSection::GetActorToRecord();
		if (ActorToRecord != nullptr)
		{
			AActor* OutActor = EditorUtilities::GetSimWorldCounterpartActor(ActorToRecord);
			if (OutActor != nullptr)
			{
				ActorToRecord = OutActor;
			}
		}

		const FString& PathToRecordTo = UMovieSceneSubSection::GetRecordingSection()->GetTargetPathToRecordTo();
		const FString& SequenceName = UMovieSceneSubSection::GetRecordingSection()->GetTargetSequenceName();
		SequenceRecorder.StartRecording(ActorToRecord, PathToRecordTo, SequenceName);
	}
	else if(SequenceRecorder.IsRecording())
	{
		SequenceRecorder.StopRecording();
	}

	return FReply::Handled();
}

void FSequencer::HandleRecordingStarted(UMovieSceneSequence* Sequence)
{
	OnPlayForward(false);

	// Make sure Slate ticks during playback
	SequencerWidget->RegisterActiveTimerForPlayback();

	// sync recording section to start
	UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
	if(Section != nullptr)
	{
		FFrameRate   TickResolution  = GetFocusedTickResolution();
		FFrameNumber StartFrame      = GetLocalTime().ConvertTo(TickResolution).CeilToFrame();
		int32        Duration        = FFrameRate::TransformTime(1, GetFocusedDisplayRate(), TickResolution).CeilToFrame().Value;

		Section->SetRange(TRange<FFrameNumber>(StartFrame, StartFrame + Duration));
	}
}

void FSequencer::HandleRecordingFinished(UMovieSceneSequence* Sequence)
{
	// toggle us to no playing if we are still playing back
	// as the post processing takes such a long time we don't really care if the sequence doesnt carry on
	if(PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		OnPlayForward(true);
	}

	// now patchup the section that was recorded to
	UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
	if(Section != nullptr)
	{
		Section->SetAsRecording(false);
		Section->SetSequence(Sequence);

		FFrameNumber EndFrame = Section->GetInclusiveStartFrame() + MovieScene::DiscreteSize(Sequence->GetMovieScene()->GetPlaybackRange());
		Section->SetRange(TRange<FFrameNumber>(Section->GetInclusiveStartFrame(), TRangeBound<FFrameNumber>::Exclusive(EndFrame)));

		if (Section->IsA<UMovieSceneCinematicShotSection>())
		{
			const FMovieSceneSpawnable* SpawnedCamera = Sequence->GetMovieScene()->FindSpawnable(
				[](FMovieSceneSpawnable& InSpawnable){
					return InSpawnable.GetObjectTemplate() && InSpawnable.GetObjectTemplate()->IsA<ACameraActor>();
				}
			);

			if (SpawnedCamera && !Sequence->GetMovieScene()->GetCameraCutTrack())
			{
				UMovieSceneTrack* CameraCutTrack = Sequence->GetMovieScene()->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
				UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
				CameraCutSection->SetCameraGuid(SpawnedCamera->GetGuid());
				CameraCutSection->SetRange(Sequence->GetMovieScene()->GetPlaybackRange());
				CameraCutTrack->AddSection(*CameraCutSection);
			}
		}
	}

	bNeedTreeRefresh = true;

	// If viewing the same sequence, rebuild
	if (RootSequence.IsValid() && RootSequence.Get() == Sequence)
	{
		ResetToNewRootSequence(*RootSequence.Get());

		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshAllImmediately);
	}
}

FReply FSequencer::OnPlayForward(bool bTogglePlay)
{
	if (PlaybackSpeed < 0)
	{
		PlaybackSpeed = -PlaybackSpeed;
		OnPlay(false);
	}
	else
	{
		OnPlay(bTogglePlay);
	}
	return FReply::Handled();
}

FReply FSequencer::OnPlayBackward(bool bTogglePlay)
{
	if (PlaybackSpeed > 0)
	{
		PlaybackSpeed = -PlaybackSpeed;
		OnPlay(false);
	}
	else
	{
		OnPlay(bTogglePlay);
	}
	return FReply::Handled();
}

FReply FSequencer::OnStepForward()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);

	FFrameRate          DisplayRate = GetFocusedDisplayRate();
	FQualifiedFrameTime CurrentTime = GetLocalTime();

	FFrameTime NewPosition = FFrameRate::TransformTime(CurrentTime.ConvertTo(DisplayRate).FloorToFrame() + 1, DisplayRate, CurrentTime.Rate);
	SetLocalTime(NewPosition, ESnapTimeMode::STM_Interval);
	return FReply::Handled();
}


FReply FSequencer::OnStepBackward()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);

	FFrameRate          DisplayRate = GetFocusedDisplayRate();
	FQualifiedFrameTime CurrentTime = GetLocalTime();

	FFrameTime NewPosition = FFrameRate::TransformTime(CurrentTime.ConvertTo(DisplayRate).FloorToFrame() - 1, DisplayRate, CurrentTime.Rate);

	SetLocalTime(NewPosition, ESnapTimeMode::STM_Interval);
	return FReply::Handled();
}


FReply FSequencer::OnJumpToStart()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	SetLocalTime(MovieScene::DiscreteInclusiveLower(GetPlaybackRange()));
	return FReply::Handled();
}


FReply FSequencer::OnJumpToEnd()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	const bool bInsetDisplayFrame = ScrubStyle == ESequencerScrubberStyle::FrameBlock && Settings->GetSnapPlayTimeToInterval() && Settings->GetIsSnapEnabled();

	FFrameRate LocalResolution = GetFocusedTickResolution();
	FFrameRate DisplayRate = GetFocusedDisplayRate();

	// Calculate an offset from the end to go to. If they have snapping on (and the scrub style is a block) the last valid frame is represented as one
	// whole display rate frame before the end, otherwise we just subtract a single frame which matches the behavior of hitting play and letting it run to the end.
	FFrameTime OneFrame = bInsetDisplayFrame ? FFrameRate::TransformTime(FFrameTime(1), DisplayRate, LocalResolution) : FFrameTime(1);
	FFrameTime NewTime = MovieScene::DiscreteExclusiveUpper(GetPlaybackRange()) - OneFrame;

	SetLocalTime(NewTime);
	return FReply::Handled();
}


FReply FSequencer::OnCycleLoopMode()
{
	ESequencerLoopMode LoopMode = Settings->GetLoopMode();
	if (LoopMode == ESequencerLoopMode::SLM_NoLoop)
	{
		Settings->SetLoopMode(ESequencerLoopMode::SLM_Loop);
	}
	else if (LoopMode == ESequencerLoopMode::SLM_Loop && !GetSelectionRange().IsEmpty())
	{
		Settings->SetLoopMode(ESequencerLoopMode::SLM_LoopSelectionRange);
	}
	else if (LoopMode == ESequencerLoopMode::SLM_LoopSelectionRange || GetSelectionRange().IsEmpty())
	{
		Settings->SetLoopMode(ESequencerLoopMode::SLM_NoLoop);
	}
	return FReply::Handled();
}


FReply FSequencer::SetPlaybackEnd()
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		FFrameNumber         CurrentFrame = GetLocalTime().Time.FloorToFrame();
		TRange<FFrameNumber> CurrentRange = FocusedSequence->GetMovieScene()->GetPlaybackRange();
		if (CurrentFrame >= MovieScene::DiscreteInclusiveLower(CurrentRange))
		{
			CurrentRange.SetUpperBound(CurrentFrame);
			SetPlaybackRange(CurrentRange);
		}
	}
	return FReply::Handled();
}

FReply FSequencer::SetPlaybackStart()
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		FFrameNumber         CurrentFrame = GetLocalTime().Time.FloorToFrame();
		TRange<FFrameNumber> CurrentRange = FocusedSequence->GetMovieScene()->GetPlaybackRange();
		if (CurrentFrame < MovieScene::DiscreteExclusiveUpper(CurrentRange))
		{
			CurrentRange.SetLowerBound(CurrentFrame);
			SetPlaybackRange(CurrentRange);
		}
	}
	return FReply::Handled();
}

FReply FSequencer::JumpToPreviousKey()
{
	if (Selection.GetSelectedOutlinerNodes().Num())
	{
		GetKeysFromSelection(SelectedKeyCollection, SMALL_NUMBER);
	}
	else
	{
		GetAllKeys(SelectedKeyCollection, SMALL_NUMBER);
	}

	if (SelectedKeyCollection.IsValid())
	{
		FFrameNumber FrameNumber = GetLocalTime().Time.FloorToFrame();
		TOptional<FFrameNumber> NewTime = SelectedKeyCollection->GetNextKey(FrameNumber, EFindKeyDirection::Backwards);
		if (NewTime.IsSet())
		{
			SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
			SetLocalTimeDirectly(NewTime.GetValue());
		}
	}
	return FReply::Handled();
}

FReply FSequencer::JumpToNextKey()
{
	if (Selection.GetSelectedOutlinerNodes().Num())
	{
		GetKeysFromSelection(SelectedKeyCollection, SMALL_NUMBER);
	}
	else
	{
		GetAllKeys(SelectedKeyCollection, SMALL_NUMBER);
	}

	if (SelectedKeyCollection.IsValid())
	{
		FFrameNumber FrameNumber = GetLocalTime().Time.FloorToFrame();
		TOptional<FFrameNumber> NewTime = SelectedKeyCollection->GetNextKey(FrameNumber, EFindKeyDirection::Forwards);
		if (NewTime.IsSet())
		{
			SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
			SetLocalTimeDirectly(NewTime.GetValue());
		}
	}

	return FReply::Handled();
}

ESequencerLoopMode FSequencer::GetLoopMode() const
{
	return Settings->GetLoopMode();
}


void FSequencer::SetLocalTimeLooped(FFrameTime NewLocalTime)
{
	TOptional<EMovieScenePlayerStatus::Type> NewPlaybackStatus;

	FFrameTime NewGlobalTime = NewLocalTime * RootToLocalTransform.Inverse();

	TRange<FFrameNumber> TimeBounds = GetTimeBounds();

	bool         bResetPosition       = false;
	FFrameRate   LocalTickResolution  = GetFocusedTickResolution();
	FFrameRate   RootTickResolution   = GetRootTickResolution();
	FFrameNumber MinInclusiveTime     = MovieScene::DiscreteInclusiveLower(TimeBounds);
	FFrameNumber MaxInclusiveTime     = MovieScene::DiscreteExclusiveUpper(TimeBounds)-1;

	bool bHasJumped = false;
	bool bRestarted = false;
	if (GetLoopMode() == ESequencerLoopMode::SLM_Loop || GetLoopMode() == ESequencerLoopMode::SLM_LoopSelectionRange)
	{
		const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
		if (FocusedSequence)
		{
			if (NewLocalTime < MinInclusiveTime || NewLocalTime > MaxInclusiveTime)
			{
				NewGlobalTime = (PlaybackSpeed > 0 ? MinInclusiveTime : MaxInclusiveTime) * RootToLocalTransform.Inverse();

				bResetPosition = true;
				bHasJumped = true;
			}
		}
	}
	else
	{
		TRange<double> WorkingRange = GetClampRange();

		bool bReachedEnd = false;
		if (PlaybackSpeed > 0)
		{
			bReachedEnd = GetLocalTime().Time < MaxInclusiveTime && NewLocalTime >= MaxInclusiveTime;
		}
		else
		{
			bReachedEnd = GetLocalTime().Time > MinInclusiveTime && NewLocalTime <= MinInclusiveTime;
		}

		// Stop if we hit the playback range end
		if (bReachedEnd)
		{
			NewGlobalTime = (PlaybackSpeed > 0 ? MaxInclusiveTime : MinInclusiveTime) * RootToLocalTransform.Inverse();
			NewPlaybackStatus = EMovieScenePlayerStatus::Stopped;
		}
		// Constrain to the play range if necessary
		else if (Settings->ShouldKeepCursorInPlayRange())
		{
			// Clamp to bound or jump back if necessary
			if (NewLocalTime < MinInclusiveTime || NewLocalTime >= MaxInclusiveTime)
			{
				NewGlobalTime = (PlaybackSpeed > 0 ? MinInclusiveTime : MaxInclusiveTime) * RootToLocalTransform.Inverse();

				bResetPosition = true;
			}
		}
		// Ensure the time is within the working range
		else if (!WorkingRange.Contains(NewLocalTime / LocalTickResolution))
		{
			FFrameTime WorkingMin = (WorkingRange.GetLowerBoundValue() * LocalTickResolution).CeilToFrame();
			FFrameTime WorkingMax = (WorkingRange.GetUpperBoundValue() * LocalTickResolution).FloorToFrame();

			NewGlobalTime = FMath::Clamp(NewLocalTime, WorkingMin, WorkingMax) * RootToLocalTransform.Inverse();

			bResetPosition = true;
			NewPlaybackStatus = EMovieScenePlayerStatus::Stopped;
		}
	}

	// Ensure the time is in the current view - must occur before the time cursor changes
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	ScrollIntoView( (NewGlobalTime * RootToLocalTransform) / RootTickResolution );

	FFrameTime NewPlayPosition = ConvertFrameTime(NewGlobalTime, RootTickResolution, PlayPosition.GetInputRate());

	// Reset the play cursor if we're looping or have otherwise jumpted to a new position in the sequence
	if (bResetPosition)
	{
		PlayPosition.Reset(NewPlayPosition);
		TimeController->Reset(FQualifiedFrameTime(NewGlobalTime, RootTickResolution));
	}

	// Evaluate the sequence
	FMovieSceneEvaluationRange EvalRange = PlayPosition.PlayTo(NewPlayPosition);
	EvaluateInternal(EvalRange, bHasJumped);

	// Set the playback status if we need to
	if (NewPlaybackStatus.IsSet())
	{
		SetPlaybackStatus(NewPlaybackStatus.GetValue());
		// Evaluate the sequence with the new status
		EvaluateInternal(EvalRange);
	}
}

EPlaybackMode::Type FSequencer::GetPlaybackMode() const
{
	if (PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		if (PlaybackSpeed > 0)
		{
			return EPlaybackMode::PlayingForward;
		}
		else
		{
			return EPlaybackMode::PlayingReverse;
		}
	}
		
	return EPlaybackMode::Stopped;
}

void FSequencer::UpdateTimeBoundsToFocusedMovieScene()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	FQualifiedFrameTime CurrentTime = GetLocalTime();

	// Set the view range to:
	// 1. The moviescene view range
	// 2. The moviescene playback range
	// 3. Some sensible default
	TRange<double> NewRange = FocusedMovieScene->GetEditorData().GetViewRange();

	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = FocusedMovieScene->GetPlaybackRange() / CurrentTime.Rate;
	}
	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = TRange<double>(0.0, 5.0);
	}

	// Set the view range to the new range
	SetViewRange(NewRange, EViewRangeInterpolation::Immediate);
}


TRange<FFrameNumber> FSequencer::GetTimeBounds() const
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();

	if(!FocusedSequence)
	{
		return TRange<FFrameNumber>( -100000, 100000 );
	}
	
	if (GetLoopMode() == ESequencerLoopMode::SLM_LoopSelectionRange)
	{
		if (!GetSelectionRange().IsEmpty())
		{
			return GetSelectionRange();
		}
	}

	if (Settings->ShouldEvaluateSubSequencesInIsolation() || ActiveTemplateIDs.Num() == 1)
	{
		return FocusedSequence->GetMovieScene()->GetPlaybackRange();
	}

	return SubSequenceRange;
}


void FSequencer::SetViewRange(TRange<double> NewViewRange, EViewRangeInterpolation Interpolation)
{
	if (!ensure(NewViewRange.HasUpperBound() && NewViewRange.HasLowerBound() && !NewViewRange.IsDegenerate()))
	{
		return;
	}

	const float AnimationLengthSeconds = Interpolation == EViewRangeInterpolation::Immediate ? 0.f : 0.1f;
	if (AnimationLengthSeconds != 0.f)
	{
		if (ZoomAnimation.GetCurve(0).DurationSeconds != AnimationLengthSeconds)
		{
			ZoomAnimation = FCurveSequence();
			ZoomCurve = ZoomAnimation.AddCurve(0.f, AnimationLengthSeconds, ECurveEaseFunction::QuadIn);
		}

		if (!ZoomAnimation.IsPlaying())
		{
			LastViewRange = TargetViewRange;
			ZoomAnimation.Play( SequencerWidget.ToSharedRef() );
		}
		TargetViewRange = NewViewRange;
	}
	else
	{
		TargetViewRange = LastViewRange = NewViewRange;
		ZoomAnimation.JumpToEnd();
	}


	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			FMovieSceneEditorData& EditorData = FocusedMovieScene->GetEditorData();
			EditorData.ViewStart = TargetViewRange.GetLowerBoundValue();
			EditorData.ViewEnd   = TargetViewRange.GetUpperBoundValue();

			// Always ensure the working range is big enough to fit the view range
			EditorData.WorkStart = FMath::Min(TargetViewRange.GetLowerBoundValue(), EditorData.WorkStart);
			EditorData.WorkEnd   = FMath::Max(TargetViewRange.GetUpperBoundValue(), EditorData.WorkEnd);
		}
	}
}


void FSequencer::OnClampRangeChanged( TRange<double> NewClampRange )
{
	if (!NewClampRange.IsEmpty())
	{
		FMovieSceneEditorData& EditorData =  GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData();

		EditorData.WorkStart = NewClampRange.GetLowerBoundValue();
		EditorData.WorkEnd   = NewClampRange.GetUpperBoundValue();
	}
}

FFrameNumber FSequencer::OnGetNearestKey(FFrameTime InTime, bool bSearchAllTracks)
{
	FFrameNumber NearestKeyTime = InTime.FloorToFrame();

	if (bSearchAllTracks)
	{
		GetAllKeys(SelectedKeyCollection, SMALL_NUMBER);
	}
	else
	{
		GetKeysFromSelection(SelectedKeyCollection, SMALL_NUMBER);
	}

	if (SelectedKeyCollection.IsValid())
	{
		TRange<FFrameNumber> FindRangeBackwards(TRangeBound<FFrameNumber>::Open(), NearestKeyTime);
		TOptional<FFrameNumber> NewTimeBackwards = SelectedKeyCollection->FindFirstKeyInRange(FindRangeBackwards, EFindKeyDirection::Backwards);

		TRange<FFrameNumber> FindRangeForwards(NearestKeyTime, TRangeBound<FFrameNumber>::Open());
		TOptional<FFrameNumber> NewTimeForwards = SelectedKeyCollection->FindFirstKeyInRange(FindRangeForwards, EFindKeyDirection::Forwards);
		if (NewTimeForwards.IsSet())
		{
			if (NewTimeBackwards.IsSet())
			{
				if (FMath::Abs(NewTimeForwards.GetValue() - NearestKeyTime) < FMath::Abs(NewTimeBackwards.GetValue() - NearestKeyTime))
				{
					NearestKeyTime = NewTimeForwards.GetValue();
				}
				else
				{
					NearestKeyTime = NewTimeBackwards.GetValue();
				}
			}
			else
			{
				NearestKeyTime = NewTimeForwards.GetValue();
			}
		}
		else if (NewTimeBackwards.IsSet())
		{
			NearestKeyTime = NewTimeBackwards.GetValue();
		}
	}
	return NearestKeyTime;
}

void FSequencer::OnScrubPositionChanged( FFrameTime NewScrubPosition, bool bScrubbing )
{
	bool bClampToViewRange = true;

	if (PlaybackState == EMovieScenePlayerStatus::Scrubbing)
	{
		if (!bScrubbing)
		{
			OnEndScrubbing();
		}
		else if (IsAutoScrollEnabled())
		{
			// Clamp to the view range when not auto-scrolling
			bClampToViewRange = false;
	
			UpdateAutoScroll(NewScrubPosition / GetFocusedTickResolution());
			
			// When scrubbing, we animate auto-scrolled scrub position in Tick()
			if (AutoscrubOffset.IsSet())
			{
				return;
			}
		}
	}

	if (bClampToViewRange)
	{
		FFrameRate DisplayRate    = GetFocusedDisplayRate();
		FFrameRate TickResolution = GetFocusedTickResolution();

		FFrameTime LowerBound = (TargetViewRange.GetLowerBoundValue() * TickResolution).CeilToFrame();
		FFrameTime UpperBound = (TargetViewRange.GetUpperBoundValue() * TickResolution).FloorToFrame();

		if (Settings->GetIsSnapEnabled() && Settings->GetSnapPlayTimeToInterval())
		{
			LowerBound = FFrameRate::Snap(LowerBound, TickResolution, DisplayRate);
			UpperBound = FFrameRate::Snap(UpperBound, TickResolution, DisplayRate);
		}

		NewScrubPosition = FMath::Clamp(NewScrubPosition, LowerBound, UpperBound);
	}

	if (!bScrubbing && FSlateApplication::Get().GetModifierKeys().IsShiftDown())
	{
		AutoScrubToTime(NewScrubPosition);
	}
	else
	{
		SetLocalTimeDirectly(NewScrubPosition);
	}
}


void FSequencer::OnBeginScrubbing()
{
	// Pause first since there's no explicit evaluation in the stopped state when OnEndScrubbing() is called
	Pause();

	SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
	SequencerWidget->RegisterActiveTimerForPlayback();

	OnBeginScrubbingDelegate.Broadcast();
}


void FSequencer::OnEndScrubbing()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
	AutoscrubOffset.Reset();
	StopAutoscroll();

	OnEndScrubbingDelegate.Broadcast();
}


void FSequencer::OnPlaybackRangeBeginDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetPlaybackRange_Transaction", "Set Playback Range"));
}


void FSequencer::OnPlaybackRangeEndDrag()
{
	GEditor->EndTransaction();
}


void FSequencer::OnSelectionRangeBeginDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetSelectionRange_Transaction", "Set Selection Range"));
}


void FSequencer::OnSelectionRangeEndDrag()
{
	GEditor->EndTransaction();
}


void FSequencer::StartAutoscroll(float UnitsPerS)
{
	AutoscrollOffset = UnitsPerS;
}


void FSequencer::StopAutoscroll()
{
	AutoscrollOffset.Reset();
	AutoscrubOffset.Reset();
}


void FSequencer::OnToggleAutoScroll()
{
	Settings->SetAutoScrollEnabled(!Settings->GetAutoScrollEnabled());
}


bool FSequencer::IsAutoScrollEnabled() const
{
	return Settings->GetAutoScrollEnabled();
}


void FSequencer::FindInContentBrowser()
{
	if (GetFocusedMovieSceneSequence())
	{
		TArray<UObject*> ObjectsToFocus;
		ObjectsToFocus.Add(GetCurrentAsset());

		GEditor->SyncBrowserToObjects(ObjectsToFocus);
	}
}


UObject* FSequencer::GetCurrentAsset() const
{
	// For now we find the asset by looking at the root movie scene's outer.
	// @todo: this may need refining if/when we support editing movie scene instances
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetOuter();
}

bool FSequencer::IsReadOnly() const
{
	return bReadOnly || (GetFocusedMovieSceneSequence() && GetFocusedMovieSceneSequence()->GetMovieScene()->IsReadOnly());
}

void FSequencer::VerticalScroll(float ScrollAmountUnits)
{
	SequencerWidget->GetTreeView()->ScrollByDelta(ScrollAmountUnits);
}

FGuid FSequencer::AddSpawnable(UObject& Object, UActorFactory* ActorFactory)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	if (!Sequence->AllowsSpawnableObjects())
	{
		return FGuid();
	}

	// Grab the MovieScene that is currently focused.  We'll add our Blueprint as an inner of the
	// MovieScene asset.
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	TValueOrError<FNewSpawnable, FText> Result = SpawnRegister->CreateNewSpawnableType(Object, *OwnerMovieScene, ActorFactory);
	if (!Result.IsValid())
	{
		FNotificationInfo Info(Result.GetError());
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FGuid();
	}

	FNewSpawnable& NewSpawnable = Result.GetValue();

	auto DuplName = [&](FMovieSceneSpawnable& InSpawnable)
	{
		return InSpawnable.GetName() == NewSpawnable.Name;
	};

	int32 Index = 2;
	FString UniqueString;
	while (OwnerMovieScene->FindSpawnable(DuplName))
	{
		NewSpawnable.Name.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		NewSpawnable.Name += UniqueString;
	}

	FGuid NewGuid = OwnerMovieScene->AddSpawnable(NewSpawnable.Name, *NewSpawnable.ObjectTemplate);

	ForceEvaluate();

	return NewGuid;
}

FGuid FSequencer::MakeNewSpawnable( UObject& Object, UActorFactory* ActorFactory, bool bSetupDefaults )
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return FGuid();
	}

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

	FGuid NewGuid = AddSpawnable(Object, ActorFactory);
	if (!NewGuid.IsValid())
	{
		return FGuid();
	}

	TArray<UMovieSceneFolder*> SelectedParentFolders;
	FString NewNodePath;
	CalculateSelectedFolderAndPath(SelectedParentFolders, NewNodePath);

	if (SelectedParentFolders.Num() > 0)
	{
		SelectedParentFolders[0]->AddChildObjectBinding(NewGuid);
	}

	FMovieSceneSpawnable* Spawnable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(NewGuid);
	if (!Spawnable)
	{
		return FGuid();
	}

	// Override spawn ownership during this process to ensure it never gets destroyed
	ESpawnOwnership SavedOwnership = Spawnable->GetSpawnOwnership();
	Spawnable->SetSpawnOwnership(ESpawnOwnership::External);

	// Spawn the object so we can position it correctly, it's going to get spawned anyway since things default to spawned.
	UObject* SpawnedObject = SpawnRegister->SpawnObject(NewGuid, *MovieScene, ActiveTemplateIDs.Top(), *this);

	if (bSetupDefaults)
	{
		FTransformData TransformData;
		SpawnRegister->SetupDefaultsForSpawnable(SpawnedObject, Spawnable->GetGuid(), TransformData, AsShared(), Settings);
	}

	Spawnable->SetSpawnOwnership(SavedOwnership);

	return NewGuid;
}

void FSequencer::AddSubSequence(UMovieSceneSequence* Sequence)
{
	// @todo Sequencer - sub-moviescenes This should be moved to the sub-moviescene editor

	// Grab the MovieScene that is currently focused.  This is the movie scene that will contain the sub-moviescene
	UMovieScene* OwnerMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (OwnerMovieScene->IsReadOnly())
	{
		return;
	}

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );
	OwnerMovieScene->Modify();

	UMovieSceneSubTrack* SubTrack = OwnerMovieScene->AddMasterTrack<UMovieSceneSubTrack>();

	FFrameNumber Duration = ConvertFrameTime(
		Sequence->GetMovieScene()->GetPlaybackRange().Size<FFrameNumber>(),
		Sequence->GetMovieScene()->GetTickResolution(),
		OwnerMovieScene->GetTickResolution()).FloorToFrame();

	SubTrack->AddSequence(Sequence, GetLocalTime().Time.FloorToFrame(), Duration.Value);
}


bool FSequencer::OnHandleAssetDropped(UObject* DroppedAsset, const FGuid& TargetObjectGuid)
{
	bool bWasConsumed = false;
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		bool bWasHandled = TrackEditors[i]->HandleAssetAdded(DroppedAsset, TargetObjectGuid);
		if (bWasHandled)
		{
			// @todo Sequencer - This will crash if multiple editors try to handle a single asset
			// Should we allow this? How should it consume then?
			// gmp 10/7/2015: the user should be presented with a dialog asking what kind of track they want to create
			check(!bWasConsumed);
			bWasConsumed = true;
		}
	}
	return bWasConsumed;
}


// Takes a display node and traverses it's parents to find the nearest track node if any.  Also collects the names of the nodes which make
// up the path from the track node to the display node being checked.  The name path includes the name of the node being checked, but not
// the name of the track node.
void GetParentTrackNodeAndNamePath(TSharedRef<const FSequencerDisplayNode> DisplayNode, TSharedPtr<FSequencerTrackNode>& OutParentTrack, TArray<FName>& OutNamePath )
{
	TArray<FName> PathToTrack;
	PathToTrack.Add( DisplayNode->GetNodeName() );
	TSharedPtr<FSequencerDisplayNode> CurrentParent = DisplayNode->GetParent();

	while ( CurrentParent.IsValid() && CurrentParent->GetType() != ESequencerNode::Track )
	{
		PathToTrack.Add( CurrentParent->GetNodeName() );
		CurrentParent = CurrentParent->GetParent();
	}

	if ( CurrentParent.IsValid() )
	{
		OutParentTrack = StaticCastSharedPtr<FSequencerTrackNode>( CurrentParent );
		for ( int32 i = PathToTrack.Num() - 1; i >= 0; i-- )
		{
			OutNamePath.Add( PathToTrack[i] );
		}
	}
}


bool FSequencer::OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted )
{
	bool bAnythingRemoved = false;
	
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	if (OwnerMovieScene->IsReadOnly())
	{
		return bAnythingRemoved;
	}

	// Remove the selected object from our selection otherwise invisible objects are still selected and it causes confusion with
	// things that are based on having a selection or not.
	TSharedRef<FSequencerDisplayNode> SelectionNodeToRemove = ConstCastSharedRef<FSequencerDisplayNode>(NodeToBeDeleted);
	Selection.RemoveFromSelection(SelectionNodeToRemove);

	if ( NodeToBeDeleted->GetType() == ESequencerNode::Folder )
	{
		// Delete Children
		for ( const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes() )
		{
			OnRequestNodeDeleted( ChildNode );
		}

		// Delete from parent, or root.
		TSharedRef<const FSequencerFolderNode> FolderToBeDeleted = StaticCastSharedRef<const FSequencerFolderNode>(NodeToBeDeleted);
		if ( NodeToBeDeleted->GetParent().IsValid() )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildFolder( &FolderToBeDeleted->GetFolder() );
		}
		else
		{
			UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
			FocusedMovieScene->Modify();
			FocusedMovieScene->GetRootFolders().Remove( &FolderToBeDeleted->GetFolder() );
		}

		bAnythingRemoved = true;
	}
	else if (NodeToBeDeleted->GetType() == ESequencerNode::Object)
	{
		// Delete any child object bindings
		for (const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes())
		{
			if (ChildNode->GetType() == ESequencerNode::Object)
			{
				OnRequestNodeDeleted(ChildNode);
			}
		}

		const FGuid& BindingToRemove = StaticCastSharedRef<const FSequencerObjectBindingNode>( NodeToBeDeleted )->GetObjectBinding();

		// Remove from a parent folder if necessary.
		if ( NodeToBeDeleted->GetParent().IsValid() && NodeToBeDeleted->GetParent()->GetType() == ESequencerNode::Folder )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildObjectBinding( BindingToRemove );
		}
		
		// Try to remove as a spawnable first
		if (OwnerMovieScene->RemoveSpawnable(BindingToRemove))
		{
			SpawnRegister->DestroySpawnedObject(BindingToRemove, ActiveTemplateIDs.Top(), *this);
		}
		// The guid should be associated with a possessable if it wasnt a spawnable
		else if (OwnerMovieScene->RemovePossessable(BindingToRemove))
		{
			Sequence->Modify();
			Sequence->UnbindPossessableObjects( BindingToRemove );
		}

		bAnythingRemoved = true;
	}
	else if( NodeToBeDeleted->GetType() == ESequencerNode::Track  )
	{
		TSharedRef<const FSequencerTrackNode> SectionAreaNode = StaticCastSharedRef<const FSequencerTrackNode>( NodeToBeDeleted );
		UMovieSceneTrack* Track = SectionAreaNode->GetTrack();

		// Remove from a parent folder if necessary.
		if ( NodeToBeDeleted->GetParent().IsValid() && NodeToBeDeleted->GetParent()->GetType() == ESequencerNode::Folder )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildMasterTrack( Track );
		}

		if (Track != nullptr)
		{
			// Remove sub tracks belonging to this row only
			if (SectionAreaNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::SubTrack)
			{
				SectionAreaNode->GetTrack()->Modify();
				TSet<TWeakObjectPtr<UMovieSceneSection> > SectionsToDelete;
				for (TSharedRef<ISequencerSection> SectionToDelete : SectionAreaNode->GetSections())
				{
					UMovieSceneSection* Section = SectionToDelete->GetSectionObject();
					if (Section)
					{
						SectionsToDelete.Add(Section);
					}
				}
				DeleteSections(SectionsToDelete);
				SectionAreaNode->GetTrack()->FixRowIndices();
			}
			else
			{
				OwnerMovieScene->Modify();
				if (OwnerMovieScene->IsAMasterTrack(*Track))
				{
					OwnerMovieScene->RemoveMasterTrack(*Track);
				}
				else if (OwnerMovieScene->GetCameraCutTrack() == Track)
				{
					OwnerMovieScene->RemoveCameraCutTrack();
				}
				else
				{
					OwnerMovieScene->RemoveTrack(*Track);
				}
			}
		
			bAnythingRemoved = true;
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::Category )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath(NodeToBeDeleted, ParentTrackNode, PathFromTrack);
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteCategory( PathFromTrack );
			}
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::KeyArea )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath( NodeToBeDeleted, ParentTrackNode, PathFromTrack );
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteKeyArea( PathFromTrack );
			}
		}
	}

	return bAnythingRemoved;
}

bool FSequencer::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjects) const
{
	// Check if we care about the undo/redo
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjects)
	{
		if (TransactionObjectPair.Value.HasPendingKillChange() || TransactionObjectPair.Key->GetClass()->IsChildOf(UMovieSceneSignedObject::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

void FSequencer::PostUndo(bool bSuccess)
{
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::Unknown );
	SynchronizeSequencerSelectionWithExternalSelection();
	SyncCurveEditorToSelection(false);
	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());
}

void FSequencer::OnNewActorsDropped(const TArray<UObject*>& DroppedObjects, const TArray<AActor*>& DroppedActors)
{
	bool bAddSpawnable = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	bool bAddPossessable = FSlateApplication::Get().GetModifierKeys().IsControlDown();

	if (bAddSpawnable || bAddPossessable)
	{
		TArray<AActor*> SpawnedActors;

		const FScopedTransaction Transaction(LOCTEXT("UndoAddActors", "Add Actors to Sequencer"));
		
		UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
		UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

		if (OwnerMovieScene->IsReadOnly())
		{
			return;
		}

		Sequence->Modify();

		for ( AActor* Actor : DroppedActors )
		{
			AActor* NewActor = Actor;
			bool bCreateAndAttachCamera = false;
			if (NewActor->GetClass() == ACameraRig_Rail::StaticClass() ||
				NewActor->GetClass() == ACameraRig_Crane::StaticClass())
			{
				bCreateAndAttachCamera = true;
			}

			FGuid PossessableGuid = CreateBinding(*NewActor, NewActor->GetActorLabel());
			FGuid NewGuid = PossessableGuid;

			OnActorAddedToSequencerEvent.Broadcast(NewActor, PossessableGuid);

			if (bAddSpawnable)
			{
				FMovieSceneSpawnable* Spawnable = ConvertToSpawnableInternal(PossessableGuid);

				ForceEvaluate();

				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Spawnable->GetGuid(), ActiveTemplateIDs.Top()))
				{
					AActor* SpawnedActor = Cast<AActor>(WeakObject.Get());
					if (SpawnedActor)
					{
						SpawnedActors.Add(SpawnedActor);
						NewActor = SpawnedActor;
					}
				}

				NewGuid = Spawnable->GetGuid();
			}

			if (bCreateAndAttachCamera)
			{
				ACameraRig_Rail* RailActor = nullptr;
				if (Actor->GetClass() == ACameraRig_Rail::StaticClass())
				{
					RailActor = Cast<ACameraRig_Rail>(NewActor);
				}

				// Create a cine camera actor
				UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
				ACineCameraActor* NewCamera = PlaybackContext->SpawnActor<ACineCameraActor>();
				FGuid NewCameraGuid = CreateBinding(*NewCamera, NewCamera->GetActorLabel());

				if (RailActor)
				{
					NewCamera->SetActorRotation(FRotator(0.f, -90.f, 0.f));
				}

				OnActorAddedToSequencerEvent.Broadcast(NewCamera, NewCameraGuid);

				if (bAddSpawnable)
				{
					FMovieSceneSpawnable* Spawnable = ConvertToSpawnableInternal(NewCameraGuid);

					ForceEvaluate();

					for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Spawnable->GetGuid(), ActiveTemplateIDs.Top()))
					{
						NewCamera = Cast<ACineCameraActor>(WeakObject.Get());
						if (NewCamera)
						{
							break;
						}
					}

					NewCameraGuid = Spawnable->GetGuid();

					// Create an attach track
					UMovieScene3DAttachTrack* AttachTrack = Cast<UMovieScene3DAttachTrack>(OwnerMovieScene->AddTrack(UMovieScene3DAttachTrack::StaticClass(), NewCameraGuid));

					FMovieSceneObjectBindingID AttachBindingID(NewGuid, MovieSceneSequenceID::Root);
					FFrameNumber StartTime = MovieScene::DiscreteInclusiveLower(GetPlaybackRange());
					FFrameNumber Duration  = MovieScene::DiscreteSize(GetPlaybackRange());

					AttachTrack->AddConstraint(StartTime, Duration.Value, NAME_None, NAME_None, AttachBindingID);
				}
				else
				{
					// Parent it
					NewCamera->AttachToActor(NewActor, FAttachmentTransformRules::KeepRelativeTransform);
				}

				if (RailActor)
				{
					// Extend the rail a bit
					if (RailActor->GetRailSplineComponent()->GetNumberOfSplinePoints() == 2)
					{
						FVector SplinePoint1 = RailActor->GetRailSplineComponent()->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
						FVector SplinePoint2 = RailActor->GetRailSplineComponent()->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local);
						FVector SplineDirection = SplinePoint2 - SplinePoint1;
						SplineDirection.Normalize();

						float DefaultRailDistance = 650.f;
						SplinePoint2 = SplinePoint1 + SplineDirection* DefaultRailDistance;
						RailActor->GetRailSplineComponent()->SetLocationAtSplinePoint(1, SplinePoint2, ESplineCoordinateSpace::Local);
						RailActor->GetRailSplineComponent()->bSplineHasBeenEdited = true;
					}

					// Create a track for the CurrentPositionOnRail
					FPropertyPath PropertyPath;
					PropertyPath.AddProperty(FPropertyInfo(RailActor->GetClass()->FindPropertyByName(TEXT("CurrentPositionOnRail"))));

					FKeyPropertyParams KeyPropertyParams(TArrayBuilder<UObject*>().Add(RailActor), PropertyPath, ESequencerKeyMode::ManualKeyForced);

					FFrameTime OriginalTime = GetLocalTime().Time;

					SetLocalTimeDirectly(MovieScene::DiscreteInclusiveLower(GetPlaybackRange()));
					RailActor->CurrentPositionOnRail = 0.f;
					KeyProperty(KeyPropertyParams);

					SetLocalTimeDirectly(MovieScene::DiscreteExclusiveUpper(GetPlaybackRange())-1);
					RailActor->CurrentPositionOnRail = 1.f;
					KeyProperty(KeyPropertyParams);

					SetLocalTimeDirectly(OriginalTime);
				}

				// New camera added, don't lock the view to the camera because we want to see where the camera rig was placed
				NewCameraAdded(NewCameraGuid);
			}
		}

		if (SpawnedActors.Num())
		{
			const bool bNotifySelectionChanged = true;
			const bool bDeselectBSP = true;
			const bool bWarnAboutTooManyActors = false;
			const bool bSelectEvenIfHidden = false;
	
			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();
			GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );
			for (auto SpawnedActor : SpawnedActors)
			{
				GEditor->SelectActor( SpawnedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden );
			}
			GEditor->GetSelectedActors()->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();
		}

		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );

		SynchronizeSequencerSelectionWithExternalSelection();
	}
}


void FSequencer::UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject, bool bJumpCut) const
{
	AActor* CameraActor = Cast<AActor>(InCameraObject);

	bool bCameraHasBeenCut = bJumpCut;

	if (CameraActor)
	{
		bCameraHasBeenCut = bCameraHasBeenCut || !InViewportClient.IsLockedToActor(CameraActor);
		InViewportClient.SetViewLocation(CameraActor->GetActorLocation());
		InViewportClient.SetViewRotation(CameraActor->GetActorRotation());

		UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraActor);

		if (CameraComponent && CameraComponent->ProjectionMode == ECameraProjectionMode::Type::Perspective)
		{
			if (InViewportClient.GetViewportType() != LVT_Perspective)
			{
				InViewportClient.SetViewportType(LVT_Perspective);
			}
		}
	}
	else
	{
		InViewportClient.ViewFOV = InViewportClient.FOVAngle;
	}


	if (bCameraHasBeenCut)
	{
		InViewportClient.SetIsCameraCut();
	}


	// Set the actor lock.
	InViewportClient.SetMatineeActorLock(CameraActor);
	InViewportClient.bLockedCameraView = CameraActor != nullptr;
	InViewportClient.RemoveCameraRoll();

	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(InCameraObject);
	if (CameraComponent)
	{
		if (bCameraHasBeenCut)
		{
			// tell the camera we cut
			CameraComponent->NotifyCameraCut();
		}

		// enforce aspect ratio.
		if (CameraComponent->AspectRatio == 0)
		{
			InViewportClient.AspectRatio = 1.7f;
		}
		else
		{
			InViewportClient.AspectRatio = CameraComponent->AspectRatio;
		}

		//don't stop the camera from zooming when not playing back
		InViewportClient.ViewFOV = CameraComponent->FieldOfView;

		// If there are selected actors, invalidate the viewports hit proxies, otherwise they won't be selectable afterwards
		if (InViewportClient.Viewport && GEditor->GetSelectedActorCount() > 0)
		{
			InViewportClient.Viewport->InvalidateHitProxy();
		}
	}

	// Update ControllingActorViewInfo, so it is in sync with the updated viewport
	InViewportClient.UpdateViewForLockedActor();
}


void FSequencer::SetShowCurveEditor(bool bInShowCurveEditor)
{
	bShowCurveEditor = bInShowCurveEditor; 
	SequencerWidget->OnCurveEditorVisibilityChanged();
}

void FSequencer::SaveCurrentMovieScene()
{
	// Capture thumbnail
	// Convert UObject* array to FAssetData array
	TArray<FAssetData> AssetDataList;
	AssetDataList.Add(FAssetData(GetCurrentAsset()));

	FViewport* Viewport = GEditor->GetActiveViewport();

	// If there's no active viewport, find any other viewport that allows cinematic preview.
	if (Viewport == nullptr)
	{
		for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			if ((LevelVC == nullptr) || !LevelVC->AllowsCinematicControl())
			{
				continue;
			}

			Viewport = LevelVC->Viewport;
		}
	}

	if ( ensure(GCurrentLevelEditingViewportClient) && Viewport != nullptr )
	{
		bool bIsInGameView = GCurrentLevelEditingViewportClient->IsInGameView();
		GCurrentLevelEditingViewportClient->SetGameView(true);

		//have to re-render the requested viewport
		FLevelEditorViewportClient* OldViewportClient = GCurrentLevelEditingViewportClient;
		//remove selection box around client during render
		GCurrentLevelEditingViewportClient = NULL;

		Viewport->Draw();

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		ContentBrowser.CaptureThumbnailFromViewport(Viewport, AssetDataList);

		//redraw viewport to have the yellow highlight again
		GCurrentLevelEditingViewportClient = OldViewportClient;
		GCurrentLevelEditingViewportClient->SetGameView(bIsInGameView);
		Viewport->Draw();
	}

	OnPreSaveEvent.Broadcast(*this);

	TArray<UPackage*> PackagesToSave;
	TArray<UMovieScene*> MovieScenesToSave;
	MovieSceneHelpers::GetDescendantMovieScenes(GetRootMovieSceneSequence(), MovieScenesToSave);
	for (auto MovieSceneToSave : MovieScenesToSave)
	{
		UPackage* MovieScenePackageToSave = MovieSceneToSave->GetOuter()->GetOutermost();
		if (MovieScenePackageToSave->IsDirty())
		{
			PackagesToSave.Add(MovieScenePackageToSave);
		}
	}

	// If there's more than 1 movie scene to save, prompt the user whether to save all dirty movie scenes.
	const bool bCheckDirty = PackagesToSave.Num() > 1;
	const bool bPromptToSave = PackagesToSave.Num() > 1;

	FEditorFileUtils::PromptForCheckoutAndSave( PackagesToSave, bCheckDirty, bPromptToSave );

	ForceEvaluate();

	OnPostSaveEvent.Broadcast(*this);
}


void FSequencer::SaveCurrentMovieSceneAs()
{
	TSharedPtr<IToolkitHost> MyToolkitHost = GetToolkitHost();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	TArray<UObject*> AssetsToSave;
	AssetsToSave.Add(GetCurrentAsset());

	TArray<UObject*> SavedAssets;
	FEditorFileUtils::SaveAssetsAs(AssetsToSave, SavedAssets);

	if (SavedAssets.Num() == 0)
	{
		return;
	}

	if ((SavedAssets[0] != AssetsToSave[0]) && (SavedAssets[0] != nullptr))
	{
		FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
		AssetEditorManager.CloseAllEditorsForAsset(AssetsToSave[0]);
		AssetEditorManager.OpenEditorForAssets(SavedAssets, EToolkitMode::Standalone, MyToolkitHost.ToSharedRef());
	}
}


TArray<FGuid> FSequencer::AddActors(const TArray<TWeakObjectPtr<AActor> >& InActors, bool bSelectActors)
{
	TArray<FGuid> PossessableGuids;

	if (GetFocusedMovieSceneSequence()->GetMovieScene()->IsReadOnly())
	{
		return PossessableGuids;
	}

	const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object in Sequencer"));
	GetFocusedMovieSceneSequence()->Modify();

	bool bPossessableAdded = false;
	for (TWeakObjectPtr<AActor> WeakActor : InActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			FGuid ExistingGuid = FindObjectId(*Actor, ActiveTemplateIDs.Top());
			if (!ExistingGuid.IsValid())
			{
				FGuid PossessableGuid = CreateBinding(*Actor, Actor->GetActorLabel());
				PossessableGuids.Add(PossessableGuid);

				if (Actor->IsA<ACameraActor>())
				{
					NewCameraAdded(PossessableGuid);
				}

				OnActorAddedToSequencerEvent.Broadcast(Actor, PossessableGuid);
			}
			bPossessableAdded = true;
		}
	}

	if (bPossessableAdded)
	{
		// Check if a folder is selected so we can add the actors to the selected folder.
		TArray<UMovieSceneFolder*> SelectedParentFolders;
		FString NewNodePath;
		if (Selection.GetSelectedOutlinerNodes().Num() > 0)
		{
			for (TSharedRef<FSequencerDisplayNode> SelectedNode : Selection.GetSelectedOutlinerNodes())
			{
				TSharedPtr<FSequencerDisplayNode> CurrentNode = SelectedNode;
				while (CurrentNode.IsValid() && CurrentNode->GetType() != ESequencerNode::Folder)
				{
					CurrentNode = CurrentNode->GetParent();
				}
				if (CurrentNode.IsValid())
				{
					SelectedParentFolders.Add(&StaticCastSharedPtr<FSequencerFolderNode>(CurrentNode)->GetFolder());

					// The first valid folder we find will be used to put the new actors into, so it's the node that we
					// want to know the path from.
					if (NewNodePath.Len() == 0)
					{
						// Add an extra delimiter (".") as we know that the new objects will be appended onto the end of this.
						NewNodePath = FString::Printf(TEXT("%s."), *CurrentNode->GetPathName());

						// Make sure the folder is expanded too so that adding objects to hidden folders become visible.
						CurrentNode->SetExpansionState(true);
					}
				}
			}
		}

		if (bSelectActors)
		{
			// Clear our editor selection so we can make the selection our added actors.
			// This has to be done after we know if the actor is going to be added to a
			// folder, otherwise it causes the folder we wanted to pick to be deselected.
			USelection* SelectedActors = GEditor->GetSelectedActors();
			SelectedActors->BeginBatchSelectOperation();
			SelectedActors->Modify();
			GEditor->SelectNone(false, true);
			for (TWeakObjectPtr<AActor> WeakActor : InActors)
			{
				if (AActor* Actor = WeakActor.Get())
				{
					GEditor->SelectActor(Actor, true, false);
				}
			}
			SelectedActors->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();
		}

		// Add the possessables as children of the first selected folder
		if (SelectedParentFolders.Num() > 0)
		{
			for (const FGuid& Possessable : PossessableGuids)
			{
				SelectedParentFolders[0]->AddChildObjectBinding(Possessable);
			}
		}

		// Now add them all to the selection set to be selected after a tree rebuild.
		if (bSelectActors)
		{
			for (const FGuid& Possessable : PossessableGuids)
			{
				FString PossessablePath = NewNodePath += Possessable.ToString();

				// Object Bindings use their FGuid as their unique key.
				SequencerWidget->AddAdditionalPathToSelectionSet(PossessablePath);
			}
		}

		RefreshTree();

		SynchronizeSequencerSelectionWithExternalSelection();
	}

	return PossessableGuids;
}


void FSequencer::OnSelectedOutlinerNodesChanged()
{
	SynchronizeExternalSelectionWithSequencerSelection();
	SyncCurveEditorToSelection(true);

	FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(GLevelEditorModeTools().GetActiveMode(FSequencerEdMode::EM_SequencerMode));
	if (SequencerEdMode != nullptr)
	{
		AActor* NewlySelectedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
		// If we selected an Actor or a node for an Actor that is a potential autokey candidate, clean up any existing mesh trails
		if (NewlySelectedActor && !NewlySelectedActor->IsEditorOnly())
		{
			SequencerEdMode->CleanUpMeshTrails();
		}
	}

	OnSelectionChangedObjectGuidsDelegate.Broadcast(Selection.GetBoundObjectsGuids());
	OnSelectionChangedTracksDelegate.Broadcast(Selection.GetSelectedTracks());
	TArray<UMovieSceneSection*> SelectedSections;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSectionPtr : Selection.GetSelectedSections())
	{
		if (SelectedSectionPtr.IsValid())
		{
			SelectedSections.Add(SelectedSectionPtr.Get());
		}
	}
	OnSelectionChangedSectionsDelegate.Broadcast(SelectedSections);
}


void FSequencer::SynchronizeExternalSelectionWithSequencerSelection()
{
	if ( bUpdatingSequencerSelection || !IsLevelEditorSequencer() || ExactCast<ULevelSequence>(GetFocusedMovieSceneSequence()) == nullptr )
	{
		return;
	}

	TGuardValue<bool> Guard(bUpdatingExternalSelection, true);

	TSet<AActor*> SelectedSequencerActors;
	TSet<UActorComponent*> SelectedSequencerComponents;

	TSet<TSharedRef<FSequencerDisplayNode> > DisplayNodes = Selection.GetNodesWithSelectedKeysOrSections();
	DisplayNodes.Append(Selection.GetSelectedOutlinerNodes());

	for ( TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes)
	{
		// Get the closest object binding node.
		TSharedPtr<FSequencerDisplayNode> CurrentNode = DisplayNode;
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode;
		while ( CurrentNode.IsValid() )
		{
			if ( CurrentNode->GetType() == ESequencerNode::Object )
			{
				ObjectBindingNode = StaticCastSharedPtr<FSequencerObjectBindingNode>(CurrentNode);
				break;
			}
			CurrentNode = CurrentNode->GetParent();
		}

		// If the closest node is an object node, try to get the actor/component nodes from it.
		if ( ObjectBindingNode.IsValid() )
		{
			for (auto RuntimeObject : FindBoundObjects(ObjectBindingNode->GetObjectBinding(), ActiveTemplateIDs.Top()) )
			{
				AActor* Actor = Cast<AActor>(RuntimeObject.Get());
				if ( Actor != nullptr )
				{
					SelectedSequencerActors.Add( Actor );
				}

				UActorComponent* ActorComponent = Cast<UActorComponent>(RuntimeObject.Get());
				if ( ActorComponent != nullptr )
				{
					SelectedSequencerComponents.Add( ActorComponent );

					Actor = ActorComponent->GetOwner();
					if ( Actor != nullptr )
					{
						SelectedSequencerActors.Add( Actor );
					}
				}
			}
		}
	}

	const bool bNotifySelectionChanged = false;
	const bool bDeselectBSP = true;
	const bool bWarnAboutTooManyActors = false;
	const bool bSelectEvenIfHidden = true;

	if (SelectedSequencerComponents.Num() + SelectedSequencerActors.Num() == 0)
	{
		if (GEditor->GetSelectedActorCount())
		{
			const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "UpdatingActorComponentSelectionNone", "Select None" ) );
			GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );
			GEditor->NoteSelectionChange();
		}
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "UpdatingActorComponentSelection", "Select Actors/Components" ) );


	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );

	for (AActor* SelectedSequencerActor : SelectedSequencerActors)
	{
		ULevel* ActorLevel = SelectedSequencerActor->GetLevel();
		if (!FLevelUtils::IsLevelLocked(ActorLevel))
		{
			GEditor->SelectActor(SelectedSequencerActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
		}
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();

	if (SelectedSequencerComponents.Num())
	{
		GEditor->GetSelectedComponents()->Modify();
		GEditor->GetSelectedComponents()->BeginBatchSelectOperation();

		for (UActorComponent* SelectedSequencerComponent : SelectedSequencerComponents)
		{
			if (!FLevelUtils::IsLevelLocked(SelectedSequencerComponent->GetOwner()->GetLevel()))
			{
				GEditor->SelectComponent(SelectedSequencerComponent, true, bNotifySelectionChanged, bSelectEvenIfHidden);
			}
		}

		GEditor->GetSelectedComponents()->EndBatchSelectOperation();
	}
		
	GEditor->NoteSelectionChange();
}


void GetRootObjectBindingNodes(const TArray<TSharedRef<FSequencerDisplayNode>>& DisplayNodes, TArray<TSharedRef<FSequencerObjectBindingNode>>& RootObjectBindings )
{
	for ( TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes )
	{
		switch ( DisplayNode->GetType() )
		{
		case ESequencerNode::Folder:
			GetRootObjectBindingNodes( DisplayNode->GetChildNodes(), RootObjectBindings );
			break;
		case ESequencerNode::Object:
			RootObjectBindings.Add( StaticCastSharedRef<FSequencerObjectBindingNode>( DisplayNode ) );
			break;
		}
	}
}


void FSequencer::SynchronizeSequencerSelectionWithExternalSelection()
{
	if ( bUpdatingExternalSelection || !IsLevelEditorSequencer() || ExactCast<ULevelSequence>(GetFocusedMovieSceneSequence()) == nullptr)
	{
		return;
	}

	TGuardValue<bool> Guard(bUpdatingSequencerSelection, true);

	// If all nodes are already selected, do nothing. This ensures that when an undo event happens, 
	// nodes are not cleared and reselected, which can cause issues with the curve editor auto-fitting 
	// based on selection.
	bool bAllAlreadySelected = true;

	USelection* ActorSelection = GEditor->GetSelectedActors();
	
	// Get the selected sequencer keys for viewport interaction
	TArray<ASequencerKeyActor*> SelectedSequencerKeyActors;
	ActorSelection->GetSelectedObjects<ASequencerKeyActor>(SelectedSequencerKeyActors);

	TSet<TSharedRef<FSequencerDisplayNode>> NodesToSelect;
	for (auto ObjectBinding : NodeTree->GetObjectBindingMap() )
	{
		if (!ObjectBinding.Value.IsValid())
		{
			continue;
		}

		TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = ObjectBinding.Value.ToSharedRef();
		for ( TWeakObjectPtr<UObject> RuntimeObjectPtr : FindBoundObjects(ObjectBindingNode->GetObjectBinding(), ActiveTemplateIDs.Top()) )
		{
			UObject* RuntimeObject = RuntimeObjectPtr.Get();
			if ( RuntimeObject != nullptr)
			{
				for (ASequencerKeyActor* KeyActor : SelectedSequencerKeyActors)
				{
					if (KeyActor->IsEditorOnly())
					{
						AActor* TrailActor = KeyActor->GetAssociatedActor();
						if (TrailActor != nullptr && RuntimeObject == TrailActor)
						{
							NodesToSelect.Add(ObjectBindingNode);
							bAllAlreadySelected = false;
							break;
						}
					}
				}

				bool bActorSelected = ActorSelection->IsSelected( RuntimeObject );
				bool bComponentSelected = GEditor->GetSelectedComponents()->IsSelected( RuntimeObject);

				if (bActorSelected || bComponentSelected)
				{
					NodesToSelect.Add( ObjectBindingNode );

					if (bAllAlreadySelected)
					{
						bool bAlreadySelected = Selection.IsSelected(ObjectBindingNode);

						if (!bAlreadySelected)
						{
							TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;
							SequencerHelpers::GetDescendantNodes(ObjectBindingNode, DescendantNodes);

							for (auto DescendantNode : DescendantNodes)
							{
								if (Selection.IsSelected(DescendantNode) || Selection.NodeHasSelectedKeysOrSections(DescendantNode))
								{
									bAlreadySelected = true;
									break;
								}
							}
						}

						if (!bAlreadySelected)
						{
							bAllAlreadySelected = false;
						}
					}
				}
				else if (Selection.IsSelected(ObjectBindingNode))
				{
					bAllAlreadySelected = false;
				}
			}
		}
	}

	if (!bAllAlreadySelected || NodesToSelect.Num() == 0)
	{
		Selection.SuspendBroadcast();
		Selection.EmptySelectedOutlinerNodes();
		for ( TSharedRef<FSequencerDisplayNode> NodeToSelect : NodesToSelect)
		{
			Selection.AddToSelection( NodeToSelect );
		}
		Selection.ResumeBroadcast();
		Selection.GetOnOutlinerNodeSelectionChanged().Broadcast();
	}
}

bool FSequencer::IsBindingVisible(const FMovieSceneBinding& InBinding)
{
	if (Settings->GetShowSelectedNodesOnly() && OnGetIsBindingVisible().IsBound())
	{
		return OnGetIsBindingVisible().Execute(InBinding);
	}

	return true;
}

bool FSequencer::IsTrackVisible(const UMovieSceneTrack* InTrack)
{
	if (Settings->GetShowSelectedNodesOnly() && OnGetIsTrackVisible().IsBound())
	{
		return OnGetIsTrackVisible().Execute(InTrack);
	}

	return true;
}

void FSequencer::OnSelectedNodesOnlyChanged()
{
	RefreshTree();
	
	SynchronizeSequencerSelectionWithExternalSelection();
}

void GatherKeyAreas(const TSet<TSharedRef<FSequencerDisplayNode>>& Selection, ECurveEditorCurveVisibility CurveVisibility, bool bAddChildren, TSharedRef<FSequencerDisplayNode> InNode, TSet<TSharedPtr<IKeyArea>>& OutKeyAreasToShow)
{
	// If we're only adding selected curves, and we've encountered a selected node, add all its child key areas
	if (CurveVisibility == ECurveEditorCurveVisibility::SelectedCurves && Selection.Contains(InNode))
	{
		bAddChildren = true;
	}

	if (bAddChildren)
	{
		TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode;
		if (InNode->GetType() == ESequencerNode::Track)
		{
			KeyAreaNode = StaticCastSharedRef<FSequencerTrackNode>(InNode)->GetTopLevelKeyNode();
		}
		else if (InNode->GetType() == ESequencerNode::KeyArea)
		{
			KeyAreaNode = StaticCastSharedRef<FSequencerSectionKeyAreaNode>(InNode);
		}

		if (KeyAreaNode.IsValid())
		{
			for (TSharedPtr<IKeyArea> KeyArea : KeyAreaNode->GetAllKeyAreas())
			{
				OutKeyAreasToShow.Add(KeyArea);
			}
		}
	}

	for (TSharedRef<FSequencerDisplayNode> Child : InNode->GetChildNodes())
	{
		GatherKeyAreas(Selection, CurveVisibility, bAddChildren, Child, OutKeyAreasToShow);
	}
}

void FSequencer::SyncCurveEditorToSelection(bool bOutlinerSelectionChanged)
{
	if (!GetShowCurveEditor())
	{
		return;
	}

	ECurveEditorCurveVisibility CurveVisibility = Settings->GetCurveVisibility();
	const TSet<TSharedRef<FSequencerDisplayNode>>& OutlinerSelection = Selection.GetSelectedOutlinerNodes();

	// Traverse the tree to gather key areas
	TSet<TSharedPtr<IKeyArea>> KeyAreasToShow;
	for (const TSharedRef<FSequencerDisplayNode>& Node : NodeTree->GetRootNodes())
	{
		bool bAddChildren = CurveVisibility != ECurveEditorCurveVisibility::SelectedCurves;
		GatherKeyAreas(OutlinerSelection, CurveVisibility, bAddChildren, Node, KeyAreasToShow);
	}

	// Remove unanimated curves if necessary
	if (CurveVisibility == ECurveEditorCurveVisibility::AnimatedCurves)
	{
		TArray<TSharedPtr<IKeyArea>> UnanimatedCurves;
		for (TSharedPtr<IKeyArea> KeyArea : KeyAreasToShow)
		{
			const FMovieSceneChannel* Channel = KeyArea->ResolveChannel();
			if (Channel && Channel->GetNumKeys() == 0)
			{
				UnanimatedCurves.Add(KeyArea);
			}
		}

		for (TSharedPtr<IKeyArea> Unanimated : UnanimatedCurves)
		{
			KeyAreasToShow.Remove(Unanimated);
		}
	}

	// Cache the curve editor's current selection
	TMap<const void*, FCurveModelID> ExistingCurveIDs;
	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditorModel->GetCurves())
	{
		if (const void* Ptr = Pair.Value->GetCurve())
		{
			ExistingCurveIDs.Add(Ptr, Pair.Key);
		}
	}

	bool bAnythingChanged = false;

	// Add newly selected curves to the curve editor
	for (TSharedPtr<IKeyArea> KeyArea : KeyAreasToShow)
	{
		const FMovieSceneChannel* Channel = KeyArea->ResolveChannel();
		if (!Channel)
		{
			continue;
		}
		if (KeyArea->GetOwningSection()->ShowCurveForChannel(Channel))
		{
			if (ExistingCurveIDs.Contains(Channel))
			{
				ExistingCurveIDs.Remove(Channel);
				continue;
			}
			else
			{
				TUniquePtr<FCurveModel> NewCurve = KeyArea->CreateCurveEditorModel(AsShared());
				if (NewCurve.IsValid())
				{
					bAnythingChanged = true;
					CurveEditorModel->AddCurve(MoveTemp(NewCurve));
				}
			}
		}
	}

	// Remove anything that's no longer selected or shown
	for (const TTuple<const void*, FCurveModelID>& Pair : ExistingCurveIDs)
	{
		bAnythingChanged = true;
		CurveEditorModel->RemoveCurve(Pair.Value);
	}

	if (bAnythingChanged && CurveEditorModel->ShouldAutoFrame())
	{
		CurveEditorModel->ZoomToFit();
	}
	else if (bOutlinerSelectionChanged && CurveEditorModel->ShouldAutoFrame())
	{
		// If outliner selection changes, zoom to fit only the selected curves
		TSet<TSharedPtr<IKeyArea>> SelectedKeyAreasToShow;
		for (const TSharedRef<FSequencerDisplayNode>& Node : NodeTree->GetRootNodes())
		{
			GatherKeyAreas(OutlinerSelection, ECurveEditorCurveVisibility::SelectedCurves, false, Node, SelectedKeyAreasToShow);
		}
	
		TArray<FCurveModelID> CurveModelIDs;
		for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditorModel->GetCurves())
		{
			if (const void* Ptr = Pair.Value->GetCurve())
			{
				for (TSharedPtr<IKeyArea> KeyArea : SelectedKeyAreasToShow)
				{
					const FMovieSceneChannel* Channel = KeyArea->ResolveChannel();
					if (Channel && Ptr == Channel)
					{
						CurveModelIDs.Add(Pair.Key);
						break;
					}
				}
			}
		}

		if (CurveModelIDs.Num())
		{
			CurveEditorModel->ZoomToFitCurves(MakeArrayView(CurveModelIDs.GetData(), CurveModelIDs.Num()));
		}
	}
}

void FSequencer::ZoomToSelectedSections()
{
	FFrameRate TickResolution = GetFocusedTickResolution();

	TRange<FFrameNumber> BoundsHull = TRange<FFrameNumber>::Empty();
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		BoundsHull = TRange<FFrameNumber>::Hull(SelectedSection->GetRange(), BoundsHull);
	}

	if (BoundsHull.IsEmpty())
	{
		BoundsHull = GetTimeBounds();
	}

	if (!BoundsHull.IsEmpty() && !BoundsHull.IsDegenerate())
	{
		const double Tolerance = KINDA_SMALL_NUMBER;

		// Zoom back to last view range if already expanded
		if (!ViewRangeBeforeZoom.IsEmpty() &&
			FMath::IsNearlyEqual(BoundsHull.GetLowerBoundValue() / TickResolution, GetViewRange().GetLowerBoundValue(), Tolerance) &&
			FMath::IsNearlyEqual(BoundsHull.GetUpperBoundValue() / TickResolution, GetViewRange().GetUpperBoundValue(), Tolerance))
		{
			SetViewRange(ViewRangeBeforeZoom, EViewRangeInterpolation::Animated);
		}
		else
		{
			ViewRangeBeforeZoom = GetViewRange();

			SetViewRange(BoundsHull / TickResolution, EViewRangeInterpolation::Animated);
		}
	}
}


bool FSequencer::CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const
{
	return ObjectChangeListener->CanKeyProperty(CanKeyPropertyParams);
} 


void FSequencer::KeyProperty(FKeyPropertyParams KeyPropertyParams) 
{
	ObjectChangeListener->KeyProperty(KeyPropertyParams);
}


FSequencerSelection& FSequencer::GetSelection()
{
	return Selection;
}


FSequencerSelectionPreview& FSequencer::GetSelectionPreview()
{
	return SelectionPreview;
}

void FSequencer::GetSelectedTracks(TArray<UMovieSceneTrack*>& OutSelectedTracks)
{
	OutSelectedTracks.Append(Selection.GetSelectedTracks());
}

void FSequencer::GetSelectedSections(TArray<UMovieSceneSection*>& OutSelectedSections)
{
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		if (SelectedSection.IsValid())
		{
			OutSelectedSections.Add(SelectedSection.Get());
		}
	}
}

void FSequencer::SelectObject(FGuid ObjectBinding)
{
	const TSharedPtr<FSequencerObjectBindingNode>* Node = NodeTree->GetObjectBindingMap().Find(ObjectBinding);
	if (Node != nullptr && Node->IsValid())
	{
		GetSelection().Empty();
		GetSelection().AddToSelection(Node->ToSharedRef());
	}
}

void FSequencer::SelectTrack(UMovieSceneTrack* Track)
{
	for (TSharedRef<FSequencerDisplayNode> Node : NodeTree->GetAllNodes())
	{
		if (Node->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
			UMovieSceneTrack* TrackForNode = TrackNode->GetTrack();
			if (TrackForNode == Track)
			{
				Selection.AddToSelection(Node);
				break;
			}
		}
	}
}

void FSequencer::SelectSection(UMovieSceneSection* Section)
{
	Selection.AddToSelection(Section);
}

void FSequencer::SelectKey(UMovieSceneSection* InSection, TSharedPtr<IKeyArea> KeyArea, FKeyHandle KeyHandle, bool bToggle)
{
	FSequencerSelectedKey SelectedKey(*InSection, KeyArea, KeyHandle);

	if (bToggle && Selection.IsSelected(SelectedKey))
	{
		Selection.RemoveFromSelection(SelectedKey);
	}
	else
	{
		Selection.AddToSelection(SelectedKey);
	}
}

void FSequencer::SelectByPropertyPaths(const TArray<FString>& InPropertyPaths)
{
	TArray<TSharedRef<FSequencerDisplayNode>> NodesToSelect;
	for (const TSharedRef<FSequencerDisplayNode>& Node : NodeTree->GetAllNodes())
	{
		if (Node->GetType() == ESequencerNode::Track)
		{
			if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(StaticCastSharedRef<FSequencerTrackNode>(Node)->GetTrack()))
			{
				for (const FString& PropertyPath : InPropertyPaths)
				{
					if (PropertyTrack->GetPropertyPath() == PropertyPath)
					{
						NodesToSelect.Add(Node);
						break;
					}
				}
			}
		}
	}

	Selection.SuspendBroadcast();
	Selection.Empty();
	Selection.ResumeBroadcast();

	if (NodesToSelect.Num())
	{
		Selection.AddToSelection(NodesToSelect);
	}
}

void FSequencer::EmptySelection()
{
	Selection.Empty();
}

void FSequencer::ThrobKeySelection()
{
	SSequencerSection::ThrobKeySelection();
}

void FSequencer::ThrobSectionSelection()
{
	SSequencerSection::ThrobSectionSelection();
}

float FSequencer::GetOverlayFadeCurve() const
{
	return OverlayCurve.GetLerp();
}


void FSequencer::DeleteSelectedItems()
{
	if (Selection.GetSelectedKeys().Num())
	{
		FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteKeys_Transaction", "Delete Keys") );
		
		DeleteSelectedKeys();
	}
	else if (Selection.GetSelectedSections().Num())
	{
		FScopedTransaction DeleteSectionsTransaction( NSLOCTEXT("Sequencer", "DeleteSections_Transaction", "Delete Sections") );
	
		DeleteSections(Selection.GetSelectedSections());
	}
	else if (Selection.GetSelectedOutlinerNodes().Num())
	{
		DeleteSelectedNodes();
	}
}


void FSequencer::AssignActor(FMenuBuilder& MenuBuilder, FGuid InObjectBinding)
{
	TSet<const AActor*> BoundObjects;
	{
		for (TWeakObjectPtr<> Ptr : FindObjectsInCurrentSequence(InObjectBinding))
		{
			if (const AActor* Actor = Cast<AActor>(Ptr.Get()))
			{
				BoundObjects.Add(Actor);
			}
		}
	}

	auto IsActorValidForAssignment = [BoundObjects](const AActor* InActor){
		return !BoundObjects.Contains(InActor);
	};

	using namespace SceneOutliner;

	// Set up a menu entry to assign an actor to the object binding node
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate( FActorFilterPredicate::CreateLambda( IsActorValidForAssignment ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SBox )
		.MaxDesiredHeight(400.0f)
		.WidthOverride(300.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					DoAssignActor(&Actor, 1, InObjectBinding);
				})
			)
		];

	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
}


FGuid FSequencer::DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid InObjectBinding)
{
	if (NumActors <= 0)
	{
		return FGuid();
	}

	//@todo: this code doesn't work with multiple actors, or when the existing binding is bound to multiple actors

	AActor* Actor = InActors[0];

	if (Actor == nullptr)
	{
		return FGuid();
	}

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	if (OwnerMovieScene->IsReadOnly())
	{
		return FGuid();
	}

	FScopedTransaction AssignActor( NSLOCTEXT("Sequencer", "AssignActor", "Assign Actor") );

	Actor->Modify();
	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	TArrayView<TWeakObjectPtr<>> RuntimeObjects = FindObjectsInCurrentSequence(InObjectBinding);

	UObject* RuntimeObject = RuntimeObjects.Num() ? RuntimeObjects[0].Get() : nullptr;

	// Replace the object itself
	FMovieScenePossessable NewPossessableActor;
	FGuid NewGuid;
	{
		// Get the object guid to assign, remove the binding if it already exists
		FGuid ParentGuid = FindObjectId(*Actor, ActiveTemplateIDs.Top());
		FString NewActorLabel = Actor->GetActorLabel();
		if (ParentGuid.IsValid())
		{
			OwnerMovieScene->RemovePossessable(ParentGuid);
			OwnerSequence->UnbindPossessableObjects(ParentGuid);
		}

		// Add this object
		NewPossessableActor = FMovieScenePossessable( NewActorLabel, Actor->GetClass());
		NewGuid = NewPossessableActor.GetGuid();
		OwnerSequence->BindPossessableObject(NewPossessableActor.GetGuid(), *Actor, GetPlaybackContext());

		// Defer replacing this object until the components have been updated
	}

	auto UpdateComponent = [&]( FGuid OldComponentGuid, UActorComponent* NewComponent )
	{
		// Get the object guid to assign, remove the binding if it already exists
		FGuid NewComponentGuid = FindObjectId( *NewComponent, ActiveTemplateIDs.Top() );
		if ( NewComponentGuid.IsValid() )
		{
			OwnerMovieScene->RemovePossessable( NewComponentGuid );
			OwnerSequence->UnbindPossessableObjects( NewComponentGuid );
		}

		// Add this object
		FMovieScenePossessable NewPossessable( NewComponent->GetName(), NewComponent->GetClass() );
		OwnerSequence->BindPossessableObject( NewPossessable.GetGuid(), *NewComponent, Actor );

		// Replace
		OwnerMovieScene->ReplacePossessable( OldComponentGuid, NewPossessable );
		OwnerSequence->UnbindPossessableObjects( OldComponentGuid );
		State.Invalidate(OldComponentGuid, ActiveTemplateIDs.Top());

		FMovieScenePossessable* ThisPossessable = OwnerMovieScene->FindPossessable( NewPossessable.GetGuid() );
		if ( ensure( ThisPossessable ) )
		{
			ThisPossessable->SetParent( NewGuid );
		}
	};

	// Handle components
	AActor* ActorToReplace = Cast<AActor>(RuntimeObject);
	if (ActorToReplace != nullptr && ActorToReplace->IsActorBeingDestroyed() == false)
	{
		for (UActorComponent* ComponentToReplace : ActorToReplace->GetComponents())
		{
			if (ComponentToReplace != nullptr)
			{
				FGuid ComponentGuid = FindObjectId(*ComponentToReplace, ActiveTemplateIDs.Top());
				if (ComponentGuid.IsValid())
				{
					for (UActorComponent* NewComponent : Actor->GetComponents())
					{
						if (NewComponent->GetFullName(Actor) == ComponentToReplace->GetFullName(ActorToReplace))
						{
							UpdateComponent( ComponentGuid, NewComponent );
						}
					}
				}
			}
		}
	}
	else // If the actor didn't exist, try to find components who's parent guids were the previous actors guid.
	{
		TMap<FString, UActorComponent*> ComponentNameToComponent;
		for ( UActorComponent* Component : Actor->GetComponents() )
		{
			ComponentNameToComponent.Add( Component->GetName(), Component );
		}
		for ( int32 i = 0; i < OwnerMovieScene->GetPossessableCount(); i++ )
		{
			FMovieScenePossessable& OldPossessable = OwnerMovieScene->GetPossessable(i);
			if ( OldPossessable.GetParent() == InObjectBinding )
			{
				UActorComponent** ComponentPtr = ComponentNameToComponent.Find( OldPossessable.GetName() );
				if ( ComponentPtr != nullptr )
				{
					UpdateComponent( OldPossessable.GetGuid(), *ComponentPtr );
				}
			}
		}
	}

	// Replace the actor itself after components have been updated
	OwnerMovieScene->ReplacePossessable(InObjectBinding, NewPossessableActor);
	OwnerSequence->UnbindPossessableObjects(InObjectBinding);

	State.Invalidate(InObjectBinding, ActiveTemplateIDs.Top());

	// Try to fix up folders
	TArray<UMovieSceneFolder*> FoldersToCheck;
	FoldersToCheck.Append(GetFocusedMovieSceneSequence()->GetMovieScene()->GetRootFolders());
	bool bFolderFound = false;
	while ( FoldersToCheck.Num() > 0 && bFolderFound == false )
	{
		UMovieSceneFolder* Folder = FoldersToCheck[0];
		FoldersToCheck.RemoveAt(0);
		if ( Folder->GetChildObjectBindings().Contains( InObjectBinding ) )
		{
			Folder->Modify();
			Folder->RemoveChildObjectBinding( InObjectBinding );
			Folder->AddChildObjectBinding( NewGuid );
			bFolderFound = true;
		}

		for ( UMovieSceneFolder* ChildFolder : Folder->GetChildFolders() )
		{
			FoldersToCheck.Add( ChildFolder );
		}
	}

	RestorePreAnimatedState();

	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );

	return NewGuid;
}


void FSequencer::AddActorsToBinding(FGuid InObjectBinding, const TArray<AActor*>& InActors)
{
	if (!InActors.Num())
	{
		return;
	}

	UClass* ActorClass = nullptr;
	int32 NumRuntimeObjects = 0;

	TArrayView<TWeakObjectPtr<>> ObjectsInCurrentSequence = FindObjectsInCurrentSequence(InObjectBinding);

	for (TWeakObjectPtr<> Ptr : ObjectsInCurrentSequence)
	{
		if (const AActor* Actor = Cast<AActor>(Ptr.Get()))
		{
			ActorClass = Actor->GetClass();
			++NumRuntimeObjects;
		}
	}

	FScopedTransaction AddSelectedToBinding(NSLOCTEXT("Sequencer", "AddSelectedToBinding", "Add Selected to Binding"));

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	// Bind objects
	int32 NumObjectsAdded = 0;
	for (AActor* ActorToAdd : InActors)
	{
		if (!ObjectsInCurrentSequence.Contains(ActorToAdd))
		{
			if (ActorClass == nullptr || UClass::FindCommonBase(ActorToAdd->GetClass(), ActorClass) != nullptr)
			{
				if (ActorClass == nullptr)
				{
					ActorClass = ActorToAdd->GetClass();
				}

				ActorToAdd->Modify();
				OwnerSequence->BindPossessableObject(InObjectBinding, *ActorToAdd, GetPlaybackContext());
				++NumObjectsAdded;
			}
			else
			{
				const FText NotificationText = FText::Format(LOCTEXT("UnableToAssignObject", "Cannot assign object {0}. Expected class {1}"), FText::FromString(ActorToAdd->GetName()), FText::FromString(ActorClass->GetName()));
				FNotificationInfo Info(NotificationText);
				Info.ExpireDuration = 3.f;
				Info.bUseLargeFont = false;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
	}

	// Update label
	if (NumRuntimeObjects + NumObjectsAdded > 0)
	{
		FMovieScenePossessable* Possessable = OwnerMovieScene->FindPossessable(InObjectBinding);
		if (Possessable && ActorClass != nullptr)
		{
			if (NumRuntimeObjects + NumObjectsAdded > 1)
			{
				FString NewLabel = ActorClass->GetName() + FString::Printf(TEXT(" (%d)"), NumRuntimeObjects + NumObjectsAdded);
				Possessable->SetName(NewLabel);
			}
			else if (NumObjectsAdded > 0 && InActors.Num() > 0)
			{
				Possessable->SetName(InActors[0]->GetActorLabel());
			}
		}
	}

	RestorePreAnimatedState();

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencer::ReplaceBindingWithActors(FGuid InObjectBinding, const TArray<AActor*>& InActors)
{
	FScopedTransaction ReplaceBindingWithActors(NSLOCTEXT("Sequencer", "ReplaceBindingWithActors", "Replace Binding with Actors"));

	TArray<AActor*> ExistingActors;
	for (TWeakObjectPtr<> Ptr : FindObjectsInCurrentSequence(InObjectBinding))
	{
		if (AActor* Actor = Cast<AActor>(Ptr.Get()))
		{
			if (!InActors.Contains(Actor))
			{
				ExistingActors.Add(Actor);
			}
		}
	}

	RemoveActorsFromBinding(InObjectBinding, ExistingActors);

	TArray<AActor*> NewActors;
	for (AActor* NewActor : InActors)
	{
		if (!ExistingActors.Contains(NewActor))
		{
			NewActors.Add(NewActor);
		}
	}

	AddActorsToBinding(InObjectBinding, NewActors);
}

void FSequencer::RemoveActorsFromBinding(FGuid InObjectBinding, const TArray<AActor*>& InActors)
{
	if (!InActors.Num())
	{
		return;
	}

	UClass* ActorClass = nullptr;
	int32 NumRuntimeObjects = 0;

	for (TWeakObjectPtr<> Ptr : FindObjectsInCurrentSequence(InObjectBinding))
	{
		if (const AActor* Actor = Cast<AActor>(Ptr.Get()))
		{
			ActorClass = Actor->GetClass();
			++NumRuntimeObjects;
		}
	}

	FScopedTransaction RemoveSelectedFromBinding(NSLOCTEXT("Sequencer", "RemoveSelectedFromBinding", "Remove Selected from Binding"));

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	TArray<UObject*> ObjectsToRemove;
	for (AActor* ActorToRemove : InActors)
	{
		ActorToRemove->Modify();

		ObjectsToRemove.Add(ActorToRemove);
	}
	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	// Unbind objects
	OwnerSequence->UnbindObjects(InObjectBinding, ObjectsToRemove, GetPlaybackContext());

	// Update label
	if (NumRuntimeObjects - ObjectsToRemove.Num() > 0)
	{
		FMovieScenePossessable* Possessable = OwnerMovieScene->FindPossessable(InObjectBinding);
		if (Possessable && ActorClass != nullptr)
		{
			if (NumRuntimeObjects - ObjectsToRemove.Num() > 1)
			{
				FString NewLabel = ActorClass->GetName() + FString::Printf(TEXT(" (%d)"), NumRuntimeObjects - ObjectsToRemove.Num());

				Possessable->SetName(NewLabel);
			}
			else if (ObjectsToRemove.Num() > 0 && InActors.Num() > 0)
			{
				Possessable->SetName(InActors[0]->GetActorLabel());
			}
		}
	}

	RestorePreAnimatedState();

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencer::RemoveAllBindings(FGuid InObjectBinding)
{
	FScopedTransaction RemoveAllBindings(NSLOCTEXT("Sequencer", "RemoveAllBindings", "Remove All Bound Objects"));

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	// Unbind objects
	OwnerSequence->UnbindPossessableObjects(InObjectBinding);

	RestorePreAnimatedState();

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencer::RemoveInvalidBindings(FGuid InObjectBinding)
{
	FScopedTransaction RemoveInvalidBindings(NSLOCTEXT("Sequencer", "RemoveMissing", "Remove Missing Objects"));

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	// Unbind objects
	OwnerSequence->UnbindInvalidObjects(InObjectBinding, GetPlaybackContext());

	// Update label
	UClass* ActorClass = nullptr;

	TArray<AActor*> ValidActors;
	for (TWeakObjectPtr<> Ptr : FindObjectsInCurrentSequence(InObjectBinding))
	{
		if (AActor* Actor = Cast<AActor>(Ptr.Get()))
		{
			ActorClass = Actor->GetClass();
			ValidActors.Add(Actor);
		}
	}

	FMovieScenePossessable* Possessable = OwnerMovieScene->FindPossessable(InObjectBinding);
	if (Possessable && ActorClass != nullptr && ValidActors.Num() != 0)
	{
		if (ValidActors.Num() > 1)
		{
			FString NewLabel = ActorClass->GetName() + FString::Printf(TEXT(" (%d)"), ValidActors.Num());

			Possessable->SetName(NewLabel);
		}
		else
		{
			Possessable->SetName(ValidActors[0]->GetActorLabel());
		}
	}

	RestorePreAnimatedState();

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencer::DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted)
{
	// If this node is selected, delete all selected nodes
	if (GetSelection().IsSelected(NodeToBeDeleted))
	{
		DeleteSelectedNodes();
	}
	else
	{
		const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );
		bool bAnythingDeleted = OnRequestNodeDeleted(NodeToBeDeleted);
		if ( bAnythingDeleted )
		{
			NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemRemoved );
		}
	}
}


void FSequencer::DeleteSelectedNodes()
{
	TSet< TSharedRef<FSequencerDisplayNode> > SelectedNodesCopy = GetSelection().GetSelectedOutlinerNodes();

	if (SelectedNodesCopy.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );

	bool bAnythingDeleted = false;

	for( const TSharedRef<FSequencerDisplayNode>& SelectedNode : SelectedNodesCopy )
	{
		if( !SelectedNode->IsHidden() )
		{
			// Delete everything in the entire node
			TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted = StaticCastSharedRef<const FSequencerDisplayNode>(SelectedNode);
			bAnythingDeleted |= OnRequestNodeDeleted( NodeToBeDeleted );
		}
	}

	if ( bAnythingDeleted )
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemRemoved );
	}
}


void ExportObjectBindingsToText(const TArray<UMovieSceneCopyableBinding*>& ObjectsToExport, FString& ExportedText)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	// Export each of the selected nodes
	UObject* LastOuter = nullptr;

	for (UMovieSceneCopyableBinding* ObjectToExport : ObjectsToExport)
	{
		// The nodes should all be from the same scope
		UObject* ThisOuter = ObjectToExport->GetOuter();
		check((LastOuter == ThisOuter) || (LastOuter == nullptr));
		LastOuter = ThisOuter;

		// We can't use TextExportTransient on USTRUCTS (which our object contains) so we're going to manually null out some references before serializing them. These references are
		// serialized manually into the archive, as the auto-serialization will only store a reference (to a privately owned object) which creates issues on deserialization. Attempting 
		// to deserialize these private objects throws a superflous error in the console that makes it look like things went wrong when they're actually OK and expected.
		TArray<UMovieSceneTrack*> OldTracks = ObjectToExport->Binding.StealTracks();
		UObject* OldSpawnableTemplate = ObjectToExport->Spawnable.GetObjectTemplate();
		ObjectToExport->Spawnable.SetObjectTemplate(nullptr);

		UExporter::ExportToOutputDevice(&Context, ObjectToExport, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);

		// Restore the references (as we don't want to modify the original in the event of a copy operation!)
		ObjectToExport->Binding.SetTracks(OldTracks);
		ObjectToExport->Spawnable.SetObjectTemplate(OldSpawnableTemplate);

		// We manually export the object template for the same private-ownership reason as above. Templates need to be re-created anyways as each Spawnable contains its own copy of the template.
		if (ObjectToExport->SpawnableObjectTemplate)
		{
			UExporter::ExportToOutputDevice(&Context, ObjectToExport->SpawnableObjectTemplate, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);
		}
	}

	ExportedText = Archive;
}

class FObjectBindingTextFactory : public FCustomizableTextObjectFactory
{
public:
	FObjectBindingTextFactory(FSequencer& InSequencer)
		: FCustomizableTextObjectFactory(GWarn)
		, SequencerPtr(&InSequencer)
	{
	}

	// FCustomizableTextObjectFactory implementation
	virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
	{
		if (InObjectClass->IsChildOf<UMovieSceneCopyableBinding>())
		{
			return true;
		}

		return SequencerPtr->GetSpawnRegister().CanSpawnObject(InObjectClass);
	}
	

	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		if (NewObject->IsA<UMovieSceneCopyableBinding>())
		{
			UMovieSceneCopyableBinding* CopyableBinding = Cast<UMovieSceneCopyableBinding>(NewObject);
			NewCopyableBindings.Add(CopyableBinding);
		}
		else
		{
			NewSpawnableObjectTemplates.Add(NewObject);
		}
	}

public:
	TArray<UMovieSceneCopyableBinding*> NewCopyableBindings;
	TArray<UObject*> NewSpawnableObjectTemplates;

private:
	FSequencer* SequencerPtr;
};


void FSequencer::ImportObjectBindingsFromText(const FString& TextToImport, /*out*/ TArray<UMovieSceneCopyableBinding*>& ImportedObjects)
{
	UPackage* TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Sequencer/Editor/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FObjectBindingTextFactory Factory(*this);
	Factory.ProcessBuffer(TempPackage, RF_Transactional, TextToImport);
	ImportedObjects = Factory.NewCopyableBindings;

	// We had to explicitly serialize object templates due to them being a reference to a privately owned object. We now deserialize these object template copies
	// and match them up with their MovieSceneCopyableBinding again.
	
	int32 SpawnableObjectTemplateIndex = 0;
	for (auto ImportedObject : ImportedObjects)
	{
		if (ImportedObject->Spawnable.GetGuid().IsValid() && SpawnableObjectTemplateIndex < Factory.NewSpawnableObjectTemplates.Num())
		{
			// This Spawnable Object Template is owned by our transient package, so you'll need to change the owner if you want to keep it later.
			ImportedObject->SpawnableObjectTemplate = Factory.NewSpawnableObjectTemplates[SpawnableObjectTemplateIndex++];
		}
	}

	// Remove the temp package from the root now that it has served its purpose
	TempPackage->RemoveFromRoot();
}


void FSequencer::CopySelectedObjects(TArray<TSharedPtr<FSequencerObjectBindingNode>>& ObjectNodes, FString& ExportedText)
{
	// Gather guids for the object nodes and any child object nodes
	TSet<FGuid> GuidsToCopy;
	for (TSharedPtr<FSequencerObjectBindingNode> ObjectNode : ObjectNodes)
	{
		GuidsToCopy.Add(ObjectNode->GetObjectBinding());

		TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;

		SequencerHelpers::GetDescendantNodes(ObjectNode.ToSharedRef(), DescendantNodes);

		for (auto DescendantNode : DescendantNodes)
		{
			if (DescendantNode->GetType() == ESequencerNode::Object)
			{
				GuidsToCopy.Add((StaticCastSharedRef<FSequencerObjectBindingNode>(DescendantNode))->GetObjectBinding());
			}
		}
	}

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Export each of the bindings
	TArray<UMovieSceneCopyableBinding*> CopyableBindings;

	for (auto ObjectBinding : GuidsToCopy)
	{
		UMovieSceneCopyableBinding *CopyableBinding = NewObject<UMovieSceneCopyableBinding>(GetTransientPackage(), UMovieSceneCopyableBinding::StaticClass(), NAME_None, RF_Transient);
		CopyableBindings.Add(CopyableBinding);

		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding);
		if (Possessable)
		{
			CopyableBinding->Possessable = *Possessable;
		}
		else
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
			if (Spawnable)
			{
				CopyableBinding->Spawnable = *Spawnable;
				
				// We manually serialize the spawnable object template so that it's not a reference to a privately owned object. Spawnables all have unique copies of their template objects anyways.
				// Object Templates are re-created on paste (based on these templates) with the correct ownership set up.
				CopyableBinding->SpawnableObjectTemplate = Spawnable->GetObjectTemplate();
			}
		}

		const FMovieSceneBinding* Binding = MovieScene->GetBindings().FindByPredicate([=](const FMovieSceneBinding& InBinding){ return InBinding.GetObjectGuid() == ObjectBinding; });
		if (Binding)
		{
			CopyableBinding->Binding = *Binding;
			for (auto Track : Binding->GetTracks())
			{
				// Tracks suffer from the same issues as Spawnable's Object Templates (reference to a privately owned object). We'll manually serialize the tracks to copy them,
				// and then restore them on paste.
				UMovieSceneTrack* DuplicatedTrack = Cast<UMovieSceneTrack>(StaticDuplicateObject(Track, CopyableBinding));

				CopyableBinding->Tracks.Add(DuplicatedTrack);
			}
		}
	}

	ExportObjectBindingsToText(CopyableBindings, /*out*/ ExportedText);
}


void FSequencer::CopySelectedTracks(TArray<TSharedPtr<FSequencerTrackNode>>& TrackNodes, FString& ExportedText)
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	TArray<UObject*> CopyableTracks;
	for (TSharedPtr<FSequencerTrackNode> TrackNode : TrackNodes)
	{
		bool bIsParentSelected = false;
		TSharedPtr<FSequencerDisplayNode> ParentNode = TrackNode->GetParent();
		while (ParentNode.IsValid())
		{
			if (Selection.GetSelectedOutlinerNodes().Contains(ParentNode.ToSharedRef()))
			{
				bIsParentSelected = true;
				break;
			}
			ParentNode = ParentNode->GetParent();
		}

		if (!bIsParentSelected)
		{
			UMovieSceneCopyableTrack *CopyableTrack = NewObject<UMovieSceneCopyableTrack>(GetTransientPackage(), UMovieSceneCopyableTrack::StaticClass(), NAME_None, RF_Transient);
			CopyableTracks.Add(CopyableTrack);

			UMovieSceneTrack* DuplicatedTrack = Cast<UMovieSceneTrack>(StaticDuplicateObject(TrackNode->GetTrack(), CopyableTrack));
			CopyableTrack->Track = DuplicatedTrack;
			CopyableTrack->bIsAMasterTrack = MovieScene->IsAMasterTrack(*TrackNode->GetTrack());
		}
	}

	ExportObjectsToText(CopyableTracks, /*out*/ ExportedText);
}


void FSequencer::ExportObjectsToText(TArray<UObject*> ObjectsToExport, FString& ExportedText)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	// Export each of the selected nodes
	UObject* LastOuter = nullptr;

	for (UObject* ObjectToExport : ObjectsToExport)
	{
		// The nodes should all be from the same scope
		UObject* ThisOuter = ObjectToExport->GetOuter();
		if (LastOuter != nullptr && ThisOuter != LastOuter)
		{
			UE_LOG(LogSequencer, Warning, TEXT("Cannot copy objects from different outers. Only copying from %s"), *LastOuter->GetName());
			continue;
		}
		LastOuter = ThisOuter;

		UExporter::ExportToOutputDevice(&Context, ObjectToExport, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);
	}

	ExportedText = Archive;
}

void FSequencer::DoPaste()
{
	// Grab the text to paste from the clipboard
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	TArray<FNotificationInfo> PasteErrors;
	bool bAnythingPasted = false;
	bAnythingPasted |= PasteObjectBindings(TextToImport, PasteErrors);
	bAnythingPasted |= PasteTracks(TextToImport, PasteErrors);
	
	if (!bAnythingPasted)
	{
		bAnythingPasted |= PasteSections(TextToImport, PasteErrors);
	}

	for (auto NotificationInfo : PasteErrors)
	{
		NotificationInfo.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	}
}

bool FSequencer::PasteObjectBindings(const FString& TextToImport, TArray<FNotificationInfo>& PasteErrors)
{
	TArray<UMovieSceneCopyableBinding*> ImportedBindings;
	ImportObjectBindingsFromText(TextToImport, ImportedBindings);

	if (ImportedBindings.Num() == 0)
	{
		return false;
	}
	
	TArray<UMovieSceneFolder*> SelectedParentFolders;
	FString NewNodePath;
	CalculateSelectedFolderAndPath(SelectedParentFolders, NewNodePath);

	FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	
	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UObject* BindingContext = GetPlaybackContext();

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	TMap<FGuid, FGuid> OldToNewGuidMap;
	TArray<FGuid> PossessableGuids;

	TArray<FMovieSceneBinding> BindingsPasted;
	for (UMovieSceneCopyableBinding* CopyableBinding : ImportedBindings)
	{
		// Clear transient flags on the imported tracks
		for (UMovieSceneTrack* CopiedTrack : CopyableBinding->Tracks)
		{
			CopiedTrack->ClearFlags(RF_Transient);
			TArray<UObject*> Subobjects;
			GetObjectsWithOuter(CopiedTrack, Subobjects);
			for (UObject* Subobject : Subobjects)
			{
				Subobject->ClearFlags(RF_Transient);
			}
		}

		if (CopyableBinding->Possessable.GetGuid().IsValid())
		{
			FGuid NewGuid = FGuid::NewGuid();

			FMovieSceneBinding NewBinding(NewGuid, CopyableBinding->Binding.GetName(), CopyableBinding->Tracks);

			FMovieScenePossessable NewPossessable = CopyableBinding->Possessable;
			NewPossessable.SetGuid(NewGuid);

			MovieScene->AddPossessable(NewPossessable, NewBinding);

			OldToNewGuidMap.Add(CopyableBinding->Possessable.GetGuid(), NewGuid);

			BindingsPasted.Add(NewBinding);

			PossessableGuids.Add(NewGuid);
		}
		else if (CopyableBinding->Spawnable.GetGuid().IsValid())
		{
			// We need to let the sequence create the spawnable so that it has everything set up properly internally.
			// This is required to get spawnables with the correct references to object templates, object templates with
			// correct owners, etc. However, making a new spawnable also creates the binding for us - this is a problem
			// because we need to use our binding (which has tracks associated with it). To solve this, we let it create
			// an object template based off of our (transient package owned) template, then find the newly created binding
			// and update it.
			FGuid NewGuid = MakeNewSpawnable(*CopyableBinding->SpawnableObjectTemplate, nullptr, false);
			FMovieSceneBinding NewBinding(NewGuid, CopyableBinding->Binding.GetName(), CopyableBinding->Tracks);
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(NewGuid);

			// Copy the name of the original spawnable too.
			Spawnable->SetName(CopyableBinding->Spawnable.GetName());

			// Clear the transient flags on the copyable binding before assigning to the new spawnable
			for (auto Track : NewBinding.GetTracks())
			{
				Track->ClearFlags(RF_Transient);
				for (auto Section : Track->GetAllSections())
				{
					Section->ClearFlags(RF_Transient);
				}
			}

			// Replace the auto-generated binding with our deserialized bindings (which has our tracks)
			MovieScene->ReplaceBinding(NewGuid, NewBinding);

			OldToNewGuidMap.Add(CopyableBinding->Spawnable.GetGuid(), NewGuid);

			BindingsPasted.Add(NewBinding);
		}
	}

	// Fix up parent guids
	for (auto PossessableGuid : PossessableGuids)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(PossessableGuid);
		if (Possessable && OldToNewGuidMap.Contains(Possessable->GetParent()))
		{
			Possessable->SetParent(OldToNewGuidMap[Possessable->GetParent()]);
		}
	}

	// Fix possessable actor bindings
	for (int32 PossessableGuidIndex = 0; PossessableGuidIndex < PossessableGuids.Num(); ++PossessableGuidIndex)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(PossessableGuids[PossessableGuidIndex]);
		UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
		if (Possessable && PlaybackContext)
		{
			for (TActorIterator<AActor> ActorItr(PlaybackContext); ActorItr; ++ActorItr)
			{
				AActor *Actor = *ActorItr;
				if (Actor && Actor->GetActorLabel() == *Possessable->GetName())
				{
					FGuid ExistingGuid = FindObjectId(*Actor, ActiveTemplateIDs.Top());

					if (!ExistingGuid.IsValid())
					{
						FGuid NewGuid = DoAssignActor(&Actor, 1, Possessable->GetGuid());

						// If assigning produces a new guid, update the possesable guids and the bindings pasted data
						if (NewGuid.IsValid())
						{
							for (auto BindingPasted : BindingsPasted)
							{
								if (BindingPasted.GetObjectGuid() == PossessableGuids[PossessableGuidIndex])
								{
									BindingPasted.SetObjectGuid(NewGuid);
								}
							}

							PossessableGuids[PossessableGuidIndex] = NewGuid;
						}
					}
				}
			}
		}
	}

	if (SelectedParentFolders.Num() > 0)
	{
		for (auto PossessableGuid : PossessableGuids)
		{
			SelectedParentFolders[0]->AddChildObjectBinding(PossessableGuid);
		}
	}

	OnMovieSceneBindingsPastedDelegate.Broadcast(BindingsPasted);

	// Refresh all immediately so that spawned actors will be generated immediately
	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshAllImmediately);

	// Fix possessable component bindings
	for (auto PossessableGuid : PossessableGuids)
	{
		// If a possessable guid does not have any bound objects, they might be 
		// possessable components for spawnables, so they need to be remapped
		if (FindBoundObjects(PossessableGuid, ActiveTemplateIDs.Top()).Num() == 0)
		{
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(PossessableGuid);
			if (Possessable)
			{
				FGuid ParentGuid = Possessable->GetParent();
				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ParentGuid, ActiveTemplateIDs.Top()))
				{
					if (AActor* SpawnedActor = Cast<AActor>(WeakObject.Get()))
					{
						for (UActorComponent* Component : SpawnedActor->GetComponents())
						{
							if (Component->GetName() == Possessable->GetName())
							{
								OwnerSequence->BindPossessableObject(PossessableGuid, *Component, SpawnedActor);
								break;
							}
						}
					}
				}
			}
		}
	}
	return true;
}

bool FSequencer::PasteTracks(const FString& TextToImport, TArray<FNotificationInfo>& PasteErrors)
{
	TArray<UMovieSceneCopyableTrack*> ImportedTracks;
	FSequencer::ImportTracksFromText(TextToImport, ImportedTracks);

	if (ImportedTracks.Num() == 0)
	{
		return false;
	}
	
	TArray<UMovieSceneFolder*> SelectedParentFolders;
	FString NewNodePath;
	CalculateSelectedFolderAndPath(SelectedParentFolders, NewNodePath);

	FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UObject* BindingContext = GetPlaybackContext();

	TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetSelectedOutlinerNodes();

	TArray<TSharedPtr<FSequencerObjectBindingNode>> ObjectNodes;
	for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
	{
		if (Node->GetType() != ESequencerNode::Object)
		{
			continue;
		}

		TSharedPtr<FSequencerObjectBindingNode> ObjectNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);
		if (ObjectNode.IsValid())
		{
			ObjectNodes.Add(ObjectNode);
		}
	}

	int32 NumMasterTracks = 0;
	int32 NumTracks = 0;

	for (UMovieSceneCopyableTrack* CopyableTrack : ImportedTracks)
	{
		if (CopyableTrack->bIsAMasterTrack)
		{
			++NumMasterTracks;
		}
		else
		{
			++NumTracks;
		}
	}

	int32 NumMasterTracksPasted = 0;
	int32 NumTracksPasted = 0;
	if (ObjectNodes.Num())
	{
		for (TSharedPtr<FSequencerObjectBindingNode> ObjectNode : ObjectNodes)
		{
			FGuid ObjectGuid = ObjectNode->GetObjectBinding();

			TArray<UMovieSceneCopyableTrack*> NewTracks;
			FSequencer::ImportTracksFromText(TextToImport, NewTracks);

			for (UMovieSceneCopyableTrack* CopyableTrack : NewTracks)
			{
				if (!CopyableTrack->bIsAMasterTrack)
				{
					UMovieSceneTrack* NewTrack = CopyableTrack->Track;
					NewTrack->ClearFlags(RF_Transient);
					TArray<UObject*> Subobjects;
					GetObjectsWithOuter(NewTrack, Subobjects);
					for (UObject* Subobject : Subobjects)
					{
						Subobject->ClearFlags(RF_Transient);
					}

					if (!GetFocusedMovieSceneSequence()->GetMovieScene()->AddGivenTrack(NewTrack, ObjectGuid))
					{
						continue;
					}
					else
					{
						++NumTracksPasted;
					}
				}
			}
		}
	}

	// Add as master track or set camera cut track
	for (UMovieSceneCopyableTrack* CopyableTrack : ImportedTracks)
	{
		if (CopyableTrack->bIsAMasterTrack)
		{
			UMovieSceneTrack* NewTrack = CopyableTrack->Track;
			NewTrack->ClearFlags(RF_Transient);
			TArray<UObject*> Subobjects;
			GetObjectsWithOuter(NewTrack, Subobjects);
			for (UObject* Subobject : Subobjects)
			{
				Subobject->ClearFlags(RF_Transient);
			}

			if (NewTrack->IsA(UMovieSceneCameraCutTrack::StaticClass()))
			{
				GetFocusedMovieSceneSequence()->GetMovieScene()->SetCameraCutTrack(NewTrack);
				if (SelectedParentFolders.Num() > 0)
				{
					SelectedParentFolders[0]->AddChildMasterTrack(NewTrack);
				}

				++NumMasterTracksPasted;
			}
			else
			{
				if (GetFocusedMovieSceneSequence()->GetMovieScene()->AddGivenMasterTrack(NewTrack))
				{
					if (SelectedParentFolders.Num() > 0)
					{
						SelectedParentFolders[0]->AddChildMasterTrack(NewTrack);
					}

				}

				++NumMasterTracksPasted;
			}
		}
	}

	if (NumMasterTracksPasted < NumMasterTracks)
	{
		FNotificationInfo Info(LOCTEXT("PasteTracks_NoMasterTracks", "Can't paste track. Master track could not be pasted"));
		PasteErrors.Add(Info);
	}

	if (NumTracksPasted < NumTracks)
	{
		FNotificationInfo Info(LOCTEXT("PasteSections_NoSelectedObjects", "Can't paste track. No selected objects to paste tracks onto"));
		PasteErrors.Add(Info);
	}

	if ((NumMasterTracksPasted + NumTracksPasted) > 0)
	{
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	return true;
}


bool FSequencer::PasteSections(const FString& TextToImport, TArray<FNotificationInfo>& PasteErrors)
{
	TArray<UMovieSceneSection*> ImportedSections;
	FSequencer::ImportSectionsFromText(TextToImport, ImportedSections);

	if (ImportedSections.Num() == 0)
	{
		return false;
	}

	TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetSelectedOutlinerNodes();

	if (SelectedNodes.Num() == 0)
	{
		FNotificationInfo Info(LOCTEXT("PasteSections_NoSelectedTracks", "Can't paste section. No selected tracks to paste sections onto"));
		PasteErrors.Add(Info);
		return false;
	}

	FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());

	FFrameNumber LocalTime = GetLocalTime().Time.GetFrame();

	TOptional<FFrameNumber> FirstFrame;
	for (UMovieSceneSection* Section : ImportedSections)
	{
		if (Section->HasStartFrame())
		{
			if (FirstFrame.IsSet())
			{
				if (FirstFrame.GetValue() > Section->GetInclusiveStartFrame())
				{
					FirstFrame = Section->GetInclusiveStartFrame();
				}
			}
			else
			{
				FirstFrame = Section->GetInclusiveStartFrame();
			}
		}
	}

	TArray<UMovieSceneSection*> NewSections;
	TArray<int32> SectionIndicesImported;

	for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
	{
		if (Node->GetType() != ESequencerNode::Track)
		{
			continue;
		}

		TSharedPtr<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
		if (TrackNode.IsValid())
		{
			UMovieSceneTrack* Track = TrackNode->GetTrack();
			for (int32 SectionIndex = 0; SectionIndex < ImportedSections.Num(); ++SectionIndex)
			{
				UMovieSceneSection* Section = ImportedSections[SectionIndex];

				if (!Track->SupportsType(Section->GetClass()))
				{
					continue;
				}

				SectionIndicesImported.AddUnique(SectionIndex);

				Track->Modify();

				Section->Rename(nullptr, Track);
				Track->AddSection(*Section);

				if (Section->HasStartFrame())
				{
					FFrameNumber NewStartFrame = LocalTime + (Section->GetInclusiveStartFrame() - FirstFrame.GetValue());
					Section->MoveSection(NewStartFrame - Section->GetInclusiveStartFrame());
				}

				NewSections.Add(Section);
			}

			// Regenerate for pasting onto the next track 
			ImportedSections.Empty();
			FSequencer::ImportSectionsFromText(TextToImport, ImportedSections);
		}
	}

	for (int32 SectionIndex = 0; SectionIndex < ImportedSections.Num(); ++SectionIndex)
	{
		if (!SectionIndicesImported.Contains(SectionIndex))
		{
			UE_LOG(LogSequencer, Display, TEXT("Could not paste section of type %s"), *ImportedSections[SectionIndex]->GetClass()->GetName());
		}
	}

	if (SectionIndicesImported.Num() == 0)
	{
		Transaction.Cancel();

		FNotificationInfo Info(LOCTEXT("PasteSections_NothingPasted", "Can't paste section. No matching section types found."));
		PasteErrors.Add(Info);
		return false;
	}

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	EmptySelection();
	for (UMovieSceneSection* NewSection : NewSections)
	{
		SelectSection(NewSection);
	}
	ThrobSectionSelection();

	return true;
}

class FTrackObjectTextFactory : public FCustomizableTextObjectFactory
{
public:
	FTrackObjectTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation
	virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
	{
		if (InObjectClass->IsChildOf(UMovieSceneCopyableTrack::StaticClass()))
		{
			return true;
		}
		return false;
	}


	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		NewTracks.Add(Cast<UMovieSceneCopyableTrack>(NewObject));
	}

public:
	TArray<UMovieSceneCopyableTrack*> NewTracks;
};


class FSectionObjectTextFactory : public FCustomizableTextObjectFactory
{
public:
	FSectionObjectTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation
	virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
	{
		if (InObjectClass->IsChildOf(UMovieSceneSection::StaticClass()))
		{
			return true;
		}
		return false;
	}


	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		NewSections.Add(Cast<UMovieSceneSection>(NewObject));
	}

public:
	TArray<UMovieSceneSection*> NewSections;
};


bool FSequencer::CanPaste(const FString& TextToImport)
{
	FObjectBindingTextFactory ObjectBindingFactory(*this);
	if (ObjectBindingFactory.CanCreateObjectsFromText(TextToImport))
	{
		return true;
	}
		
	FTrackObjectTextFactory TrackFactory;
	if (TrackFactory.CanCreateObjectsFromText(TextToImport))
	{
		return true;
	}

	FSectionObjectTextFactory SectionFactory;
	if (SectionFactory.CanCreateObjectsFromText(TextToImport))
	{
		return true;
	}

	return false;
}

void FSequencer::ImportTracksFromText(const FString& TextToImport, /*out*/ TArray<UMovieSceneCopyableTrack*>& ImportedTracks)
{
	UPackage* TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Sequencer/Editor/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FTrackObjectTextFactory Factory;
	Factory.ProcessBuffer(TempPackage, RF_Transactional, TextToImport);

	ImportedTracks = Factory.NewTracks;

	// Remove the temp package from the root now that it has served its purpose
	TempPackage->RemoveFromRoot();
}


void FSequencer::ImportSectionsFromText(const FString& TextToImport, /*out*/ TArray<UMovieSceneSection*>& ImportedSections)
{
	UPackage* TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Sequencer/Editor/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FSectionObjectTextFactory Factory;
	Factory.ProcessBuffer(TempPackage, RF_Transactional, TextToImport);

	ImportedSections = Factory.NewSections;

	// Remove the temp package from the root now that it has served its purpose
	TempPackage->RemoveFromRoot();
}


void FSequencer::ToggleNodeActive()
{
	bool bIsActive = !IsNodeActive();
	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeActive", "Toggle Node Active") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->Modify();
			Section->SetIsActive(bIsActive);
		}
	}

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
}


bool FSequencer::IsNodeActive() const
{
	// Active if ONE is active, changed in 4.20
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);
		if (Sections.Num() > 0)
		{
			for (auto Section : Sections)
			{
				if (Section->IsActive())
				{
					return true;
				}
			}
			return false;
		}
	}
	return true;
}


void FSequencer::ToggleNodeLocked()
{
	bool bIsLocked = !IsNodeLocked();

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeLocked", "Toggle Node Locked") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->Modify();
			Section->SetIsLocked(bIsLocked);
		}
	}
}


bool FSequencer::IsNodeLocked() const
{
	// Locked only if all are locked
	int NumSections = 0;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsLocked())
			{
				return false;
			}
			++NumSections;
		}
	}
	return NumSections > 0;
}

void FSequencer::SaveSelectedNodesSpawnableState()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT("SaveSpawnableState", "Save spawnable state") );

	MovieScene->Modify();

	TArray<FMovieSceneSpawnable*> Spawnables;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(StaticCastSharedRef<FSequencerObjectBindingNode>(Node)->GetObjectBinding());
			if (Spawnable)
			{
				Spawnables.Add(Spawnable);
			}
		}
	}

	FScopedSlowTask SlowTask(Spawnables.Num(), LOCTEXT("SaveSpawnableStateProgress", "Saving selected spawnables"));
	SlowTask.MakeDialog(true);

	TArray<AActor*> PossessedActors;
	for (FMovieSceneSpawnable* Spawnable : Spawnables)
	{
		SlowTask.EnterProgressFrame();
		
		SpawnRegister->SaveDefaultSpawnableState(*Spawnable, ActiveTemplateIDs.Top(), *this);

		if (GWarn->ReceivedUserCancel())
		{
			break;
		}
	}

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencer::SetSelectedNodesSpawnableLevel(FName InLevelName)
{							
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{	
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT("SetSpawnableLevel", "Set Spawnable Level") );

	MovieScene->Modify();

	TArray<FMovieSceneSpawnable*> Spawnables;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(StaticCastSharedRef<FSequencerObjectBindingNode>(Node)->GetObjectBinding());
			if (Spawnable)
			{
				Spawnable->SetLevelName(InLevelName);
			}
		}
	}
}

void FSequencer::ConvertToSpawnable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted)
{
	if (GetFocusedMovieSceneSequence()->GetMovieScene()->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedNodeSpawnable", "Convert Node to Spawnables") );

	// Ensure we're in a non-possessed state
	RestorePreAnimatedState();
	GetFocusedMovieSceneSequence()->GetMovieScene()->Modify();
	FMovieScenePossessable* Possessable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindPossessable(NodeToBeConverted->GetObjectBinding());
	if (Possessable)
	{
		ConvertToSpawnableInternal(Possessable->GetGuid());
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
	}
}

void FSequencer::ConvertSelectedNodesToSpawnables()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return;
	}

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedNodesSpawnable", "Convert Selected Nodes to Spawnables") );

	// Ensure we're in a non-possessed state
	RestorePreAnimatedState();
	MovieScene->Modify();

	TArray<TSharedRef<FSequencerObjectBindingNode>> ObjectBindingNodes;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			// If we have a possessable for this node, and it has no parent, we can convert it to a spawnable
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingNode->GetObjectBinding());
			if (Possessable && !Possessable->GetParent().IsValid())
			{
				ObjectBindingNodes.Add(ObjectBindingNode);
			}
		}
	}

	FScopedSlowTask SlowTask(ObjectBindingNodes.Num(), LOCTEXT("ConvertSpawnableProgress", "Converting Selected Possessable Nodes to Spawnables"));
	SlowTask.MakeDialog(true);

	TArray<AActor*> SpawnedActors;
	for (const TSharedRef<FSequencerObjectBindingNode>& ObjectBindingNode : ObjectBindingNodes)
	{
		SlowTask.EnterProgressFrame();
	
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingNode->GetObjectBinding());
		if (Possessable)
		{
			FMovieSceneSpawnable* Spawnable = ConvertToSpawnableInternal(Possessable->GetGuid());

			if (Spawnable)
			{
				ForceEvaluate();

				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Spawnable->GetGuid(), ActiveTemplateIDs.Top()))
				{
					if (AActor* SpawnedActor = Cast<AActor>(WeakObject.Get()))
					{
						SpawnedActors.Add(SpawnedActor);
					}
				}
			}
		}

		if (GWarn->ReceivedUserCancel())
		{
			break;
		}
	}

	if (SpawnedActors.Num())
	{
		const bool bNotifySelectionChanged = true;
		const bool bDeselectBSP = true;
		const bool bWarnAboutTooManyActors = false;
		const bool bSelectEvenIfHidden = false;

		GEditor->GetSelectedActors()->Modify();
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
		GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
		for (auto SpawnedActor : SpawnedActors)
		{
			GEditor->SelectActor(SpawnedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
		}
		GEditor->GetSelectedActors()->EndBatchSelectOperation();
		GEditor->NoteSelectionChange();
	}

	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
}

FMovieSceneSpawnable* FSequencer::ConvertToSpawnableInternal(FGuid PossessableGuid)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return nullptr;
	}

	//@todo: this code doesn't work where multiple objects are bound
	TArrayView<TWeakObjectPtr<>> FoundObjects = FindBoundObjects(PossessableGuid, ActiveTemplateIDs.Top());
	if (FoundObjects.Num() != 1)
	{
		return nullptr;
	}

	UObject* FoundObject = FoundObjects[0].Get();
	if (!FoundObject)
	{
		return nullptr;
	}

	Sequence->Modify();
	MovieScene->Modify();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(AddSpawnable(*FoundObject));
	if (Spawnable)
	{
		FGuid SpawnableGuid = Spawnable->GetGuid();

		// Remap all the spawnable's tracks and child bindings onto the new possessable
		MovieScene->MoveBindingContents(PossessableGuid, SpawnableGuid);

		FMovieSceneBinding* PossessableBinding = (FMovieSceneBinding*)MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding) { return Binding.GetObjectGuid() == PossessableGuid; });
		check(PossessableBinding);

		for (UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
		{
			if (ReplaceFolderBindingGUID(Folder, PossessableGuid, SpawnableGuid))
			{
				break;
			}
		}

		int32 SortingOrder = PossessableBinding->GetSortingOrder();

		if (MovieScene->RemovePossessable(PossessableGuid))
		{
			Sequence->UnbindPossessableObjects(PossessableGuid);

			FMovieSceneBinding* SpawnableBinding = (FMovieSceneBinding*)MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding) { return Binding.GetObjectGuid() == SpawnableGuid; });
			check(SpawnableBinding);
			
			SpawnableBinding->SetSortingOrder(SortingOrder);

		}

		TOptional<FTransformData> TransformData;
		SpawnRegister->HandleConvertPossessableToSpawnable(FoundObject, *this, TransformData);
		SpawnRegister->SetupDefaultsForSpawnable(nullptr, Spawnable->GetGuid(), TransformData, AsShared(), Settings);

		ForceEvaluate();
	}

	return Spawnable;
}

void FSequencer::ConvertToPossessable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted)
{
	if (GetFocusedMovieSceneSequence()->GetMovieScene()->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedNodePossessable", "Convert Node to Possessables") );

	// Ensure we're in a non-possessed state
	RestorePreAnimatedState();
	GetFocusedMovieSceneSequence()->GetMovieScene()->Modify();
	FMovieSceneSpawnable* Spawnable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(NodeToBeConverted->GetObjectBinding());
	if (Spawnable)
	{
		ConvertToPossessableInternal(Spawnable->GetGuid());
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

void FSequencer::ConvertSelectedNodesToPossessables()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return;
	}

	TArray<TSharedRef<FSequencerObjectBindingNode>> ObjectBindingNodes;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingNode->GetObjectBinding());
			if (Spawnable && SpawnRegister->CanConvertSpawnableToPossessable(*Spawnable))
			{
				ObjectBindingNodes.Add(ObjectBindingNode);
			}
		}
	}

	if (ObjectBindingNodes.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("ConvertSelectedNodesPossessable", "Convert Selected Nodes to Possessables"));
		MovieScene->Modify();

		FScopedSlowTask SlowTask(ObjectBindingNodes.Num(), LOCTEXT("ConvertPossessablesProgress", "Converting Selected Spawnable Nodes to Possessables"));
		SlowTask.MakeDialog(true);

		TArray<AActor*> PossessedActors;
		for (const TSharedRef<FSequencerObjectBindingNode>& ObjectBindingNode : ObjectBindingNodes)
		{
			SlowTask.EnterProgressFrame();

			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingNode->GetObjectBinding());
			if (Spawnable)
			{
				FMovieScenePossessable* Possessable = ConvertToPossessableInternal(Spawnable->GetGuid());

				ForceEvaluate();

				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Possessable->GetGuid(), ActiveTemplateIDs.Top()))
				{
					if (AActor* PossessedActor = Cast<AActor>(WeakObject.Get()))
					{
						PossessedActors.Add(PossessedActor);
					}
				}
			}

			if (GWarn->ReceivedUserCancel())
			{
				break;
			}
		}

		if (PossessedActors.Num())
		{
			const bool bNotifySelectionChanged = true;
			const bool bDeselectBSP = true;
			const bool bWarnAboutTooManyActors = false;
			const bool bSelectEvenIfHidden = false;

			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();
			GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
			for (auto PossessedActor : PossessedActors)
			{
				GEditor->SelectActor(PossessedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
			}
			GEditor->GetSelectedActors()->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();

			NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		}
	}
}

FMovieScenePossessable* FSequencer::ConvertToPossessableInternal(FGuid SpawnableGuid)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return nullptr;
	}

	// Find the object in the environment
	UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(SpawnableGuid);
	if (!Spawnable || !Spawnable->GetObjectTemplate())
	{
		return nullptr;
	}

	AActor* SpawnableActorTemplate = Cast<AActor>(Spawnable->GetObjectTemplate());
	if (!SpawnableActorTemplate)
	{
		return nullptr;
	}

	Sequence->Modify();
	MovieScene->Modify();

	// Delete the spawn track
	UMovieSceneSpawnTrack* SpawnTrack = Cast<UMovieSceneSpawnTrack>(MovieScene->FindTrack(UMovieSceneSpawnTrack::StaticClass(), SpawnableGuid, NAME_None));
	if (SpawnTrack)
	{
		MovieScene->RemoveTrack(*SpawnTrack);
	}

	FTransform SpawnTransform = SpawnableActorTemplate->GetActorTransform();
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bDeferConstruction = true;
	SpawnInfo.Template = SpawnableActorTemplate;

	UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
	AActor* PossessedActor = PlaybackContext->SpawnActor(Spawnable->GetObjectTemplate()->GetClass(), &SpawnTransform, SpawnInfo);

	if (!PossessedActor)
	{
		return nullptr;
	}

	PossessedActor->SetActorLabel(Spawnable->GetName());

	const bool bIsDefaultTransform = true;
	PossessedActor->FinishSpawning(SpawnTransform, bIsDefaultTransform);

	const FGuid NewPossessableGuid = CreateBinding(*PossessedActor, PossessedActor->GetActorLabel());
	const FGuid OldSpawnableGuid = Spawnable->GetGuid();

	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(NewPossessableGuid);
	if (Possessable)
	{
		// Remap all the spawnable's tracks and child bindings onto the new possessable
		MovieScene->MoveBindingContents(OldSpawnableGuid, NewPossessableGuid);

		FMovieSceneBinding* SpawnableBinding = (FMovieSceneBinding*)MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding) { return Binding.GetObjectGuid() == OldSpawnableGuid; });
		check(SpawnableBinding);

		for (UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
		{
			if (ReplaceFolderBindingGUID(Folder, Spawnable->GetGuid(), Possessable->GetGuid()))
			{
				break;
			}
		}

		int32 SortingOrder = SpawnableBinding->GetSortingOrder();

		// Remove the spawnable and all it's sub tracks
		if (MovieScene->RemoveSpawnable(OldSpawnableGuid))
		{
			SpawnRegister->DestroySpawnedObject(OldSpawnableGuid, ActiveTemplateIDs.Top(), *this);

			FMovieSceneBinding* PossessableBinding = (FMovieSceneBinding*)MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding) { return Binding.GetObjectGuid() == NewPossessableGuid; });
			check(PossessableBinding);
			
			PossessableBinding->SetSortingOrder(SortingOrder);
		}

		static const FName SequencerActorTag(TEXT("SequencerActor"));
		PossessedActor->Tags.Remove(SequencerActorTag);

		GEditor->SelectActor(PossessedActor, false, true);

		ForceEvaluate();
	}

	return Possessable;
}



void FSequencer::OnLoadRecordedData()
{
	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	if (!FocusedMovieSceneSequence)
	{
		return;
	}
	UMovieScene* FocusedMovieScene = FocusedMovieSceneSequence->GetMovieScene();
	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString FileTypeDescription = TEXT("");
		FString DialogTitle = TEXT("Open Recorded Sequencer Data");
		FString InOpenDirectory = FPaths::ProjectSavedDir();
		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			DialogTitle,
			InOpenDirectory,
			TEXT(""),
			FileTypeDescription,
			EFileDialogFlags::None,
			OpenFilenames
		);
	}

	if (!bOpen || !OpenFilenames.Num())
	{
		return;
	}
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ISerializedRecorder::ModularFeatureName))
	{
		ISerializedRecorder* Recorder = &IModularFeatures::Get().GetModularFeature<ISerializedRecorder>(ISerializedRecorder::ModularFeatureName);
		if (Recorder)
		{
			FScopedTransaction AddFolderTransaction(NSLOCTEXT("Sequencer", "LoadRecordedData_Transaction", "Load Recorded Data"));
			auto OnReadComplete = [this]()
			{
				NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

			}; //callback
			UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
			for (const FString& FileName : OpenFilenames)
			{
				Recorder->LoadRecordedSequencerFile(FocusedMovieSceneSequence, PlaybackContext, FileName, OnReadComplete);
			}
		}
	}

}

bool FSequencer::ReplaceFolderBindingGUID(UMovieSceneFolder* Folder, FGuid Original, FGuid Converted)
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return true;
	}

	for (FGuid ChildGuid : Folder->GetChildObjectBindings())
	{
		if (ChildGuid == Original)
		{
			Folder->AddChildObjectBinding(Converted);
			Folder->RemoveChildObjectBinding(Original);
			return true;
		}
	}

	for (UMovieSceneFolder* ChildFolder : Folder->GetChildFolders())
	{
		if (ReplaceFolderBindingGUID(ChildFolder, Original, Converted))
		{
			return true;
		}
	}

	return false;
}

void FSequencer::OnAddFolder()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	FScopedTransaction AddFolderTransaction( NSLOCTEXT("Sequencer", "AddFolder_Transaction", "Add Folder") );

	// Check if a folder, or child of a folder is currently selected.
	TArray<UMovieSceneFolder*> SelectedParentFolders;
	FString NewNodePath;
	CalculateSelectedFolderAndPath(SelectedParentFolders, NewNodePath);

	TArray<FName> ExistingFolderNames;
	
	// If there is a folder selected the existing folder names are the sibling folders.
	if ( SelectedParentFolders.Num() == 1 )
	{
		for ( UMovieSceneFolder* SiblingFolder : SelectedParentFolders[0]->GetChildFolders() )
		{
			ExistingFolderNames.Add( SiblingFolder->GetFolderName() );
		}
	}
	// Otherwise use the root folders.
	else
	{
		for ( UMovieSceneFolder* MovieSceneFolder : FocusedMovieScene->GetRootFolders() )
		{
			ExistingFolderNames.Add( MovieSceneFolder->GetFolderName() );
		}
	}

	FName UniqueName = FSequencerUtilities::GetUniqueName(FName("New Folder"), ExistingFolderNames);
	UMovieSceneFolder* NewFolder = NewObject<UMovieSceneFolder>( FocusedMovieScene, NAME_None, RF_Transactional );
	NewFolder->SetFolderName( UniqueName );

	// The folder's name is used as it's key in the path system.
	NewNodePath += UniqueName.ToString();

	if ( SelectedParentFolders.Num() == 1 )
	{
		SelectedParentFolders[0]->Modify();
		SelectedParentFolders[0]->AddChildFolder( NewFolder );
	}
	else
	{
		FocusedMovieScene->Modify();
		FocusedMovieScene->GetRootFolders().Add( NewFolder );
	}

	Selection.Empty();

	// We can't add the newly created folder to the selection set as the nodes for it don't actually exist yet.
	// However, we can calculate the resulting path that the node will end up at and add that to the selection
	// set, which will cause the newly created node to be selected when the selection is restored post-refresh.
	SequencerWidget->AddAdditionalPathToSelectionSet(NewNodePath);

	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

void FSequencer::OnAddTrack(const TWeakObjectPtr<UMovieSceneTrack>& InTrack)
{
	FString NewNodePath;

	// Cinematic Shot Tracks and Camera Cut Tracks are always in the root and ignore sorting/folders, so we don't give them a chance to be placed into a folder.
	bool bIsValidTrack = !(InTrack->IsA(UMovieSceneCinematicShotTrack::StaticClass()) || InTrack->IsA(UMovieSceneCameraCutTrack::StaticClass()));
	if(bIsValidTrack)
	{
		TArray<UMovieSceneFolder*> SelectedParentFolders;
		CalculateSelectedFolderAndPath(SelectedParentFolders, NewNodePath);

		if (SelectedParentFolders.Num() == 1)
		{
			SelectedParentFolders[0]->Modify();
			SelectedParentFolders[0]->AddChildMasterTrack(InTrack.Get());
		}
	}
	Selection.Empty();

	// We can't add the newly created folder to the selection set as the nodes for it don't actually exist yet.
	// However, we can calculate the resulting path that the node will end up at and add that to the selection
	// set, which will cause the newly created node to be selected when the selection is restored post-refresh.
	NewNodePath += InTrack->GetFName().ToString();
	SequencerWidget->AddAdditionalPathToSelectionSet(NewNodePath);
}


void FSequencer::CalculateSelectedFolderAndPath(TArray<UMovieSceneFolder*>& OutSelectedParentFolders, FString& OutNewNodePath)
{
	// Check if a folder, or child of a folder is currently selected.
	if (Selection.GetSelectedOutlinerNodes().Num() > 0)
	{
		for (TSharedRef<FSequencerDisplayNode> SelectedNode : Selection.GetSelectedOutlinerNodes())
		{
			TSharedPtr<FSequencerDisplayNode> CurrentNode = SelectedNode;
			while (CurrentNode.IsValid() && CurrentNode->GetType() != ESequencerNode::Folder)
			{
				CurrentNode = CurrentNode->GetParent();
			}
			if (CurrentNode.IsValid())
			{
				OutSelectedParentFolders.Add(&StaticCastSharedPtr<FSequencerFolderNode>(CurrentNode)->GetFolder());

				// The first valid folder we find will be used to put the new folder into, so it's the node that we
				// want to know the path from.
				if (OutNewNodePath.Len() == 0)
				{
					// Add an extra delimiter (".") as we know that the new folder will be appended onto the end of this.
					OutNewNodePath = FString::Printf(TEXT("%s."), *CurrentNode->GetPathName());

					// Make sure this folder is expanded too so that adding objects to hidden folders become visible.
					CurrentNode->SetExpansionState(true);
				}
			}
		}
	}
}

void FSequencer::TogglePlay()
{
	OnPlayForward(true);
}

void FSequencer::JumpToStart()
{
	OnJumpToStart();
}

void FSequencer::JumpToEnd()
{
	OnJumpToEnd();
}

void FSequencer::ShuttleForward()
{
	float NewPlaybackSpeed = PlaybackSpeed;
	if (ShuttleMultiplier == 0 || PlaybackSpeed < 0) 
	{
		ShuttleMultiplier = 2.f;
		NewPlaybackSpeed = 1.f;
	}
	else
	{
		NewPlaybackSpeed *= ShuttleMultiplier;
	}

	PlaybackSpeed = NewPlaybackSpeed;
	OnPlayForward(false);
}

void FSequencer::ShuttleBackward()
{
	float NewPlaybackSpeed = PlaybackSpeed;
	if (ShuttleMultiplier == 0 || PlaybackSpeed > 0)
	{
		ShuttleMultiplier = 2.f;
		NewPlaybackSpeed = -1.f;
	}
	else
	{
		NewPlaybackSpeed *= ShuttleMultiplier;
	}

	PlaybackSpeed = NewPlaybackSpeed;
	OnPlayBackward(false);
}

void FSequencer::Pause()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);

	// When stopping a sequence, we always evaluate a non-empty range if possible. This ensures accurate paused motion blur effects.
	if (Settings->GetIsSnapEnabled())
	{
		FQualifiedFrameTime LocalTime          = GetLocalTime();
		FFrameRate          FocusedDisplayRate = GetFocusedDisplayRate();

		// Snap to the focused play rate
		FFrameTime RootPosition  = FFrameRate::Snap(LocalTime.Time, LocalTime.Rate, FocusedDisplayRate) * RootToLocalTransform.Inverse();

		// Convert the root position from tick resolution time base (the output rate), to the play position input rate
		FFrameTime InputPosition = ConvertFrameTime(RootPosition, PlayPosition.GetOutputRate(), PlayPosition.GetInputRate());
		EvaluateInternal(PlayPosition.PlayTo(InputPosition));
	}
	else
	{
		// Update on stop (cleans up things like sounds that are playing)
		FMovieSceneEvaluationRange Range = PlayPosition.GetLastRange().Get(PlayPosition.GetCurrentPositionAsRange());
		EvaluateInternal(Range);
	}

	OnStopDelegate.Broadcast();
}

void FSequencer::StepForward()
{
	OnStepForward();
}


void FSequencer::StepBackward()
{
	OnStepBackward();
}


void FSequencer::StepToNextKey()
{
	SequencerWidget->StepToNextKey();
}


void FSequencer::StepToPreviousKey()
{
	SequencerWidget->StepToPreviousKey();
}


void FSequencer::StepToNextCameraKey()
{
	SequencerWidget->StepToNextCameraKey();
}


void FSequencer::StepToPreviousCameraKey()
{
	SequencerWidget->StepToPreviousCameraKey();
}


void FSequencer::StepToNextShot()
{
	if (ActiveTemplateIDs.Num() < 2)
	{
		return;
	}

	FMovieSceneSequenceID OuterSequenceID = ActiveTemplateIDs[ActiveTemplateIDs.Num()-2];
	UMovieSceneSequence* Sequence = RootTemplateInstance.GetSequence(OuterSequenceID);

	FFrameTime StartTime = FFrameTime(0) * RootToLocalTransform.Inverse();
	FFrameTime CurrentTime = StartTime;
	if (FMovieSceneSubSequenceData* SubSequenceData = RootTemplateInstance.GetHierarchy().FindSubData(OuterSequenceID))
	{
		CurrentTime = StartTime * SubSequenceData->RootToSequenceTransform;
	}

	UMovieSceneSubSection* NextShot = Cast<UMovieSceneSubSection>(FindNextOrPreviousShot(Sequence, CurrentTime.FloorToFrame(), true));
	if (!NextShot)
	{
		return;
	}

	SequencerWidget->PopBreadcrumb();

	PopToSequenceInstance(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2]);
	FocusSequenceInstance(*NextShot);

	SetLocalTime(FFrameTime(0));
}


void FSequencer::StepToPreviousShot()
{
	if (ActiveTemplateIDs.Num() < 2)
	{
		return;
	}

	FMovieSceneSequenceID OuterSequenceID = ActiveTemplateIDs[ActiveTemplateIDs.Num()-2];
	UMovieSceneSequence* Sequence = RootTemplateInstance.GetSequence(OuterSequenceID);

	FFrameTime StartTime = FFrameTime(0) * RootToLocalTransform.Inverse();
	FFrameTime CurrentTime = StartTime;
	if (FMovieSceneSubSequenceData* SubSequenceData = RootTemplateInstance.GetHierarchy().FindSubData(OuterSequenceID))
	{
		CurrentTime = StartTime * SubSequenceData->RootToSequenceTransform;
	}

	UMovieSceneSubSection* PreviousShot = Cast<UMovieSceneSubSection>(FindNextOrPreviousShot(Sequence, CurrentTime.FloorToFrame(), false));
	if (!PreviousShot)
	{
		return;
	}

	SequencerWidget->PopBreadcrumb();

	PopToSequenceInstance(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2]);
	FocusSequenceInstance(*PreviousShot);

	SetLocalTime(0);
}


void FSequencer::ExpandAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->ExpandNodes(ETreeRecursion::Recursive, bExpandAll);
}


void FSequencer::CollapseAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->CollapseNodes(ETreeRecursion::Recursive, bExpandAll);
}

void FSequencer::SortAllNodesAndDescendants()
{
	FScopedTransaction SortAllNodesTransaction(NSLOCTEXT("Sequencer", "SortAllNodes_Transaction", "Sort Tracks"));
	SequencerWidget->GetTreeView()->GetNodeTree()->SortAllNodesAndDescendants();
}

void FSequencer::ToggleExpandCollapseNodes()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::NonRecursive);
}


void FSequencer::ToggleExpandCollapseNodesAndDescendants()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::Recursive);
}


void FSequencer::SetKey()
{
	FScopedTransaction SetKeyTransaction( NSLOCTEXT("Sequencer", "SetKey_Transaction", "Set Key") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		if (OutlinerNode->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(OutlinerNode);

			TSharedRef<FSequencerDisplayNode> ObjectBindingNode = OutlinerNode;
			if (SequencerHelpers::FindObjectBindingNode(TrackNode, ObjectBindingNode))
			{
				FGuid ObjectGuid = StaticCastSharedRef<FSequencerObjectBindingNode>(ObjectBindingNode)->GetObjectBinding();
				TrackNode->AddKey(ObjectGuid);
			}
		}
	}

	TSet<TSharedRef<FSequencerDisplayNode>> NodesToKey = Selection.GetSelectedOutlinerNodes();
	{
		TSet<TSharedRef<FSequencerDisplayNode>> ChildNodes;
		for (TSharedRef<FSequencerDisplayNode> Node : NodesToKey.Array())
		{
			ChildNodes.Reset();
			SequencerHelpers::GetDescendantNodes(Node, ChildNodes);

			for (TSharedRef<FSequencerDisplayNode> ChildNode : ChildNodes)
			{
				NodesToKey.Remove(ChildNode);
			}
		}
	}

	const FFrameNumber AddKeyTime = GetLocalTime().Time.FloorToFrame();

	TSet<TSharedPtr<IKeyArea>> KeyAreas;
	TSet<UMovieSceneSection*>  ModifiedSections;

	for (TSharedRef<FSequencerDisplayNode> Node : NodesToKey)
	{
		KeyAreas.Reset();
		SequencerHelpers::GetAllKeyAreas(Node, KeyAreas);

		FGuid ObjectBinding;
		if (Node->GetType() == ESequencerNode::Object)
		{
			TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);
			if (ObjectBindingNode.IsValid())
			{
				ObjectBinding = ObjectBindingNode->GetObjectBinding();
			}
		}
		else
		{
			TSharedPtr<FSequencerObjectBindingNode> ParentObjectBinding = Node->FindParentObjectBindingNode();
			ObjectBinding = ParentObjectBinding.IsValid() ? ParentObjectBinding->GetObjectBinding() : FGuid();
		}

		for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
		{
			UMovieSceneSection* Section = KeyArea->GetOwningSection();
			if (Section)
			{
				if (!ModifiedSections.Contains(Section))
				{
					Section->Modify();
					ModifiedSections.Add(Section);
				}


				KeyArea->AddOrUpdateKey(AddKeyTime, ObjectBinding, *this);
			}
		}
	}

	UpdatePlaybackRange();
}


bool FSequencer::CanSetKeyTime() const
{
	return Selection.GetSelectedKeys().Num() > 0;
}


void FSequencer::SetKeyTime()
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	FFrameNumber KeyTime = 0;
	for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
	{
		if (Key.IsValid())
		{
			KeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());
			break;
		}
	}

	// Create a popup showing the existing time value and let the user set a new one.
 	GenericTextEntryModeless(NSLOCTEXT("Sequencer.Popups", "SetKeyTimePopup", "New Time"), FText::FromString(GetNumericTypeInterface()->ToString(KeyTime.Value)),
 		FOnTextCommitted::CreateSP(this, &FSequencer::OnSetKeyTimeTextCommitted)
 	);
}


void FSequencer::OnSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	bool bAnythingChanged = false;

	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		TOptional<double> NewFrameTime = GetNumericTypeInterface()->FromString(InText.ToString(), 0);
		if (!NewFrameTime.IsSet())
			return;

		FFrameNumber NewFrame = FFrameNumber((int32)NewFrameTime.GetValue());

		FScopedTransaction SetKeyTimeTransaction(NSLOCTEXT("Sequencer", "SetKeyTime_Transaction", "Set Key Time"));
		TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	
		for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
		{
			if (Key.IsValid())
			{
	 			if (Key.Section->TryModify())
	 			{
	 				Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewFrame);
	 				bAnythingChanged = true;

					Key.Section->ExpandToFrame(NewFrame);
	 			}
			}
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

bool FSequencer::CanRekey() const
{
	return Selection.GetSelectedKeys().Num() > 0;
}


void FSequencer::Rekey()
{
	bool bAnythingChanged = false;

	FQualifiedFrameTime CurrentTime = GetLocalTime();

	FScopedTransaction RekeyTransaction(NSLOCTEXT("Sequencer", "Rekey_Transaction", "Rekey"));
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	
	for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
	{
		if (Key.IsValid())
		{
	 		if (Key.Section->TryModify())
	 		{
	 			Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), CurrentTime.Time.FrameNumber);
	 			bAnythingChanged = true;

				Key.Section->ExpandToFrame(CurrentTime.Time.FrameNumber);
	 		}
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

TSet<FFrameNumber> FSequencer::GetVerticalFrames() const
{
	TSet<FFrameNumber> VerticalFrames;

	auto AddVerticalFrames = [](auto &InVerticalFrames, auto InTrack) 
	{
		for (UMovieSceneSection* Section : InTrack->GetAllSections())
		{
			if (Section->GetRange().HasLowerBound())
			{
				InVerticalFrames.Add(Section->GetRange().GetLowerBoundValue());
			}

			if (Section->GetRange().HasUpperBound())
			{
				InVerticalFrames.Add(Section->GetRange().GetUpperBoundValue());
			}
		}
	};

	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		for (UMovieSceneTrack* MasterTrack : FocusedMovieScene->GetMasterTracks())
		{
			if (FocusedMovieScene != nullptr)
			{
				if (MasterTrack->DisplayOptions.bShowVerticalFrames)
				{
					AddVerticalFrames(VerticalFrames, MasterTrack);
				}
			}
		}

		if (UMovieSceneTrack* CameraCutTrack = FocusedMovieScene->GetCameraCutTrack())
		{
			if (CameraCutTrack->DisplayOptions.bShowVerticalFrames)
			{
				AddVerticalFrames(VerticalFrames, CameraCutTrack);
			}
		}
	}

	return VerticalFrames;
}

TArray<FMovieSceneMarkedFrame> FSequencer::GetMarkedFrames() const
{
	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			return FocusedMovieScene->GetMarkedFrames();
		}
	}

	return TArray<FMovieSceneMarkedFrame>();
}

void FSequencer::ToggleMarkAtPlayPosition()
{
	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			FFrameNumber TickFrameNumber = GetLocalTime().Time.FloorToFrame();
			int32 MarkedFrameIndex = FocusedMovieScene->FindMarkedFrameByFrameNumber(TickFrameNumber);
			if (MarkedFrameIndex != INDEX_NONE)
			{
				FocusedMovieScene->RemoveMarkedFrame(MarkedFrameIndex);
			}
			else
			{
				FocusedMovieScene->AddMarkedFrame(FMovieSceneMarkedFrame(TickFrameNumber));
			}
		}
	}
}

void FSequencer::SetMarkedFrame(FFrameNumber FrameNumber, bool bSetMark)
{
	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			if (bSetMark)
			{
				FocusedMovieScene->AddMarkedFrame(FMovieSceneMarkedFrame(FrameNumber));
			}
			else
			{
				int32 MarkedFrameIndex = FocusedMovieScene->FindMarkedFrameByFrameNumber(FrameNumber);
				if (MarkedFrameIndex != INDEX_NONE)
				{
					FocusedMovieScene->RemoveMarkedFrame(MarkedFrameIndex);
				}
			}
		}
	}
}

void FSequencer::ClearAllMarkedFrames()
{
	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			FocusedMovieScene->ClearMarkedFrames();
		}
	}
}

void FSequencer::StepToNextMark()
{
	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			const bool bForwards = true;
			int32 MarkedIndex = FocusedMovieScene->FindNextMarkedFrame(GetLocalTime().Time.FloorToFrame(), bForwards);
			if (MarkedIndex != INDEX_NONE)
			{
				AutoScrubToTime(FocusedMovieScene->GetMarkedFrames()[MarkedIndex].FrameNumber.Value);
			}
		}
	}
}

void FSequencer::StepToPreviousMark()
{
	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			const bool bForwards = false;
			int32 MarkedIndex = FocusedMovieScene->FindNextMarkedFrame(GetLocalTime().Time.FloorToFrame(), bForwards);
			if (MarkedIndex != INDEX_NONE)
			{
				AutoScrubToTime(FocusedMovieScene->GetMarkedFrames()[MarkedIndex].FrameNumber.Value);
			}
		}
	}
}


TArray<TSharedPtr<FMovieSceneClipboard>> GClipboardStack;

void FSequencer::CopySelection()
{
	if (Selection.GetSelectedKeys().Num() != 0)
	{
		CopySelectedKeys();
	}
	else if (Selection.GetSelectedSections().Num() != 0)
	{
		CopySelectedSections();
	}
	else
	{
		TArray<TSharedPtr<FSequencerTrackNode>> TracksToCopy;
		TArray<TSharedPtr<FSequencerObjectBindingNode>> ObjectsToCopy;
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetNodesWithSelectedKeysOrSections();
		if (SelectedNodes.Num() == 0)
		{
			SelectedNodes = Selection.GetSelectedOutlinerNodes();
		}
		for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
		{
			if (Node->GetType() == ESequencerNode::Track)
			{
				TSharedPtr<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
				if (TrackNode.IsValid())
				{
					TracksToCopy.Add(TrackNode);
				}
			}
			else if (Node->GetType() == ESequencerNode::Object)
			{
				TSharedPtr<FSequencerObjectBindingNode> ObjectNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);
				if (ObjectNode.IsValid())
				{
					ObjectsToCopy.Add(ObjectNode);
				}
			}
		}

		// Make a empty clipboard if the stack is empty
		if (GClipboardStack.Num() == 0)
		{
			TSharedRef<FMovieSceneClipboard> NullClipboard = MakeShareable(new FMovieSceneClipboard());
			GClipboardStack.Push(NullClipboard);
		}

		FString ObjectsExportedText;
		FString TracksExportedText;

		if (ObjectsToCopy.Num())
		{
			CopySelectedObjects(ObjectsToCopy, ObjectsExportedText);
		}

		if (TracksToCopy.Num())
		{
			CopySelectedTracks(TracksToCopy, TracksExportedText);
		}

		FString ExportedText;
		ExportedText += ObjectsExportedText;
		ExportedText += TracksExportedText;

		FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
	}
}

void FSequencer::CutSelection()
{
	if (Selection.GetSelectedKeys().Num() != 0)
	{
		CutSelectedKeys();
	}
	else if (Selection.GetSelectedSections().Num() != 0)
	{
		CutSelectedSections();
	}
	else
	{
		FScopedTransaction CutSelectionTransaction(LOCTEXT("CutSelection_Transaction", "Cut Selection"));
		CopySelection();
		DeleteSelectedItems();
	}
}

void FSequencer::DuplicateSelection()
{
	CopySelection();
	DoPaste();
}

void FSequencer::CopySelectedKeys()
{
	TOptional<FFrameNumber> CopyRelativeTo;
	
	// Copy relative to the current key hotspot, if applicable
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		CopyRelativeTo = StaticCastSharedPtr<FKeyHotspot>(Hotspot)->GetTime();
	}

	FMovieSceneClipboardBuilder Builder;

	// Map selected keys to their key areas
	TMap<TSharedPtr<IKeyArea>, TArray<FKeyHandle>> KeyAreaMap;
	for (const FSequencerSelectedKey& Key : Selection.GetSelectedKeys())
	{
		if (Key.KeyHandle.IsSet())
		{
			KeyAreaMap.FindOrAdd(Key.KeyArea).Add(Key.KeyHandle.GetValue());
		}
	}

	// Serialize each key area to the clipboard
	for (auto& Pair : KeyAreaMap)
	{
		Pair.Key->CopyKeys(Builder, Pair.Value);
	}

	TSharedRef<FMovieSceneClipboard> Clipboard = MakeShareable( new FMovieSceneClipboard(Builder.Commit(CopyRelativeTo)) );
	
	Clipboard->GetEnvironment().TickResolution = GetFocusedTickResolution();

	if (Clipboard->GetKeyTrackGroups().Num())
	{
		GClipboardStack.Push(Clipboard);

		if (GClipboardStack.Num() > 10)
		{
			GClipboardStack.RemoveAt(0, 1);
		}
	}
}

void FSequencer::CutSelectedKeys()
{
	FScopedTransaction CutSelectedKeysTransaction(LOCTEXT("CutSelectedKeys_Transaction", "Cut Selected keys"));
	CopySelectedKeys();
	DeleteSelectedKeys();
}


void FSequencer::CopySelectedSections()
{
	TArray<UObject*> SelectedSections;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSectionPtr : Selection.GetSelectedSections())
	{
		if (SelectedSectionPtr.IsValid())
		{
			SelectedSections.Add(SelectedSectionPtr.Get());
		}
	}

	FString ExportedText;
	FSequencer::ExportObjectsToText(SelectedSections, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

void FSequencer::CutSelectedSections()
{
	FScopedTransaction CutSelectedSectionsTransaction(LOCTEXT("CutSelectedSections_Transaction", "Cut Selected sections"));
	CopySelectedSections();
	DeleteSections(Selection.GetSelectedSections());
}


const TArray<TSharedPtr<FMovieSceneClipboard>>& FSequencer::GetClipboardStack() const
{
	return GClipboardStack;
}


void FSequencer::OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard)
{
	Clipboard->GetEnvironment().DateTime = FDateTime::UtcNow();

	// Last entry in the stack should be the most up-to-date
	GClipboardStack.Sort([](const TSharedPtr<FMovieSceneClipboard>& A, const TSharedPtr<FMovieSceneClipboard>& B){
		return A->GetEnvironment().DateTime < B->GetEnvironment().DateTime;
	});
}


void FSequencer::DiscardChanges()
{
	if (ActiveTemplateIDs.Num() == 0)
	{
		return;
	}

	TSharedPtr<IToolkitHost> MyToolkitHost = GetToolkitHost();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	UMovieSceneSequence* EditedSequence = GetFocusedMovieSceneSequence();

	if (EditedSequence == nullptr)
	{
		return;
	}

	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("RevertConfirm", "Are you sure you want to discard your current changes?")) != EAppReturnType::Yes)
	{
		return;
	}

	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	UClass* SequenceClass = EditedSequence->GetClass();
	FString SequencePath = EditedSequence->GetPathName();
	UPackage* SequencePackage = EditedSequence->GetOutermost();

	// close asset editor
	AssetEditorManager.CloseAllEditorsForAsset(EditedSequence);

	// collect objects to be unloaded
	TMap<FString, UObject*> MovedObjects;

	ForEachObjectWithOuter(SequencePackage, [&](UObject* Object) {
		MovedObjects.Add(Object->GetPathName(), Object);
	}, true);

	// move objects into transient package
	UPackage* const TransientPackage = GetTransientPackage();

	for (auto MovedObject : MovedObjects)
	{
		UObject* Object = MovedObject.Value;

		const FString OldName = Object->GetName();
		const FString NewName = FString::Printf(TEXT("UNLOADING_%s"), *OldName);
		const FName UniqueName = MakeUniqueObjectName(TransientPackage, Object->GetClass(), FName(*NewName));
		UObject* NewOuter = (Object->GetOuter() == SequencePackage) ? TransientPackage : Object->GetOuter();
	
		Object->Rename(*UniqueName.ToString(), NewOuter, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
		Object->SetFlags(RF_Transient);
		Object->ClearFlags(RF_Standalone | RF_Transactional);
	}

	for (auto MovedObject : MovedObjects)
	{
		GLog->Logf(TEXT("Moved %s ---------> %s"), *MovedObject.Key, *MovedObject.Value->GetPathName());
	}

	// unload package
	SequencePackage->SetDirtyFlag(false);

	TArray<UPackage*> PackagesToUnload;
	PackagesToUnload.Add(SequencePackage);

	FText PackageUnloadError;
	UPackageTools::UnloadPackages(PackagesToUnload, PackageUnloadError);

	if (!PackageUnloadError.IsEmpty())
	{
		ResetLoaders(SequencePackage);
		SequencePackage->ClearFlags(RF_WasLoaded);
		SequencePackage->bHasBeenFullyLoaded = false;
		SequencePackage->GetMetaData()->RemoveMetaDataOutsidePackage();
	}

	// reload package
	TMap<UObject*, UObject*> MovedToReloadedObjectMap;

	for (const auto MovedObject : MovedObjects)
	{
		UObject* ReloadedObject = StaticLoadObject(MovedObject.Value->GetClass(), nullptr, *MovedObject.Key, nullptr);//, LOAD_NoWarn);
		MovedToReloadedObjectMap.Add(MovedObject.Value, ReloadedObject);
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		// @todo sequencer: only process objects that actually reference the package?
		FArchiveReplaceObjectRef<UObject> Ar(*It, MovedToReloadedObjectMap, false, false, false, false);
	}

	auto ReloadedSequence = Cast<UMovieSceneSequence>(StaticLoadObject(SequenceClass, nullptr, *SequencePath, nullptr));//, LOAD_NoWarn));

	// release transient objects
	for (auto MovedObject : MovedObjects)
	{
		MovedObject.Value->RemoveFromRoot();
		MovedObject.Value->MarkPendingKill();
	}

//	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// clear undo buffer
	if (true) // @todo sequencer: check whether objects are actually referenced in undo buffer
	{
		GEditor->Trans->Reset(LOCTEXT("UnloadedSequence", "Unloaded Sequence"));
	}

	// reopen asset editor
	TArray<UObject*> AssetsToReopen;
	AssetsToReopen.Add(ReloadedSequence);

	AssetEditorManager.OpenEditorForAssets(AssetsToReopen, EToolkitMode::Standalone, MyToolkitHost.ToSharedRef());
}


void FSequencer::CreateCamera()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "CreateCameraHere", "Create Camera Here"));

	const bool bCreateAsSpawnable = Settings->GetCreateSpawnableCameras();

	FActorSpawnParameters SpawnParams;
	if (bCreateAsSpawnable)
	{
		// Don't bother transacting this object if we're creating a spawnable since it's temporary
		SpawnParams.ObjectFlags &= ~RF_Transactional;
	}

	// Set new camera to match viewport
	ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(SpawnParams);
	if (!NewCamera)
	{
		return;
	}

	FGuid CameraGuid;

	FMovieSceneSpawnable* Spawnable = nullptr;
	ESpawnOwnership SavedOwnership = Spawnable ? Spawnable->GetSpawnOwnership() : ESpawnOwnership::InnerSequence;

	if (bCreateAsSpawnable)
	{
		CameraGuid = MakeNewSpawnable(*NewCamera);
		Spawnable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(CameraGuid);

		if (ensure(Spawnable))
		{
			// Override spawn ownership during this process to ensure it never gets destroyed
			SavedOwnership = Spawnable->GetSpawnOwnership();
			Spawnable->SetSpawnOwnership(ESpawnOwnership::External);
		}

		// Destroy the old actor
		World->EditorDestroyActor(NewCamera, false);

		for (TWeakObjectPtr<UObject>& Object : FindBoundObjects(CameraGuid, ActiveTemplateIDs.Top()))
		{
			NewCamera = Cast<ACineCameraActor>(Object.Get());
			if (NewCamera)
			{
				break;
			}
		}
		ensure(NewCamera);
	}
	else
	{
		CameraGuid = CreateBinding(*NewCamera, NewCamera->GetActorLabel());
	}
	
	if (!CameraGuid.IsValid())
	{
		return;
	}
	
	NewCamera->SetActorLocation( GCurrentLevelEditingViewportClient->GetViewLocation(), false );
	NewCamera->SetActorRotation( GCurrentLevelEditingViewportClient->GetViewRotation() );
	//pNewCamera->CameraComponent->FieldOfView = ViewportClient->ViewFOV; //@todo set the focal length from this field of view

	OnActorAddedToSequencerEvent.Broadcast(NewCamera, CameraGuid);

	NewCameraAdded(CameraGuid, NewCamera);

	if (bCreateAsSpawnable && ensure(Spawnable))
	{
		Spawnable->SetSpawnOwnership(SavedOwnership);
	}

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

void FSequencer::NewCameraAdded(FGuid CameraGuid, ACameraActor* NewCamera)
{
	SetPerspectiveViewportCameraCutEnabled(false);

	// Lock the viewport to this camera
	if (NewCamera && NewCamera->GetLevel())
	{
		GCurrentLevelEditingViewportClient->SetMatineeActorLock(nullptr);
		GCurrentLevelEditingViewportClient->SetActorLock(NewCamera);
		GCurrentLevelEditingViewportClient->bLockedCameraView = true;
		GCurrentLevelEditingViewportClient->UpdateViewForLockedActor();
		GCurrentLevelEditingViewportClient->Invalidate();
	}

	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	// If there's a cinematic shot track, no need to set this camera to a shot
	UMovieSceneTrack* CinematicShotTrack = OwnerMovieScene->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass());
	if (CinematicShotTrack)
	{
		return;
	}

	UMovieSceneTrack* CameraCutTrack = OwnerMovieScene->GetCameraCutTrack();

	// If there's a camera cut track with at least one section, no need to change the section
	if (CameraCutTrack && CameraCutTrack->GetAllSections().Num() > 0)
	{
		return;
	}

	if (!CameraCutTrack)
	{
		CameraCutTrack = OwnerMovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
	}

	if (CameraCutTrack)
	{
		UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime(CameraCutTrack->GetAllSections(), GetLocalTime().Time.FloorToFrame());
		UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);

		if (CameraCutSection)
		{
			CameraCutSection->Modify();
			CameraCutSection->SetCameraGuid(CameraGuid);
		}
		else
		{
			CameraCutTrack->Modify();

			UMovieSceneCameraCutSection* NewSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
			NewSection->SetRange(GetPlaybackRange());
			NewSection->SetCameraGuid(CameraGuid);
			CameraCutTrack->AddSection(*NewSection);
		}
	}
}


void FSequencer::FixActorReferences()
{
	UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());

	if (!PlaybackContext)
	{
		return;
	}

	FScopedTransaction FixActorReferencesTransaction( NSLOCTEXT( "Sequencer", "FixActorReferences", "Fix Actor References" ) );

	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	TMap<FString, AActor*> ActorNameToActorMap;

	for ( TActorIterator<AActor> ActorItr( PlaybackContext ); ActorItr; ++ActorItr )
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AActor *Actor = *ActorItr;
		ActorNameToActorMap.Add( Actor->GetActorLabel(), Actor);
	}

	// Cache the possessables to fix up first since the bindings will change as the fix ups happen.
	TArray<FMovieScenePossessable> ActorsPossessablesToFix;
	for ( int32 i = 0; i < FocusedMovieScene->GetPossessableCount(); i++ )
	{
		FMovieScenePossessable& Possessable = FocusedMovieScene->GetPossessable( i );
		// Possessables with parents are components so ignore them.
		if ( Possessable.GetParent().IsValid() == false )
		{
			if ( FindBoundObjects(Possessable.GetGuid(), ActiveTemplateIDs.Top()).Num() == 0 )
			{
				ActorsPossessablesToFix.Add( Possessable );
			}
		}
	}

	// For the possessables to fix, look up the actors by name and reassign them if found.
	TMap<FGuid, FGuid> OldGuidToNewGuidMap;
	for ( const FMovieScenePossessable& ActorPossessableToFix : ActorsPossessablesToFix )
	{
		AActor** ActorPtr = ActorNameToActorMap.Find( ActorPossessableToFix.GetName() );
		if ( ActorPtr != nullptr )
		{
			FGuid OldGuid = ActorPossessableToFix.GetGuid();

			// The actor might have an existing guid while the possessable with the same name might not. 
			// In that case, make sure we also replace the existing guid with the new guid 
			FGuid ExistingGuid = FindObjectId( **ActorPtr, ActiveTemplateIDs.Top() );

			FGuid NewGuid = DoAssignActor( ActorPtr, 1, ActorPossessableToFix.GetGuid() );

			OldGuidToNewGuidMap.Add(OldGuid, NewGuid);

			if (ExistingGuid.IsValid())
			{
				OldGuidToNewGuidMap.Add(ExistingGuid, NewGuid);
			}
		}
	}

	// Fixup any section bindings
	for (UMovieSceneSection* Section : FocusedMovieScene->GetAllSections())
	{
		Section->OnBindingsUpdated(OldGuidToNewGuidMap);
	}
}

void FSequencer::RebindPossessableReferences()
{
	UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	UMovieScene* FocusedMovieScene = FocusedSequence->GetMovieScene();

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("RebindAllPossessables", "Rebind Possessable References"));

	FocusedSequence->Modify();

	TMap<FGuid, TArray<UObject*, TInlineAllocator<1>>> AllObjects;

	UObject* PlaybackContext = PlaybackContextAttribute.Get(nullptr);

	for (int32 Index = 0; Index < FocusedMovieScene->GetPossessableCount(); Index++)
	{
		const FMovieScenePossessable& Possessable = FocusedMovieScene->GetPossessable(Index);

		TArray<UObject*, TInlineAllocator<1>>& References = AllObjects.FindOrAdd(Possessable.GetGuid());
		FocusedSequence->LocateBoundObjects(Possessable.GetGuid(), PlaybackContext, References);
	}

	for (auto& Pair : AllObjects)
	{
		// Only rebind things if they exist
		if (Pair.Value.Num() > 0)
		{
			FocusedSequence->UnbindPossessableObjects(Pair.Key);
			for (UObject* Object : Pair.Value)
			{
				FocusedSequence->BindPossessableObject(Pair.Key, *Object, PlaybackContext);
			}
		}
	}
}

void FSequencer::ImportFBX()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	TMap<FGuid, FString> ObjectBindingNameMap;

	TArray<TSharedRef<FSequencerObjectBindingNode>> RootObjectBindingNodes;
	GetRootObjectBindingNodes(NodeTree->GetRootNodes(), RootObjectBindingNodes);

	for (auto RootObjectBindingNode : RootObjectBindingNodes)
	{
		FGuid ObjectBinding = RootObjectBindingNode.Get().GetObjectBinding();

		ObjectBindingNameMap.Add(ObjectBinding, RootObjectBindingNode.Get().GetDisplayName().ToString());
	}

	MovieSceneToolHelpers::ImportFBX(MovieScene, *this, ObjectBindingNameMap, TOptional<bool>());
}

void FSequencer::ImportFBXOntoSelectedNodes()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// The object binding and names to match when importing from fbx
	TMap<FGuid, FString> ObjectBindingNameMap;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			FGuid ObjectBinding = ObjectBindingNode.Get().GetObjectBinding();

			ObjectBindingNameMap.Add(ObjectBinding, ObjectBindingNode.Get().GetDisplayName().ToString());
		}
	}

	MovieSceneToolHelpers::ImportFBX(MovieScene, *this, ObjectBindingNameMap, TOptional<bool>(false));
}

void FSequencer::ExportFBX()
{
	TArray<UExporter*> Exporters;
	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bExportFileNamePicked = false;
	if ( DesktopPlatform != NULL )
	{
		FString FileTypes = "FBX document|*.fbx";
		UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(UExporter::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			UExporter* Default = It->GetDefaultObject<UExporter>();
			if (!Default->SupportsObject(Sequence))
			{
				continue;
			}

			for (int32 i = 0; i < Default->FormatExtension.Num(); ++i)
			{
				const FString& FormatExtension = Default->FormatExtension[i];
				const FString& FormatDescription = Default->FormatDescription[i];

				if (FileTypes.Len() > 0)
				{
					FileTypes += TEXT("|");
				}
				FileTypes += FormatDescription;
				FileTypes += TEXT("|*.");
				FileTypes += FormatExtension;
			}

			Exporters.Add(Default);
		}

		bExportFileNamePicked = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT( "ExportLevelSequence", "Export Level Sequence" ).ToString(),
			*( FEditorDirectories::Get().GetLastDirectory( ELastDirectory::FBX ) ),
			TEXT( "" ),
			*FileTypes,
			EFileDialogFlags::None,
			SaveFilenames );
	}

	if ( bExportFileNamePicked )
	{
		FString ExportFilename = SaveFilenames[0];
		FEditorDirectories::Get().SetLastDirectory( ELastDirectory::FBX, FPaths::GetPath( ExportFilename ) ); // Save path as default for next time.

		// Select selected nodes if there are selected nodes
		TArray<FGuid> Bindings;
		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);
				Bindings.Add(ObjectBindingNode.Get().GetObjectBinding());

				TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;
				SequencerHelpers::GetDescendantNodes(Node, DescendantNodes);
				for (auto DescendantNode : DescendantNodes)
				{
					if (!Selection.IsSelected(DescendantNode) && DescendantNode->GetType() == ESequencerNode::Object)
					{
						auto DescendantObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(DescendantNode);
						Bindings.Add(DescendantObjectBindingNode.Get().GetObjectBinding());
					}
				}
			}
		}

		FString FileExtension = FPaths::GetExtension(ExportFilename);
		if (FileExtension == TEXT("fbx"))
		{
			ExportFBXInternal(ExportFilename, Bindings);
		}
		else
		{
			for (UExporter* Exporter : Exporters)
			{
				if (Exporter->FormatExtension.Contains(FileExtension))
				{
					USequencerExportTask* ExportTask = NewObject<USequencerExportTask>();
					TStrongObjectPtr<USequencerExportTask> ExportTaskGuard(ExportTask);
					ExportTask->Object = GetFocusedMovieSceneSequence();
					ExportTask->Exporter = nullptr;
					ExportTask->Filename = ExportFilename;
					ExportTask->bSelected = false;
					ExportTask->bReplaceIdentical = true;
					ExportTask->bPrompt = false;
					ExportTask->bUseFileArchive = false;
					ExportTask->bWriteEmptyFiles = false;
					ExportTask->bAutomated = false;
					ExportTask->Exporter = NewObject<UExporter>(GetTransientPackage(), Exporter->GetClass());

					ExportTask->SequencerContext = GetPlaybackContext();

					UExporter::RunAssetExportTask(ExportTask);

					ExportTask->Object = nullptr;
					ExportTask->Exporter = nullptr;
					ExportTask->SequencerContext = nullptr;

					break;
				}
			}
		}
	}
}


void FSequencer::ExportFBXInternal(const FString& ExportFilename, TArray<FGuid>& Bindings)
{
	{
		UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
		//Show the fbx export dialog options
		bool ExportCancel = false;
		bool ExportAll = false;
		Exporter->FillExportOptions(false, true, ExportFilename, ExportCancel, ExportAll);
		if (!ExportCancel)
		{
			UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

			Exporter->CreateDocument();
			Exporter->SetTrasformBaking(false);
			Exporter->SetKeepHierarchy(true);

			const bool bSelectedOnly = (Selection.GetSelectedTracks().Num() + Bindings.Num()) != 0;

			// Make sure external selection is up to date since export could happen on tracks that have been right clicked but not have their underlying bound objects selected yet since that happens on mouse up.
			if (bSelectedOnly)
			{
				SynchronizeExternalSelectionWithSequencerSelection();
			}

			UnFbx::FFbxExporter::FLevelSequenceNodeNameAdapter NodeNameAdapter(MovieScene, this, GetFocusedTemplateID());

			// Export the persistent level and all of it's actors
			UWorld* World = Cast<UWorld>(GetPlaybackContext());
			const bool bSaveAnimSeq = false; //force off saving any AnimSequences since this can conflict when we export the level sequence animations.
			Exporter->ExportLevelMesh(World->PersistentLevel, bSelectedOnly, NodeNameAdapter, bSaveAnimSeq);

			// Export streaming levels and actors
			for (int32 CurLevelIndex = 0; CurLevelIndex < World->GetNumLevels(); ++CurLevelIndex)
			{
				ULevel* CurLevel = World->GetLevel(CurLevelIndex);
				if (CurLevel != NULL && CurLevel != (World->PersistentLevel))
				{
					Exporter->ExportLevelMesh(CurLevel, bSelectedOnly, NodeNameAdapter, bSaveAnimSeq);
				}
			}

			// Export the movie scene data.
			Exporter->ExportLevelSequence(MovieScene, Bindings, this, GetFocusedTemplateID());

			// Export selected or all master tracks
			if (Selection.GetSelectedOutlinerNodes().Num())
			{
				for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
				{
					if (Node->GetType() == ESequencerNode::Track)
					{
						TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
						UMovieSceneTrack* MasterTrack = TrackNode->GetTrack();
						if (MovieScene->GetMasterTracks().Contains(MasterTrack))
						{
							TArray<UMovieSceneTrack*> Tracks;
							Tracks.Add(MasterTrack);
							Exporter->ExportLevelSequenceTracks(MovieScene, this, nullptr, nullptr, Tracks);
						}
					}
				}
			}
			else
			{
				for (UMovieSceneTrack* MasterTrack : MovieScene->GetMasterTracks())
				{
					TArray<UMovieSceneTrack*> Tracks;
					Tracks.Add(MasterTrack);
					Exporter->ExportLevelSequenceTracks(MovieScene, this, nullptr, nullptr, Tracks);
				}
			}

			// Save to disk
			Exporter->WriteToFile(*ExportFilename);
		}
	}
}


void FSequencer::ExportToCameraAnim()
{
	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() != ESequencerNode::Object)
		{
			continue;
		}
		auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

		FGuid Guid = ObjectBindingNode->GetObjectBinding();
		
		MovieSceneToolHelpers::ExportToCameraAnim(GetFocusedMovieSceneSequence()->GetMovieScene(), Guid);
	}
}


void FSequencer::GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted)
{
	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText(DefaultText)
		.OnTextCommitted(OnTextComitted)
		.ClearKeyboardFocusOnCommit(false)
		.SelectAllTextWhenFocused(true)
		.MaxWidth(1024.0f);

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		ToolkitHost.Pin()->GetParentWidget(),
		FWidgetPath(),
		TextEntryPopup,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
	);
}


void FSequencer::CloseEntryPopupMenu()
{
	if (EntryPopupMenu.IsValid())
	{
		EntryPopupMenu.Pin()->Dismiss();
	}
}


void FSequencer::TrimSection(bool bTrimLeft)
{
	FScopedTransaction TrimSectionTransaction( NSLOCTEXT("Sequencer", "TrimSection_Transaction", "Trim Section") );
	MovieSceneToolHelpers::TrimSection(Selection.GetSelectedSections(), GetLocalTime(), bTrimLeft);
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}


void FSequencer::SplitSection()
{
	FScopedTransaction SplitSectionTransaction( NSLOCTEXT("Sequencer", "SplitSection_Transaction", "Split Section") );
	MovieSceneToolHelpers::SplitSection(Selection.GetSelectedSections(), GetLocalTime());
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

const ISequencerEditTool* FSequencer::GetEditTool() const
{
	return SequencerWidget->GetEditTool();
}

TSharedPtr<ISequencerHotspot> FSequencer::GetHotspot() const
{
	return Hotspot;
}

void FSequencer::SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot)
{
	if (!Hotspot.IsValid() || !Hotspot->bIsLocked)
	{
		Hotspot = MoveTemp(NewHotspot);
	}
}

void FSequencer::BindCommands()
{
	const FSequencerCommands& Commands = FSequencerCommands::Get();

	SequencerCommandBindings->MapAction(
		Commands.StepToNextKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToNextCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.ExpandAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ExpandAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.CollapseAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::CollapseAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.SortAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::SortAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodes,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodes));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateSP( this, &FSequencer::SetKey ) );

	SequencerCommandBindings->MapAction(
		Commands.TranslateLeft,
		FExecuteAction::CreateSP( this, &FSequencer::TranslateSelectedKeysAndSections, true) );

	SequencerCommandBindings->MapAction(
		Commands.TranslateRight,
		FExecuteAction::CreateSP( this, &FSequencer::TranslateSelectedKeysAndSections, false) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionLeft,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, true ) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionRight,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, false ) );

	SequencerCommandBindings->MapAction(
		Commands.SplitSection,
		FExecuteAction::CreateSP( this, &FSequencer::SplitSection ) );

	// We can convert to spawnables if anything selected is a root-level possessable
	auto CanConvertToSpawnables = [this]{
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieScenePossessable* Possessable = MovieScene->FindPossessable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Possessable && !Possessable->GetParent().IsValid())
				{
					return true;
				}
			}
		}
		return false;
	};
	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ConvertToSpawnable,
		FExecuteAction::CreateSP(this, &FSequencer::ConvertSelectedNodesToSpawnables),
		FCanExecuteAction::CreateLambda(CanConvertToSpawnables)
	);

	auto AreConvertableSpawnablesSelected = [this] {
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Spawnable && SpawnRegister->CanConvertSpawnableToPossessable(*Spawnable))
				{
					return true;
				}
			}
		}
		return false;
	};

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ConvertToPossessable,
		FExecuteAction::CreateSP(this, &FSequencer::ConvertSelectedNodesToPossessables),
		FCanExecuteAction::CreateLambda(AreConvertableSpawnablesSelected)
	);

	auto AreSpawnablesSelected = [this] {
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Spawnable)
				{
					return true;
				}
			}
		}
		return false;
	};

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().SaveCurrentSpawnableState,
		FExecuteAction::CreateSP(this, &FSequencer::SaveSelectedNodesSpawnableState),
		FCanExecuteAction::CreateLambda(AreSpawnablesSelected)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().RestoreAnimatedState,
		FExecuteAction::CreateSP(this, &FSequencer::RestorePreAnimatedState)
	);

	SequencerCommandBindings->MapAction(
		Commands.SetAutoKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoChangeMode( EAutoChangeMode::AutoKey ); } ),
		FCanExecuteAction::CreateLambda( [this]{ return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoChangeMode() == EAutoChangeMode::AutoKey; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAutoTrack,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoChangeMode(EAutoChangeMode::AutoTrack); } ),
		FCanExecuteAction::CreateLambda([this] { return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoChangeMode() == EAutoChangeMode::AutoTrack; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAutoChangeAll,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoChangeMode(EAutoChangeMode::All); } ),
		FCanExecuteAction::CreateLambda([this] { return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoChangeMode() == EAutoChangeMode::All; } ) );
	
	SequencerCommandBindings->MapAction(
		Commands.SetAutoChangeNone,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoChangeMode(EAutoChangeMode::None); } ),
		FCanExecuteAction::CreateLambda([this] { return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoChangeMode() == EAutoChangeMode::None; } ) );

	SequencerCommandBindings->MapAction(
		Commands.AllowAllEdits,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAllowEditsMode( EAllowEditsMode::AllEdits ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAllowEditsMode() == EAllowEditsMode::AllEdits; } ) );

	SequencerCommandBindings->MapAction(
		Commands.AllowSequencerEditsOnly,
		FExecuteAction::CreateLambda([this] { Settings->SetAllowEditsMode(EAllowEditsMode::AllowSequencerEditsOnly); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAllowEditsMode() == EAllowEditsMode::AllowSequencerEditsOnly; }));

	SequencerCommandBindings->MapAction(
		Commands.AllowLevelEditsOnly,
		FExecuteAction::CreateLambda([this] { Settings->SetAllowEditsMode(EAllowEditsMode::AllowLevelEditsOnly); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAllowEditsMode() == EAllowEditsMode::AllowLevelEditsOnly; }));

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoKeyEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoChangeMode(Settings->GetAutoChangeMode() == EAutoChangeMode::None ? EAutoChangeMode::AutoKey : EAutoChangeMode::None); } ),
		FCanExecuteAction::CreateLambda( [this]{ return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoChangeMode() == EAutoChangeMode::AutoKey; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetKeyChanged,
		FExecuteAction::CreateLambda([this] { Settings->SetKeyGroupMode(EKeyGroupMode::KeyChanged); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetKeyGroupMode() == EKeyGroupMode::KeyChanged; }));

	SequencerCommandBindings->MapAction(
		Commands.SetKeyGroup,
		FExecuteAction::CreateLambda([this] { Settings->SetKeyGroupMode(EKeyGroupMode::KeyGroup); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetKeyGroupMode() == EKeyGroupMode::KeyGroup; }));

	SequencerCommandBindings->MapAction(
		Commands.SetKeyAll,
		FExecuteAction::CreateLambda([this] { Settings->SetKeyGroupMode(EKeyGroupMode::KeyAll); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetKeyGroupMode() == EKeyGroupMode::KeyAll; }));

	SequencerCommandBindings->MapAction(
		Commands.ToggleMarkAtPlayPosition,
		FExecuteAction::CreateSP( this, &FSequencer::ToggleMarkAtPlayPosition));

	SequencerCommandBindings->MapAction(
		Commands.StepToNextMark,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextMark));

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousMark,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousMark));

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoScroll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoScrollEnabled( !Settings->GetAutoScrollEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoScrollEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.FindInContentBrowser,
		FExecuteAction::CreateSP( this, &FSequencer::FindInContentBrowser ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleCombinedKeyframes,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowCombinedKeyframes( !Settings->GetShowCombinedKeyframes() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowCombinedKeyframes(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleChannelColors,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowChannelColors( !Settings->GetShowChannelColors() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowChannelColors(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLabelBrowser,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetLabelBrowserVisible( !Settings->GetLabelBrowserVisible() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLabelBrowserVisible(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowSelectedNodesOnly,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowSelectedNodesOnly( !Settings->GetShowSelectedNodesOnly() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowSelectedNodesOnly(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ChangeTimeDisplayFormat,
		FExecuteAction::CreateLambda( [this]{
			EFrameNumberDisplayFormats NextFormat = (EFrameNumberDisplayFormats)((uint8)Settings->GetTimeDisplayFormat() + 1);
			if (NextFormat == EFrameNumberDisplayFormats::MAX_Count)
			{
				NextFormat = EFrameNumberDisplayFormats::NonDropFrameTimecode;
			}

			// If the next framerate in the list is drop format timecode and we're not in a play rate that supports drop format timecode,
			// then we will skip over it.
			bool bCanShowDropFrameTimecode = FTimecode::IsDropFormatTimecodeSupported(GetFocusedDisplayRate());
			if (!bCanShowDropFrameTimecode && NextFormat == EFrameNumberDisplayFormats::DropFrameTimecode)
			{
				NextFormat = EFrameNumberDisplayFormats::Seconds;
			}
			Settings->SetTimeDisplayFormat( NextFormat );
		} ),
		FCanExecuteAction::CreateLambda([] { return true; }));

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowRangeSlider,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowRangeSlider( !Settings->GetShowRangeSlider() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowRangeSlider(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleIsSnapEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetIsSnapEnabled( !Settings->GetIsSnapEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetIsSnapEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToInterval( !Settings->GetSnapKeyTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToKeys( !Settings->GetSnapKeyTimesToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToInterval( !Settings->GetSnapSectionTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToSections,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToSections( !Settings->GetSnapSectionTimesToSections() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToSections(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToKeys( !Settings->GetSnapPlayTimeToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToInterval( !Settings->GetSnapPlayTimeToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToPressedKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToPressedKey( !Settings->GetSnapPlayTimeToPressedKey() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToPressedKey(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToDraggedKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToDraggedKey( !Settings->GetSnapPlayTimeToDraggedKey() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToDraggedKey(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapCurveValueToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapCurveValueToInterval( !Settings->GetSnapCurveValueToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapCurveValueToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowCurveEditor,
		FExecuteAction::CreateLambda( [this]{ SetShowCurveEditor(!GetShowCurveEditor()); } ),
		FCanExecuteAction::CreateLambda( [this]{ return !IsReadOnly(); } ),
		FIsActionChecked::CreateLambda( [this]{ return GetShowCurveEditor(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLinkCurveEditorTimeRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetLinkCurveEditorTimeRange(!Settings->GetLinkCurveEditorTimeRange()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLinkCurveEditorTimeRange(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowPreAndPostRoll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShouldShowPrePostRoll(!Settings->ShouldShowPrePostRoll()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldShowPrePostRoll(); } ) );

	auto CanCutOrCopy = [this]{
		// For copy tracks
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetNodesWithSelectedKeysOrSections();
		// If this is empty then we are selecting display nodes
		if (SelectedNodes.Num() == 0)
		{
			SelectedNodes = Selection.GetSelectedOutlinerNodes();
			for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
			{
				if (Node->GetType() == ESequencerNode::Track || Node->GetType() == ESequencerNode::Object)
				{
					// if contains one node that can be copied we allow the action
					// later on we will filter out the invalid nodes in CopySelection() or CutSelection()
					return true;
				}
				else if (Node->GetParent().IsValid() && Node->GetParent()->GetType() == ESequencerNode::Track)
				{
					// Although copying only the child nodes (ex. translation) is not allowed, we still show the copy & cut button
					// so that users are not misled and can achieve this in copy/cut the parent node (ex. transform)
					return true;
				}
			}
			return false;
		}

		UMovieSceneTrack* Track = nullptr;
		for (FSequencerSelectedKey Key : Selection.GetSelectedKeys())
		{
			if (!Track)
			{
				Track = Key.Section->GetTypedOuter<UMovieSceneTrack>();
			}
			if (!Track || Track != Key.Section->GetTypedOuter<UMovieSceneTrack>())
			{
				return false;
			}
		}
		return true;
	};

	auto CanDelete = [this]{
		return Selection.GetSelectedKeys().Num() || Selection.GetSelectedSections().Num() || Selection.GetSelectedOutlinerNodes().Num();
	};

	auto CanDuplicate = [this]{
		// For duplicate object tracks
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetNodesWithSelectedKeysOrSections();
		if (SelectedNodes.Num() == 0)
		{
			SelectedNodes = Selection.GetSelectedOutlinerNodes();
			for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
			{
				if (Node->GetType() == ESequencerNode::Object)
				{
					// if contains one node that can be copied we allow the action
					return true;
				}
			}
			return false;
		}
		return false;
	};

	auto IsSelectionRangeNonEmpty = [this]{
		UMovieSceneSequence* EditedSequence = GetFocusedMovieSceneSequence();
		if (!EditedSequence || !EditedSequence->GetMovieScene())
		{
			return false;
		}

		return !EditedSequence->GetMovieScene()->GetSelectionRange().IsEmpty();
	};

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateLambda([this]
		{
			Selection.GetSelectedOutlinerNodes().Array()[0]->OnRenameRequested().Broadcast();
		}),
		FCanExecuteAction::CreateLambda([this]
		{
			return (Selection.GetSelectedOutlinerNodes().Num() > 0) && (Selection.GetSelectedOutlinerNodes().Array()[0]->CanRenameNode());
		})
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FSequencer::CutSelection),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FSequencer::CopySelection),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &FSequencer::DuplicateSelection),
		FCanExecuteAction::CreateLambda(CanDuplicate)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP( this, &FSequencer::DeleteSelectedItems ),
		FCanExecuteAction::CreateLambda(CanDelete));

	SequencerCommandBindings->MapAction(
		Commands.TogglePlaybackRangeLocked,
		FExecuteAction::CreateSP( this, &FSequencer::TogglePlaybackRangeLocked ),
		FCanExecuteAction::CreateLambda( [this] { return GetFocusedMovieSceneSequence() != nullptr;	} ),
		FIsActionChecked::CreateSP( this, &FSequencer::IsPlaybackRangeLocked ));

	SequencerCommandBindings->MapAction(
		Commands.ToggleRerunConstructionScripts,
		FExecuteAction::CreateLambda( [this]{ Settings->SetRerunConstructionScripts( !Settings->ShouldRerunConstructionScripts() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldRerunConstructionScripts(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepCursorInPlaybackRangeWhileScrubbing,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepCursorInPlayRangeWhileScrubbing( !Settings->ShouldKeepCursorInPlayRangeWhileScrubbing() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepCursorInPlayRangeWhileScrubbing(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepCursorInPlaybackRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepCursorInPlayRange( !Settings->ShouldKeepCursorInPlayRange() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepCursorInPlayRange(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepPlaybackRangeInSectionBounds,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepPlayRangeInSectionBounds( !Settings->ShouldKeepPlayRangeInSectionBounds() ); NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepPlayRangeInSectionBounds(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleEvaluateSubSequencesInIsolation,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetEvaluateSubSequencesInIsolation( !Settings->ShouldEvaluateSubSequencesInIsolation() );
			ForceEvaluate();
		} ),
		FCanExecuteAction::CreateLambda( [this]{ return ActiveTemplateIDs.Num() > 1; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldEvaluateSubSequencesInIsolation(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.RenderMovie,
		FExecuteAction::CreateLambda([this]{ RenderMovieInternal(GetPlaybackRange()); }),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this]{ return ExactCast<ULevelSequence>(GetFocusedMovieSceneSequence()) != nullptr; })
	);

	SequencerCommandBindings->MapAction(
		Commands.CreateCamera,
		FExecuteAction::CreateSP(this, &FSequencer::CreateCamera),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this] { return ExactCast<ULevelSequence>(GetFocusedMovieSceneSequence()) != nullptr && IVREditorModule::Get().IsVREditorModeActive() == false; }) //@todo VREditor: Creating a camera while in VR mode disrupts the hmd. This is a temporary fix by hiding the button when in VR mode.
	);

	SequencerCommandBindings->MapAction(
		Commands.DiscardChanges,
		FExecuteAction::CreateSP(this, &FSequencer::DiscardChanges),
		FCanExecuteAction::CreateLambda([this]{
			UMovieSceneSequence* EditedSequence = GetFocusedMovieSceneSequence();
			if (!EditedSequence)
			{
				return false;
			}

			UPackage* EditedPackage = EditedSequence->GetOutermost();

			return ((EditedPackage->FileSize != 0) && EditedPackage->IsDirty());
		})
	);

	SequencerCommandBindings->MapAction(
		Commands.BakeTransform,
		FExecuteAction::CreateSP( this, &FSequencer::BakeTransform ),
		FCanExecuteAction::CreateLambda( []{ return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SyncSectionsUsingSourceTimecode,
		FExecuteAction::CreateSP( this, &FSequencer::SyncSectionsUsingSourceTimecode ),
		FCanExecuteAction::CreateLambda( [this]{ return (GetSelection().GetSelectedSections().Num() > 1); } ) );

	SequencerCommandBindings->MapAction(
		Commands.FixActorReferences,
		FExecuteAction::CreateSP( this, &FSequencer::FixActorReferences ),
		FCanExecuteAction::CreateLambda( []{ return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.RebindPossessableReferences,
		FExecuteAction::CreateSP( this, &FSequencer::RebindPossessableReferences ),
		FCanExecuteAction::CreateLambda( []{ return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.ImportFBX,
		FExecuteAction::CreateSP( this, &FSequencer::ImportFBX ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.ExportFBX,
		FExecuteAction::CreateSP( this, &FSequencer::ExportFBX ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.ExportToCameraAnim,
		FExecuteAction::CreateSP( this, &FSequencer::ExportToCameraAnim ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BindCommands(SequencerCommandBindings);
	}

	// copy subset of sequencer commands to shared commands
	*SequencerSharedBindings = *SequencerCommandBindings;

	// Sequencer-only bindings
	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicAuto,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Auto));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicUser,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_User));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicBreak,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Break));

	SequencerCommandBindings->MapAction(
		Commands.ToggleWeightedTangents,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleInterpTangentWeightMode));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationLinear,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Linear, ERichCurveTangentMode::RCTM_Auto));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationConstant,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Constant, ERichCurveTangentMode::RCTM_Auto));

	SequencerCommandBindings->MapAction(
		Commands.TogglePlay,
		FExecuteAction::CreateSP( this, &FSequencer::TogglePlay ));

	SequencerCommandBindings->MapAction(
		Commands.PlayForward,
		FExecuteAction::CreateLambda( [this] { OnPlayForward(false); }));

	SequencerCommandBindings->MapAction(
		Commands.JumpToStart,
		FExecuteAction::CreateSP( this, &FSequencer::JumpToStart ));

	SequencerCommandBindings->MapAction(
		Commands.JumpToEnd,
		FExecuteAction::CreateSP( this, &FSequencer::JumpToEnd ));

	SequencerCommandBindings->MapAction(
		Commands.ShuttleForward,
		FExecuteAction::CreateSP( this, &FSequencer::ShuttleForward ));

	SequencerCommandBindings->MapAction(
		Commands.ShuttleBackward,
		FExecuteAction::CreateSP( this, &FSequencer::ShuttleBackward ));

	SequencerCommandBindings->MapAction(
		Commands.Pause,
		FExecuteAction::CreateSP( this, &FSequencer::Pause ));

	SequencerCommandBindings->MapAction(
		Commands.StepForward,
		FExecuteAction::CreateSP( this, &FSequencer::StepForward ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.StepBackward,
		FExecuteAction::CreateSP( this, &FSequencer::StepBackward ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeEnd,
		FExecuteAction::CreateLambda([this]{ SetSelectionRangeEnd(); }));

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeStart,
		FExecuteAction::CreateLambda([this]{ SetSelectionRangeStart(); }));

	SequencerCommandBindings->MapAction(
		Commands.ResetSelectionRange,
		FExecuteAction::CreateLambda([this]{ ResetSelectionRange(); }),
		FCanExecuteAction::CreateLambda(IsSelectionRangeNonEmpty));

	SequencerCommandBindings->MapAction(
		Commands.SelectKeysInSelectionRange,
		FExecuteAction::CreateSP(this, &FSequencer::SelectInSelectionRange, true, false),
		FCanExecuteAction::CreateLambda(IsSelectionRangeNonEmpty));

	SequencerCommandBindings->MapAction(
		Commands.SelectSectionsInSelectionRange,
		FExecuteAction::CreateSP(this, &FSequencer::SelectInSelectionRange, false, true),
		FCanExecuteAction::CreateLambda(IsSelectionRangeNonEmpty));

	SequencerCommandBindings->MapAction(
		Commands.SelectAllInSelectionRange,
		FExecuteAction::CreateSP(this, &FSequencer::SelectInSelectionRange, true, true),
		FCanExecuteAction::CreateLambda(IsSelectionRangeNonEmpty));

	SequencerCommandBindings->MapAction(
		Commands.StepToNextShot,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextShot ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousShot,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousShot ) );

	SequencerCommandBindings->MapAction(
		Commands.SetStartPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeStart ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.ResetViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ResetViewRange ) );

	SequencerCommandBindings->MapAction(
		Commands.ZoomInViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ZoomInViewRange ),
		FCanExecuteAction(),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.ZoomOutViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ZoomOutViewRange ),		
		FCanExecuteAction(),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.SetEndPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeEnd ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeToNextShot,
		FExecuteAction::CreateSP( this, &FSequencer::SetSelectionRangeToShot, true ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeToPreviousShot,
		FExecuteAction::CreateSP( this, &FSequencer::SetSelectionRangeToShot, false ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.SetPlaybackRangeToAllShots,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeToAllShots ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	// Curve Visibility
	SequencerCommandBindings->MapAction(Commands.SetAllCurveVisibility,
		FExecuteAction::CreateLambda( [this]{ Settings->SetCurveVisibility( ECurveEditorCurveVisibility::AllCurves ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetCurveVisibility() == ECurveEditorCurveVisibility::AllCurves; } ) );

	SequencerCommandBindings->MapAction(Commands.SetSelectedCurveVisibility,
		FExecuteAction::CreateLambda( [this]{ Settings->SetCurveVisibility( ECurveEditorCurveVisibility::SelectedCurves ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetCurveVisibility() == ECurveEditorCurveVisibility::SelectedCurves; } ) );

	SequencerCommandBindings->MapAction(Commands.SetAnimatedCurveVisibility,
		FExecuteAction::CreateLambda( [this]{ Settings->SetCurveVisibility( ECurveEditorCurveVisibility::AnimatedCurves ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetCurveVisibility() == ECurveEditorCurveVisibility::AnimatedCurves; } ) );

	// bind widget specific commands
	SequencerWidget->BindCommands(SequencerCommandBindings);
}

void FSequencer::BuildAddTrackMenu(class FMenuBuilder& MenuBuilder)
{
	if (IsLevelEditorSequencer())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoadRecording", "Load Recorded Data"),
			LOCTEXT("LoadRecordingDataTooltip", "Load in saved data from a previous recording."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetTreeFolderOpen"),
			FUIAction(FExecuteAction::CreateRaw(this, &FSequencer::OnLoadRecordedData)));
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT( "AddFolder", "Add Folder" ),
		LOCTEXT( "AddFolderToolTip", "Adds a new folder." ),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetTreeFolderOpen" ),
		FUIAction( FExecuteAction::CreateRaw( this, &FSequencer::OnAddFolder ) ) );

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		if (TrackEditors[i]->SupportsSequence(GetFocusedMovieSceneSequence()))
		{
			TrackEditors[i]->BuildAddTrackMenu(MenuBuilder);
		}
	}
}


void FSequencer::BuildAddObjectBindingsMenu(class FMenuBuilder& MenuBuilder)
{
	for (int32 i = 0; i < ObjectBindings.Num(); ++i)
	{
		if (ObjectBindings[i]->SupportsSequence(GetFocusedMovieSceneSequence()))
		{
			ObjectBindings[i]->BuildSequencerAddMenu(MenuBuilder);
		}
	}
}

void FSequencer::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingTrackMenu(MenuBuilder, ObjectBinding, ObjectClass);
	}
}


void FSequencer::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingEditButtons(EditBox, ObjectBinding, ObjectClass);
	}
}

void FSequencer::UpdateTimeBases()
{
	UMovieSceneSequence* RootSequencePtr = GetRootMovieSceneSequence();
	UMovieScene*         RootMovieScene  = RootSequencePtr ? RootSequencePtr->GetMovieScene() : nullptr;

	if (RootMovieScene)
	{
		EMovieSceneEvaluationType EvaluationType  = RootMovieScene->GetEvaluationType();
		FFrameRate                TickResolution  = RootMovieScene->GetTickResolution();
		FFrameRate                DisplayRate     = EvaluationType == EMovieSceneEvaluationType::FrameLocked ? RootMovieScene->GetDisplayRate() : TickResolution;

		if (DisplayRate != PlayPosition.GetInputRate())
		{
			bNeedsEvaluate = true;
		}

		// We set the play position in terms of the display rate,
		// but want evaluation ranges in the moviescene's tick resolution
		PlayPosition.SetTimeBase(DisplayRate, TickResolution, EvaluationType);
	}
}

void FSequencer::ResetTimeController()
{
	switch (GetRootMovieSceneSequence()->GetMovieScene()->GetClockSource())
	{
	case EUpdateClockSource::Audio:    TimeController = MakeShared<FMovieSceneTimeController_AudioClock>();    break;
	case EUpdateClockSource::Platform: TimeController = MakeShared<FMovieSceneTimeController_PlatformClock>(); break;
	case EUpdateClockSource::Timecode: TimeController = MakeShared<FMovieSceneTimeController_TimecodeClock>(); break;
	default:                           TimeController = MakeShared<FMovieSceneTimeController_Tick>();          break;
	}

	TimeController->PlayerStatusChanged(PlaybackState, GetGlobalTime());
}


void FSequencer::BuildCustomContextMenuForGuid(FMenuBuilder& MenuBuilder, FGuid ObjectBinding)
{
	SequencerWidget->BuildCustomContextMenuForGuid(MenuBuilder, ObjectBinding);
}

FKeyAttributes FSequencer::GetDefaultKeyAttributes() const
{
	switch (Settings->GetKeyInterpolation())
	{
	case EMovieSceneKeyInterpolation::User:     return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_User);
	case EMovieSceneKeyInterpolation::Break:    return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Break);
	case EMovieSceneKeyInterpolation::Linear:   return FKeyAttributes().SetInterpMode(RCIM_Linear).SetTangentMode(RCTM_Auto);
	case EMovieSceneKeyInterpolation::Constant: return FKeyAttributes().SetInterpMode(RCIM_Constant).SetTangentMode(RCTM_Auto);
	default:                                    return FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Auto);
	}
}

bool FSequencer::GetGridMetrics(float PhysicalWidth, double& OutMajorInterval, int32& OutMinorDivisions) const
{
	FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	double BiggestTime = GetViewRange().GetUpperBoundValue();
	FString TickString = GetNumericTypeInterface()->ToString((BiggestTime * GetFocusedDisplayRate()).FrameNumber.Value);
	FVector2D MaxTextSize = FontMeasureService->Measure(TickString, SmallLayoutFont);

	static float MajorTickMultiplier = 2.f;

	float MinTickPx = MaxTextSize.X + 5.f;
	float DesiredMajorTickPx = MaxTextSize.X * MajorTickMultiplier;

	if (PhysicalWidth > 0)
	{
		return GetFocusedDisplayRate().ComputeGridSpacing(
			PhysicalWidth / GetViewRange().Size<double>(),
			OutMajorInterval,
			OutMinorDivisions,
			MinTickPx,
			DesiredMajorTickPx);
	}

	return false;
}

double FSequencer::GetDisplayRateDeltaFrameCount() const
{
	return GetFocusedTickResolution().AsDecimal() * GetFocusedDisplayRate().AsInterval();
}

void FSequencer::RecompileDirtyDirectors()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

	TSet<UMovieSceneSequence*> AllSequences;

	// Gather all sequences in the hierarchy
	if (UMovieSceneSequence* Sequence = RootSequence.Get())
	{
		AllSequences.Add(Sequence);
	}

	for (const TTuple<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& Pair : RootTemplateInstance.GetHierarchy().AllSubSequenceData())
	{
		if (UMovieSceneSequence* Sequence = Pair.Value.GetSequence())
		{
			AllSequences.Add(Sequence);
		}
	}

	// Recompile them all if they are dirty
	for (UMovieSceneSequence* Sequence : AllSequences)
	{
		FMovieSceneSequenceEditor* SequenceEditor = SequencerModule.FindSequenceEditor(Sequence->GetClass());
		UBlueprint*                DirectorBP     = SequenceEditor ? SequenceEditor->GetDirectorBlueprint(Sequence) : nullptr;

		if (DirectorBP && (DirectorBP->Status == BS_Unknown || DirectorBP->Status == BS_Dirty))
		{
			FKismetEditorUtilities::CompileBlueprint(DirectorBP);
		}
	}
}


#undef LOCTEXT_NAMESPACE

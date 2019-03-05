// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Recorder/TakeRecorder.h"
#include "TakePreset.h"
#include "TakeMetaData.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderOverlayWidget.h"
#include "LevelSequence.h"
#include "Tickable.h"
#include "AssetRegistryModule.h"
#include "IAssetRegistry.h"
#include "Stats/Stats.h"
#include "ISequencer.h"
#include "SequencerSettings.h"
#include "MovieSceneTimeHelpers.h"
#include "TakesUtils.h"

// LevelSequenceEditor includes
#include "ILevelSequenceEditorToolkit.h"

// Engine includes
#include "GameFramework/WorldSettings.h"

// UnrealEd includes
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "ObjectTools.h"

// Slate includes
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/INotificationWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SButton.h"

// LevelEditor includes
#include "ILevelViewport.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "TakeRecorder"

DEFINE_LOG_CATEGORY(ManifestSerialization);

class STakeRecorderNotification : public SCompoundWidget, public INotificationWidget
{
public:
	SLATE_BEGIN_ARGS(STakeRecorderNotification){}
	SLATE_END_ARGS()

	void SetOwner(TSharedPtr<SNotificationItem> InOwningNotification)
	{
		WeakOwningNotification = InOwningNotification;
	}

	void Construct(const FArguments& InArgs, UTakeRecorder* InTakeRecorder, ULevelSequence* InFinishedAsset = nullptr)
	{
		WeakRecorder = InTakeRecorder;
		WeakFinishedAsset = InFinishedAsset;
		TakeRecorderState = InTakeRecorder->GetState();

		UTakeMetaData* TakeMetaData = InTakeRecorder->GetSequence()->FindMetaData<UTakeMetaData>();
		check(TakeMetaData);

		ChildSlot
		[
			SNew(SBorder)
			.Padding(FMargin(15.0f))
			.BorderImage(FCoreStyle::Get().GetBrush("NotificationList.ItemBackground"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(FMargin(0,0,0,5.0f))
				.HAlign(HAlign_Right)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::Get().GetFontStyle(TEXT("NotificationList.FontBold")))
						.Text(FText::Format(LOCTEXT("RecordingTitleFormat", "Take {0} of slate {1}"), FText::AsNumber(TakeMetaData->GetTakeNumber()), FText::FromString(TakeMetaData->GetSlate())))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(15.f,0,0,0))
					[
						SAssignNew(Throbber, SThrobber)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0,0,0,5.0f))
				.HAlign(HAlign_Right)
				[
					SAssignNew(TextBlock, STextBlock)
					.Font(FCoreStyle::Get().GetFontStyle(TEXT("NotificationList.FontLight")))
					.Text(GetDetailText())
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(Hyperlink, SHyperlink)
						.Text(LOCTEXT("BrowseToAsset", "Browse To..."))
						.OnNavigate(this, &STakeRecorderNotification::BrowseToAssetFolder)
						.Visibility(this, &STakeRecorderNotification::CanBrowseToAssetFolder)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5.0f,0,0,0))
					.VAlign(VAlign_Center)
					[
						SAssignNew(Button, SButton)
						.Text(LOCTEXT("StopButton", "Stop"))
						.OnClicked(this, &STakeRecorderNotification::ButtonClicked)
					]
				]
			]
		];
	}

private:

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		bool bCloseNotification = false;
		bool bCloseImmediately = false;
		if (WeakFinishedAsset.IsValid())
		{
			TextBlock->SetText(GetDetailText());

			Throbber->SetVisibility(EVisibility::Collapsed);
			Button->SetVisibility(EVisibility::Collapsed);

			return;
		}
		else if (WeakRecorder.IsStale())
		{
			// Reset so we don't continually close the notification
			bCloseImmediately = true;
		}
		else if (UTakeRecorder* Recorder = WeakRecorder.Get())
		{
			ETakeRecorderState NewTakeRecorderState = Recorder->GetState();

			if (NewTakeRecorderState == ETakeRecorderState::CountingDown)
			{
				// When counting down the text may change on tick
				TextBlock->SetText(GetDetailText());
			}

			if (NewTakeRecorderState != TakeRecorderState)
			{
				TextBlock->SetText(GetDetailText());

				if (NewTakeRecorderState == ETakeRecorderState::Stopped || NewTakeRecorderState == ETakeRecorderState::Cancelled)
				{
					Throbber->SetVisibility(EVisibility::Collapsed);
					Button->SetVisibility(EVisibility::Collapsed);

					bCloseNotification = true;
				}
			}

			TakeRecorderState = NewTakeRecorderState;
		}

		TSharedPtr<SNotificationItem> Owner = WeakOwningNotification.Pin();
		if ((bCloseNotification || bCloseImmediately) && Owner.IsValid())
		{
			if (bCloseImmediately)
			{
				Owner->SetFadeOutDuration(0.f);
				Owner->SetExpireDuration(0.f);
			}

			Owner->ExpireAndFadeout();

			// Remove our reference to the owner now that it's fading out
			Owner = nullptr;
		}
	}

	FText GetDetailText() const
	{
		if (WeakFinishedAsset.IsValid())
		{
			return LOCTEXT("CompleteText", "Recording Complete");
		}

		UTakeRecorder* Recorder = WeakRecorder.Get();
		if (Recorder)
		{
			if (Recorder->GetState() == ETakeRecorderState::CountingDown)
			{
				return FText::Format(LOCTEXT("CountdownText", "Recording in {0}s..."), FText::AsNumber(FMath::CeilToInt(Recorder->GetCountdownSeconds())));
			}
			else if (Recorder->GetState() == ETakeRecorderState::Stopped)
			{
				return LOCTEXT("CompleteText", "Recording Complete");
			}
			else if (Recorder->GetState() == ETakeRecorderState::Cancelled)
			{
				return LOCTEXT("CancelledText", "Recording Cancelled");
			}
		}

		return LOCTEXT("RecordingText", "Recording...");
	}

	virtual TSharedRef<SWidget> AsWidget() override
	{
		return AsShared();
	}

	// Unused
	virtual void OnSetCompletionState(SNotificationItem::ECompletionState InState) override
	{
	}

private:

	FReply ButtonClicked()
	{
		UTakeRecorder* Recorder = WeakRecorder.Get();
		if (Recorder)
		{
			Recorder->Stop();
		}
		return FReply::Handled();
	}

	void BrowseToAssetFolder() const
	{
		ULevelSequence* Asset = WeakFinishedAsset.Get();

		if (!Asset)
		{
			UTakeRecorder*  Recorder = WeakRecorder.Get();
			Asset = Recorder ? Recorder->GetSequence() : nullptr;
		}

		if (Asset)
		{
			TArray<FAssetData> Assets{ Asset };
			GEditor->SyncBrowserToObjects(Assets);
		}
	}

	EVisibility CanBrowseToAssetFolder() const
	{
		if (WeakRecorder.IsValid())
		{
			if (WeakRecorder.Get()->GetState() == ETakeRecorderState::Cancelled)
			{
				return EVisibility::Hidden;
			}
		}

		return EVisibility::Visible;
	}

private:
	TSharedPtr<SWidget> Button, Throbber, Hyperlink;
	TSharedPtr<STextBlock> TextBlock;

	ETakeRecorderState TakeRecorderState;
	TWeakPtr<SNotificationItem> WeakOwningNotification;
	TWeakObjectPtr<UTakeRecorder> WeakRecorder;

	/* Optional asset */
	TWeakObjectPtr<ULevelSequence> WeakFinishedAsset;
};


class FTickableTakeRecorder : public FTickableGameObject
{
public:

	TWeakObjectPtr<UTakeRecorder> WeakRecorder;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTickableTakeRecorder, STATGROUP_Tickables);
	}

	//Make sure it always ticks, otherwise we can miss recording, in particularly when time code is always increasing throughout the system.
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

	virtual bool IsTickableInEditor() const override
	{
		return true;
	}

	virtual UWorld* GetTickableGameObjectWorld() const override
	{
		UTakeRecorder* Recorder = WeakRecorder.Get();
		return Recorder ? Recorder->GetWorld() : nullptr;
	}

	virtual void Tick(float DeltaTime) override
	{
		if (UTakeRecorder* Recorder = WeakRecorder.Get())
		{
			Recorder->Tick(DeltaTime);
		}
	}
};

FTickableTakeRecorder TickableTakeRecorder;

// Static members of UTakeRecorder
UTakeRecorder*          UTakeRecorder::CurrentRecorder = nullptr;
FOnTakeRecordingInitialized UTakeRecorder::OnRecordingInitializedEvent;

// Static functions for UTakeRecorder
UTakeRecorder* UTakeRecorder::GetActiveRecorder()
{
	return CurrentRecorder;
}

FOnTakeRecordingInitialized& UTakeRecorder::OnRecordingInitialized()
{
	return OnRecordingInitializedEvent;
}

bool UTakeRecorder::SetActiveRecorder(UTakeRecorder* NewActiveRecorder)
{
	if (CurrentRecorder)
	{
		return false;
	}

	CurrentRecorder = NewActiveRecorder;
	TickableTakeRecorder.WeakRecorder = CurrentRecorder;
	OnRecordingInitializedEvent.Broadcast(NewActiveRecorder);
	return true;
}

// Non-static api for UTakeRecorder

UTakeRecorder::UTakeRecorder(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	CountdownSeconds = 0.f;
	SequenceAsset = nullptr;
	OverlayWidget = nullptr;
}

bool UTakeRecorder::Initialize(ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& InParameters, FText* OutError)
{
	if (GetActiveRecorder())
	{
		if (OutError)
		{
			*OutError = LOCTEXT("RecordingInProgressError", "A recording is currently in progress.");
		}
		return false;
	}

	if (MetaData->GetSlate().IsEmpty())
	{
		if (OutError)
		{
			*OutError = LOCTEXT("NoSlateSpecifiedError", "No slate specified.");
		}
		return false;
	}

	if (!CreateDestinationAsset(*InParameters.Project.GetTakeAssetPath(), LevelSequenceBase, Sources, MetaData, OutError))
	{
		return false;
	}

	if (!InitializeSequencer(OutError))
	{
		return false;
	}

	// -----------------------------------------------------------
	// Anything after this point assumes successful initialization
	// -----------------------------------------------------------

	AddToRoot();

	Parameters = InParameters;
	State      = ETakeRecorderState::CountingDown;

	// Figure out which world we're recording from
	DiscoverSourceWorld();

	// Perform any other parameter-configurable initialization. Must have a valid world at this point.
	InitializeFromParameters();

	// Open a recording notification
	// @todo: is this too intrusive? does it potentially overlap the takerecorder slate UI?
	{
		TSharedRef<STakeRecorderNotification> Content = SNew(STakeRecorderNotification, this);

		FNotificationInfo Info(Content);
		Info.bFireAndForget = false;
		Info.ExpireDuration = 5.f;

		TSharedPtr<SNotificationItem> PendingNotification = FSlateNotificationManager::Get().AddNotification(Info);
		Content->SetOwner(PendingNotification);
	}

	ensure(SetActiveRecorder(this));

	if (WeakSequencer.Pin().IsValid())
	{
		USequencerSettings* SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("TakeRecorderSequenceEditor"));

		CachedAllowEditsMode = SequencerSettings->GetAllowEditsMode();
		CachedAutoChangeMode = SequencerSettings->GetAutoChangeMode();
		
		//When we start recording we don't want to track anymore.  It will be restored when stopping recording.
		SequencerSettings->SetAllowEditsMode(EAllowEditsMode::AllEdits);
		SequencerSettings->SetAutoChangeMode(EAutoChangeMode::None);

		WeakSequencer.Pin()->SetSequencerSettings(SequencerSettings);
	}

	return true;
}

void UTakeRecorder::DiscoverSourceWorld()
{
	UWorld* WorldToRecordIn = nullptr;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			WorldToRecordIn = WorldContext.World();
			break;
		}
		else if (WorldContext.WorldType == EWorldType::Editor)
		{
			WorldToRecordIn = WorldContext.World();
		}
	}

	check(WorldToRecordIn);
	WeakWorld = WorldToRecordIn;

	UClass* Class = StaticLoadClass(UTakeRecorderOverlayWidget::StaticClass(), nullptr, TEXT("/Takes/UMG/DefaultRecordingOverlay.DefaultRecordingOverlay_C"));
	if (Class)
	{
		OverlayWidget = CreateWidget<UTakeRecorderOverlayWidget>(WorldToRecordIn, Class);
		OverlayWidget->SetFlags(RF_Transient);
		OverlayWidget->SetRecorder(this);
		OverlayWidget->AddToViewport();
	}

	// If recording via PIE, be sure to stop recording cleanly when PIE ends
	if (WorldToRecordIn->WorldType == EWorldType::PIE)
	{
		FEditorDelegates::EndPIE.AddUObject(this, &UTakeRecorder::HandleEndPIE);
	}
}

bool UTakeRecorder::CreateDestinationAsset(const TCHAR* AssetPathFormat, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError)
{
	check(LevelSequenceBase && Sources && MetaData);

	FString   PackageName = MetaData->GenerateAssetPath(AssetPathFormat);
	 
	// Initialize a new package, ensuring that it has a unique name
	if (!TakesUtils::CreateNewAssetPackage<ULevelSequence>(PackageName, SequenceAsset, OutError, LevelSequenceBase))
	{
		return false;
	}

	// Copy the sources into the level sequence for future reference (and potentially mutation throughout recording)
	SequenceAsset->CopyMetaData(Sources);

	UMovieScene*   MovieScene    = SequenceAsset->GetMovieScene();
	UTakeMetaData* AssetMetaData = SequenceAsset->CopyMetaData(MetaData);

	// Ensure the asset meta-data is unlocked for the recording (it is later Locked when the recording finishes)
	AssetMetaData->Unlock();
	AssetMetaData->ClearFlags(RF_Transient);

	FDateTime UtcNow = FDateTime::UtcNow();
	AssetMetaData->SetTimestamp(UtcNow);

	// @todo: duration / tick resolution / sample rate / frame rate needs some clarification between sync clocks, template sequences and meta data
	if (AssetMetaData->GetDuration() > 0)
	{
		TRange<FFrameNumber> PlaybackRange = TRange<FFrameNumber>::Inclusive(0, ConvertFrameTime(AssetMetaData->GetDuration(), AssetMetaData->GetFrameRate(), MovieScene->GetTickResolution()).CeilToFrame());
		MovieScene->SetPlaybackRange(PlaybackRange);
	}
	MovieScene->SetDisplayRate(AssetMetaData->GetFrameRate());

	SequenceAsset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(SequenceAsset);

	return true;
}

bool UTakeRecorder::InitializeSequencer(FText* OutError)
{
	// Open the sequence and set the sequencer ptr
	FAssetEditorManager::Get().OpenEditorForAsset(SequenceAsset);

	IAssetEditorInstance*        AssetEditor         = FAssetEditorManager::Get().FindEditorForAsset(SequenceAsset, false);
	ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);

	WeakSequencer = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;

	if (!WeakSequencer.Pin().IsValid())
	{
		if (OutError)
		{
			*OutError = FText::Format(LOCTEXT("FailedToOpenSequencerError", "Failed to open Sequencer for asset '{0}."), FText::FromString(SequenceAsset->GetPathName()));
		}
		return false;
	}

	return true;
}

void UTakeRecorder::InitializeFromParameters()
{
	// Initialize the countdown delay
	CountdownSeconds = Parameters.User.CountdownSeconds;

	// Apply immersive mode if the parameters demand it
	if (Parameters.User.bMaximizeViewport)
	{
		TSharedPtr<ILevelViewport> ActiveLevelViewport = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstActiveViewport();

		// If it's already immersive we just leave it alone
		if (ActiveLevelViewport.IsValid() && !ActiveLevelViewport->IsImmersive())
		{
			ActiveLevelViewport->MakeImmersive(true/*bWantImmersive*/, false/*bAllowAnimation*/);

			// Restore it when we're done
			auto RestoreImmersiveMode = [WeakViewport = TWeakPtr<ILevelViewport>(ActiveLevelViewport)]
			{
				if (TSharedPtr<ILevelViewport> CleaupViewport = WeakViewport.Pin())
				{
					CleaupViewport->MakeImmersive(false/*bWantImmersive*/, false/*bAllowAnimation*/);
				}
			};
			OnStopCleanup.Add(RestoreImmersiveMode);
		}
	}

	// Apply engine Time Dilation
	UWorld* RecordingWorld = GetWorld();
	check(RecordingWorld);
	if (AWorldSettings* WorldSettings = RecordingWorld->GetWorldSettings())
	{
		const float ExistingTimeDilation = WorldSettings->TimeDilation;
		if (Parameters.User.EngineTimeDilation != ExistingTimeDilation)
		{
			WorldSettings->SetTimeDilation(Parameters.User.EngineTimeDilation);

			// Restore it when we're done
			auto RestoreTimeDilation = [ExistingTimeDilation, WeakWorldSettings = MakeWeakObjectPtr(WorldSettings)]
			{
				if (AWorldSettings* CleaupWorldSettings = WeakWorldSettings.Get())
				{
					CleaupWorldSettings->SetTimeDilation(ExistingTimeDilation);
				}
			};
			OnStopCleanup.Add(RestoreTimeDilation);
		}
	}
}

UWorld* UTakeRecorder::GetWorld() const
{
	return WeakWorld.Get();
}

void UTakeRecorder::Tick(float DeltaTime)
{
	if (State == ETakeRecorderState::CountingDown)
	{
		CountdownSeconds = FMath::Max(0.f, CountdownSeconds - DeltaTime);
		if (CountdownSeconds > 0.f)
		{
			return;
		}
		Start();
	}
	else if (State == ETakeRecorderState::Started)
	{
		UTakeRecorderSources* Sources = SequenceAsset->FindOrAddMetaData<UTakeRecorderSources>();
		FFrameTime CurrentFrameTime = Sources->TickRecording(SequenceAsset, DeltaTime);
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			FAnimatedRange Range = Sequencer->GetViewRange();
			UMovieScene*   MovieScene = SequenceAsset->GetMovieScene();
			if(MovieScene)
			{ 
				FFrameRate FrameRate = MovieScene->GetTickResolution();
				double CurrentTimeSeconds = FrameRate.AsSeconds(CurrentFrameTime) + 0.5f;
				CurrentTimeSeconds = CurrentTimeSeconds > Range.GetUpperBoundValue() ? CurrentTimeSeconds: Range.GetUpperBoundValue();
				TRange<double> NewRange(Range.GetLowerBoundValue(), CurrentTimeSeconds );
				Sequencer->SetViewRange(NewRange, EViewRangeInterpolation::Immediate);
			}
		}
	}
}

void UTakeRecorder::Start()
{
	State = ETakeRecorderState::Started;

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		FFrameNumber SequenceStart = MovieScene::DiscreteInclusiveLower(SequenceAsset->GetMovieScene()->GetPlaybackRange());
		// Discard any entity tokens we have so that restore state does not take effect when we delete any sections that recording will be replacing.
		Sequencer->DiscardEntityTokens();
		UMovieScene*   MovieScene = SequenceAsset->GetMovieScene();
		if (MovieScene)
		{
			MovieScene->SetClockSource(EUpdateClockSource::Timecode);
			Sequencer->ResetTimeController();
		}
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing); //set to pause since we will set time while recording
	}
	UTakeRecorderSources* Sources = SequenceAsset->FindMetaData<UTakeRecorderSources>();
	check(Sources);

	UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();
	FDateTime UtcNow = FDateTime::UtcNow();
	AssetMetaData->SetTimestamp(UtcNow);

	//Set the flag to specify if we should auto save the serialized data or not when recording.
	
	MovieSceneSerializationNamespace::bAutoSerialize = Parameters.User.bAutoSerialize;
	if (Parameters.User.bAutoSerialize)
	{
		FString AssetName = AssetMetaData->GenerateAssetPath(Parameters.Project.GetTakeAssetPath());
		FString AssetPath = FPaths::ProjectSavedDir() + AssetName;
		FPaths::RemoveDuplicateSlashes(AssetPath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*AssetPath))
		{
			PlatformFile.CreateDirectoryTree(*AssetPath);
		}

		ManifestSerializer.SetLocalCaptureDir(AssetPath);
		FName SerializedType("Sequence");
		FString Name = SequenceAsset->GetName();
		FManifestFileHeader Header(Name, SerializedType, FGuid());
		FText Error;
		FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *(Name));

		if (!ManifestSerializer.OpenForWrite(FileName, Header, Error))
		{
			UE_LOG(ManifestSerialization, Warning, TEXT("Error Opening Sequence Sequencer File: Subject '%s' Error '%s'"), *(Name), *(Error.ToString()));
		}
	}

	Sources->SetRecordToSubSequence(Parameters.Project.bRecordSourcesIntoSubSequences);

	Sources->StartRecording(SequenceAsset, Parameters.User.bSaveRecordedAssets ?  &ManifestSerializer : nullptr);

	// Refresh sequencer in case the movie scene data has mutated (ie. existing object bindings removed because they will be recorded again)
	if (Sequencer.IsValid())
	{
		Sequencer->RefreshTree();
	}
	OnRecordingStartedEvent.Broadcast(this);
}

void UTakeRecorder::Stop()
{
	USequencerSettings* SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("TakeRecorderSequenceEditor"));

	SequencerSettings->SetAllowEditsMode(CachedAllowEditsMode);
	SequencerSettings->SetAutoChangeMode(CachedAutoChangeMode);

	ManifestSerializer.Close();

	const bool bDidEverStartRecording = State == ETakeRecorderState::Started;

	FEditorDelegates::EndPIE.RemoveAll(this);

	State = bDidEverStartRecording ? ETakeRecorderState::Stopped : ETakeRecorderState::Cancelled;

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		UMovieScene*   MovieScene = SequenceAsset->GetMovieScene();
		if (MovieScene)
		{
			MovieScene->SetClockSource(EUpdateClockSource::Tick);
			Sequencer->ResetTimeController();
		}

	}

	if (bDidEverStartRecording)
	{
		FTakeRecorderSourcesSettings TakeRecorderSourcesSettings;
		TakeRecorderSourcesSettings.bSaveRecordedAssets = Parameters.User.bSaveRecordedAssets || GEditor == nullptr;
		TakeRecorderSourcesSettings.bRemoveRedundantTracks = Parameters.User.bRemoveRedundantTracks;

		UTakeRecorderSources* Sources = SequenceAsset->FindMetaData<UTakeRecorderSources>();
		check(Sources);
		Sources->StopRecording(SequenceAsset, TakeRecorderSourcesSettings);

		TakesUtils::ClampPlaybackRangeToEncompassAllSections(SequenceAsset->GetMovieScene());

		// Lock the sequence so that it can't be changed without implicitly unlocking it now
		SequenceAsset->GetMovieScene()->SetReadOnly(true);

		UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();
		check(AssetMetaData);

		if (GEditor && GEditor->GetEditorWorldContext().World())
		{
			AssetMetaData->SetLevelOrigin(GEditor->GetEditorWorldContext().World()->PersistentLevel);
		}

		// Lock the meta data so it can't be changed without implicitly unlocking it now
		AssetMetaData->Lock();

		if (TakeRecorderSourcesSettings.bSaveRecordedAssets)
		{
			TakesUtils::SaveAsset(SequenceAsset);
		}
	}
	else
	{
		// Recording was cancelled before it started, so delete the asset
		FAssetRegistryModule::AssetDeleted(SequenceAsset);

		// Move the asset to the transient package so that new takes with the same number can be created in its place
		FName DeletedPackageName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), *(FString(TEXT("/Temp/") + SequenceAsset->GetName() + TEXT("_Cancelled"))));
		SequenceAsset->GetOutermost()->Rename(*DeletedPackageName.ToString());

		SequenceAsset->ClearFlags(RF_Standalone | RF_Public);
		SequenceAsset->RemoveFromRoot();
		SequenceAsset->MarkPendingKill();
		SequenceAsset = nullptr;
	}

	// Perform any other cleanup that has been defined for this recording
	for (const TFunction<void()>& Cleanup : OnStopCleanup)
	{
		Cleanup();
	}
	OnStopCleanup.Reset();

	// reset the current recorder and stop us from being ticked
	if (CurrentRecorder == this)
	{
		CurrentRecorder = nullptr;
		TickableTakeRecorder.WeakRecorder = nullptr;

		if (bDidEverStartRecording)
		{
			OnRecordingFinishedEvent.Broadcast(this);
		}
		else
		{
			OnRecordingCancelledEvent.Broadcast(this);
		}
	}

	RemoveFromRoot();
}

FOnTakeRecordingStarted& UTakeRecorder::OnRecordingStarted()
{
	return OnRecordingStartedEvent;
}

FOnTakeRecordingFinished& UTakeRecorder::OnRecordingFinished()
{
	return OnRecordingFinishedEvent;
}

FOnTakeRecordingCancelled& UTakeRecorder::OnRecordingCancelled()
{
	return OnRecordingCancelledEvent;
}

void UTakeRecorder::HandleEndPIE(bool bIsSimulating)
{
	ULevelSequence* FinishedAsset = GetSequence();

	TSharedRef<STakeRecorderNotification> Content = SNew(STakeRecorderNotification, this, FinishedAsset);

	FNotificationInfo Info(Content);
	Info.ExpireDuration = 5.f;

	TSharedPtr<SNotificationItem> PendingNotification = FSlateNotificationManager::Get().AddNotification(Info);
	PendingNotification->SetCompletionState(SNotificationItem::CS_Success);
	
	Stop();
}

#undef LOCTEXT_NAMESPACE
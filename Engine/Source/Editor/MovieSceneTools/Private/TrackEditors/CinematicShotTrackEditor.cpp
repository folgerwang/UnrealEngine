// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/CinematicShotTrackEditor.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Factories/Factory.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Modules/ModuleManager.h"
#include "Application/ThrottleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "LevelEditorViewport.h"
#include "MovieSceneToolHelpers.h"
#include "FCPXML/FCPXMLMovieSceneTranslator.h"
#include "Sections/CinematicShotSection.h"
#include "SequencerUtilities.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "LevelSequence.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneCaptureModule.h"
#include "AssetToolsModule.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "MovieSceneToolsProjectSettings.h"
#include "Editor.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "MovieSceneTimeHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FCinematicShotTrackEditor"

/* FCinematicShotTrackEditor structors
 *****************************************************************************/

FCinematicShotTrackEditor::FCinematicShotTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{
	ThumbnailPool = MakeShareable(new FTrackEditorThumbnailPool(InSequencer));
}


TSharedRef<ISequencerTrackEditor> FCinematicShotTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FCinematicShotTrackEditor(InSequencer));
}


void FCinematicShotTrackEditor::OnInitialize()
{
	OnCameraCutHandle = GetSequencer()->OnCameraCut().AddSP(this, &FCinematicShotTrackEditor::OnUpdateCameraCut);
}


void FCinematicShotTrackEditor::OnRelease()
{
	if (OnCameraCutHandle.IsValid() && GetSequencer().IsValid())
	{
		GetSequencer()->OnCameraCut().Remove(OnCameraCutHandle);
	}
}


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FCinematicShotTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddCinematicShotTrack", "Shot Track"),
		LOCTEXT("AddCinematicShotTooltip", "Adds a shot track."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.CinematicShot"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryCanExecute)
		)
	);
}


TSharedPtr<SWidget> FCinematicShotTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the camera combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("CinematicShotText", "Shot"), FOnGetContent::CreateSP(this, &FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonGetMenuContent), Params.NodeIsHovered, GetSequencer())
	]

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Right)
	.AutoWidth()
	.Padding(4, 0, 0, 0)
	[
		SNew(SCheckBox)
		.IsFocusable(false)
		.IsChecked(this, &FCinematicShotTrackEditor::AreShotsLocked)
		.OnCheckStateChanged(this, &FCinematicShotTrackEditor::OnLockShotsClicked)
		.ToolTipText(this, &FCinematicShotTrackEditor::GetLockShotsToolTip)
		.ForegroundColor(FLinearColor::White)
		.CheckedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
		.CheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
		.CheckedPressedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
		.UncheckedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
		.UncheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
		.UncheckedPressedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
	];
}


TSharedRef<ISequencerSection> FCinematicShotTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FCinematicShotSection(GetSequencer(), ThumbnailPool, SectionObject, SharedThis(this)));
}


bool FCinematicShotTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(Asset);

	if (Sequence == nullptr)
	{
		return false;
	}

	if (!SupportsSequence(Sequence))
	{
		return false;
	}

	//@todo If there's already a subscenes track, allow that track to handle this asset
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene != nullptr && FocusedMovieScene->FindMasterTrack<UMovieSceneSubTrack>() != nullptr)
	{
		return false;
	}

	if (CanAddSubSequence(*Sequence))
	{
		const FScopedTransaction Transaction(LOCTEXT("AddShot_Transaction", "Add Shot"));
		
		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCinematicShotTrackEditor::HandleSequenceAdded, Sequence, RowIndex));

		return true;
	}
		
	FNotificationInfo Info(FText::Format( LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), Sequence->GetDisplayName()));	
	Info.bUseLargeFont = false;
	FSlateNotificationManager::Get().AddNotification(Info);

	return false;
}


bool FCinematicShotTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}


bool FCinematicShotTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneCinematicShotTrack::StaticClass());
}


void FCinematicShotTrackEditor::Tick(float DeltaTime)
{
	TSharedPtr<ISequencer> SequencerPin = GetSequencer();
	if (!SequencerPin.IsValid())
	{
		return;
	}

	EMovieScenePlayerStatus::Type PlaybackState = SequencerPin->GetPlaybackStatus();

	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks() && PlaybackState != EMovieScenePlayerStatus::Playing && PlaybackState != EMovieScenePlayerStatus::Scrubbing)
	{
		SequencerPin->EnterSilentMode();

		FFrameTime SavedTime = SequencerPin->GetGlobalTime().Time;

		if (DeltaTime > 0.f && ThumbnailPool->DrawThumbnails())
		{
			SequencerPin->SetGlobalTime(SavedTime);
		}

		SequencerPin->ExitSilentMode();
	}
}


void FCinematicShotTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	MenuBuilder.BeginSection("Import/Export", NSLOCTEXT("Sequencer", "ImportExportMenuSectionName", "Import/Export"));

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "ImportEDL", "Import EDL..." ),
		NSLOCTEXT( "Sequencer", "ImportEDLTooltip", "Import Edit Decision List (EDL) for non-linear editors." ),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::ImportEDL )));

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "ExportEDL", "Export EDL..." ),
		NSLOCTEXT( "Sequencer", "ExportEDLTooltip", "Export Edit Decision List (EDL) for non-linear editors." ),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::ExportEDL )));

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "ImportFCPXML", "Import Final Cut Pro 7 XML..."),
		NSLOCTEXT("Sequencer", "ImportFCPXMLTooltip", "Import Final Cut Pro 7 XML file for non-linear editors."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::ImportFCPXML )));

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("Sequencer", "ExportFCPXML", "Export Final Cut Pro 7 XML..."),
		NSLOCTEXT("Sequencer", "ExportFCPXMLTooltip", "Export Final Cut Pro 7 XML file for non-linear editors."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::ExportFCPXML )));

	MenuBuilder.EndSection();
}


const FSlateBrush* FCinematicShotTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.CinematicShot");
}

bool FCinematicShotTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid)
{
	if (!Track->IsA(UMovieSceneCinematicShotTrack::StaticClass()))
	{
		return false;
	}

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>() )
	{
		return false;
	}
	
	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );

	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
		if (Cast<UMovieSceneSequence>(AssetData.GetAsset()))
		{
			return true;
		}
	}

	return false;
}


FReply FCinematicShotTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid)
{
	if (!Track->IsA(UMovieSceneCinematicShotTrack::StaticClass()))
	{
		return FReply::Unhandled();
	}

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>() )
	{
		return FReply::Unhandled();
	}
	
	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );
	
	bool bAnyDropped = false;
	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
		UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(AssetData.GetAsset());

		if (Sequence)
		{
			AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FCinematicShotTrackEditor::AddKeyInternal, Sequence, RowIndex) );
			
			bAnyDropped = true;
		}
	}

	return bAnyDropped ? FReply::Handled() : FReply::Unhandled();
}

UMovieSceneSubSection* FCinematicShotTrackEditor::CreateShotInternal(FString& NewShotName, FFrameNumber NewShotStartTime, UMovieSceneCinematicShotSection* ShotToDuplicate)
{
	FString NewShotPath;
	
	if (ShotToDuplicate != nullptr)
	{
		// If duplicating a shot, use that shot's path
		NewShotPath = FPaths::GetPath(ShotToDuplicate->GetSequence()->GetPathName());
	}
	else
	{
		NewShotPath = MovieSceneToolHelpers::GenerateNewShotPath(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), NewShotName);
	}

	// Create a new level sequence asset with the appropriate name
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UObject* NewAsset = nullptr;
	for (TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* CurrentClass = *It;
		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass())
			{
				if (ShotToDuplicate != nullptr)
				{
					NewAsset = AssetTools.DuplicateAssetWithDialog(NewShotName, NewShotPath, ShotToDuplicate->GetSequence());
				}
				else
				{
					NewAsset = AssetTools.CreateAssetWithDialog(NewShotName, NewShotPath, ULevelSequence::StaticClass(), Factory);
				}
				break;
			}
		}
	}

	if (NewAsset == nullptr)
	{
		return nullptr;
	}

	UMovieSceneSequence* NewSequence = Cast<UMovieSceneSequence>(NewAsset);

	int32 Duration = MovieScene::DiscreteSize(ShotToDuplicate ? ShotToDuplicate->GetRange() : NewSequence->GetMovieScene()->GetPlaybackRange());

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();

	// Create a cinematic shot section. 
	UMovieSceneSubSection* NewSection = CinematicShotTrack->AddSequence(NewSequence, NewShotStartTime, Duration);
	return NewSection;
}

void FCinematicShotTrackEditor::InsertShot()
{
	const FScopedTransaction Transaction(LOCTEXT("InsertShot_Transaction", "Insert Shot"));

	FFrameTime NewShotStartTime = GetSequencer()->GetLocalTime().Time;

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
	FString NewShotName = MovieSceneToolHelpers::GenerateNewShotName(CinematicShotTrack->GetAllSections(), NewShotStartTime.FrameNumber);

	UMovieSceneSubSection* NewShot = CreateShotInternal(NewShotName, NewShotStartTime.FrameNumber);
	if (NewShot)
	{
		NewShot->SetRowIndex(MovieSceneToolHelpers::FindAvailableRowIndex(CinematicShotTrack, NewShot));
	}

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	GetSequencer()->EmptySelection();
	GetSequencer()->SelectSection(NewShot);
	GetSequencer()->ThrobSectionSelection();
}


void FCinematicShotTrackEditor::InsertFiller()
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	const FScopedTransaction Transaction(LOCTEXT("InsertFiller_Transaction", "Insert Filler"));

	FQualifiedFrameTime CurrentTime = GetSequencer()->GetLocalTime();

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();

	int32 Duration = (ProjectSettings->DefaultDuration * CurrentTime.Rate).FrameNumber.Value;

	UMovieSceneSequence* NullSequence = nullptr;

	UMovieSceneSubSection* NewSection = CinematicShotTrack->AddSequence(NullSequence, CurrentTime.Time.FrameNumber, Duration);

	UMovieSceneCinematicShotSection* NewCinematicShotSection = Cast<UMovieSceneCinematicShotSection>(NewSection);

	NewCinematicShotSection->SetShotDisplayName(FText(LOCTEXT("Filler", "Filler")).ToString());

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	GetSequencer()->EmptySelection();
	GetSequencer()->SelectSection(NewSection);
	GetSequencer()->ThrobSectionSelection();
}


void FCinematicShotTrackEditor::DuplicateShot(UMovieSceneCinematicShotSection* Section)
{
	const FScopedTransaction Transaction(LOCTEXT("DuplicateShot_Transaction", "Duplicate Shot"));

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();

	FFrameNumber StartTime = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : 0;
	FString NewShotName = MovieSceneToolHelpers::GenerateNewShotName(CinematicShotTrack->GetAllSections(), StartTime);

	// Duplicate the shot and put it on the next available row
	UMovieSceneSubSection* NewShot = CreateShotInternal(NewShotName, StartTime, Section);
	if (NewShot)
	{
		NewShot->SetRange(Section->GetRange());
		NewShot->SetRowIndex(MovieSceneToolHelpers::FindAvailableRowIndex(CinematicShotTrack, NewShot));
		NewShot->Parameters.StartFrameOffset = Section->Parameters.StartFrameOffset;
		NewShot->Parameters.TimeScale = Section->Parameters.TimeScale;
		NewShot->SetPreRollFrames(Section->GetPreRollFrames());

		GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
		GetSequencer()->EmptySelection();
		GetSequencer()->SelectSection(NewShot);
		GetSequencer()->ThrobSectionSelection();
	}
}


void FCinematicShotTrackEditor::RenderShot(UMovieSceneCinematicShotSection* Section)
{
	GetSequencer()->RenderMovie(Section);

}


void FCinematicShotTrackEditor::RenameShot(UMovieSceneCinematicShotSection* Section)
{
	//@todo
}


void FCinematicShotTrackEditor::NewTake(UMovieSceneCinematicShotSection* Section)
{
	const FScopedTransaction Transaction(LOCTEXT("NewTake_Transaction", "New Take"));

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	uint32 TakeNumber = INDEX_NONE;
	if (MovieSceneToolHelpers::ParseShotName(Section->GetShotDisplayName(), ShotPrefix, ShotNumber, TakeNumber))
	{
		TArray<uint32> TakeNumbers;
		uint32 CurrentTakeNumber;
		MovieSceneToolHelpers::GatherTakes(Section, TakeNumbers, CurrentTakeNumber);
		uint32 NewTakeNumber = CurrentTakeNumber;
		if (TakeNumbers.Num() > 0)
		{
			NewTakeNumber = TakeNumbers[TakeNumbers.Num()-1] + 1;
		}

		FString NewShotName = MovieSceneToolHelpers::ComposeShotName(ShotPrefix, ShotNumber, NewTakeNumber);

		TRange<FFrameNumber> NewShotRange         = Section->GetRange();
		FFrameNumber         NewShotStartOffset   = Section->Parameters.StartFrameOffset;
		float                NewShotTimeScale     = Section->Parameters.TimeScale;
		int32                NewShotPrerollFrames = Section->GetPreRollFrames();
		int32                NewRowIndex          = Section->GetRowIndex();
		FFrameNumber         NewShotStartTime     = NewShotRange.GetLowerBound().IsClosed() ? MovieScene::DiscreteInclusiveLower(NewShotRange) : 0;

		UMovieSceneSubSection* NewShot = CreateShotInternal(NewShotName, NewShotStartTime, Section);

		if (NewShot)
		{
			UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
			CinematicShotTrack->RemoveSection(*Section);

			NewShot->SetRange(NewShotRange);
			NewShot->Parameters.StartFrameOffset = NewShotStartOffset;
			NewShot->Parameters.TimeScale = NewShotTimeScale;
			NewShot->SetPreRollFrames(NewShotPrerollFrames);
			NewShot->SetRowIndex(NewRowIndex);

			GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewShot);
			GetSequencer()->ThrobSectionSelection();
		}
	}
}


void FCinematicShotTrackEditor::SwitchTake(uint32 TakeNumber)
{
	bool bSwitchedTake = false;

	const FScopedTransaction Transaction(LOCTEXT("SwitchTake_Transaction", "Switch Take"));

	TArray<UMovieSceneSection*> Sections;
	GetSequencer()->GetSelectedSections(Sections);

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		if (!Sections[SectionIndex]->IsA<UMovieSceneSubSection>())
		{
			continue;
		}

		UMovieSceneSubSection* Section = Cast<UMovieSceneSubSection>(Sections[SectionIndex]);

		UObject* TakeObject = MovieSceneToolHelpers::GetTake(Section, TakeNumber);

		if (TakeObject && TakeObject->IsA(UMovieSceneSequence::StaticClass()))
		{
			UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(TakeObject);
			
			UMovieSceneCinematicShotTrack* CinematicShotTrack = CastChecked<UMovieSceneCinematicShotTrack>(Section->GetOuter());

			TRange<FFrameNumber> NewShotRange         = Section->GetRange();
			FFrameNumber         NewShotStartOffset   = Section->Parameters.StartFrameOffset;
			float                NewShotTimeScale     = Section->Parameters.TimeScale;
			int32                NewShotPrerollFrames = Section->GetPreRollFrames();
			int32                NewRowIndex          = Section->GetRowIndex();
			FFrameNumber         NewShotStartTime     = NewShotRange.GetLowerBound().IsClosed() ? MovieScene::DiscreteInclusiveLower(NewShotRange) : 0;
			int32                NewShotRowIndex      = Section->GetRowIndex();

			const int32 Duration = (NewShotRange.GetLowerBound().IsClosed() && NewShotRange.GetUpperBound().IsClosed() ) ? MovieScene::DiscreteSize(NewShotRange) : 1;
			UMovieSceneSubSection* NewShot = CinematicShotTrack->AddSequence(MovieSceneSequence, NewShotStartTime, Duration);

			if (NewShot != nullptr)
			{
				CinematicShotTrack->RemoveSection(*Section);

				NewShot->SetRange(NewShotRange);
				NewShot->Parameters.StartFrameOffset = NewShotStartOffset;
				NewShot->Parameters.TimeScale = NewShotTimeScale;
				NewShot->SetPreRollFrames(NewShotPrerollFrames);
				NewShot->SetRowIndex(NewShotRowIndex);
				bSwitchedTake = true;
			}
		}
	}

	if (bSwitchedTake)
	{
		GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
	}
}


/* FCinematicShotTrackEditor callbacks
 *****************************************************************************/

bool FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>() == nullptr));
}


void FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryExecute()
{
	UMovieSceneCinematicShotTrack* ShotTrack = FindOrCreateCinematicShotTrack();
	if (ShotTrack)
	{
		if (GetSequencer().IsValid())
		{
			GetSequencer()->OnAddTrack(ShotTrack);
		}
		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}


TSharedRef<SWidget> FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonGetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertShot", "Insert Shot"),
			LOCTEXT("InsertShotTooltip", "Insert new shot at current time"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FCinematicShotTrackEditor::InsertShot))
	);

	MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertFiller", "Insert Filler"),
			LOCTEXT("InsertFillerTooltip", "Insert filler at current time"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FCinematicShotTrackEditor::InsertFiller))
	);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonMenuEntryExecute);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw( this, &FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonMenuEntryEnterPressed);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
		AssetPickerConfig.Filter.ClassNames.Add(TEXT("LevelSequence"));
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);

	return MenuBuilder.MakeWidget();
}


void FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonMenuEntryExecute(const FAssetData& AssetData)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UMovieSceneSequence::StaticClass()))
	{
		UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(AssetData.GetAsset());

		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FCinematicShotTrackEditor::AddKeyInternal, MovieSceneSequence, RowIndex) );
	}
}

void FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonMenuEntryEnterPressed(const TArray<FAssetData>& AssetData)
{
	if (AssetData.Num() > 0)
	{
		HandleAddCinematicShotComboButtonMenuEntryExecute(AssetData[0].GetAsset());
	}
}

FKeyPropertyResult FCinematicShotTrackEditor::AddKeyInternal(FFrameNumber KeyTime, UMovieSceneSequence* InMovieSceneSequence, int32 RowIndex)
{	
	FKeyPropertyResult KeyPropertyResult;

	if (CanAddSubSequence(*InMovieSceneSequence))
	{
		UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
		
		const FFrameRate TickResolution = InMovieSceneSequence->GetMovieScene()->GetTickResolution();
		const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
			MovieScene::DiscreteSize(InMovieSceneSequence->GetMovieScene()->GetPlaybackRange()),
			TickResolution);

		const FFrameRate OuterFrameRate = CinematicShotTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
		const int32      OuterDuration  = InnerDuration.ConvertTo(OuterFrameRate).FrameNumber.Value;

		UMovieSceneSubSection* NewSection = CinematicShotTrack->AddSequenceOnRow(InMovieSceneSequence, KeyTime, OuterDuration, RowIndex);
		KeyPropertyResult.bTrackModified = true;

		GetSequencer()->EmptySelection();
		GetSequencer()->SelectSection(NewSection);
		GetSequencer()->ThrobSectionSelection();

		if (TickResolution != OuterFrameRate)
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("TickResolutionMismatch", "The parent sequence has a different tick resolution {0} than the newly added sequence {1}"), OuterFrameRate.ToPrettyText(), TickResolution.ToPrettyText()));
			Info.bUseLargeFont = false;
			FSlateNotificationManager::Get().AddNotification(Info);
		}

		return KeyPropertyResult;
	}

	FNotificationInfo Info(FText::Format( LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), InMovieSceneSequence->GetDisplayName()));	
	Info.bUseLargeFont = false;
	FSlateNotificationManager::Get().AddNotification(Info);

	return KeyPropertyResult;
}


UMovieSceneCinematicShotTrack* FCinematicShotTrackEditor::FindOrCreateCinematicShotTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	
	if (FocusedMovieScene == nullptr)
	{
		return nullptr;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return nullptr;
	}

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FocusedMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (CinematicShotTrack != nullptr)
	{
		return CinematicShotTrack;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddCinematicShotTrack_Transaction", "Add Cinematic Shot Track"));
	FocusedMovieScene->Modify();

	auto NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
	ensure(NewTrack);

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );

	return NewTrack;
}


ECheckBoxState FCinematicShotTrackEditor::AreShotsLocked() const
{
	if (GetSequencer()->IsPerspectiveViewportCameraCutEnabled())
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FCinematicShotTrackEditor::OnLockShotsClicked(ECheckBoxState CheckBoxState)
{
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		for( FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients() )
		{
			if (LevelVC && LevelVC->AllowsCinematicControl() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(true);
	}
	else
	{
		GetSequencer()->UpdateCameraCut(nullptr, nullptr);
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
	}

	GetSequencer()->ForceEvaluate();
}


FText FCinematicShotTrackEditor::GetLockShotsToolTip() const
{
	return AreShotsLocked() == ECheckBoxState::Checked ?
		LOCTEXT("UnlockShots", "Unlock Viewport from Shots") :
		LOCTEXT("LockShots", "Lock Viewport to Shots");
}


bool FCinematicShotTrackEditor::CanAddSubSequence(const UMovieSceneSequence& Sequence) const
{
	// prevent adding ourselves and ensure we have a valid movie scene
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();

	if ((FocusedSequence == nullptr) || (FocusedSequence == &Sequence) || (FocusedSequence->GetMovieScene() == nullptr))
	{
		return false;
	}

	// ensure that the other sequence has a valid movie scene
	UMovieScene* SequenceMovieScene = Sequence.GetMovieScene();

	if (SequenceMovieScene == nullptr)
	{
		return false;
	}

	// make sure we are not contained in the other sequence (circular dependency)
	// @todo sequencer: this check is not sufficient (does not prevent circular dependencies of 2+ levels)
	UMovieSceneSubTrack* SequenceSubTrack = SequenceMovieScene->FindMasterTrack<UMovieSceneSubTrack>();
	if (SequenceSubTrack && SequenceSubTrack->ContainsSequence(*FocusedSequence, true))
	{
		return false;
	}

	UMovieSceneCinematicShotTrack* SequenceCinematicTrack = SequenceMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (SequenceCinematicTrack && SequenceCinematicTrack->ContainsSequence(*FocusedSequence, true))
	{
		return false;
	}

	return true;
}


void FCinematicShotTrackEditor::OnUpdateCameraCut(UObject* CameraObject, bool bJumpCut)
{
	// Keep track of the camera when it switches so that the thumbnail can be drawn with the correct camera
	CinematicShotCamera = Cast<AActor>(CameraObject);
}


FKeyPropertyResult FCinematicShotTrackEditor::HandleSequenceAdded(FFrameNumber KeyTime, UMovieSceneSequence* Sequence, int32 RowIndex)
{
	FKeyPropertyResult KeyPropertyResult;

	auto CinematicShotTrack = FindOrCreateCinematicShotTrack();

	const FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();
	const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
		MovieScene::DiscreteSize(Sequence->GetMovieScene()->GetPlaybackRange()),
		TickResolution);

	const FFrameRate OuterFrameRate = CinematicShotTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const int32      OuterDuration  = InnerDuration.ConvertTo(OuterFrameRate).FrameNumber.Value;

	UMovieSceneSubSection* NewSection = CinematicShotTrack->AddSequenceOnRow(Sequence, KeyTime, OuterDuration, RowIndex);
	KeyPropertyResult.bTrackModified = true;

	GetSequencer()->EmptySelection();
	GetSequencer()->SelectSection(NewSection);
	GetSequencer()->ThrobSectionSelection();

	if (TickResolution != OuterFrameRate)
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("TickResolutionMismatch", "The parent sequence has a different tick resolution {0} than the newly added sequence {1}"), OuterFrameRate.ToPrettyText(), TickResolution.ToPrettyText()));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	return KeyPropertyResult;
}

UAutomatedLevelSequenceCapture* GetMovieSceneCapture()
{
	UAutomatedLevelSequenceCapture* MovieSceneCapture = Cast<UAutomatedLevelSequenceCapture>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	if (!MovieSceneCapture)
	{
		MovieSceneCapture = FindObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), *UAutomatedLevelSequenceCapture::AutomatedLevelSequenceCaptureUIName.ToString());
	}
	
	if (!MovieSceneCapture)
	{
		MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), UMovieSceneCapture::MovieSceneCaptureUIName, RF_Transient);
		MovieSceneCapture->LoadFromConfig();
	}

	return MovieSceneCapture;
}

void FCinematicShotTrackEditor::ImportEDL()
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return;
	}

	UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	UAutomatedLevelSequenceCapture* MovieSceneCapture = GetMovieSceneCapture();
	if (!MovieSceneCapture)
	{
		return;
	}

	const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();
	FString SaveDirectory = FPaths::ConvertRelativePathToFull(Settings.OutputDirectory.Path);

	if (MovieSceneToolHelpers::ShowImportEDLDialog(MovieScene, MovieScene->GetDisplayRate(), SaveDirectory))
	{
		GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
	}
}


void FCinematicShotTrackEditor::ExportEDL()
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return;
	}

	UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}
		
	UAutomatedLevelSequenceCapture* MovieSceneCapture = GetMovieSceneCapture();
	if (!MovieSceneCapture)
	{
		return;
	}

	const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();
	FString SaveDirectory = FPaths::ConvertRelativePathToFull(Settings.OutputDirectory.Path);
	int32 HandleFrames = Settings.HandleFrames;
	FString MovieExtension = Settings.MovieExtension;

	MovieSceneToolHelpers::ShowExportEDLDialog(MovieScene, MovieScene->GetDisplayRate(), SaveDirectory, HandleFrames, MovieExtension);
}


void FCinematicShotTrackEditor::ImportFCPXML()
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return;
	}

	UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	UAutomatedLevelSequenceCapture* MovieSceneCapture = GetMovieSceneCapture();
	if (!MovieSceneCapture)
	{
		return;
	}

	const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();
	FString SaveDirectory = FPaths::ConvertRelativePathToFull(Settings.OutputDirectory.Path);

	FFCPXMLImporter *Importer = new FFCPXMLImporter;

	if (MovieSceneToolHelpers::MovieSceneTranslatorImport(Importer, MovieScene, MovieScene->GetDisplayRate(), SaveDirectory))
	{
		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	delete Importer;
}


void FCinematicShotTrackEditor::ExportFCPXML()
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return;
	}

	const UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	UAutomatedLevelSequenceCapture* MovieSceneCapture = GetMovieSceneCapture();
	if (!MovieSceneCapture)
	{
		return;
	}

	const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();

	FFCPXMLExporter *Exporter = new FFCPXMLExporter;

	MovieSceneToolHelpers::MovieSceneTranslatorExport(Exporter, MovieScene, Settings);

	delete Exporter;
}




#undef LOCTEXT_NAMESPACE

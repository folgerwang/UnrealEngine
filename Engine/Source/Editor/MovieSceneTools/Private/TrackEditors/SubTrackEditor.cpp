// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SubTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EditorStyleSet.h"
#include "GameFramework/PlayerController.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#include "SequencerSectionPainter.h"
#include "ISequenceRecorder.h"
#include "SequenceRecorderSettings.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "MovieSceneToolHelpers.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeHelpers.h"

namespace SubTrackEditorConstants
{
	const float TrackHeight = 50.0f;
}


#define LOCTEXT_NAMESPACE "FSubTrackEditor"


/**
 * A generic implementation for displaying simple property sections.
 */
class FSubSection
	: public ISequencerSection
{
public:

	FSubSection(TSharedPtr<ISequencer> InSequencer, UMovieSceneSection& InSection, const FText& InDisplayName, TSharedPtr<FSubTrackEditor> InSubTrackEditor)
		: DisplayName(InDisplayName)
		, SectionObject(*CastChecked<UMovieSceneSubSection>(&InSection))
		, Sequencer(InSequencer)
		, SubTrackEditor(InSubTrackEditor)
		, InitialStartOffsetDuringResize(0)
		, InitialStartTimeDuringResize(0)
	{
	}

public:

	// ISequencerSection interface

	virtual float GetSectionHeight() const override
	{
		return SubTrackEditorConstants::TrackHeight;
	}

	virtual UMovieSceneSection* GetSectionObject() override
	{
		return &SectionObject;
	}

	virtual FText GetSectionTitle() const override
	{
		if(SectionObject.GetSequence())
		{
			return FText::FromString(SectionObject.GetSequence()->GetName());
		}
		else if(UMovieSceneSubSection::GetRecordingSection() == &SectionObject)
		{
			AActor* ActorToRecord = UMovieSceneSubSection::GetActorToRecord();

			ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			if(SequenceRecorder.IsRecording())
			{
				if(ActorToRecord != nullptr)
				{
					return FText::Format(LOCTEXT("RecordingIndicatorWithActor", "Sequence Recording for \"{0}\""), FText::FromString(ActorToRecord->GetActorLabel()));
				}
				else
				{
					return LOCTEXT("RecordingIndicator", "Sequence Recording");
				}
			}
			else
			{
				if(ActorToRecord != nullptr)
				{
					return FText::Format(LOCTEXT("RecordingPendingIndicatorWithActor", "Sequence Recording Pending for \"{0}\""), FText::FromString(ActorToRecord->GetActorLabel()));
				}
				else
				{
					return LOCTEXT("RecordingPendingIndicator", "Sequence Recording Pending");
				}
			}
		}
		else
		{
			return LOCTEXT("NoSequenceSelected", "No Sequence Selected");
		}
	}
	
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override
	{
		int32 LayerId = InPainter.PaintSectionBackground();

		ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		TRange<FFrameNumber> SectionRange = SectionObject.GetRange();
		if (SectionRange.GetLowerBound().IsOpen() || SectionRange.GetUpperBound().IsOpen())
		{
			return InPainter.LayerId;
		}

		const FFrameNumber SectionStartFrame = SectionObject.GetInclusiveStartFrame();
		const FFrameNumber SectionEndFrame   = SectionObject.GetExclusiveEndFrame();
		const int32        SectionSize       = MovieScene::DiscreteSize(SectionRange);

		if (SectionSize <= 0)
		{
			return InPainter.LayerId;
		}

		const float        PixelsPerFrame    = InPainter.SectionGeometry.Size.X / float(SectionSize);

		UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();
		if (InnerSequence)
		{
			UMovieScene*         MovieScene    = InnerSequence->GetMovieScene();
			TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

			FMovieSceneSequenceTransform InnerToOuterTransform = SectionObject.OuterToInnerTransform().Inverse();

			const FFrameNumber PlaybackStart = (MovieScene::DiscreteInclusiveLower(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
			if (SectionRange.Contains(PlaybackStart))
			{
				const int32 StartOffset = (PlaybackStart-SectionStartFrame).Value;
				// add dark tint for left out-of-bounds
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(0.0f, 0.f),
						FVector2D(StartOffset * PixelsPerFrame, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FLinearColor::Black.CopyWithNewOpacity(0.5f)
				);

				// add green line for playback start
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(StartOffset * PixelsPerFrame, 0.f),
						FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FColor(32, 128, 32)	// 120, 75, 50 (HSV)
				);
			}

			const FFrameNumber PlaybackEnd = (MovieScene::DiscreteExclusiveUpper(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
			if (SectionRange.Contains(PlaybackEnd))
			{
				// add dark tint for right out-of-bounds
				const int32 EndOffset = (PlaybackEnd-SectionStartFrame).Value;
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(EndOffset * PixelsPerFrame, 0.f),
						FVector2D((SectionSize - EndOffset) * PixelsPerFrame, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FLinearColor::Black.CopyWithNewOpacity(0.5f)
				);


				// add red line for playback end
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2D(EndOffset * PixelsPerFrame, 0.f),
						FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
					),
					FEditorStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FColor(128, 32, 32)	// 0, 75, 50 (HSV)
				);
			}

			FMargin ContentPadding = GetContentPadding();

			int32 NumTracks = MovieScene->GetPossessableCount() + MovieScene->GetSpawnableCount() + MovieScene->GetMasterTracks().Num();

			FVector2D TopLeft = InPainter.SectionGeometry.AbsoluteToLocal(InPainter.SectionClippingRect.GetTopLeft()) + FVector2D(1.f, -1.f);

			FSlateFontInfo FontInfo = FEditorStyle::GetFontStyle("NormalFont");

			TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

			auto GetFontHeight = [&]
			{
				return FontCache->GetMaxCharacterHeight(FontInfo, 1.f) + FontCache->GetBaseline(FontInfo, 1.f);
			};
			while (GetFontHeight() > InPainter.SectionGeometry.Size.Y && FontInfo.Size > 11)
			{
				FontInfo.Size = FMath::Max(FMath::FloorToInt(FontInfo.Size - 6.f), 11);
			}

			FSlateDrawElement::MakeText(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.MakeChild(
					FVector2D(InPainter.SectionGeometry.Size.X, GetFontHeight()),
					FSlateLayoutTransform(TopLeft + FVector2D(ContentPadding.Left, ContentPadding.Top) + FVector2D(11.f, GetFontHeight()*2.f))
				).ToPaintGeometry(),
				FText::Format(LOCTEXT("NumTracksFormat", "{0} track(s)"), FText::AsNumber(NumTracks)),
				FontInfo,
				DrawEffects,
				FColor(200, 200, 200)
			);
		}
		else if (UMovieSceneSubSection::GetRecordingSection() == &SectionObject)
		{
			FColor SubSectionColor = FColor(180, 75, 75, 190);
	
			ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			if(SequenceRecorder.IsRecording())
			{
				SubSectionColor = FColor(200, 10, 10, 190);
			}

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(0.f, 0.f),
					InPainter.SectionGeometry.Size
				),
				FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint"),
				DrawEffects,
				SubSectionColor
			);

			// display where we will create the recording
			FString Path = SectionObject.GetTargetPathToRecordTo() / SectionObject.GetTargetSequenceName();
			if (Path.Len() > 0)
			{
				FSlateDrawElement::MakeText(
					InPainter.DrawElements,
					++LayerId,
					InPainter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(11.0f, 32.0f)),
					FText::Format(LOCTEXT("RecordingDestination", "Target: \"{0}\""), FText::FromString(Path)),
					FEditorStyle::GetFontStyle("NormalFont"),
					DrawEffects,
					FColor(200, 200, 200)
				);
			}
		}

		return LayerId;
	}

	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent) override
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			if (SectionObject.GetSequence())
			{
				Sequencer.Pin()->FocusSequenceInstance(SectionObject);
			}
		}

		return FReply::Handled();
	}

	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
	{
		ISequencerSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

		MenuBuilder.AddSubMenu(
			LOCTEXT("TakesMenu", "Takes"),
			LOCTEXT("TakesMenuTooltip", "Sub section takes"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& InMenuBuilder){ AddTakesMenu(InMenuBuilder); }));
	}

	void BeginResizeSection()
	{
		InitialStartOffsetDuringResize = SectionObject.Parameters.StartFrameOffset;
		InitialStartTimeDuringResize = SectionObject.HasStartFrame() ? SectionObject.GetInclusiveStartFrame() : 0;
	}

	void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
	{
		UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();

		// Adjust the start offset when resizing from the beginning
		if (ResizeMode == SSRM_LeadingEdge && InnerSequence)
		{
			const FFrameRate    OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
			const FFrameRate    InnerFrameRate   = InnerSequence->GetMovieScene()->GetTickResolution();
			const FFrameNumber  ResizeDifference = ResizeTime - InitialStartTimeDuringResize;
			const FFrameTime    InnerFrameTime   = ConvertFrameTime(ResizeDifference, OuterFrameRate, InnerFrameRate);
			FFrameNumber		NewStartOffset   = FFrameTime::FromDecimal(InnerFrameTime.AsDecimal() * SectionObject.Parameters.TimeScale).FrameNumber;

			NewStartOffset += InitialStartOffsetDuringResize;

			// Ensure start offset is not less than 0
			if (NewStartOffset < 0)
			{
				FFrameTime OuterFrameTimeOver = ConvertFrameTime(FFrameTime::FromDecimal(NewStartOffset.Value/SectionObject.Parameters.TimeScale), InnerFrameRate, OuterFrameRate);
				ResizeTime = ResizeTime - OuterFrameTimeOver.GetFrame(); 
				NewStartOffset = 0;
			}

			SectionObject.Parameters.StartFrameOffset = FFrameNumber(NewStartOffset);
		}

		ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
	}

	virtual void BeginSlipSection() override
	{
		InitialStartOffsetDuringResize = SectionObject.Parameters.StartFrameOffset;
		InitialStartTimeDuringResize = SectionObject.HasStartFrame() ? SectionObject.GetInclusiveStartFrame() : 0;
	}

	virtual void SlipSection(FFrameNumber SlipTime) override
	{
		UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();

		// Adjust the start offset when resizing from the beginning
		if (InnerSequence)
		{
			const FFrameRate    OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
			const FFrameRate    InnerFrameRate   = InnerSequence->GetMovieScene()->GetTickResolution();
			const FFrameNumber  ResizeDifference = SlipTime - InitialStartTimeDuringResize;
			const FFrameTime    InnerFrameTime = ConvertFrameTime(ResizeDifference, OuterFrameRate, InnerFrameRate);
			const int32         NewStartOffset = FFrameTime::FromDecimal(InnerFrameTime.AsDecimal() * SectionObject.Parameters.TimeScale).FrameNumber.Value;

			// Ensure start offset is not less than 0
			SectionObject.Parameters.StartFrameOffset = FFrameNumber(FMath::Max(NewStartOffset, 0));
		}

		ISequencerSection::SlipSection(SlipTime);
	}

	virtual bool IsReadOnly() const override
	{
		// Overridden to false regardless of movie scene section read only state so that we can double click into the sub section
		return false;
	}

private:

	void AddTakesMenu(FMenuBuilder& MenuBuilder)
	{
		TArray<uint32> TakeNumbers;
		uint32 CurrentTakeNumber = INDEX_NONE;
		MovieSceneToolHelpers::GatherTakes(&SectionObject, TakeNumbers, CurrentTakeNumber);

		for (auto TakeNumber : TakeNumbers)
		{
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("TakeNumber", "Take {0}"), FText::AsNumber(TakeNumber)),
				FText::Format(LOCTEXT("TakeNumberTooltip", "Switch to take {0}"), FText::AsNumber(TakeNumber)),
				TakeNumber == CurrentTakeNumber ? FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Star") : FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Empty"),
				FUIAction(FExecuteAction::CreateSP(SubTrackEditor.Pin().ToSharedRef(), &FSubTrackEditor::SwitchTake, TakeNumber))
			);
		}
	}

private:

	/** Display name of the section */
	FText DisplayName;

	/** The section we are visualizing */
	UMovieSceneSubSection& SectionObject;

	/** Sequencer interface */
	TWeakPtr<ISequencer> Sequencer;

	/** The sub track editor that contains this section */
	TWeakPtr<FSubTrackEditor> SubTrackEditor;

	/** Cached start offset value valid only during resize */
	FFrameNumber InitialStartOffsetDuringResize;

	/** Cached start time valid only during resize */
	FFrameNumber InitialStartTimeDuringResize;
};


/* FSubTrackEditor structors
 *****************************************************************************/

FSubTrackEditor::FSubTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{ }


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FSubTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddSubTrack", "Subscenes Track"),
		LOCTEXT("AddSubTooltip", "Adds a new track that can contain other sequences."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Sub"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSubTrackEditor::HandleAddSubTrackMenuEntryExecute)
		)
	);
}

TSharedPtr<SWidget> FSubTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the sub sequence combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("SubText", "Sequence"), FOnGetContent::CreateSP(this, &FSubTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent, Track), Params.NodeIsHovered, GetSequencer())
	];
}


TSharedRef<ISequencerTrackEditor> FSubTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FSubTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FSubTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShareable(new FSubSection(GetSequencer(), SectionObject, Track.GetDisplayName(), SharedThis(this)));
}


bool FSubTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
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

	//@todo If there's already a cinematic shot track, allow that track to handle this asset
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene != nullptr && FocusedMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>() != nullptr)
	{
		return false;
	}

	if (CanAddSubSequence(*Sequence))
	{
		const FScopedTransaction Transaction(LOCTEXT("AddSubScene_Transaction", "Add Subscene"));

		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSubTrackEditor::HandleSequenceAdded, Sequence, RowIndex));

		return true;
	}

	FNotificationInfo Info(FText::Format( LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), Sequence->GetDisplayName()));	
	Info.bUseLargeFont = false;
	FSlateNotificationManager::Get().AddNotification(Info);

	return false;
}

bool FSubTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FSubTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	// We support sub movie scenes
	return Type == UMovieSceneSubTrack::StaticClass();
}

const FSlateBrush* FSubTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.Sub");
}


bool FSubTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid)
{
	if (!Track->IsA(UMovieSceneSubTrack::StaticClass()) || Track->IsA(UMovieSceneCinematicShotTrack::StaticClass()))
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


FReply FSubTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid)
{
	if (!Track->IsA(UMovieSceneSubTrack::StaticClass()) || Track->IsA(UMovieSceneCinematicShotTrack::StaticClass()))
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
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSubTrackEditor::HandleSequenceAdded, Sequence, RowIndex));

			bAnyDropped = true;
		}
	}

	return bAnyDropped ? FReply::Handled() : FReply::Unhandled();
}

/* FSubTrackEditor callbacks
 *****************************************************************************/

bool FSubTrackEditor::CanAddSubSequence(const UMovieSceneSequence& Sequence) const
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


/* FSubTrackEditor callbacks
 *****************************************************************************/

void FSubTrackEditor::HandleAddSubTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddSubTrack_Transaction", "Add Sub Track"));
	FocusedMovieScene->Modify();

	auto NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneSubTrack>();
	ensure(NewTrack);

	if (GetSequencer().IsValid())
	{
		GetSequencer()->OnAddTrack(NewTrack);
	}
	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

/** Helper function - get the first PIE world (or first PIE client world if there is more than one) */
static UWorld* GetFirstPIEWorld()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World()->IsPlayInEditor())
		{
			if(Context.World()->GetNetMode() == ENetMode::NM_Standalone ||
				(Context.World()->GetNetMode() == ENetMode::NM_Client && Context.PIEInstance == 2))
			{
				return Context.World();
			}
		}
	}

	return nullptr;
}

TSharedRef<SWidget> FSubTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent(UMovieSceneTrack* InTrack)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(TEXT("RecordSequence"), LOCTEXT("RecordSequence", "Record Sequence"));
	{
		AActor* ActorToRecord = nullptr;
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RecordNewSequence", "Record New Sequence"), 
			LOCTEXT("RecordNewSequence_ToolTip", "Record a new level sequence into this sub-track from gameplay/simulation etc.\nThis only primes the track for recording. Click the record button to begin recording into this track once primed.\nOnly one sequence can be recorded at a time."), 
			FSlateIcon(), 
			FUIAction(
				FExecuteAction::CreateSP(this, &FSubTrackEditor::HandleRecordNewSequence, ActorToRecord, InTrack),
				FCanExecuteAction::CreateSP(this, &FSubTrackEditor::CanRecordNewSequence)));

		if(UWorld* PIEWorld = GetFirstPIEWorld())
		{
			APlayerController* Controller = GEngine->GetFirstLocalPlayerController(PIEWorld);
			if(Controller && Controller->GetPawn())
			{
				ActorToRecord = Controller->GetPawn();
				MenuBuilder.AddMenuEntry(
					LOCTEXT("RecordNewSequenceFromPlayer", "Record New Sequence From Current Player"), 
					LOCTEXT("RecordNewSequenceFromPlayer_ToolTip", "Record a new level sequence into this sub track using the current player's pawn.\nThis only primes the track for recording. Click the record button to begin recording into this track once primed.\nOnly one sequence can be recorded at a time."), 
					FSlateIcon(), 
					FUIAction(
						FExecuteAction::CreateSP(this, &FSubTrackEditor::HandleRecordNewSequence, ActorToRecord, InTrack),
						FCanExecuteAction::CreateSP(this, &FSubTrackEditor::CanRecordNewSequence)));
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("ChooseSequence"), LOCTEXT("ChooseSequence", "Choose Sequence"));
	{
		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryExecute, InTrack);
			AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw( this, &FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryEnterPressed, InTrack);
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
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryExecute(const FAssetData& AssetData, UMovieSceneTrack* InTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UMovieSceneSequence::StaticClass()))
	{
		UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(AssetData.GetAsset());

		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSubTrackEditor::AddKeyInternal, MovieSceneSequence, InTrack, RowIndex) );
	}
}

void FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* InTrack)
{
	if (AssetData.Num() > 0)
	{
		HandleAddSubSequenceComboButtonMenuEntryExecute(AssetData[0].GetAsset(), InTrack);
	}
}

FKeyPropertyResult FSubTrackEditor::AddKeyInternal(FFrameNumber KeyTime, UMovieSceneSequence* InMovieSceneSequence, UMovieSceneTrack* InTrack, int32 RowIndex)
{	
	FKeyPropertyResult KeyPropertyResult;

	if (CanAddSubSequence(*InMovieSceneSequence))
	{
		UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(InTrack);

		const FFrameRate TickResolution = InMovieSceneSequence->GetMovieScene()->GetTickResolution();
		const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
			MovieScene::DiscreteSize(InMovieSceneSequence->GetMovieScene()->GetPlaybackRange()),
			TickResolution);

		const FFrameRate OuterFrameRate = SubTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
		const int32      OuterDuration  = InnerDuration.ConvertTo(OuterFrameRate).FrameNumber.Value;

		UMovieSceneSubSection* NewSection = SubTrack->AddSequenceOnRow(InMovieSceneSequence, KeyTime, OuterDuration, RowIndex);
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

FKeyPropertyResult FSubTrackEditor::HandleSequenceAdded(FFrameNumber KeyTime, UMovieSceneSequence* Sequence, int32 RowIndex)
{
	FKeyPropertyResult KeyPropertyResult;

	auto SubTrack = FindOrCreateMasterTrack<UMovieSceneSubTrack>().Track;

	const FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();
	const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
		MovieScene::DiscreteSize(Sequence->GetMovieScene()->GetPlaybackRange()),
		TickResolution);

	const FFrameRate OuterFrameRate = SubTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const int32      OuterDuration  = InnerDuration.ConvertTo(OuterFrameRate).FrameNumber.Value;

	UMovieSceneSubSection* NewSection = SubTrack->AddSequenceOnRow(Sequence, KeyTime, OuterDuration, RowIndex);
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

bool FSubTrackEditor::CanRecordNewSequence() const
{
	return !UMovieSceneSubSection::IsSetAsRecording();
}

void FSubTrackEditor::HandleRecordNewSequence(AActor* InActorToRecord, UMovieSceneTrack* InTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	const FScopedTransaction Transaction(LOCTEXT("AddRecordNewSequence_Transaction", "Add Record New Sequence"));

	AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSubTrackEditor::HandleRecordNewSequenceInternal, InActorToRecord, InTrack) );
}

FKeyPropertyResult FSubTrackEditor::HandleRecordNewSequenceInternal(FFrameNumber KeyTime, AActor* InActorToRecord, UMovieSceneTrack* InTrack)
{
	FKeyPropertyResult KeyPropertyResult;

	UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(InTrack);
	UMovieSceneSubSection* Section = SubTrack->AddSequenceToRecord();

	// @todo: we could default to the same directory as a parent sequence, or the last sequence recorded. Lots of options!
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	Section->SetTargetSequenceName(SequenceRecorder.GetSequenceRecordingName());
	Section->SetTargetPathToRecordTo(SequenceRecorder.GetSequenceRecordingBasePath());
	Section->SetActorToRecord(InActorToRecord);
	KeyPropertyResult.bTrackModified = true;

	return KeyPropertyResult;
}

void FSubTrackEditor::SwitchTake(uint32 TakeNumber)
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

			UMovieSceneSubTrack* SubTrack = CastChecked<UMovieSceneSubTrack>(Section->GetOuter());

			TRange<FFrameNumber> NewShotRange         = Section->GetRange();
			FFrameNumber		 NewShotStartOffset   = Section->Parameters.StartFrameOffset;
			float                NewShotTimeScale     = Section->Parameters.TimeScale;
			int32                NewShotPrerollFrames = Section->GetPreRollFrames();
			int32                NewRowIndex          = Section->GetRowIndex();
			FFrameNumber         NewShotStartTime     = NewShotRange.GetLowerBound().IsClosed() ? MovieScene::DiscreteInclusiveLower(NewShotRange) : 0;
			int32                NewShotRowIndex      = Section->GetRowIndex();

			const int32 Duration = (NewShotRange.GetLowerBound().IsClosed() && NewShotRange.GetUpperBound().IsClosed() ) ? MovieScene::DiscreteSize(NewShotRange) : 1;
			UMovieSceneSubSection* NewShot = SubTrack->AddSequence(MovieSceneSequence, NewShotStartTime, Duration);

			if (NewShot != nullptr)
			{
				SubTrack->RemoveSection(*Section);

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

#undef LOCTEXT_NAMESPACE

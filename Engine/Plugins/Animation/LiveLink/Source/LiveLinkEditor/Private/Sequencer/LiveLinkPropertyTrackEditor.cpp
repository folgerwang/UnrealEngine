// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkPropertyTrackEditor.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/QualifiedFrameTime.h"
#include "EditorStyleSet.h" //for add track stuff mz todo remove maybe
#include "Features/IModularFeatures.h"
#include "ISequencerSection.h"

/**
* An implementation of live link property sections.
*/
class FLiveLinkSection : public FSequencerSection
{
public:

	/**
	* Creates a new Live Link section.
	*
	* @param InSection The section object which is being displayed and edited.
	* @param InSequencer The sequencer which is controlling this section.
	*/
	FLiveLinkSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
		: FSequencerSection(InSection), WeakSequencer(InSequencer)
	{
	}

public:
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;

protected:

	/** The sequencer which is controlling this section. */
	TWeakPtr<ISequencer> WeakSequencer;

};


#define LOCTEXT_NAMESPACE "FLiveLinkSection"

//MZ todo will fill out possible for mask support
void FLiveLinkSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding)
{
}


#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "LiveLinkPropertyTrackEditor"


TSharedRef<ISequencerTrackEditor> FLiveLinkPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FLiveLinkPropertyTrackEditor(InSequencer));
}

//MZ todo will fillout possible for mask support
void FLiveLinkPropertyTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
}

/* ISequencerTrackEditor interface
*****************************************************************************/

void FLiveLinkPropertyTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();


	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddLiveLinkTrack", "Live Link Track"),
		LOCTEXT("AddLiveLinkTrackTooltip", "Adds a new track that exposes Live Link Sources."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.LiveLink"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryCanExecute)
		)
	);
}

TSharedRef<ISequencerSection> FLiveLinkPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShared<FLiveLinkSection>(SectionObject, GetSequencer());
}

bool FLiveLinkPropertyTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FLiveLinkPropertyTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneLiveLinkTrack::StaticClass());
}


/* FLiveLinkTrackEditor callbacks
*****************************************************************************/

void FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryExecute()
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	UMovieSceneTrack* Track = MovieScene->FindMasterTrack<UMovieSceneLiveLinkTrack>();

	if (Track != nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddLiveLinkTrack_Transaction", "Add Live Link Track"));
	MovieScene->Modify();
	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

bool FLiveLinkPropertyTrackEditor::HandleAddLiveLinkTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->FindMasterTrack<UMovieSceneLiveLinkTrack>() == nullptr));
}


#undef LOCTEXT_NAMESPACE
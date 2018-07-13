
#include "MovieSceneSequenceEditor.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "Channels/MovieSceneEvent.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventSectionBase.h"
#include "ISequencerModule.h"
#include "ScopedTransaction.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "MovieSceneSequenceEditor"

namespace
{
	FText GetDefaultCommentText()
	{
		FText DefaultComment_01 = LOCTEXT("DefaultComment_01", "Sequencer Events can be bound to either of the following function signatures. Return values are not supported.");
		FText DefaultComment_02 = LOCTEXT("DefaultComment_02", "\t1. A function with no parameters");
		FText DefaultComment_03 = LOCTEXT("DefaultComment_03", "\t\tCompatible with master tracks or object bindings");
		FText DefaultComment_04 = LOCTEXT("DefaultComment_04", "\t2. A function with a single object or interface parameter");
		FText DefaultComment_05 = LOCTEXT("DefaultComment_05", "\t\tCompatible with master tracks or object bindings");
		FText DefaultComment_06 = LOCTEXT("DefaultComment_06", "\t\tWill only trigger if the source object is of the same type as the parameter (or interface)");
		FText DefaultComment_07 = LOCTEXT("DefaultComment_07", "\t\tWill be triggered with objects in the following order:");
		FText DefaultComment_08 = LOCTEXT("DefaultComment_08", "\t\t\ta) Objects bound to the track's object binding, or:");
		FText DefaultComment_09 = LOCTEXT("DefaultComment_09", "\t\t\tb) Objects specified on the track's event receivers array, or:");
		FText DefaultComment_10 = LOCTEXT("DefaultComment_10", "\t\t\tc) Objects provided by the playback context (level blueprints, widgets etc)");
		FText DefaultComment_11 = LOCTEXT("DefaultComment_11", "Tip: Trigger events on level blueprints by implementing an interface on it");

		return FText::Format(LOCTEXT("DefaultComment_Format", "{0}\n{1}\n{2}\n{3}\n{4}\n{5}\n{6}\n{7}\n{8}\n{9}\n\n{10}"),
			DefaultComment_01, DefaultComment_02, DefaultComment_03, DefaultComment_04, DefaultComment_05,
			DefaultComment_06, DefaultComment_07, DefaultComment_08, DefaultComment_09, DefaultComment_10, DefaultComment_11);
	}
}

FName FMovieSceneSequenceEditor::TargetPinName("Target");

FMovieSceneSequenceEditor* FMovieSceneSequenceEditor::Find(UMovieSceneSequence* InSequence)
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	return InSequence ? SequencerModule.FindSequenceEditor(InSequence->GetClass()) : nullptr;
}

bool FMovieSceneSequenceEditor::SupportsEvents(UMovieSceneSequence* InSequence) const
{
	return CanCreateEvents(InSequence);
}

UBlueprint* FMovieSceneSequenceEditor::GetDirectorBlueprint(UMovieSceneSequence* Sequence) const
{
	return GetBlueprintForSequence(Sequence);
}

UBlueprint* FMovieSceneSequenceEditor::AccessDirectorBlueprint(UMovieSceneSequence* Sequence) const
{
	UBlueprint* Blueprint = GetBlueprintForSequence(Sequence);
	if (!Blueprint)
	{
		Blueprint = CreateBlueprintForSequence(Sequence);
	}
	return Blueprint;
}

UK2Node_FunctionEntry* FMovieSceneSequenceEditor::CreateEventEndpoint(UMovieSceneSequence* Sequence, const FString& DesiredName) const
{
	FScopedTransaction Transaction(LOCTEXT("CreateEventEndpoint", "Create Event Endpoint"));

	UBlueprint* Blueprint = AccessDirectorBlueprint(Sequence);
	if (Blueprint)
	{
		static FString DefaultEventName = "SequenceEvent";
		FName UniqueGraphName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, DesiredName.Len() == 0 ? DefaultEventName : DesiredName);

		Blueprint->Modify();

		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, UniqueGraphName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

		const bool bIsUserCreated = false;
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, bIsUserCreated, nullptr);

		TArray<UK2Node_FunctionEntry*> EntryNodes;
		Graph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);

		if (ensure(EntryNodes.Num() == 1 && EntryNodes[0]))
		{
			int32 ExtraFunctionFlags = ( FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public );
			EntryNodes[0]->AddExtraFlags(ExtraFunctionFlags);

			EntryNodes[0]->bIsEditable = true;
			EntryNodes[0]->MetaData.Category = LOCTEXT("DefaultCategory", "Sequencer Event Endpoints");

			EntryNodes[0]->NodeComment = GetDefaultCommentText().ToString();
			EntryNodes[0]->bCommentBubblePinned = true;
			EntryNodes[0]->bCommentBubbleVisible = true;

			return EntryNodes[0];
		}
	}

	return nullptr;
}

void FMovieSceneSequenceEditor::InitializeEndpointForTrack(UMovieSceneEventTrack* EventTrack, UK2Node_FunctionEntry* Endpoint) const
{
	SetupDefaultPinForEndpoint(EventTrack, Endpoint);
}

UClass* FMovieSceneSequenceEditor::FindTrackObjectBindingClass(UMovieSceneTrack* Track)
{
	UMovieScene* MovieScene = Track->GetTypedOuter<UMovieScene>();

	// We have to find the object binding that owns this track, which is potentially slow, but there is no back reference here.
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		if (!Algo::Find(Binding.GetTracks(), Track))
		{
			continue;
		}

		if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid()))
		{
			return const_cast<UClass*>(Possessable->GetPossessedObjectClass());
		}
		else if(FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Binding.GetObjectGuid()))
		{
			return Spawnable->GetObjectTemplate()->GetClass();
		}
	}

	return nullptr;
}

void FMovieSceneSequenceEditor::BindEventToEndpoint(UMovieSceneEventSectionBase* EventSection, FMovieSceneEvent* Event, UK2Node_FunctionEntry* Endpoint)
{
	check(EventSection && Event);

	EventSection->Modify();
	Event->SetFunctionEntry(Endpoint);

	if (UBlueprint* Blueprint = Endpoint ? Endpoint->GetBlueprint() : nullptr)
	{
		EventSection->SetDirectorBlueprint(Blueprint);
	}
}

#undef LOCTEXT_NAMESPACE	// MovieSceneSequenceEditor
// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemTrackEditor.h"
#include "MovieScene/MovieSceneNiagaraSystemTrack.h"
#include "MovieScene/MovieSceneNiagaraSystemSpawnSection.h"
#include "MovieScene/Parameters/MovieSceneNiagaraParameterTrack.h"
#include "NiagaraSystemSpawnSection.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEditorUtilities.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEditorModule.h"
#include "NiagaraSystemEditorData.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemTrackEditor"

TSharedRef<ISequencerTrackEditor> FNiagaraSystemTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FNiagaraSystemTrackEditor(InSequencer));
}

FNiagaraSystemTrackEditor::FNiagaraSystemTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerSection> FNiagaraSystemTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	checkf(SectionObject.GetClass()->IsChildOf<UMovieSceneNiagaraSystemSpawnSection>(), TEXT("Unsupported section."));
	return MakeShareable(new FNiagaraSystemSpawnSection(SectionObject));
}

bool FNiagaraSystemTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneNiagaraSystemTrack::StaticClass();
}

void GetAnimatedParameters(UMovieScene& MovieScene, FGuid ObjectBinding, TSet<FNiagaraVariable>& AnimatedParameters)
{
	for (const FMovieSceneBinding& Binding : MovieScene.GetBindings())
	{
		if (Binding.GetObjectGuid() == ObjectBinding)
		{
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				UMovieSceneNiagaraParameterTrack* ParameterTrack = Cast<UMovieSceneNiagaraParameterTrack>(Track);
				if (ParameterTrack != nullptr)
				{
					AnimatedParameters.Add(ParameterTrack->GetParameter());
				}
			}
			break;
		}
	}
}

void FNiagaraSystemTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(UNiagaraComponent::StaticClass()))
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddNiagaraSystemTrack", "Niagara System Life Cycle Track"),
			LOCTEXT("AddNiagaraSystemTrackToolTip", "Add a track for controlling niagara system life cycle behavior."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FNiagaraSystemTrackEditor::AddNiagaraSystemTrack, ObjectBinding))
		);
	}

	TArrayView<TWeakObjectPtr<UObject>> BoundObjects = GetSequencer()->FindBoundObjects(ObjectBinding, GetSequencer()->GetFocusedTemplateID());

	UNiagaraSystem* System = nullptr;
	for (TWeakObjectPtr<UObject> BoundObject : BoundObjects)
	{
		UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(BoundObject.Get());
		if (NiagaraComponent != nullptr)
		{
			System = NiagaraComponent->GetAsset();
		}
	}

	if (System != nullptr)
	{
		TArray<FNiagaraVariable> ParameterVariables;
		System->GetExposedParameters().GetUserParameters(ParameterVariables);

		TSet<FNiagaraVariable> AnimatedParameters;
		GetAnimatedParameters(*GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectBinding, AnimatedParameters);

		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		for (const FNiagaraVariable& ParameterVariable : ParameterVariables)
		{
			if (ParameterVariable.GetType().IsDataInterface() == false &&
				NiagaraEditorModule.CanCreateParameterTrackForType(*ParameterVariable.GetType().GetScriptStruct()) && 
				AnimatedParameters.Contains(ParameterVariable) == false)
			{
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("AddNiagaraParameterTrackFormat", "{0} Parameter Track"), FText::FromName(ParameterVariable.GetName())),
					FText::Format(LOCTEXT("AddNiagaraSystemTrackToolTipFormat", "Add a track for animating the {0} parameter."), FText::FromName(ParameterVariable.GetName())),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &FNiagaraSystemTrackEditor::AddNiagaraParameterTrack, ObjectBinding, ParameterVariable))
				);
			}
		}
	}
}

void FNiagaraSystemTrackEditor::AddNiagaraSystemTrack(FGuid ObjectBinding)
{
	TArrayView<TWeakObjectPtr<UObject>> BoundObjects = GetSequencer()->FindBoundObjects(ObjectBinding, GetSequencer()->GetFocusedTemplateID());

	UNiagaraSystem* System = nullptr;
	for (TWeakObjectPtr<UObject> BoundObject : BoundObjects)
	{
		UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(BoundObject.Get());
		if (NiagaraComponent != nullptr)
		{
			System = NiagaraComponent->GetAsset();
		}
	}

	if (System != nullptr)
	{
		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		if (MovieScene->IsReadOnly())
		{
			return;
		}

		FScopedTransaction AddTrackTransaction(LOCTEXT("AddNiagaraSystemLifeCycleTrackTransaction", "Add Niagara System Life Cycle Track"));
		MovieScene->Modify();

		UMovieSceneNiagaraSystemTrack* NiagaraSystemTrack = MovieScene->AddTrack<UMovieSceneNiagaraSystemTrack>(ObjectBinding);
		NiagaraSystemTrack->SetDisplayName(LOCTEXT("SystemLifeCycleTrackName", "System Life Cycle"));
		UMovieSceneNiagaraSystemSpawnSection* SpawnSection = NewObject<UMovieSceneNiagaraSystemSpawnSection>(NiagaraSystemTrack, NAME_None, RF_Transactional);

		FFrameRate FrameResolution = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->GetTickResolution();
		FFrameTime SpawnSectionStartTime = GetSequencer()->GetLocalTime().ConvertTo(FrameResolution);
		FFrameTime SpawnSectionDuration;

		UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData());
		if (SystemEditorData != nullptr && SystemEditorData->GetPlaybackRange().HasLowerBound() && SystemEditorData->GetPlaybackRange().HasUpperBound())
		{
			SpawnSectionDuration = FrameResolution.AsFrameTime(SystemEditorData->GetPlaybackRange().Size<float>());
		}
		else
		{
			SpawnSectionDuration = FrameResolution.AsFrameTime(5.0);
		}

		SpawnSection->SetRange(TRange<FFrameNumber>(
			SpawnSectionStartTime.RoundToFrame(),
			(SpawnSectionStartTime + SpawnSectionDuration).RoundToFrame()));
		NiagaraSystemTrack->AddSection(*SpawnSection);

		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

void FNiagaraSystemTrackEditor::AddNiagaraParameterTrack(FGuid ObjectBinding, FNiagaraVariable Parameter)
{
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	if (NiagaraEditorModule.CanCreateParameterTrackForType(*Parameter.GetType().GetScriptStruct()))
	{
		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		if (MovieScene->IsReadOnly())
		{
			return;
		}

		FScopedTransaction AddTrackTransaction(LOCTEXT("AddNiagaraParameterTrackTransaction", "Add Niagara Parameter Track"));
		MovieScene->Modify();

		UMovieSceneNiagaraParameterTrack* ParameterTrack = NiagaraEditorModule.CreateParameterTrackForType(*Parameter.GetType().GetScriptStruct(), Parameter);
		MovieScene->AddGivenTrack(ParameterTrack, ObjectBinding);

		ParameterTrack->SetParameter(Parameter);
		ParameterTrack->SetDisplayName(FText::FromName(Parameter.GetName()));

		UMovieSceneSection* ParameterSection = ParameterTrack->CreateNewSection();
		ParameterTrack->AddSection(*ParameterSection);

		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
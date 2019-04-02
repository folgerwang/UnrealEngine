// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PrimitiveMaterialTrackEditor.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "ISequencerModule.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Algo/Find.h"


#define LOCTEXT_NAMESPACE "PrimitiveMaterialTrackEditor"


FPrimitiveMaterialTrackEditor::FPrimitiveMaterialTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FKeyframeTrackEditor(InSequencer)
{}

TSharedRef<ISequencerTrackEditor> FPrimitiveMaterialTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FPrimitiveMaterialTrackEditor>(OwningSequencer);
}

void FPrimitiveMaterialTrackEditor::ExtendObjectBindingTrackMenu(TSharedRef<FExtender> Extender, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(UPrimitiveComponent::StaticClass()))
	{
		Extender->AddMenuExtension(SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateSP(this, &FPrimitiveMaterialTrackEditor::ConstructObjectBindingTrackMenu, ObjectBinding));
	}
}

void FPrimitiveMaterialTrackEditor::ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding)
{
	auto GetMaterialIndexForTrack = [](UMovieSceneTrack* InTrack)
	{
		UMovieScenePrimitiveMaterialTrack* MaterialTrack = Cast<UMovieScenePrimitiveMaterialTrack>(InTrack);
		return MaterialTrack ? MaterialTrack->MaterialIndex : INDEX_NONE;
	};

	int32 MinNumMaterials = TNumericLimits<int32>::Max();

	for (TWeakObjectPtr<> WeakObject : GetSequencer()->FindObjectsInCurrentSequence(ObjectBinding))
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(WeakObject.Get());
		if (PrimitiveComponent)
		{
			MinNumMaterials = FMath::Min(MinNumMaterials, PrimitiveComponent->GetNumMaterials());
		}
	}

	if (MinNumMaterials > 0 && MinNumMaterials < TNumericLimits<int32>::Max())
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("MaterialSwitcherTitle", "Material Switchers"));

		const UMovieScene*        MovieScene = GetFocusedMovieScene();
		const FMovieSceneBinding* Binding    = Algo::FindBy(MovieScene->GetBindings(), ObjectBinding, &FMovieSceneBinding::GetObjectGuid);

		check(Binding);

		for (int32 Index = 0; Index < MinNumMaterials; ++Index)
		{
			const bool bAlreadyExists = Algo::FindBy(Binding->GetTracks(), Index, GetMaterialIndexForTrack) != nullptr;
			if (!bAlreadyExists)
			{
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("MaterialID_Format", "Material Element {0} Switcher"), FText::AsNumber(Index)),
					FText(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FPrimitiveMaterialTrackEditor::CreateTrackForElement, ObjectBinding, Index)
					)
				);
			}
		}

		MenuBuilder.EndSection();
	}
}

void FPrimitiveMaterialTrackEditor::CreateTrackForElement(FGuid ObjectBindingID, int32 MaterialIndex)
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	FScopedTransaction Transaction(LOCTEXT("CreateTrack", "Create Material Track"));
	MovieScene->Modify();

	UMovieScenePrimitiveMaterialTrack* NewTrack = MovieScene->AddTrack<UMovieScenePrimitiveMaterialTrack>(ObjectBindingID);
	NewTrack->MaterialIndex = MaterialIndex;
	NewTrack->SetDisplayName(FText::Format(LOCTEXT("MaterialTrackName_Format", "Material Element {0}"), FText::AsNumber(MaterialIndex)));

	NewTrack->AddSection(*NewTrack->CreateNewSection());

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

#undef LOCTEXT_NAMESPACE

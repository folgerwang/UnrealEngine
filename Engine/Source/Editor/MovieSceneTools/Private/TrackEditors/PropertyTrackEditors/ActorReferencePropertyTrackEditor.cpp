// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/ActorReferencePropertyTrackEditor.h"
#include "GameFramework/Actor.h"


TSharedRef<ISequencerTrackEditor> FActorReferencePropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FActorReferencePropertyTrackEditor(OwningSequencer));
}

void FActorReferencePropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys )
{
	AActor* NewReferencedActor = PropertyChangedParams.GetPropertyValue<AActor*>();
	if ( NewReferencedActor != nullptr )
	{
		FGuid ActorGuid = GetSequencer()->GetHandleToObject( NewReferencedActor );
		if ( ActorGuid.IsValid() )
		{
			FMovieSceneActorReferenceKey NewKey;
			NewKey.Object = FMovieSceneObjectBindingID(ActorGuid, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
			OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneActorReferenceData>(0, NewKey, true));
		}
	}
}

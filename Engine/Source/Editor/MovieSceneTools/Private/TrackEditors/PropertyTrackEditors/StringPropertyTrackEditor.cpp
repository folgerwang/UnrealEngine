// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/StringPropertyTrackEditor.h"


TSharedRef<ISequencerTrackEditor> FStringPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FStringPropertyTrackEditor(OwningSequencer));
}


void FStringPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys )
{
	void* CurrentObject = PropertyChangedParams.ObjectsThatChanged[0];
	void* PropertyValue = nullptr;
	for (int32 i = 0; i < PropertyChangedParams.PropertyPath.GetNumProperties(); i++)
	{
		if (UProperty* Property = PropertyChangedParams.PropertyPath.GetPropertyInfo(i).Property.Get())
		{
			CurrentObject = Property->ContainerPtrToValuePtr<FString>(CurrentObject, 0);
		}
	}

	const UStrProperty* StrProperty = Cast<const UStrProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	if ( StrProperty )
	{
		FString StrPropertyValue = StrProperty->GetPropertyValue(CurrentObject);
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneStringChannel>(0, MoveTemp(StrPropertyValue), true));
	}
}

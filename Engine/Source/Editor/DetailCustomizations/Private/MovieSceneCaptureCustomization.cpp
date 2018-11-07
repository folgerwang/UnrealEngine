// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCaptureCustomization.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "MovieSceneCapture.h"
#include "IPropertyUtilities.h"
#include "Editor.h"

TSharedRef<IDetailCustomization> FMovieSceneCaptureCustomization::MakeInstance()
{
	return MakeShareable(new FMovieSceneCaptureCustomization);
}

FMovieSceneCaptureCustomization::~FMovieSceneCaptureCustomization()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	GEditor->OnObjectsReplaced().Remove(ObjectsReplacedHandle);
}

void FMovieSceneCaptureCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FMovieSceneCaptureCustomization::OnObjectPostEditChange);
	ObjectsReplacedHandle = GEditor->OnObjectsReplaced().AddRaw(this, &FMovieSceneCaptureCustomization::OnObjectsReplaced);
}

void FMovieSceneCaptureCustomization::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	// Defer the update 1 frame to ensure that we don't end up in a recursive loop adding bindings to the OnObjectsReplaced delegate that is currently being triggered
	// (since the bindings are added in FMovieSceneCaptureCustomization::CustomizeDetails)
	PropertyUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateLambda([LocalPropertyUtilities = PropertyUtilities]{ LocalPropertyUtilities->ForceRefresh(); }));
}

void FMovieSceneCaptureCustomization::OnObjectPostEditChange( UObject* Object, FPropertyChangedEvent& PropertyChangedEvent )
{
	if (ObjectsBeingCustomized.Contains(Object))
	{
		static FName ImageCaptureProtocolTypeName = GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, ImageCaptureProtocolType);
		static FName ImageCaptureProtocolName     = GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, ImageCaptureProtocol);
		static FName AudioCaptureProtocolTypeName = GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, AudioCaptureProtocolType);
		static FName AudioCaptureProtocolName	  = GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, AudioCaptureProtocol);

		FName PropertyName = PropertyChangedEvent.GetPropertyName();
		if (PropertyName == ImageCaptureProtocolTypeName || ( PropertyName == ImageCaptureProtocolName && PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet ) ||
			PropertyName == AudioCaptureProtocolTypeName || ( PropertyName == AudioCaptureProtocolName && PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet ))
		{
			// Defer the update 1 frame to ensure that we don't end up in a recursive loop adding bindings to the OnObjectPropertyChanged delegate that is currently being triggered
			// (since the bindings are added in FMovieSceneCaptureCustomization::CustomizeDetails)
			PropertyUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateLambda([LocalPropertyUtilities = PropertyUtilities]{ LocalPropertyUtilities->ForceRefresh(); }));
		}
	}
}

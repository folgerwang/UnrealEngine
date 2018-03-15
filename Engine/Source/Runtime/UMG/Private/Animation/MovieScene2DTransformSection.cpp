// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieScene2DTransformSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Slate/WidgetTransform.h"


#if WITH_EDITOR

struct F2DTransformSectionEditorData
{
	F2DTransformSectionEditorData()
	{
		FText TranslationGroup = NSLOCTEXT("MovieScene2DTransformSection", "Translation", "Translation");
		FText RotationGroup = NSLOCTEXT("MovieScene2DTransformSection", "Rotation", "Rotation");
		FText ScaleGroup = NSLOCTEXT("MovieScene2DTransformSection", "Scale", "Scale");
		FText ShearGroup = NSLOCTEXT("MovieScene2DTransformSection", "Shear", "Shear");

		CommonData[0].SetIdentifiers("Translation.X", FCommonChannelData::ChannelX, TranslationGroup);
		CommonData[0].SortOrder = 0;

		CommonData[1].SetIdentifiers("Translation.Y", FCommonChannelData::ChannelY, TranslationGroup);
		CommonData[1].SortOrder = 1;

		CommonData[2].SetIdentifiers("Angle", NSLOCTEXT("MovieScene2DTransformSection", "AngleText", "Angle"), RotationGroup);
		CommonData[2].SortOrder = 2;

		CommonData[3].SetIdentifiers("Scale.X", FCommonChannelData::ChannelX, ScaleGroup);
		CommonData[3].SortOrder = 3;
		
		CommonData[4].SetIdentifiers("Scale.Y", FCommonChannelData::ChannelY, ScaleGroup);
		CommonData[4].SortOrder = 4;

		CommonData[5].SetIdentifiers("Shear.X", FCommonChannelData::ChannelX, ShearGroup);
		CommonData[5].SortOrder = 5;
		
		CommonData[6].SetIdentifiers("Shear.Y", FCommonChannelData::ChannelY, ShearGroup);
		CommonData[6].SortOrder = 6;

		ExternalValues[0].OnGetExternalValue = ExtractTranslationX;
		ExternalValues[1].OnGetExternalValue = ExtractTranslationY;
		ExternalValues[2].OnGetExternalValue = ExtractRotation;
		ExternalValues[3].OnGetExternalValue = ExtractScaleX;
		ExternalValues[4].OnGetExternalValue = ExtractScaleY;
		ExternalValues[5].OnGetExternalValue = ExtractShearX;
		ExternalValues[6].OnGetExternalValue = ExtractShearY;
	}

	static TOptional<float> ExtractTranslationX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Translation.X : TOptional<float>();
	}
	static TOptional<float> ExtractTranslationY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Translation.Y : TOptional<float>();
	}

	static TOptional<float> ExtractRotation(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Angle : TOptional<float>();
	}

	static TOptional<float> ExtractScaleX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Scale.X : TOptional<float>();
	}
	static TOptional<float> ExtractScaleY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Scale.Y : TOptional<float>();
	}

	static TOptional<float> ExtractShearX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Shear.X : TOptional<float>();
	}
	static TOptional<float> ExtractShearY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FWidgetTransform>(InObject).Shear.Y : TOptional<float>();
	}

	FMovieSceneChannelEditorData    CommonData[7];
	TMovieSceneExternalValue<float> ExternalValues[7];
};

#endif	// WITH_EDITOR

UMovieScene2DTransformSection::UMovieScene2DTransformSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	BlendType = EMovieSceneBlendType::Absolute;

	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	static const F2DTransformSectionEditorData EditorData;

	Channels.Add(Translation[0], EditorData.CommonData[0], EditorData.ExternalValues[0]);
	Channels.Add(Translation[1], EditorData.CommonData[1], EditorData.ExternalValues[1]);
	Channels.Add(Rotation,       EditorData.CommonData[2], EditorData.ExternalValues[2]);
	Channels.Add(Scale[0],       EditorData.CommonData[3], EditorData.ExternalValues[3]);
	Channels.Add(Scale[1],       EditorData.CommonData[4], EditorData.ExternalValues[4]);
	Channels.Add(Shear[0],       EditorData.CommonData[5], EditorData.ExternalValues[5]);
	Channels.Add(Shear[1],       EditorData.CommonData[6], EditorData.ExternalValues[6]);

#else

	Channels.Add(Translation[0]);
	Channels.Add(Translation[1]);
	Channels.Add(Rotation);
	Channels.Add(Scale[0]);
	Channels.Add(Scale[1]);
	Channels.Add(Shear[0]);
	Channels.Add(Shear[1]);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

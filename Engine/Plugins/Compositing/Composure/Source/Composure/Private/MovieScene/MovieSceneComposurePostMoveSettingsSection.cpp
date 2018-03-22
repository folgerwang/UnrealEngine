// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneComposurePostMoveSettingsSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "ComposurePostMoves.h"

#if WITH_EDITOR

struct FPostMoveSettingsChannelEditorData
{
	FPostMoveSettingsChannelEditorData()
	{
		FText PivotGroup = NSLOCTEXT("PostMoves", "Pivot", "Pivot");
		FText TranslationGroup = NSLOCTEXT("PostMoves", "Translation", "Translation");

		CommonData[0].SetIdentifiers("Pivot.X", FCommonChannelData::ChannelX, PivotGroup);
		CommonData[0].SortOrder = 0;
		CommonData[0].Color = FCommonChannelData::RedChannelColor;

		CommonData[1].SetIdentifiers("Pivot.Y", FCommonChannelData::ChannelY, PivotGroup);
		CommonData[1].SortOrder = 1;
		CommonData[1].Color = FCommonChannelData::GreenChannelColor;

		CommonData[2].SetIdentifiers("Translation.X", FCommonChannelData::ChannelX, TranslationGroup);
		CommonData[2].SortOrder = 2;
		CommonData[2].Color = FCommonChannelData::RedChannelColor;

		CommonData[3].SetIdentifiers("Translation.Y", FCommonChannelData::ChannelY, TranslationGroup);
		CommonData[3].SortOrder = 3;
		CommonData[3].Color = FCommonChannelData::GreenChannelColor;

		CommonData[4].SetIdentifiers("Rotation", NSLOCTEXT("PostMoves", "Rotation", "Rotation"));
		CommonData[4].SortOrder = 4;

		CommonData[5].SetIdentifiers("Scale", NSLOCTEXT("PostMoves", "Scale", "Scale"));
		CommonData[5].SortOrder = 5;

		ExternalValues[0].OnGetExternalValue = ExtractPivotX;
		ExternalValues[1].OnGetExternalValue = ExtractPivotY;
		ExternalValues[2].OnGetExternalValue = ExtractTranslationX;
		ExternalValues[3].OnGetExternalValue = ExtractTranslationY;
		ExternalValues[4].OnGetExternalValue = ExtractRotation;
		ExternalValues[5].OnGetExternalValue = ExtractScale;
	}

	static TOptional<float> ExtractPivotX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FComposurePostMoveSettings>(InObject).Pivot.X : TOptional<float>();
	}
	static TOptional<float> ExtractPivotY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FComposurePostMoveSettings>(InObject).Pivot.Y : TOptional<float>();
	}

	static TOptional<float> ExtractTranslationX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FComposurePostMoveSettings>(InObject).Translation.X : TOptional<float>();
	}
	static TOptional<float> ExtractTranslationY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FComposurePostMoveSettings>(InObject).Translation.Y : TOptional<float>();
	}

	static TOptional<float> ExtractRotation(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FComposurePostMoveSettings>(InObject).RotationAngle : TOptional<float>();
	}

	static TOptional<float> ExtractScale(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FComposurePostMoveSettings>(InObject).Scale : TOptional<float>();
	}

	FMovieSceneChannelEditorData CommonData[6];
	TMovieSceneExternalValue<float> ExternalValues[6];
};

#endif // WITH_EDITOR


UMovieSceneComposurePostMoveSettingsSection::UMovieSceneComposurePostMoveSettingsSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	BlendType = EMovieSceneBlendType::Absolute;

	// Initialize this section's channel proxy
	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	static const FPostMoveSettingsChannelEditorData EditorData;

	Channels.Add(Pivot[0],       EditorData.CommonData[0], EditorData.ExternalValues[0]);
	Channels.Add(Pivot[1],       EditorData.CommonData[1], EditorData.ExternalValues[1]);
	Channels.Add(Translation[0], EditorData.CommonData[2], EditorData.ExternalValues[2]);
	Channels.Add(Translation[1], EditorData.CommonData[3], EditorData.ExternalValues[3]);
	Channels.Add(RotationAngle,  EditorData.CommonData[4], EditorData.ExternalValues[4]);
	Channels.Add(Scale,          EditorData.CommonData[5], EditorData.ExternalValues[5]);

#else

	Channels.Add(Pivot[0]);
	Channels.Add(Pivot[1]);
	Channels.Add(Translation[0]);
	Channels.Add(Translation[1]);
	Channels.Add(RotationAngle);
	Channels.Add(Scale);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

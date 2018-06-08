// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieScene2DTransformSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Slate/WidgetTransform.h"


#if WITH_EDITOR

struct F2DTransformSectionEditorData
{
	F2DTransformSectionEditorData(EMovieScene2DTransformChannel Mask)
	{
		FText TranslationGroup = NSLOCTEXT("MovieScene2DTransformSection", "Translation", "Translation");
		FText RotationGroup = NSLOCTEXT("MovieScene2DTransformSection", "Rotation", "Rotation");
		FText ScaleGroup = NSLOCTEXT("MovieScene2DTransformSection", "Scale", "Scale");
		FText ShearGroup = NSLOCTEXT("MovieScene2DTransformSection", "Shear", "Shear");

		MetaData[0].SetIdentifiers("Translation.X", FCommonChannelData::ChannelX, TranslationGroup);
		MetaData[0].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::TranslationX);
		MetaData[0].SortOrder = 0;

		MetaData[1].SetIdentifiers("Translation.Y", FCommonChannelData::ChannelY, TranslationGroup);
		MetaData[1].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::TranslationY);
		MetaData[1].SortOrder = 1;

		MetaData[2].SetIdentifiers("Angle", NSLOCTEXT("MovieScene2DTransformSection", "AngleText", "Angle"), RotationGroup);
		MetaData[2].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::Rotation);
		MetaData[2].SortOrder = 2;

		MetaData[3].SetIdentifiers("Scale.X", FCommonChannelData::ChannelX, ScaleGroup);
		MetaData[3].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::ScaleX);
		MetaData[3].SortOrder = 3;
		
		MetaData[4].SetIdentifiers("Scale.Y", FCommonChannelData::ChannelY, ScaleGroup);
		MetaData[4].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::ScaleY);
		MetaData[4].SortOrder = 4;

		MetaData[5].SetIdentifiers("Shear.X", FCommonChannelData::ChannelX, ShearGroup);
		MetaData[5].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::ShearX);
		MetaData[5].SortOrder = 5;
		
		MetaData[6].SetIdentifiers("Shear.Y", FCommonChannelData::ChannelY, ShearGroup);
		MetaData[6].bEnabled = EnumHasAllFlags(Mask, EMovieScene2DTransformChannel::ShearY);
		MetaData[6].SortOrder = 6;

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

	FMovieSceneChannelMetaData      MetaData[7];
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

	ProxyChannels = EMovieScene2DTransformChannel::None;
	TransformMask = EMovieScene2DTransformChannel::AllTransform;
	BlendType = EMovieSceneBlendType::Absolute;
	bSupportsInfiniteRange = true;

	UpdateChannelProxy();
}

void UMovieScene2DTransformSection::UpdateChannelProxy()
{
	if (ProxyChannels == TransformMask.GetChannels())
	{
		return;
	}
	FMovieSceneChannelProxyData Channels;

	ProxyChannels = TransformMask.GetChannels();


#if WITH_EDITOR

	F2DTransformSectionEditorData EditorData(TransformMask.GetChannels());

	Channels.Add(Translation[0], EditorData.MetaData[0], EditorData.ExternalValues[0]);
	Channels.Add(Translation[1], EditorData.MetaData[1], EditorData.ExternalValues[1]);
	Channels.Add(Rotation,       EditorData.MetaData[2], EditorData.ExternalValues[2]);
	Channels.Add(Scale[0],       EditorData.MetaData[3], EditorData.ExternalValues[3]);
	Channels.Add(Scale[1],       EditorData.MetaData[4], EditorData.ExternalValues[4]);
	Channels.Add(Shear[0],       EditorData.MetaData[5], EditorData.ExternalValues[5]);
	Channels.Add(Shear[1],       EditorData.MetaData[6], EditorData.ExternalValues[6]);

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

void UMovieScene2DTransformSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		UpdateChannelProxy();
	}
}


FMovieScene2DTransformMask UMovieScene2DTransformSection::GetMask() const
{
	return TransformMask;
}

void UMovieScene2DTransformSection::SetMask(FMovieScene2DTransformMask NewMask)
{
	TransformMask = NewMask;
	UpdateChannelProxy();
}

FMovieScene2DTransformMask UMovieScene2DTransformSection::GetMaskByName(const FName& InName) const
{
	if (InName == TEXT("Translation"))
	{
		return EMovieScene2DTransformChannel::Translation;
	}
	else if (InName == TEXT("Translation.X"))
	{
		return EMovieScene2DTransformChannel::TranslationX;
	}
	else if (InName == TEXT("Translation.Y"))
	{
		return EMovieScene2DTransformChannel::TranslationY;
	}
	else if (InName == TEXT("Angle"))
	{
		return EMovieScene2DTransformChannel::Rotation;
	}
	else if (InName == TEXT("Scale"))
	{
		return EMovieScene2DTransformChannel::Scale;
	}
	else if (InName == TEXT("Scale.X"))
	{
		return EMovieScene2DTransformChannel::ScaleX;
	}
	else if (InName == TEXT("Scale.Y"))
	{
		return EMovieScene2DTransformChannel::ScaleY;
	}
	else if (InName == TEXT("Shear"))
	{
		return EMovieScene2DTransformChannel::Shear;
	}
	else if (InName == TEXT("Shear.X"))
	{
		return EMovieScene2DTransformChannel::ShearX;
	}
	else if (InName == TEXT("Shear.Y"))
	{
		return EMovieScene2DTransformChannel::ShearY;
	}

	return EMovieScene2DTransformChannel::AllTransform;
}

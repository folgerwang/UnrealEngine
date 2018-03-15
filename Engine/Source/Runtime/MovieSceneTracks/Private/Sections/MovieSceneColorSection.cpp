// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneColorSection.h"
#include "UObject/StructOnScope.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Styling/SlateColor.h"

#if WITH_EDITOR
struct FColorSectionEditorData
{
	FColorSectionEditorData()
	{
		CommonData[0].SetIdentifiers("Color.R", FCommonChannelData::ChannelR);
		CommonData[0].SortOrder = 0;
		CommonData[0].Color = FCommonChannelData::RedChannelColor;

		CommonData[1].SetIdentifiers("Color.G", FCommonChannelData::ChannelG);
		CommonData[1].SortOrder = 1;
		CommonData[1].Color = FCommonChannelData::GreenChannelColor;

		CommonData[2].SetIdentifiers("Color.B", FCommonChannelData::ChannelB);
		CommonData[2].SortOrder = 2;
		CommonData[2].Color = FCommonChannelData::BlueChannelColor;

		CommonData[3].SetIdentifiers("Color.A", FCommonChannelData::ChannelA);
		CommonData[3].SortOrder = 3;

		ExternalValues[0].OnGetExternalValue = ExtractChannelR;
		ExternalValues[1].OnGetExternalValue = ExtractChannelG;
		ExternalValues[2].OnGetExternalValue = ExtractChannelB;
		ExternalValues[3].OnGetExternalValue = ExtractChannelA;
	}

	static FLinearColor GetPropertyValue(UObject& InObject, FTrackInstancePropertyBindings& Bindings)
	{
		const FName SlateColorName("SlateColor");

		UStructProperty* ColorStructProperty = Cast<UStructProperty>(Bindings.GetProperty(InObject));
		if (ColorStructProperty != nullptr)
		{
			if (ColorStructProperty->Struct->GetFName() == SlateColorName)
			{
				return Bindings.GetCurrentValue<FSlateColor>(InObject).GetSpecifiedColor();
			}

			if (ColorStructProperty->Struct->GetFName() == NAME_LinearColor)
			{
				return Bindings.GetCurrentValue<FLinearColor>(InObject);
			}

			if (ColorStructProperty->Struct->GetFName() == NAME_Color)
			{
				return Bindings.GetCurrentValue<FColor>(InObject);
			}
		}
		return FLinearColor(0.f,0.f,0.f,0.f);
	}

	static TOptional<float> ExtractChannelR(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings).R : TOptional<float>();
	}
	static TOptional<float> ExtractChannelG(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings).G : TOptional<float>();
	}
	static TOptional<float> ExtractChannelB(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings).B : TOptional<float>();
	}
	static TOptional<float> ExtractChannelA(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings).A : TOptional<float>();
	}

	FMovieSceneChannelEditorData    CommonData[4];
	TMovieSceneExternalValue<float> ExternalValues[4];
};
#endif

/* FMovieSceneColorKeyStruct interface
 *****************************************************************************/

void FMovieSceneColorKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* UMovieSceneColorSection structors
 *****************************************************************************/

UMovieSceneColorSection::UMovieSceneColorSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	BlendType = EMovieSceneBlendType::Absolute;
	bSupportsInfiniteRange = true;

	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	static FColorSectionEditorData EditorData;
	Channels.Add(RedCurve,   EditorData.CommonData[0], EditorData.ExternalValues[0]);
	Channels.Add(GreenCurve, EditorData.CommonData[1], EditorData.ExternalValues[1]);
	Channels.Add(BlueCurve,  EditorData.CommonData[2], EditorData.ExternalValues[2]);
	Channels.Add(AlphaCurve, EditorData.CommonData[3], EditorData.ExternalValues[3]);

#else

	Channels.Add(RedCurve);
	Channels.Add(GreenCurve);
	Channels.Add(BlueCurve);
	Channels.Add(AlphaCurve);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

/* UMovieSceneSection interface
 *****************************************************************************/

TSharedPtr<FStructOnScope> UMovieSceneColorSection::GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles)
{
	TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneColorKeyStruct::StaticStruct()));
	auto Struct = (FMovieSceneColorKeyStruct*)KeyStruct->GetStructMemory();

	TArrayView<FMovieSceneFloatChannel*> FloatChannels = ChannelProxy->GetChannels<FMovieSceneFloatChannel>();

	Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[0]), &Struct->Color.R, KeyHandles));
	Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[1]), &Struct->Color.G, KeyHandles));
	Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[2]), &Struct->Color.B, KeyHandles));
	Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[3]), &Struct->Color.A, KeyHandles));

	Struct->KeyStructInterop.SetStartingValues();
	Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);

	return KeyStruct;
}

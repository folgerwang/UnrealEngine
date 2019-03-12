// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneVectorSection.h"
#include "UObject/StructOnScope.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

/* FMovieSceneVectorKeyStruct interface
 *****************************************************************************/

#if WITH_EDITOR
struct FVectorSectionEditorData
{
	FVectorSectionEditorData(int32 NumChannels)
	{
		MetaData[0].SetIdentifiers("Vector.X", FCommonChannelData::ChannelX);
		MetaData[0].SortOrder = 0;
		MetaData[0].Color = FCommonChannelData::RedChannelColor;
		MetaData[0].bCanCollapseToTrack = false;

		MetaData[1].SetIdentifiers("Vector.Y", FCommonChannelData::ChannelY);
		MetaData[1].SortOrder = 1;
		MetaData[1].Color = FCommonChannelData::GreenChannelColor;
		MetaData[1].bCanCollapseToTrack = false;

		MetaData[2].SetIdentifiers("Vector.Z", FCommonChannelData::ChannelZ);
		MetaData[2].SortOrder = 2;
		MetaData[2].Color = FCommonChannelData::BlueChannelColor;
		MetaData[2].bCanCollapseToTrack = false;

		MetaData[3].SetIdentifiers("Vector.W", FCommonChannelData::ChannelW);
		MetaData[3].SortOrder = 3;
		MetaData[3].bCanCollapseToTrack = false;

		ExternalValues[0].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelX(InObject, Bindings, NumChannels); };
		ExternalValues[1].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelY(InObject, Bindings, NumChannels); };
		ExternalValues[2].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelZ(InObject, Bindings, NumChannels); };
		ExternalValues[3].OnGetExternalValue = [NumChannels](UObject& InObject, FTrackInstancePropertyBindings* Bindings) { return ExtractChannelW(InObject, Bindings, NumChannels); };


		ExternalValues[0].OnGetCurrentValueAndWeight = [NumChannels](UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
			float& OutValue, float& OutWeight) { GetChannelValueAndWeight(NumChannels, 0, Object, SectionToKey, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight); };
		ExternalValues[1].OnGetCurrentValueAndWeight = [NumChannels](UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
			float& OutValue, float& OutWeight) { GetChannelValueAndWeight(NumChannels, 1, Object, SectionToKey, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight); };
		ExternalValues[2].OnGetCurrentValueAndWeight = [NumChannels](UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
			float& OutValue, float& OutWeight) { GetChannelValueAndWeight(NumChannels, 2,Object, SectionToKey, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight); };
		ExternalValues[3].OnGetCurrentValueAndWeight = [NumChannels](UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
			float& OutValue, float& OutWeight) { GetChannelValueAndWeight(NumChannels, 3, Object, SectionToKey, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight); };

	}

	static FVector4 GetPropertyValue(UObject& InObject, FTrackInstancePropertyBindings& Bindings, int32 NumChannels)
	{
		if (NumChannels == 2)
		{
			FVector2D Vector = Bindings.GetCurrentValue<FVector2D>(InObject);
			return FVector4(Vector.X, Vector.Y, 0.f, 0.f);
		}
		else if (NumChannels == 3)
		{
			FVector Vector = Bindings.GetCurrentValue<FVector>(InObject);
			return FVector4(Vector.X, Vector.Y, Vector.Z, 0.f);
		}
		else
		{
			return Bindings.GetCurrentValue<FVector4>(InObject);
		}
	}

	static TOptional<float> ExtractChannelX(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).X : TOptional<float>();
	}
	static TOptional<float> ExtractChannelY(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).Y : TOptional<float>();
	}
	static TOptional<float> ExtractChannelZ(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).Z : TOptional<float>();
	}
	static TOptional<float> ExtractChannelW(UObject& InObject, FTrackInstancePropertyBindings* Bindings, int32 NumChannels)
	{
		return Bindings ? GetPropertyValue(InObject, *Bindings, NumChannels).W : TOptional<float>();
	}

	static void GetChannelValueAndWeight(int32 NumChannels, int32 Index, UObject* Object, UMovieSceneSection*  SectionToKey,  FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
		float& OutValue, float& OutWeight)
	{
		OutValue = 0.0f;
		OutWeight = 1.0f;
		if (Index >= NumChannels)
		{
			return;
		}

		UMovieSceneTrack* Track = SectionToKey->GetTypedOuter<UMovieSceneTrack>();

		if (Track)
		{
			FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();
			FMovieSceneInterrogationData InterrogationData;
			RootTemplate.CopyActuators(InterrogationData.GetAccumulator());

			FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, TickResolution));
			EvalTrack.Interrogate(Context, InterrogationData, Object);

			switch (NumChannels)
			{
			case 2:
			{
				FVector2D Val(0.0f, 0.0f);
				for (const FVector2D& InVector : InterrogationData.Iterate<FVector2D>(FMovieScenePropertySectionTemplate::GetVector2DInterrogationKey()))
				{
					Val = InVector;
					break;
				}
				switch (Index)
				{
				case 0:
					OutValue = Val.X;
					break;
				case 1:
					OutValue = Val.Y;
					break;
				}
			}
			break;
			case 3:
			{
				FVector Val(0.0f, 0.0f, 0.0f);
				for (const FVector& InVector : InterrogationData.Iterate<FVector>(FMovieScenePropertySectionTemplate::GetVectorInterrogationKey()))
				{
					Val = InVector;
					break;
				}
				switch (Index)
				{
				case 0:
					OutValue = Val.X;
					break;
				case 1:
					OutValue = Val.Y;
					break;
				case 2:
					OutValue = Val.Z;
					break;
				}
			}
			break;
			case 4:
			{
				FVector4 Val(0.0f, 0.0f, 0.0f, 0.0f);
				for (const FVector4& InVector : InterrogationData.Iterate<FVector4>(FMovieScenePropertySectionTemplate::GetVector4InterrogationKey()))
				{
					Val = InVector;
					break;
				}
				switch (Index)
				{
				case 0:
					OutValue = Val.X;
					break;
				case 1:
					OutValue = Val.Y;
					break;
				case 2:
					OutValue = Val.Z;
					break;
				case 3:
					OutValue = Val.W;
					break;
				}
			}
			break;
			}
		}
		OutWeight = MovieSceneHelpers::CalculateWeightForBlending(SectionToKey, KeyTime);
	}

	FMovieSceneChannelMetaData      MetaData[4];
	TMovieSceneExternalValue<float> ExternalValues[4];

};
#endif

void FMovieSceneVectorKeyStructBase::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* UMovieSceneVectorSection structors
 *****************************************************************************/

UMovieSceneVectorSection::UMovieSceneVectorSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChannelsUsed = 0;
	bSupportsInfiniteRange = true;

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	BlendType = EMovieSceneBlendType::Absolute;
}

void UMovieSceneVectorSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		RecreateChannelProxy();
	}
}

void UMovieSceneVectorSection::PostEditImport()
{
	Super::PostEditImport();

	RecreateChannelProxy();
}

void UMovieSceneVectorSection::RecreateChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

	check(ChannelsUsed <= ARRAY_COUNT(Curves));

#if WITH_EDITOR

	const FVectorSectionEditorData EditorData(ChannelsUsed);
	for (int32 Index = 0; Index < ChannelsUsed; ++Index)
	{
		Channels.Add(Curves[Index], EditorData.MetaData[Index], EditorData.ExternalValues[Index]);
	}

#else

	for (int32 Index = 0; Index < ChannelsUsed; ++Index)
	{
		Channels.Add(Curves[Index]);
	}

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

TSharedPtr<FStructOnScope> UMovieSceneVectorSection::GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles)
{
	TSharedPtr<FStructOnScope> KeyStruct;
	if (ChannelsUsed == 2)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVector2DKeyStruct::StaticStruct()));
	}
	else if (ChannelsUsed == 3)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVectorKeyStruct::StaticStruct()));
	}
	else if (ChannelsUsed == 4)
	{
		KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVector4KeyStruct::StaticStruct()));
	}

	if (KeyStruct.IsValid())
	{
		FMovieSceneVectorKeyStructBase* Struct = (FMovieSceneVectorKeyStructBase*)KeyStruct->GetStructMemory();
		for (int32 Index = 0; Index < ChannelsUsed; ++Index)
		{
			Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle<FMovieSceneFloatChannel>(Index), Struct->GetPropertyChannelByIndex(Index), KeyHandles));
		}

		Struct->KeyStructInterop.SetStartingValues();
		Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);
	}

	return KeyStruct;
}

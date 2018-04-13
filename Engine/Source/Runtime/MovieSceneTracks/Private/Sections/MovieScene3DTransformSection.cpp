// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScene3DTransformSection.h"
#include "UObject/StructOnScope.h"
#include "Evaluation/MovieScene3DTransformTemplate.h"
#include "UObject/SequencerObjectVersion.h"
#include "Algo/AnyOf.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "GameFramework/Actor.h"
#include "EulerTransform.h"

#if WITH_EDITOR

struct F3DTransformChannelEditorData
{
	F3DTransformChannelEditorData(EMovieSceneTransformChannel Mask)
	{
		FText LocationGroup = NSLOCTEXT("MovieSceneTransformSection", "Location", "Location");
		FText RotationGroup = NSLOCTEXT("MovieSceneTransformSection", "Rotation", "Rotation");
		FText ScaleGroup    = NSLOCTEXT("MovieSceneTransformSection", "Scale",    "Scale");
		{
			CommonData[0].SetIdentifiers("Location.X", FCommonChannelData::ChannelX, LocationGroup);
			CommonData[0].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::TranslationX);
			CommonData[0].Color = FCommonChannelData::RedChannelColor;
			CommonData[0].SortOrder = 0;

			CommonData[1].SetIdentifiers("Location.Y", FCommonChannelData::ChannelY, LocationGroup);
			CommonData[1].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::TranslationY);
			CommonData[1].Color = FCommonChannelData::GreenChannelColor;
			CommonData[1].SortOrder = 1;

			CommonData[2].SetIdentifiers("Location.Z", FCommonChannelData::ChannelZ, LocationGroup);
			CommonData[2].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::TranslationZ);
			CommonData[2].Color = FCommonChannelData::BlueChannelColor;
			CommonData[2].SortOrder = 2;
		}
		{
			CommonData[3].SetIdentifiers("Rotation.X", NSLOCTEXT("MovieSceneTransformSection", "RotationX", "Roll"), RotationGroup);
			CommonData[3].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::RotationX);
			CommonData[3].Color = FCommonChannelData::RedChannelColor;
			CommonData[3].SortOrder = 3;

			CommonData[4].SetIdentifiers("Rotation.Y", NSLOCTEXT("MovieSceneTransformSection", "RotationY", "Pitch"), RotationGroup);
			CommonData[4].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::RotationY);
			CommonData[4].Color = FCommonChannelData::GreenChannelColor;
			CommonData[4].SortOrder = 4;

			CommonData[5].SetIdentifiers("Rotation.Z", NSLOCTEXT("MovieSceneTransformSection", "RotationZ", "Yaw"), RotationGroup);
			CommonData[5].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::RotationZ);
			CommonData[5].Color = FCommonChannelData::BlueChannelColor;
			CommonData[5].SortOrder = 5;
		}
		{
			CommonData[6].SetIdentifiers("Scale.X", FCommonChannelData::ChannelX, ScaleGroup);
			CommonData[6].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::ScaleX);
			CommonData[6].Color = FCommonChannelData::RedChannelColor;
			CommonData[6].SortOrder = 6;

			CommonData[7].SetIdentifiers("Scale.Y", FCommonChannelData::ChannelY, ScaleGroup);
			CommonData[7].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::ScaleY);
			CommonData[7].Color = FCommonChannelData::GreenChannelColor;
			CommonData[7].SortOrder = 7;

			CommonData[8].SetIdentifiers("Scale.Z", FCommonChannelData::ChannelZ, ScaleGroup);
			CommonData[8].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::ScaleZ);
			CommonData[8].Color = FCommonChannelData::BlueChannelColor;
			CommonData[8].SortOrder = 8;
		}
		{
			CommonData[9].SetIdentifiers("Weight", NSLOCTEXT("MovieSceneTransformSection", "Weight", "Weight"));
			CommonData[9].bEnabled = EnumHasAllFlags(Mask, EMovieSceneTransformChannel::Weight);
		}

		ExternalValues[0].OnGetExternalValue = ExtractTranslationX;
		ExternalValues[1].OnGetExternalValue = ExtractTranslationY;
		ExternalValues[2].OnGetExternalValue = ExtractTranslationZ;
		ExternalValues[3].OnGetExternalValue = ExtractRotationX;
		ExternalValues[4].OnGetExternalValue = ExtractRotationY;
		ExternalValues[5].OnGetExternalValue = ExtractRotationZ;
		ExternalValues[6].OnGetExternalValue = ExtractScaleX;
		ExternalValues[7].OnGetExternalValue = ExtractScaleY;
		ExternalValues[8].OnGetExternalValue = ExtractScaleZ;
	}

	static TOptional<FVector> GetTranslation(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		const UStructProperty* TransformProperty = Bindings ? Cast<UStructProperty>(Bindings->GetProperty(InObject)) : nullptr;

		if (TransformProperty)
		{
			if (TransformProperty->Struct == TBaseStructure<FTransform>::Get())
			{
				if (TOptional<FTransform> Transform = Bindings->GetOptionalValue<FTransform>(InObject))
				{
					return Transform->GetTranslation();
				}
			}
			else if (TransformProperty->Struct == TBaseStructure<FEulerTransform>::Get())
			{
				if (TOptional<FEulerTransform> EulerTransform = Bindings->GetOptionalValue<FEulerTransform>(InObject))
				{
					return EulerTransform->Location;
				}
			}
		}
		else if (AActor* Actor = Cast<AActor>(&InObject))
		{
			if (USceneComponent* RootComponent = Actor->GetRootComponent())
			{
				return RootComponent->GetRelativeTransform().GetTranslation();
			}
		}

		return TOptional<FVector>();
	}

	static TOptional<FRotator> GetRotator(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		const UStructProperty* TransformProperty = Bindings ? Cast<UStructProperty>(Bindings->GetProperty(InObject)) : nullptr;

		if (TransformProperty)
		{
			if (TransformProperty->Struct == TBaseStructure<FTransform>::Get())
			{
				if (TOptional<FTransform> Transform = Bindings->GetOptionalValue<FTransform>(InObject))
				{
					return Transform->GetRotation().Rotator();
				}
			}
			else if (TransformProperty->Struct == TBaseStructure<FEulerTransform>::Get())
			{
				if (TOptional<FEulerTransform> EulerTransform = Bindings->GetOptionalValue<FEulerTransform>(InObject))
				{
					return EulerTransform->Rotation;
				}
			}
		}
		else if (AActor* Actor = Cast<AActor>(&InObject))
		{
			if (USceneComponent* RootComponent = Actor->GetRootComponent())
			{
				return RootComponent->RelativeRotation;
			}
		}

		return TOptional<FRotator>();
	}

	static TOptional<FVector> GetScale(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		const UStructProperty* TransformProperty = Bindings ? Cast<UStructProperty>(Bindings->GetProperty(InObject)) : nullptr;

		if (TransformProperty)
		{
			if (TransformProperty->Struct == TBaseStructure<FTransform>::Get())
			{
				if (TOptional<FTransform> Transform = Bindings->GetOptionalValue<FTransform>(InObject))
				{
					return Transform->GetScale3D();
				}
			}
			else if (TransformProperty->Struct == TBaseStructure<FEulerTransform>::Get())
			{
				if (TOptional<FEulerTransform> EulerTransform = Bindings->GetOptionalValue<FEulerTransform>(InObject))
				{
					return EulerTransform->Scale;
				}
			}
		}
		else if (AActor* Actor = Cast<AActor>(&InObject))
		{
			if (USceneComponent* RootComponent = Actor->GetRootComponent())
			{
				return RootComponent->GetRelativeTransform().GetScale3D();
			}
		}

		return TOptional<FVector>();
	}

	static TOptional<float> ExtractTranslationX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FVector> Translation = GetTranslation(InObject, Bindings);
		return Translation.IsSet() ? Translation->X : TOptional<float>();
	}
	static TOptional<float> ExtractTranslationY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FVector> Translation = GetTranslation(InObject, Bindings);
		return Translation.IsSet() ? Translation->Y : TOptional<float>();
	}
	static TOptional<float> ExtractTranslationZ(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FVector> Translation = GetTranslation(InObject, Bindings);
		return Translation.IsSet() ? Translation->Z : TOptional<float>();
	}

	static TOptional<float> ExtractRotationX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FRotator> Rotator = GetRotator(InObject, Bindings);
		return Rotator.IsSet() ? Rotator->Roll : TOptional<float>();
	}
	static TOptional<float> ExtractRotationY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FRotator> Rotator = GetRotator(InObject, Bindings);
		return Rotator.IsSet() ? Rotator->Pitch : TOptional<float>();
	}
	static TOptional<float> ExtractRotationZ(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FRotator> Rotator = GetRotator(InObject, Bindings);
		return Rotator.IsSet() ? Rotator->Yaw : TOptional<float>();
	}

	static TOptional<float> ExtractScaleX(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FVector> Scale = GetScale(InObject, Bindings);
		return Scale.IsSet() ? Scale->X : TOptional<float>();
	}
	static TOptional<float> ExtractScaleY(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FVector> Scale = GetScale(InObject, Bindings);
		return Scale.IsSet() ? Scale->Y : TOptional<float>();
	}
	static TOptional<float> ExtractScaleZ(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		TOptional<FVector> Scale = GetScale(InObject, Bindings);
		return Scale.IsSet() ? Scale->Z : TOptional<float>();
	}

	FMovieSceneChannelEditorData    CommonData[10];
	TMovieSceneExternalValue<float> ExternalValues[10];
};

#endif // WITH_EDITOR



/* FMovieScene3DLocationKeyStruct interface
 *****************************************************************************/

void FMovieScene3DLocationKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* FMovieScene3DRotationKeyStruct interface
 *****************************************************************************/

void FMovieScene3DRotationKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* FMovieScene3DScaleKeyStruct interface
 *****************************************************************************/

void FMovieScene3DScaleKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* FMovieScene3DTransformKeyStruct interface
 *****************************************************************************/

void FMovieScene3DTransformKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	KeyStructInterop.Apply(Time);
}


/* UMovieScene3DTransformSection interface
 *****************************************************************************/

UMovieScene3DTransformSection::UMovieScene3DTransformSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, Show3DTrajectory(EShow3DTrajectory::EST_OnlyWhenSelected)
#endif
{
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

	ProxyChannels = EMovieSceneTransformChannel::None;
	TransformMask = EMovieSceneTransformChannel::AllTransform;
	BlendType = EMovieSceneBlendType::Absolute;
	bSupportsInfiniteRange = true;

	UpdateChannelProxy();
}

void UMovieScene3DTransformSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		UpdateChannelProxy();
	}
}

FMovieSceneTransformMask UMovieScene3DTransformSection::GetMask() const
{
	return TransformMask;
}

void UMovieScene3DTransformSection::SetMask(FMovieSceneTransformMask NewMask)
{
	TransformMask = NewMask;
	UpdateChannelProxy();
}

void UMovieScene3DTransformSection::UpdateChannelProxy()
{
	if (ProxyChannels == TransformMask.GetChannels())
	{
		return;
	}

	ProxyChannels = TransformMask.GetChannels();

	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	F3DTransformChannelEditorData EditorData(TransformMask.GetChannels());

	Channels.Add(Translation[0], EditorData.CommonData[0], EditorData.ExternalValues[0]);
	Channels.Add(Translation[1], EditorData.CommonData[1], EditorData.ExternalValues[1]);
	Channels.Add(Translation[2], EditorData.CommonData[2], EditorData.ExternalValues[2]);
	Channels.Add(Rotation[0],    EditorData.CommonData[3], EditorData.ExternalValues[3]);
	Channels.Add(Rotation[1],    EditorData.CommonData[4], EditorData.ExternalValues[4]);
	Channels.Add(Rotation[2],    EditorData.CommonData[5], EditorData.ExternalValues[5]);
	Channels.Add(Scale[0],       EditorData.CommonData[6], EditorData.ExternalValues[6]);
	Channels.Add(Scale[1],       EditorData.CommonData[7], EditorData.ExternalValues[7]);
	Channels.Add(Scale[2],       EditorData.CommonData[8], EditorData.ExternalValues[8]);
	Channels.Add(ManualWeight,   EditorData.CommonData[9], EditorData.ExternalValues[9]);

#else

	Channels.Add(Translation[0]);
	Channels.Add(Translation[1]);
	Channels.Add(Translation[2]);
	Channels.Add(Rotation[0]);
	Channels.Add(Rotation[1]);
	Channels.Add(Rotation[2]);
	Channels.Add(Scale[0]);
	Channels.Add(Scale[1]);
	Channels.Add(Scale[2]);
	Channels.Add(ManualWeight);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

/* UMovieSceneSection interface
 *****************************************************************************/


TSharedPtr<FStructOnScope> UMovieScene3DTransformSection::GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles)
{
	FVector  StartingLocation;
	FRotator StartingRotation;
	FVector  StartingScale;

	TArrayView<FMovieSceneFloatChannel*> FloatChannels = ChannelProxy->GetChannels<FMovieSceneFloatChannel>();

	TOptional<TTuple<FKeyHandle, FFrameNumber>> LocationKeys[3] = {
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[0], KeyHandles),
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[1], KeyHandles),
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[2], KeyHandles)
	};

	TOptional<TTuple<FKeyHandle, FFrameNumber>> RotationKeys[3] = {
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[3], KeyHandles),
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[4], KeyHandles),
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[5], KeyHandles)
	};

	TOptional<TTuple<FKeyHandle, FFrameNumber>> ScaleKeys[3] = {
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[6], KeyHandles),
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[7], KeyHandles),
		FMovieSceneChannelValueHelper::FindFirstKey(FloatChannels[8], KeyHandles)
	};

	const int32 AnyLocationKeys = Algo::AnyOf(LocationKeys);
	const int32 AnyRotationKeys = Algo::AnyOf(RotationKeys);
	const int32 AnyScaleKeys =    Algo::AnyOf(ScaleKeys);

	// do we have multiple keys on multiple parts of the transform?
	if (AnyLocationKeys + AnyRotationKeys + AnyScaleKeys > 1)
	{
		TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieScene3DTransformKeyStruct::StaticStruct()));
		auto Struct = (FMovieScene3DTransformKeyStruct*)KeyStruct->GetStructMemory();

		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[0]), &Struct->Location.X,     LocationKeys[0]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[1]), &Struct->Location.Y,     LocationKeys[1]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[2]), &Struct->Location.Z,     LocationKeys[2]));

		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[3]), &Struct->Rotation.Roll,  RotationKeys[0]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[4]), &Struct->Rotation.Pitch, RotationKeys[1]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[5]), &Struct->Rotation.Yaw,   RotationKeys[2]));

		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[6]), &Struct->Scale.X,        ScaleKeys[0]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[7]), &Struct->Scale.Y,        ScaleKeys[1]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[8]), &Struct->Scale.Z,        ScaleKeys[2]));

		Struct->KeyStructInterop.SetStartingValues();
		Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);
		return KeyStruct;
	}

	if (AnyLocationKeys > 1)
	{
		TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieScene3DLocationKeyStruct::StaticStruct()));
		auto Struct = (FMovieScene3DLocationKeyStruct*)KeyStruct->GetStructMemory();

		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[0]), &Struct->Location.X,     LocationKeys[0]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[1]), &Struct->Location.Y,     LocationKeys[1]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[2]), &Struct->Location.Z,     LocationKeys[2]));

		Struct->KeyStructInterop.SetStartingValues();
		Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);
		return KeyStruct;
	}

	if (AnyRotationKeys)
	{
		TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieScene3DRotationKeyStruct::StaticStruct()));
		auto Struct = (FMovieScene3DRotationKeyStruct*)KeyStruct->GetStructMemory();

		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[3]), &Struct->Rotation.Roll,  RotationKeys[0]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[4]), &Struct->Rotation.Pitch, RotationKeys[1]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[5]), &Struct->Rotation.Yaw,   RotationKeys[2]));

		Struct->KeyStructInterop.SetStartingValues();
		Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);
		return KeyStruct;
	}

	if (AnyScaleKeys)
	{
		TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieScene3DScaleKeyStruct::StaticStruct()));
		auto Struct = (FMovieScene3DScaleKeyStruct*)KeyStruct->GetStructMemory();

		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[6]), &Struct->Scale.X,        ScaleKeys[0]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[7]), &Struct->Scale.Y,        ScaleKeys[1]));
		Struct->KeyStructInterop.Add(FMovieSceneChannelValueHelper(ChannelProxy->MakeHandle(FloatChannels[8]), &Struct->Scale.Z,        ScaleKeys[2]));

		Struct->KeyStructInterop.SetStartingValues();
		Struct->Time = Struct->KeyStructInterop.GetUnifiedKeyTime().Get(0);
		return KeyStruct;
	}

	return nullptr;
}

FMovieSceneEvalTemplatePtr UMovieScene3DTransformSection::GenerateTemplate() const
{
	return FMovieSceneComponentTransformSectionTemplate(*this);
}

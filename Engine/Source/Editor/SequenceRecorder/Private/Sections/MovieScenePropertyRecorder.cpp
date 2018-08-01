// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScenePropertyRecorder.h"
#include "MovieScene.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Sections/MovieSceneByteSection.h"
#include "Tracks/MovieSceneByteTrack.h"
#include "Sections/MovieSceneEnumSection.h"
#include "Tracks/MovieSceneEnumTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneColorSection.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "SequenceRecorderUtils.h"

// current set of compiled-in property types

template <>
bool FMovieScenePropertyRecorder<bool>::ShouldAddNewKey(const bool& InNewValue) const
{
	return InNewValue != PreviousValue;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<bool>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	if (!InObjectToRecord)
	{
		return nullptr;
	}

	FString TrackDisplayName = *Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString();
	FName TrackName = *Binding.GetPropertyPath();
	UMovieSceneBoolTrack* Track = InMovieScene->FindTrack<UMovieSceneBoolTrack>(InGuid, TrackName);
	if (!Track)
	{
		Track = InMovieScene->AddTrack<UMovieSceneBoolTrack>(InGuid);
	}
	else
	{
		Track->RemoveAllAnimationData();
	}

	if (Track)
	{
		Track->SetPropertyNameAndPath(*TrackDisplayName, Binding.GetPropertyPath());

		UMovieSceneBoolSection* Section         = Cast<UMovieSceneBoolSection>(Track->CreateNewSection());

		FFrameRate   TickResolution  = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber CurrentFrame    = (InTime * TickResolution).FloorToFrame();

		Section->SetRange(TRange<FFrameNumber>(CurrentFrame));

		Section->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

		FMovieSceneBoolChannel* BoolChannel = Section->GetChannelProxy().GetChannels<FMovieSceneBoolChannel>()[0];
		BoolChannel->SetDefault(PreviousValue);
		BoolChannel->GetData().AddKey(CurrentFrame, PreviousValue);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<bool>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<bool>& InKey)
{
	InSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0)->GetData().AddKey(InKey.Time, InKey.Value);
}

template <>
void FMovieScenePropertyRecorder<bool>::ReduceKeys(UMovieSceneSection* InSection)
{
}

template <>
bool FMovieScenePropertyRecorder<uint8>::ShouldAddNewKey(const uint8& InNewValue) const
{
	return InNewValue != PreviousValue;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<uint8>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	if (!InObjectToRecord)
	{
		return nullptr;
	}

	FString TrackDisplayName = *Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString();
	FName TrackName = *Binding.GetPropertyPath();
	UMovieSceneByteTrack* Track = InMovieScene->FindTrack<UMovieSceneByteTrack>(InGuid, TrackName);
	if (!Track)
	{
		Track = InMovieScene->AddTrack<UMovieSceneByteTrack>(InGuid);
	}
	else
	{
		Track->RemoveAllAnimationData();
	}

	if (Track)
	{
		Track->SetPropertyNameAndPath(*TrackDisplayName, Binding.GetPropertyPath());

		UMovieSceneByteSection* Section = Cast<UMovieSceneByteSection>(Track->CreateNewSection());

		FFrameRate   TickResolution  = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber CurrentFrame    = (InTime * TickResolution).FloorToFrame();

		Section->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

		Section->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

		FMovieSceneByteChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneByteChannel>(0);
		Channel->SetDefault(PreviousValue);
		Channel->GetData().AddKey(CurrentFrame, PreviousValue);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<uint8>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<uint8>& InKey)
{
	InSection->GetChannelProxy().GetChannel<FMovieSceneByteChannel>(0)->GetData().AddKey(InKey.Time, InKey.Value);
}

template <>
void FMovieScenePropertyRecorder<uint8>::ReduceKeys(UMovieSceneSection* InSection)
{
}

bool FMovieScenePropertyRecorderEnum::ShouldAddNewKey(const int64& InNewValue) const
{
	return InNewValue != PreviousValue;
}

UMovieSceneSection* FMovieScenePropertyRecorderEnum::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	if (!InObjectToRecord)
	{
		return nullptr;
	}

	FString TrackDisplayName = *Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString();
	FName TrackName = *Binding.GetPropertyPath();
	UMovieSceneEnumTrack* Track = InMovieScene->FindTrack<UMovieSceneEnumTrack>(InGuid, TrackName);
	if (!Track)
	{
		Track = InMovieScene->AddTrack<UMovieSceneEnumTrack>(InGuid);
	}
	else
	{
		Track->RemoveAllAnimationData();
	}

	if (Track)
	{
		Track->SetPropertyNameAndPath(*TrackDisplayName, Binding.GetPropertyPath());

		UMovieSceneEnumSection* Section = Cast<UMovieSceneEnumSection>(Track->CreateNewSection());

		FFrameRate   TickResolution  = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber CurrentFrame    = (InTime * TickResolution).FloorToFrame();

		Section->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

		Section->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

		FMovieSceneByteChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneByteChannel>(0);
		Channel->SetDefault(PreviousValue);
		Channel->GetData().AddKey(CurrentFrame, PreviousValue);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

void FMovieScenePropertyRecorderEnum::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<int64>& InKey)
{
	InSection->GetChannelProxy().GetChannel<FMovieSceneByteChannel>(0)->GetData().AddKey(InKey.Time, InKey.Value);
}

void FMovieScenePropertyRecorderEnum::ReduceKeys(UMovieSceneSection* InSection)
{
}

template <>
bool FMovieScenePropertyRecorder<float>::ShouldAddNewKey(const float& InNewValue) const
{
	return true;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<float>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	if (!InObjectToRecord)
	{
		return nullptr;
	}

	FString TrackDisplayName = *Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString();
	FName TrackName = *Binding.GetPropertyPath();
	UMovieSceneFloatTrack* Track = InMovieScene->FindTrack<UMovieSceneFloatTrack>(InGuid, TrackName);
	if (!Track)
	{
		Track = InMovieScene->AddTrack<UMovieSceneFloatTrack>(InGuid);
	}
	else
	{
		Track->RemoveAllAnimationData();
	}

	if (Track)
	{
		Track->SetPropertyNameAndPath(*TrackDisplayName, Binding.GetPropertyPath());

		UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());

		FFrameRate   TickResolution  = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber CurrentFrame    = (InTime * TickResolution).FloorToFrame();

		Section->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

		Section->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

		FMovieSceneFloatChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
		check(Channel);
		Channel->SetDefault(PreviousValue);
		Channel->AddCubicKey(CurrentFrame, PreviousValue, RCTM_Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<float>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<float>& InKey)
{
	InSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0)->AddCubicKey(InKey.Time, InKey.Value);
}

template <>
void FMovieScenePropertyRecorder<float>::ReduceKeys(UMovieSceneSection* InSection)
{
	FKeyDataOptimizationParams Params;
	MovieScene::Optimize(InSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0), Params);
}

template <>
bool FMovieScenePropertyRecorder<FColor>::ShouldAddNewKey(const FColor& InNewValue) const
{
	return true;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<FColor>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	if (!InObjectToRecord)
	{
		return nullptr;
	}

	FString TrackDisplayName = *Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString();
	FName TrackName = *Binding.GetPropertyPath();
	UMovieSceneColorTrack* Track = InMovieScene->FindTrack<UMovieSceneColorTrack>(InGuid, TrackName);
	if (!Track)
	{
		Track = InMovieScene->AddTrack<UMovieSceneColorTrack>(InGuid);
	}
	else
	{
		Track->RemoveAllAnimationData();
	}

	if (Track)
	{
		Track->SetPropertyNameAndPath(*TrackDisplayName, Binding.GetPropertyPath());

		UMovieSceneColorSection* Section = Cast<UMovieSceneColorSection>(Track->CreateNewSection());

		FFrameRate   TickResolution  = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber CurrentFrame    = (InTime * TickResolution).FloorToFrame();

		Section->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

		Section->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

		FloatChannels[0]->SetDefault(PreviousValue.R);
		FloatChannels[0]->AddCubicKey(CurrentFrame, PreviousValue.R, RCTM_Break);

		FloatChannels[1]->SetDefault(PreviousValue.G);
		FloatChannels[1]->AddCubicKey(CurrentFrame, PreviousValue.G, RCTM_Break);

		FloatChannels[2]->SetDefault(PreviousValue.B);
		FloatChannels[2]->AddCubicKey(CurrentFrame, PreviousValue.B, RCTM_Break);

		FloatChannels[3]->SetDefault(PreviousValue.A);
		FloatChannels[3]->AddCubicKey(CurrentFrame, PreviousValue.A, RCTM_Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<FColor>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<FColor>& InKey)
{
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = InSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
	FloatChannels[0]->AddCubicKey(InKey.Time, InKey.Value.R);
	FloatChannels[1]->AddCubicKey(InKey.Time, InKey.Value.G);
	FloatChannels[2]->AddCubicKey(InKey.Time, InKey.Value.B);
	FloatChannels[3]->AddCubicKey(InKey.Time, InKey.Value.A);
}

template <>
void FMovieScenePropertyRecorder<FColor>::ReduceKeys(UMovieSceneSection* InSection)
{
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = InSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

	FKeyDataOptimizationParams Params;
	MovieScene::Optimize(FloatChannels[0], Params);
	MovieScene::Optimize(FloatChannels[1], Params);
	MovieScene::Optimize(FloatChannels[2], Params);
	MovieScene::Optimize(FloatChannels[3], Params);
}

template <>
bool FMovieScenePropertyRecorder<FVector>::ShouldAddNewKey(const FVector& InNewValue) const
{
	return true;
}

template <>
UMovieSceneSection* FMovieScenePropertyRecorder<FVector>::AddSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& InGuid, float InTime)
{
	if (!InObjectToRecord)
	{
		return nullptr;
	}

	FString TrackDisplayName = *Binding.GetProperty(*InObjectToRecord)->GetDisplayNameText().ToString();
	FName TrackName = *Binding.GetPropertyPath();
	UMovieSceneVectorTrack* Track = InMovieScene->FindTrack<UMovieSceneVectorTrack>(InGuid, TrackName);
	if (!Track)
	{
		Track = InMovieScene->AddTrack<UMovieSceneVectorTrack>(InGuid);
	}
	else
	{
		Track->RemoveAllAnimationData();
	}

	if (Track)
	{
		Track->SetNumChannelsUsed(3);
		Track->SetPropertyNameAndPath(*TrackDisplayName, Binding.GetPropertyPath());

		UMovieSceneVectorSection* Section = Cast<UMovieSceneVectorSection>(Track->CreateNewSection());

		FFrameRate   TickResolution  = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber CurrentFrame    = (InTime * TickResolution).FloorToFrame();

		Section->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

		Section->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

		FloatChannels[0]->SetDefault(PreviousValue.X);
		FloatChannels[0]->AddCubicKey(CurrentFrame, PreviousValue.X, RCTM_Break);

		FloatChannels[1]->SetDefault(PreviousValue.Y);
		FloatChannels[1]->AddCubicKey(CurrentFrame, PreviousValue.Y, RCTM_Break);

		FloatChannels[2]->SetDefault(PreviousValue.Z);
		FloatChannels[2]->AddCubicKey(CurrentFrame, PreviousValue.Z, RCTM_Break);

		Track->AddSection(*Section);

		return Section;
	}

	return nullptr;
}

template <>
void FMovieScenePropertyRecorder<FVector>::AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<FVector>& InKey)
{
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = InSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
	FloatChannels[0]->AddCubicKey(InKey.Time, InKey.Value.X);
	FloatChannels[1]->AddCubicKey(InKey.Time, InKey.Value.Y);
	FloatChannels[2]->AddCubicKey(InKey.Time, InKey.Value.Z);
}

template <>
void FMovieScenePropertyRecorder<FVector>::ReduceKeys(UMovieSceneSection* InSection)
{
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = InSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

	FKeyDataOptimizationParams Params;
	MovieScene::Optimize(FloatChannels[0], Params);
	MovieScene::Optimize(FloatChannels[1], Params);
	MovieScene::Optimize(FloatChannels[2], Params);
}

template class FMovieScenePropertyRecorder<bool>;
template class FMovieScenePropertyRecorder<uint8>;
template class FMovieScenePropertyRecorder<float>;
template class FMovieScenePropertyRecorder<FColor>;
template class FMovieScenePropertyRecorder<FVector>;

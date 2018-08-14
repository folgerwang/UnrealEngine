// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "LiveLinkTypes.h"
#include "MovieSceneCommonHelpers.h"
#define LOCTEXT_NAMESPACE "MovieSceneLiveLinkSection"

static const TArray<FString> StringArray = {
	"Translation-X",
	"Translation-Y",
	"Translation-Z",
	"Rotation-X",
	"Rotation-Y",
	"Rotation-Z",
	"Scale-X",
	"Scale-Y",
	"Scale-Z"
};

UMovieSceneLiveLinkSection::UMovieSceneLiveLinkSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	BlendType = EMovieSceneBlendType::Absolute;


}
void UMovieSceneLiveLinkSection::SetSubjectName(const FName& InSubjectName)
{
	SubjectName = InSubjectName;
}
//This is called when first creatd.
int32 UMovieSceneLiveLinkSection::CreateChannelProxy(const FLiveLinkFrame &Frame, const FLiveLinkRefSkeleton& InRefSkeleton,const  FLiveLinkCurveKey& CurveKey)
{
	FMovieSceneChannelProxyData Channels;
	int32 ChannelIndex = 0;
	int32 TransformIndex = 0;
	int32 CurveIndex = 0;
	RefSkeleton = InRefSkeleton;
	TemplateToPush.Transforms.Reserve(Frame.Transforms.Num());
	TemplateToPush.CurveElements.Reserve(Frame.Curves.Num());
	PropertyFloatChannels.SetNum(Frame.Transforms.Num() * 9 + Frame.Curves.Num());
	for (const FTransform& Transform : Frame.Transforms)
	{
		TemplateToPush.Transforms.Add(FTransform());
#if WITH_EDITOR
		for (FString String : StringArray)
		{
			FText DisplayName = FText::Format(LOCTEXT("LinkLinkCurveFormat", " Transform {0} - {1}"),
				FText::AsNumber(TransformIndex),
				FText::FromString(String));
			FMovieSceneChannelMetaData ChannelEditorData(FName(*(DisplayName.ToString())), DisplayName);
			ChannelEditorData.SortOrder = ChannelIndex;
			ChannelEditorData.bCanCollapseToTrack = false;
			Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
		}
#else
		for (int32 i = 0; i < 9; ++i)
		{
			Channels.Add(PropertyFloatChannels[ChannelIndex++]);
		}
#endif
		++TransformIndex;

	}

	for (const FOptionalCurveElement& Curve : Frame.Curves)
	{
		FLiveLinkCurveElement CurveElement;
		CurveElement.CurveName = CurveKey.CurveNames[CurveIndex];
		TemplateToPush.CurveElements.Add(CurveElement);
#if WITH_EDITOR
		FMovieSceneChannelMetaData ChannelEditorData(CurveElement.CurveName, FText::FromName(CurveElement.CurveName));
		ChannelEditorData.SortOrder = ChannelIndex;
		ChannelEditorData.bCanCollapseToTrack = false;
		Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
#else

		Channels.Add(PropertyFloatChannels[ChannelIndex++]);

#endif
		++CurveIndex;

	}
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
	return ChannelIndex;
}

//This is called on load.
void UMovieSceneLiveLinkSection::UpdateChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

	int ChannelIndex = 0;
	int TransformIndex = 0;
	int CurveIndex = 0;
	for (FTransform Transform : TemplateToPush.Transforms)
	{
#if WITH_EDITOR
		for (const FString& String : StringArray)
		{
			FText DisplayName = FText::Format(LOCTEXT("LinkLinkCurveFormat", " Transform {0} - {1}"),
				FText::AsNumber(TransformIndex),
				FText::FromString(String));
			FMovieSceneChannelMetaData ChannelEditorData(FName(*(DisplayName.ToString())), DisplayName);
			ChannelEditorData.SortOrder = ChannelIndex;
			ChannelEditorData.bCanCollapseToTrack = false;
			Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
		}
#else
		for (int32 i = 0; i < 9; ++i)
		{
			Channels.Add(PropertyFloatChannels[ChannelIndex++]);
		}
#endif
		++TransformIndex;

	}
	for (const FLiveLinkCurveElement& CurveElement : TemplateToPush.CurveElements)
	{
#if WITH_EDITOR
		FMovieSceneChannelMetaData ChannelEditorData(CurveElement.CurveName, FText::FromName(CurveElement.CurveName));
		ChannelEditorData.SortOrder = ChannelIndex;
		ChannelEditorData.bCanCollapseToTrack = false;
		Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
#else
		Channels.Add(PropertyFloatChannels[ChannelIndex++]);

#endif
		++CurveIndex;

	}
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}


void UMovieSceneLiveLinkSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		UpdateChannelProxy();
	}
}
#undef LOCTEXT_NAMESPACE // MovieSceneNiagaraEmitterTimedSection
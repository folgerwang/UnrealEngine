// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
int32 UMovieSceneLiveLinkSection::CreateChannelProxy( const FLiveLinkRefSkeleton& InRefSkeleton,const TArray<FName>& InCurveNames)
{
	FMovieSceneChannelProxyData Channels;
	int32 ChannelIndex = 0;
	RefSkeleton = InRefSkeleton;
	CurveNames = InCurveNames;
	TArray<FName> BoneNames = RefSkeleton.GetBoneNames();

	TemplateToPush.Transforms.Reserve(BoneNames.Num());
	TemplateToPush.CurveElements.Reserve(CurveNames.Num());
	PropertyFloatChannels.SetNum(BoneNames.Num() * 9 + CurveNames.Num());
	ChannelMask.SetNum(PropertyFloatChannels.Num());
	for (const FName& BoneName : BoneNames)
	{
		TemplateToPush.Transforms.Add(FTransform());
#if WITH_EDITOR
		for (FString String : StringArray)
		{
			FText DisplayName = FText::Format(LOCTEXT("LinkLinkCurveFormat", "{0} : {1}"),
				FText::FromName(BoneName),
				FText::FromString(String));
			FMovieSceneChannelMetaData ChannelEditorData(FName(*(DisplayName.ToString())), DisplayName);
			ChannelEditorData.SortOrder = ChannelIndex;
			ChannelEditorData.bCanCollapseToTrack = false;
			ChannelMask[ChannelIndex] = true;
			ChannelEditorData.bEnabled = true;
			Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
		}
#else
		for (int32 i = 0; i < 9; ++i)
		{
			Channels.Add(PropertyFloatChannels[ChannelIndex++]);
		}
#endif

	}

	for (const FName& CurveName : CurveNames)
	{
		FLiveLinkCurveElement CurveElement;
		CurveElement.CurveName = CurveName;
		TemplateToPush.CurveElements.Add(CurveElement);
#if WITH_EDITOR
		FMovieSceneChannelMetaData ChannelEditorData(CurveElement.CurveName, FText::FromName(CurveElement.CurveName));
		ChannelEditorData.SortOrder = ChannelIndex;
		ChannelEditorData.bCanCollapseToTrack = false;
		ChannelMask[ChannelIndex] = true;
		ChannelEditorData.bEnabled = true;
		Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
#else

		Channels.Add(PropertyFloatChannels[ChannelIndex++]);

#endif
	}

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
	return ChannelIndex;
}

//This is called on load.
void UMovieSceneLiveLinkSection::UpdateChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

	int ChannelIndex = 0;
	TArray<FName> BoneNames = RefSkeleton.GetBoneNames();
	for (const FName& BoneName : BoneNames)
	{
		
#if WITH_EDITOR
		for (const FString& String : StringArray)
		{
			FText DisplayName = FText::Format(LOCTEXT("LinkLinkCurveFormat", "{0} : {1}"),
				FText::FromName(BoneName),
				FText::FromString(String));
			FMovieSceneChannelMetaData ChannelEditorData(FName(*(DisplayName.ToString())), DisplayName);
			ChannelEditorData.SortOrder = ChannelIndex;
			ChannelEditorData.bCanCollapseToTrack = false;
			ChannelEditorData.bEnabled = ChannelMask[ChannelIndex];
			Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
		}
#else
		for (int32 i = 0; i < 9; ++i)
		{
			Channels.Add(PropertyFloatChannels[ChannelIndex++]);
		}
#endif

	}
	for (const FName& CurveName : CurveNames)
	{
#if WITH_EDITOR
		FMovieSceneChannelMetaData ChannelEditorData(CurveName, FText::FromName(CurveName));
		ChannelEditorData.SortOrder = ChannelIndex;
		ChannelEditorData.bCanCollapseToTrack = false;
		ChannelEditorData.bEnabled = ChannelMask[ChannelIndex];
		Channels.Add(PropertyFloatChannels[ChannelIndex++], ChannelEditorData, TMovieSceneExternalValue<float>());
#else
		Channels.Add(PropertyFloatChannels[ChannelIndex++]);

#endif

	}
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}


void UMovieSceneLiveLinkSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		//Fix for asset saved before Channel Mask
		if(ChannelMask.Num() != PropertyFloatChannels.Num())
		{
			ChannelMask.SetNum(PropertyFloatChannels.Num());
			ChannelMask.Init(true, PropertyFloatChannels.Num());
		}
		UpdateChannelProxy();
	}
}

void UMovieSceneLiveLinkSection::PostEditImport()
{
	Super::PostEditImport();

	UpdateChannelProxy();
}

void UMovieSceneLiveLinkSection::SetMask(const TArray<bool>& InChannelMask)
{
	ChannelMask = InChannelMask;
	UpdateChannelProxy();
	//MattH todo set priorities or whatever based upon mask

}

#undef LOCTEXT_NAMESPACE // MovieSceneNiagaraEmitterTimedSection
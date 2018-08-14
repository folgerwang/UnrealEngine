// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneLiveLinkSectionTemplate.h"
#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "MovieSceneLiveLinkSource.h"
#include "Channels/MovieSceneChannelProxy.h"

FMovieSceneLiveLinkTemplateData::FMovieSceneLiveLinkTemplateData(const UMovieSceneLiveLinkSection& Section)
{
	TArrayView<FMovieSceneFloatChannel*> ProxyFloatChannels = Section.GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
	FloatChannels.SetNum(ProxyFloatChannels.Num());
	for (int32 Index = 0; Index < ProxyFloatChannels.Num(); ++Index)
	{
		FloatChannels[Index] = *ProxyFloatChannels[Index];
	}
	TemplateToPush = Section.TemplateToPush;
	SubjectName = Section.SubjectName;
	RefSkeleton = Section.RefSkeleton;
}

bool FMovieSceneLiveLinkTemplateData::GetLiveLinkFrameArray(FFrameTime FrameTime, TArray<FLiveLinkFrameData>&  LiveLinkFrameDataArray) const
{
	static FLiveLinkFrameData FrameData; //NOTE if we send more then one frame this won't work.
	FrameData.Transforms.Reset(TemplateToPush.Transforms.Num());
	FrameData.CurveElements.Reset(TemplateToPush.CurveElements.Num());
	FrameData.WorldTime = FLiveLinkWorldTime(); //this calls FPlatform::Seconds()
	int32 ChannelIndex = 0;
	FVector Vector;
	for (FTransform Transform : TemplateToPush.Transforms)
	{
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.X);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.Y);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.Z);
		Transform.SetLocation(Vector);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.X);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.Y);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.Z);
		FRotator Rotator(Vector.Y, Vector.Z, Vector.X); //pitch, yaw, roll
		FQuat Quat = Rotator.Quaternion();
		//mz todo handle flips somehow if needed. 
		Transform.SetRotation(Quat);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.X);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.Y);
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, Vector.Z);
		Transform.SetScale3D(Vector);
		FrameData.Transforms.Add(Transform);
	}
	for (FLiveLinkCurveElement CurveElement : TemplateToPush.CurveElements)
	{
		FloatChannels[ChannelIndex++].Evaluate(FrameTime, CurveElement.CurveValue);
		FrameData.CurveElements.Add(CurveElement);
	}

	LiveLinkFrameDataArray.Add(FrameData);

	return true;
}
FMovieSceneLiveLinkSectionTemplate::FMovieSceneLiveLinkSectionTemplate(const UMovieSceneLiveLinkSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath()),  TemplateData(Section)
{

}
struct FMovieSceneLiveLinkSectionTemplatePersistentData : IPersistentEvaluationData
{
	TSharedPtr<FMovieSceneLiveLinkSource> LiveLinkSource;
};
void FMovieSceneLiveLinkSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FMovieSceneLiveLinkSectionTemplatePersistentData* Data = PersistentData.FindSectionData<FMovieSceneLiveLinkSectionTemplatePersistentData>();
	if (Data && Data->LiveLinkSource.IsValid() && Data->LiveLinkSource->IsSourceStillValid())
	{
		TArray<FLiveLinkFrameData>  LiveLinkFrameDataArray;
		FFrameTime FrameTime = Context.GetTime();

		TemplateData.GetLiveLinkFrameArray(FrameTime, LiveLinkFrameDataArray);
		Data->LiveLinkSource->PublishLiveLinkFrameData(TemplateData.SubjectName, LiveLinkFrameDataArray,TemplateData.RefSkeleton);
	}

}

void FMovieSceneLiveLinkSectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	PersistentData.GetOrAddSectionData<FMovieSceneLiveLinkSectionTemplatePersistentData>().LiveLinkSource = FMovieSceneLiveLinkSource::CreateLiveLinkSource();


}
void FMovieSceneLiveLinkSectionTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FMovieSceneLiveLinkSectionTemplatePersistentData* Data = PersistentData.FindSectionData<FMovieSceneLiveLinkSectionTemplatePersistentData>();

	if (Data && Data->LiveLinkSource.IsValid())
	{
		if ((Data->LiveLinkSource)->IsSourceStillValid())
		{
			FMovieSceneLiveLinkSource::RemoveLiveLinkSource(Data->LiveLinkSource);
		}	
		Data->LiveLinkSource.Reset();
	}
}
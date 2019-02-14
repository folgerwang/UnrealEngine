// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

#if WITH_EDITOR

struct FMarginSectionEditorData
{
	FMarginSectionEditorData()
	{
		MetaData[0].SetIdentifiers("Left", NSLOCTEXT("MovieSceneMarginSection", "LeftText", "Left"));
		MetaData[0].SortOrder = 0;
		MetaData[0].bCanCollapseToTrack = false;

		MetaData[1].SetIdentifiers("Top", NSLOCTEXT("MovieSceneMarginSection", "TopText", "Top"));
		MetaData[1].SortOrder = 1;
		MetaData[1].bCanCollapseToTrack = false;

		MetaData[2].SetIdentifiers("Right", NSLOCTEXT("MovieSceneMarginSection", "RightText", "Right"));
		MetaData[2].SortOrder = 2;
		MetaData[2].bCanCollapseToTrack = false;

		MetaData[3].SetIdentifiers("Bottom", NSLOCTEXT("MovieSceneMarginSection", "BottomText", "Bottom"));
		MetaData[3].SortOrder = 3;
		MetaData[3].bCanCollapseToTrack = false;

		ExternalValues[0].OnGetExternalValue = ExtractLeftChannel;
		ExternalValues[1].OnGetExternalValue = ExtractTopChannel;
		ExternalValues[2].OnGetExternalValue = ExtractRightChannel;
		ExternalValues[3].OnGetExternalValue = ExtractBottomChannel;

		ExternalValues[0].OnGetCurrentValueAndWeight = GetLeftChannelValueAndWeight;
		ExternalValues[1].OnGetCurrentValueAndWeight = GetTopChannelValueAndWeight;
		ExternalValues[2].OnGetCurrentValueAndWeight = GetRightChannelValueAndWeight;
		ExternalValues[3].OnGetCurrentValueAndWeight = GetBottomChannelValueAndWeight;
	}

	static TOptional<float> ExtractLeftChannel(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FMargin>(InObject).Left : TOptional<float>();
	}
	static TOptional<float> ExtractTopChannel(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FMargin>(InObject).Top : TOptional<float>();
	}
	static TOptional<float> ExtractRightChannel(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FMargin>(InObject).Right : TOptional<float>();
	}
	static TOptional<float> ExtractBottomChannel(UObject& InObject, FTrackInstancePropertyBindings* Bindings)
	{
		return Bindings ? Bindings->GetCurrentValue<FMargin>(InObject).Bottom : TOptional<float>();
	}
	static void GetLeftChannelValueAndWeight(UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
		float& OutValue, float& OutWeight)
	{
		GetValueAndWeight(Object, SectionToKey, 0, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight);
	}
	static void GetTopChannelValueAndWeight(UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
		float& OutValue, float& OutWeight)
	{
		GetValueAndWeight(Object, SectionToKey, 1, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight);
	}
	static void GetRightChannelValueAndWeight(UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
		float& OutValue, float& OutWeight)
	{
		GetValueAndWeight(Object, SectionToKey, 2, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight);
	}
	static void GetBottomChannelValueAndWeight(UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
		float& OutValue, float& OutWeight)
	{
		GetValueAndWeight(Object, SectionToKey, 3, KeyTime, TickResolution, RootTemplate, OutValue, OutWeight);
	}
	static void GetValueAndWeight(UObject* Object, UMovieSceneSection*  SectionToKey, int32 Index, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
		float& OutValue, float& OutWeight)
	{
		UMovieSceneTrack* Track = SectionToKey->GetTypedOuter<UMovieSceneTrack>();
		FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();
		FMovieSceneInterrogationData InterrogationData;
		RootTemplate.CopyActuators(InterrogationData.GetAccumulator());

		FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, TickResolution));
		EvalTrack.Interrogate(Context, InterrogationData, Object);

		float Left = 0.0f, Right = 0.0f, Top = 0.0f, Bottom = 0.0f;

		for (const FMargin& Margin : InterrogationData.Iterate<FMargin>(UMovieSceneMarginSection::GetMarginInterrogationKey()))
		{
			Left = Margin.Left;
			Right = Margin.Right;
			Top = Margin.Top;
			Bottom = Margin.Bottom;
			break;
		}

		switch (Index)
		{
		case 0:
			OutValue = Left;
			break;
		case 1:
			OutValue = Top;
			break;
		case 2:
			OutValue = Right;
			break;
		case 3:
			OutValue = Bottom;
			break;
		}
		OutWeight = MovieSceneHelpers::CalculateWeightForBlending(SectionToKey, KeyTime);
	}

	FMovieSceneChannelMetaData      MetaData[4];
	TMovieSceneExternalValue<float> ExternalValues[4];
};

#endif	// WITH_EDITOR

UMovieSceneMarginSection::UMovieSceneMarginSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	BlendType = EMovieSceneBlendType::Absolute;

	FMovieSceneChannelProxyData Channels;
	bSupportsInfiniteRange = true;

#if WITH_EDITOR

	static const FMarginSectionEditorData EditorData;
	Channels.Add(LeftCurve,   EditorData.MetaData[0], EditorData.ExternalValues[0]);
	Channels.Add(TopCurve,    EditorData.MetaData[1], EditorData.ExternalValues[1]);
	Channels.Add(RightCurve,  EditorData.MetaData[2], EditorData.ExternalValues[2]);
	Channels.Add(BottomCurve, EditorData.MetaData[3], EditorData.ExternalValues[3]);

#else

	Channels.Add(LeftCurve);
	Channels.Add(TopCurve);
	Channels.Add(RightCurve);
	Channels.Add(BottomCurve);

#endif
	
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

const FMovieSceneInterrogationKey UMovieSceneMarginSection::GetMarginInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
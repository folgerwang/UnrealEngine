// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginSection.h"
#include "Channels/MovieSceneChannelProxy.h"

#if WITH_EDITOR

struct FMarginSectionEditorData
{
	FMarginSectionEditorData()
	{
		CommonData[0].SetIdentifiers("Left", NSLOCTEXT("MovieSceneMarginSection", "LeftText", "Left"));
		CommonData[0].SortOrder = 0;

		CommonData[1].SetIdentifiers("Top", NSLOCTEXT("MovieSceneMarginSection", "TopText", "Top"));
		CommonData[1].SortOrder = 1;

		CommonData[2].SetIdentifiers("Right", NSLOCTEXT("MovieSceneMarginSection", "RightText", "Right"));
		CommonData[2].SortOrder = 2;

		CommonData[3].SetIdentifiers("Bottom", NSLOCTEXT("MovieSceneMarginSection", "BottomText", "Bottom"));
		CommonData[3].SortOrder = 3;

		ExternalValues[0].OnGetExternalValue = ExtractLeftChannel;
		ExternalValues[1].OnGetExternalValue = ExtractTopChannel;
		ExternalValues[2].OnGetExternalValue = ExtractRightChannel;
		ExternalValues[3].OnGetExternalValue = ExtractBottomChannel;
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

	FMovieSceneChannelEditorData    CommonData[4];
	TMovieSceneExternalValue<float> ExternalValues[4];
};

#endif	// WITH_EDITOR

UMovieSceneMarginSection::UMovieSceneMarginSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	BlendType = EMovieSceneBlendType::Absolute;

	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	static const FMarginSectionEditorData EditorData;
	Channels.Add(LeftCurve,   EditorData.CommonData[0], EditorData.ExternalValues[0]);
	Channels.Add(TopCurve,    EditorData.CommonData[1], EditorData.ExternalValues[1]);
	Channels.Add(RightCurve,  EditorData.CommonData[2], EditorData.ExternalValues[2]);
	Channels.Add(BottomCurve, EditorData.CommonData[3], EditorData.ExternalValues[3]);

#else

	Channels.Add(LeftCurve);
	Channels.Add(TopCurve);
	Channels.Add(RightCurve);
	Channels.Add(BottomCurve);

#endif
	
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}
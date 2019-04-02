// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/MarginTrackEditor.h"
#include "ISectionLayoutBuilder.h"

FName FMarginTrackEditor::LeftName( "Left" );
FName FMarginTrackEditor::TopName( "Top" );
FName FMarginTrackEditor::RightName( "Right" );
FName FMarginTrackEditor::BottomName( "Bottom" );

TSharedRef<ISequencerTrackEditor> FMarginTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FMarginTrackEditor( InSequencer ) );
}

void FMarginTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys)
{
	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;
	FName ChannelName = StructPath.GetNumProperties() != 0 ? StructPath.GetLeafMostProperty().Property->GetFName() : NAME_None;

	FMargin Margin = PropertyChangedParams.GetPropertyValue<FMargin>();

	const bool bKeyLeft   = ChannelName == NAME_None || ChannelName == LeftName;
	const bool bKeyTop    = ChannelName == NAME_None || ChannelName == TopName;
	const bool bKeyRight  = ChannelName == NAME_None || ChannelName == RightName;
	const bool bKeyBottom = ChannelName == NAME_None || ChannelName == BottomName;

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, Margin.Left,   bKeyLeft));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, Margin.Top,    bKeyTop));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, Margin.Right,  bKeyRight));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, Margin.Bottom, bKeyBottom));
}

bool FMarginTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();

	UMovieSceneMarginTrack* MarginTrack = Cast<UMovieSceneMarginTrack>(Track);
	FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();

	if (MarginTrack)
	{
		FMovieSceneInterrogationData InterrogationData;
		GetSequencer()->GetEvaluationTemplate().CopyActuators(InterrogationData.GetAccumulator());

		FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, GetSequencer()->GetFocusedTickResolution()));
		EvalTrack.Interrogate(Context, InterrogationData, Object);

		FMargin Val(0.0f, 0.0f, 0.0f, 0.0f);
		for (const FMargin& InMargin : InterrogationData.Iterate<FMargin>(UMovieSceneMarginSection::GetMarginInterrogationKey()))
		{
			Val = InMargin;
			break;
		}
		FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
		GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Left, Weight);
		GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Top, Weight);
		GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Right, Weight);
		GeneratedTotalKeys[3]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Bottom, Weight);
		return true;
	}
	return false;
}
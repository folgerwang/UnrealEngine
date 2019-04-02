// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneFloatSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieScenePropertyTemplate.h"


UMovieSceneFloatSection::UMovieSceneFloatSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	BlendType = EMovieSceneBlendType::Absolute;
	bSupportsInfiniteRange = true;
#if WITH_EDITOR

	struct FGetCurrentValueAndWeight
	{
		static void GetFloatValueAndWeight(UObject* Object, UMovieSceneSection*  SectionToKey, FFrameNumber KeyTime, FFrameRate TickResolution, FMovieSceneRootEvaluationTemplateInstance& RootTemplate,
			float& OutValue, float& OutWeight)
		{
			OutValue = 0.0f;
			OutWeight = 1.0f;

			UMovieSceneTrack* Track = SectionToKey->GetTypedOuter<UMovieSceneTrack>();

			if (Track)
			{
				FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();
				FMovieSceneInterrogationData InterrogationData;
				RootTemplate.CopyActuators(InterrogationData.GetAccumulator());

				FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, TickResolution));
				EvalTrack.Interrogate(Context, InterrogationData, Object);

				for (const float &Value : InterrogationData.Iterate<float>(FMovieScenePropertySectionTemplate::GetFloatInterrogationKey()))
				{
					OutValue = Value;
					break;
				}
			}
			OutWeight = MovieSceneHelpers::CalculateWeightForBlending(SectionToKey, KeyTime);
		}
	};

	TMovieSceneExternalValue<float> ExternalValue;
	ExternalValue.OnGetCurrentValueAndWeight = FGetCurrentValueAndWeight::GetFloatValueAndWeight;

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(FloatCurve, FMovieSceneChannelMetaData(), ExternalValue);

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(FloatCurve);

#endif
}
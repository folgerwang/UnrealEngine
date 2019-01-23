// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieScenePropertyTemplate.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "LiveLinkTypes.h"
#include "MovieSceneLiveLinkSectionTemplate.generated.h"

class UMovieSceneLiveLinkSection;
class UMovieScenePropertyTrack;
class FMovieSceneLiveLinkSource;


USTRUCT()
struct FMovieSceneLiveLinkTemplateData
{
	GENERATED_BODY()

	FMovieSceneLiveLinkTemplateData(){};
	FMovieSceneLiveLinkTemplateData(const UMovieSceneLiveLinkSection& Section);
	
	UPROPERTY()
	TArray<FMovieSceneFloatChannel> FloatChannels;

	UPROPERTY()
	FLiveLinkFrameData TemplateToPush; 

	UPROPERTY()
	FLiveLinkRefSkeleton RefSkeleton;

	UPROPERTY()
	FName SubjectName;

	UPROPERTY()
	TArray<bool> ChannelMask; 


	bool GetLiveLinkFrameArray(const FFrameTime &FrameTime, const FFrameTime& LowerBound,const FFrameTime& UpperBound, TArray<FLiveLinkFrameData>& LiveLinkFrameDataArray, const FFrameRate& FrameRate) const;

};

/** A movie scene evaluation template for post move settings sections. */
USTRUCT()
struct FMovieSceneLiveLinkSectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieSceneLiveLinkTemplateData TemplateData;

	FMovieSceneLiveLinkSectionTemplate(){}
	FMovieSceneLiveLinkSectionTemplate(const UMovieSceneLiveLinkSection& Section, const UMovieScenePropertyTrack& Track);
	
private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	virtual void EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	virtual void SetupOverrides() override
	{
		EnableOverrides(RequiresSetupFlag | RequiresTearDownFlag );
	}
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
};


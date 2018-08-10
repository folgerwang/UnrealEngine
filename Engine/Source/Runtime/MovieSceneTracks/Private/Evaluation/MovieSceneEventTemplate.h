// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneObjectBindingID.h"

#include "MovieSceneEventTemplate.generated.h"

class UMovieSceneEventTrack;
struct EventData;

USTRUCT()
struct FMovieSceneEventTemplateBase : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneEventTemplateBase() : bFireEventsWhenForwards(false), bFireEventsWhenBackwards(false) {}
	FMovieSceneEventTemplateBase(const UMovieSceneEventTrack& Track);

	UPROPERTY()
	TArray<FMovieSceneObjectBindingID> EventReceivers;

	UPROPERTY()
	uint32 bFireEventsWhenForwards : 1;

	UPROPERTY()
	uint32 bFireEventsWhenBackwards : 1;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
};

USTRUCT()
struct FMovieSceneEventSectionTemplate : public FMovieSceneEventTemplateBase
{
	GENERATED_BODY()
	
	FMovieSceneEventSectionTemplate() {}
	FMovieSceneEventSectionTemplate(const UMovieSceneEventSection& Section, const UMovieSceneEventTrack& Track);

	UPROPERTY()
	FMovieSceneEventSectionData EventData;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};


USTRUCT()
struct FMovieSceneEventTriggerTemplate : public FMovieSceneEventTemplateBase
{
	GENERATED_BODY()
	
	FMovieSceneEventTriggerTemplate() {}
	FMovieSceneEventTriggerTemplate(const UMovieSceneEventTriggerSection& Section, const UMovieSceneEventTrack& Track);

	UPROPERTY()
	TArray<FFrameNumber> EventTimes;

	UPROPERTY()
	TArray<FName> EventFunctions;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};

USTRUCT()
struct FMovieSceneEventRepeaterTemplate : public FMovieSceneEventTemplateBase
{
	GENERATED_BODY()
	
	FMovieSceneEventRepeaterTemplate() {}
	FMovieSceneEventRepeaterTemplate(const UMovieSceneEventRepeaterSection& Section, const UMovieSceneEventTrack& Track);

	UPROPERTY()
	FName EventToTrigger;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};

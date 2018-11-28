// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneObjectPropertyTemplate.h"
#include "Tracks/MovieSceneObjectPropertyTrack.h"
#include "Sections/MovieSceneObjectPropertySection.h"


struct FObjectPropertyExecToken : IMovieSceneExecutionToken
{
	FObjectPropertyExecToken(TSoftObjectPtr<>&& InValue)
		: Value(MoveTemp(InValue))
	{}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		FSectionData& PropertyTrackData = PersistentData.GetSectionData<FSectionData>();
		FTrackInstancePropertyBindings* PropertyBindings = PropertyTrackData.PropertyBindings.Get();

		check(PropertyBindings);

		UObject* NewObjectValue = Value.LoadSynchronous();

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = WeakObject.Get();
			if (!ObjectPtr)
			{
				continue;
			}

			UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(PropertyBindings->GetProperty(*ObjectPtr));
			if (!ObjectProperty || !CanAssignValue(ObjectProperty, NewObjectValue))
			{
				continue;
			}

			Player.SavePreAnimatedState(*ObjectPtr, PropertyTrackData.PropertyID, FTokenProducer<UObject*>(*PropertyBindings));

			UObject* ExistingValue = PropertyBindings->GetCurrentValue<UObject*>(*ObjectPtr);
			if (ExistingValue != NewObjectValue)
			{
				PropertyBindings->CallFunction<UObject*>(*ObjectPtr, NewObjectValue);
			}
		}
	}

	bool CanAssignValue(UObjectPropertyBase* TargetProperty, UObject* DesiredValue) const
	{
		check(TargetProperty);
		if (!TargetProperty->PropertyClass)
		{
			return false;
		}
		else if (!DesiredValue)
		{
			return !TargetProperty->HasAnyPropertyFlags(CPF_NoClear);
		}
		else
		{
			return DesiredValue->GetClass()->IsChildOf(TargetProperty->PropertyClass);
		}
	}

	TSoftObjectPtr<> Value;
};


FMovieSceneObjectPropertyTemplate::FMovieSceneObjectPropertyTemplate(const UMovieSceneObjectPropertySection& Section, const UMovieSceneObjectPropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, ObjectChannel(Section.ObjectChannel)
{}

void FMovieSceneObjectPropertyTemplate::SetupOverrides()
{
	// We need FMovieScenePropertySectionTemplate::Setup to be called for initialization of the track instance bindings
	EnableOverrides(RequiresSetupFlag);
}

void FMovieSceneObjectPropertyTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	TSoftObjectPtr<> Ptr;
	if (ObjectChannel.Evaluate(Context.GetTime(), Ptr))
	{
		ExecutionTokens.Add(FObjectPropertyExecToken(MoveTemp(Ptr)));
	}
}

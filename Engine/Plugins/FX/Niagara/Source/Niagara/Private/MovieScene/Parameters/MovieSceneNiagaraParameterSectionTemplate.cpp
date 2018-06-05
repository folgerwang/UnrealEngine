// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraParameterSectionTemplate.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "IMovieScenePlayer.h"

struct FComponentData
{
	TWeakObjectPtr<UNiagaraComponent> Component;
	TOptional<TArray<uint8>> CurrentValue;
};

struct FParameterSectionData : IPersistentEvaluationData
{
	TArray<FComponentData> CachedComponentData;
};

struct FPreAnimatedParameterValueToken : IMovieScenePreAnimatedToken
{
	FPreAnimatedParameterValueToken(FNiagaraVariable InParameter, TOptional<TArray<uint8>>&& InPreviousValueData)
		: Parameter(InParameter)
		, PreviousValueData(InPreviousValueData)
	{
	}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& InPlayer)
	{
		UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(&InObject);
		if (PreviousValueData.IsSet() == false)
		{
			NiagaraComponent->GetOverrideParameters().RemoveParameter(Parameter);
		}
		else
		{
			NiagaraComponent->GetOverrideParameters().SetParameterData(PreviousValueData.GetValue().GetData(), Parameter);
		}
	}

	FNiagaraVariable Parameter;
	TOptional<TArray<uint8>> PreviousValueData;
};

struct FPreAnimatedParameterValueTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FPreAnimatedParameterValueTokenProducer(FNiagaraVariable InParameter)
		: Parameter(InParameter)
	{
	}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(&Object);
		const uint8* ParameterData = NiagaraComponent->GetOverrideParameters().GetParameterData(Parameter);
		TOptional<TArray<uint8>> PreviousValue;
		if (ParameterData != nullptr)
		{
			TArray<uint8> ParameterDataArray;
			ParameterDataArray.AddUninitialized(Parameter.GetSizeInBytes());
			FMemory::Memcpy(ParameterDataArray.GetData(), ParameterData, Parameter.GetSizeInBytes());
			PreviousValue.Emplace(MoveTemp(ParameterDataArray));
		}
		return FPreAnimatedParameterValueToken(Parameter, MoveTemp(PreviousValue));
	}

	FNiagaraVariable Parameter;
};

struct FSetParameterValueExecutionToken : IMovieSceneExecutionToken
{
	FSetParameterValueExecutionToken(TWeakObjectPtr<UNiagaraComponent> InComponentPtr, FNiagaraVariable InParameter, TArray<uint8>&& InData)
		: ComponentPtr(InComponentPtr)
		, Parameter(InParameter)
		, Data(InData)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		static FMovieSceneAnimTypeID TypeID = TMovieSceneAnimTypeID<FSetParameterValueExecutionToken, 0>();
		UNiagaraComponent* NiagaraComponent = ComponentPtr.Get();
		if (NiagaraComponent != nullptr)
		{
			Player.SavePreAnimatedState(*NiagaraComponent, TypeID, FPreAnimatedParameterValueTokenProducer(Parameter));
			NiagaraComponent->GetOverrideParameters().AddParameter(Parameter, false);
			NiagaraComponent->GetOverrideParameters().SetParameterData(Data.GetData(), Parameter);
		}
	}

	TWeakObjectPtr<UNiagaraComponent> ComponentPtr;
	FNiagaraVariable Parameter;
	TArray<uint8> Data;
};

FMovieSceneNiagaraParameterSectionTemplate::FMovieSceneNiagaraParameterSectionTemplate()
{
}

FMovieSceneNiagaraParameterSectionTemplate::FMovieSceneNiagaraParameterSectionTemplate(FNiagaraVariable InParameter)
	: Parameter(InParameter)
{
	EnableOverrides(RequiresInitializeFlag);
}

void FMovieSceneNiagaraParameterSectionTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FParameterSectionData& SectionData = PersistentData.GetOrAddSectionData<FParameterSectionData>();
	SectionData.CachedComponentData.Empty();

	for (TWeakObjectPtr<> ObjectPtr : Player.FindBoundObjects(Operand))
	{
		UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(ObjectPtr.Get());
		if (NiagaraComponent != nullptr)
		{
			const uint8* ParameterData = NiagaraComponent->GetOverrideParameters().GetParameterData(Parameter);
			if (ParameterData == nullptr)
			{
				ParameterData = NiagaraComponent->GetAsset()->GetExposedParameters().GetParameterData(Parameter);
			}

			TArray<uint8> CurrentValueData;
			if(ParameterData != nullptr)
			{
				CurrentValueData.AddUninitialized(Parameter.GetSizeInBytes());
				FMemory::Memcpy(CurrentValueData.GetData(), ParameterData, Parameter.GetSizeInBytes());
			}
			else
			{
				CurrentValueData.AddDefaulted(Parameter.GetSizeInBytes());
			}

			FComponentData Data;
			Data.Component = NiagaraComponent;
			Data.CurrentValue.Emplace(CurrentValueData);
			SectionData.CachedComponentData.Add(Data);
		}
	}
}

void FMovieSceneNiagaraParameterSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FParameterSectionData const* SectionData = PersistentData.FindSectionData<FParameterSectionData>();
	if (SectionData != nullptr)
	{
		for (const FComponentData& ComponentData : SectionData->CachedComponentData)
		{
			if (ComponentData.CurrentValue.IsSet())
			{
				TArray<uint8> AnimatedValueData;
				GetParameterValue(Context.GetTime(), ComponentData.CurrentValue.GetValue(), AnimatedValueData);
				ExecutionTokens.Add(FSetParameterValueExecutionToken(ComponentData.Component, Parameter, MoveTemp(AnimatedValueData)));
			}
		}
	}
}
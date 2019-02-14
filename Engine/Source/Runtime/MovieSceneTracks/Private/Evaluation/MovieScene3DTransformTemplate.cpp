// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScene3DTransformTemplate.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Evaluation/MovieSceneTemplateCommon.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Evaluation/Blending/BlendableTokenStack.h"
#include "Evaluation/Blending/MovieSceneBlendingActuatorID.h"
#include "IMovieScenePlaybackClient.h"
#include "Tracks/IMovieSceneTransformOrigin.h"

DECLARE_CYCLE_STAT(TEXT("Transform Track Evaluate"), MovieSceneEval_TransformTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Transform Track Token Execute"), MovieSceneEval_TransformTrack_TokenExecute, STATGROUP_MovieSceneEval);

namespace MovieScene
{
	/** Convert a transform track token to a 9 channel float */
	void MultiChannelFromData(const F3DTransformTrackToken& In, TMultiChannelValue<float, 9>& Out)
	{
		FVector Rotation = In.Rotation.Euler();
		Out = { In.Translation.X, In.Translation.Y, In.Translation.Z, Rotation.X, Rotation.Y, Rotation.Z, In.Scale.X, In.Scale.Y, In.Scale.Z };
	}

	/** Convert a 9 channel float to a transform track token */
	void ResolveChannelsToData(const TMultiChannelValue<float, 9>& In, F3DTransformTrackToken& Out)
	{
		Out.Translation = FVector(In[0], In[1], In[2]);
		Out.Rotation = FRotator::MakeFromEuler(FVector(In[3], In[4], In[5]));
		Out.Scale = FVector(In[6], In[7], In[8]);
	}
}

// Specify a unique runtime type identifier for 3d transform track tokens
template<> FMovieSceneAnimTypeID GetBlendingDataType<F3DTransformTrackToken>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

/** Define working data types for blending calculations - we use a 9 channel masked blendable float */
template<> struct TBlendableTokenTraits<F3DTransformTrackToken> 
{
	typedef MovieScene::TMaskedBlendable<float, 9> WorkingDataType;
};

/** Actuator that knows how to apply transform track tokens to an object */
struct FComponentTransformActuator : TMovieSceneBlendingActuator<F3DTransformTrackToken>
{
	FComponentTransformActuator()
		: TMovieSceneBlendingActuator<F3DTransformTrackToken>(GetActuatorTypeID())
	{}

	/** Access a unique identifier for this actuator type */
	static FMovieSceneBlendingActuatorID GetActuatorTypeID()
	{
		static FMovieSceneAnimTypeID TypeID = TMovieSceneAnimTypeID<FComponentTransformActuator>();
		return FMovieSceneBlendingActuatorID(TypeID);
	}

	/** Get an object's current transform */
	virtual F3DTransformTrackToken RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		if (USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(InObject))
		{
			return F3DTransformTrackToken(SceneComponent->RelativeLocation, SceneComponent->RelativeRotation, SceneComponent->RelativeScale3D);
		}

		return F3DTransformTrackToken();
	}

	/** Set an object's transform */
	virtual void Actuate(UObject* InObject, const F3DTransformTrackToken& InFinalValue, const TBlendableTokenStack<F3DTransformTrackToken>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		ensureMsgf(InObject, TEXT("Attempting to evaluate a Transform track with a null object."));

		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(InObject);
		if (SceneComponent)
		{
			// Save preanimated state for all currently animating entities
			OriginalStack.SavePreAnimatedState(Player, *SceneComponent, FMobilityTokenProducer::GetAnimTypeID(), FMobilityTokenProducer());
			OriginalStack.SavePreAnimatedState(Player, *SceneComponent, F3DTransformTokenProducer::GetAnimTypeID(), F3DTransformTokenProducer());

			SceneComponent->SetMobility(EComponentMobility::Movable);

			InFinalValue.Apply(*SceneComponent, Context.GetDelta() / Context.GetFrameRate());
		}
	}

	virtual void Actuate(FMovieSceneInterrogationData& InterrogationData, const F3DTransformTrackToken& InValue, const TBlendableTokenStack<F3DTransformTrackToken>& OriginalStack, const FMovieSceneContext& Context) const override
	{
		InterrogationData.Add(FTransform(InValue.Rotation.Quaternion(), InValue.Translation, InValue.Scale), UMovieScene3DTransformSection::GetInterrogationKey());
	}
};

FMovieSceneComponentTransformSectionTemplate::FMovieSceneComponentTransformSectionTemplate(const UMovieScene3DTransformSection& Section)
	: TemplateData(Section)
{
}

struct FComponentTransformPersistentData : IPersistentEvaluationData
{
	FTransform Origin;
};

void FMovieSceneComponentTransformSectionTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	// If the global instance data implements a transform origin interface, use its transform as an origin for this transform
	IMovieScenePlaybackClient*        PlaybackClient = Player.GetPlaybackClient();
	UObject*                          InstanceData   = PlaybackClient ? PlaybackClient->GetInstanceData() : nullptr;
	const IMovieSceneTransformOrigin* RawInterface   = Cast<IMovieSceneTransformOrigin>(InstanceData);

	const bool bHasInterface = RawInterface || (InstanceData && InstanceData->GetClass()->ImplementsInterface(UMovieSceneTransformOrigin::StaticClass()));

	if (bHasInterface && TemplateData.BlendType == EMovieSceneBlendType::Absolute)
	{
		// Retrieve the current origin
		FTransform TransformOrigin = RawInterface ? RawInterface->GetTransformOrigin() : IMovieSceneTransformOrigin::Execute_BP_GetTransformOrigin(InstanceData);

		// Assign the transform origin to the peristent data so it can be queried in Evaluate
		PersistentData.GetOrAddSectionData<FComponentTransformPersistentData>().Origin = TransformOrigin;
	}
}

void FMovieSceneComponentTransformSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace MovieScene;

	TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());
	if (TransformValue.IsEmpty())
	{
		return;
	}

	// Apply origin transformation if necessary 
	const FComponentTransformPersistentData* Data = PersistentData.FindSectionData<FComponentTransformPersistentData>();
	if (TemplateData.BlendType == EMovieSceneBlendType::Absolute && Data)
	{
		float Components[6] = {
			TransformValue.Get(0, 0.f), TransformValue.Get(1, 0.f), TransformValue.Get(2, 0.f),
			TransformValue.Get(3, 0.f), TransformValue.Get(4, 0.f), TransformValue.Get(5, 0.f),
		};

		FTransform AnimatedTransform(FRotator(Components[4], Components[5], Components[3]), FVector(Components[0], Components[1], Components[2]));
		AnimatedTransform = AnimatedTransform * Data->Origin;

		FVector Location = AnimatedTransform.GetTranslation();
		Components[0] = Location.X;
		Components[1] = Location.Y;
		Components[2] = Location.Z;

		FVector Rotation = AnimatedTransform.GetRotation().Euler();
		Components[3] = Rotation.X;
		Components[4] = Rotation.Y;
		Components[5] = Rotation.Z;

		for (int32 Index = 0; Index < ARRAY_COUNT(Components); ++Index)
		{
			if (TransformValue.IsSet(Index))
			{
				TransformValue.Set(Index, Components[Index]);
			}
		}
	}

	// Ensure the accumulator knows how to actually apply component transforms
	FMovieSceneBlendingActuatorID ActuatorTypeID = FComponentTransformActuator::GetActuatorTypeID();
	if (!ExecutionTokens.GetBlendingAccumulator().FindActuator<F3DTransformTrackToken>(ActuatorTypeID))
	{
		ExecutionTokens.GetBlendingAccumulator().DefineActuator(ActuatorTypeID, MakeShared<FComponentTransformActuator>());
	}

	// Add the blendable to the accumulator
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight))
	{
		float ManualWeight = 1.f;
		TemplateData.ManualWeight.Evaluate(Context.GetTime(), ManualWeight);
		Weight *= ManualWeight;
	}

	ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<F3DTransformTrackToken>(TransformValue, TemplateData.BlendType, Weight));
}

void FMovieSceneComponentTransformSectionTemplate::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const
{
	using namespace MovieScene;

	TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());
	if (TransformValue.IsEmpty())
	{
		return;
	}

	// Ensure the accumulator knows how to actually apply component transforms
	FMovieSceneBlendingActuatorID ActuatorTypeID = FComponentTransformActuator::GetActuatorTypeID();
	if (!Container.GetAccumulator().FindActuator<F3DTransformTrackToken>(ActuatorTypeID))
	{
		Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared<FComponentTransformActuator>());
	}

	// Add the blendable to the accumulator
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight))
	{
		float ManualWeight = 1.f;
		TemplateData.ManualWeight.Evaluate(Context.GetTime(), ManualWeight);
		Weight *= ManualWeight;
	}

	Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<F3DTransformTrackToken>(TransformValue, TemplateData.BlendType, Weight));
}

FMovieScene3DTransformTemplateData::FMovieScene3DTransformTemplateData(const UMovieScene3DTransformSection& Section)
	: BlendType(Section.GetBlendType().Get())
	, Mask(Section.GetMask())
	, bUseQuaternionInterpolation(Section.GetUseQuaternionInterpolation())
{
	EMovieSceneTransformChannel MaskChannels = Mask.GetChannels();
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section.GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::TranslationX))	TranslationCurve[0]	= *FloatChannels[0];
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::TranslationY))	TranslationCurve[1]	= *FloatChannels[1];
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::TranslationZ))	TranslationCurve[2]	= *FloatChannels[2];

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::RotationX))		RotationCurve[0]	= *FloatChannels[3];
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::RotationY))		RotationCurve[1]	= *FloatChannels[4];
	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::RotationZ))		RotationCurve[2]	= *FloatChannels[5];

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::ScaleX))
	{
		ScaleCurve[0] = *FloatChannels[6];
	}
	else
	{
		ScaleCurve[0].SetDefault(1.0f);
	}

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::ScaleY))
	{
		ScaleCurve[1] = *FloatChannels[7];
	}
	else
	{
		ScaleCurve[1].SetDefault(1.0f);
	}

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::ScaleZ))
	{
		ScaleCurve[2] = *FloatChannels[8];
	}
	else
	{
		ScaleCurve[2].SetDefault(1.0f);
	}

	if (EnumHasAllFlags(MaskChannels, EMovieSceneTransformChannel::Weight))
	{
		ManualWeight = *FloatChannels[9];
	}
	else
	{
		ManualWeight.SetDefault(1.0f);
	}
}

MovieScene::TMultiChannelValue<float, 9> FMovieScene3DTransformTemplateData::Evaluate(FFrameTime Time) const
{
	MovieScene::TMultiChannelValue<float, 9> AnimatedData;

	EMovieSceneTransformChannel ChannelMask = Mask.GetChannels();

	auto EvalChannel = [&AnimatedData, Time, ChannelMask](uint8 ChanneIndex, EMovieSceneTransformChannel ChannelType, const FMovieSceneFloatChannel& Channel)
	{
		float Value = 0.f;
		if (EnumHasAllFlags(ChannelMask, ChannelType) && Channel.Evaluate(Time, Value))
		{
			AnimatedData.Set(ChanneIndex, Value);
		}
	};

	EvalChannel(0, EMovieSceneTransformChannel::TranslationX, TranslationCurve[0]);
	EvalChannel(1, EMovieSceneTransformChannel::TranslationY, TranslationCurve[1]);
	EvalChannel(2, EMovieSceneTransformChannel::TranslationZ, TranslationCurve[2]);

	if (!bUseQuaternionInterpolation)
	{
		EvalChannel(3, EMovieSceneTransformChannel::RotationX, RotationCurve[0]);
		EvalChannel(4, EMovieSceneTransformChannel::RotationY, RotationCurve[1]);
		EvalChannel(5, EMovieSceneTransformChannel::RotationZ, RotationCurve[2]);
	}
	else
	{
		//Use Quaternion Interpolation. This is complicated since unlike Matinee we may not have
		//to perform the interpolation. To do this if finds the exclusive closest range of keys to encompass
		//the passed in time
		auto SetFrameRange = [Time](TRange<FFrameNumber> &FrameRange, const TArrayView<const FFrameNumber> &Times)
		{
			int32 Index1, Index2;
			Index2 = 0;
			Index2 = Algo::UpperBound(Times, Time.FrameNumber);
			Index1 = Index2 - 1;
			Index1 = Index1 >= 0 ? Index1 : INDEX_NONE;
			Index2 = Index2 < Times.Num() ? Index2 : INDEX_NONE;
			if (Index1 != INDEX_NONE && Index2 != INDEX_NONE)
			{
				if (Times[Index1] != Time.FrameNumber && Times[Index1] > FrameRange.GetLowerBoundValue()) 
				{
					FrameRange.SetLowerBoundValue(Times[Index1]);
				}
				if (Times[Index2] != Time.FrameNumber && Times[Index1] < FrameRange.GetUpperBoundValue())
				{
					FrameRange.SetUpperBoundValue(Times[Index2]);
				}
			}
		};

		TRange<FFrameNumber> FrameRange(TNumericLimits<FFrameNumber>::Min(), TNumericLimits<FFrameNumber>::Max());
		if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationX))
		{
			SetFrameRange(FrameRange, RotationCurve[0].GetTimes());
		}
		if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationY))
		{
			SetFrameRange(FrameRange, RotationCurve[1].GetTimes());
		}
		if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationZ))
		{
			SetFrameRange(FrameRange, RotationCurve[2].GetTimes());
		}
		FFrameNumber LowerBound = FrameRange.GetLowerBoundValue();
		FFrameNumber UpperBound = FrameRange.GetUpperBoundValue();
		if (LowerBound != TNumericLimits<FFrameNumber>::Min() && UpperBound != TNumericLimits<FFrameNumber>::Max())
		{
			float Value;
			FVector FirstRot(0.0f, 0.0f, 0.0f);
			FVector SecondRot(0.0f, 0.0f, 0.0f);
			double U = (Time.AsDecimal() - (double) FrameRange.GetLowerBoundValue().Value) /
				double(FrameRange.GetUpperBoundValue().Value - FrameRange.GetLowerBoundValue().Value);
			U = FMath::Clamp(U, 0.0, 1.0);

			if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationX))
			{
				if (RotationCurve[0].Evaluate(LowerBound, Value))
				{
					FirstRot[0] = Value;
				}
				if (RotationCurve[0].Evaluate(UpperBound, Value))
				{
					SecondRot[0] = Value;
				}
			}
			if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationY))
			{
				if (RotationCurve[1].Evaluate(LowerBound, Value))
				{
					FirstRot[1] = Value;
				}
				if (RotationCurve[1].Evaluate(UpperBound, Value))
				{
					SecondRot[1] = Value;
				}
			}
			if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationZ))
			{
				if (RotationCurve[2].Evaluate(LowerBound, Value))
				{
					FirstRot[2] = Value;
				}
				if (RotationCurve[2].Evaluate(UpperBound, Value))
				{
					SecondRot[2] = Value;
				}
			}

			const FQuat Key1Quat = FQuat::MakeFromEuler(FirstRot);
			const FQuat Key2Quat = FQuat::MakeFromEuler(SecondRot);

			const FQuat SlerpQuat = FQuat::Slerp(Key1Quat, Key2Quat, U);
			FVector Euler = FRotator(SlerpQuat).Euler();
			AnimatedData.Set(3, Euler[0]);
			AnimatedData.Set(4, Euler[1]);
			AnimatedData.Set(5, Euler[2]);
		}

		else  //no range found default to regular, but still do RotToQuat
		{
			float Value;
			FVector CurrentRot(0.0f, 0.0f, 0.0f);
			FRotator Rotator;
			if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationX) && RotationCurve[0].Evaluate(Time, Value))
			{
				CurrentRot[0] = Value;
			}
			if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationY) && RotationCurve[1].Evaluate(Time, Value))
			{
				CurrentRot[1] = Value;
			}
			if (EnumHasAllFlags(ChannelMask, EMovieSceneTransformChannel::RotationZ) && RotationCurve[2].Evaluate(Time, Value))
			{
				CurrentRot[2] = Value;
			}
			FQuat Quat = FQuat::MakeFromEuler(CurrentRot);
			FVector Euler = FRotator(Quat).Euler();
			AnimatedData.Set(3, Euler[0]);
			AnimatedData.Set(4, Euler[1]);
			AnimatedData.Set(5, Euler[2]);
		}
	}

	EvalChannel(6, EMovieSceneTransformChannel::ScaleX, ScaleCurve[0]);
	EvalChannel(7, EMovieSceneTransformChannel::ScaleY, ScaleCurve[1]);
	EvalChannel(8, EMovieSceneTransformChannel::ScaleZ, ScaleCurve[2]);

	return AnimatedData;
}

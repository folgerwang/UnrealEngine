// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"

#include "Compilation/MovieSceneCompilerRules.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Runtime/AnimGraphRuntime/Public/AnimSequencerInstance.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "UObject/ObjectKey.h"

bool ShouldUsePreviewPlayback(IMovieScenePlayer& Player, UObject& RuntimeObject)
{
	// we also use PreviewSetAnimPosition in PIE when not playing, as we can preview in PIE
	bool bIsNotInPIEOrNotPlaying = (RuntimeObject.GetWorld() && !RuntimeObject.GetWorld()->HasBegunPlay()) || Player.GetPlaybackStatus() != EMovieScenePlayerStatus::Playing;
	return GIsEditor && bIsNotInPIEOrNotPlaying;
}

bool CanPlayAnimation(USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* AnimAssetBase)
{
	return (SkeletalMeshComponent->SkeletalMesh && SkeletalMeshComponent->SkeletalMesh->Skeleton && 
		(!AnimAssetBase || SkeletalMeshComponent->SkeletalMesh->Skeleton->IsCompatible(AnimAssetBase->GetSkeleton())));
}

void ResetAnimSequencerInstance(UObject& ObjectToRestore, IMovieScenePlayer& Player)
{
	CastChecked<UAnimSequencerInstance>(&ObjectToRestore)->ResetNodes();
}

struct FStopPlayingMontageTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	TWeakObjectPtr<UAnimInstance> TempInstance;
	int32 TempMontageInstanceId;

	FStopPlayingMontageTokenProducer(TWeakObjectPtr<UAnimInstance> InTempInstance, int32 InTempMontageInstanceId)
	: TempInstance(InTempInstance)
	, TempMontageInstanceId(InTempMontageInstanceId){}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			TWeakObjectPtr<UAnimInstance> WeakInstance;
			int32 MontageInstanceId;

			FToken(TWeakObjectPtr<UAnimInstance> InWeakInstance, int32 InMontageInstanceId) 
			: WeakInstance(InWeakInstance)
			, MontageInstanceId(InMontageInstanceId) {}

			virtual void RestoreState(UObject& ObjectToRestore, IMovieScenePlayer& Player) override
			{
				UAnimInstance* AnimInstance = WeakInstance.Get();
				if (AnimInstance)
				{
					FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(MontageInstanceId);
					if (MontageInstance)
					{
						MontageInstance->Stop(FAlphaBlend(0.f), false);
					}
				}
			}
		};

		return FToken(TempInstance, TempMontageInstanceId);
	}
};

struct FPreAnimatedAnimationTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			FToken(USkeletalMeshComponent* InComponent)
			{
				// Cache this object's current update flag and animation mode
				VisibilityBasedAnimTickOption = InComponent->VisibilityBasedAnimTickOption;
				AnimationMode = InComponent->GetAnimationMode();
#if WITH_EDITOR
				bUpdateAnimationInEditor = InComponent->GetUpdateAnimationInEditor();
#endif
			}

			virtual void RestoreState(UObject& ObjectToRestore, IMovieScenePlayer& Player)
			{
				USkeletalMeshComponent* Component = CastChecked<USkeletalMeshComponent>(&ObjectToRestore);

				UAnimSequencerInstance* SequencerInst = Cast<UAnimSequencerInstance>(Component->GetAnimInstance());
				if (SequencerInst)
				{
					SequencerInst->ResetNodes();
				}

				UAnimSequencerInstance::UnbindFromSkeletalMeshComponent(Component);

				// Reset the mesh component update flag and animation mode to what they were before we animated the object
				Component->VisibilityBasedAnimTickOption = VisibilityBasedAnimTickOption;
				if (Component->GetAnimationMode() != AnimationMode)
				{
					// this SetAnimationMode reinitializes even if the mode is same
					// if we're using same anim blueprint, we don't want to keep reinitializing it. 
					Component->SetAnimationMode(AnimationMode);
				}
#if WITH_EDITOR
				Component->SetUpdateAnimationInEditor(bUpdateAnimationInEditor);
#endif
			}

			EVisibilityBasedAnimTickOption VisibilityBasedAnimTickOption;
			EAnimationMode::Type AnimationMode;
#if WITH_EDITOR
			bool bUpdateAnimationInEditor;
#endif
		};

		return FToken(CastChecked<USkeletalMeshComponent>(&Object));
	}
};


struct FMinimalAnimParameters
{
	FMinimalAnimParameters(UAnimSequenceBase* InAnimation, float InEvalTime, float InBlendWeight, const FMovieSceneEvaluationScope& InScope, FName InSlotName, FObjectKey InSection, bool InSkipAnimationNotifiers, bool InForceCustomMode)
		: Animation(InAnimation)
		, EvalTime(InEvalTime)
		, BlendWeight(InBlendWeight)
		, EvaluationScope(InScope)
		, SlotName(InSlotName)
		, Section(InSection)
		, bSkipAnimNotifiers(InSkipAnimationNotifiers)
		, bForceCustomMode(InForceCustomMode)
	{}
	
	UAnimSequenceBase* Animation;
	float EvalTime;
	float BlendWeight;
	FMovieSceneEvaluationScope EvaluationScope;
	FName SlotName;
	FObjectKey Section;
	bool bSkipAnimNotifiers;
	bool bForceCustomMode;
};

/** Montage player per section data */
struct FMontagePlayerPerSectionData 
{
	TWeakObjectPtr<UAnimMontage> Montage;
	int32 MontageInstanceId;
};

namespace MovieScene
{
	struct FBlendedAnimation
	{
		TArray<FMinimalAnimParameters> AllAnimations;

		FBlendedAnimation& Resolve(TMovieSceneInitialValueStore<FBlendedAnimation>& InitialValueStore)
		{
			return *this;
		}
	};

	void BlendValue(FBlendedAnimation& OutBlend, const FMinimalAnimParameters& InValue, float Weight, EMovieSceneBlendType BlendType, TMovieSceneInitialValueStore<FBlendedAnimation>& InitialValueStore)
	{
		OutBlend.AllAnimations.Add(InValue);
	}

	struct FComponentAnimationActuator : TMovieSceneBlendingActuator<FBlendedAnimation>
	{
		FComponentAnimationActuator() : TMovieSceneBlendingActuator<FBlendedAnimation>(GetActuatorTypeID()) {}

		static FMovieSceneBlendingActuatorID GetActuatorTypeID()
		{
			static FMovieSceneAnimTypeID TypeID = TMovieSceneAnimTypeID<FComponentAnimationActuator, 0>();
			return FMovieSceneBlendingActuatorID(TypeID);
		}

		static FMovieSceneAnimTypeID GetAnimControlTypeID()
		{
			static FMovieSceneAnimTypeID TypeID = TMovieSceneAnimTypeID<FComponentAnimationActuator, 2>();
			return TypeID;
		}

		virtual FBlendedAnimation RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
		{
			check(false);
			return FBlendedAnimation();
		}

		virtual void Actuate(UObject* InObject, const FBlendedAnimation& InFinalValue, const TBlendableTokenStack<FBlendedAnimation>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
		{
			ensureMsgf(InObject, TEXT("Attempting to evaluate an Animation track with a null object."));

			USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshComponentFromObject(InObject);
			if (!SkeletalMeshComponent)
			{
				return;
			}
			OriginalStack.SavePreAnimatedState(Player, *SkeletalMeshComponent, GetAnimControlTypeID(), FPreAnimatedAnimationTokenProducer());

			UAnimInstance* ExistingAnimInstance = SkeletalMeshComponent->GetAnimInstance();

			UAnimSequencerInstance* SequencerInstance = UAnimCustomInstance::BindToSkeletalMeshComponent<UAnimSequencerInstance>(SkeletalMeshComponent);

			const bool bPreviewPlayback = ShouldUsePreviewPlayback(Player, *SkeletalMeshComponent);

			const EMovieScenePlayerStatus::Type PlayerStatus = Player.GetPlaybackStatus();

			// If the playback status is jumping, ie. one such occurrence is setting the time for thumbnail generation, disable anim notifies updates because it could fire audio
			// We now layer this with the passed in notify toggleto force a disable in this case.
			const bool bFireNotifies = !bPreviewPlayback || (PlayerStatus != EMovieScenePlayerStatus::Jumping && PlayerStatus != EMovieScenePlayerStatus::Stopped);

			// When jumping from one cut to another cut, the delta time should be 0 so that anim notifies before the current position are not evaluated. Note, anim notifies at the current time should still be evaluated.
			const double DeltaTime = ( Context.HasJumped() ? FFrameTime(0) : Context.GetRange().Size<FFrameTime>() ) / Context.GetFrameRate();

			const bool bResetDynamics = PlayerStatus == EMovieScenePlayerStatus::Stepping || 
										PlayerStatus == EMovieScenePlayerStatus::Jumping || 
										PlayerStatus == EMovieScenePlayerStatus::Scrubbing || 
										(DeltaTime == 0.0f && PlayerStatus != EMovieScenePlayerStatus::Stopped); 
		
			static const bool bLooping = false;
			//Need to zero all weights first since we may be blending animation that are keeping state but are no longer active.
			
			if(SequencerInstance)
			{
				SequencerInstance->ResetNodes();
			}
			else if (ExistingAnimInstance)
			{
				for (const TPair<FObjectKey, FMontagePlayerPerSectionData >& Pair : MontageData)
				{
					int32 InstanceId = Pair.Value.MontageInstanceId;
					FAnimMontageInstance* MontageInstanceToUpdate = ExistingAnimInstance->GetMontageInstanceForID(InstanceId);
					if (MontageInstanceToUpdate)
					{
						MontageInstanceToUpdate->SetDesiredWeight(0.0f);
						MontageInstanceToUpdate->SetWeight(0.0f);
					}
				}
			}
			for (const FMinimalAnimParameters& AnimParams : InFinalValue.AllAnimations)
			{
				Player.PreAnimatedState.SetCaptureEntity(AnimParams.EvaluationScope.Key, AnimParams.EvaluationScope.CompletionMode);

				if (bPreviewPlayback)
				{
					PreviewSetAnimPosition(PersistentData, Player, SkeletalMeshComponent,
						AnimParams.SlotName, AnimParams.Section, AnimParams.Animation, AnimParams.EvalTime, AnimParams.BlendWeight,
						bLooping, bFireNotifies && !AnimParams.bSkipAnimNotifiers, DeltaTime, Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing, 
						bResetDynamics, AnimParams.bForceCustomMode);
				}
				else
				{
					SetAnimPosition(PersistentData, Player, SkeletalMeshComponent,
						AnimParams.SlotName, AnimParams.Section, AnimParams.Animation, AnimParams.EvalTime, AnimParams.BlendWeight,
						bLooping, Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing, bFireNotifies && !AnimParams.bSkipAnimNotifiers,
						AnimParams.bForceCustomMode
					);
				}
			}

			// If the skeletal component has already ticked this frame because tick prerequisites weren't set up yet or a new binding was created, forcibly tick this component to update.
			// This resolves first frame issues where the skeletal component ticks first, then the sequencer binding is resolved which sets up tick prerequisites
			// for the next frame.
			if (SkeletalMeshComponent->PoseTickedThisFrame() || (SequencerInstance && SequencerInstance != ExistingAnimInstance))
			{
				SkeletalMeshComponent->TickAnimation(0.f, false);

				SkeletalMeshComponent->RefreshBoneTransforms();
				SkeletalMeshComponent->RefreshSlaveComponents();
				SkeletalMeshComponent->UpdateComponentToWorld();
				SkeletalMeshComponent->FinalizeBoneTransform();
				SkeletalMeshComponent->MarkRenderTransformDirty();
				SkeletalMeshComponent->MarkRenderDynamicDataDirty();
			}

			Player.PreAnimatedState.SetCaptureEntity(FMovieSceneEvaluationKey(), EMovieSceneCompletionMode::KeepState);
		}

	private:

		static USkeletalMeshComponent* SkeletalMeshComponentFromObject(UObject* InObject)
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObject);
			if (SkeletalMeshComponent)
			{
				return SkeletalMeshComponent;
			}
			// then check to see if we are controlling an actor & if so use its first USkeletalMeshComponent 
			else if (AActor* Actor = Cast<AActor>(InObject))
			{
				return Actor->FindComponentByClass<USkeletalMeshComponent>();
			}
			return nullptr;
		}

		void SetAnimPosition(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player, USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, FObjectKey Section, UAnimSequenceBase* InAnimSequence, float InPosition, float Weight, bool bLooping, bool bPlaying, bool bFireNotifies, bool bForceCustomMode)
		{
			if (!CanPlayAnimation(SkeletalMeshComponent, InAnimSequence))
			{
				return;
			}
			if (bForceCustomMode)
			{
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
			}
			UAnimSequencerInstance* SequencerInst = Cast<UAnimSequencerInstance>(SkeletalMeshComponent->GetAnimInstance());
			if (SequencerInst)
			{
				FMovieSceneAnimTypeID AnimTypeID = SectionToAnimationIDs.GetAnimTypeID(Section);

				Player.SavePreAnimatedState(*SequencerInst, AnimTypeID, FStatelessPreAnimatedTokenProducer(&ResetAnimSequencerInstance));

				// Set position and weight
				SequencerInst->UpdateAnimTrack(InAnimSequence, GetTypeHash(AnimTypeID), InPosition, Weight, bFireNotifies);
			}
			else if (UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance())
			{
				FMontagePlayerPerSectionData* SectionData = MontageData.Find(Section);

				int32 InstanceId = (SectionData) ? SectionData->MontageInstanceId : INDEX_NONE;
				TWeakObjectPtr<UAnimMontage> Montage = FAnimMontageInstance::SetSequencerMontagePosition(SlotName, SkeletalMeshComponent, InstanceId, InAnimSequence, InPosition, Weight, bLooping, bPlaying);

				if (Montage.IsValid())
				{
					FMontagePlayerPerSectionData& DataContainer = MontageData.FindOrAdd(Section);
					DataContainer.Montage = Montage;
					DataContainer.MontageInstanceId = InstanceId;

					FMovieSceneAnimTypeID SlotTypeID = SectionToAnimationIDs.GetAnimTypeID(Section);
					Player.SavePreAnimatedState(*Montage.Get(), SlotTypeID, FStopPlayingMontageTokenProducer(AnimInst, InstanceId));

					// make sure it's playing if the sequence is
					FAnimMontageInstance* Instance = AnimInst->GetMontageInstanceForID(InstanceId);
					Instance->bPlaying = bPlaying;
				}
			}
		}

		void PreviewSetAnimPosition(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player, USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, FObjectKey Section, UAnimSequenceBase* InAnimSequence, float InPosition, float Weight, bool bLooping, bool bFireNotifies, float DeltaTime, bool bPlaying, bool bResetDynamics, bool bForceCustomMode)
		{
			if (!CanPlayAnimation(SkeletalMeshComponent, InAnimSequence))
			{
				return;
			}
			if (bForceCustomMode)
			{
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
			}
			UAnimSequencerInstance* SequencerInst = Cast<UAnimSequencerInstance>(SkeletalMeshComponent->GetAnimInstance());
			if (SequencerInst)
			{
				// Unique anim type ID per slot
				FMovieSceneAnimTypeID AnimTypeID = SectionToAnimationIDs.GetAnimTypeID(Section);
				Player.SavePreAnimatedState(*SequencerInst, AnimTypeID, FStatelessPreAnimatedTokenProducer(&ResetAnimSequencerInstance));

				// Set position and weight
				SequencerInst->UpdateAnimTrack(InAnimSequence, GetTypeHash(AnimTypeID), InPosition, Weight, bFireNotifies);
			}
			else if (UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance())
			{
				FMontagePlayerPerSectionData* SectionData = MontageData.Find(Section);

				int32 InstanceId = (SectionData)? SectionData->MontageInstanceId : INDEX_NONE;
				TWeakObjectPtr<UAnimMontage> Montage = FAnimMontageInstance::PreviewSequencerMontagePosition(SlotName, SkeletalMeshComponent, InstanceId, InAnimSequence, InPosition, Weight, bLooping, bFireNotifies, bPlaying);

				if (Montage.IsValid())
				{
					FMontagePlayerPerSectionData& DataContainer = MontageData.FindOrAdd(Section);
					DataContainer.Montage = Montage;
					DataContainer.MontageInstanceId = InstanceId;

					FMovieSceneAnimTypeID AnimTypeID = SectionToAnimationIDs.GetAnimTypeID(InAnimSequence);
					Player.SavePreAnimatedState(*Montage.Get(), AnimTypeID, FStopPlayingMontageTokenProducer(AnimInst, InstanceId));

					FAnimMontageInstance* Instance = AnimInst->GetMontageInstanceForID(InstanceId);
					Instance->bPlaying = bPlaying;
				}
	
				if (bResetDynamics)
				{
					// make sure we reset any simulations
					AnimInst->ResetDynamics(ETeleportType::ResetPhysics);
				}
			}
		}

		TMovieSceneAnimTypeIDContainer<FObjectKey> SectionToAnimationIDs;
		TMap<FObjectKey, FMontagePlayerPerSectionData> MontageData;
	};

}	// namespace MovieScene

template<> FMovieSceneAnimTypeID GetBlendingDataType<MovieScene::FBlendedAnimation>()
{
	static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}

FMovieSceneSkeletalAnimationSectionTemplate::FMovieSceneSkeletalAnimationSectionTemplate(const UMovieSceneSkeletalAnimationSection& InSection)
	: Params(InSection.Params, InSection.GetInclusiveStartFrame(), InSection.GetExclusiveEndFrame())
{
}

void FMovieSceneSkeletalAnimationSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if (Params.Animation)
	{
		// calculate the time at which to evaluate the animation
		float EvalTime = Params.MapTimeToAnimation(Context.GetTime(), Context.GetFrameRate());

		float ManualWeight = 1.f;
		Params.Weight.Evaluate(Context.GetTime(), ManualWeight);

		const float Weight = ManualWeight * EvaluateEasing(Context.GetTime());

		FOptionalMovieSceneBlendType BlendType = GetSourceSection()->GetBlendType();
		check(BlendType.IsValid());

		// Ensure the accumulator knows how to actually apply component transforms
		FMovieSceneBlendingActuatorID ActuatorTypeID = MovieScene::FComponentAnimationActuator::GetActuatorTypeID();
		FMovieSceneBlendingAccumulator& Accumulator = ExecutionTokens.GetBlendingAccumulator();
		if (!Accumulator.FindActuator<MovieScene::FBlendedAnimation>(ActuatorTypeID))
		{
			Accumulator.DefineActuator(ActuatorTypeID, MakeShared<MovieScene::FComponentAnimationActuator>());
		}

		// Add the blendable to the accumulator
		FMinimalAnimParameters AnimParams(
			Params.Animation, EvalTime, Weight, ExecutionTokens.GetCurrentScope(), Params.SlotName, GetSourceSection(), Params.bSkipAnimNotifiers, 
			Params.bForceCustomMode
		);
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<MovieScene::FBlendedAnimation>(AnimParams, BlendType.Get(), 1.f));
	}
}

float FMovieSceneSkeletalAnimationSectionTemplateParameters::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
	InPosition = FMath::Clamp(InPosition, FFrameTime(SectionStartTime), FFrameTime(SectionEndTime-1));
	
	const float SectionPlayRate = PlayRate * Animation->RateScale;
	const float AnimPlayRate = FMath::IsNearlyZero(SectionPlayRate) ? 1.0f : SectionPlayRate;

	const float SeqLength = GetSequenceLength() - InFrameRate.AsSeconds(StartFrameOffset + EndFrameOffset);

	float AnimPosition = FFrameTime::FromDecimal((InPosition - SectionStartTime).AsDecimal() * AnimPlayRate) / InFrameRate;
	if (SeqLength > 0.f)
	{
		AnimPosition = FMath::Fmod(AnimPosition, SeqLength);
	}
	AnimPosition += InFrameRate.AsSeconds(StartFrameOffset);
	if (bReverse)
	{
		AnimPosition = (SeqLength - (AnimPosition - InFrameRate.AsSeconds(StartFrameOffset))) + InFrameRate.AsSeconds(StartFrameOffset);
	}

	return AnimPosition;
}

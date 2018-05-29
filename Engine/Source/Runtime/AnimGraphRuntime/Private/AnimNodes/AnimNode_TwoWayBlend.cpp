// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// FAnimNode_TwoWayBlend

void FAnimNode_TwoWayBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	A.Initialize(Context);
	B.Initialize(Context);

	bAIsRelevant = false;
	bBIsRelevant = false;

	AlphaBoolBlend.Reinitialize();
	AlphaScaleBiasClamp.Reinitialize();
}

void FAnimNode_TwoWayBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	A.CacheBones(Context);
	B.CacheBones(Context);
}

void FAnimNode_TwoWayBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnimationNode_TwoWayBlend_Update);
	EvaluateGraphExposedInputs.Execute(Context);

	InternalBlendAlpha = 0.f;
	switch (AlphaInputType)
	{
	case EAnimAlphaInputType::Float:
		InternalBlendAlpha = AlphaScaleBias.ApplyTo(AlphaScaleBiasClamp.ApplyTo(Alpha, Context.GetDeltaTime()));
		break;
	case EAnimAlphaInputType::Bool:
		InternalBlendAlpha = AlphaBoolBlend.ApplyTo(bAlphaBoolEnabled, Context.GetDeltaTime());
		break;
	case EAnimAlphaInputType::Curve:
		if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
		{
			InternalBlendAlpha = AlphaScaleBiasClamp.ApplyTo(AnimInstance->GetCurveValue(AlphaCurveName), Context.GetDeltaTime());
		}
		break;
	};

	// Make sure Alpha is clamped between 0 and 1.
	InternalBlendAlpha = FMath::Clamp<float>(InternalBlendAlpha, 0.f, 1.f);

	const bool bNewAIsRelevant = !FAnimWeight::IsFullWeight(InternalBlendAlpha);
	const bool bNewBIsRelevant = FAnimWeight::IsRelevant(InternalBlendAlpha);

	// when this flag is true, we'll reinitialize the children
	if (bResetChildOnActivation)
	{
		if (bNewAIsRelevant && !bAIsRelevant)
		{
			FAnimationInitializeContext ReinitializeContext(Context.AnimInstanceProxy);

			// reinitialize
			A.Initialize(ReinitializeContext);
		}

		if (bNewBIsRelevant && !bBIsRelevant)
		{
			FAnimationInitializeContext ReinitializeContext(Context.AnimInstanceProxy);

			// reinitialize
			B.Initialize(ReinitializeContext);
		}
	}

	bAIsRelevant = bNewAIsRelevant;
	bBIsRelevant = bNewBIsRelevant;

	if (bBIsRelevant)
	{
		if (bAIsRelevant)
		{
			// Blend A and B together
			A.Update(Context.FractionalWeight(1.0f - InternalBlendAlpha));
			B.Update(Context.FractionalWeight(InternalBlendAlpha));
		}
		else
		{
			// Take all of B
			B.Update(Context);
		}
	}
	else
	{
		// Take all of A
		A.Update(Context);
	}
}

void FAnimNode_TwoWayBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	if (bBIsRelevant)
	{
		if (bAIsRelevant)
		{
			FPoseContext Pose1(Output);
			FPoseContext Pose2(Output);

			A.Evaluate(Pose1);
			B.Evaluate(Pose2);

			FAnimationRuntime::BlendTwoPosesTogether(Pose1.Pose, Pose2.Pose, Pose1.Curve, Pose2.Curve, (1.0f - InternalBlendAlpha), Output.Pose, Output.Curve);
		}
		else
		{
			B.Evaluate(Output);
		}
	}
	else
	{
		A.Evaluate(Output);
	}
}


void FAnimNode_TwoWayBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), InternalBlendAlpha *100);
	DebugData.AddDebugItem(DebugLine);

	A.GatherDebugData(DebugData.BranchFlow(1.f - InternalBlendAlpha));
	B.GatherDebugData(DebugData.BranchFlow(InternalBlendAlpha));
}

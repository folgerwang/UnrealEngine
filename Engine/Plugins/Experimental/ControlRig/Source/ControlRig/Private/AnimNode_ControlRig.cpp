// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRig.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameFramework/Actor.h"
#include "Animation/NodeMappingContainer.h"
#include "AnimationRuntime.h"

FAnimNode_ControlRig::FAnimNode_ControlRig()
{
}

void FAnimNode_ControlRig::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	if (ControlRigClass)
	{
		ControlRig = NewObject<UControlRig>(InAnimInstance->GetOwningComponent(), ControlRigClass);
	}

	FAnimNode_ControlRigBase::OnInitializeAnimInstance(InProxy, InAnimInstance);
}

void FAnimNode_ControlRig::GatherDebugData(FNodeDebugData& DebugData)
{
	FAnimNode_ControlRigBase::GatherDebugData(DebugData);
}

void FAnimNode_ControlRig::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	FAnimNode_ControlRigBase::Update_AnyThread(Context);

	Source.Update(Context);
}

void FAnimNode_ControlRig::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_ControlRigBase::Initialize_AnyThread(Context);

	Source.Initialize(Context);
}

void FAnimNode_ControlRig::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_ControlRigBase::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);
}

void FAnimNode_ControlRig::Evaluate_AnyThread(FPoseContext & Output)
{
	// If not playing a montage, just pass through
	Source.Evaluate(Output);

	// evaluate 
	FAnimNode_ControlRigBase::Evaluate_AnyThread(Output);
}

void FAnimNode_ControlRig::PostSerialize(const FArchive& Ar)
{
	// after compile, we have to reinitialize
	// because it needs new execution code
	// since memory has changed
	if (Ar.IsObjectReferenceCollector())
	{
		if (ControlRig)
		{
			ControlRig->Initialize();
		}
	}
}
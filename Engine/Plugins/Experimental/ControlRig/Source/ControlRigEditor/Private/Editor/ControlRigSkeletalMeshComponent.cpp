// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSkeletalMeshComponent.h"
#include "Sequencer/ControlRigSequencerAnimInstance.h"
#include "SkeletalDebugRendering.h"
#include "ControlRig.h"

UControlRigSkeletalMeshComponent::UControlRigSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DebugDrawSkeleton(false)
{
	SetDisablePostProcessBlueprint(true);
}

void UControlRigSkeletalMeshComponent::InitAnim(bool bForceReinit)
{
	// skip preview init entirely, just init the super class
	USkeletalMeshComponent::InitAnim(bForceReinit);

	RebuildDebugDrawSkeleton();
}

void UControlRigSkeletalMeshComponent::ShowReferencePose(bool bRefPose)
{
	UControlRigSequencerAnimInstance* ControlRigInstance = Cast<UControlRigSequencerAnimInstance>(GetAnimInstance());

	if (ControlRigInstance && ControlRigInstance->CachedControlRig.IsValid())
	{
		UControlRig* ControlRig = ControlRigInstance->CachedControlRig.Get();
		ControlRig->bExecutionOn = !bRefPose;
	}
}

bool UControlRigSkeletalMeshComponent::IsReferencePoseShown() const
{
	UControlRigSequencerAnimInstance* ControlRigInstance = Cast<UControlRigSequencerAnimInstance>(GetAnimInstance());

	if (ControlRigInstance && ControlRigInstance->CachedControlRig.IsValid())
	{
		UControlRig* ControlRig = ControlRigInstance->CachedControlRig.Get();
		return !ControlRig->bExecutionOn;
	}

	return false;
}

void UControlRigSkeletalMeshComponent::SetCustomDefaultPose()
{
	ShowReferencePose(false);
}

void UControlRigSkeletalMeshComponent::RebuildDebugDrawSkeleton()
{
	UControlRigSequencerAnimInstance* ControlRigInstance = Cast<UControlRigSequencerAnimInstance>(GetAnimInstance());

	if (ControlRigInstance && ControlRigInstance->CachedControlRig.IsValid())
	{
		UControlRig* ControlRig = ControlRigInstance->CachedControlRig.Get();
		// just copy it because this is not thread safe
		const FRigHierarchy BaseHiearchy = ControlRig->GetBaseHierarchy();

		DebugDrawSkeleton.Empty();
		DebugDrawBones.Reset();

		// create ref modifier
 		FReferenceSkeletonModifier RefSkelModifier(DebugDrawSkeleton, nullptr);
 
 		for (int32 Index = 0; Index < BaseHiearchy.GetNum(); Index++)
 		{
			FMeshBoneInfo NewMeshBoneInfo;
			NewMeshBoneInfo.Name = BaseHiearchy.GetName(Index);
			NewMeshBoneInfo.ParentIndex = BaseHiearchy.GetParentIndex(Index);
			// give ref pose here
			RefSkelModifier.Add(NewMeshBoneInfo, BaseHiearchy.GetInitialTransform(Index));

			DebugDrawBones.Add(Index);
		}
	}
}

FTransform UControlRigSkeletalMeshComponent::GetDrawTransform(int32 BoneIndex) const
{
	UControlRigSequencerAnimInstance* ControlRigInstance = Cast<UControlRigSequencerAnimInstance>(GetAnimInstance());

	if (ControlRigInstance && ControlRigInstance->CachedControlRig.IsValid())
	{
		UControlRig* ControlRig = ControlRigInstance->CachedControlRig.Get();
		// just copy it because this is not thread safe
		const FRigHierarchy& BaseHiearchy = ControlRig->GetBaseHierarchy();
		return BaseHiearchy.GetGlobalTransform(BoneIndex);
	}

	return FTransform::Identity;
}
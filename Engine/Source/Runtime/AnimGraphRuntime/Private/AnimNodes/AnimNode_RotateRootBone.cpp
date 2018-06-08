// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_RotateRootBone.h"

/////////////////////////////////////////////////////
// FAnimNode_RotateRootBone

void FAnimNode_RotateRootBone::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	BasePose.Initialize(Context);

	PitchScaleBiasClamp.Reinitialize();
	YawScaleBiasClamp.Reinitialize();
}

void FAnimNode_RotateRootBone::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	BasePose.CacheBones(Context);
}

void FAnimNode_RotateRootBone::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
	BasePose.Update(Context);

	ActualPitch = PitchScaleBiasClamp.ApplyTo(Pitch, Context.GetDeltaTime());
	ActualYaw = YawScaleBiasClamp.ApplyTo(Yaw, Context.GetDeltaTime());
}

void FAnimNode_RotateRootBone::Evaluate_AnyThread(FPoseContext& Output)
{
	// Evaluate the input
	BasePose.Evaluate(Output);

	checkSlow(!FMath::IsNaN(ActualYaw) && FMath::IsFinite(ActualYaw));
	checkSlow(!FMath::IsNaN(ActualPitch) && FMath::IsFinite(ActualPitch));

	if (!FMath::IsNearlyZero(ActualPitch, KINDA_SMALL_NUMBER) || !FMath::IsNearlyZero(ActualYaw, KINDA_SMALL_NUMBER))
	{
		// Build our desired rotation
		const FRotator DeltaRotation(ActualPitch, ActualYaw, 0.f);
		const FQuat DeltaQuat(DeltaRotation);
		const FQuat MeshToComponentQuat(MeshToComponent);

		// Convert our rotation from Component Space to Mesh Space.
		const FQuat MeshSpaceDeltaQuat = MeshToComponentQuat.Inverse() * DeltaQuat * MeshToComponentQuat;

		// Apply rotation to root bone.
		FCompactPoseBoneIndex RootBoneIndex(0);
		Output.Pose[RootBoneIndex].SetRotation(Output.Pose[RootBoneIndex].GetRotation() * MeshSpaceDeltaQuat);
		Output.Pose[RootBoneIndex].NormalizeRotation();
	}
}


void FAnimNode_RotateRootBone::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += FString::Printf(TEXT("Pitch(%.2f) Yaw(%.2f)"), ActualPitch, ActualYaw);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData);
}

FAnimNode_RotateRootBone::FAnimNode_RotateRootBone()
	: Pitch(0.0f)
	, Yaw(0.0f)
	, MeshToComponent(FRotator::ZeroRotator)
	, ActualPitch(0.f)
	, ActualYaw(0.f)
{
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_ApplyLimits.h"
#include "AnimationCoreLibrary.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// FAnimNode_ApplyLimits

FAnimNode_ApplyLimits::FAnimNode_ApplyLimits()
{
}

void FAnimNode_ApplyLimits::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(")"));
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

bool ConstrainAngularRange(FQuat& InOutQuatRotation, const FQuat& InRefRotation, const FVector& InLimitMinDegrees, const FVector& InLimitMaxDegrees)
{
	// Simple clamping of euler angles. This might be better off refactored to use switch/twist decomposition and maybe an ellipsoid clamp

	FQuat DeltaQuat = InRefRotation * InOutQuatRotation.Inverse();
	FRotator DeltaRotator = DeltaQuat.Rotator();
	FRotator NewRotator;
	NewRotator.Pitch = FMath::Clamp(DeltaRotator.Pitch, InLimitMinDegrees.Y, InLimitMaxDegrees.Y);
	NewRotator.Yaw = FMath::Clamp(DeltaRotator.Yaw, InLimitMinDegrees.X, InLimitMaxDegrees.X);
	NewRotator.Roll = FMath::Clamp(DeltaRotator.Roll, InLimitMinDegrees.Z, InLimitMaxDegrees.Z);
	DeltaQuat = FQuat(NewRotator);

	InOutQuatRotation = DeltaQuat.Inverse() * InRefRotation;

	return !NewRotator.Equals(DeltaRotator);
}

void FAnimNode_ApplyLimits::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	checkSlow(OutBoneTransforms.Num() == 0);

	FPoseContext LocalPose0(Output.AnimInstanceProxy);
	FPoseContext LocalPose1(Output.AnimInstanceProxy);
	Output.Pose.ConvertToLocalPoses(LocalPose0.Pose);
	Output.Pose.ConvertToLocalPoses(LocalPose1.Pose);
	LocalPose0.Curve = Output.Curve;
	LocalPose1.Curve = Output.Curve;

	const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
	const FBoneContainer& BoneContainer = LocalPose0.Pose.GetBoneContainer();

	bool bAppliedLimit = false;
	const int32 AngularLimitCount = AngularRangeLimits.Num();
	for (int32 AngularLimitIndex = 0; AngularLimitIndex < AngularLimitCount; ++AngularLimitIndex)
	{
		const FAngularRangeLimit& AngularLimit = AngularRangeLimits[AngularLimitIndex];
		const FCompactPoseBoneIndex BoneIndex = AngularLimit.Bone.GetCompactPoseIndex(BoneContainer);

		FTransform& BoneTransform = LocalPose0.Pose[BoneIndex];

		const FTransform& RefBoneTransform = BoneContainer.GetRefPoseTransform(BoneIndex);

		FQuat BoneRotation = BoneTransform.GetRotation();
		if (ConstrainAngularRange(BoneRotation, RefBoneTransform.GetRotation(), AngularLimit.LimitMin + AngularOffsets[AngularLimitIndex], AngularLimit.LimitMax + AngularOffsets[AngularLimitIndex]))
		{
			BoneTransform.SetRotation(BoneRotation);
			bAppliedLimit = true;
		}
	}

	if(bAppliedLimit)
	{
		const float BlendWeight = FMath::Clamp<float>(ActualAlpha, 0.f, 1.f);

		FPoseContext BlendedPose(Output.AnimInstanceProxy);
		FAnimationRuntime::BlendTwoPosesTogether(LocalPose0.Pose, LocalPose1.Pose, LocalPose0.Curve, LocalPose1.Curve, BlendWeight, BlendedPose.Pose, BlendedPose.Curve);

 		Output.Pose.InitPose(BlendedPose.Pose);
	 	Output.Curve = BlendedPose.Curve;
	}
}

bool FAnimNode_ApplyLimits::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	for (FAngularRangeLimit& AngularLimit : AngularRangeLimits)
	{
		if (AngularLimit.Bone.IsValidToEvaluate(RequiredBones))
		{
			return true;
		}
	}

	return false;
}

void FAnimNode_ApplyLimits::RecalcLimits()
{
	AngularOffsets.SetNumZeroed(AngularRangeLimits.Num());
}

void FAnimNode_ApplyLimits::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	RecalcLimits();
}

void FAnimNode_ApplyLimits::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	for (FAngularRangeLimit& AngularLimit : AngularRangeLimits)
	{
		AngularLimit.Bone.Initialize(RequiredBones);
	}

	RecalcLimits();
}


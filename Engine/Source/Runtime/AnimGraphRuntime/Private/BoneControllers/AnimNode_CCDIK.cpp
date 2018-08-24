// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_CCDIK.h"
#include "Animation/AnimTypes.h"
#include "AnimationRuntime.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstanceProxy.h"

PRAGMA_DISABLE_OPTIMIZATION
/////////////////////////////////////////////////////
// AnimNode_CCDIK
// Implementation of the CCDIK IK Algorithm

FAnimNode_CCDIK::FAnimNode_CCDIK()
	: EffectorLocation(FVector::ZeroVector)
	, EffectorLocationSpace(BCS_ComponentSpace)
	, Precision(1.f)
	, MaxIterations(10)
	, bStartFromTail(true)
	, bEnableRotationLimit(false)
{
}

FVector FAnimNode_CCDIK::GetCurrentLocation(FCSPose<FCompactPose>& MeshBases, const FCompactPoseBoneIndex& BoneIndex)
{
	return MeshBases.GetComponentSpaceTransform(BoneIndex).GetLocation();
}

FTransform FAnimNode_CCDIK::GetTargetTransform(const FTransform& InComponentTransform, FCSPose<FCompactPose>& MeshBases, FBoneSocketTarget& InTarget, EBoneControlSpace Space, const FVector& InOffset)
{
	FTransform OutTransform;
	if (Space == BCS_BoneSpace)
	{
		OutTransform = InTarget.GetTargetTransform(InOffset, MeshBases, InComponentTransform);
	}
	else
	{
		// parent bone space still goes through this way
		// if your target is socket, it will try find parents of joint that socket belongs to
		OutTransform.SetLocation(InOffset);
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(InComponentTransform, MeshBases, OutTransform, InTarget.GetCompactPoseBoneIndex(), Space);
	}

	return OutTransform;
}

void FAnimNode_CCDIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	// Update EffectorLocation if it is based off a bone position
	FTransform CSEffectorTransform = GetTargetTransform(Output.AnimInstanceProxy->GetComponentTransform(), Output.Pose, EffectorTarget, EffectorLocationSpace, EffectorLocation);
	FVector const CSEffectorLocation = CSEffectorTransform.GetLocation();

	// Gather all bone indices between root and tip.
	TArray<FCompactPoseBoneIndex> BoneIndices;
	
	{
		const FCompactPoseBoneIndex RootIndex = RootBone.GetCompactPoseIndex(BoneContainer);
		FCompactPoseBoneIndex BoneIndex = TipBone.GetCompactPoseIndex(BoneContainer);
		do
		{
			BoneIndices.Insert(BoneIndex, 0);
			BoneIndex = Output.Pose.GetPose().GetParentBoneIndex(BoneIndex);
		} while (BoneIndex != RootIndex);
		BoneIndices.Insert(BoneIndex, 0);
	}

	// Gather transforms
	int32 const NumTransforms = BoneIndices.Num();
	OutBoneTransforms.AddUninitialized(NumTransforms);

	// Gather chain links. These are non zero length bones.
	TArray<CCDIKChainLink> Chain;
	Chain.Reserve(NumTransforms);
	// Start with Root Bone
	{
		const FCompactPoseBoneIndex& RootBoneIndex = BoneIndices[0];
		const FTransform& LocalTransform = Output.Pose.GetLocalSpaceTransform(RootBoneIndex);
		const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(RootBoneIndex);

		OutBoneTransforms[0] = FBoneTransform(RootBoneIndex, BoneCSTransform);
		Chain.Add(CCDIKChainLink(BoneCSTransform, LocalTransform, RootBoneIndex, 0));
	}

	// Go through remaining transforms
	for (int32 TransformIndex = 1; TransformIndex < NumTransforms; TransformIndex++)
	{
		const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];

		const FTransform& LocalTransform = Output.Pose.GetLocalSpaceTransform(BoneIndex);
		const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(BoneIndex);
		FVector const BoneCSPosition = BoneCSTransform.GetLocation();

		OutBoneTransforms[TransformIndex] = FBoneTransform(BoneIndex, BoneCSTransform);

		// Calculate the combined length of this segment of skeleton
		float const BoneLength = FVector::Dist(BoneCSPosition, OutBoneTransforms[TransformIndex - 1].Transform.GetLocation());

		if (!FMath::IsNearlyZero(BoneLength))
		{
			Chain.Add(CCDIKChainLink(BoneCSTransform, LocalTransform, BoneIndex, TransformIndex));
		}
		else
		{
			// Mark this transform as a zero length child of the last link.
			// It will inherit position and delta rotation from parent link.
			CCDIKChainLink & ParentLink = Chain[Chain.Num() - 1];
			ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
		}
	}

	bool bBoneLocationUpdated = false;
	int32 const NumChainLinks = BoneIndices.Num();

	// iterate
	{
		int32 const TipBoneLinkIndex = NumChainLinks - 1;

		// @todo optimize locally if no update, stop?
		bool bLocalUpdated = false;
		// check how far
		const FVector TargetPos = CSEffectorLocation;
		FVector TipPos = Chain[TipBoneLinkIndex].Transform.GetLocation();
		float Distance = FVector::Dist(TargetPos, TipPos);
		int32 IterationCount = 0;
		while ((Distance > Precision) && (IterationCount++ < MaxIterations))
		{
			// iterate from tip to root
			if (bStartFromTail)
			{
				for (int32 LinkIndex = TipBoneLinkIndex - 1; LinkIndex > 0; --LinkIndex)
				{
					bLocalUpdated |= UpdateChainLink(Chain, LinkIndex, TargetPos);
				}
			}
			else
			{
				for (int32 LinkIndex = 1; LinkIndex < TipBoneLinkIndex; ++LinkIndex)
				{
					bLocalUpdated |= UpdateChainLink(Chain, LinkIndex, TargetPos);
				}
			}

			Distance = FVector::Dist(Chain[TipBoneLinkIndex].Transform.GetLocation(), CSEffectorLocation);

			bBoneLocationUpdated |= bLocalUpdated;

			// no more update in this iteration
			if (!bLocalUpdated)
			{
				break;
			}
		}
	}

	// If we moved some bones, update bone transforms.
	if (bBoneLocationUpdated)
	{
		// First step: update bone transform positions from chain links.
		for (int32 LinkIndex = 0; LinkIndex < NumChainLinks; LinkIndex++)
		{
			CCDIKChainLink const & ChainLink = Chain[LinkIndex];
			OutBoneTransforms[ChainLink.TransformIndex].Transform = ChainLink.Transform;

			// If there are any zero length children, update position of those
			int32 const NumChildren = ChainLink.ChildZeroLengthTransformIndices.Num();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				OutBoneTransforms[ChainLink.ChildZeroLengthTransformIndices[ChildIndex]].Transform = ChainLink.Transform;
			}
		}

#if WITH_EDITOR
		DebugLines.Reset(OutBoneTransforms.Num());
		DebugLines.AddUninitialized(OutBoneTransforms.Num());
		for (int32 Index = 0; Index < OutBoneTransforms.Num(); ++Index)
		{
			DebugLines[Index] = OutBoneTransforms[Index].Transform.GetLocation();
		}
#endif // WITH_EDITOR

	}
}

bool FAnimNode_CCDIK::UpdateChainLink(TArray<CCDIKChainLink>& Chain, int32 LinkIndex, const FVector& TargetPos) const
{
	int32 const TipBoneLinkIndex = Chain.Num() - 1;

	ensure(Chain.IsValidIndex(TipBoneLinkIndex));
	CCDIKChainLink& CurrentLink = Chain[LinkIndex];

	// update new tip pos
	FVector TipPos = Chain[TipBoneLinkIndex].Transform.GetLocation();

	FTransform& CurrentLinkTransform = CurrentLink.Transform;
	FVector ToEnd = TipPos - CurrentLinkTransform.GetLocation();
	FVector ToTarget = TargetPos - CurrentLinkTransform.GetLocation();

	ToEnd.Normalize();
	ToTarget.Normalize();

	float RotationLimitPerJointInRadian = FMath::DegreesToRadians(RotationLimitPerJoints[LinkIndex]);
	float Angle = FMath::ClampAngle(FMath::Acos(FVector::DotProduct(ToEnd, ToTarget)), -RotationLimitPerJointInRadian, RotationLimitPerJointInRadian);
	bool bCanRotate = (FMath::Abs(Angle) > KINDA_SMALL_NUMBER) && (!bEnableRotationLimit || RotationLimitPerJointInRadian > CurrentLink.CurrentAngleDelta);
	if (bCanRotate)
	{
		// check rotation limit first, if fails, just abort
		if (bEnableRotationLimit)
		{
			if (RotationLimitPerJointInRadian < CurrentLink.CurrentAngleDelta + Angle)
			{
				Angle = RotationLimitPerJointInRadian - CurrentLink.CurrentAngleDelta;
				if (Angle <= KINDA_SMALL_NUMBER)
				{
					return false;
				}
			}

			CurrentLink.CurrentAngleDelta += Angle;
		}

		// continue with rotating toward to target
		FVector RotationAxis = FVector::CrossProduct(ToEnd, ToTarget);
		if (RotationAxis.SizeSquared() > 0.f)
		{
			RotationAxis.Normalize();
			// Delta Rotation is the rotation to target
			FQuat DeltaRotation(RotationAxis, Angle);

			FQuat NewRotation = DeltaRotation * CurrentLinkTransform.GetRotation();
			NewRotation.Normalize();
			CurrentLinkTransform.SetRotation(NewRotation);

			// if I have parent, make sure to refresh local transform since my current transform has changed
			if (LinkIndex > 0)
			{
				CCDIKChainLink const & Parent = Chain[LinkIndex - 1];
				CurrentLink.LocalTransform = CurrentLinkTransform.GetRelativeTransform(Parent.Transform);
				CurrentLink.LocalTransform.NormalizeRotation();
			}

			// now update all my children to have proper transform
			FTransform CurrentParentTransform = CurrentLinkTransform;

			// now update all chain
			for (int32 ChildLinkIndex = LinkIndex + 1; ChildLinkIndex <= TipBoneLinkIndex; ++ChildLinkIndex)
			{
				CCDIKChainLink& ChildIterLink = Chain[ChildLinkIndex];
				FCompactPoseBoneIndex ChildBoneIndex = ChildIterLink.BoneIndex;
				const FTransform LocalTransform = ChildIterLink.LocalTransform;
				ChildIterLink.Transform = LocalTransform * CurrentParentTransform;
				ChildIterLink.Transform.NormalizeRotation();
				CurrentParentTransform = ChildIterLink.Transform;
			}

			return true;
		}
	}

	return false;
}

bool FAnimNode_CCDIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (EffectorLocationSpace == BCS_ParentBoneSpace || EffectorLocationSpace == BCS_BoneSpace)
	{
		if (!EffectorTarget.IsValidToEvaluate(RequiredBones))
		{
			return false;
		}
	}

	// Allow evaluation if all parameters are initialized and TipBone is child of RootBone
	return
		(
		TipBone.IsValidToEvaluate(RequiredBones)
		&& RootBone.IsValidToEvaluate(RequiredBones)
		&& Precision > 0
		&& RequiredBones.BoneIsChildOf(TipBone.BoneIndex, RootBone.BoneIndex)
		);
}

#if WITH_EDITOR
void FAnimNode_CCDIK::ResizeRotationLimitPerJoints(int32 NewSize)
{
	if (NewSize == 0)
	{
		RotationLimitPerJoints.Reset();
	}
	else if (RotationLimitPerJoints.Num() != NewSize)
	{
		int32 StartIndex = RotationLimitPerJoints.Num();
		RotationLimitPerJoints.SetNum(NewSize);
		for (int32 Index = StartIndex; Index < RotationLimitPerJoints.Num(); ++Index)
		{
			RotationLimitPerJoints[Index] = 30.f;
		}
	}
}
#endif 

void FAnimNode_CCDIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	TipBone.Initialize(RequiredBones);
	RootBone.Initialize(RequiredBones);
	EffectorTarget.InitializeBoneReferences(RequiredBones);
}

void FAnimNode_CCDIK::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

PRAGMA_ENABLE_OPTIMIZATION
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRigBase.h"
#include "ControlRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/NodeMappingContainer.h"
#include "AnimationRuntime.h"

FAnimNode_ControlRigBase::FAnimNode_ControlRigBase()
{
}

void FAnimNode_ControlRigBase::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	FAnimNode_Base::OnInitializeAnimInstance(InProxy, InAnimInstance);

	USkeletalMeshComponent* Component = InAnimInstance->GetOwningComponent();
	UControlRig* ControlRig = GetControlRig();
	if (Component && Component->SkeletalMesh && ControlRig)
	{
		UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(ControlRig->GetClass());
		if (BlueprintClass)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy);
			NodeMappingContainer = Component->SkeletalMesh->GetNodeMappingContainer(Blueprint);
		}
	}
}

void FAnimNode_ControlRigBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	if (UControlRig* ControlRig = GetControlRig())
	{
		ControlRig->Initialize();
	}
}

void FAnimNode_ControlRigBase::GatherDebugData(FNodeDebugData& DebugData)
{

}

void FAnimNode_ControlRigBase::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	FAnimNode_Base::Update_AnyThread(Context);

	if (UControlRig* ControlRig = GetControlRig())
	{
		// @TODO: fix this to be thread-safe
		// Pre-update doesn't work for custom anim instances
		// FAnimNode_ControlRigExternalSource needs this to be called to reset to ref pose
		ControlRig->PreEvaluate_GameThread();
	}
}

void FAnimNode_ControlRigBase::UpdateInput(UControlRig* ControlRig, const FPoseContext& InOutput)
{
	const FBoneContainer& RequiredBones = InOutput.Pose.GetBoneContainer();

	// get component pose from control rig
	FCSPose<FCompactPose> MeshPoses;
	// first I need to convert to local pose
	MeshPoses.InitPose(InOutput.Pose);

	// should we also update refpose? 
	const int32 NumNodes = RigHierarchyItemNameMapping.Num();
	for (int32 Index = 0; Index < NumNodes; ++Index)
	{
		if (RigHierarchyItemNameMapping[Index] != NAME_None)
		{
			FCompactPoseBoneIndex CompactPoseIndex(Index);
			FTransform ComponentTransform = MeshPoses.GetComponentSpaceTransform(CompactPoseIndex);
			if (NodeMappingContainer.IsValid())
			{
				ComponentTransform = NodeMappingContainer->GetSourceToTargetTransform(RigHierarchyItemNameMapping[Index]).GetRelativeTransformReverse(ComponentTransform);
			}

			ControlRig->SetGlobalTransform(RigHierarchyItemNameMapping[Index], ComponentTransform);
		}
	}
}

void FAnimNode_ControlRigBase::UpdateOutput(const UControlRig* ControlRig, FPoseContext& InOutput)
{
	// copy output of the rig
	const FBoneContainer& RequiredBones = InOutput.Pose.GetBoneContainer();

	// get component pose from control rig
	FCSPose<FCompactPose> MeshPoses;
	MeshPoses.InitPose(InOutput.Pose);

	const int32 NumNodes = RigHierarchyItemNameMapping.Num();
	for (int32 Index = 0; Index < NumNodes; ++Index)
	{
		if (RigHierarchyItemNameMapping[Index] != NAME_None)
		{
			FCompactPoseBoneIndex CompactPoseIndex(Index);
			FTransform ComponentTransform = ControlRig->GetGlobalTransform(RigHierarchyItemNameMapping[Index]);
			if (NodeMappingContainer.IsValid())
			{
				ComponentTransform = NodeMappingContainer->GetSourceToTargetTransform(RigHierarchyItemNameMapping[Index]) * ComponentTransform;
			}

			MeshPoses.SetComponentSpaceTransform(CompactPoseIndex, ComponentTransform);
		}
	}

	// now convert to what you'd like
	// initialize with refpose
	InOutput.ResetToRefPose();
	for (int32 Index = 0; Index < NumNodes; ++Index)
	{
		if (RigHierarchyItemNameMapping[Index] != NAME_None)
		{
			FCompactPoseBoneIndex CompactPoseIndex(Index);
			InOutput.Pose[CompactPoseIndex] = MeshPoses.GetLocalSpaceTransform(CompactPoseIndex);
		}
	}
}

void FAnimNode_ControlRigBase::Evaluate_AnyThread(FPoseContext& Output)
{
	if (UControlRig* ControlRig = GetControlRig())
	{
		// first update input to the system
		UpdateInput(ControlRig, Output);
		// first evaluate control rig
		ControlRig->Evaluate_AnyThread();
		// now update output
		UpdateOutput(ControlRig, Output);
	}
	else
	{
		// apply refpose
		Output.ResetToRefPose();
	}
}

void FAnimNode_ControlRigBase::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	if (UControlRig* ControlRig = GetControlRig())
	{
		// fill up node names
		FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
		const TArray<FBoneIndexType>& RequiredBonesArray = RequiredBones.GetBoneIndicesArray();
		const int32 NumBones = RequiredBonesArray.Num();
		RigHierarchyItemNameMapping.Reset(NumBones);
		RigHierarchyItemNameMapping.AddDefaulted(NumBones);

		const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();

		// @todo: thread-safe? probably not in editor, but it may not be a big issue in editor
		if (NodeMappingContainer.IsValid())
		{
			// get target to source mapping table - this is reversed mapping table
			TMap<FName, FName> TargetToSourceMappingTable;
			NodeMappingContainer->GetTargetToSourceMappingTable(TargetToSourceMappingTable);

			// now fill up node name
			for (int32 Index = 0; Index < NumBones; ++Index)
			{
				// get bone name, and find reverse mapping
				FName TargetNodeName = RefSkeleton.GetBoneName(RequiredBonesArray[Index]);
				FName* SourceName = TargetToSourceMappingTable.Find(TargetNodeName);
				RigHierarchyItemNameMapping[Index] = (SourceName)? *SourceName : NAME_None;
			}

			UE_LOG(LogAnimation, Log, TEXT("%s : %d"), *GetNameSafe(ControlRig), RigHierarchyItemNameMapping.Num());
		}
		else
		{
			for (int32 Index = 0; Index < NumBones; ++Index)
			{
				RigHierarchyItemNameMapping[Index] = RefSkeleton.GetBoneName(RequiredBonesArray[Index]);
			}
		}

	}
}

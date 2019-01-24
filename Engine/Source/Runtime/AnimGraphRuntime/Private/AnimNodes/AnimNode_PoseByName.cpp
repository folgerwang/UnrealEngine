// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseByName.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

void FAnimNode_PoseByName::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_PoseHandler::Initialize_AnyThread(Context);
}

void FAnimNode_PoseByName::RebuildPoseList(const FBoneContainer& InBoneContainer, const UPoseAsset* InPoseAsset)
{
	PoseExtractContext.PoseCurves.Reset();
	const TArray<FSmartName>& PoseNames = InPoseAsset->GetPoseNames();
	const int32 PoseIndex = InPoseAsset->GetPoseIndexByName(PoseName);
	TArray<uint16> const& LUTIndex = InBoneContainer.GetUIDToArrayLookupTable();
	if (PoseIndex != INDEX_NONE && ensure(LUTIndex.IsValidIndex(PoseNames[PoseIndex].UID)) && LUTIndex[PoseNames[PoseIndex].UID] != MAX_uint16)
	{
		// we keep pose index as that is the fastest way to search when extracting pose asset
		PoseExtractContext.PoseCurves.Add(FPoseCurve(PoseIndex, PoseNames[PoseIndex].UID, 0.f));
	}
}

void FAnimNode_PoseByName::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);

	// update pose extraction context if the name differs
	if (CurrentPoseName != PoseName)
	{
		RebuildPoseList(Context.AnimInstanceProxy->GetRequiredBones(), PoseAsset);
		CurrentPoseName = PoseName;
	}
}

void FAnimNode_PoseByName::Evaluate_AnyThread(FPoseContext& Output)
{
	// make sure we have curve to eval
	if ((CurrentPoseAsset.IsValid()) && (PoseExtractContext.PoseCurves.Num() > 0) && (Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton())))
	{
		// we only have one 
		PoseExtractContext.PoseCurves[0].Value = PoseWeight;
		// only give pose curve, we don't set any more curve here
		CurrentPoseAsset->GetAnimationPose(Output.Pose, Output.Curve, PoseExtractContext);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_PoseByName::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s' Pose: %s)"), CurrentPoseAsset.IsValid()? *CurrentPoseAsset.Get()->GetName() : TEXT("None"), *PoseName.ToString());
	DebugData.AddDebugItem(DebugLine, true);
}

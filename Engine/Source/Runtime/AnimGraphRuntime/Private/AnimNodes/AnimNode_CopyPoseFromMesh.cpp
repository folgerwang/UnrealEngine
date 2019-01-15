// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_CopyPoseFromMesh

FAnimNode_CopyPoseFromMesh::FAnimNode_CopyPoseFromMesh()
	: SourceMeshComponent(nullptr)
	, bUseAttachedParent (false)
	, bCopyCurves (false)
{
}

void FAnimNode_CopyPoseFromMesh::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	// Initial update of the node, so we dont have a frame-delay on setup
	FAnimationUpdateContext UpdateContext(Context.AnimInstanceProxy, 0.0f);
	GetEvaluateGraphExposedInputs().Execute(UpdateContext);

	RefreshMeshComponent(Context.AnimInstanceProxy->GetSkelMeshComponent());
}

void FAnimNode_CopyPoseFromMesh::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{

}

void FAnimNode_CopyPoseFromMesh::RefreshMeshComponent(USkeletalMeshComponent* TargetMeshComponent)
{
	auto ResetMeshComponent = [this](USkeletalMeshComponent* InMeshComponent, USkeletalMeshComponent* InTargetMeshComponent)
	{
		USkeletalMeshComponent* CurrentMeshComponent = CurrentlyUsedSourceMeshComponent.Get();
		// if current mesh exists, but not same as input mesh
		if (CurrentMeshComponent)
		{
			// if component has been changed, reinitialize
			if (CurrentMeshComponent != InMeshComponent)
			{
				ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
			}
			// if component is still same but mesh has been changed, we have to reinitialize
			else if (CurrentMeshComponent->SkeletalMesh != CurrentlyUsedSourceMesh.Get())
			{
				ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
			}
		}
		// if not valid, but input mesh is
		else if (!CurrentMeshComponent && InMeshComponent)
		{
			ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
		}
	};

	if(SourceMeshComponent.IsValid())
	{
		ResetMeshComponent(SourceMeshComponent.Get(), TargetMeshComponent);
	}
	else if (bUseAttachedParent)
	{
		if (TargetMeshComponent)
		{
			USkeletalMeshComponent* ParentComponent = Cast<USkeletalMeshComponent>(TargetMeshComponent->GetAttachParent());
			if (ParentComponent)
			{
				ResetMeshComponent(ParentComponent, TargetMeshComponent);
			}
			else
			{
				CurrentlyUsedSourceMeshComponent.Reset();
			}
		}
		else
		{
			CurrentlyUsedSourceMeshComponent.Reset();
		}
	}
	else
	{
		CurrentlyUsedSourceMeshComponent.Reset();
	}
}

void FAnimNode_CopyPoseFromMesh::PreUpdate(const UAnimInstance* InAnimInstance)
{
	QUICK_SCOPE_CYCLE_COUNTER(FAnimNode_CopyPoseFromMesh_PreUpdate);

	RefreshMeshComponent(InAnimInstance->GetSkelMeshComponent());

	USkeletalMeshComponent* CurrentMeshComponent = CurrentlyUsedSourceMeshComponent.IsValid() ? CurrentlyUsedSourceMeshComponent.Get() : nullptr;

	if (CurrentMeshComponent && CurrentMeshComponent->SkeletalMesh && CurrentMeshComponent->IsRegistered())
	{
		// If our source is running under master-pose, then get bone data from there
		if(USkeletalMeshComponent* MasterPoseComponent = Cast<USkeletalMeshComponent>(CurrentMeshComponent->MasterPoseComponent.Get()))
		{
			CurrentMeshComponent = MasterPoseComponent;
		}

		// re-check mesh component validity as it may have changed to master
		if(CurrentMeshComponent->SkeletalMesh && CurrentMeshComponent->IsRegistered())
		{
			const bool bUROInSync = CurrentMeshComponent->ShouldUseUpdateRateOptimizations() && CurrentMeshComponent->AnimUpdateRateParams != nullptr && CurrentMeshComponent->AnimUpdateRateParams == InAnimInstance->GetSkelMeshComponent()->AnimUpdateRateParams;
			const bool bUsingExternalInterpolation = CurrentMeshComponent->IsUsingExternalInterpolation();
			const TArray<FTransform>& CachedComponentSpaceTransforms = CurrentMeshComponent->GetCachedComponentSpaceTransforms();
			const bool bArraySizesMatch = CachedComponentSpaceTransforms.Num() == CurrentMeshComponent->GetComponentSpaceTransforms().Num();

			// Copy source array from the appropriate location
			SourceMeshTransformArray.Reset();
			SourceMeshTransformArray.Append((bUROInSync || bUsingExternalInterpolation) && bArraySizesMatch ? CachedComponentSpaceTransforms : CurrentMeshComponent->GetComponentSpaceTransforms());

			// Ref skeleton is need for parent index lookups later, so store it now
			RefSkeleton = &CurrentMeshComponent->SkeletalMesh->RefSkeleton;

			if(bCopyCurves)
			{
				UAnimInstance* SourceAnimInstance = CurrentMeshComponent->GetAnimInstance();
				if (SourceAnimInstance)
				{
					// attribute curve contains all list
					SourceCurveList.Reset();
					SourceCurveList.Append(SourceAnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve));
				}
				else
				{
					SourceCurveList.Reset();
				}
			}
		}
	}
}

void FAnimNode_CopyPoseFromMesh::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// This introduces a frame of latency in setting the pin-driven source component,
	// but we cannot do the work to extract transforms on a worker thread as it is not thread safe.
	GetEvaluateGraphExposedInputs().Execute(Context);
}

void FAnimNode_CopyPoseFromMesh::Evaluate_AnyThread(FPoseContext& Output)
{
	FCompactPose& OutPose = Output.Pose;
	OutPose.ResetToRefPose();

	if(SourceMeshTransformArray.Num() > 0 && RefSkeleton != nullptr)
	{
		const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();

		for(FCompactPoseBoneIndex PoseBoneIndex : OutPose.ForEachBoneIndex())
		{
			const int32 SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(PoseBoneIndex);
			const int32 MeshBoneIndex = RequiredBones.GetSkeletonToPoseBoneIndexArray()[SkeletonBoneIndex];
			const int32* Value = BoneMapToSource.Find(MeshBoneIndex);
			if(Value && SourceMeshTransformArray.IsValidIndex(*Value))
			{
				const int32 SourceBoneIndex = *Value;
				const int32 ParentIndex = RefSkeleton->GetParentIndex(SourceBoneIndex);
				const FCompactPoseBoneIndex MyParentIndex = RequiredBones.GetParentBoneIndex(PoseBoneIndex);
				// only apply if I also have parent, otherwise, it should apply the space bases
				if (SourceMeshTransformArray.IsValidIndex(ParentIndex) && MyParentIndex != INDEX_NONE)
				{
					const FTransform& ParentTransform = SourceMeshTransformArray[ParentIndex];
					const FTransform& ChildTransform = SourceMeshTransformArray[SourceBoneIndex];
					OutPose[PoseBoneIndex] = ChildTransform.GetRelativeTransform(ParentTransform);
				}
				else
				{
					OutPose[PoseBoneIndex] = SourceMeshTransformArray[SourceBoneIndex];
				}
			}
		}
	}

	if (bCopyCurves)
	{
		for (auto Iter = SourceCurveList.CreateConstIterator(); Iter; ++Iter)
		{
			const SmartName::UID_Type* UID = CurveNameToUIDMap.Find(Iter.Key());
			if (UID)
			{
				// set source value to output curve
				Output.Curve.Set(*UID, Iter.Value());
			}
		}
	}
}

void FAnimNode_CopyPoseFromMesh::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += FString::Printf(TEXT("('%s')"), *GetNameSafe(CurrentlyUsedSourceMeshComponent.IsValid() ? CurrentlyUsedSourceMeshComponent.Get()->SkeletalMesh : nullptr));
	DebugData.AddDebugItem(DebugLine, true);
}

void FAnimNode_CopyPoseFromMesh::ReinitializeMeshComponent(USkeletalMeshComponent* NewSourceMeshComponent, USkeletalMeshComponent* TargetMeshComponent)
{
	CurrentlyUsedSourceMeshComponent = NewSourceMeshComponent;
	// reset source mesh
	CurrentlyUsedSourceMesh.Reset();
	BoneMapToSource.Reset();
	CurveNameToUIDMap.Reset();

	if (TargetMeshComponent && NewSourceMeshComponent && NewSourceMeshComponent->SkeletalMesh && !NewSourceMeshComponent->IsPendingKill())
	{
		USkeletalMesh* SourceSkelMesh = NewSourceMeshComponent->SkeletalMesh;
		USkeletalMesh* TargetSkelMesh = TargetMeshComponent->SkeletalMesh;
		CurrentlyUsedSourceMesh = SourceSkelMesh;

		if (SourceSkelMesh == TargetSkelMesh)
		{
			for(int32 ComponentSpaceBoneId = 0; ComponentSpaceBoneId < SourceSkelMesh->RefSkeleton.GetNum(); ++ComponentSpaceBoneId)
			{
				BoneMapToSource.Add(ComponentSpaceBoneId, ComponentSpaceBoneId);
			}
		}
		else
		{
			for (int32 ComponentSpaceBoneId=0; ComponentSpaceBoneId<TargetSkelMesh->RefSkeleton.GetNum(); ++ComponentSpaceBoneId)
			{
				FName BoneName = TargetSkelMesh->RefSkeleton.GetBoneName(ComponentSpaceBoneId);
				BoneMapToSource.Add(ComponentSpaceBoneId, SourceSkelMesh->RefSkeleton.FindBoneIndex(BoneName));
			}
		}

		if (bCopyCurves)
		{
			USkeleton* SourceSkeleton = SourceSkelMesh->Skeleton;
			USkeleton* TargetSkeleton = TargetSkelMesh->Skeleton;

			// you shouldn't be here if this happened
			check(SourceSkeleton && TargetSkeleton);

			const FSmartNameMapping* SourceContainer = SourceSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
			const FSmartNameMapping* TargetContainer = TargetSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

			TArray<FName> SourceCurveNames;
			SourceContainer->FillNameArray(SourceCurveNames);
			for (int32 Index = 0; Index < SourceCurveNames.Num(); ++Index)
			{
				SmartName::UID_Type UID = TargetContainer->FindUID(SourceCurveNames[Index]);
				if (UID != SmartName::MaxUID)
				{
					// has a valid UID, add to the list
					SmartName::UID_Type& Value = CurveNameToUIDMap.Add(SourceCurveNames[Index]);
					Value = UID;
				}
			}
		}
	}
}

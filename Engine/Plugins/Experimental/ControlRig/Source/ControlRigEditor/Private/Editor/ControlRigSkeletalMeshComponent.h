// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/DebugSkelMeshComponent.h"
#include "ControlRigSkeletalMeshComponent.generated.h"

UCLASS(MinimalAPI)
class UControlRigSkeletalMeshComponent : public UDebugSkelMeshComponent
{
	GENERATED_UCLASS_BODY()

	// USkeletalMeshComponent interface
	virtual void InitAnim(bool bForceReinit) override;

	// BEGIN UDebugSkeletalMeshComponent interface
	virtual void ShowReferencePose(bool bRefPose) override;
	virtual bool IsReferencePoseShown() const override;
	virtual void SetCustomDefaultPose() override;
	virtual const FReferenceSkeleton& GetReferenceSkeleton() const override
	{
		return DebugDrawSkeleton;
	}

	virtual const TArray<FBoneIndexType>& GetDrawBoneIndices() const override
	{
		return DebugDrawBones;
	}

	virtual FTransform GetDrawTransform(int32 BoneIndex) const override;

	virtual int32 GetNumDrawTransform() const
	{
		return DebugDrawBones.Num();
	}
	// END UDebugSkeletalMeshComponent interface

public:
	/*
	 *	Rebuild debug draw skeleton 
	 */
	void RebuildDebugDrawSkeleton();
private:
	FReferenceSkeleton DebugDrawSkeleton;
	TArray<FBoneIndexType> DebugDrawBones;
};
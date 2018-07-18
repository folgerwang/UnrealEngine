// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimSequencerInstance.h"
#include "ControlRigSequencerAnimInstance.generated.h"

class UControlRig;
struct FInputBlendPose;

UCLASS(Transient, NotBlueprintable)
class CONTROLRIG_API UControlRigSequencerAnimInstance : public UAnimSequencerInstance
{
	GENERATED_UCLASS_BODY()

public:
	/** Update an animation ControlRig in this sequence */
	bool UpdateControlRig(UControlRig* InControlRig, uint32 SequenceId, bool bAdditive, bool bApplyBoneFilter, const FInputBlendPose& BoneFilter, float Weight);

	/** This is cached control rig that is used to draw the joint with. Do not exapect this would be reliable data to exist */
	TWeakObjectPtr<UControlRig> CachedControlRig;
protected:
	// UAnimInstance interface
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_ControlRigBase.h"
#include "AnimNode_ControlRig.generated.h"

class UControlRig;
class UNodeMappingContainer;

/**
 * Animation node that allows animation ControlRig output to be used in an animation graph
 */
USTRUCT()
struct CONTROLRIG_API FAnimNode_ControlRig : public FAnimNode_ControlRigBase
{
	GENERATED_BODY()

	FAnimNode_ControlRig();

	UControlRig* GetControlRig() const { return ControlRig; }

	// FAnimNode_Base interface
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext & Output) override;

private:

	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

	/** Cached ControlRig */
	UPROPERTY(EditAnywhere, Category = ControlRig)
	TSubclassOf<UControlRig> ControlRigClass;

	UPROPERTY(transient)
	UControlRig* ControlRig;

public:
	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FAnimNode_ControlRig> : public TStructOpsTypeTraitsBase2<FAnimNode_ControlRig>
{
	enum
	{
		WithPostSerialize = true,
	};
};
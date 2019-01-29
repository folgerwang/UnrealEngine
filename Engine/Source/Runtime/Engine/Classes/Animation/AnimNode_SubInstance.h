// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_SubInstance.generated.h"

struct FAnimInstanceProxy;

USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_SubInstance : public FAnimNode_Base
{
	GENERATED_BODY()

public:

	FAnimNode_SubInstance();

	/** 
	 *  Input pose for the node, intentionally not accessible because if there's no input
	 *  Node in the target class we don't want to show this as a pin
	 */
	UPROPERTY()
	FPoseLink InPose;

	/** The class spawned for this sub-instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TSubclassOf<UAnimInstance> InstanceClass;

	/** Optional tag used to identify this sub-instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName Tag;

	/** List of source properties to use, 1-1 with Dest names below, built by the compiler */
	UPROPERTY()
	TArray<FName> SourcePropertyNames;

	/** List of destination properties to use, 1-1 with Source names above, built by the compiler */
	UPROPERTY()
	TArray<FName> DestPropertyNames;

	/** This is the actual instance allocated at runtime that will run */
	UPROPERTY(Transient)
	UAnimInstance* InstanceToRun;

	/** List of properties on the calling instance to push from */
	UPROPERTY(Transient)
	TArray<UProperty*> InstanceProperties;

	/** List of properties on the sub instance to push to, built from name list when initialised */
	UPROPERTY(Transient)
	TArray<UProperty*> SubInstanceProperties;

	// Temporary storage for the output of the subinstance, will be copied into output pose.
	FBlendedHeapCurve BlendedCurve;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

protected:
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

	// Shutdown the currently running instance
	void TeardownInstance();
};

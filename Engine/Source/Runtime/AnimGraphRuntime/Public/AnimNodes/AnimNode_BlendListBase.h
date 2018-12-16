// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimNodeBase.h"
#include "AlphaBlend.h"
#include "AnimNode_BlendListBase.generated.h"

class UBlendProfile;
class UCurveFloat;

// Blend list node; has many children
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendListBase : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category=Links)
	TArray<FPoseLink> BlendPose;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category=Config, meta=(PinShownByDefault))
	TArray<float> BlendTime;

	UPROPERTY(EditAnywhere, Category=BlendType)
	EAlphaBlendOption BlendType;

protected:
	/** This reinitializes child pose when re-activated. For example, when active child changes */
	UPROPERTY(EditAnywhere, Category = Option)
	bool bResetChildOnActivation;

	int32 LastActiveChildIndex;

public:
	UPROPERTY(EditAnywhere, Category=BlendType)
	UCurveFloat* CustomBlendCurve;

	UPROPERTY(EditAnywhere, Category=BlendType)
	UBlendProfile* BlendProfile;

	TArray<struct FAlphaBlend> Blends;

protected:
	TArray<float> BlendWeights;

	TArray<float> RemainingBlendTimes;

	TArray<FBlendSampleData> PerBoneSampleData;

	//Store which poses we need to evaluate
	TArray<int32> PosesToEvaluate;

public:	
	FAnimNode_BlendListBase()
		: BlendType(EAlphaBlendOption::Linear)
		, bResetChildOnActivation(false)
		, LastActiveChildIndex(0)
		, CustomBlendCurve(nullptr)
		, BlendProfile(nullptr)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

#if WITH_EDITOR
	virtual void AddPose()
	{
		BlendTime.Add(0.1f);
		new (BlendPose) FPoseLink();
	}

	virtual void RemovePose(int32 PoseIndex)
	{
		BlendTime.RemoveAt(PoseIndex);
		BlendPose.RemoveAt(PoseIndex);
	}
#endif

protected:
	virtual int32 GetActiveChildIndex() { return 0; }
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};

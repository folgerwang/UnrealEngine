// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/InputScaleBias.h"
#include "AnimNode_TwoWayBlend.generated.h"

// This represents a baked transition
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EAnimAlphaInputType AlphaInputType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	mutable float Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FInputScaleBias AlphaScaleBias;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault, DisplayName = "bEnabled"))
	mutable bool bAlphaBoolEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DisplayName = "Blend Settings"))
	FInputAlphaBoolBlend AlphaBoolBlend;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	mutable FName AlphaCurveName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FInputScaleBiasClamp AlphaScaleBiasClamp;

protected:
	UPROPERTY(Transient)
	mutable float InternalBlendAlpha;

	UPROPERTY(Transient)
	mutable bool bAIsRelevant;

	UPROPERTY(Transient)
	mutable bool bBIsRelevant;

	/** This reinitializes child pose when re-activated. For example, when active child changes */
	UPROPERTY(EditAnywhere, Category = Option)
	bool bResetChildOnActivation;

public:
	FAnimNode_TwoWayBlend()
		: AlphaInputType(EAnimAlphaInputType::Float)
		, Alpha(0.0f)
		, bAlphaBoolEnabled(true)
		, AlphaCurveName(NAME_None)
		, InternalBlendAlpha(0.0f)
		, bAIsRelevant(false)
		, bBIsRelevant(false)
		, bResetChildOnActivation(false)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};


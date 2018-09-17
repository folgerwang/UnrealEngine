// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkInstance.h"

void FLiveLinkInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	// initialize node manually 
	FAnimationInitializeContext InitContext(this);
	PoseNode.Initialize_AnyThread(InitContext);
}

bool FLiveLinkInstanceProxy::Evaluate(FPoseContext& Output)
{
	PoseNode.Evaluate_AnyThread(Output);

	return true;
}

void FLiveLinkInstanceProxy::UpdateAnimationNode(float DeltaSeconds)
{
	UpdateCounter.Increment();

	FAnimationUpdateContext UpdateContext(this, DeltaSeconds);
	PoseNode.Update_AnyThread(UpdateContext);
	
	if(ULiveLinkInstance* Instance = Cast<ULiveLinkInstance>(GetAnimInstanceObject()))
	{
		Instance->CurrentRetargetAsset = PoseNode.CurrentRetargetAsset; //Cache for GC
	}
}

ULiveLinkInstance::ULiveLinkInstance(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, CurrentRetargetAsset(nullptr)
{

}

FAnimInstanceProxy* ULiveLinkInstance::CreateAnimInstanceProxy()
{
	return new FLiveLinkInstanceProxy(this);
}

void ULiveLinkInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	Super::DestroyAnimInstanceProxy(InProxy);
	CurrentRetargetAsset = nullptr;
}
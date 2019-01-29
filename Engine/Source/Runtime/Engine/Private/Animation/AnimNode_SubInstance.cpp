// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_SubInstance.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SubInput.h"

FAnimNode_SubInstance::FAnimNode_SubInstance()
	: InstanceClass(nullptr)
	, Tag(NAME_None)
	, InstanceToRun(nullptr)
{

}

void FAnimNode_SubInstance::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	InPose.Initialize(Context);
}

void FAnimNode_SubInstance::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	InPose.CacheBones(Context);
}

void FAnimNode_SubInstance::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	InPose.Update(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	if(InstanceToRun)
	{
		FAnimInstanceProxy& Proxy = InstanceToRun->GetProxyOnAnyThread<FAnimInstanceProxy>();

		// First copy properties
		check(InstanceProperties.Num() == SubInstanceProperties.Num());
		for(int32 PropIdx = 0; PropIdx < InstanceProperties.Num(); ++PropIdx)
		{
			UProperty* CallerProperty = InstanceProperties[PropIdx];
			UProperty* SubProperty = SubInstanceProperties[PropIdx];

			check(CallerProperty && SubProperty);

#if WITH_EDITOR
			if (ensure(CallerProperty->SameType(SubProperty)))
#endif
			{
				uint8* SrcPtr = CallerProperty->ContainerPtrToValuePtr<uint8>(Context.AnimInstanceProxy->GetAnimInstanceObject());
				uint8* DestPtr = SubProperty->ContainerPtrToValuePtr<uint8>(InstanceToRun);

				CallerProperty->CopyCompleteValue(DestPtr, SrcPtr);
			}
		}

		// Only update if we've not had a single-threaded update already
		if(InstanceToRun->bNeedsUpdate)
		{
			Proxy.UpdateAnimation();
		}
	}
}

void FAnimNode_SubInstance::Evaluate_AnyThread(FPoseContext& Output)
{
	if(InstanceToRun)
	{
		InPose.Evaluate(Output);

		FAnimInstanceProxy& Proxy = InstanceToRun->GetProxyOnAnyThread<FAnimInstanceProxy>();
		FAnimNode_SubInput* InputNode = Proxy.SubInstanceInputNode;

		if(InputNode)
		{
			InputNode->InputPose.CopyBonesFrom(Output.Pose);
			InputNode->InputCurve.CopyFrom(Output.Curve);
		}

		InstanceToRun->ParallelEvaluateAnimation(false, nullptr, BlendedCurve, Output.Pose);

		Output.Curve.CopyFrom(BlendedCurve);
	}
	else
	{
		Output.ResetToRefPose();
	}

}

void FAnimNode_SubInstance::GatherDebugData(FNodeDebugData& DebugData)
{
	// Add our entry
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("Target: %s"), (*InstanceClass) ? *InstanceClass->GetName() : TEXT("None"));

	DebugData.AddDebugItem(DebugLine);

	// Gather data from the sub instance
	if(InstanceToRun)
	{
		FAnimInstanceProxy& Proxy = InstanceToRun->GetProxyOnAnyThread<FAnimInstanceProxy>();
		Proxy.GatherDebugData(DebugData.BranchFlow(1.0f));
	}

	// Pass to next
	InPose.GatherDebugData(DebugData.BranchFlow(1.0f));
}

void FAnimNode_SubInstance::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	if(*InstanceClass)
	{
		USkeletalMeshComponent* MeshComp = InAnimInstance->GetSkelMeshComponent();
		check(MeshComp);

		// Full reinit, kill old instances
		if(InstanceToRun)
		{
			MeshComp->SubInstances.Remove(InstanceToRun);
			InstanceToRun->MarkPendingKill();
			InstanceToRun = nullptr;
		}

		// Need an instance to run, so create it now
		// We use the tag to name the object, but as we verify there are no duplicates in the compiler we
		// dont need to verify it is unique here.
		InstanceToRun = NewObject<UAnimInstance>(MeshComp, InstanceClass, Tag);

		// Initialize the new instance
		InstanceToRun->InitializeAnimation();

		MeshComp->SubInstances.Add(InstanceToRun);

		// Build property lists
		InstanceProperties.Reset(SourcePropertyNames.Num());
		SubInstanceProperties.Reset(SourcePropertyNames.Num());

		check(SourcePropertyNames.Num() == DestPropertyNames.Num());

		for(int32 Idx = 0; Idx < SourcePropertyNames.Num(); ++Idx)
		{
			FName& SourceName = SourcePropertyNames[Idx];
			FName& DestName = DestPropertyNames[Idx];

			UClass* SourceClass = InAnimInstance->GetClass();

			UProperty* SourceProperty = FindField<UProperty>(SourceClass, SourceName);
			UProperty* DestProperty = FindField<UProperty>(*InstanceClass, DestName);

			if (SourceProperty && DestProperty
#if WITH_EDITOR
				// This type check can fail when anim blueprints are in an error state:
				&& SourceProperty->SameType(DestProperty)
#endif
				)
			{
				InstanceProperties.Add(SourceProperty);
				SubInstanceProperties.Add(DestProperty);
			}
		}
		
	}
	else if(InstanceToRun)
	{
		// We have an instance but no instance class
		TeardownInstance();
	}
}

void FAnimNode_SubInstance::TeardownInstance()
{
	InstanceToRun->UninitializeAnimation();
	InstanceToRun = nullptr;
}


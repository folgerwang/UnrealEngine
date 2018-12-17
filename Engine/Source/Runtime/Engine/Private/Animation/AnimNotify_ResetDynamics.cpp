// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify_ResetDynamics.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_ResetDynamics::UAnimNotify_ResetDynamics()
	: Super()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(90, 220, 255, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_ResetDynamics::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	MeshComp->ResetAnimInstanceDynamics();
}

FString UAnimNotify_ResetDynamics::GetNotifyName_Implementation() const
{
	return TEXT("Reset Dynamics");
}
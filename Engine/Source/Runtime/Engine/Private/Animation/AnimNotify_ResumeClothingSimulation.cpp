// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify_ResumeClothingSimulation.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_ResumeClothingSimulation::UAnimNotify_ResumeClothingSimulation()
	: Super()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(90, 220, 255, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_ResumeClothingSimulation::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	MeshComp->ResumeClothingSimulation();
}

FString UAnimNotify_ResumeClothingSimulation::GetNotifyName_Implementation() const
{
	return TEXT("Resume Clothing Sim");
}
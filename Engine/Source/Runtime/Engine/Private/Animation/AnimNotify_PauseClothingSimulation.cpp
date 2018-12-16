// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify_PauseClothingSimulation.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_PauseClothingSimulation::UAnimNotify_PauseClothingSimulation()
	: Super()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(90, 220, 255, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_PauseClothingSimulation::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	MeshComp->SuspendClothingSimulation();
}

FString UAnimNotify_PauseClothingSimulation::GetNotifyName_Implementation() const
{
	return TEXT("Pause Clothing Sim");
}
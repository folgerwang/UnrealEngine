// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshComponentBudgeted.h"
#include "AnimationBudgetAllocator.h"
#include "Kismet/KismetSystemLibrary.h"

FOnCalculateSignificance USkeletalMeshComponentBudgeted::OnCalculateSignificanceDelegate;

USkeletalMeshComponentBudgeted::USkeletalMeshComponentBudgeted(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AnimationBudgetHandle(INDEX_NONE)
	, AnimationBudgetAllocator(nullptr)
	, bAutoRegisterWithBudgetAllocator(true)
	, bAutoCalculateSignificance(false)
{
}

void USkeletalMeshComponentBudgeted::BeginPlay()
{
	Super::BeginPlay();

	if(bAutoRegisterWithBudgetAllocator && !UKismetSystemLibrary::IsDedicatedServer(this))
	{
		IAnimationBudgetAllocator::Get(GetWorld())->RegisterComponent(this);
	}
}

void USkeletalMeshComponentBudgeted::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Dont unregister if we are in the process of being destroyed in a GC.
	// As reciprocal ptrs are null, handles are all invalid.
	if(!IsUnreachable())	
	{
		IAnimationBudgetAllocator::Get(GetWorld())->UnregisterComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void USkeletalMeshComponentBudgeted::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if(AnimationBudgetAllocator)
	{
		uint64 StartTime = FPlatformTime::Cycles64();

		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		AnimationBudgetAllocator->SetGameThreadLastTickTimeMs(AnimationBudgetHandle, FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime));
	}
	else
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}
}

void USkeletalMeshComponentBudgeted::CompleteParallelAnimationEvaluation(bool bDoPostAnimEvaluation)
{
	if(AnimationBudgetAllocator)
	{
		uint64 StartTime = FPlatformTime::Cycles64();

		Super::CompleteParallelAnimationEvaluation(bDoPostAnimEvaluation);

		AnimationBudgetAllocator->SetGameThreadLastCompletionTimeMs(AnimationBudgetHandle, FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime));
	}
	else
	{
		Super::CompleteParallelAnimationEvaluation(bDoPostAnimEvaluation);
	}
}
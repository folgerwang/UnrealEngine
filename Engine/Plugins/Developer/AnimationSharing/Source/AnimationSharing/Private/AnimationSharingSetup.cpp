// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimationSharingSetup.h"

UAnimationSharingSetup::UAnimationSharingSetup(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

#if WITH_EDITOR
void UAnimationSharingSetup::PostLoad()
{
	Super::PostLoad();

	/** Ensure all data required for the UI is loaded */
	for (const FPerSkeletonAnimationSharingSetup& SharingSetup : SkeletonSetups)
	{ 
		SharingSetup.Skeleton.LoadSynchronous();
		SharingSetup.SkeletalMesh.LoadSynchronous();
		
		for (const FAnimationStateEntry& Entry : SharingSetup.AnimationStates)
		{
			for (const FAnimationSetup& AnimSetup : Entry.AnimationSetups)
			{
				AnimSetup.AnimSequence.LoadSynchronous();				
			}
		}
	}
}
#endif // WITH_EDITOR

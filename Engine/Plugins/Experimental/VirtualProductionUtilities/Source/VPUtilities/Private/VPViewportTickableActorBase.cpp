// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPViewportTickableActorBase.h"


AVPViewportTickableActorBase::AVPViewportTickableActorBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorTickEnabled(true);
}


 bool AVPViewportTickableActorBase::ShouldTickIfViewportsOnly() const
{
	 return true;
}
 

 void AVPViewportTickableActorBase::Tick(float DeltaSeconds)
 {
	 Super::Tick(DeltaSeconds);

	 FEditorScriptExecutionGuard ScriptGuard;
	 EditorTick(DeltaSeconds);
 }


 void AVPViewportTickableActorBase::EditorTick_Implementation(float DeltaSeconds)
 {

 }
 
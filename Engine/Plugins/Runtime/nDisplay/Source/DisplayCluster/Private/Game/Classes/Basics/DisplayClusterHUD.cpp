// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterHUD.h"


ADisplayClusterHUD::ADisplayClusterHUD(const FObjectInitializer& ObjectInitializer) :
	AHUD(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}


void ADisplayClusterHUD::BeginPlay()
{
	Super::BeginPlay();
}

void ADisplayClusterHUD::DrawHUD()
{
	Super::DrawHUD();
}

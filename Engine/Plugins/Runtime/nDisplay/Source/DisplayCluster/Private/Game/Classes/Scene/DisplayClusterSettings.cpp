// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSettings.h"


ADisplayClusterSettings::ADisplayClusterSettings(const FObjectInitializer& ObjectInitializer) :
	AActor(ObjectInitializer),
	MovementMaxSpeed(1200.f),
	MovementAcceleration(4000.f),
	MovementDeceleration(8000.f),
	MovementTurningBoost(8.f),
	RotationSpeed(45.f)
{
	PrimaryActorTick.bCanEverTick = true;

}

ADisplayClusterSettings::~ADisplayClusterSettings()
{
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavCollisionBase.h"


UNavCollisionBase::FConstructNew UNavCollisionBase::ConstructNewInstanceDelegate;
UNavCollisionBase::FDelegateInitializer UNavCollisionBase::DelegateInitializer;

UNavCollisionBase::FDelegateInitializer::FDelegateInitializer()
{
	UNavCollisionBase::ConstructNewInstanceDelegate.BindLambda([](UObject&) { return nullptr; });
}
	
UNavCollisionBase::UNavCollisionBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsDynamicObstacle = false;
	bHasConvexGeometry = false;
}

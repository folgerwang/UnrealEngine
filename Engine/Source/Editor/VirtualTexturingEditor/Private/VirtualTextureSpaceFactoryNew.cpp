// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSpaceFactoryNew.h"
#include "VT/VirtualTextureSpace.h"

UVirtualTextureSpaceFactoryNew::UVirtualTextureSpaceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UVirtualTextureSpace::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVirtualTextureSpaceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UVirtualTextureSpace>(InParent, InClass, InName, Flags);
}


bool UVirtualTextureSpaceFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
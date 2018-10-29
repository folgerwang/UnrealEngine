// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureFactoryNew.h"
#include "VT/VirtualTexture.h"

UVirtualTextureFactoryNew::UVirtualTextureFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UVirtualTexture::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVirtualTextureFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UVirtualTexture>(InParent, InClass, InName, Flags);
}


bool UVirtualTextureFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
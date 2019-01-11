// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOBlueprintLibrary.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "OpenColorIORendering.h"


UOpenColorIOBlueprintLibrary::UOpenColorIOBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


bool UOpenColorIOBlueprintLibrary::ApplyColorSpaceTransform(const UObject* WorldContextObject, const FOpenColorIOColorConversionSettings& ConversionSettings, UTexture* InputTexture, UTextureRenderTarget2D* OutputRenderTarget)
{
	return FOpenColorIORendering::ApplyColorTransform(WorldContextObject->GetWorld(), ConversionSettings, InputTexture, OutputRenderTarget);
}

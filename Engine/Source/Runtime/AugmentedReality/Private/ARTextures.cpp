// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ARTextures.h"

UARTexture::UARTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UARTextureCameraImage::UARTextureCameraImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UARTextureCameraDepth::UARTextureCameraDepth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAREnvironmentCaptureProbeTexture::UAREnvironmentCaptureProbeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TextureType(EARTextureType::EnvironmentCapture)
{
}

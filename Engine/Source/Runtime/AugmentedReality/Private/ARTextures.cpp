// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

//@joeg -- Added environmental texture probe support
UAREnvironmentCaptureProbeTexture::UAREnvironmentCaptureProbeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TextureType(EARTextureType::EnvironmentCapture)
{
}

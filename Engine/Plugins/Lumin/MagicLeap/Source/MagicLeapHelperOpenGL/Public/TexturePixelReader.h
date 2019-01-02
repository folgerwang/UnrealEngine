// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"

/**
 * Read the texture, perform any format conversions, and return the pixel data in a byte array.
 */
class MAGICLEAPHELPEROPENGL_API FTexturePixelReader
{
public:
	FTexturePixelReader();
	virtual ~FTexturePixelReader();

	bool RenderTextureToRenderBuffer(const UTexture2D& SrcTexture, uint8* PixelData);

private:
	void Init();
	void UpdateVertexData();
	void Release();

	class FTexturePixelReaderImpl* Impl;
};

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VirtualTexturing.h"
#include "VirtualTextureSpace.h"
#include "TexturePagePool.h"
#include "RenderTargetPool.h"

IVirtualTextureSpace* IVirtualTextureSpace::Create(const FVirtualTextureSpaceDesc& desc)
{
	return new FVirtualTextureSpace(desc);
}

void IVirtualTextureSpace::Delete(IVirtualTextureSpace*& Space)
{
	delete Space;
}

FMatrix FVirtualTextureUniformData::Invalid = FMatrix::Identity;

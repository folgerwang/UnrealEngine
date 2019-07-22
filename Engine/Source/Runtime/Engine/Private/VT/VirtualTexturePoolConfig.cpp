// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTexturePoolConfig.h"

UVirtualTexturePoolConfig::UVirtualTexturePoolConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UVirtualTexturePoolConfig::~UVirtualTexturePoolConfig()
{
}

void UVirtualTexturePoolConfig::PostLoad()
{
	DefaultConfig.SizeInMegabyte = DefaultSizeInMegabyte;
}

const FVirtualTextureSpacePoolConfig *UVirtualTexturePoolConfig::FindPoolConfig(int32 TileSize, EPixelFormat Format)
{
	// Reverse iterate so that project config can override base config
	for (int32 Id = Pools.Num() - 1; Id >= 0 ; Id--)
	{
		const FVirtualTextureSpacePoolConfig& Config = Pools[Id];
		if (Config.TileSize == TileSize && Config.Format == Format)
		{
			return &Config;
		}
	}

	DefaultConfig.SizeInMegabyte = DefaultSizeInMegabyte;
	return &DefaultConfig;
}

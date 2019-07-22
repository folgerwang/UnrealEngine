// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RenderResource.h"
#include "RenderCommandFence.h"
#include "Engine/Texture.h"
#include "VirtualTexturing.h"
#include "VirtualTexturePoolConfig.generated.h"

/**
* Settings of a single pool
*/
USTRUCT()
struct FVirtualTextureSpacePoolConfig
{
	GENERATED_USTRUCT_BODY()

	FVirtualTextureSpacePoolConfig() : SizeInMegabyte(0), TileSize(0), Format(PF_Unknown) {}

	UPROPERTY()
	int32 SizeInMegabyte; // Size counted in tiles of the pool

	UPROPERTY()
	int32 TileSize; // This is *including* any filtering borders

	UPROPERTY()
	TEnumAsByte<EPixelFormat> Format;

	bool IsDefault() const { return TileSize == 0; }
};

UCLASS(config = Engine, transient)
class ENGINE_API UVirtualTexturePoolConfig : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(config)
	int32 DefaultSizeInMegabyte; // Size in tiles of any pools not explicitly specified in the config

	UPROPERTY(config)
	TArray<FVirtualTextureSpacePoolConfig> Pools; // All the VT pools specified in the config

	const FVirtualTextureSpacePoolConfig *FindPoolConfig(int32 TileSize, EPixelFormat Format);

	virtual ~UVirtualTexturePoolConfig();

	virtual void PostLoad() override;

private:
	FVirtualTextureSpacePoolConfig DefaultConfig;
};

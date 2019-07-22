// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RenderResource.h"
#include "RenderCommandFence.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "VirtualTexturing.h"
#include "VirtualTexture.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVirtualTexturingModule, Log, All);

UCLASS(ClassGroup = Rendering)
class ENGINE_API UVirtualTexture : public UObject
{
	GENERATED_UCLASS_BODY()
	virtual void Serialize(FArchive& Ar) override;
};

UCLASS(ClassGroup = Rendering)
class ENGINE_API ULightMapVirtualTexture : public UVirtualTexture
{
	GENERATED_UCLASS_BODY()
};

enum class ELightMapVirtualTextureType
{
	HqLayer0,
	HqLayer1,
	ShadowMask,
	SkyOcclusion,
	AOMaterialMask,

	Count,
};

UCLASS(ClassGroup = Rendering)
class ENGINE_API ULightMapVirtualTexture2D : public UTexture2D
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = VirtualTexture)
	TArray<int8> TypeToLayer;

	void SetLayerForType(ELightMapVirtualTextureType InType, uint8 InLayer);
	uint32 GetLayerForType(ELightMapVirtualTextureType InType) const;

	inline bool HasLayerForType(ELightMapVirtualTextureType InType) const { return GetLayerForType(InType) != ~0u; }
};

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PixelFormat.h"
#include "VirtualTexturing.h"
#include "RenderCommandFence.h"
#include "Engine/Texture.h"
#include "VirtualTextureSpace.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVirtualTexturingModule, Log, All);

class ITargetPlatform;

USTRUCT()
struct FVirtualTextureLayer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Data)
	TEnumAsByte<ETextureSourceFormat> Format = TSF_BGRA8;

	UPROPERTY(EditAnywhere, Category = Compression)
	bool bCompressed = true; // Use (lossy) texture compression on this layer

	UPROPERTY(EditAnywhere, Category = Data)
	bool bHasAlpha = true; // The data on this layer has an alpha channel

	UPROPERTY(EditAnywhere, Category = Compression)
	TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_Default;
};

UENUM()
enum PageTableFormat
{
	PTF_16,	//16 bpp 
	PTF_32	//32 bpp
};

struct FVirtualTextureSpaceCustomVersion
{
	static const FGuid Key;
	enum Type {
		Initial,
		AddedFormats,
		Latest = AddedFormats	// Always update this to be equal to the latest version
	};
};

UCLASS(ClassGroup = Rendering)	
class ENGINE_API UVirtualTextureSpace : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FVirtualTextureLayer> Layers;

	UPROPERTY(EditAnywhere, Category = Advanced)
	int32 TileSize;

	UPROPERTY(EditAnywhere, Category = Advanced)
	int32 BorderWidth;

	UPROPERTY(EditAnywhere, Category = Advanced)
	int32 Size;

	UPROPERTY(EditAnywhere, Category = Advanced)
	int32 Dimensions;

	UPROPERTY(EditAnywhere, Category = Advanced)
	TEnumAsByte<PageTableFormat> Format;

	// The VTPool is at the moment coupled at the Space 
	UPROPERTY(EditAnywhere, Category = Advanced)
	int32 PoolSize;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * Determine the texture format to use for the given layer on the given platform.
	 */
	FName GetTextureFormatName(int32 Layer,const ITargetPlatform *Platform);
	EPixelFormat GetTextureFormat(int32 Layer,const ITargetPlatform *Platform);
#endif

	EPixelFormat GetTextureFormat(int32 Layer);

	virtual void PostLoad() override;

	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;

	class IVirtualTextureSpace* GetRenderResource() const { return Resource; }
	void UpdateResource();

	virtual void Serialize(FArchive& Ar) override;

protected:
	virtual void GetSpaceDesc(struct FVirtualTextureSpaceDesc& desc);

private:
	void ReleaseResource();
	void UpdateLayerFormats();

	IVirtualTextureSpace* Resource;
	FRenderCommandFence ReleaseFence;

	// These are derived by the platform system and should not be directly set
	TArray<TEnumAsByte<EPixelFormat>> LayerFormats;
};

UCLASS(ClassGroup = Rendering)
class ENGINE_API ULightMapVirtualTextureSpace final : public UVirtualTextureSpace
{
	GENERATED_UCLASS_BODY()

protected:
	virtual void GetSpaceDesc(struct FVirtualTextureSpaceDesc& desc) override;
};
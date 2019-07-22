// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "RenderResource.h"
#include "UObject/ObjectMacros.h"
#include "VirtualTexturing.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "RuntimeVirtualTexture.generated.h"

/** Runtime virtual texture FRenderResource */
class FRuntimeVirtualTextureRenderResource : public FRenderResource
{
public:
	FRuntimeVirtualTextureRenderResource(FVTProducerDescription const& InDesc, IVirtualTexture* InVirtualTextureProducer);

	/** Getter for the virtual texture allocation */
	IAllocatedVirtualTexture* GetAllocatedVirtualTexture() const { return AllocatedVirtualTexture; }

protected:
	//~ Begin FRenderResource Interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface.

	/** Allocate in the global virtual texture system. */
	IAllocatedVirtualTexture* AcquireAllocatedVirtualTexture();
	/** Release our virtual texture allocations  */
	void ReleaseAllocatedVirtualTexture();

private:
	const FVTProducerDescription ProducerDesc;
	IVirtualTexture* Producer;
	FVirtualTextureProducerHandle ProducerHandle;
	IAllocatedVirtualTexture* AllocatedVirtualTexture;
};

/** Runtime virtual texture UObject */
UCLASS(ClassGroup = Rendering)
class ENGINE_API URuntimeVirtualTexture : public UObject
{
	GENERATED_UCLASS_BODY()
	~URuntimeVirtualTexture();

protected:
	/** Contents of virtual texture. */
	UPROPERTY(EditAnywhere, Category = Layout, meta = (DisplayName = "Virtual texture content"), AssetRegistrySearchable)
	ERuntimeVirtualTextureMaterialType MaterialType = ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular;

	/** Enable storing the virtual texture in GPU supported compression formats. Using uncompressed is only recommended for debugging and quality comparisons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Layout, meta = (DisplayName = "Enable BC texture compression"))
	bool bCompressTextures = true;

	/** Width of virtual texture. (Actual values increase in powers of 2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Size, meta = (UIMin = "0", UIMax = "8", DisplayName = "Width of the virtual texture"))
	int32 Width = 6; // 65536

	/** Height of virtual texture. (Actual values increase in powers of 2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Size, meta = (UIMin = "0", UIMax = "8", DisplayName = "Height of the virtual texture"))
	int32 Height = 6; // 65536

	/** Page tile size. (Actual values increase in powers of 2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Size, meta = (UIMin = "0", UIMax = "4", DisplayName = "Size of each virtual texture tile"))
	int32 TileSize = 2; // 256

	/** Page tile border size divided by 2 (Actual values increase in multiples of 2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Size, meta = (UIMin = "0", UIMax = "4", DisplayName = "Border padding for each virtual texture tile"))
	int32 TileBorderSize = 2; // 4

	/** Number of low mips to cut from the virtual texture. This can reduce peak virtual texture update cost but will also increase the probability of mip shimmering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Size, meta = (UIMin = "0", UIMax = "6", DisplayName = "Number of low mips to remove from the virtual texture"))
	int32 RemoveLowMips = 0;

	/** Enable usage of the virtual texture. This option is intended only for debugging and visualization of the scene without virtual textures. It isn't serialized. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Transient, Category = Debug, meta = (DisplayName = "Enable virtual texture"))
	bool bEnable = true;

public:
	/** Public getter for enabled status */
	bool GetEnabled() { return bEnable; }

	/** Get the material set that this virtual texture stores. */
	ERuntimeVirtualTextureMaterialType GetMaterialType() const { return MaterialType; }

	/** Public getter for virtual texture width */
	int32 GetWidth() const { return 1 << FMath::Clamp(Width + 10, 10, 18); }
	/** Public getter for virtual texture height */
	int32 GetHeight() const { return 1 << FMath::Clamp(Height + 10, 10, 18); }
	/** Public getter for virtual texture tile size */
	int32 GetTileSize() const { return 1 << FMath::Clamp(TileSize + 6, 6, 10); }
	/** Public getter for virtual texture tile border size */
	int32 GetTileBorderSize() const { return 2 * FMath::Clamp(TileBorderSize, 0, 4); }
	/** Public getter for virtual texture tile border size */
	int32 GetRemoveLowMips() const { return RemoveLowMips; }

	/** Returns an approximate estimated value for the memory used by the page table texture. */
	int32 GetEstimatedPageTableTextureMemoryKb() const;
	/** Returns an approximate estimated value for the memory used by the physical texture. */
	int32 GetEstimatedPhysicalTextureMemoryKb() const;

	/** Get virtual texture description based on the properties of this object. */
	void GetProducerDescription(FVTProducerDescription& OutDesc) const;

	/** (Re)Initialize this object. Call this whenever we modify the producer or transform. */
	void Initialize(IVirtualTexture* InProducer, FTransform const& BoxToWorld);

	/** Release the resources for this object This will need to be called if our producer becomes stale and we aren't doing a full reinit with a new producer. */
	void Release();

	/** Getter for the associated virtual texture allocation. */
	IAllocatedVirtualTexture* GetAllocatedVirtualTexture() const;

	/** Getter for the shader uniform parameters. */
	FVector4 GetUniformParameter(int32 Index);

#if WITH_EDITOR
	/** Delegate to broadcast property changes. Use this so that the IVirtualTexture can be updated accordingly. */
	DECLARE_DELEGATE_OneParam(FOnEditProperty, URuntimeVirtualTexture const*);
	FOnEditProperty OnEditProperty;
#endif

protected:
	/** Initialize the render resource. This kicks off render thread work. */
	void InitResource(IVirtualTexture* InProducer);
	/** Release the render resource. This kicks off render thread work. */
	void ReleaseResource();
	/** Trigger an update for materials that reference this object. */
	void NotifyMaterials();

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface.

private:
	/** Render thread resource container */
	FRuntimeVirtualTextureRenderResource* Resource;

	/** Material uniform parameters to support transform from world to UV coordinates. */
	FVector4 WorldToUVTransformParameters[3];
};

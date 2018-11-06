// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RenderResource.h"
#include "RenderCommandFence.h"
#include "VirtualTexture.generated.h"

class UVirtualTextureSpace;
struct FVirtualTextureBuiltData;
class UVirtualTexture;
struct FVirtualTextureBuilderSettings;

class FVirtualTexture final : public FRenderResource
{
public:
	FVirtualTexture(/*const*/ UVirtualTexture* InOwner)
		: Owner(InOwner), Provider(nullptr)
	{}

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	uint64 vAddress = ~0u;
private:
	/*const*/ UVirtualTexture* Owner;
	class FChunkProvider* Provider;
};

UCLASS(ClassGroup = Rendering)
class ENGINE_API UVirtualTexture : public UObject
{
	GENERATED_UCLASS_BODY()
	virtual ~UVirtualTexture();

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = SourceData)
	TArray<class UTexture *> Layers;
#endif

	UPROPERTY(EditAnywhere, Category = VirtualTexture)
	UVirtualTextureSpace *Space;

	// TEMP HACK: Set this to true from within the editor to trigger a rebuild of the VT for now.
	// eventually this'll be something smarter with a better GUI
	UPROPERTY(EditAnywhere, Category = AlphaTesting)
	bool Rebuild;

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;

	inline FVirtualTextureBuiltData* GetTextureBuildData() const { return Data; }
	
	/**
	 * Trigger a rebuild of the texture data.
	 */
	void RebuildData(bool bAllowAsync);

	bool IsAsyncBuildComplete() { return true; }
	void FinishAsyncRebuild() {}

	void UpdateResource();
	bool IsResourceValid() const { return Resource != nullptr; }

	uint64 GetVAddress() const
	{
		ensure(IsResourceValid());

		// If this triggers, this VT failed to allocate in the VTSpace.
		// This is most likely because the space has not enough room left for the size of this VT (fragmentation ?)
		// for lightmaps, increase r.VT.LightmapVTSpaceSize
		//todo Handle this gracefully at a higher level
		ensure(Resource->vAddress != ~0u);

		return Resource->vAddress;
	}

	// Get the maximum level this VT has data
	uint8 GetMaxLevel() const;

	FVector4 GetTransform(const FVector4& srcRect) const;

protected:

	/**
	 * Build the data for a given platform.
	 * Note: Settings is non const it will fill in some of the final settings for the platform.
	 */
#if WITH_EDITOR
	void BuildPlatformData(ITargetPlatform *Platform, FVirtualTextureBuilderSettings &Settings, bool bAllowAsync);
#endif
	void ReleaseResource();
	class FVirtualTexture* Resource;
	FRenderCommandFence ReleaseFence;

	FVirtualTextureBuiltData *Data;
};

class LightMapVirtualTextureLayerFlags
{
public:

	enum Flag
	{
		HqLayers = 0,				// We always store the two HQ coefficient textures
		SkyOcclusionLayer = 1,		// We have a sky occlusion layer
		AOMaterialMaskLayer = 2,	// We have an AO material mask layer
		ShadowMapLayer = 3,			// We have a shadow mask layer
		Default = HqLayers,			// Default settings used for initialization
		All = HqLayers | SkyOcclusionLayer | AOMaterialMaskLayer | ShadowMapLayer // All layers enabled, keep this up to date when adding!
	};

	static ENGINE_API int32 GetNumLayers(int32 LayerFlags);

	/**
	 * Returns INDEX_NONE if the layer is not present according to the passed in layer flags
	 * Otherwise returns the index of the layer the given flag is stored on
	 */
	static ENGINE_API int32 GetLayerIndex(int32 LayerFlags, Flag LayerFlag);
};

UCLASS(ClassGroup = Rendering)
class ENGINE_API ULightMapVirtualTexture : public UVirtualTexture
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = VirtualTexture)
	int32 LayerFlags;

	void BuildLightmapData(bool bAllowAsync);
};

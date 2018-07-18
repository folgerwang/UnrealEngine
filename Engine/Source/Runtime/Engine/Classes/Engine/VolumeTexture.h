// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture2D.h"
#include "VolumeTexture.generated.h"

class FTextureResource;

UCLASS(hidecategories=(Object, Compositing, ImportSettings), MinimalAPI)
class UVolumeTexture : public UTexture
{
	GENERATED_UCLASS_BODY()

public:
	/** Platform data. */
	FTexturePlatformData* PlatformData;
	TMap<FString, FTexturePlatformData*> CookedPlatformData;

#if WITH_EDITORONLY_DATA
	/** A (optional) reference texture from which the volume texture was built */
	UPROPERTY(EditAnywhere, Category=Source2D, meta=(DisplayName="Source Texture"))
	UTexture2D* Source2DTexture;
	/** The lighting Guid of the source 2D texture, used to trigger rebuild when the source changes. */
	UPROPERTY()
	FGuid SourceLightingGuid;
	UPROPERTY(EditAnywhere, Category=Source2D, meta=(DisplayName="Tile Size X"))
	/** The reference texture tile size X */
	int32 Source2DTileSizeX;
	/** The reference texture tile size Y */
	UPROPERTY(EditAnywhere, Category=Source2D, meta=(DisplayName="Tile Size Y"))
	int32 Source2DTileSizeY;
#endif

	ENGINE_API bool UpdateSourceFromSourceTexture();

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual FString GetDesc() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface.

	/** Trivial accessors. */
	FORCEINLINE int32 GetSizeX() const
	{
		return PlatformData ? PlatformData->SizeX : 0;
	}
	FORCEINLINE int32 GetSizeY() const
	{
		return PlatformData ? PlatformData->SizeY : 0;
	}
	FORCEINLINE int32 GetSizeZ() const
	{
		return PlatformData ? PlatformData->NumSlices : 0;
	}
	FORCEINLINE int32 GetNumMips() const
	{
		return PlatformData ? PlatformData->Mips.Num() : 0;
	}
	FORCEINLINE EPixelFormat GetPixelFormat() const
	{
		return PlatformData ? PlatformData->PixelFormat : PF_Unknown;
	}

	//~ Begin UTexture Interface
	virtual float GetSurfaceWidth() const override { return GetSizeX(); }
	virtual float GetSurfaceHeight() const override { return GetSizeY(); }
	virtual FTextureResource* CreateResource() override;
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void UpdateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_VolumeTexture; }
	virtual FTexturePlatformData** GetRunningPlatformData() override { return &PlatformData; }
	virtual TMap<FString, FTexturePlatformData*> *GetCookedPlatformData() override { return &CookedPlatformData; }
	//~ End UTexture Interface
	
	/**
	 * Calculates the size of this texture in bytes if it had MipCount miplevels streamed in.
	 *
	 * @param	MipCount	Number of mips to calculate size for, counting from the smallest 1x1 mip-level and up.
	 * @return	Size of MipCount mips in bytes
	 */
	uint32 CalcTextureMemorySize(int32 MipCount) const;

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 *
	 * @param	Enum	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes
	 */
	virtual uint32 CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const override;

#if WITH_EDITOR
	/**
	* Return maximum dimension for this texture type.
	*/
	virtual uint32 GetMaximumDimension() const override;

#endif

	ENGINE_API static bool ShaderPlatformSupportsCompression(EShaderPlatform ShaderPlatform);

protected:

#if WITH_EDITOR
	void UpdateMipGenSettings();
#endif
};




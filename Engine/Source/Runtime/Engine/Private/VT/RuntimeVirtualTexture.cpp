// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VT/RuntimeVirtualTexture.h"

#include "EngineModule.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "RendererInterface.h"
#include "UObject/UObjectIterator.h"
#include "VT/VirtualTextureUtility.h"


FRuntimeVirtualTextureRenderResource::FRuntimeVirtualTextureRenderResource(FVTProducerDescription const& InProducerDesc, IVirtualTexture* InVirtualTextureProducer)
	: ProducerDesc(InProducerDesc)
	, Producer(InVirtualTextureProducer)
	, AllocatedVirtualTexture(nullptr)
{
	check(InVirtualTextureProducer != nullptr);
}

void FRuntimeVirtualTextureRenderResource::InitRHI()
{
	ProducerHandle = GetRendererModule().RegisterVirtualTextureProducer(ProducerDesc, Producer);

	AcquireAllocatedVirtualTexture();
}

void FRuntimeVirtualTextureRenderResource::ReleaseRHI()
{
	ReleaseAllocatedVirtualTexture();

	GetRendererModule().ReleaseVirtualTextureProducer(ProducerHandle);
	ProducerHandle = FVirtualTextureProducerHandle();
}

IAllocatedVirtualTexture* FRuntimeVirtualTextureRenderResource::AcquireAllocatedVirtualTexture()
{
	check(IsInRenderingThread());
	
	if (AllocatedVirtualTexture == nullptr)
	{
		FAllocatedVTDescription VTDesc;
		VTDesc.Dimensions = ProducerDesc.Dimensions;
		VTDesc.TileSize = ProducerDesc.TileSize;
		VTDesc.TileBorderSize = ProducerDesc.TileBorderSize;
		VTDesc.NumLayers = ProducerDesc.NumLayers;
		VTDesc.bPrivateSpace = true; // Dedicated page table allocation for runtime VTs
		
		for (uint32 LayerIndex = 0u; LayerIndex < VTDesc.NumLayers; ++LayerIndex)
		{
			VTDesc.ProducerHandle[LayerIndex] = ProducerHandle;
			VTDesc.LocalLayerToProduce[LayerIndex] = LayerIndex;
		}
		
		AllocatedVirtualTexture = GetRendererModule().AllocateVirtualTexture(VTDesc);
	}

	return AllocatedVirtualTexture;
}

void FRuntimeVirtualTextureRenderResource::ReleaseAllocatedVirtualTexture()
{
	if (AllocatedVirtualTexture)
	{
		GetRendererModule().DestroyVirtualTexture(AllocatedVirtualTexture);
		AllocatedVirtualTexture = nullptr;
	}
}

URuntimeVirtualTexture::URuntimeVirtualTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Resource(nullptr)
{
}

URuntimeVirtualTexture::~URuntimeVirtualTexture()
{
	check(Resource == nullptr);
}

void URuntimeVirtualTexture::GetProducerDescription(FVTProducerDescription& OutDesc) const
{
	OutDesc.Name = GetFName();
	OutDesc.Dimensions = 2;
	OutDesc.TileSize = GetTileSize();
	OutDesc.TileBorderSize = GetTileBorderSize();
	OutDesc.WidthInTiles = GetWidth() / GetTileSize();
	OutDesc.HeightInTiles = GetHeight() / GetTileSize();
	OutDesc.MaxLevel = FMath::Max(FMath::CeilLogTwo(FMath::Max(OutDesc.WidthInTiles, OutDesc.HeightInTiles)) - RemoveLowMips, 1u);
	OutDesc.DepthInTiles = 1;

	switch (MaterialType)
	{
	case ERuntimeVirtualTextureMaterialType::BaseColor:
		OutDesc.NumLayers = 1;
		OutDesc.LayerFormat[0] = bCompressTextures ? PF_DXT1 : PF_B8G8R8A8;
		break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal:
		OutDesc.NumLayers = 2;
		OutDesc.LayerFormat[0] = bCompressTextures ? PF_DXT1 : PF_B8G8R8A8;
		OutDesc.LayerFormat[1] = bCompressTextures ? PF_BC5 : PF_B8G8R8A8;
		break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
		OutDesc.NumLayers = 2;
		OutDesc.LayerFormat[0] = bCompressTextures ? PF_DXT1 : PF_B8G8R8A8;
		OutDesc.LayerFormat[1] = bCompressTextures ? PF_DXT5 : PF_B8G8R8A8;
		break;
	default:
		checkf(0, TEXT("Invalid Runtime Virtual Texture setup: %s, %d"), *GetName(), MaterialType);
		OutDesc.NumLayers = 1;
		OutDesc.LayerFormat[0] = PF_B8G8R8A8;
		break;
	}
}

int32 URuntimeVirtualTexture::GetEstimatedPageTableTextureMemoryKb() const
{
	//todo[vt]: Estimate memory usage
	return 0;
}

int32 URuntimeVirtualTexture::GetEstimatedPhysicalTextureMemoryKb() const
{
	//todo[vt]: Estimate memory usage
	return 0;
}

IAllocatedVirtualTexture* URuntimeVirtualTexture::GetAllocatedVirtualTexture() const
{
	return (Resource != nullptr) ? Resource->GetAllocatedVirtualTexture() : nullptr;
}

FVector4 URuntimeVirtualTexture::GetUniformParameter(int32 Index)
{
	check(Index >= 0 && Index < sizeof(WorldToUVTransformParameters)/sizeof(WorldToUVTransformParameters[0]));
	
	return WorldToUVTransformParameters[Index];
}

void URuntimeVirtualTexture::Initialize(IVirtualTexture* InProducer, FTransform const& BoxToWorld)
{
	//todo[vt]: possible issues with precision in large worlds here it might be better to calculate/upload camera space relative transform per frame?
	WorldToUVTransformParameters[0] = BoxToWorld.GetTranslation();
	WorldToUVTransformParameters[1] = BoxToWorld.GetUnitAxis(EAxis::X) * 1.f / BoxToWorld.GetScale3D().X;
	WorldToUVTransformParameters[2] = BoxToWorld.GetUnitAxis(EAxis::Y) * 1.f / BoxToWorld.GetScale3D().Y;

	ReleaseResource();
	InitResource(InProducer);
	NotifyMaterials();
}

void URuntimeVirtualTexture::Release()
{
	ReleaseResource();
	NotifyMaterials();
}

void URuntimeVirtualTexture::InitResource(IVirtualTexture* InProducer)
{
	check(Resource == nullptr);
	if (Resource == nullptr && InProducer != nullptr)
	{
		FVTProducerDescription Desc;
		GetProducerDescription(Desc);
		Resource = new FRuntimeVirtualTextureRenderResource(Desc, InProducer);
		BeginInitResource(Resource);
	}
}

void URuntimeVirtualTexture::ReleaseResource()
{
	if (Resource != nullptr)
	{
		ReleaseResourceAndFlush(Resource);
		Resource = nullptr;
	}
}

void URuntimeVirtualTexture::BeginDestroy()
{
	ReleaseResource();
	Super::BeginDestroy();
}

#if WITH_EDITOR

void URuntimeVirtualTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnEditProperty.ExecuteIfBound(this);
}

#endif

void URuntimeVirtualTexture::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	OutTags.Add(FAssetRegistryTag("Width", FString::FromInt(GetWidth()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Height", FString::FromInt(GetHeight()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("TileSize", FString::FromInt(GetTileSize()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("TileBorderSize", FString::FromInt(GetTileBorderSize()), FAssetRegistryTag::TT_Numerical));
}

void URuntimeVirtualTexture::NotifyMaterials()
{
	//todo[vt]: This can be expensive. We need to make sure we don't call this too often (or at all?) in non-editor builds?
	
	// In non-editor builds we can come here > 1 times during PostLoad callbacks.
	// Is there a way to express the dependency so that materials are init'd after the VT is setup and then we don't need this at all?

	TSet<UMaterial*> BaseMaterialsThatUseThisTexture;
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* MaterialInterface = *It;

		TArray<UObject*> Textures;
		MaterialInterface->AppendReferencedTextures(Textures);

		for (auto Texture : Textures)
		{
			if (Texture == this)
			{
				BaseMaterialsThatUseThisTexture.Add(MaterialInterface->GetMaterial());
				break;
			}
		}
	}

	if (BaseMaterialsThatUseThisTexture.Num() > 0)
	{
		FMaterialUpdateContext UpdateContext;
		for (TSet<UMaterial*>::TConstIterator It(BaseMaterialsThatUseThisTexture); It; ++It)
		{
			(*It)->RecacheUniformExpressions(false);
			UpdateContext.AddMaterial(*It);
		}
	}
}

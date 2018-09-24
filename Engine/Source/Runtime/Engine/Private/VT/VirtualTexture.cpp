#include "VT/VirtualTexture.h"
#include "VT/VirtualTextureSpace.h"
#include "VT/VirtualTextureBuiltData.h"

#if WITH_EDITOR
#include "VT/VirtualTextureDataBuilder.h"
#endif

#include "VT/VirtualTextureChunkProviders.h"

DEFINE_LOG_CATEGORY(LogVirtualTexturingModule);

// Is there a cleaner place to put this??
static TAutoConsoleVariable<int32> CVarVirtualTexturedLightMaps(
	TEXT("r.VirtualTexturedLightmaps"),
	0,
	TEXT("Controls wether to stream the lightmaps using virtual texturing.\n") \
	TEXT(" 0: Disabled.\n") \
	TEXT(" 1: Enabled."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);


UVirtualTexture::UVirtualTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Space = nullptr;
	Data = new FVirtualTextureBuiltData;
	Resource = nullptr;
}

UVirtualTexture::~UVirtualTexture()
{
	delete Data;
}

void UVirtualTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	check(Data);
	Data->Serialize(Ar, this);
}

void UVirtualTexture::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UVirtualTexture* This = CastChecked<UVirtualTexture>(InThis);
	//Collector.AddReferencedObject(This->Space);

	Super::AddReferencedObjects(InThis, Collector);
}

#if WITH_EDITOR
void UVirtualTexture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Temp hack use the Rebuild property to get notifications from the UI
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UVirtualTexture, Rebuild))
	{
		RebuildData(false);
		Rebuild = false;
	}
}
#endif

void UVirtualTexture::PostLoad()
{
	Super::PostLoad();

	// Lightmap VTs can be present inside the map bulk data (editor only but they are serialized into a dummy temp variable) but when not using VT, do not create resources
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
	bool bUsingVTLightmaps = CVar->GetValueOnAnyThread() != 0;
	if (bUsingVTLightmaps == false)
	{
		return;
	}

	Space->ConditionalPostLoad();

	//TODO EDITOR: Rebuild data here if out of data and store in DDC etc.
	//we just do some basic sanity checks for now.
	checkf(Data->GetNumLayers() == Space->Layers.Num(), TEXT("Different number of layers in data and space, data needs to be rebuilt"));
	for (uint32 Layer = 0; Layer < Data->GetNumLayers(); Layer++)
	{
		checkf(Data->LayerTypes[Layer] == Space->GetTextureFormat(Layer), TEXT("Different layers formats in data and space, data needs to be rebuilt"));
	}

	UpdateResource();
}


void UVirtualTexture::BeginDestroy()
{
	Super::BeginDestroy();
	if (Resource)
	{
		BeginReleaseResource(Resource);
		ReleaseFence.BeginFence();
	}
}

bool UVirtualTexture::IsReadyForFinishDestroy()
{
	if (Resource == nullptr)
	{
		return Super::IsReadyForFinishDestroy();
	}
	return Super::IsReadyForFinishDestroy() && ReleaseFence.IsFenceComplete();
}

void UVirtualTexture::FinishDestroy()
{
	if (Resource)
	{
		delete Resource;
		Resource = nullptr;
	}

	Super::FinishDestroy();
}

void UVirtualTexture::ReleaseResource()
{
	if (Resource)
	{
		ReleaseResourceAndFlush(Resource);
		delete Resource;
		Resource = nullptr;
	}
}

void UVirtualTexture::UpdateResource()
{
	ReleaseResource();

	if (FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject) && Space != nullptr && Data->IsInitialized())
	{
		Resource = new FVirtualTexture(this);
		if (Resource)
		{
			BeginInitResource(Resource);
		}
	}
}

uint8 UVirtualTexture::GetMaxLevel() const
{
	const uint32 minTiles = FMath::Min(Data->NumTilesX, Data->NumTilesY);
	return FMath::CeilLogTwo(minTiles);
}

FVector4 UVirtualTexture::GetTransform(const FVector4& srcRect) const
{
	const uint64 vAddress = GetVAddress();

	const uint32 ofsXPixels = FMath::ReverseMortonCode2(vAddress) * Space->TileSize;
	const uint32 ofsYPixels = FMath::ReverseMortonCode2(vAddress >> 1) * Space->TileSize;

	const uint32 widthPixels = GetTextureBuildData()->Width;
	const uint32 heightPixels = GetTextureBuildData()->Height;

	const uint32 tileSetWidthPixels = Space->Size * Space->TileSize;
	const uint32 tileSetHeightPixels = Space->Size * Space->TileSize;

	struct DRect
	{
		double x;
		double y;
		double width;
		double height;
	};

	struct DTransform
	{
		double ofsX;
		double ofsY;
		double scaleX;
		double scaleY;
	};

	auto MapRect = [](DRect src, DRect dst, DTransform &out) {
		const double iSrcScaleX = 1.0 / src.width;
		const double iSrcScaleY = 1.0 / src.height;
		out.scaleX = iSrcScaleX * dst.width;
		out.scaleY = iSrcScaleY * dst.height;
		out.ofsX = -src.x*iSrcScaleX*dst.width + dst.x;
		out.ofsY = -src.y*iSrcScaleY*dst.height + dst.y;
	};

	// first map the lightmap UVs to 0-1 range	
	const DRect sourceRect = {
		srcRect.Z,
		srcRect.W,
		srcRect.X,
		srcRect.Y
	};
	const DRect destRect01 = { 0,0,1,1 };

	DTransform Transform01;
	MapRect(sourceRect, destRect01, Transform01);

	// now map them to VT range
	const DRect destRectVT = {
		ofsXPixels / (float)tileSetWidthPixels,
		ofsYPixels / (float)tileSetHeightPixels,
		widthPixels / (float)tileSetWidthPixels,
		heightPixels / (float)tileSetHeightPixels
	};

	const DRect sourceRect01 = {
		Transform01.ofsX,
		Transform01.ofsY,
		Transform01.scaleX,
		Transform01.scaleY
	};

	DTransform TransformVT;
	MapRect(sourceRect01, destRectVT, TransformVT);

	return FVector4(TransformVT.scaleX, TransformVT.scaleY, TransformVT.ofsX, TransformVT.ofsY);
}

void UVirtualTexture::RebuildData(bool bAllowAsync)
{
#if WITH_EDITOR
	ReleaseResource();

	if (Space == nullptr)
	{
		UE_LOG(LogVirtualTexturingModule, Error, TEXT("No virtual texutre space assigned."));
		return;
	}

	if (Layers.Num() != Space->Layers.Num())
	{
		UE_LOG(LogVirtualTexturingModule, Error, TEXT("Number of layers in the virtual texture differs from the number of layers in the space."));
		return;
	}
	else
	{
		int32 Width = 0;
		int32 Height = 0;
		for (int32 Layer = 0; Layer < Layers.Num(); Layer++)
		{
			UTexture *LayerTexture = Layers[Layer];
			if (LayerTexture == nullptr)
			{
				UE_LOG(LogVirtualTexturingModule, Error, TEXT("Null textures assigned to some of the layers."));
				return;
			}
			else
			{
				if (Layer == 0)
				{
					Width = Layers[0]->Source.GetSizeX();
					Height = Layers[0]->Source.GetSizeY();
				}
				else
				{
					// For now we demand that textures on different layers are the same size. This could be relaxed later on
					if (Width != Layers[Layer]->Source.GetSizeX() ||
						Height != Layers[Layer]->Source.GetSizeY())
					{
						UE_LOG(LogVirtualTexturingModule, Error, TEXT("Textures assigned to layers have different sizes."));
						return;
					}
				}
			}
		}
	}

	FVirtualTextureBuilderSettings Settings;
	Settings.DebugName = GetName();

	for (int32 Layer = 0; Layer < Layers.Num(); Layer++)
	{
		Settings.Layers.Add(FVirtualTextureBuilderLayerSettings(&Layers[Layer]->Source));
	}

	BuildPlatformData(nullptr, Settings, bAllowAsync);
	UpdateResource();
#endif
}

#if WITH_EDITOR
void UVirtualTexture::BuildPlatformData(ITargetPlatform *Platform, FVirtualTextureBuilderSettings &Settings, bool bAllowAsync)
{
	checkf(Settings.Layers.Num() == Space->Layers.Num(), TEXT("Layers in the settings does not match the number of layers in the space"));

	for (int32 Layer = 0; Layer < Settings.Layers.Num(); Layer++)
	{
		Settings.Layers[Layer].SourceBuildSettings.TextureFormatName = Space->GetTextureFormatName(Layer, Platform);
	}

	FVirtualTextureDataBuilder Builder(*Data);
	Builder.Build(Settings);
}
#endif // WITH_EDITOR

int32 LightMapVirtualTextureLayerFlags::GetNumLayers(int32 LayerFlags)
{
	static int32 NumLayers[8] =
	{
		2, // 0
		3, // 1: SkyOcclusionLayer
		3, // 2: AOMaterialMaskLayer
		4, // 3: SkyOcclusionLayer | AOMaterialMaskLayer
		3, // 4: ShadowMapLayer
		4, // 5: ShadowMapLayer | SkyOcclusionLayer
		4, // 6: ShadowMapLayer | AOMaterialMaskLayer
		5, // 7: ShadowMapLayer | AOMaterialMaskLayer | SkyOcclusionLayer
	};

	checkf(LayerFlags >= 0 && LayerFlags <= All, TEXT("Invalid layer flags field"));
	return NumLayers[LayerFlags];
}

int32 LightMapVirtualTextureLayerFlags::GetLayerIndex(int32 LayerFlags, Flag LayerFlag)
{
	if (LayerFlag == HqLayers)
	{
		return 0;
	}
	else
	{
		if ( (LayerFlags & LayerFlag) == 0)
		{
			return INDEX_NONE;
		}

		if (LayerFlag == SkyOcclusionLayer)
		{
			// Sky occlusion is always the first optional layer
			return 2;
		}	
		else if (LayerFlag == AOMaterialMaskLayer)
		{
			// AO material mask always come after the SkyOcclusionLayer 
			return 2 + ((LayerFlags & SkyOcclusionLayer) ? 1 : 0);
		}
		else if (LayerFlag == ShadowMapLayer)
		{
			// Shadow map is always the last optional layer
			return 2 + ((LayerFlags & SkyOcclusionLayer) ? 1 : 0)
				     + ((LayerFlags & AOMaterialMaskLayer) ? 1 : 0);
		}
		else
		{
			checkf(false, TEXT("Unknown layer"));
			return 0;
		}
	}

}

void ULightMapVirtualTexture::BuildLightmapData(bool bAllowAsync)
{
#if WITH_EDITOR
	if (Space == nullptr)
	{
		UE_LOG(LogVirtualTexturingModule, Error, TEXT("No virtual texutre space assigned."));
		return;
	}

	// Lightmap building assumes a variable number of layers based on the flags field
	if (Layers.Num() != LightMapVirtualTextureLayerFlags::GetNumLayers(LayerFlags))
	{
		UE_LOG(LogVirtualTexturingModule, Error, TEXT("Invalid number of lightmap layers."));
		return;
	}

	// Lightmap building assumes a variable number of layers based on the flags field
	if (Space->Layers.Num() != LightMapVirtualTextureLayerFlags::GetNumLayers(LayerFlags))
	{
		UE_LOG(LogVirtualTexturingModule, Error, TEXT("Invalid lightmap space."));
		return;
	}

	FVirtualTextureBuilderSettings Settings;
	Settings.DebugName = GetName();
	Settings.Layers.AddDefaulted(Space->Layers.Num());

	for (int32 Layer = 0; Layer < Space->Layers.Num(); Layer++)
	{
		// Layer 0 takes the lower half of the coefficients on source layer 0
		if (Layer == 0)
		{
			Settings.Layers[Layer].SourceRectangle = FIntRect(0, 0, Layers[0]->Source.GetSizeX(), Layers[0]->Source.GetSizeY() / 2);
			Settings.Layers[Layer].Source = &Layers[0]->Source;
			Settings.Layers[Layer].GammaSpace = Layers[0]->SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;
			Settings.Layers[Layer].SourceBuildSettings.MipGenSettings = Layers[0]->MipGenSettings;
		}
		// Layer 1 takes the upper half of the coefficients on source layer 0 (we ignore layer 1 for now)
		else if (Layer == 1)
		{
			Settings.Layers[Layer].SourceRectangle = FIntRect(0, Layers[0]->Source.GetSizeY() / 2, Layers[0]->Source.GetSizeX(), Layers[0]->Source.GetSizeY());
			Settings.Layers[Layer].Source = &Layers[0]->Source;
			Settings.Layers[Layer].GammaSpace = Layers[0]->SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;
			Settings.Layers[Layer].SourceBuildSettings.MipGenSettings = Layers[0]->MipGenSettings;
		}
		// Other layers just get imported as-is
		else
		{
			Settings.Layers[Layer].Source = &Layers[Layer]->Source;
			Settings.Layers[Layer].GammaSpace = Layers[Layer]->SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;
			Settings.Layers[Layer].SourceBuildSettings.MipGenSettings = Layers[Layer]->MipGenSettings;
		}

		// Grayscale or alpha value is stored in red actually
		if (Space->Layers[Layer].CompressionSettings == TC_Grayscale || Space->Layers[Layer].CompressionSettings == TC_Alpha)
		{
			Settings.Layers[Layer].SourceBuildSettings.bReplicateRed = true;
		}
	}

	BuildPlatformData(nullptr, Settings, bAllowAsync);
	UpdateResource();
#endif
}

ULightMapVirtualTexture::ULightMapVirtualTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LayerFlags = LightMapVirtualTextureLayerFlags::Default;
}

void FVirtualTexture::InitDynamicRHI()
{
	ensure(Owner->Space->GetRenderResource());
	Provider = new FChunkProvider(Owner);
	vAddress = Provider->v_Address;
}

void FVirtualTexture::ReleaseDynamicRHI()
{
	delete Provider;
}

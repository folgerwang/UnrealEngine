#include "VT/VirtualTextureSpace.h"
#include "RenderResource.h"
#include "Runtime/Renderer/Public/VirtualTexturing.h"
#include "LightMap.h"
#include "Serialization/CustomVersion.h"
#include "EngineModule.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif

const FGuid FVirtualTextureSpaceCustomVersion::Key(0xa5adcf52, 0x63b24351, 0xA4D68C06, 0x905560c8);
static FCustomVersionRegistration GVTSpaceRegisterVersion(FVirtualTextureSpaceCustomVersion::Key, FVirtualTextureSpaceCustomVersion::Latest, TEXT("VirtualTextureSpaceVersion"));


static EPixelFormat GetPixelFormat(PageTableFormat f)
{
	switch (f)
	{
	case PTF_16:
		return PF_R16_UINT;
	case PTF_32:
		return PF_R8G8B8A8;
	default:
		return PF_Unknown;
	}
}

UVirtualTextureSpace::UVirtualTextureSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TileSize = 128;
	BorderWidth = 4;
	Layers.AddDefaulted(1);
	Resource = nullptr;
	Size = 1024;
	PoolSize = 64;
	Format = PTF_16;
	Dimensions = 2;
}

void UVirtualTextureSpace::PostLoad()
{
	Super::PostLoad();

	// Lightmap VTs can be present inside the map bulk data (editor only but they are serialized into a dummy temp variable) but when not using VT, do not create resources
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
	bool bUsingVTLightmaps = CVar->GetValueOnAnyThread() != 0;
	if (bUsingVTLightmaps == false)
	{
		return;
	}
	UpdateResource();
}

void UVirtualTextureSpace::BeginDestroy()
{
	Super::BeginDestroy();
	if (Resource)
	{
		BeginReleaseResource(Resource);
		ReleaseFence.BeginFence();
	}
}

bool UVirtualTextureSpace::IsReadyForFinishDestroy()
{
	if (Resource == nullptr)
	{
		return Super::IsReadyForFinishDestroy();
	}
	return Super::IsReadyForFinishDestroy() && ReleaseFence.IsFenceComplete();
}

void UVirtualTextureSpace::FinishDestroy()
{
	if (Resource)
	{
		GetRendererModule().DestroyVirtualTextureSpace(Resource);
		Resource = nullptr;
	}
	Super::FinishDestroy();
}

void UVirtualTextureSpace::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FVirtualTextureSpaceCustomVersion::Key);
	const int32 Version = Ar.CustomVer(FVirtualTextureSpaceCustomVersion::Key);

	Super::Serialize(Ar);

#if WITH_EDITOR

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
	const bool bVTLightmapsEnabled = CVar ? (CVar->GetValueOnAnyThread() != 0) : false;

	if (bVTLightmapsEnabled)
	{
		// TODO EDITOR: Make sure they are up to date before saving. Ideally this is not called here 
		// and the actual editor makes sure formats are up to date before we get here.
		UpdateLayerFormats();
	}	

	// If we are cooking save the layer formats for the platform we're shipping to
	// instead of the "LayerFormats" which contain values for the platform we're currently
	// running on.
	if (Ar.IsCooking())
	{
		TArray<TEnumAsByte<EPixelFormat>> CookedLayerFormats;
		CookedLayerFormats.AddUninitialized(Layers.Num());
		for (int32 i = 0; i < Layers.Num(); ++i)
		{
			EPixelFormat VTFormat = EPixelFormat::PF_Unknown;
			if (bVTLightmapsEnabled)
			{
				VTFormat = GetTextureFormat(i, Ar.CookingTarget());
			}
			else
			{
				CookedLayerFormats[i] = VTFormat;
			}
		}
		Ar << CookedLayerFormats;
	}
	else
#endif
	{
		Ar << LayerFormats;
	}
}

#if WITH_EDITOR
void UVirtualTextureSpace::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// atm we do not update the resource as this is invalid as long as there are VTs alive that reference this space
	// we would need to do a full lightmap invalidation and flush
	//UpdateResource();
	UpdateLayerFormats();
}

void UVirtualTextureSpace::UpdateLayerFormats()
{
	// TODO EDITOR: These are just updated here but we should also rebuild any virtual textures referencing this space
	// to ensure they are updated if the format is changed
	LayerFormats.SetNum(Layers.Num());
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		LayerFormats[i] = GetTextureFormat(i, nullptr);
	}
}

FName UVirtualTextureSpace::GetTextureFormatName(int32 LayerIndex, const ITargetPlatform *Platform)
{
	if (Platform == nullptr)
	{
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		Platform = TPM->GetRunningTargetPlatform();
	}

	FVirtualTextureLayer &Layer = Layers[LayerIndex];
	FName Result = Platform->GetVirtualTextureLayerFormat(Layer.Format, Layer.bCompressed, !Layer.bHasAlpha, true, Layer.CompressionSettings);
	checkf(Result.IsNone() == false, TEXT("ITargetPlatform::GetVirtualTextureLayerFormat returned null probably VT is not implemented/tested on this platform yet"));
	return Result;
}

EPixelFormat UVirtualTextureSpace::GetTextureFormat(int32 LayerIndex, const ITargetPlatform *Platform)
{
	FString PixelFormatString = FString(TEXT("PF_")) + GetTextureFormatName(LayerIndex, Platform).ToString();

	// Not all names are consistent so fix them up here...
	// there doesn't seem to be a clean way to handle this if we want
	// Platform->GetVirtualTextureLayerFormat to return names consistent with the names returned
	// by the regular (non-VT) texture functions.
	if (PixelFormatString == TEXT("PF_BGRA8"))
	{
		PixelFormatString = TEXT("PF_B8G8R8A8");
	}

	UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();
	int32 PixelFormat = PixelFormatEnum->GetValueByName(FName(*PixelFormatString));
	checkf(PixelFormat != INDEX_NONE, TEXT("Unkown pixel format string"));
	return (EPixelFormat)PixelFormat;
}
#endif

EPixelFormat UVirtualTextureSpace::GetTextureFormat(int32 Layer)
{
	return LayerFormats[Layer];
}

void UVirtualTextureSpace::ReleaseResource()
{
	if (Resource)
	{
		ReleaseResourceAndFlush(Resource);
		GetRendererModule().DestroyVirtualTextureSpace(Resource);
		Resource = nullptr;
	}
}

void UVirtualTextureSpace::UpdateResource()
{
	ReleaseResource();

	if (FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject))
	{
#if WITH_EDITOR
		// TODO EDITOR: Make sure they are up to date before creating render side. Ideally this is not called here 
		// and the actual editor makes sure formats are up to date before we get here.
		UpdateLayerFormats();
#endif

		FVirtualTextureSpaceDesc desc;
		GetSpaceDesc(desc);
		Resource = GetRendererModule().CreateVirtualTextureSpace(desc);
		if (Resource)
		{
			BeginInitResource(Resource);
		}
	}
}

void UVirtualTextureSpace::GetSpaceDesc(FVirtualTextureSpaceDesc& desc)
{
	desc.Size = Size;
	desc.Dimensions = Dimensions;
	desc.PageTableFormat = GetPixelFormat(Format);
	desc.PhysicalTileSize = TileSize + 2 * BorderWidth;
	desc.Poolsize = PoolSize;

	EPixelFormat PhysicalTextureFormats[VIRTUALTEXTURESPACE_MAXLAYERS] = {PF_Unknown};
	check(Layers.Num() <= VIRTUALTEXTURESPACE_MAXLAYERS);
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		PhysicalTextureFormats[i] =  LayerFormats[i];
	}
	FMemory::Memcpy(desc.PhysicalTextureFormats, PhysicalTextureFormats, sizeof(PhysicalTextureFormats));
}

static int32 LightmapVTPoolSize = 64;
static FAutoConsoleVariableRef CVarLightmapVTPoolSize(
	TEXT("r.VT.LightmapPoolsize"),
	LightmapVTPoolSize,
	TEXT("Size of the lightmap VT pool. Larger means less streaming at expense of memory. default 64\n")
	, ECVF_ReadOnly
);

static int32 LightmapVTSpaceSize = 1024; //== 128k*128k with 128 tiles
static FAutoConsoleVariableRef CVarLightmapVTSpaceSize(
	TEXT("r.VT.LightmapVTSpaceSize"),
	LightmapVTSpaceSize,
	TEXT("Size of the VT lightmap space. tradeoff between total maximum lightmap dimensions and resources used. default 512\n")
	, ECVF_ReadOnly
);

ULightMapVirtualTextureSpace::ULightMapVirtualTextureSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}


void ULightMapVirtualTextureSpace::GetSpaceDesc(FVirtualTextureSpaceDesc& desc)
{
	Super::GetSpaceDesc(desc);
	desc.Dimensions = 2;
	desc.PageTableFormat = LIGHTMAP_VT_16BIT ? PF_R16_UINT : PF_R8G8B8A8;
	int32 ActualPoolSize = LightmapVTPoolSize;
	if (LightmapVTPoolSize > 64 && LIGHTMAP_VT_16BIT)
	{
		ActualPoolSize = 64;
		UE_LOG(LogVirtualTexturingModule, Error, TEXT("The lightmap VT poolsize is limited to 64 because LIGHTMAP_VT_16BIT is enabled"));
	}
	desc.Poolsize = ActualPoolSize;
	desc.Size = LightmapVTSpaceSize;
}


#include "OvrAvatarManager.h"
#include "OvrAvatar.h"
#include "OVR_Avatar.h"
#include "RenderUtils.h"
#include "UObject/UObjectIterator.h"
#include "OculusHMDModule.h"

DEFINE_LOG_CATEGORY(LogAvatars);

FOvrAvatarManager* FOvrAvatarManager::sAvatarManager = nullptr;

FSoftObjectPath FOvrAvatarManager::AssetList[] =
{
	FString(TEXT("/OculusAvatar/Materials/AvatarsPBR_2/OculusAvatars_PBRV2.OculusAvatars_PBRV2")),
	FString(TEXT("/OculusAvatar/Materials/AvatarsPBR_2/OculusAvatars_PBRV2_2_Depth.OculusAvatars_PBRV2_2_Depth")),
	FString(TEXT("/OculusAvatar/Materials/AvatarsPBR_2/OculusAvatars_PBRV2_Masked.OculusAvatars_PBRV2_Masked")),
	FString(TEXT("/OculusAvatar/Materials/OculusAvatarsPBR.OculusAvatarsPBR")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_OFF/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_OFF_P_ON/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_OFF/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Off/N_ON_P_ON/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_OFF/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_OFF_P_ON/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_OFF/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_0Layers.OculusAvatar8Layers_Inst_0Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_1Layers.OculusAvatar8Layers_Inst_1Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_2Layers.OculusAvatar8Layers_Inst_2Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_3Layers.OculusAvatar8Layers_Inst_3Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_4Layers.OculusAvatar8Layers_Inst_4Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_5Layers.OculusAvatar8Layers_Inst_5Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_6Layers.OculusAvatar8Layers_Inst_6Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_7Layers.OculusAvatar8Layers_Inst_7Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/On/N_ON_P_ON/OculusAvatar8Layers_Inst_8Layers.OculusAvatar8Layers_Inst_8Layers")),
	FString(TEXT("/OculusAvatar/Materials/v1/Inst/Projector.Projector"))
};

TArray<UObject*> FOvrAvatarManager::AssetObjects;

FOvrAvatarManager& FOvrAvatarManager::Get()
{
	return sAvatarManager != nullptr ? *sAvatarManager : *(sAvatarManager = new FOvrAvatarManager());
}

void FOvrAvatarManager::Destroy()
{
	if (sAvatarManager)
	{
		delete sAvatarManager;
		sAvatarManager = nullptr;
	}
}

FOvrAvatarManager::~FOvrAvatarManager()
{
	if (OVRPluginHandle)
	{
		FPlatformProcess::FreeDllHandle(OVRPluginHandle);
		OVRPluginHandle = nullptr;
	}
}

static const FString sTextureFormatStrings[ovrAvatarTextureFormat_Count] =
{
	FString("ovrAvatarTextureFormat_RGB24"),
	FString("ovrAvatarTextureFormat_DXT1"),
	FString("ovrAvatarTextureFormat_DXT5")
};

static const FString sOvrEmptyString = "";

static const FString TextureFormatToString(ovrAvatarTextureFormat format)
{
	return format < ovrAvatarTextureFormat_Count ? sTextureFormatStrings[format] : sOvrEmptyString;
}

bool FOvrAvatarManager::Tick(float DeltaTime)
{
	if (!IsInitialized)
		return false;

	while (ovrAvatarMessage* message = ovrAvatarMessage_Pop())
	{
		switch (ovrAvatarMessage_GetType(message))
		{
		case ovrAvatarMessageType_AvatarSpecification:
			HandleAvatarSpecification(ovrAvatarMessage_GetAvatarSpecification(message));
			break;
		case ovrAvatarMessageType_AssetLoaded:
			HandleAssetLoaded(ovrAvatarMessage_GetAssetLoaded(message));
			break;
		default:
			break;
		}

		ovrAvatarMessage_Free(message);
	}

	return true;
}

void FOvrAvatarManager::SDKLogger(const char * str)
{	
	UE_LOG(LogAvatars, Display, TEXT("[AVATAR SDK]: %s"), *FString(str));
}

void FOvrAvatarManager::InitializeSDK()
{
	if (!IsInitialized)
	{
#if WITH_EDITORONLY_DATA
		for (int32 AssetIndex = 0; AssetIndex < ARRAY_COUNT(AssetList); ++AssetIndex)
		{
			UObject* AssetObj = AssetList[AssetIndex].TryLoad();
			if (AssetObj != nullptr)
			{
				AssetObjects.AddUnique(AssetObj);
			}
		}
#endif
		if (IOculusHMDModule::IsAvailable())
		{
			OVRPluginHandle = FOculusHMDModule::GetOVRPluginHandle();
		}

		IsInitialized = true;
		AVATAR_APP_ID = TCHAR_TO_ANSI(*GConfig->GetStr(TEXT("OnlineSubsystemOculus"), TEXT("RiftAppId"), GEngineIni));
		ovrAvatar_Initialize(AVATAR_APP_ID);

		ovrAvatar_RegisterLoggingCallback(&FOvrAvatarManager::SDKLogger);
	}
}

void FOvrAvatarManager::ShutdownSDK()
{
	if (IsInitialized)
	{
#if WITH_EDITORONLY_DATA
		AssetObjects.Empty();
#endif
		IsInitialized = false;
		ovrAvatar_Shutdown();
	}
}

void FOvrAvatarManager::HandleAvatarSpecification(const ovrAvatarMessage_AvatarSpecification* message)
{
	UE_LOG(LogAvatars, Display, TEXT("[Avatars] Request Spec Arrived [%llu]"), message->oculusUserID);

	for (TObjectIterator<UOvrAvatar> Itr; Itr; ++Itr)
	{
		(*Itr)->HandleAvatarSpecification(message);
	}
}

void FOvrAvatarManager::HandleAssetLoaded(const ovrAvatarMessage_AssetLoaded* message)
{
	for (TObjectIterator<UOvrAvatar> Itr; Itr; ++Itr)
	{
		(*Itr)->HandleAssetLoaded(message);
	}
}

void FOvrAvatarManager::LoadTexture(const uint64_t id, const ovrAvatarTextureAssetData* data)
{ 
	UE_LOG(LogAvatars, Display, TEXT("[Avatars] Loaded Texture: [%llu] - [%s]"), id, *TextureFormatToString(data->format));

	const bool isNormalMap = NormalMapIDs.Find(id) != nullptr;
	Textures.Add(id, LoadTexture(data, isNormalMap));
}


UTexture2D* FOvrAvatarManager::LoadTexture(const ovrAvatarTextureAssetData* data, bool isNormalMap)
{
	const uint8_t* TextureData = data->textureData;
	uint8_t* NewTextureData = nullptr;
	EPixelFormat PixelFormat = PF_Unknown;
	uint64_t TextureSize = data->textureDataSize;

	switch (data->format)
	{
	case ovrAvatarTextureFormat_RGB24:
	{
		check(data->textureDataSize % 3 == 0);

		PixelFormat = EPixelFormat::PF_R8G8B8A8;
		const auto PixelCount = data->textureDataSize / 3;
		const auto NewTextureSize = PixelCount * 4;
		NewTextureData = (uint8_t*)FMemory::Malloc(NewTextureSize);
		uint8_t* TextureDataIter = NewTextureData;

		for (int pixel_index = 0; pixel_index < PixelCount; ++pixel_index)
		{
			// rgba -> bgra
			TextureDataIter[0] = TextureData[2];
			TextureDataIter[1] = TextureData[1];
			TextureDataIter[2] = TextureData[0];
			TextureDataIter[3] = 255;
			TextureData += 3;
			TextureDataIter += 4;
		}

		TextureSize = NewTextureSize;
		TextureData = NewTextureData;
		break;
	}

	case ovrAvatarTextureFormat_DXT1:
		PixelFormat = EPixelFormat::PF_DXT1;
		break;

	case ovrAvatarTextureFormat_DXT5:
		PixelFormat = EPixelFormat::PF_DXT5;
		break;

	default:
		UE_LOG(LogAvatars, Warning, TEXT("[Avatars] Unknown pixel format [%u]."), (int)data->format);
		break;
	}

	// TODO SW: Return Default Texture
	if (PixelFormat == PF_Unknown)
		return nullptr;

	const uint32_t BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32_t BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32_t BlockBytes = GPixelFormats[PixelFormat].BlockBytes;

	uint32_t Width = data->sizeX;
	uint32_t Height = data->sizeY;

	UTexture2D* UnrealTexture = NULL;

	if (Width > 0 && Height > 0 && Width % BlockSizeX == 0 && Height % BlockSizeY == 0)
	{
		UnrealTexture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);

		UnrealTexture->PlatformData = new FTexturePlatformData();
		UnrealTexture->PlatformData->SizeX = Width;
		UnrealTexture->PlatformData->SizeY = Height;
		UnrealTexture->PlatformData->PixelFormat = PixelFormat;
		UnrealTexture->SRGB = !isNormalMap;

		uint32_t DataOffset = 0;

		for (uint32_t MipIndex = 0; MipIndex < data->mipCount; MipIndex++)
		{
			const uint32_t BlocksX = Width / BlockSizeX;
			const uint32_t BlocksY = Height / BlockSizeY;
			const uint32_t MipSize = BlocksX * BlocksY * BlockBytes;

			if (MipSize == 0)
				break;

			check(DataOffset + MipSize <= TextureSize)

			FTexture2DMipMap* MipMap = new FTexture2DMipMap();

			UnrealTexture->PlatformData->Mips.Add(MipMap);
			MipMap->SizeX = Width;
			MipMap->SizeY = Height;
			MipMap->BulkData.Lock(LOCK_READ_WRITE);

			void* MipMemory = MipMap->BulkData.Realloc(MipSize);
			FMemory::Memcpy(MipMemory, TextureData + DataOffset, MipSize);
			DataOffset += MipSize;

			MipMap->BulkData.Unlock();

			Width /= 2;
			Height /= 2;
		}
	}

	if (NewTextureData)
	{
		delete NewTextureData;
	}

	// TODO SW: Offload to background thread to avoid hiccups on load.
	UnrealTexture->UpdateResource();

	return UnrealTexture;
}

UTexture* FOvrAvatarManager::FindTexture(uint64_t id) const
{
	if (auto Tex = Textures.Find(id))
	{
		return Tex->IsValid() ? Tex->Get() : nullptr;
	}

	return nullptr;
}

void FOvrAvatarManager::CacheNormalMapID(uint64_t id)
{
	if (!NormalMapIDs.Contains(id))
	{
		NormalMapIDs.Emplace(id);
	}
}

// Setting a max in case there is no consumer and recording turned on.
static const uint32_t SANITY_SIZE = 500; 
void FOvrAvatarManager::QueueAvatarPacket(ovrAvatarPacket* packet)
{
	if (packet == nullptr)
	{
		return;
	}

	SerializedPacketBuffer Buffer;

	for (auto& QueuePair : AvatarPacketQueues)
	{
		auto Queue = QueuePair.Value;
		if (Queue->PacketQueueSize >= SANITY_SIZE)
		{
			UE_LOG(LogAvatars, Warning, TEXT("[Avatars] Unexpectedly large amount of packets recorded, losing data"));
			Queue->PacketQueue.Dequeue(Buffer);
			Queue->PacketQueueSize--;
			delete[] Buffer.Buffer;
		}

		Queue->PacketQueueSize++;

		// TODO SW: We should use a fast slot allocator for these packets.
		Buffer.Size = ovrAvatarPacket_GetSize(packet);
		Buffer.Buffer = new uint8_t[Buffer.Size];
		ovrAvatarPacket_Write(packet, Buffer.Size, Buffer.Buffer);
		Queue->PacketQueue.Enqueue(Buffer);
	}

	ovrAvatarPacket_Free(packet);
}

ovrAvatarPacket* FOvrAvatarManager::RequestAvatarPacket(const FString& key)
{
	ovrAvatarPacket* ReturnPacket = nullptr;

	if (auto Queue = AvatarPacketQueues.Find(key))
	{
		SerializedPacketBuffer Buffer;

		if ((*Queue)->PacketQueue.Peek(Buffer))
		{
			(*Queue)->PacketQueueSize--;
			(*Queue)->PacketQueue.Dequeue(Buffer);
			ReturnPacket = ovrAvatarPacket_Read(Buffer.Size, Buffer.Buffer);
			delete[] Buffer.Buffer;
		}
	}

	return ReturnPacket;
}

void FOvrAvatarManager::RegisterRemoteAvatar(const FString& key)
{
	check(!AvatarPacketQueues.Find(key));

	AvatarPacketQueue* NewQueue = new AvatarPacketQueue();
	AvatarPacketQueues.Add(key, NewQueue);
}

void FOvrAvatarManager::UnregisterRemoteAvatar(const FString& key)
{
	if (auto Queue = AvatarPacketQueues.Find(key))
	{
		SerializedPacketBuffer Buffer;
		while ((*Queue)->PacketQueue.Peek(Buffer))
		{
			(*Queue)->PacketQueue.Dequeue(Buffer);
			delete[] Buffer.Buffer;
		}

		AvatarPacketQueues.Remove(key);

		delete (*Queue);
	}
}

float FOvrAvatarManager::GetSDKPacketDuration(ovrAvatarPacket* packet)
{
	return packet != nullptr ? ovrAvatarPacket_GetDurationSeconds(packet) : 0.f;
}

void FOvrAvatarManager::FreeSDKPacket(ovrAvatarPacket* packet)
{
	if (packet != nullptr)
	{
		ovrAvatarPacket_Free(packet);
	}
}


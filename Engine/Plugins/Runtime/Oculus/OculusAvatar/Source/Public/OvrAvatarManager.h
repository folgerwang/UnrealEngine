// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "OVR_Avatar.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Engine/Texture2D.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAvatars, Log, All);

class OCULUSAVATAR_API FOvrAvatarManager : public FTickerObjectBase
{
public:
	static FOvrAvatarManager& Get();

	static void Destroy();

	bool Tick(float DeltaTime) override;

	void InitializeSDK();
	void ShutdownSDK();

	void LoadTexture(const uint64_t id, const ovrAvatarTextureAssetData* data);
	UTexture* FindTexture(uint64_t id) const; 
	void CacheNormalMapID(uint64_t id);

	//These both call from the main game thread so should be thread safe.
	void QueueAvatarPacket(ovrAvatarPacket* packet);
	ovrAvatarPacket* RequestAvatarPacket(const FString& key);

	void RegisterRemoteAvatar(const FString& key);
	void UnregisterRemoteAvatar(const FString& key);

	float GetSDKPacketDuration(ovrAvatarPacket* packet);
	void FreeSDKPacket(ovrAvatarPacket* packet);

	bool IsOVRPluginValid() const;
	
	void SetSDKLoggingLevel(ovrAvatarLogLevel level) { ovrAvatar_SetLoggingLevel(level); }
private:
	static void SDKLogger(const char * str);

	FOvrAvatarManager() {};
	~FOvrAvatarManager();

	void HandleAvatarSpecification(const ovrAvatarMessage_AvatarSpecification* message);
	void HandleAssetLoaded(const ovrAvatarMessage_AssetLoaded* message);

	UTexture2D* LoadTexture(const ovrAvatarTextureAssetData* data, bool isNormalMap);

	bool IsInitialized = false;

	TMap<uint64, TWeakObjectPtr<UTexture>> Textures;
	TSet<uint64> NormalMapIDs;

	static FOvrAvatarManager* sAvatarManager;
	const char* AVATAR_APP_ID = nullptr;

	struct SerializedPacketBuffer
	{
		uint32_t Size;
		uint8_t* Buffer;
	};

	struct AvatarPacketQueue
	{
		TQueue<SerializedPacketBuffer> PacketQueue;
		uint32_t PacketQueueSize = 0;
	};

	TMap<FString, AvatarPacketQueue*> AvatarPacketQueues;
	
	void* OVRPluginHandle = nullptr;
	void* OVRAvatarHandle = nullptr;

	ovrAvatarLogLevel LogLevel = ovrAvatarLogLevel::ovrAvatarLogLevel_Silent;

	static FSoftObjectPath AssetList[];
	static TArray<UObject*> AssetObjects;
};

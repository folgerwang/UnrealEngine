// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapSecureStorage.h"
#include "IMagicLeapSecureStoragePlugin.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"

#if WITH_MLSDK
#include "ml_secure_storage.h"
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogSecureStorage, Display, All);

class FMagicLeapSecureStoragePlugin : public IMagicLeapSecureStoragePlugin
{
public:
	virtual void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
#if WITH_MLSDK
		APISetup.LoadDLL(TEXT("ml_secure_storage"));
#endif
	}

	virtual void ShutdownModule() override
	{
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

private:
	FMagicLeapAPISetup APISetup;
};

IMPLEMENT_MODULE(FMagicLeapSecureStoragePlugin, MagicLeapSecureStorage);

//////////////////////////////////////////////////////////////////////////

template<class T>
bool UMagicLeapSecureStorage::PutSecureBlob(const FString& Key, const T* DataToStore)
{
#if WITH_MLSDK
	return MLSecureStoragePutBlob(TCHAR_TO_ANSI(*Key), reinterpret_cast<const uint8*>(DataToStore), sizeof(T));
#else
	return false;
#endif //WITH_MLSDK
}

template<>
bool UMagicLeapSecureStorage::PutSecureBlob<FString>(const FString& Key, const FString* DataToStore)
{
#if WITH_MLSDK
	return MLSecureStoragePutBlob(TCHAR_TO_ANSI(*Key), reinterpret_cast<const uint8*>(TCHAR_TO_ANSI(*(*DataToStore))), DataToStore->Len() + 1);
#else
	return false;
#endif
}

template<class T>
bool UMagicLeapSecureStorage::GetSecureBlob(const FString& Key, T& DataToRetrieve)
{
	uint8* blob = nullptr;
	size_t blobLength = 0;

	bool bResult = false;

#if WITH_MLSDK
	bResult = MLSecureStorageGetBlob(TCHAR_TO_ANSI(*Key), &blob, &blobLength);
	if (bResult)
	{
		if (blob == nullptr)
		{
			UE_LOG(LogSecureStorage, Error, TEXT("Error retrieving secure blob with key %s. Blob was null."), *Key);
			bResult = false;
		}
		else if (blobLength != sizeof(T))
		{
			UE_LOG(LogSecureStorage, Error, TEXT("Size of blob data %s does not match the size of requested data type. Requested size = %d vs Actual size = %d"), *Key, sizeof(T), blobLength);
			bResult = false;
			// Replace with library function call when it comes online.
			free(blob);
		}
		else
		{
			DataToRetrieve = *reinterpret_cast<T*>(blob);
			// Replace with library function call when it comes online.
			free(blob);
		}
	}
	else
	{
		UE_LOG(LogSecureStorage, Error, TEXT("Error retrieving secure blob with key %s"), *Key);
	}
#endif //WITH_MLSDK

	return bResult;
}

template<>
bool UMagicLeapSecureStorage::GetSecureBlob<FString>(const FString& Key, FString& DataToRetrieve)
{
	uint8* blob = nullptr;
	size_t blobLength = 0;

	bool bResult = false;
#if WITH_MLSDK
	bResult = MLSecureStorageGetBlob(TCHAR_TO_ANSI(*Key), &blob, &blobLength);
#endif //WITH_MLSDK
	if (bResult)
	{
		if (blob == nullptr)
		{
			UE_LOG(LogSecureStorage, Error, TEXT("Error retrieving secure blob with key %s"), *Key);
			bResult = false;
		}
		else
		{
			DataToRetrieve = FString(ANSI_TO_TCHAR(reinterpret_cast<ANSICHAR*>(blob)));
			// Replace with library function call when it comes online.
			free(blob);
		}
	}
	else
	{
		UE_LOG(LogSecureStorage, Error, TEXT("Error retrieving secure blob with key %s"), *Key);
	}

	return bResult;
}

bool UMagicLeapSecureStorage::PutSecureBool(const FString& Key, bool DataToStore)
{
	return PutSecureBlob<bool>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureByte(const FString& Key, uint8 DataToStore)
{
	return PutSecureBlob<uint8>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureInt(const FString& Key, int32 DataToStore)
{
	return PutSecureBlob<int32>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureFloat(const FString& Key, float DataToStore)
{
	return PutSecureBlob<float>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureString(const FString& Key, const FString& DataToStore)
{
	return PutSecureBlob<FString>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureVector(const FString& Key, const FVector& DataToStore)
{
	return PutSecureBlob<FVector>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureRotator(const FString& Key, const FRotator& DataToStore)
{
	return PutSecureBlob<FRotator>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::PutSecureTransform(const FString& Key, const FTransform& DataToStore)
{
	return PutSecureBlob<FTransform>(Key, &DataToStore);
}

bool UMagicLeapSecureStorage::GetSecureBool(const FString& Key, bool& DataToRetrieve)
{
	return GetSecureBlob<bool>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureByte(const FString& Key, uint8& DataToRetrieve)
{
	return GetSecureBlob<uint8>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureInt(const FString& Key, int32& DataToRetrieve)
{
	return GetSecureBlob<int32>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureFloat(const FString& Key, float& DataToRetrieve)
{
	return GetSecureBlob<float>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureString(const FString& Key, FString& DataToRetrieve)
{
	return GetSecureBlob<FString>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureVector(const FString& Key, FVector& DataToRetrieve)
{
	return GetSecureBlob<FVector>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureRotator(const FString& Key, FRotator& DataToRetrieve)
{
	return GetSecureBlob<FRotator>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::GetSecureTransform(const FString& Key, FTransform& DataToRetrieve)
{
	return GetSecureBlob<FTransform>(Key, DataToRetrieve);
}

bool UMagicLeapSecureStorage::DeleteSecureData(const FString& Key)
{
#if WITH_MLSDK
	return MLSecureStorageDeleteBlob(TCHAR_TO_ANSI(*Key));
#else
	return false;
#endif //WITH_MLSDK
}

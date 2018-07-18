// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysXLibs.cpp: PhysX library imports
=============================================================================*/
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "EngineDefines.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "EngineLogs.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

#if WITH_PHYSX 

// PhysX library imports
namespace PhysDLLHelper
{
	const static int32 NumModuleLoadRetries = 5;
	const static float ModuleReloadDelay = 0.5f;

#if PLATFORM_WINDOWS || PLATFORM_MAC
	void* PxFoundationHandle = nullptr;
	void* PhysX3CommonHandle = nullptr;
	void* PhysX3Handle = nullptr;
	void* PxPvdSDKHandle = nullptr;
	void* PhysX3CookingHandle = nullptr;
	void* nvToolsExtHandle = nullptr;
	#if WITH_APEX
		void* APEXFrameworkHandle = nullptr;
		void* APEX_LegacyHandle = nullptr;
		#if WITH_APEX_CLOTHING
			void* APEX_ClothingHandle = nullptr;
		#endif  //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif
#if PLATFORM_WINDOWS
	FString PhysXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX3/");
	FString APEXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX3/");
	FString SharedBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX3/");

#if _MSC_VER >= 1900
	FString VSDirectory(TEXT("VS2015/"));
#else
#error "Unrecognized Visual Studio version."
#endif

#if PLATFORM_64BITS
	FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win64/") + VSDirectory);
	FString RootAPEXPath(APEXBinariesRoot + TEXT("Win64/") + VSDirectory);
	FString RootSharedPath(SharedBinariesRoot + TEXT("Win64/") + VSDirectory);
	FString ArchName(TEXT("_x64"));
	FString ArchBits(TEXT("64"));
#else
	FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win32/") + VSDirectory);
	FString RootAPEXPath(APEXBinariesRoot + TEXT("Win32/") + VSDirectory);
	FString RootSharedPath(SharedBinariesRoot + TEXT("Win32/") + VSDirectory);
	FString ArchName(TEXT("_x86"));
	FString ArchBits(TEXT("32"));
#endif

#ifdef UE_PHYSX_SUFFIX
	FString PhysXSuffix(TEXT(PREPROCESSOR_TO_STRING(UE_PHYSX_SUFFIX)) + ArchName + TEXT(".dll"));
#else
	FString PhysXSuffix(ArchName + TEXT(".dll"));
#endif

#ifdef UE_APEX_SUFFIX
	FString APEXSuffix(TEXT(PREPROCESSOR_TO_STRING(UE_APEX_SUFFIX)) + ArchName + TEXT(".dll"));
#else
	FString APEXSuffix(ArchName + TEXT(".dll"));
#endif
#elif PLATFORM_MAC
	FString PhysXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX3/Mac/");
#ifdef UE_PHYSX_SUFFIX
	FString PhysXSuffix = FString(TEXT(PREPROCESSOR_TO_STRING(UE_PHYSX_SUFFIX))) + TEXT(".dylib");
#else
	FString PhysXSuffix(TEXT(".dylib"));
#endif

#ifdef UE_APEX_SUFFIX
	FString APEXSuffix = FString(TEXT(PREPROCESSOR_TO_STRING(UE_APEX_SUFFIX))) + TEXT(".dylib");
#else
	FString APEXSuffix(TEXT(".dylib"));
#endif
#endif

void* LoadPhysicsLibrary(const FString& Path)
{
	void* Handle = FPlatformProcess::GetDllHandle(*Path);
	if(!Handle)
	{
		// Spin a few times and reattempt the load in-case the file is temporarily locked
		for(int32 RetryCount = 0; RetryCount < NumModuleLoadRetries; ++RetryCount)
		{
			FPlatformProcess::Sleep(ModuleReloadDelay);

			Handle = FPlatformProcess::GetDllHandle(*Path);

			if(Handle)
			{
				break;
			}
		}

		if(!Handle)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

			bool bExists = PlatformFile.FileExists(*Path);
			int64 ModuleFileSize = PlatformFile.FileSize(*Path);

			bool bCouldRead = false;

			TUniquePtr<IFileHandle> ModuleFileHandle(PlatformFile.OpenRead(*Path));
			if(ModuleFileHandle.IsValid())
			{
				bCouldRead = true;
			}

			UE_LOG(LogPhysics, Warning, TEXT("Failed to load module '%s'"), *Path);
			UE_LOG(LogPhysics, Warning, TEXT("\tExists: %s"), bExists ? TEXT("true") : TEXT("false"));
			UE_LOG(LogPhysics, Warning, TEXT("\tFileSize: %d"), ModuleFileSize);
			UE_LOG(LogPhysics, Warning, TEXT("\tAble to read: %s"), bCouldRead ? TEXT("true") : TEXT("false"));

			if(!bExists)
			{
				// No library
				UE_LOG(LogPhysics, Warning, TEXT("\tLibrary does not exist."));
			}
			else if(!bCouldRead)
			{
				// No read access to library
				UE_LOG(LogPhysics, Warning, TEXT("\tLibrary exists, but read access could not be gained. It is possible the user does not have read permission for this file."));
			}
		}
	}
	return Handle;
}

#if WITH_APEX
ENGINE_API void* LoadAPEXModule(const FString& Path)
{
#if PLATFORM_WINDOWS
	return LoadPhysicsLibrary(RootAPEXPath + Path + APEXSuffix);
#elif PLATFORM_MAC
	const FString APEX_HandleLibName = FString::Printf(TEXT("%slib%s%s"), *PhysXBinariesRoot, *Path, *APEXSuffix);
	return LoadPhysicsLibrary(APEX_HandleLibName);
#endif
	return nullptr;
}
#endif

/**
 *	Load the required modules for PhysX
 */
ENGINE_API bool LoadPhysXModules(bool bLoadCookingModule)
{
	bool bHasToolsExtensions = false;
#if PLATFORM_WINDOWS
	PxFoundationHandle = LoadPhysicsLibrary(RootSharedPath + "PxFoundation" + PhysXSuffix);
	PhysX3CommonHandle = LoadPhysicsLibrary(RootPhysXPath + "PhysX3Common" + PhysXSuffix);
	const FString nvToolsExtPath = RootPhysXPath + "nvToolsExt" + ArchBits + "_1.dll";
	bHasToolsExtensions = IFileManager::Get().FileExists(*nvToolsExtPath);
	if (bHasToolsExtensions)
	{
		nvToolsExtHandle = LoadPhysicsLibrary(nvToolsExtPath);
	}
	PxPvdSDKHandle = LoadPhysicsLibrary(RootSharedPath + "PxPvdSDK" + PhysXSuffix);
	PhysX3Handle = LoadPhysicsLibrary(RootPhysXPath + "PhysX3" + PhysXSuffix);

	if(bLoadCookingModule)
	{
		PhysX3CookingHandle = LoadPhysicsLibrary(RootPhysXPath + "PhysX3Cooking" + PhysXSuffix);
	}

	#if WITH_APEX
		APEXFrameworkHandle = LoadPhysicsLibrary(RootAPEXPath + "APEXFramework" + APEXSuffix);
		#if WITH_APEX_LEGACY
			APEX_LegacyHandle = LoadPhysicsLibrary(RootAPEXPath + "APEX_Legacy" + APEXSuffix);
		#endif //WITH_APEX_LEGACY
		#if WITH_APEX_CLOTHING
			APEX_ClothingHandle = LoadPhysicsLibrary(RootAPEXPath + "APEX_Clothing" + APEXSuffix);
		#endif //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#elif PLATFORM_MAC
	const FString PxFoundationLibName = FString::Printf(TEXT("%slibPxFoundation%s"), *PhysXBinariesRoot, *PhysXSuffix);
	PxFoundationHandle = LoadPhysicsLibrary(PxFoundationLibName);

	const FString PhysX3CommonLibName = FString::Printf(TEXT("%slibPhysX3Common%s"), *PhysXBinariesRoot, *PhysXSuffix);
	PhysX3CommonHandle = LoadPhysicsLibrary(PhysX3CommonLibName);

	const FString PxPvdSDKLibName = FString::Printf(TEXT("%slibPxPvdSDK%s"), *PhysXBinariesRoot, *PhysXSuffix);
	PxPvdSDKHandle = LoadPhysicsLibrary(PxPvdSDKLibName);

	const FString PhysX3LibName = FString::Printf(TEXT("%slibPhysX3%s"), *PhysXBinariesRoot, *PhysXSuffix);
	PhysX3Handle = LoadPhysicsLibrary(PhysX3LibName);

	if(bLoadCookingModule)
	{
		const FString PhysX3CookinLibName = FString::Printf(TEXT("%slibPhysX3Cooking%s"), *PhysXBinariesRoot, *PhysXSuffix);
		PhysX3CookingHandle = LoadPhysicsLibrary(PhysX3CookinLibName);
	}

	#if WITH_APEX
		const FString APEXFrameworkLibName = FString::Printf(TEXT("%slibAPEXFramework%s"), *PhysXBinariesRoot, *APEXSuffix);
		APEXFrameworkHandle = LoadPhysicsLibrary(APEXFrameworkLibName);
		#if WITH_APEX_LEGACY
			const FString APEX_LegacyHandleLibName = FString::Printf(TEXT("%slibAPEX_Legacy%s"), *PhysXBinariesRoot, *APEXSuffix);
			APEX_LegacyHandle = LoadPhysicsLibrary(APEX_LegacyHandleLibName);
		#endif //WITH_APEX_LEGACY
		#if WITH_APEX_CLOTHING
			const FString APEX_ClothingHandleLibName = FString::Printf(TEXT("%slibAPEX_Clothing%s"), *PhysXBinariesRoot, *APEXSuffix);
			APEX_ClothingHandle = LoadPhysicsLibrary(APEX_ClothingHandleLibName);
		#endif //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif	//PLATFORM_WINDOWS

	bool bSucceeded = true;

#if PLATFORM_WINDOWS || PLATFORM_MAC
	// Required modules (core PhysX)
	bSucceeded = bSucceeded && PxFoundationHandle;
	bSucceeded = bSucceeded && PhysX3CommonHandle;
	bSucceeded = bSucceeded && PxPvdSDKHandle;
	bSucceeded = bSucceeded && PhysX3Handle;
	// Tools extension if present
	bSucceeded = bSucceeded && (!bHasToolsExtensions || nvToolsExtHandle);
	// Cooking module if present
	bSucceeded = bSucceeded && (!bLoadCookingModule || PhysX3CookingHandle);
	// Apex if present
#if WITH_APEX
	bSucceeded = bSucceeded && APEXFrameworkHandle;
#if WITH_APEX_LEGACY
	bSucceeded = bSucceeded && APEX_LegacyHandle;
#endif //WITH_APEX_LEGACY
#if WITH_APEX_CLOTHING
	bSucceeded = bSucceeded && APEX_ClothingHandle;
#endif // WITH_APEX_CLOTHING
#endif // WITH_APEX
#endif // PLATFORM_WINDOWS || PLATFORM_MAC

	return bSucceeded;
}

/** 
 *	Unload the required modules for PhysX
 */
void UnloadPhysXModules()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	FPlatformProcess::FreeDllHandle(PxPvdSDKHandle);
	FPlatformProcess::FreeDllHandle(PhysX3Handle);
	if(PhysX3CookingHandle)
	{
		FPlatformProcess::FreeDllHandle(PhysX3CookingHandle);
	}
	FPlatformProcess::FreeDllHandle(PhysX3CommonHandle);
	FPlatformProcess::FreeDllHandle(PxFoundationHandle);
	#if WITH_APEX
		FPlatformProcess::FreeDllHandle(APEXFrameworkHandle);
		FPlatformProcess::FreeDllHandle(APEX_LegacyHandle);
		#if WITH_APEX_CLOTHING
			FPlatformProcess::FreeDllHandle(APEX_ClothingHandle);
		#endif //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif
}

#if WITH_APEX
ENGINE_API void UnloadAPEXModule(void* Handle)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if(Handle)
	{
		FPlatformProcess::FreeDllHandle(Handle);
	}
#endif
}
#endif
}
#endif // WITH_PHYSX
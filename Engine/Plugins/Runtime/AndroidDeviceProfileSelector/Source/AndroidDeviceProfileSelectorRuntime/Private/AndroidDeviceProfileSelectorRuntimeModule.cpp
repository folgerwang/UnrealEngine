// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.AndroidDeviceProfileSelector

#include "AndroidDeviceProfileSelectorRuntimeModule.h"
#include "AndroidDeviceProfileSelectorRuntime.h"
#include "Templates/Casts.h"
#include "Internationalization/Regex.h"
#include "Modules/ModuleManager.h"
#include "AndroidDeviceProfileSelectorRuntime.h"
#include "AndroidDeviceProfileSelector.h"
#include "AndroidJavaSurfaceViewDevices.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorRuntimeModule, AndroidDeviceProfileSelectorRuntime);

void FAndroidDeviceProfileSelectorRuntimeModule::StartupModule()
{
}

void FAndroidDeviceProfileSelectorRuntimeModule::ShutdownModule()
{
}

FString const FAndroidDeviceProfileSelectorRuntimeModule::GetRuntimeDeviceProfileName()
{
	static FString ProfileName;

#if PLATFORM_LUMIN
	if (ProfileName.IsEmpty())
	{
		// Fallback profiles in case we do not match any rules
		ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}
	}

	// @todo Lumin: when removing this, also remove Lumin from the .uplugin
	UE_LOG(LogAndroid, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	return ProfileName;
#endif
	
	if (ProfileName.IsEmpty())
	{
		// Fallback profiles in case we do not match any rules
		ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}

		FString GPUFamily = FAndroidMisc::GetGPUFamily();
		FString GLVersion = FAndroidMisc::GetGLVersion();

		FString VulkanVersion = FAndroidMisc::GetVulkanVersion();
		FString VulkanAvailable = FAndroidMisc::IsVulkanAvailable() ? TEXT("true") : TEXT("false");
		FString AndroidVersion = FAndroidMisc::GetAndroidVersion();
		FString DeviceMake = FAndroidMisc::GetDeviceMake();
		FString DeviceModel = FAndroidMisc::GetDeviceModel();
		FString DeviceBuildNumber = FAndroidMisc::GetDeviceBuildNumber();

#if !(PLATFORM_ANDROID_X86 || PLATFORM_ANDROID_X64)
		// Not running an Intel libUE4.so with Houdini library present means we're emulated
		bool bUsingHoudini = (access("/system/lib/libhoudini.so", F_OK) != -1);
#else
		bool bUsingHoudini = false;
#endif
		FString UsingHoudini = bUsingHoudini ? TEXT("true") : TEXT("false");

		UE_LOG(LogAndroid, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FAndroidDeviceProfileSelector::GetNumProfiles() );
		UE_LOG(LogAndroid, Log, TEXT("  Default profile: %s"), * ProfileName);
		UE_LOG(LogAndroid, Log, TEXT("  GpuFamily: %s"), *GPUFamily);
		UE_LOG(LogAndroid, Log, TEXT("  GlVersion: %s"), *GLVersion);
		UE_LOG(LogAndroid, Log, TEXT("  VulkanAvailable: %s"), *VulkanAvailable);
		UE_LOG(LogAndroid, Log, TEXT("  VulkanVersion: %s"), *VulkanVersion);
		UE_LOG(LogAndroid, Log, TEXT("  AndroidVersion: %s"), *AndroidVersion);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceMake: %s"), *DeviceMake);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceModel: %s"), *DeviceModel);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceBuildNumber: %s"), *DeviceBuildNumber);
		UE_LOG(LogAndroid, Log, TEXT("  UsingHoudini: %s"), *UsingHoudini);

		CheckForJavaSurfaceViewWorkaround(DeviceMake, DeviceModel);

		// Use override from ConfigRules if set
		FString* ConfigProfile = FAndroidMisc::GetConfigRulesVariable(TEXT("Profile"));
		if (ConfigProfile != nullptr)
		{
			ProfileName = *ConfigProfile;
			UE_LOG(LogAndroid, Log, TEXT("Using ConfigRules Profile: [%s]"), *ProfileName);
		}
		else
		{
			// Find a match with the DeviceProfiles matching rules
			ProfileName = FAndroidDeviceProfileSelector::FindMatchingProfile(GPUFamily, GLVersion, AndroidVersion, DeviceMake, DeviceModel, DeviceBuildNumber, VulkanAvailable, VulkanVersion, UsingHoudini, ProfileName);
			UE_LOG(LogAndroid, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
		}
	}

	return ProfileName;
}

void FAndroidDeviceProfileSelectorRuntimeModule::CheckForJavaSurfaceViewWorkaround(const FString& DeviceMake, const FString& DeviceModel) const
{
#if USE_ANDROID_JNI
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UAndroidJavaSurfaceViewDevices();
	Z_Construct_UClass_UAndroidJavaSurfaceViewDevices();

	const UAndroidJavaSurfaceViewDevices *const SurfaceViewDevices = Cast<UAndroidJavaSurfaceViewDevices>(UAndroidJavaSurfaceViewDevices::StaticClass()->GetDefaultObject());
	check(SurfaceViewDevices);

	for(const FJavaSurfaceViewDevice& Device : SurfaceViewDevices->SurfaceViewDevices)
	{
		if(Device.Manufacturer == DeviceMake && Device.Model == DeviceModel)
		{
			extern void AndroidThunkCpp_UseSurfaceViewWorkaround();
			AndroidThunkCpp_UseSurfaceViewWorkaround();
			return;
		}
	}
#endif
}

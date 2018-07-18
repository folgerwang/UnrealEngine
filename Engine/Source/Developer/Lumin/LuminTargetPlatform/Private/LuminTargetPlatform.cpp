// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LuminTargetPlatform.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"

/*=============================================================================
	LuminTargetPlatform.cpp: Implements the FLuminTargetPlatform class.
=============================================================================*/
#define LOCTEXT_NAMESPACE "FLuminTargetPlatformModule"

/* FLuminTargetPlatform structors
 *****************************************************************************/

FLuminTargetPlatform::FLuminTargetPlatform(bool bIsClient)
	: FAndroidTargetPlatform(bIsClient)
{
#if WITH_ENGINE
	// by using the FAndroidPlatformProperties, the PlatformInfo up in TTargetPlatformBase/FTargetPlatformBase would be Android
	this->PlatformInfo = PlatformInfo::FindPlatformInfo("Lumin");

	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *IniPlatformName());

	// Get the Target RHIs for this platform, we do not always want all those that are supported.
	TArray<FName> TargetedShaderFormats;
	GetAllTargetedShaderFormats(TargetedShaderFormats);

	// If we are targeting ES 2.0/3.1, we also must cook encoded HDR reflection captures
	static FName NAME_VULKAN_ES31(TEXT("SF_VULKAN_ES31_LUMIN"));
	static FName NAME_GLSL_ES2(TEXT("GLSL_ES2"));
	static FName NAME_GLSL_SM5(TEXT("GLSL_430"));
	bRequiresEncodedHDRReflectionCaptures = TargetedShaderFormats.Contains(NAME_VULKAN_ES31)
		|| TargetedShaderFormats.Contains(NAME_GLSL_ES2)
		|| TargetedShaderFormats.Contains(NAME_GLSL_SM5);
#endif
}


FLuminTargetPlatform::~FLuminTargetPlatform()
{ 
}



bool FLuminTargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	// @todo Lumin: implement me
	OutDocumentationPath = FString("Shared/Tutorials/SettingUpLuminTutorial");
	return true;
}

int32 FLuminTargetPlatform::CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/Android/GettingStarted");

	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready;
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}

	return bReadyToBuild;
}

bool FLuminTargetPlatform::SupportsMobileRendering() const
{
	bool bUseMobileRendering = true;
	EngineSettings.GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseMobileRendering"), bUseMobileRendering);
	return bUseMobileRendering;
}

bool FLuminTargetPlatform::SupportsDesktopRendering() const
{
	bool bUseMobileRendering = true;
	EngineSettings.GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseMobileRendering"), bUseMobileRendering);
	return bUseMobileRendering == false;
}

static bool LuminSupportsVulkan(const FConfigFile& EngineSettings)
{
	bool bSupportsVulkan = false;
	EngineSettings.GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseVulkan"), bSupportsVulkan);
	return bSupportsVulkan;
}

bool FLuminTargetPlatform::SupportsFeature(ETargetPlatformFeatures Feature) const
{
	switch (Feature)
	{
	case ETargetPlatformFeatures::Packaging:
		return true;

	case ETargetPlatformFeatures::LowQualityLightmaps:
	case ETargetPlatformFeatures::MobileRendering:
		return SupportsMobileRendering() || SupportsVulkan();

	case ETargetPlatformFeatures::HighQualityLightmaps:
	//#todo-rco: Enable when Vulkan supports it
	//case ETargetPlatformFeatures::Tessellation:
	case ETargetPlatformFeatures::DeferredRendering:
		return SupportsDesktopRendering();

	case ETargetPlatformFeatures::SoftwareOcclusion:
		return SupportsSoftwareOcclusion();

	default:
		break;
	}

	return TTargetPlatformBase<FAndroidPlatformProperties>::SupportsFeature(Feature);
}

FAndroidTargetDeviceRef FLuminTargetPlatform::CreateNewDevice(const FAndroidDeviceInfo &DeviceInfo)
{
	return MakeShareable(new FLuminTargetDevice(*this, DeviceInfo.SerialNumber, GetAndroidVariantName()));
}

void FLuminTargetPlatform::InitializeDeviceDetection()
{
	DeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection").GetAndroidDeviceDetection(TEXT("Lumin"));
	DeviceDetection->Initialize(TEXT("MLSDK"),
#if PLATFORM_WINDOWS
	TEXT("tools/mldb/mldb.exe"),
#else
	TEXT("tools/mldb/mldb"),
#endif
	TEXT("getprop"), false);

}

#if WITH_ENGINE

void FLuminTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	// @todo Lumin: re-use Android version? Make sure Android has VULKAN_SM5
	static FName NAME_GLSL_ES2(TEXT("GLSL_ES2"));
//	static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
//	static FName NAME_GLSL_SM4(TEXT("GLSL_150"));
	static FName NAME_GLSL_SM5(TEXT("GLSL_430"));
	static FName NAME_VULKAN_SM5_LUMIN(TEXT("SF_VULKAN_SM5_LUMIN"));
	static FName NAME_VULKAN_ES31_LUMIN(TEXT("SF_VULKAN_ES31_LUMIN"));

	if (SupportsMobileRendering())
	{
		OutFormats.AddUnique(NAME_GLSL_ES2);
		if (LuminSupportsVulkan(EngineSettings))
		{
			OutFormats.AddUnique(NAME_VULKAN_ES31_LUMIN);
		}
	}

	if (SupportsDesktopRendering())
	{
//		OutFormats.AddUnique(NAME_GLSL_310_ES_EXT);
//		OutFormats.AddUnique(NAME_GLSL_SM4);
		OutFormats.AddUnique(NAME_GLSL_SM5);

		if (LuminSupportsVulkan(EngineSettings))
		{
			OutFormats.AddUnique(NAME_VULKAN_SM5_LUMIN);
		}
	}
}


static FName FormatRemap[][2] =
{
	// Default format:				ASTC format:
	{ { FName(TEXT("DXT1")) },{ FName(TEXT("ASTC_RGB")) } },
	{ { FName(TEXT("DXT5")) },{ FName(TEXT("ASTC_RGBA")) } },
	{ { FName(TEXT("DXT5n")) },{ FName(TEXT("ASTC_NormalAG")) } },
	{ { FName(TEXT("BC5")) },{ FName(TEXT("ASTC_NormalRG")) } },
	{ { FName(TEXT("BC6H")) },{ FName(TEXT("ASTC_RGB")) } },
	{ { FName(TEXT("BC7")) },{ FName(TEXT("ASTC_RGBAuto")) } },
	{ { FName(TEXT("AutoDXT")) },{ FName(TEXT("ASTC_RGBAuto")) } },
};


void FLuminTargetPlatform::GetTextureFormats(const UTexture* InTexture, TArray<FName>& OutFormats) const
{
	check(InTexture);

	FName TextureFormatName = NAME_None;

	// forward rendering only needs one channel for shadow maps
	if (InTexture->LODGroup == TEXTUREGROUP_Shadowmap)
	{
		TextureFormatName = FName(TEXT("G8"));
	}

	// if we didn't assign anything specially, then use the defaults
	if (TextureFormatName == NAME_None)
	{
		TextureFormatName = GetDefaultTextureFormatName(this, InTexture, EngineSettings, false);
	}

	// perform any remapping away from defaults
	bool bFoundRemap = false;
	for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); ++RemapIndex)
	{
		if (TextureFormatName == FormatRemap[RemapIndex][0])
		{
			// we found a remapping
			bFoundRemap = true;
			OutFormats.AddUnique(FormatRemap[RemapIndex][1]);
		}
	}

	// if we didn't already remap above, add it now
	if (!bFoundRemap)
	{
		OutFormats.Add(TextureFormatName);
	}
}

void FLuminTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	GetAllDefaultTextureFormats(this, OutFormats, false);

	for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex++)
	{
		OutFormats.Remove(FormatRemap[RemapIndex][0]);
	}

	// include the formats we want
	for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex++)
	{
		OutFormats.AddUnique(FormatRemap[RemapIndex][1]);
	}
}

#endif //WITH_ENGINE


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.inl: Implements the FAndroidTargetPlatform class.
=============================================================================*/


/* FAndroidTargetPlatform structors
 *****************************************************************************/

#include "AndroidTargetPlatform.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Serialization/Archive.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Modules/ModuleManager.h"
#include "Misc/SecureHash.h"


#if WITH_ENGINE
#include "AudioCompressionSettings.h"
#endif

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatform"

class Error;
class FAndroidTargetDevice;
class FConfigCacheIni;
class FModuleManager;
class FScopeLock;
class FStaticMeshLODSettings;
class FTargetDeviceId;
class FTicker;
class IAndroidDeviceDetectionModule;
class UTexture;
class UTextureLODSettings;
struct FAndroidDeviceInfo;
enum class ETargetPlatformFeatures;
template<typename TPlatformProperties> class TTargetPlatformBase;



static FString GetLicensePath()
{
	auto &AndroidDeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection");
	IAndroidDeviceDetection* DeviceDetection = AndroidDeviceDetection.GetAndroidDeviceDetection();
	FString ADBPath = DeviceDetection->GetADBPath();

	if (!FPaths::FileExists(*ADBPath))
	{
		return TEXT("");
	}

	// strip off the adb.exe part
	FString PlatformToolsPath;
	FString Filename;
	FString Extension;
	FPaths::Split(ADBPath, PlatformToolsPath, Filename, Extension);

	// remove the platform-tools part and point to licenses
	FPaths::NormalizeDirectoryName(PlatformToolsPath);
	FString LicensePath = PlatformToolsPath + "/../licenses";
	FPaths::CollapseRelativeDirectories(LicensePath);

	return LicensePath;
}

#if WITH_ENGINE
static bool GetLicenseHash(FSHAHash& LicenseHash)
{
	bool bLicenseValid = false;

	// from Android SDK Tools 25.2.3
	FString LicenseFilename = FPaths::EngineDir() + TEXT("Source/ThirdParty/Android/package.xml");

	// Create file reader
	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*LicenseFilename));
	if (FileReader)
	{
		// Create buffer for file input
		uint32 BufferSize = FileReader->TotalSize();
		uint8* Buffer = (uint8*)FMemory::Malloc(BufferSize);
		FileReader->Serialize(Buffer, BufferSize);

		uint8 StartPattern[] = "<license id=\"android-sdk-license\" type=\"text\">";
		int32 StartPatternLength = strlen((char *)StartPattern);

		uint8* LicenseStart = Buffer;
		uint8* BufferEnd = Buffer + BufferSize - StartPatternLength;
		while (LicenseStart < BufferEnd)
		{
			if (!memcmp(LicenseStart, StartPattern, StartPatternLength))
			{
				break;
			}
			LicenseStart++;
		}

		if (LicenseStart < BufferEnd)
		{
			LicenseStart += StartPatternLength;

			uint8 EndPattern[] = "</license>";
			int32 EndPatternLength = strlen((char *)EndPattern);

			uint8* LicenseEnd = LicenseStart;
			BufferEnd = Buffer + BufferSize - EndPatternLength;
			while (LicenseEnd < BufferEnd)
			{
				if (!memcmp(LicenseEnd, EndPattern, EndPatternLength))
				{
					break;
				}
				LicenseEnd++;
			}

			if (LicenseEnd < BufferEnd)
			{
				int32 LicenseLength = LicenseEnd - LicenseStart;
				FSHA1::HashBuffer(LicenseStart, LicenseLength, LicenseHash.Hash);
				bLicenseValid = true;
			}
		}
		FMemory::Free(Buffer);
	}

	return bLicenseValid;
}
#endif

static bool HasLicense()
{
#if WITH_ENGINE
	FString LicensePath = GetLicensePath();

	if (LicensePath.IsEmpty())
	{
		return false;
	}

	// directory must exist
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LicensePath))
	{
		return false;
	}

	// license file must exist
	FString LicenseFilename = LicensePath + "/android-sdk-license";
	if (!PlatformFile.FileExists(*LicenseFilename))
	{
		return false;
	}

	FSHAHash LicenseHash;
	if (!GetLicenseHash(LicenseHash))
	{
		return false;
	}

	// contents must match hash of license text
	FString FileData = "";
	FFileHelper::LoadFileToString(FileData, *LicenseFilename);
	TArray<FString> lines;
	int32 lineCount = FileData.ParseIntoArray(lines, TEXT("\n"), true);

	FString LicenseString = LicenseHash.ToString().ToLower();
	for (FString &line : lines)
	{
		if (line.TrimStartAndEnd().Equals(LicenseString))
		{
			return true;
		}
	}
#endif

	// doesn't match
	return false;
}

FAndroidTargetPlatform::FAndroidTargetPlatform(bool bInIsClient )
	: bIsClient(bInIsClient)
	, DeviceDetection(nullptr)

{
	#if WITH_ENGINE
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *IniPlatformName());
			TextureLODSettings = nullptr; // These are registered by the device profile system.
		StaticMeshLODSettings.Initialize(EngineSettings);
	#endif

	TickDelegate = FTickerDelegate::CreateRaw(this, &FAndroidTargetPlatform::HandleTicker);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 4.0f);
}


FAndroidTargetPlatform::~FAndroidTargetPlatform()
{ 
	 FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}

FAndroidTargetDevicePtr FAndroidTargetPlatform::CreateTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant) const
{
	return MakeShareable(new FAndroidTargetDevice(InTargetPlatform, InSerialNumber, InAndroidVariant));
}

bool FAndroidTargetPlatform::SupportsES2() const
{
	// default to support ES2
	bool bBuildForES2 = true;
#if WITH_ENGINE
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES2"), bBuildForES2, GEngineIni);
#endif
	return bBuildForES2;
}

bool FAndroidTargetPlatform::SupportsES31() const
{
	// default no support for ES31
	bool bBuildForES31 = false;
#if WITH_ENGINE
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bBuildForES31, GEngineIni);
#endif
	return bBuildForES31;
}

bool FAndroidTargetPlatform::SupportsAEP() const
{
	return false;
}

bool FAndroidTargetPlatform::SupportsVulkan() const
{
	// default to not supporting Vulkan
	bool bSupportsVulkan = false;
#if WITH_ENGINE
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bSupportsVulkan, GEngineIni);
#endif
	return bSupportsVulkan;
}

bool FAndroidTargetPlatform::SupportsSoftwareOcclusion() const
{
	static auto* CVarMobileAllowSoftwareOcclusion = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowSoftwareOcclusion"));
	return CVarMobileAllowSoftwareOcclusion->GetValueOnAnyThread() != 0;
}

/* ITargetPlatform overrides
 *****************************************************************************/

void FAndroidTargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}

ECompressionFlags FAndroidTargetPlatform::GetBaseCompressionMethod( ) const
{
	return COMPRESS_ZLIB;
}

ITargetDevicePtr FAndroidTargetPlatform::GetDefaultDevice( ) const
{
	// return the first device in the list
	if (Devices.Num() > 0)
	{
		auto Iter = Devices.CreateConstIterator();
		if (Iter)
		{
			return Iter.Value();
		}
	}

	return nullptr;
}

ITargetDevicePtr FAndroidTargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	if (DeviceId.GetPlatformName() == PlatformName())
	{
		return Devices.FindRef(DeviceId.GetDeviceName());
	}

	return nullptr;
}

bool FAndroidTargetPlatform::IsRunningPlatform( ) const
{
	return false; // This platform never runs the target platform framework
}


bool FAndroidTargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	OutDocumentationPath = FString("Shared/Tutorials/SettingUpAndroidTutorial");
	return true;
}

int32 FAndroidTargetPlatform::CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/Android/GettingStarted");

	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready;
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}

	bool bEnableGradle;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bEnableGradle"), bEnableGradle, GEngineIni);

	if (bEnableGradle)
	{
		// need to check license was accepted
		if (!HasLicense())
		{
			OutTutorialPath.Empty();
			CustomizedLogMessage = LOCTEXT("AndroidLicenseNotAcceptedMessageDetail", "SDK License must be accepted in the Android project settings to deploy your app to the device.");
			bReadyToBuild |= ETargetPlatformReadyStatus::LicenseNotAccepted;
		}
	}

	return bReadyToBuild;
}

bool FAndroidTargetPlatform::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
		case ETargetPlatformFeatures::Packaging:
		case ETargetPlatformFeatures::DeviceOutputLog:
			return true;
			
		case ETargetPlatformFeatures::LowQualityLightmaps:
		case ETargetPlatformFeatures::MobileRendering:
			return SupportsES31() || SupportsES2() || SupportsVulkan();
			
		case ETargetPlatformFeatures::HighQualityLightmaps:
		case ETargetPlatformFeatures::Tessellation:
		case ETargetPlatformFeatures::DeferredRendering:
			return SupportsAEP();
			
		case ETargetPlatformFeatures::SoftwareOcclusion:
			return SupportsSoftwareOcclusion();
			
		default:
			break;
	}
	
	return TTargetPlatformBase<FAndroidPlatformProperties>::SupportsFeature(Feature);
}


#if WITH_ENGINE

void FAndroidTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_OPENGL_ES2(TEXT("GLSL_ES2"));
	static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
	static FName NAME_SF_VULKAN_ES31_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID_NOUB"));
	static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));

	if (SupportsVulkan())
	{
		OutFormats.AddUnique(NAME_SF_VULKAN_ES31_ANDROID);
	}

	if (SupportsES2())
	{
		OutFormats.AddUnique(NAME_OPENGL_ES2);
	}

	if (SupportsES31())
	{
		OutFormats.AddUnique(NAME_GLSL_ES3_1_ANDROID);
	}

	if (SupportsAEP())
	{
		OutFormats.AddUnique(NAME_GLSL_310_ES_EXT);
	}
}

void FAndroidTargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


const FStaticMeshLODSettings& FAndroidTargetPlatform::GetStaticMeshLODSettings( ) const
{
	return StaticMeshLODSettings;
}


void FAndroidTargetPlatform::GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const
{
	check(InTexture);

	// The order we add texture formats to OutFormats is important. When multiple formats are cooked
	// and supported by the device, the first supported format listed will be used. 
	// eg, ETC1/uncompressed should always be last

	bool bNoCompression = InTexture->CompressionNone				// Code wants the texture uncompressed.
		|| (InTexture->LODGroup == TEXTUREGROUP_ColorLookupTable)	// Textures in certain LOD groups should remain uncompressed.
		|| (InTexture->LODGroup == TEXTUREGROUP_Bokeh)
		|| (InTexture->CompressionSettings == TC_EditorIcon)
		|| (InTexture->Source.GetSizeX() < 4)						// Don't compress textures smaller than the DXT block size.
		|| (InTexture->Source.GetSizeY() < 4)
		|| (InTexture->Source.GetSizeX() % 4 != 0)
		|| (InTexture->Source.GetSizeY() % 4 != 0);

	bool bIsNonPOT = false;
#if WITH_EDITORONLY_DATA
	// is this texture not a power of 2?
	bIsNonPOT = !InTexture->Source.IsPowerOfTwo();
#endif

	// Determine the pixel format of the compressed texture.
	if (InTexture->LODGroup == TEXTUREGROUP_Shadowmap)
	{
		// forward rendering only needs one channel for shadow maps
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (bNoCompression && InTexture->HasHDRSource())
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (bNoCompression)
	{
		OutFormats.Add(AndroidTexFormat::NameBGRA8);
	}
	else if (InTexture->CompressionSettings == TC_HDR
		|| InTexture->CompressionSettings == TC_HDR_Compressed)
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (InTexture->CompressionSettings == TC_Normalmap)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC2_RGB, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1a, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
	else if (InTexture->CompressionSettings == TC_Displacementmap)
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (InTexture->CompressionSettings == TC_VectorDisplacementmap)
	{
		OutFormats.Add(AndroidTexFormat::NameBGRA8);
	}
	else if (InTexture->CompressionSettings == TC_Grayscale)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->CompressionSettings == TC_Alpha)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->CompressionSettings == TC_DistanceFieldFont)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->bForcePVRTC4
		|| InTexture->CompressionSettings == TC_BC7)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1a, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
	else if (InTexture->CompressionNoAlpha)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT1, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGB, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC2_RGB, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1a, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC1, OutFormats, bIsNonPOT);
	}
	else if (InTexture->bDitherMipMapAlpha)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1a, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
	else
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoPVRTC, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoDXT, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoATC, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1a, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
}



void FAndroidTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{


	OutFormats.Add(AndroidTexFormat::NameG8);
	OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	OutFormats.Add(AndroidTexFormat::NameBGRA8);
	OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	OutFormats.Add(AndroidTexFormat::NameBGRA8);
	OutFormats.Add(AndroidTexFormat::NameG8);
	OutFormats.Add(AndroidTexFormat::NameG8);
	OutFormats.Add(AndroidTexFormat::NameG8);

	auto AddAllTextureFormatIfSupports = [=, &OutFormats](bool bIsNonPOT) 
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoPVRTC, OutFormats, bIsNonPOT); 
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoDXT, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT1, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGB, OutFormats, bIsNonPOT); 
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT); 
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1a, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoATC, OutFormats, bIsNonPOT);
	};

	AddAllTextureFormatIfSupports(true);
	AddAllTextureFormatIfSupports(false);

}


void FAndroidTargetPlatform::GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const
{
	if (SupportsAEP())
	{
		// use Full HDR with AEP
		OutFormats.Add(FName(TEXT("FullHDR")));
	}
	
	// always emit encoded
	OutFormats.Add(FName(TEXT("EncodedHDR")));
}


const UTextureLODSettings& FAndroidTargetPlatform::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}


FName FAndroidTargetPlatform::GetWaveFormat( const class USoundWave* Wave ) const
{
	static bool formatRead = false;
	static FName NAME_FORMAT;

	if (!formatRead)
	{
		formatRead = true;

		FString audioSetting;
		if (!GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("AndroidAudio"), audioSetting, GEngineIni))
		{
			audioSetting = TEXT("DEFAULT");
		}

#if WITH_OGGVORBIS
		if (audioSetting == TEXT("OGG") || audioSetting == TEXT("Default"))
		{
			static FName NAME_OGG(TEXT("OGG"));
			NAME_FORMAT = NAME_OGG;
		}
#else
		if (audioSetting == TEXT("OGG"))
		{
			UE_LOG(LogAudio, Error, TEXT("Attemped to select Ogg Vorbis encoding when the cooker is built without Ogg Vorbis support."));
		}
#endif
		else
		{
	
			// Otherwise return ADPCM as it'll either be option '2' or 'default' depending on WITH_OGGVORBIS config
			static FName NAME_ADPCM(TEXT("ADPCM"));
			NAME_FORMAT = NAME_ADPCM;
		}
	}
	return NAME_FORMAT;
}


void FAndroidTargetPlatform::GetAllWaveFormats(TArray<FName>& OutFormats) const
{
	static FName NAME_OGG(TEXT("OGG"));
	static FName NAME_ADPCM(TEXT("ADPCM"));

	OutFormats.Add(NAME_OGG); 
	OutFormats.Add(NAME_ADPCM);
}

namespace
{
	void CachePlatformAudioCookOverrides(FPlatformAudioCookOverrides& OutOverrides)
	{
		const TCHAR* CategoryName = TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");

		GConfig->GetBool(CategoryName, TEXT("bResampleForDevice"), OutOverrides.bResampleForDevice, GEngineIni);

		GConfig->GetFloat(CategoryName, TEXT("CompressionQualityModifier"), OutOverrides.CompressionQualityModifier, GEngineIni);

		//Cache sample rate map.
		OutOverrides.PlatformSampleRates.Reset();

		float RetrievedSampleRate = -1.0f;

		GConfig->GetFloat(CategoryName, TEXT("MaxSampleRate"), RetrievedSampleRate, GEngineIni);
		OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Max, RetrievedSampleRate);

		RetrievedSampleRate = -1.0f;

		GConfig->GetFloat(CategoryName, TEXT("HighSampleRate"), RetrievedSampleRate, GEngineIni);
		OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::High, RetrievedSampleRate);

		RetrievedSampleRate = -1.0f;

		GConfig->GetFloat(CategoryName, TEXT("MedSampleRate"), RetrievedSampleRate, GEngineIni);
		OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Medium, RetrievedSampleRate);

		RetrievedSampleRate = -1.0f;

		GConfig->GetFloat(CategoryName, TEXT("LowSampleRate"), RetrievedSampleRate, GEngineIni);
		OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Low, RetrievedSampleRate);

		RetrievedSampleRate = -1.0f;

		GConfig->GetFloat(CategoryName, TEXT("MinSampleRate"), RetrievedSampleRate, GEngineIni);
		OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Min, RetrievedSampleRate);
	}
}

FPlatformAudioCookOverrides* FAndroidTargetPlatform::GetAudioCompressionSettings() const
{
	static FPlatformAudioCookOverrides Settings;

#if !WITH_EDITOR
	static bool bCachedPlatformSettings = false;

	if (!bCachedPlatformSettings)
	{
		CachePlatformAudioCookOverrides(Settings);
		bCachedPlatformSettings = true;
	}
#else
	CachePlatformAudioCookOverrides(Settings);
#endif

	return &Settings;
}

#endif //WITH_ENGINE

bool FAndroidTargetPlatform::SupportsVariants() const
{
	return true;
}

FText FAndroidTargetPlatform::GetVariantTitle() const
{
	return LOCTEXT("AndroidVariantTitle", "Texture Format");
}

/* FAndroidTargetPlatform implementation
 *****************************************************************************/

void FAndroidTargetPlatform::AddTextureFormatIfSupports( FName Format, TArray<FName>& OutFormats, bool bIsCompressedNonPOT ) const
{
	if (SupportsTextureFormat(Format))
	{
		if (bIsCompressedNonPOT && SupportsCompressedNonPOT() == false)
		{
			OutFormats.Add(AndroidTexFormat::NamePOTERROR);
		}
		else
		{
			OutFormats.Add(Format);
		}
	}
}

void FAndroidTargetPlatform::InitializeDeviceDetection()
{
	DeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection").GetAndroidDeviceDetection();
	DeviceDetection->Initialize(TEXT("ANDROID_HOME"),
#if PLATFORM_WINDOWS
		TEXT("platform-tools\\adb.exe"),
#else
		TEXT("platform-tools/adb"),
#endif
		TEXT("shell getprop"), true);
}
/* FAndroidTargetPlatform callbacks
 *****************************************************************************/

bool FAndroidTargetPlatform::HandleTicker( float DeltaTime )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAndroidTargetPlatform_HandleTicker);

	if (DeviceDetection == nullptr)
	{
		InitializeDeviceDetection();
		checkf(DeviceDetection != nullptr, TEXT("A target platform didn't create a device detection object in InitializeDeviceDetection()!"));
	}

	TArray<FString> ConnectedDeviceIds;

	{
		FScopeLock ScopeLock(DeviceDetection->GetDeviceMapLock());

		auto DeviceIt = DeviceDetection->GetDeviceMap().CreateConstIterator();
		
		for (; DeviceIt; ++DeviceIt)
		{
			ConnectedDeviceIds.Add(DeviceIt.Key());

			const FAndroidDeviceInfo& DeviceInfo = DeviceIt.Value();

			// see if this device is already known
			if (Devices.Contains(DeviceIt.Key()))
			{
				FAndroidTargetDevicePtr TestDevice = Devices[DeviceIt.Key()];

				// ignore if authorization didn't change
				if (DeviceInfo.bAuthorizedDevice == TestDevice->IsAuthorized())
				{
					continue;
				}

				// remove it to add again
				TestDevice->SetConnected(false);
				Devices.Remove(DeviceIt.Key());

				DeviceLostEvent.Broadcast(TestDevice.ToSharedRef());
			}

			// check if this platform is supported by the extensions and version
			if (!SupportedByExtensionsString(DeviceInfo.GLESExtensions, DeviceInfo.GLESVersion))
			{
				continue;
			}

			// create target device
			FAndroidTargetDevicePtr& Device = Devices.Add(DeviceInfo.SerialNumber);

			Device = CreateTargetDevice(*this, DeviceInfo.SerialNumber, GetAndroidVariantName());

			Device->SetConnected(true);
			Device->SetModel(DeviceInfo.Model);
			Device->SetDeviceName(DeviceInfo.DeviceName);
			Device->SetAuthorized(DeviceInfo.bAuthorizedDevice);
			Device->SetVersions(DeviceInfo.SDKVersion, DeviceInfo.HumanAndroidVersion);

			DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		}
	}

	// remove disconnected devices
	for (auto Iter = Devices.CreateIterator(); Iter; ++Iter)
	{
		if (!ConnectedDeviceIds.Contains(Iter.Key()))
		{
			FAndroidTargetDevicePtr RemovedDevice = Iter.Value();
			RemovedDevice->SetConnected(false);

			Iter.RemoveCurrent();

			DeviceLostEvent.Broadcast(RemovedDevice.ToSharedRef());
		}
	}

	return true;
}

FAndroidTargetDeviceRef FAndroidTargetPlatform::CreateNewDevice(const FAndroidDeviceInfo &DeviceInfo)
{
	return MakeShareable(new FAndroidTargetDevice(*this, DeviceInfo.SerialNumber, GetAndroidVariantName()));
}

#undef LOCTEXT_NAMESPACE

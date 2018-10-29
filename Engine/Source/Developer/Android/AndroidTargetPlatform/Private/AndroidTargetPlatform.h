// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.h: Declares the FAndroidTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"
#include "Android/AndroidProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "AndroidTargetDevice.h"

#if WITH_ENGINE
#include "Internationalization/Text.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatform" 

class FTargetDeviceId;
class IAndroidDeviceDetection;
class ITargetPlatform;
class UTextureLODSettings;
enum class ETargetPlatformFeatures;
template<typename TPlatformProperties> class TTargetPlatformBase;

template< typename InElementType, typename KeyFuncs , typename Allocator > class TSet;
template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMap;
template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMultiMap;
template<typename TPlatformProperties> class TTargetPlatformBase;

/**
 * Defines supported texture format names.
 */
namespace AndroidTexFormat
{
	// Compressed Texture Formats
	static FName NamePVRTC2(TEXT("PVRTC2"));
	static FName NamePVRTC4(TEXT("PVRTC4"));
	static FName NameAutoPVRTC(TEXT("AutoPVRTC"));
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameATC_RGB(TEXT("ATC_RGB"));
	static FName NameATC_RGBA_E(TEXT("ATC_RGBA_E"));		// explicit alpha
	static FName NameATC_RGBA_I(TEXT("ATC_RGBA_I"));		// interpolated alpha
	static FName NameAutoATC(TEXT("AutoATC"));
	static FName NameETC1(TEXT("ETC1"));
	static FName NameAutoETC1(TEXT("AutoETC1"));			// ETC1 or uncompressed RGBA, if alpha channel required
	static FName NameAutoETC1a(TEXT("AutoETC1a"));
	static FName NameETC2_RGB(TEXT("ETC2_RGB"));
	static FName NameETC2_RGBA(TEXT("ETC2_RGBA"));
	static FName NameAutoETC2(TEXT("AutoETC2"));
	static FName NameASTC_4x4(TEXT("ASTC_4x4"));
	static FName NameASTC_6x6(TEXT("ASTC_6x6"));
	static FName NameASTC_8x8(TEXT("ASTC_8x8"));
	static FName NameASTC_10x10(TEXT("ASTC_10x10"));
	static FName NameASTC_12x12(TEXT("ASTC_12x12"));
	static FName NameAutoASTC(TEXT("AutoASTC"));

	// Uncompressed Texture Formats
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameG8(TEXT("G8"));
	static FName NameVU8(TEXT("VU8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));

	// Error "formats" (uncompressed)
	static FName NamePOTERROR(TEXT("POTERROR"));
}


/**
 * FAndroidTargetPlatform, abstraction for cooking Android platforms
 */
class ANDROIDTARGETPLATFORM_API FAndroidTargetPlatform : public TTargetPlatformBase<FAndroidPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FAndroidTargetPlatform(bool bInIsClient);

	/**
	 * Destructor
	 */
	virtual ~FAndroidTargetPlatform();

public:

	/**
	 * Gets the name of the Android platform variant, i.e. ATC, DXT, PVRTC, etc.
	 *
	 * @param Variant name.
	 */
	virtual FString GetAndroidVariantName() const
	{
		return FString();	
	}


	virtual FString IniPlatformName() const override
	{
		return "Android";
	}

	virtual FString PlatformName() const override
	{
		FString PlatformName = TEXT("Android");
		FString Variant = GetAndroidVariantName();
		if (Variant.Len() > 0)
		{
			PlatformName += FString(TEXT("_")) + Variant;
		}
		if (bIsClient)
		{
			PlatformName += TEXT("Client");
		}

		return PlatformName;
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsRunningPlatform( ) const override;

	virtual bool IsServerOnly( ) const override
	{
		return false;
	}

	virtual bool IsClientOnly() const override
	{
		return bIsClient;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override;

	virtual bool SupportsTextureFormat( FName Format ) const 
	{
		// By default we support all texture formats.
		return true;
	}

	virtual bool SupportsCompressedNonPOT( ) const
	{
		// most formats do support non-POT compressed textures
		return true;
	}

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override;
	
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override;
	virtual void GetAllWaveFormats( TArray<FName>& OutFormats) const override;

	virtual FPlatformAudioCookOverrides* GetAudioCompressionSettings() const override;

#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override;

	virtual FText GetVariantTitle() const override;

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
		InBoolKeys.Add(TEXT("bBuildForArmV7")); InBoolKeys.Add(TEXT("bBuildForArm64")); InBoolKeys.Add(TEXT("bBuildForX86"));
		InBoolKeys.Add(TEXT("bBuildForX8664")); InBoolKeys.Add(TEXT("bBuildForES2"));
		InBoolKeys.Add(TEXT("bBuildForES31")); InBoolKeys.Add(TEXT("bBuildWithHiddenSymbolVisibility"));
		InBoolKeys.Add(TEXT("bUseNEONForArmV7")); InBoolKeys.Add(TEXT("bSaveSymbols"));
		InStringKeys.Add(TEXT("NDKAPILevel"));
	}

	DECLARE_DERIVED_EVENT(FAndroidTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FAndroidTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

	virtual void InitializeDeviceDetection();
	
protected:

	/**
	 * Adds the specified texture format to the OutFormats if this android target platforms supports it.
	 *
	 * @param Format - The format to add.
	 * @param OutFormats - The collection of formats to add to.
	 * @param bIsCompressedNonPOT - If this is true, the texture wants to be compressed but is not a power of 2
	 */
	void AddTextureFormatIfSupports( FName Format, TArray<FName>& OutFormats, bool bIsCompressedNonPOT=false ) const;

	/**
	 * Return true if this device has a supported set of extensions for this platform.
	 *
	 * @param Extensions - The GL extensions string.
	 * @param GLESVersion - The GLES version reported by this device.
	 */
	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const
	{
		return true;
	}

	virtual FAndroidTargetDevicePtr CreateTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant) const;

	// query for rene3ring mode support
	bool SupportsES2() const;
	bool SupportsES31() const;
	bool SupportsAEP() const;
	bool SupportsVulkan() const;
	bool SupportsSoftwareOcclusion() const;

#if WITH_ENGINE
	// Holds the Engine INI settings (for quick access).
	FConfigFile EngineSettings;
#endif //WITH_ENGINE

protected:

	// Handles when the ticker fires.
	bool HandleTicker( float DeltaTime );

	virtual FAndroidTargetDeviceRef CreateNewDevice(const FAndroidDeviceInfo &DeviceInfo);

	// true if this is a client TP
	bool bIsClient;

	// Holds a map of valid devices.
	TMap<FString, FAndroidTargetDevicePtr> Devices;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FDelegateHandle TickDelegateHandle;

	// Pointer to the device detection handler that grabs device ids in another thread
	IAndroidDeviceDetection* DeviceDetection;

#if WITH_ENGINE
	// Holds a cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	ITargetDevicePtr DefaultDevice;
#endif //WITH_ENGINE

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};


//#include "AndroidTargetPlatform.inl"


class FAndroid_DXTTargetPlatform : public FAndroidTargetPlatform
{
public:
	FAndroid_DXTTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_DXT");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("DXT");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_DXT", "Android (DXT)");
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameDXT1 ||
			Format == AndroidTexFormat::NameDXT5 ||
			Format == AndroidTexFormat::NameAutoDXT)
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_DXT_ShortName", "DXT");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_DXT"), Priority, GEngineIni) ?
			Priority : 0.6f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};



class FAndroid_ATCTargetPlatform : public FAndroidTargetPlatform
{
public:
	FAndroid_ATCTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_ATC");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("ATC");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_ATC", "Android (ATC)");
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameATC_RGB ||
			Format == AndroidTexFormat::NameATC_RGBA_I ||
			Format == AndroidTexFormat::NameAutoATC)
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture")));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ATC_ShortName", "ATC");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ATC"), Priority, GEngineIni) ?
			Priority : 0.5f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};



class FAndroid_ASTCTargetPlatform : public FAndroidTargetPlatform
{
public:
	FAndroid_ASTCTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient) 
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_ASTC");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("ASTC");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_ASTC", "Android (ASTC)");
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameASTC_4x4 ||
			Format == AndroidTexFormat::NameASTC_6x6 ||
			Format == AndroidTexFormat::NameASTC_8x8 ||
			Format == AndroidTexFormat::NameASTC_10x10 ||
			Format == AndroidTexFormat::NameASTC_12x12 ||
			Format == AndroidTexFormat::NameAutoASTC)
		{
			return true;
		}
		return false;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray<FName>& OutFormats) const
	{
		check(Texture);

		// we remap some of the defaults (with PVRTC and ASTC formats)
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

		FName TextureFormatName = NAME_None;

		// forward rendering only needs one channel for shadow maps
		if (Texture->LODGroup == TEXTUREGROUP_Shadowmap)
		{
			TextureFormatName = FName(TEXT("G8"));
		}

		// if we didn't assign anything specially, then use the defaults
		if (TextureFormatName == NAME_None)
		{
			TextureFormatName = GetDefaultTextureFormatName(this, Texture, EngineSettings, false, false, 1);
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


	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		// we remap some of the defaults (with PVRTC and ASTC formats)
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

		GetAllDefaultTextureFormats(this, OutFormats, false);

		for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); ++RemapIndex)
		{
			OutFormats.Remove(FormatRemap[RemapIndex][0]);
			OutFormats.AddUnique(FormatRemap[RemapIndex][1]);
		}
	}

#endif

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ASTC_ShortName", "ASTC");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ASTC"), Priority, GEngineIni) ?
			Priority : 0.9f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};



class FAndroid_PVRTCTargetPlatform : public FAndroidTargetPlatform
{
public:
	FAndroid_PVRTCTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_PVRTC");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("PVRTC");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_PVRTC", "Android (PVRTC)");
	}

	virtual bool SupportsCompressedNonPOT() const override
	{
		return false;
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NamePVRTC2 ||
			Format == AndroidTexFormat::NamePVRTC4 ||
			Format == AndroidTexFormat::NameAutoPVRTC)
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc"));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_PVRTC_ShortName", "PVRTC");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_PVRTC"), Priority, GEngineIni) ?
			Priority : 0.8f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};


class FAndroid_ETC2TargetPlatform : public FAndroidTargetPlatform
{
public:

	FAndroid_ETC2TargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_ETC2");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_ETC2", "Android (ETC2)");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("ETC2");
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameETC2_RGB ||
			Format == AndroidTexFormat::NameETC2_RGBA ||
			Format == AndroidTexFormat::NameAutoETC2)
		{
			return true;
		}

		return false;
	}


	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return GLESVersion >= 0x30000;
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ETC2_ShortName", "ETC2");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ETC2"), Priority, GEngineIni) ?
			Priority : 0.2f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};

class FAndroid_ETC1TargetPlatform : public FAndroidTargetPlatform
{
public:

	FAndroid_ETC1TargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_ETC1");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_ETC1", "Android (ETC1)");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("ETC1");
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameETC1 ||
			Format == AndroidTexFormat::NameAutoETC1)
		{
			return true;
		}

		return false;
	}

	// End FAndroidTargetPlatform overrides

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return ExtensionsString.Contains(TEXT("GL_OES_compressed_ETC1_RGB8_texture"));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ETC1_ShortName", "ETC1");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ETC1"), Priority, GEngineIni) ?
			Priority : 0.1f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};

/**
* Android cooking platform which cooks only ETC1a based textures.
*/
class FAndroid_ETC1aTargetPlatform : public FAndroidTargetPlatform
{
public:

	FAndroid_ETC1aTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_ETC1a");
	}

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_ETC1a", "Android (ETCa1)");
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("ETC1a");
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameAutoETC1a)
		{
			return true;
		}

		return false;
	}

	// End FAndroidTargetPlatform overrides

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return GLESVersion >= 0x30000;
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ETC1a_ShortName", "ETC1a");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ETC1a"), Priority, GEngineIni) ? Priority : 1.0f;
	}
};




class FAndroid_MultiTargetPlatform : public FAndroidTargetPlatform
{
	TArray<ITargetPlatform*> FormatTargetPlatforms;
	FString FormatTargetString;

public:
	FAndroid_MultiTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient)
	{
		this->PlatformInfo = PlatformInfo::FindPlatformInfo("Android_Multi");
	}

	// set up all of the multiple formats together into this one
	void LoadFormats(TArray<FAndroidTargetPlatform*> SingleFormatTPs)
	{
		// sort formats by priority so higher priority formats are packaged (and thus used by the device) first
		// note that we passed this by value, not ref, so we can sort it
		SingleFormatTPs.Sort([](const FAndroidTargetPlatform& A, const FAndroidTargetPlatform& B)
		{
			float PriorityA = 0.f;
			float PriorityB = 0.f;
			FString VariantA = A.GetAndroidVariantName().Replace(TEXT("Client"), TEXT(""));
			FString VariantB = B.GetAndroidVariantName().Replace(TEXT("Client"), TEXT(""));
			GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantA), PriorityA, GEngineIni);
			GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantB), PriorityB, GEngineIni);
			return PriorityA > PriorityB;
		});

		FormatTargetPlatforms.Empty();
		FormatTargetString = TEXT("");

		TSet<FString> SeenFormats;

		// Load the TargetPlatform module for each format
		for (FAndroidTargetPlatform* SingleFormatTP : SingleFormatTPs)
		{
			// only use once each
			if (SeenFormats.Contains(SingleFormatTP->GetAndroidVariantName()))
			{
				continue;
			}
			SeenFormats.Add(SingleFormatTP->GetAndroidVariantName());

			bool bEnabled = false;
			FString SettingsName = FString(TEXT("bMultiTargetFormat_")) + *SingleFormatTP->GetAndroidVariantName();
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *SettingsName, bEnabled, GEngineIni);
			if (bEnabled)
			{
				if (FormatTargetPlatforms.Num())
				{
					FormatTargetString += TEXT(",");
				}
				FormatTargetString += SingleFormatTP->GetAndroidVariantName();
				FormatTargetPlatforms.Add(SingleFormatTP);
			}
		}

		PlatformInfo::UpdatePlatformDisplayName(TEXT("Android_Multi"), DisplayName());
	}

	virtual FString GetAndroidVariantName() const override
	{
		return TEXT("Multi");
	}

	virtual FText DisplayName() const override
	{
		return FText::Format(LOCTEXT("Android_Multi", "Android (Multi:{0})"), FText::FromString(FormatTargetString));
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray<FName>& OutFormats) const
	{
		// Ask each platform variant to choose texture formats
		for (ITargetPlatform* Platform : FormatTargetPlatforms)
		{
			TArray<FName> PlatformFormats;
			Platform->GetTextureFormats(Texture, PlatformFormats);
			for (FName Format : PlatformFormats)
			{
				OutFormats.AddUnique(Format);
			}
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		// Ask each platform variant to choose texture formats
		for (ITargetPlatform* Platform : FormatTargetPlatforms)
		{
			TArray<FName> PlatformFormats;
			Platform->GetAllTextureFormats(PlatformFormats);
			for (FName Format : PlatformFormats)
			{
				OutFormats.AddUnique(Format);
			}
		}
	}
#endif	

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_Multi_ShortName", "Multi");
	}

	virtual float GetVariantPriority() const override
	{
		// lowest priority so specific variants are chosen first
		return (IsClientOnly() ? 0.25f : 0.5f);
	}
};

#undef LOCTEXT_NAMESPACE

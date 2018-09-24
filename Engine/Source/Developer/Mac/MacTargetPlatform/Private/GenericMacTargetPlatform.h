// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericMacTargetPlatform.h: Declares the TGenericMacTargetPlatform class template.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Common/TargetPlatformBase.h"
#include "Mac/MacPlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "LocalMacTargetDevice.h"


#if WITH_ENGINE
#include "AudioCompressionSettings.h"
#include "Sound/SoundWave.h"
#include "TextureResource.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "TGenericMacTargetPlatform"

/**
 * Template for Mac target platforms
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
class TGenericMacTargetPlatform
	: public TTargetPlatformBase<FMacPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> >
{
public:

	typedef FMacPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> TProperties;
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TGenericMacTargetPlatform( )
	{
#if PLATFORM_MAC
		// only add local device if actually running on Mac
		LocalDevice = MakeShareable(new FLocalMacTargetDevice(*this));
#endif

		#if WITH_ENGINE
			FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *this->PlatformName());
			TextureLODSettings = nullptr;
			StaticMeshLODSettings.Initialize(EngineSettings);
		#endif
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		OutDevices.Reset();
		if (LocalDevice.IsValid())
		{
			OutDevices.Add(LocalDevice);
		}
	}

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override
	{
		return COMPRESS_ZLIB;
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override
	{
		if (LocalDevice.IsValid())
		{
			return LocalDevice;
		}

		return nullptr;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId )
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		return NULL;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Mac platform as editor for this to be considered a running platform
		return PLATFORM_MAC && !UE_SERVER && !UE_GAME && WITH_EDITOR && HAS_EDITOR_DATA;
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		// we currently do not have a build target for MacServer
		if (Feature == ETargetPlatformFeatures::Packaging)
		{
			return (HAS_EDITOR_DATA || !IS_DEDICATED_SERVER);
		}

return TSuper::SupportsFeature(Feature);
	}

#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!IS_DEDICATED_SERVER)
		{
			static FName NAME_SF_METAL_SM5_NOTESS(TEXT("SF_METAL_SM5_NOTESS"));
			OutFormats.AddUnique(NAME_SF_METAL_SM5_NOTESS);
			static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
			OutFormats.AddUnique(NAME_SF_METAL_SM5);
			static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
			OutFormats.AddUnique(NAME_SF_METAL_MACES3_1);
			static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
			OutFormats.AddUnique(NAME_SF_METAL_MACES2);
			static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
			OutFormats.AddUnique(NAME_SF_METAL_MRT_MAC);
		}
	}

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override
	{
		// Get the Target RHIs for this platform, we do not always want all those that are supported.
		TArray<FString>TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

		// Gather the list of Target RHIs and filter out any that may be invalid.
		TArray<FName> PossibleShaderFormats;
		GetAllPossibleShaderFormats(PossibleShaderFormats);

		for (int32 ShaderFormatIdx = TargetedShaderFormats.Num() - 1; ShaderFormatIdx >= 0; ShaderFormatIdx--)
		{
			FString ShaderFormat = TargetedShaderFormats[ShaderFormatIdx];
			if (PossibleShaderFormats.Contains(FName(*ShaderFormat)) == false)
			{
				TargetedShaderFormats.RemoveAt(ShaderFormatIdx);
			}
		}

		for(const FString& ShaderFormat : TargetedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}

	virtual void GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture (with DX11 support)
			FName TextureFormatName = GetDefaultTextureFormatName(this, Texture, EngineSettings, true);
			OutFormats.Add(TextureFormatName);
		}
	}


	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture (with DX11 support)
			GetAllDefaultTextureFormats(this, OutFormats, true);
		}
	}

	virtual FName GetVirtualTextureLayerFormat(int32 SourceFormat, bool bAllowCompression, bool bNoAlpha, bool bSupportDX11TextureFormats, int32 Settings) const override
	{
		FName TextureFormatName = NAME_None;

		// Supported texture format names.
		static FName NameDXT1(TEXT("DXT1"));
		static FName NameDXT3(TEXT("DXT3"));
		static FName NameDXT5(TEXT("DXT5"));
		static FName NameDXT5n(TEXT("DXT5n"));
		static FName NameBC4(TEXT("BC4"));
		static FName NameBC5(TEXT("BC5"));
		static FName NameBGRA8(TEXT("BGRA8"));
		static FName NameXGXR8(TEXT("XGXR8"));
		static FName NameG8(TEXT("G8"));
		static FName NameVU8(TEXT("VU8"));
		static FName NameRGBA16F(TEXT("RGBA16F"));
		static FName NameBC6H(TEXT("BC6H"));
		static FName NameBC7(TEXT("BC7"));

		// Note: We can't use things here like autoDXT here which defer the exact choice to the compressor as it would mean that
		// some textures on a VT layer may get a different format than others.
		// We need to guarantee the format to be the same for all textures on the layer so we need to decide on the exact final format here.

		bool bUseDXT5NormalMap = false;
		FString UseDXT5NormalMapsString;
		if (EngineSettings.GetString(TEXT("SystemSettings"), TEXT("Compat.UseDXT5NormalMaps"), UseDXT5NormalMapsString))
		{
			bUseDXT5NormalMap = FCString::ToBool(*UseDXT5NormalMapsString);
		}

		// Determine the pixel format of the (un/)compressed texture
		if (!bAllowCompression)
		{
			if (SourceFormat == TSF_RGBA16F)
			{
				TextureFormatName = NameRGBA16F;
			}
			else if (SourceFormat == TSF_G8 || Settings == TC_Grayscale)
			{
				TextureFormatName = NameG8;
			}
			else if (Settings == TC_Normalmap && bUseDXT5NormalMap)
			{
				TextureFormatName = NameXGXR8;
			}
			else
			{
				TextureFormatName = NameBGRA8;
			}
		}
		else if (Settings == TC_HDR)
		{
			TextureFormatName = NameRGBA16F;
		}
		else if (Settings == TC_Normalmap)
		{
			TextureFormatName = bUseDXT5NormalMap ? NameDXT5n : NameBC5;
		}
		else if (Settings == TC_Displacementmap)
		{
			TextureFormatName = NameG8;
		}
		else if (Settings == TC_VectorDisplacementmap)
		{
			TextureFormatName = NameBGRA8;
		}
		else if (Settings == TC_Grayscale)
		{
			TextureFormatName = NameG8;
		}
		else if (Settings == TC_Alpha)
		{
			TextureFormatName = NameBC4;
		}
		else if (Settings == TC_DistanceFieldFont)
		{
			TextureFormatName = NameG8;
		}
		else if (Settings == TC_HDR_Compressed)
		{
			TextureFormatName = NameBC6H;
		}
		else if (Settings == TC_BC7)
		{
			TextureFormatName = NameBC7;
		}
		else if (bNoAlpha)
		{
			TextureFormatName = NameDXT1;
		}
		else
		{
			TextureFormatName = NameDXT5;
		}

		/*
		FIXME: IS this still relevant for VT ? comes from the texture variant
		// Some PC GPUs don't support sRGB read from G8 textures (e.g. AMD DX10 cards on ShaderModel3.0)
		// This solution requires 4x more memory but a lot of PC HW emulate the format anyway
		if ((TextureFormatName == NameG8) && Texture->SRGB && !SupportsFeature(ETargetPlatformFeatures::GrayscaleSRGB))
		{
		TextureFormatName = NameBGRA8;
		}*/

		// fallback to non-DX11 formats if one was chosen, but we can't use it
		if (!bSupportDX11TextureFormats)
		{
			if (TextureFormatName == NameBC6H)
			{
				TextureFormatName = NameRGBA16F;
			}
			else if (TextureFormatName == NameBC7)
			{
				TextureFormatName = NameDXT5;
			}
		}

		return TextureFormatName;
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}


	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		static FName NAME_OPUS(TEXT("OPUS"));

		if (Wave->IsStreaming())
		{
			return NAME_OPUS;
		}

		return NAME_OGG;
	}

	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		static FName NAME_OPUS(TEXT("OPUS"));
		OutFormats.Add(NAME_OGG);
		OutFormats.Add(NAME_OPUS);
	}

	virtual FPlatformAudioCookOverrides* GetAudioCompressionSettings() const override
	{
		return nullptr;
	}

#endif //WITH_ENGINE


	virtual bool SupportsVariants() const override
	{
		return true;
	}


	virtual FText GetVariantDisplayName() const override
	{
		if (IS_DEDICATED_SERVER)
		{
			return LOCTEXT("MacServerVariantTitle", "Dedicated Server");
		}

		if (HAS_EDITOR_DATA)
		{
			return LOCTEXT("MacClientEditorDataVariantTitle", "Client with Editor Data");
		}

		if (IS_CLIENT_ONLY)
		{
			return LOCTEXT("MacClientOnlyVariantTitle", "Client only");
		}

		return LOCTEXT("MacClientVariantTitle", "Client");
	}


	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("MacVariantTitle", "Build Type");
	}


	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}


	DECLARE_DERIVED_EVENT(TGenericMacTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(TGenericMacTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

private:

	// Holds the local device.
	ITargetDevicePtr LocalDevice;

#if WITH_ENGINE
	// Holds the Engine INI settings for quick use.
	FConfigFile EngineSettings;

	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

#endif // WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};

#undef LOCTEXT_NAMESPACE

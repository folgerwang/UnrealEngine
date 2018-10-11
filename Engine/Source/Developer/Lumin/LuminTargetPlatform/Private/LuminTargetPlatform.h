// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LuminTargetPlatform.h: Declares the FLuminTargetPlatform class.
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
#include "Common/TargetPlatformBase.h"
#include "Android/AndroidProperties.h"
#include "AndroidTargetPlatform.h"

#if WITH_ENGINE
#include "Internationalization/Text.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

class FTargetDeviceId;
class ILuminDeviceDetection;
class UTextureLODSettings;
enum class ETargetPlatformFeatures;

/**
 * FSeakTargetPlatform, abstraction for cooking Lumin platforms
 */
class FLuminTargetPlatform : public FAndroidTargetPlatform
{
public:

	/**
	 * Default constructor.
	 */
	FLuminTargetPlatform(bool bIsClient);

	/**
	 * Destructor
	 */
	virtual ~FLuminTargetPlatform();

public:

	// Begin ITargetPlatform interface

	virtual FString PlatformName() const override
	{
		return IsClientOnly() ? TEXT("LuminClient") : TEXT("Lumin");
	}

	virtual FString IniPlatformName() const override
	{
		return TEXT("Lumin");
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

#if WITH_ENGINE

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual void GetReflectionCaptureFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(FName(TEXT("EncodedHDR")));
		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	virtual FPlatformAudioCookOverrides* GetAudioCompressionSettings() const override
	{
		return nullptr;
	}
	
	// True if the project requires encoded HDR reflection captures
	bool bRequiresEncodedHDRReflectionCaptures;
#endif //WITH_ENGINE

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings");
	}


	// End ITargetPlatform interface

	virtual bool SupportsDesktopRendering() const;// override;
	virtual bool SupportsMobileRendering() const;// override;
	virtual void InitializeDeviceDetection() override;

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override;

	virtual void RefreshSettings() override;

protected:
	virtual FAndroidTargetDevicePtr CreateTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant) const override;

protected:
	// Holds the Engine INI settings, for quick use.
	FConfigFile LuminEngineSettings;

};


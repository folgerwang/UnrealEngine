// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "LuminRuntimeSettings.generated.h"

UENUM(BlueprintType)
enum class ELuminFrameTimingHint : uint8
{
	/* Default rate is unspecified, adjusted based on system conditions. */
	Unspecified,
	/* Run at the maximum rate allowed by the system. */
	Maximum,
	/* Run at a specified rate of 60Hz (i.e. one frame every ~16.67 ms). */
	FPS_60,
	/* Run at a specified rate of 120Hz (i.e. one frame every ~8.33 ms). */
	FPS_120
};

UENUM(BlueprintType)
enum class ELuminPrivilege : uint8
{
	Invalid,
	AudioRecognizer,
	BatteryInfo,
	CameraCapture,
	WorldReconstruction,
	InAppPurchase,
	AudioCaptureMic,
	DrmCertificates,
	Occlusion,
	LowLatencyLightwear,
	Internet,
	IdentityRead,
	BackgroundDownload,
	BackgroundUpload,
	MediaDrm,
	Media,
	MediaMetadata,
	PowerInfo,
	LocalAreaNetwork,
	VoiceInput,
	Documents,
	ConnectBackgroundMusicService,
	RegisterBackgroundMusicService,
	PwFoundObjRead,
	NormalNotificationsUsage,
	MusicService,
	ControllerPose,
	ScreensProvider,
	GesturesSubscribe,
	GesturesConfig,
};

/**
 * IMPORTANT!! Add a default value for every new UPROPERTY in the ULuminRuntimeSettings class in <UnrealEngine>/Engine/Config/BaseEngine.ini
 */

/**
 * Implements the settings for the Lumin runtime platform.
 */
UCLASS(config=Engine, defaultconfig)
class LUMINRUNTIMESETTINGS_API ULuminRuntimeSettings : public UObject
{
public:
	GENERATED_BODY()

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** The official name of the project. Note: Must have at least 2 sections separated by a period and be unique! */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "MPK Packaging", Meta = (DisplayName = "Magic Leap Package Name ('com.Company.Project', [PROJECT] is replaced with project name)"))
	FString PackageName;

	/** The visual application name displayed for end users. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "MPK Packaging", Meta = (DisplayName = "Application Display Name (project name if blank)"))
	FString ApplicationDisplayName;

	/** Is a Screens type (Magic TV) app. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", Meta = (DisplayName = "Is Screens App"))
	bool bIsScreensApp;

	/** Indicates to the Lumin OS what the application's target framerate is, to improve prediction and reprojection */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Runtime", Meta = (DisplayName = "Frame timing hint"))
	ELuminFrameTimingHint FrameTimingHint;

	/** Content for this app is protected and should not be recorded or captured outside the graphics system. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Runtime", Meta = (DisplayName = "Protected Content"))
	bool bProtectedContent;

	/** If checked, use Mobile Rendering. Otherwise, use Desktop Rendering [FOR FULL SOURCE GAMES ONLY]. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", Meta = (DisplayName = "Use Mobile Rendering.  Otherwise, Desktop Rendering"))
	bool bUseMobileRendering;

	UPROPERTY(GlobalConfig, Meta = (DisplayName = "Use Vulkan (otherwise, OpenGL)"))
	bool bUseVulkan;

	/** Enable support for NVIDIA Tegra Graphics Debugger [FOR FULL SOURCE GAMES ONLY]. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", Meta = (DisplayName = "Support NVIDIA Tegra Graphics Debugger"))
	bool bBuildWithNvTegraGfxDebugger;

	/** Certificate File used to sign builds for distribution. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Distribution Signing", Meta = (DisplayName = "Certificate File Path"))
	FFilePath Certificate;
	
	/** Folder containing the assets (FBX / OBJ / MTL / PNG files) used for the Magic Leap App Icon model. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Magic Leap App Tile", Meta = (DisplayName = "Icon Model"))
	FDirectoryPath IconModelPath;

	/** Folder containing the assets (FBX / OBJ / MTL / PNG files) used for the Magic Leap App Icon portal. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Magic Leap App Tile", Meta = (DisplayName = "Icon Portal"))
	FDirectoryPath IconPortalPath;

	/** Used as an internal version number. This number is used only to determine whether one version is more recent than another, with higher numbers indicating more recent versions. This is not the version number shown to users. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced MPK Packaging", Meta = (DisplayName = "Version Code", ClampMin = 0))
	int32 VersionCode;

	/** Minimum API level required based on which APIs have been integrated into the base engine. Developers can set higher API levels if they are implementing newer APIs. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced MPK Packaging", Meta = (DisplayName = "Minimum API Level", ClampMin = 2))
	int32 MinimumAPILevel;

	/** Any privileges your app needs. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced MPK Packaging", Meta = (DisplayName = "App Privileges"))
	TArray<ELuminPrivilege> AppPrivileges;

	/** Extra nodes under the <application> node. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced MPK Packaging", Meta = (DisplayName = "Extra nodes under the <application> node"))
	TArray<FString> ExtraApplicationNodes;

	/** Extra nodes under the <component> node like <mime-type>, <schema> etc. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced MPK Packaging", Meta = (DisplayName = "Extra nodes under the <component> node"))
	TArray<FString> ExtraComponentNodes;

	/** Which of the currently enabled spatialization plugins to use on Lumin. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled reverb plugins to use on Lumin. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use on Lumin. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;

	// Strip debug symbols from packaged builds even if they aren't shipping builds.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = AdvancedBuild, meta = (DisplayName = "Strip debug symbols from packaged builds even if they aren't shipping builds"))
	bool bRemoveDebugInfo;
};

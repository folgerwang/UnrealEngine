// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "ProjectPackagingSettings.generated.h"

/**
 * Enumerates the available build configurations for project packaging.
 */
UENUM()
enum EProjectPackagingBuildConfigurations
{
	/** Debug configuration. */
	PPBC_DebugGame UMETA(DisplayName="DebugGame"),

	/** Debug Client configuration. */
	PPBC_DebugGameClient UMETA(DisplayName = "DebugGame Client"),

	/** Development configuration. */
	PPBC_Development UMETA(DisplayName="Development"),

	/** Development Client configuration. */
	PPBC_DevelopmentClient UMETA(DisplayName = "Development Client"),

	/** Shipping configuration. */
	PPBC_Shipping UMETA(DisplayName="Shipping"),

	/** Shipping Client configuration. */
	PPBC_ShippingClient UMETA(DisplayName = "Shipping Client")
};

/**
 * Enumerates the available internationalization data presets for project packaging.
 */
UENUM()
enum class EProjectPackagingInternationalizationPresets : uint8
{
	/** English only. */
	English,

	/** English, French, Italian, German, Spanish. */
	EFIGS,

	/** English, French, Italian, German, Spanish, Chinese, Japanese, Korean. */
	EFIGSCJK,

	/** Chinese, Japanese, Korean. */
	CJK,

	/** All known cultures. */
	All
};

/**
 * Determines whether to build the executable when packaging. Note the equivalence between these settings and EPlayOnBuildMode.
 */
UENUM()
enum class EProjectPackagingBuild
{
	/** Always build. */
	Always UMETA(DisplayName="Always"),

	/** Never build. */
	Never UMETA(DisplayName="Never"),

	/** Default (if the Never build. */
	IfProjectHasCode UMETA(DisplayName="If project has code, or running a locally built editor"),

	/** If we're not packaging from a promoted build. */
	IfEditorWasBuiltLocally UMETA(DisplayName="If running a locally built editor")
};

/**
* Enumerates the available methods for Blueprint nativization during project packaging.
*/
UENUM()
enum class EProjectPackagingBlueprintNativizationMethod : uint8
{
	/** Disable Blueprint nativization (default). */
	Disabled,

	/** Enable nativization for all Blueprint assets. */
	Inclusive,

	/** Enable nativization for selected Blueprint assets only. */
	Exclusive
};

/**
 * Implements the Editor's user settings.
 */
UCLASS(config=Game, defaultconfig)
class UNREALED_API UProjectPackagingSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Specifies whether to build the game executable during packaging. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	EProjectPackagingBuild Build;

	/** The build configuration for which the project is packaged. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	TEnumAsByte<EProjectPackagingBuildConfigurations> BuildConfiguration;

	/** The directory to which the packaged project will be copied. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	FDirectoryPath StagingDirectory;

	/**
	 * If enabled, a full rebuild will be enforced each time the project is being packaged.
	 * If disabled, only modified files will be built, which can improve iteration time.
	 * Unless you iterate on packaging, we recommend full rebuilds when packaging.
	 */
	UPROPERTY(config, EditAnywhere, Category=Project)
	bool FullRebuild;

	/**
	 * If enabled, a distribution build will be created and the shipping configuration will be used
	 * If disabled, a development build will be created
	 * Distribution builds are for publishing to the App Store
	 */
	UPROPERTY(config, EditAnywhere, Category=Project)
	bool ForDistribution;

	/** If enabled, debug files will be included in the packaged game */
	UPROPERTY(config, EditAnywhere, Category=Project)
	bool IncludeDebugFiles;

	/** If enabled, then the project's Blueprint assets (including structs and enums) will be intermediately converted into C++ and used in the packaged project (in place of the .uasset files).*/
	UPROPERTY(config, EditAnywhere, Category = Blueprints)
	EProjectPackagingBlueprintNativizationMethod BlueprintNativizationMethod;

	/** List of Blueprints to include for nativization when using the exclusive method. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Blueprints, meta = (DisplayName = "List of Blueprint assets to nativize", RelativeToGameContentDir, LongPackageName))
	TArray<FFilePath> NativizeBlueprintAssets;

	/** If enabled, the nativized assets code plugin will be added to the Visual Studio solution if it exists when regenerating the game project. Intended primarily to assist with debugging the target platform after cooking with nativization turned on. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Blueprints)
	bool bIncludeNativizedAssetsInProjectGeneration;

	/** Whether or not to exclude monolithic engine headers (e.g. Engine.h) in the generated code when nativizing Blueprint assets. This may improve C++ compiler performance if your game code does not depend on monolithic engine headers to build. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Blueprints)
	bool bExcludeMonolithicEngineHeadersInNativizedCode;

	/** If enabled, all content will be put into a one or more .pak files instead of many individual files (default = enabled). */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool UsePakFile;

	/** 
	 * If enabled, will generate pak file chunks.  Assets can be assigned to chunks in the editor or via a delegate (See ShooterGameDelegates.cpp). 
	 * Can be used for streaming installs (PS4 Playgo, XboxOne Streaming Install, etc)
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bGenerateChunks;

	/** 
	 * If enabled, no platform will generate chunks, regardless of settings in platform-specific ini files.
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bGenerateNoChunks;

	/**
	 * Normally during chunk generation all dependencies of a package in a chunk will be pulled into that package's chunk.
	 * If this is enabled then only hard dependencies are pulled in. Soft dependencies stay in their original chunk.
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging)
	bool bChunkHardReferencesOnly;

	/**
	 * If true, individual files are only allowed to be in a single chunk and it will assign it to the lowest number requested
	 * If false, it may end up in multiple chunks if requested by the cooker
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay)
	bool bForceOneChunkPerFile;

	/**
	 * If > 0 this sets a maximum size per chunk. Chunks larger than this size will be split into multiple pak files such as pakchunk0_s1
	 * This can be set in platform specific game.ini files
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay)
	int64 MaxChunkSize;

	/** 
	 * If enabled, will generate data for HTTP Chunk Installer. This data can be hosted on webserver to be installed at runtime. Requires "Generate Chunks" enabled.
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bBuildHttpChunkInstallData;

	/** 
	 * When "Build HTTP Chunk Install Data" is enabled this is the directory where the data will be build to.
	 */	
	UPROPERTY(config, EditAnywhere, Category = Packaging)
	FDirectoryPath HttpChunkInstallDataDirectory;

	/**
	 * A comma separated list of formats to use for .pak file compression. If more than one is specified, the list is in order of priority, with fallbacks to other formats
	 * in case of errors or unavailability of the format (plugin not enabled, etc).
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, meta = (DisplayName = "Pak File Compression Format(s)"))
	FString PakFileCompressionFormats;

	/**
	 * A generic setting for allowing a project to control compression settings during .pak file compression. For instance, if using the Oodle plugin, you could use -oodlemethod=Selkie -oodlelevel=Optimal1
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, meta = (DisplayName = "Pak File Compression Commandline Options"))
	FString PakFileAdditionalCompressionOptions;

	/** 
	 * Version name for HTTP Chunk Install Data.
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging)
	FString HttpChunkInstallDataVersion;

	/** Specifies whether to include an installer for prerequisites of packaged games, such as redistributable operating system components, on platforms that support it. */
	UPROPERTY(config, EditAnywhere, Category=Prerequisites, meta=(DisplayName="Include prerequisites installer"))
	bool IncludePrerequisites;

	/** Specifies whether to include prerequisites alongside the game executable. */
	UPROPERTY(config, EditAnywhere, Category = Prerequisites, meta = (DisplayName = "Include app-local prerequisites"))
	bool IncludeAppLocalPrerequisites;

	/** 
	 * By default shader code gets saved inline inside material assets, 
	 * enabling this option will store only shader code once as individual files
	 * This will reduce overall package size but might increase loading time
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bShareMaterialShaderCode;

	/** 
	 * By default shader shader code gets saved into individual platform agnostic files,
	 * enabling this option will use the platform-specific library format if and only if one is available
	 * This will reduce overall package size but might increase loading time
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, meta = (EditCondition = "bShareMaterialShaderCode", ConfigRestartRequired = true))
	bool bSharedMaterialNativeLibraries;

	/** A directory containing additional prerequisite packages that should be staged in the executable directory. Can be relative to $(EngineDir) or $(ProjectDir) */
	UPROPERTY(config, EditAnywhere, Category=Prerequisites, AdvancedDisplay)
	FDirectoryPath ApplocalPrerequisitesDirectory;

	/**
	 * Specifies whether to include the crash reporter in the packaged project. 
	 * This is included by default for Blueprint based projects, but can optionally be disabled.
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay)
	bool IncludeCrashReporter;

	/** Predefined sets of culture whose internationalization data should be packaged. */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Internationalization Support"))
	EProjectPackagingInternationalizationPresets InternationalizationPreset;

	/** Cultures whose data should be cooked, staged, and packaged. */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Localizations to Package"))
	TArray<FString> CulturesToStage;

	/**
	 * Cook all things in the project content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Cook everything in the project content directory (ignore list of maps below)"))
	bool bCookAll;

	/**
	 * Cook only maps (this only affects the cookall flag)
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Cook only maps (this only affects cookall)"))
	bool bCookMapsOnly;


	/**
	 * Create compressed cooked packages (decreased deployment size)
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Create compressed cooked packages"))
	bool bCompressed;

	/**
	* Encrypt ini files inside of the pak file
	* NOTE: Replaced by the settings inside the cryptokeys system. Kept here for legacy migration purposes.
	*/
	UPROPERTY(config)
	bool bEncryptIniFiles_DEPRECATED;

	/**
	* Encrypt the pak index
	* NOTE: Replaced by the settings inside the cryptokeys system. Kept here for legacy migration purposes.
	*/
	UPROPERTY(config)
	bool bEncryptPakIndex_DEPRECATED;

	/**
	 * Enable the early downloader pak file pakearly.txt
	 * This has been superseded by the functionality in DefaultPakFileRules.ini
	 */
	UPROPERTY(config)
	bool GenerateEarlyDownloaderPakFile_DEPRECATED;
	
	/**
	 * Don't include content in any editor folders when cooking.  This can cause issues with missing content in cooked games if the content is being used. 
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Exclude editor content when cooking"))
	bool bSkipEditorContent;

	/**
	 * Don't include movies by default when staging/packaging
	 * Specific movies can be specified below, and this can be in a platform ini
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Exclude movie files when staging"))
	bool bSkipMovies;

	/**
	 * If SkipMovies is true, these specific movies will still be added to the .pak file (if using a .pak file; otherwise they're copied as individual files)
	 * This should be the name with no extension
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Specific movies to Package"))
	TArray<FString> UFSMovies;

	/**
	 * If SkipMovies is true, these specific movies will be copied when packaging your project, but are not supposed to be part of the .pak file
	 * This should be the name with no extension
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Specific movies to Copy"))
	TArray<FString> NonUFSMovies;

	/**
	 * If set, only these specific pak files will be compressed. This should take the form of "*pakchunk0*"
	 * This can be set in a platform-specific ini file
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay)
	TArray<FString> CompressedChunkWildcard;

	/**
	 * List of specific files to include with GenerateEarlyDownloaderPakFile
	 */
	UPROPERTY(config)
	TArray<FString> EarlyDownloaderPakFileFiles_DEPRECATED;


	/**
	 * List of maps to include when no other map list is specified on commandline
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "List of maps to include in a packaged build", RelativeToGameContentDir, LongPackageName))
	TArray<FFilePath> MapsToCook;	

	/**
	 * Directories containing .uasset files that should always be cooked regardless of whether they're referenced by anything in your project
	 * These paths are stored relative to the project root so they can start with /game, /engine, or /pluginname
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Asset Directories to Cook", LongPackageName))
	TArray<FDirectoryPath> DirectoriesToAlwaysCook;

	/**
	 * Directories containing .uasset files that should never be cooked even if they are referenced by your project
	 * These paths are stored relative to the project root so they can start with /game, /engine, or /pluginname
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Directories to never cook", LongPackageName))
	TArray<FDirectoryPath> DirectoriesToNeverCook;

	/**
	 * Directories containing files that should always be added to the .pak file (if using a .pak file; otherwise they're copied as individual files)
	 * This is used to stage additional files that you manually load via the UFS (Unreal File System) file IO API
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories to Package", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsUFS;

	/**
	 * Directories containing files that should always be copied when packaging your project, but are not supposed to be part of the .pak file
	 * This is used to stage additional files that you manually load without using the UFS (Unreal File System) file IO API, eg, third-party libraries that perform their own internal file IO
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories To Copy", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsNonUFS;	

	/**
	 * Directories containing files that should always be added to the .pak file for a dedicated server (if using a .pak file; otherwise they're copied as individual files)
	 * This is used to stage additional files that you manually load via the UFS (Unreal File System) file IO API
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories to Package for dedicated server only", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsUFSServer;

	/**
	 * Directories containing files that should always be copied when packaging your project for a dedicated server, but are not supposed to be part of the .pak file
	 * This is used to stage additional files that you manually load without using the UFS (Unreal File System) file IO API, eg, third-party libraries that perform their own internal file IO
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories To Copy for dedicated server only", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsNonUFSServer;	

private:
	/** Helper array used to mirror Blueprint asset selections across edits */
	TArray<FFilePath> CachedNativizeBlueprintAssets;

	UPROPERTY(config)
	bool bNativizeBlueprintAssets_DEPRECATED;

	UPROPERTY(config)
	bool bNativizeOnlySelectedBlueprints_DEPRECATED;
	
public:

	// UObject Interface

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
	virtual bool CanEditChange( const UProperty* InProperty ) const override;

	/** Adds the given Blueprint asset to the exclusive nativization list. */
	bool AddBlueprintAssetToNativizationList(const class UBlueprint* InBlueprint);

	/** Removes the given Blueprint asset from the exclusive nativization list. */
	bool RemoveBlueprintAssetFromNativizationList(const class UBlueprint* InBlueprint);

	/** Determines if the specified Blueprint is already saved for exclusive nativization. */
	bool IsBlueprintAssetInNativizationList(const class UBlueprint* InBlueprint) const { return FindBlueprintInNativizationList(InBlueprint) >= 0; }

private:
	/** Returns the index of the specified Blueprint in the exclusive nativization list (otherwise INDEX_NONE) */
	int32 FindBlueprintInNativizationList(const UBlueprint* InBlueprint) const;

	/** Fix up cooking paths after they've been edited or laoded */
	void FixCookingPaths();
};

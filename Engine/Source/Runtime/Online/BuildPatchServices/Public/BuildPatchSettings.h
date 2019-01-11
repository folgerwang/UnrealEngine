// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Variant.h"

#include "Interfaces/IBuildManifest.h"
#include "BuildPatchDelta.h"
#include "BuildPatchFeatureLevel.h"
#include "BuildPatchInstall.h"
#include "BuildPatchVerify.h"

namespace BuildPatchServices
{
	/**
	 * Defines a list of all build patch services initialization settings, can be used to override default init behaviors.
	 */
	struct BUILDPATCHSERVICES_API FBuildPatchServicesInitSettings
	{
	public:
		/**
		 * Default constructor. Initializes all members with default behavior values.
		 */
		FBuildPatchServicesInitSettings();

	public:
		// The application settings directory.
		FString ApplicationSettingsDir;
		// The application project name.
		FString ProjectName;
		// The local machine config file name.
		FString LocalMachineConfigFileName;
	};

	/**
	 * Defines a list of all the options of an installation task.
	 */
	struct BUILDPATCHSERVICES_API FInstallerConfiguration
	{
		/**
		 * Construct with install manifest, provides common defaults for other settings.
		 */
		FInstallerConfiguration(const IBuildManifestRef& InInstallManifest);

		/**
		 * Copy constructor.
		 */
		FInstallerConfiguration(const FInstallerConfiguration& CopyFrom);

		/**
		 * RValue constructor to allow move semantics.
		 */
		FInstallerConfiguration(FInstallerConfiguration&& MoveFrom);

	public:
		// The manifest that the current install was generated from (if applicable).
		IBuildManifestPtr CurrentManifest;
		// The manifest to be installed.
		IBuildManifestRef InstallManifest;
		// The directory to install to.
		FString InstallDirectory;
		// The directory for storing the intermediate files. This would usually be inside the InstallDirectory. Empty string will use module's global setting.
		FString StagingDirectory;
		// The directory for placing files that are believed to have local changes, before we overwrite them. Empty string will use module's global setting. If both empty, the feature disables.
		FString BackupDirectory;
		// The list of chunk database filenames that will be used to pull patch data from.
		TArray<FString> ChunkDatabaseFiles;
		// The list of cloud directory roots that will be used to pull patch data from. Empty array will use module's global setting.
		TArray<FString> CloudDirectories;
		// The set of tags that describe what to be installed. Empty set means full installation.
		TSet<FString> InstallTags;
		// The mode for installation.
		EInstallMode InstallMode;
		// The mode for verification.
		EVerifyMode VerifyMode;
		// The policy to follow for requesting an optimised delta.
		EDeltaPolicy DeltaPolicy;
		// Whether the operation is a repair to an existing installation only.
		bool bIsRepair;
		// Whether to run the prerequisite installer provided if it hasn't been ran before on this machine.
		bool bRunRequiredPrereqs;
		// Whether to allow this installation to run concurrently with any existing installations.
		bool bAllowConcurrentExecution;
	};

	/**
	 * Defines a list of all options for the build chunking task.
	 */
	struct BUILDPATCHSERVICES_API FChunkBuildConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FChunkBuildConfiguration();

	public:
		// The client feature level to output data for.
		EFeatureLevel FeatureLevel;
		// The directory to analyze.
		FString RootDirectory;
		// The ID of the app of this build.
		uint32 AppId;
		// The name of the app of this build.
		FString AppName;
		// The version string for this build.
		FString BuildVersion;
		// The local exe path that would launch this build.
		FString LaunchExe;
		// The command line that would launch this build.
		FString LaunchCommand;
		// The path to a file containing a \r\n separated list of RootDirectory relative files to read.
		FString InputListFile;
		// The path to a file containing a \r\n separated list of RootDirectory relative files to ignore.
		FString IgnoreListFile;
		// The path to a file containing a \r\n separated list of RootDirectory relative files followed by attribute keywords.
		FString AttributeListFile;
		// The set of identifiers which the prerequisites satisfy.
		TSet<FString> PrereqIds;
		// The display name of the prerequisites installer.
		FString PrereqName;
		// The path to the prerequisites installer.
		FString PrereqPath;
		// The command line arguments for the prerequisites installer.
		FString PrereqArgs;
		// The maximum age (in days) of existing data files which can be reused in this build.
		float DataAgeThreshold;
		// Indicates whether data age threshold should be honored. If false, ALL data files can be reused.
		bool bShouldHonorReuseThreshold;
		// The chunk window size to be used when saving out new data.
		uint32 OutputChunkWindowSize;
		// Indicates whether any window size chunks should be matched, rather than just out output window size.
		bool bShouldMatchAnyWindowSize;
		// Map of custom fields to add to the manifest.
		TMap<FString, FVariant> CustomFields;
		// The cloud directory that all patch data will be saved to. An empty value will use module's global setting.
		FString CloudDirectory;
		// The output manifest filename.
		FString OutputFilename;
	};

	// Temporary for use with deprecated module function.
	typedef FChunkBuildConfiguration FGenerationConfiguration;

	/**
	 * Defines a list of all options for the chunk delta optimisation task.
	 */
	struct BUILDPATCHSERVICES_API FChunkDeltaOptimiserConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FChunkDeltaOptimiserConfiguration();

	public:
		// A full file or http path for the manifest to be used as the source build.
		FString ManifestAUri;
		// A full file or http path for the manifest to be used as the destination build.
		FString ManifestBUri;
		// The cloud directory that all patch data will be saved to. An empty value will use ManifestB's directory.
		FString CloudDirectory;
		// The window size to use for find new matches.
		uint32 ScanWindowSize;
		// The chunk size to use for saving new diff data.
		uint32 OutputChunkSize;
	};

	/**
	 * Defines a list of all options for the patch data enumeration task.
	 */
	struct BUILDPATCHSERVICES_API FPatchDataEnumerationConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FPatchDataEnumerationConfiguration();

	public:
		// A full file path for the manifest or chunkdb to enumerate referenced data for.
		FString InputFile;
		// A full file path to a file where the list will be saved out to.
		FString OutputFile;
		// Whether to include files sizes.
		bool bIncludeSizes;
	};

	/**
	 * Defines a list of all options for the diff manifests task.
	 */
	struct BUILDPATCHSERVICES_API FDiffManifestsConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FDiffManifestsConfiguration();

	public:
		// A full file or http path for the manifest to be used as the source build.
		FString ManifestAUri;
		// A full file or http path for the manifest to be used as the destination build.
		FString ManifestBUri;
		// The tag set to use to filter desired files from ManifestA.
		TSet<FString> TagSetA;
		// The tag set to use to filter desired files from ManifestB.
		TSet<FString> TagSetB;
		// Tag sets that will be used to calculate additional differential size statistics between manifests.
		// They must all be a subset of anything used in TagSetB.
		TArray<TSet<FString>> CompareTagSets;
		// A full file path where a JSON object will be saved for the diff details.Empty string if not desired.
		FString OutputFilePath;
	};

	/**
	 * Defines a list of all options for the cloud directory compactifier task.
	 */
	struct BUILDPATCHSERVICES_API FCompactifyConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FCompactifyConfiguration();

	public:
		// The path to the directory to compactify.
		FString CloudDirectory;
		// Chunks which are not referenced by a valid manifest, and which are older than this age(in days), will be deleted.
		float DataAgeThreshold;
		// The full path to a file to which a list of all chunk files deleted by compactify will be written.The output filenames will be relative to the cloud directory.
		FString DeletedChunkLogFile;
		// If ran in preview mode, then the process will run in logging mode only - no files will be deleted.
		bool bRunPreview;
	};

	/**
	 * Defines a list of all options for the chunk packaging task.
	 */
	struct BUILDPATCHSERVICES_API FPackageChunksConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FPackageChunksConfiguration();

	public:
		// The client feature level to output data for.
		EFeatureLevel FeatureLevel;
		// A full file path to the manifest to enumerate chunks from.
		FString ManifestFilePath;
		// A full file path to a manifest describing a previous build, which will filter out saved chunks for patch only chunkdbs.
		FString PrevManifestFilePath;
		// Optional list of tagsets to split chunkdb files on.Empty array will include all data as normal.
		TArray<TSet<FString>> TagSetArray;
		// A full file path to the chunkdb file to save. Extension of .chunkdb will be added if not present.
		FString OutputFile;
		// Cloud directory where chunks to be packaged can be found.
		FString CloudDir;
		// The maximum desired size for each chunkdb file.
		uint64 MaxOutputFileSize;
		// A full file path to use when saving the json output data.
		FString ResultDataFilePath;
	};
}

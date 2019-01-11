// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BuildPatchManifest.h"
#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class IDownloadService;

	/**
	 * An interface providing access for retrieving the optimised delta manifest used to patch from a specific source to a specific destination.
	 */
	class IOptimisedDelta
	{
	public:
		virtual ~IOptimisedDelta() {}

		/**
		 * Gets the manifest that should be used as the destination manifest. Invalid if there was an error.
		 * @return the destination manifest.
		 */
		virtual FBuildPatchAppManifestPtr GetDestinationManifest() = 0;

		/**
		 * Gets the size of the metadata downloaded to create the optimised manifest.
		 * @return the downloaded size in bytes.
		 */
		virtual int32 GetMetaDownloadSize() = 0;
	};

	/**
	 * Defines a list of configuration details required for the IOptimisedDelta construction.
	 */
	struct FOptimisedDeltaConfiguration
	{
	public:
		/**
		 * Construct with destination manifest, this is a required param.
		 */
		FOptimisedDeltaConfiguration(FBuildPatchAppManifestRef DestinationManifest);

	public:
		// The installation provided source manifest.
		FBuildPatchAppManifestPtr SourceManifest;
		// The installation provided destination manifest.
		FBuildPatchAppManifestRef DestinationManifest;
		// The list of cloud directory roots that will be used to pull patch data from.
		TArray<FString> CloudDirectories;
		// The policy to follow for requesting an optimised delta.
		EDeltaPolicy DeltaPolicy;
		// Optional ptr to an Installer configuration for additional logic based on installer setup.
		const FInstallerConfiguration* InstallerConfiguration;
	};

	/**
	 * Defines a list of dependencies required for the IOptimisedDelta construction.
	 */
	struct FOptimisedDeltaDependencies
	{
	public:
		/**
		 * Constructor setting up default values.
		 */
		FOptimisedDeltaDependencies();

	public:
		// A download service instance.
		IDownloadService* DownloadService;
		// Function to call once the destination manifest has been selected.
		TFunction<void(FBuildPatchAppManifestPtr)> OnComplete;
	};

	class FOptimisedDeltaFactory
	{
	public:
		static IOptimisedDelta* Create(const FOptimisedDeltaConfiguration& Configuration, const FOptimisedDeltaDependencies& Dependencies);
	};
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Internationalization/GatherableTextData.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextFromAssetsCommandlet.generated.h"

/**
 *	UGatherTextFromAssetsCommandlet: Localization commandlet that collects all text to be localized from the game assets.
 */
UCLASS()
class UGatherTextFromAssetsCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

	void ProcessGatherableTextDataArray(const TArray<FGatherableTextData>& GatherableTextDataArray);
	void CalculateDependenciesForPackagesPendingGather();
	bool HasExceededMemoryLimit();
	void PurgeGarbage(const bool bPurgeReferencedPackages);
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;

	bool GetConfigurationScript(const TMap<FString, FString>& InCommandLineParameters, FString& OutFilePath, FString& OutStepSectionName);
	bool ConfigureFromScript(const FString& GatherTextConfigPath, const FString& SectionName);

	//~ End UCommandlet Interface

private:
	/** Struct containing the data needed by a pending package that we will gather text from */
	struct FPackagePendingGather
	{
		/** The name of the package */
		FName PackageName;

		/** The filename of the package on disk */
		FString PackageFilename;

		/** The complete set of dependencies for the package */
		TSet<FName> Dependencies;
	};

	static const FString UsageText;

	TArray<FString> ModulesToPreload;
	TArray<FString> IncludePathFilters;
	TArray<FString> CollectionFilters;
	TArray<FString> ExcludePathFilters;
	TArray<FString> PackageFileNameFilters;
	TArray<FString> ExcludeClassNames;
	TArray<FString> ManifestDependenciesList;

	TArray<FPackagePendingGather> PackagesPendingGather;

	/** The number of packages to process per-batch */
	int32 PackagesPerBatchCount;

	/** Max memory we should use before forcing a full GC */
	uint64 MaxMemoryAllowanceBytes;

	/** Array of objects that should be kept alive during the next call to CollectGarbage (used by PurgeGarbage and AddReferencedObjects) */
	TSet<UObject*> ObjectsToKeepAlive;

	bool bSkipGatherCache;
	bool ShouldGatherFromEditorOnlyData;
	bool ShouldExcludeDerivedClasses;
};

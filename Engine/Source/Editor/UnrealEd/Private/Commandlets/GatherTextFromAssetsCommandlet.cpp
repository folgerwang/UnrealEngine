// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextFromAssetsCommandlet.h"
#include "UObject/Class.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/FeedbackContext.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "Modules/ModuleManager.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "UObject/PackageFileSummary.h"
#include "Framework/Commands/Commands.h"
#include "Commandlets/GatherTextFromSourceCommandlet.h"
#include "Templates/ScopedPointer.h"
#include "AssetData.h"
#include "Sound/DialogueWave.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "PackageHelperFunctions.h"
#include "Templates/UniquePtr.h"
#include "CollectionManagerModule.h"
#include "ICollectionManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromAssetsCommandlet, Log, All);

/** Special feedback context used to stop the commandlet to reporting failure due to a package load error */
class FLoadPackageLogOutputRedirector : public FFeedbackContext
{
public:
	struct FScopedCapture
	{
		FScopedCapture(FLoadPackageLogOutputRedirector* InLogOutputRedirector, const FString& InPackageContext)
			: LogOutputRedirector(InLogOutputRedirector)
		{
			LogOutputRedirector->BeginCapturingLogData(InPackageContext);
		}

		~FScopedCapture()
		{
			LogOutputRedirector->EndCapturingLogData();
		}

		FLoadPackageLogOutputRedirector* LogOutputRedirector;
	};

	FLoadPackageLogOutputRedirector()
		: ErrorCount(0)
		, WarningCount(0)
		, FormattedErrorsAndWarningsList()
		, PackageContext()
		, OriginalWarningContext(nullptr)
	{
	}

	virtual ~FLoadPackageLogOutputRedirector()
	{
	}

	void BeginCapturingLogData(const FString& InPackageContext)
	{
		// Override GWarn so that we can capture any log data
		check(!OriginalWarningContext);
		OriginalWarningContext = GWarn;
		GWarn = this;

		PackageContext = InPackageContext;

		// Reset the counts and previous log output
		ErrorCount = 0;
		WarningCount = 0;
		FormattedErrorsAndWarningsList.Reset();
	}

	void EndCapturingLogData()
	{
		// Restore the original GWarn now that we've finished capturing log data
		check(OriginalWarningContext);
		GWarn = OriginalWarningContext;
		OriginalWarningContext = nullptr;

		// Report any messages, and also report a warning if we silenced some warnings or errors when loading
		if (ErrorCount > 0 || WarningCount > 0)
		{
			static const FString LogIndentation = TEXT("    ");

			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Package '%s' produced %d error(s) and %d warning(s) while loading. Please verify that your text has gathered correctly."), *PackageContext, ErrorCount, WarningCount);

			GWarn->Log(NAME_None, ELogVerbosity::Display, FString::Printf(TEXT("The following errors and warnings were reported while loading '%s':"), *PackageContext));
			for (const auto& FormattedOutput : FormattedErrorsAndWarningsList)
			{
				GWarn->Log(NAME_None, ELogVerbosity::Display, LogIndentation + FormattedOutput);
			}
		}
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Error)
		{
			++ErrorCount;
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V));
		}
		else if (Verbosity == ELogVerbosity::Warning)
		{
			++WarningCount;
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V));
		}
		else if (Verbosity == ELogVerbosity::Display)
		{
			// Downgrade Display to Log while loading packages
			OriginalWarningContext->Serialize(V, ELogVerbosity::Log, Category);
		}
		else
		{
			// Pass anything else on to GWarn so that it can handle them appropriately
			OriginalWarningContext->Serialize(V, Verbosity, Category);
		}
	}

private:
	int32 ErrorCount;
	int32 WarningCount;
	TArray<FString> FormattedErrorsAndWarningsList;

	FString PackageContext;
	FFeedbackContext* OriginalWarningContext;
};

class FAssetGatherCacheMetrics
{
public:
	FAssetGatherCacheMetrics()
		: CachedAssetCount(0)
		, UncachedAssetCount(0)
	{
		FMemory::Memzero(UncachedAssetBreakdown);
	}

	void CountCachedAsset()
	{
		++CachedAssetCount;
	}

	void CountUncachedAsset(const UGatherTextFromAssetsCommandlet::EPackageLocCacheState InState)
	{
		check(InState != UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Cached);
		++UncachedAssetCount;
		++UncachedAssetBreakdown[(int32)InState];
	}

	void LogMetrics() const
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("%s"), *ToString());
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT("Asset gather cache metrics: %d cached, %d uncached (%d too old, %d no cache or contained bytecode)"), 
			CachedAssetCount, 
			UncachedAssetCount, 
			UncachedAssetBreakdown[(int32)UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Uncached_TooOld], 
			UncachedAssetBreakdown[(int32)UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Uncached_NoCache]
			);
	}

private:
	int32 CachedAssetCount;
	int32 UncachedAssetCount;
	int32 UncachedAssetBreakdown[(int32)UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Cached];
};

#define LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
//UGatherTextFromAssetsCommandlet

const FString UGatherTextFromAssetsCommandlet::UsageText
(
	TEXT("GatherTextFromAssetsCommandlet usage...\r\n")
	TEXT("    <GameName> UGatherTextFromAssetsCommandlet -root=<parsed code root folder> -exclude=<paths to exclude>\r\n")
	TEXT("    \r\n")
	TEXT("    <paths to include> Paths to include. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, everything will be included. \r\n")
	TEXT("    <paths to exclude> Paths to exclude. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, nothing will be excluded.\r\n")
);

UGatherTextFromAssetsCommandlet::UGatherTextFromAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PackagesPerBatchCount(100)
	, MaxMemoryAllowanceBytes(0)
	, bSkipGatherCache(false)
	, ShouldGatherFromEditorOnlyData(false)
	, ShouldExcludeDerivedClasses(false)
{
}

void UGatherTextFromAssetsCommandlet::ProcessGatherableTextDataArray(const TArray<FGatherableTextData>& GatherableTextDataArray)
{
	for (const FGatherableTextData& GatherableTextData : GatherableTextDataArray)
	{
		for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
		{
			if (!TextSourceSiteContext.IsEditorOnly || ShouldGatherFromEditorOnlyData)
			{
				if (TextSourceSiteContext.KeyName.IsEmpty())
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected missing key on asset \"%s\"."), *TextSourceSiteContext.SiteDescription);
					continue;
				}

				static const FLocMetadataObject DefaultMetadataObject;

				FManifestContext Context;
				Context.Key = TextSourceSiteContext.KeyName;
				Context.KeyMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.KeyMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.KeyMetaData)) : nullptr;
				Context.InfoMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.InfoMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.InfoMetaData)) : nullptr;
				Context.bIsOptional = TextSourceSiteContext.IsOptional;
				Context.SourceLocation = TextSourceSiteContext.SiteDescription;
				Context.PlatformName = GetSplitPlatformNameFromPath(TextSourceSiteContext.SiteDescription);

				FLocItem Source(GatherableTextData.SourceData.SourceString);

				GatherManifestHelper->AddSourceText(GatherableTextData.NamespaceName, Source, Context, &TextSourceSiteContext.SiteDescription);
			}
		}
	}
}

void CalculateDependenciesImpl(IAssetRegistry& InAssetRegistry, const FName& InPackageName, TSet<FName>& OutDependencies, TMap<FName, TSet<FName>>& InOutPackageNameToDependencies)
{
	const TSet<FName>* CachedDependencies = InOutPackageNameToDependencies.Find(InPackageName);

	if (!CachedDependencies)
	{
		// Add a dummy entry now to avoid any infinite recursion for this package as we build the dependencies list
		InOutPackageNameToDependencies.Add(InPackageName);

		// Build the complete list of dependencies for this package
		TSet<FName> LocalDependencies;
		{
			TArray<FName> LocalDependenciesArray;
			InAssetRegistry.GetDependencies(InPackageName, LocalDependenciesArray);

			LocalDependencies.Append(LocalDependenciesArray);
			for (const FName& LocalDependency : LocalDependenciesArray)
			{
				CalculateDependenciesImpl(InAssetRegistry, LocalDependency, LocalDependencies, InOutPackageNameToDependencies);
			}
		}

		// Add the real data now
		CachedDependencies = &InOutPackageNameToDependencies.Add(InPackageName, MoveTemp(LocalDependencies));
	}

	check(CachedDependencies);
	OutDependencies.Append(*CachedDependencies);
}

void UGatherTextFromAssetsCommandlet::CalculateDependenciesForPackagesPendingGather()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	TMap<FName, TSet<FName>> PackageNameToDependencies;

	for (FPackagePendingGather& PackagePendingGather : PackagesPendingGather)
	{
		CalculateDependenciesImpl(AssetRegistry, PackagePendingGather.PackageName, PackagePendingGather.Dependencies, PackageNameToDependencies);
	}
}

bool UGatherTextFromAssetsCommandlet::HasExceededMemoryLimit()
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	const uint64 UsedMemory = MemStats.UsedPhysical;
	if (MaxMemoryAllowanceBytes > 0u && UsedMemory >= MaxMemoryAllowanceBytes)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Used memory %d kb exceeded max memory %d kb"), UsedMemory / 1024, MaxMemoryAllowanceBytes / 1024);
		return true;
	}

	return false;
}

void UGatherTextFromAssetsCommandlet::PurgeGarbage(const bool bPurgeReferencedPackages)
{
	check(ObjectsToKeepAlive.Num() == 0);

	TSet<FName> LoadedPackageNames;
	TSet<FName> PackageNamesToKeepAlive;

	if (!bPurgeReferencedPackages)
	{
		// Build a complete list of packages that we still need to keep alive, either because we still 
		// have to process them, or because they're a dependency for something we still have to process
		for (const FPackagePendingGather& PackagePendingGather : PackagesPendingGather)
		{
			PackageNamesToKeepAlive.Add(PackagePendingGather.PackageName);
			PackageNamesToKeepAlive.Append(PackagePendingGather.Dependencies);
		}

		for (TObjectIterator<UPackage> PackageIt; PackageIt; ++PackageIt)
		{
			UPackage* Package = *PackageIt;
			if (PackageNamesToKeepAlive.Contains(Package->GetFName()))
			{
				LoadedPackageNames.Add(Package->GetFName());

				// Keep any requested packages (and their RF_Standalone inners) alive during a call to PurgeGarbage
				ObjectsToKeepAlive.Add(Package);
				ForEachObjectWithOuter(Package, [this](UObject* InPackageInner)
				{
					if (InPackageInner->HasAnyFlags(RF_Standalone))
					{
						ObjectsToKeepAlive.Add(InPackageInner);
					}
				}, true, RF_NoFlags, EInternalObjectFlags::PendingKill);
			}
		}
	}

	CollectGarbage(RF_NoFlags);
	ObjectsToKeepAlive.Reset();

	if (!bPurgeReferencedPackages)
	{
		// Sort the remaining packages to gather so that currently loaded packages are processed first, followed by those with the most dependencies
		// This aims to allow packages to be GC'd as soon as possible once nothing is no longer referencing them as a dependency
		PackagesPendingGather.Sort([&LoadedPackageNames](const FPackagePendingGather& PackagePendingGatherOne, const FPackagePendingGather& PackagePendingGatherTwo)
		{
			const bool bIsPackageOneLoaded = LoadedPackageNames.Contains(PackagePendingGatherOne.PackageName);
			const bool bIsPackageTwoLoaded = LoadedPackageNames.Contains(PackagePendingGatherTwo.PackageName);
			return (bIsPackageOneLoaded == bIsPackageTwoLoaded) 
				? PackagePendingGatherOne.Dependencies.Num() > PackagePendingGatherTwo.Dependencies.Num() 
				: bIsPackageOneLoaded;
		});
	}
}

void UGatherTextFromAssetsCommandlet::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	// Keep any requested objects alive during a call to PurgeGarbage
	UGatherTextFromAssetsCommandlet* This = CastChecked<UGatherTextFromAssetsCommandlet>(InThis);
	Collector.AddReferencedObjects(This->ObjectsToKeepAlive);
}

bool IsGatherableTextDataIdentical(const TArray<FGatherableTextData>& GatherableTextDataArrayOne, const TArray<FGatherableTextData>& GatherableTextDataArrayTwo)
{
	struct FSignificantGatherableTextData
	{
		FLocKey Identity;
		FString SourceString;
	};

	auto ExtractSignificantGatherableTextData = [](const TArray<FGatherableTextData>& InGatherableTextDataArray)
	{
		TArray<FSignificantGatherableTextData> SignificantGatherableTextDataArray;

		for (const FGatherableTextData& GatherableTextData : InGatherableTextDataArray)
		{
			for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
			{
				SignificantGatherableTextDataArray.Add({ FString::Printf(TEXT("%s:%s"), *GatherableTextData.NamespaceName, *TextSourceSiteContext.KeyName), GatherableTextData.SourceData.SourceString });
			}
		}

		SignificantGatherableTextDataArray.Sort([](const FSignificantGatherableTextData& SignificantGatherableTextDataOne, const FSignificantGatherableTextData& SignificantGatherableTextDataTwo)
		{
			return SignificantGatherableTextDataOne.Identity < SignificantGatherableTextDataTwo.Identity;
		});

		return SignificantGatherableTextDataArray;
	};

	TArray<FSignificantGatherableTextData> SignificantGatherableTextDataArrayOne = ExtractSignificantGatherableTextData(GatherableTextDataArrayOne);
	TArray<FSignificantGatherableTextData> SignificantGatherableTextDataArrayTwo = ExtractSignificantGatherableTextData(GatherableTextDataArrayTwo);

	if (SignificantGatherableTextDataArrayOne.Num() != SignificantGatherableTextDataArrayTwo.Num())
	{
		return false;
	}

	// These arrays are sorted by identity, so everything should match as we iterate through the array
	// If it doesn't, then these caches aren't identical
	for (int32 Idx = 0; Idx < SignificantGatherableTextDataArrayOne.Num(); ++Idx)
	{
		const FSignificantGatherableTextData& SignificantGatherableTextDataOne = SignificantGatherableTextDataArrayOne[Idx];
		const FSignificantGatherableTextData& SignificantGatherableTextDataTwo = SignificantGatherableTextDataArrayTwo[Idx];

		if (SignificantGatherableTextDataOne.Identity != SignificantGatherableTextDataTwo.Identity)
		{
			return false;
		}

		if (!SignificantGatherableTextDataOne.SourceString.Equals(SignificantGatherableTextDataTwo.SourceString, ESearchCase::CaseSensitive))
		{
			return false;
		}
	}

	return true;
}

int32 UGatherTextFromAssetsCommandlet::Main(const FString& Params)
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	FString GatherTextConfigPath;
	FString SectionName;
	if (!GetConfigurationScript(ParamVals, GatherTextConfigPath, SectionName))
	{
		return -1;
	}

	if (!ConfigureFromScript(GatherTextConfigPath, SectionName))
	{
		return -1;
	}

	// Get destination path
	FString DestinationPath;
	if (!GetPathFromConfig(*SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath))
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Add any manifest dependencies if they were provided
	{
		bool HasFailedToAddManifestDependency = false;
		for (const FString& ManifestDependency : ManifestDependenciesList)
		{
			FText OutError;
			if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
			{
				UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("The GatherTextFromAssets commandlet couldn't load the specified manifest dependency: '%'. %s"), *ManifestDependency, *OutError.ToString());
				HasFailedToAddManifestDependency = true;
			}
		}
		if (HasFailedToAddManifestDependency)
		{
			return -1;
		}
	}

	// Preload necessary modules.
	{
		bool HasFailedToPreloadAnyModules = false;
		for (const FString& ModuleName : ModulesToPreload)
		{
			EModuleLoadResult ModuleLoadResult;
			FModuleManager::Get().LoadModuleWithFailureReason(*ModuleName, ModuleLoadResult);

			if (ModuleLoadResult != EModuleLoadResult::Success)
			{
				HasFailedToPreloadAnyModules = true;
				continue;
			}
		}

		if (HasFailedToPreloadAnyModules)
		{
			return -1;
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);
	TArray<FAssetData> AssetDataArray;

	{
		FARFilter FirstPassFilter;

		// Filter object paths to only those in any of the specified collections.
		{
			bool HasFailedToGetACollection = false;
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			ICollectionManager& CollectionManager = CollectionManagerModule.Get();
			for (const FString& CollectionName : CollectionFilters)
			{
				if (!CollectionManager.GetObjectsInCollection(FName(*CollectionName), ECollectionShareType::CST_All, FirstPassFilter.ObjectPaths, ECollectionRecursionFlags::SelfAndChildren))
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("Failed get objects in specified collection: %s"), *CollectionName);
					HasFailedToGetACollection = true;
				}
			}
			if (HasFailedToGetACollection)
			{
				return -1;
			}
		}

		// Filter out any objects of the specified classes and their children at this point.
		if (ShouldExcludeDerivedClasses)
		{
			FirstPassFilter.bRecursiveClasses = true;
			FirstPassFilter.ClassNames.Add(TEXT("Object"));
			for (const FString& ExcludeClassName : ExcludeClassNames)
			{
				// Note: Can't necessarily validate these class names here, as the class may be a generated blueprint class that hasn't been loaded yet.
				FirstPassFilter.RecursiveClassesExclusionSet.Add(*ExcludeClassName);
			}
		}

		// Apply filter if valid to do so, get all assets otherwise.
		if (FirstPassFilter.IsEmpty())
		{
			AssetRegistry.GetAllAssets(AssetDataArray);
		}
		else
		{
			AssetRegistry.GetAssets(FirstPassFilter, AssetDataArray);
		}
	}

	if (!ShouldExcludeDerivedClasses)
	{
		// Filter out any objects of the specified classes.
		FARFilter ExcludeExactClassesFilter;
		ExcludeExactClassesFilter.bRecursiveClasses = false;
		for (const FString& ExcludeClassName : ExcludeClassNames)
		{
			// Note: Can't necessarily validate these class names here, as the class may be a generated blueprint class that hasn't been loaded yet.
			ExcludeExactClassesFilter.ClassNames.Add(*ExcludeClassName);
		}

		// Reapply filter over the current set of assets.
		if (!ExcludeExactClassesFilter.IsEmpty())
		{
			// NOTE: The filter applied is actually the inverse, due to API limitations, so the resultant set must be removed from the current set.
			TArray<FAssetData> AssetsToExclude = AssetDataArray;
			AssetRegistry.RunAssetsThroughFilter(AssetsToExclude, ExcludeExactClassesFilter);
			AssetDataArray.RemoveAll([&](const FAssetData& AssetData)
			{
				return AssetsToExclude.Contains(AssetData);
			});
		}
	}

	// Note: AssetDataArray now contains all assets in the specified collections that are not instances of the specified excluded classes.

	const FFuzzyPathMatcher FuzzyPathMatcher = FFuzzyPathMatcher(IncludePathFilters, ExcludePathFilters);
	AssetDataArray.RemoveAll([&](const FAssetData& PartiallyFilteredAssetData) -> bool
	{
		FString PackageFilePath;
		if (!FPackageName::FindPackageFileWithoutExtension(FPackageName::LongPackageNameToFilename(PartiallyFilteredAssetData.PackageName.ToString()), PackageFilePath))
		{
			return true;
		}
		PackageFilePath = FPaths::ConvertRelativePathToFull(PackageFilePath);
		const FString PackageFileName = FPaths::GetCleanFilename(PackageFilePath);

		// Filter out assets whose package file names DO NOT match any of the package file name filters.
		{
			bool HasPassedAnyFileNameFilter = false;
			for (const FString& PackageFileNameFilter : PackageFileNameFilters)
			{
				if (PackageFileName.MatchesWildcard(PackageFileNameFilter))
				{
					HasPassedAnyFileNameFilter = true;
					break;
				}
			}
			if (!HasPassedAnyFileNameFilter)
			{
				return true;
			}
		}

		// Filter out assets whose package file paths do not pass the "fuzzy path" filters.
		if (FuzzyPathMatcher.TestPath(PackageFilePath) != FFuzzyPathMatcher::Included)
		{
			return true;
		}

		return false;
	});

	if (AssetDataArray.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("No assets matched the specified criteria."));
		return 0;
	}

	// Collect the basic information about the packages that we're going to gather from
	{
		// Collapse the assets down to a set of packages
		TSet<FName> PackageNamesToGather;
		PackageNamesToGather.Reserve(AssetDataArray.Num());
		for (const FAssetData& AssetData : AssetDataArray)
		{
			PackageNamesToGather.Add(AssetData.PackageName);
		}
		AssetDataArray.Empty();

		// Build the basic information for the packages to gather (dependencies are filled in later once we've processed cached packages)
		PackagesPendingGather.Reserve(PackageNamesToGather.Num());
		for (const FName& PackageNameToGather : PackageNamesToGather)
		{
			FString PackageFilename;
			if (!FPackageName::FindPackageFileWithoutExtension(FPackageName::LongPackageNameToFilename(PackageNameToGather.ToString()), PackageFilename))
			{
				continue;
			}
			PackageFilename = FPaths::ConvertRelativePathToFull(PackageFilename);

			FPackagePendingGather& PackagePendingGather = PackagesPendingGather[PackagesPendingGather.AddDefaulted()];
			PackagePendingGather.PackageName = PackageNameToGather;
			PackagePendingGather.PackageFilename = MoveTemp(PackageFilename);
			PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Cached;
		}
	}

	FAssetGatherCacheMetrics AssetGatherCacheMetrics;
	TMap<FString, FName> AssignedPackageLocalizationIds;

	// Process all packages that do not need to be loaded. Remove processed packages from the list.
	PackagesPendingGather.RemoveAll([&](FPackagePendingGather& PackagePendingGather) -> bool
	{
		TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*PackagePendingGather.PackageFilename));
		if (!FileReader)
		{
			return false;
		}

		// Read package file summary from the file.
		FPackageFileSummary PackageFileSummary;
		*FileReader << PackageFileSummary;

		// Track the package localization ID of this package (if known) and detect duplicates
		if (!PackageFileSummary.LocalizationId.IsEmpty())
		{
			if (const FName* ExistingLongPackageName = AssignedPackageLocalizationIds.Find(PackageFileSummary.LocalizationId))
			{
				UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Package '%s' and '%s' have the same localization ID (%s). Please reset one of these (Asset Localization -> Reset Localization ID) to avoid conflicts."), *PackagePendingGather.PackageName.ToString(), *ExistingLongPackageName->ToString(), *PackageFileSummary.LocalizationId);
			}
			else
			{
				AssignedPackageLocalizationIds.Add(PackageFileSummary.LocalizationId, PackagePendingGather.PackageName);
			}
		}

		PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Cached;

		// Have we been asked to skip the cache of text that exists in the header of newer packages?
		if (bSkipGatherCache && PackageFileSummary.GetFileVersionUE4() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
		{
			// Fallback on the old package flag check.
			if (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather)
			{
				PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Uncached_NoCache;
			}
		}

		const FCustomVersion* const EditorVersion = PackageFileSummary.GetCustomVersionContainer().GetVersion(FEditorObjectVersion::GUID);

		// Packages not resaved since localization gathering flagging was added to packages must be loaded.
		if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING)
		{
			PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Uncached_TooOld;
		}
		// Package not resaved since gatherable text data was added to package headers must be loaded, since their package header won't contain pregathered text data.
		else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_SERIALIZE_TEXT_IN_PACKAGES || (!EditorVersion || EditorVersion->Version < FEditorObjectVersion::GatheredTextEditorOnlyPackageLocId))
		{
			// Fallback on the old package flag check.
			if (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather)
			{
				PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Uncached_TooOld;
			}
		}
		else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_DIALOGUE_WAVE_NAMESPACE_AND_CONTEXT_CHANGES)
		{
			TArray<FAssetData> AllAssetDataInSamePackage;
			AssetRegistry.GetAssetsByPackageName(PackagePendingGather.PackageName, AllAssetDataInSamePackage);
			for (const FAssetData& AssetData : AllAssetDataInSamePackage)
			{
				if (AssetData.AssetClass == UDialogueWave::StaticClass()->GetFName())
				{
					PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Uncached_TooOld;
				}
			}
		}

		// If this package doesn't have any cached data, then we have to load it for gather
		if (PackageFileSummary.GetFileVersionUE4() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES && PackageFileSummary.GatherableTextDataOffset == 0 && (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather))
		{
			PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Uncached_NoCache;
		}

		if (PackagePendingGather.PackageLocCacheState != EPackageLocCacheState::Cached)
		{
			AssetGatherCacheMetrics.CountUncachedAsset(PackagePendingGather.PackageLocCacheState);
			return false;
		}

		// Process packages that don't require loading to process.
		if (PackageFileSummary.GatherableTextDataOffset > 0)
		{
			AssetGatherCacheMetrics.CountCachedAsset();

			FileReader->Seek(PackageFileSummary.GatherableTextDataOffset);

			PackagePendingGather.GatherableTextDataArray.SetNum(PackageFileSummary.GatherableTextDataCount);
			for (int32 GatherableTextDataIndex = 0; GatherableTextDataIndex < PackageFileSummary.GatherableTextDataCount; ++GatherableTextDataIndex)
			{
				(*FileReader) << PackagePendingGather.GatherableTextDataArray[GatherableTextDataIndex];
			}

			ProcessGatherableTextDataArray(PackagePendingGather.GatherableTextDataArray);
		}

		// If we're reporting or fixing assets with a stale gather cache then we still need to load this 
		// package in order to do that, but the PackageLocCacheState prevents it being gathered again
		if (bReportStaleGatherCache || bFixStaleGatherCache)
		{
			check(PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Cached);
			return false;
		}

		return true;
	});

	AssetGatherCacheMetrics.LogMetrics();

	const int32 PackageCount = PackagesPendingGather.Num();
	const int32 BatchCount = PackageCount / PackagesPerBatchCount + (PackageCount % PackagesPerBatchCount > 0 ? 1 : 0); // Add an extra batch for any remainder if necessary.
	if (PackageCount > 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loading %i packages in %i batches of %i."), PackageCount, BatchCount, PackagesPerBatchCount);
	}
	FLoadPackageLogOutputRedirector LogOutputRedirector;

	CalculateDependenciesForPackagesPendingGather();

	TArray<FName> PackagesWithStaleGatherCache;

	// Process the packages in batches
	TArray<FGatherableTextData> GatherableTextDataArray;
	for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
	{
		int32 PackagesInThisBatch = 0;
		int32 FailuresInThisBatch = 0;

		// Collect garbage before beginning to load packages for this batch
		// This also sorts the list of packages into the best processing order
		PurgeGarbage(/*bPurgeReferencedPackages*/false);

		// Process this batch
		const int32 PackagesToProcessThisBatch = FMath::Min(PackagesPendingGather.Num(), PackagesPerBatchCount);
		for (int32 PackageIndex = 0; PackageIndex < PackagesToProcessThisBatch; ++PackageIndex)
		{
			const FPackagePendingGather& PackagePendingGather = PackagesPendingGather[PackageIndex];
			const FString PackageNameStr = PackagePendingGather.PackageName.ToString();

			UE_LOG(LogGatherTextFromAssetsCommandlet, Verbose, TEXT("Loading package: '%s'."), *PackageNameStr);

			UPackage* Package = nullptr;
			{
				FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, PackageNameStr);
				Package = LoadPackage(nullptr, *PackageNameStr, LOAD_NoWarn | LOAD_Quiet);
			}

			if (!Package)
			{
				++FailuresInThisBatch;
				continue;
			}
			
			++PackagesInThisBatch;

			// Because packages may not have been resaved after this flagging was implemented, we may have added packages to load that weren't flagged - potential false positives.
			// The loading process should have reflagged said packages so that only true positives will have this flag.
			if (Package->RequiresLocalizationGather())
			{
				// Gathers from the given package
				EPropertyLocalizationGathererResultFlags GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
				FPropertyLocalizationDataGatherer(GatherableTextDataArray, Package, GatherableTextResultFlags);

				bool bSavePackage = false;

				// Optionally check to see whether the clean gather we did is in-sync with the gather cache and deal with it accordingly
				if ((bReportStaleGatherCache || bFixStaleGatherCache) && PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Cached)
				{
					// Look for any structurally significant changes (missing, added, or changed texts) in the cache
					// Ignore insignificant things (like source changes caused by assets moving or being renamed)
					if (!IsGatherableTextDataIdentical(GatherableTextDataArray, PackagePendingGather.GatherableTextDataArray))
					{
						PackagesWithStaleGatherCache.Add(PackagePendingGather.PackageName);
						
						if (bFixStaleGatherCache)
						{
							bSavePackage = true;
						}
					}
				}

				// Optionally save the package if it is missing a gather cache
				if (bFixMissingGatherCache && PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Uncached_TooOld)
				{
					bSavePackage = true;
				}

				// Re-save the package to attempt to fix it?
				if (bSavePackage)
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Resaving package: '%s'."), *PackageNameStr);

					bool bSavedPackage = false;
					{
						FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, PackageNameStr);
						bSavedPackage = FLocalizedAssetSCCUtil::SavePackageWithSCC(SourceControlInfo, Package, PackagePendingGather.PackageFilename);
					}

					if (!bSavedPackage)
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("Failed to resave package: '%s'."), *PackageNameStr);
					}
				}

				// This package may have already been cached in cases where we're reporting or fixing assets with a stale gather cache
				// This check prevents it being gathered a second time
				if (PackagePendingGather.PackageLocCacheState != EPackageLocCacheState::Cached)
				{
					ProcessGatherableTextDataArray(GatherableTextDataArray);
				}

				GatherableTextDataArray.Reset();
			}

			if (HasExceededMemoryLimit())
			{
				// Over the memory limit, perform a full purge
				PurgeGarbage(/*bPurgeReferencedPackages*/true);
			}
		}

		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loaded %i packages in batch %i of %i. %i failed."), PackagesInThisBatch, BatchIndex + 1, BatchCount, FailuresInThisBatch);

		// Remove the processed packages
		PackagesPendingGather.RemoveAt(0, PackagesToProcessThisBatch, /*bAllowShrinking*/false);
	}
	check(PackagesPendingGather.Num() == 0);

	PackagesWithStaleGatherCache.Sort();

	if (bReportStaleGatherCache)
	{
		FString StaleGatherCacheReport;
		for (const FName& PackageWithStaleGatherCache : PackagesWithStaleGatherCache)
		{
			StaleGatherCacheReport += PackageWithStaleGatherCache.ToString();
			StaleGatherCacheReport += TEXT("\n");
		}

		const FString StaleGatherCacheReportFilename = DestinationPath / TEXT("StaleGatherCacheReport.txt");
		const bool bStaleGatherCacheReportSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, StaleGatherCacheReportFilename, [&StaleGatherCacheReport](const FString& InSaveFileName) -> bool
		{
			return FFileHelper::SaveStringToFile(StaleGatherCacheReport, *InSaveFileName, FFileHelper::EEncodingOptions::ForceUTF8);
		});

		if (!bStaleGatherCacheReportSaved)
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("Failed to save report: '%s'."), *StaleGatherCacheReportFilename);
		}
	}

	return 0;
}

bool UGatherTextFromAssetsCommandlet::GetConfigurationScript(const TMap<FString, FString>& InCommandLineParameters, FString& OutFilePath, FString& OutStepSectionName)
{
	//Set config file
	const FString* ParamVal = InCommandLineParameters.Find(FString(TEXT("Config")));
	if (ParamVal)
	{
		OutFilePath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config specified."));
		return false;
	}

	//Set config section
	ParamVal = InCommandLineParameters.Find(FString(TEXT("Section")));
	if (ParamVal)
	{
		OutStepSectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config section specified."));
		return false;
	}

	return true;
}

bool UGatherTextFromAssetsCommandlet::ConfigureFromScript(const FString& GatherTextConfigPath, const FString& SectionName)
{
	bool HasFatalError = false;

	// Modules to Preload
	GetStringArrayFromConfig(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	// IncludePathFilters
	GetPathArrayFromConfig(*SectionName, TEXT("IncludePathFilters"), IncludePathFilters, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			IncludePathFilters.Append(IncludePaths);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("IncludePaths detected in section %s. IncludePaths is deprecated, please use IncludePathFilters."), *SectionName);
		}
	}

	if (IncludePathFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No include path filters in section %s."), *SectionName);
		HasFatalError = true;
	}

	// Collections
	GetStringArrayFromConfig(*SectionName, TEXT("CollectionFilters"), CollectionFilters, GatherTextConfigPath);
	for (const FString& CollectionName : CollectionFilters)
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		ICollectionManager& CollectionManager = CollectionManagerModule.Get();

		const bool DoesCollectionExist = CollectionManager.CollectionExists(FName(*CollectionName), ECollectionShareType::CST_All);
		if (!DoesCollectionExist)
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("Failed to find a collection with name \"%s\", collection does not exist."), *CollectionName);
			HasFatalError = true;
		}
	}

	// ExcludePathFilters
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("ExcludePaths detected in section %s. ExcludePaths is deprecated, please use ExcludePathFilters."), *SectionName);
		}
	}

	// PackageNameFilters
	GetStringArrayFromConfig(*SectionName, TEXT("PackageFileNameFilters"), PackageFileNameFilters, GatherTextConfigPath);

	// PackageExtensions (DEPRECATED)
	{
		TArray<FString> PackageExtensions;
		GetStringArrayFromConfig(*SectionName, TEXT("PackageExtensions"), PackageExtensions, GatherTextConfigPath);
		if (PackageExtensions.Num())
		{
			PackageFileNameFilters.Append(PackageExtensions);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("PackageExtensions detected in section %s. PackageExtensions is deprecated, please use PackageFileNameFilters."), *SectionName);
		}
	}

	if (PackageFileNameFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No package file name filters in section %s."), *SectionName);
		HasFatalError = true;
	}

	// Recursive asset class exclusion
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldExcludeDerivedClasses"), ShouldExcludeDerivedClasses, GatherTextConfigPath))
	{
		ShouldExcludeDerivedClasses = false;
	}

	// Asset class exclude
	GetStringArrayFromConfig(*SectionName, TEXT("ExcludeClasses"), ExcludeClassNames, GatherTextConfigPath);

	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE4 itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), ShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		ShouldGatherFromEditorOnlyData = false;
	}

	auto ReadBoolFlagWithFallback = [this, &SectionName, &GatherTextConfigPath](const TCHAR* FlagName, bool& OutValue)
	{
		OutValue = FParse::Param(FCommandLine::Get(), FlagName);
		if (!OutValue)
		{
			GetBoolFromConfig(*SectionName, FlagName, OutValue, GatherTextConfigPath);
		}
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("%s: %s"), FlagName, OutValue ? TEXT("true") : TEXT("false"));
	};

	ReadBoolFlagWithFallback(TEXT("SkipGatherCache"), bSkipGatherCache);
	ReadBoolFlagWithFallback(TEXT("ReportStaleGatherCache"), bReportStaleGatherCache);
	ReadBoolFlagWithFallback(TEXT("FixStaleGatherCache"), bFixStaleGatherCache);
	ReadBoolFlagWithFallback(TEXT("FixMissingGatherCache"), bFixMissingGatherCache);

	// Read some settings from the editor config
	{
		int32 MaxMemoryAllowanceInMB = 0;
		GConfig->GetInt(TEXT("GatherTextFromAssets"), TEXT("MaxMemoryAllowance"), MaxMemoryAllowanceInMB, GEditorIni);
		MaxMemoryAllowanceInMB = FMath::Max(MaxMemoryAllowanceInMB, 0);
		MaxMemoryAllowanceBytes = MaxMemoryAllowanceInMB * 1024LL * 1024LL;

		PackagesPerBatchCount = 100;
		GConfig->GetInt(TEXT("GatherTextFromAssets"), TEXT("BatchCount"), PackagesPerBatchCount, GEditorIni);
	}

	return !HasFatalError;
}

#undef LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////

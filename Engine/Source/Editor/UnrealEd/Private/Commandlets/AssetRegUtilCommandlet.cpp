// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AssetRegUtilCommandlet.cpp: General-purpose commandlet for anything which
	makes integral use of the asset registry.
=============================================================================*/

#include "Commandlets/AssetRegUtilCommandlet.h"
#include "PackageHelperFunctions.h"
#include "Engine/Texture.h"
#include "Logging/LogMacros.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"
#include "Stats/StatsMisc.h"
#include "AssetRegistryModule.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY(LogAssetRegUtil);

const static FName NAME_UnresolvedPackageName = FName(TEXT("UnresolvedPackageName"));

const static FName NAME_uasset(TEXT("uasset"));
const static FName NAME_umap(TEXT("umap"));
const static FName NAME_uexp(TEXT("uexp"));
const static FName NAME_ubulk(TEXT("ubulk"));
const static FName NAME_uptnl(TEXT("uptnl"));


struct FSortableDependencyEntry
{
	FSortableDependencyEntry(const FName& InLongPackageName, const FName& InFilePath, const FName& InExtension, const int32 InDepSet, const int32 InDepHierarchy, const int32 InDepOrder, bool InHasDependencies, TSet<FName> &&InClasses)
		: LongPackageName(InLongPackageName)
		, FilePath(InFilePath)
		, Extension(InExtension)
		, Classes(MoveTemp(InClasses))
		, DepSet(InDepSet)
		, DepHierarchy(InDepHierarchy)
		, DepOrder(InDepOrder)
		, bHasDependencies(InHasDependencies)
		, bIsAsset(true)
	{ }

	// case for packages which arn't uassets
	FSortableDependencyEntry(const FName& InFilePath, const FName& InExtension, const int32 InDepSet)
		: LongPackageName(NAME_UnresolvedPackageName)
		, FilePath(InFilePath)
		, Extension( InExtension )
		, DepSet(InDepSet)
		, DepHierarchy(0)
		, DepOrder(0)
		, bHasDependencies(false)
		, bIsAsset(false)
	{ }

	FName LongPackageName;
	FName FilePath;
	FName Extension;
	TSet<FName> Classes;
	int32 DepSet;
	int32 DepHierarchy;
	int32 DepOrder;
	bool bHasDependencies;
	bool bIsAsset;
};

/*
We want exports to be sorted in reverse hierarchical order, to replicate this kind of ordering as seen in a natural OpenOrder log:

	"../../../engine/Content/EngineMaterials/WorldGridMaterial.uasset" 274
	"../../../engine/Content/EngineMaterials/T_Default_Material_Grid_N.uasset" 275
	"../../../engine/Content/EngineMaterials/T_Default_Material_Grid_M.uasset" 276
	"../../../engine/Content/Functions/Engine_MaterialFunctions01/Opacity/CameraDepthFade.uasset" 277
	...
	"../../../engine/Content/EngineMaterials/T_Default_Material_Grid_N.uexp" 432
	"../../../engine/Content/EngineMaterials/T_Default_Material_Grid_M.uexp" 433
	"../../../engine/Content/Functions/Engine_MaterialFunctions01/Opacity/CameraDepthFade.uexp" 434
	"../../../engine/Content/EngineMaterials/WorldGridMaterial.uexp" 435
*/

struct FSortableDependencySortForHeaders
{
	bool operator()(const FSortableDependencyEntry& A, const FSortableDependencyEntry& B) const
	{
		return (A.DepHierarchy == B.DepHierarchy) ? A.DepOrder < B.DepOrder : A.DepHierarchy < B.DepHierarchy;
	}
};

struct FSortableDependencySortForExports
{
	bool operator()(const FSortableDependencyEntry& A, const FSortableDependencyEntry& B) const
	{
		return (A.DepHierarchy == B.DepHierarchy) ? A.DepOrder < B.DepOrder : A.DepHierarchy > B.DepHierarchy;
	}
};


struct FSortableDependencySort
{
	FSortableDependencySort(const TArray<FName>& InGroupExtensions, const TArray<FName>& InGroupClasses, const TMap<FName, int32> InExtensionPriority)
		: GroupExtensions(InGroupExtensions)
		, GroupClasses(InGroupClasses)
		, ExtensionPriority(InExtensionPriority)
	{

	}

	const TArray<FName>& GroupExtensions;
	const TArray<FName>& GroupClasses;
	const TMap<FName, int32> ExtensionPriority;

	bool operator()(const FSortableDependencyEntry& A, const FSortableDependencyEntry& B) const
	{
		// we want to sort everything in the order it came in on primarily (Ie the DepSet).
		bool bIsAExtensionGrouped = GroupExtensions.Contains(A.Extension);
		bool bIsBExtensionGrouped = GroupExtensions.Contains(B.Extension);

		// the extensions which are grouped we want to primarily sort on the grouping
		if (bIsAExtensionGrouped != bIsBExtensionGrouped)
		{
			return bIsAExtensionGrouped < bIsBExtensionGrouped;
		}


		bool bIsAClassGrouped = false;
		bool bIsBClassGrouped = false;
		FName AClass = NAME_None;
		FName BClass = NAME_None;
		for (const FName& Class : GroupClasses)
		{
			if (A.Classes.Contains(Class))
			{
				AClass = Class;
				bIsAClassGrouped = true;
			}
			if (B.Classes.Contains(Class))
			{
				BClass = Class;
				bIsBClassGrouped = true;
			}
		}
		if (bIsAClassGrouped != bIsBClassGrouped)
		{
			return bIsAClassGrouped < bIsBClassGrouped;
		}

		if ((AClass != BClass) && bIsAClassGrouped && bIsBClassGrouped)
		{
			return AClass > BClass;
		}

		if (A.DepSet != B.DepSet)
		{
			return A.DepSet < B.DepSet;
		}

		const int32 *AExtPriority = ExtensionPriority.Find(A.Extension);
		const int32 *BExtPriority = ExtensionPriority.Find(B.Extension);


		if (!AExtPriority || !BExtPriority)
		{
			if (!AExtPriority && !BExtPriority)
			{
				return A.DepHierarchy == B.DepHierarchy ? A.DepOrder < B.DepOrder : A.DepHierarchy < B.DepHierarchy;
			}

			if (!AExtPriority)
			{
				return true;
			}
			return false;
		}

		if (*AExtPriority != *BExtPriority)
		{
			return *AExtPriority < *BExtPriority;
		}

		if (A.DepHierarchy != B.DepHierarchy)
		{
			if (A.Extension == NAME_uexp) // the case of uexp we actually want to reverse the hierarchy order
			{
				return A.DepHierarchy > B.DepHierarchy;
			}
			return A.DepHierarchy < B.DepHierarchy;
		}

		return A.DepOrder < B.DepOrder;

		// now everything else goes on either normal dependency order or reverse
	}
};



UAssetRegUtilCommandlet::UAssetRegUtilCommandlet( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}

void UAssetRegUtilCommandlet::RecursivelyGrabDependencies(TArray<FSortableDependencyEntry>& OutSortableDependencies,
                                                          const int32& DepSet, int32& DepOrder, int32 DepHierarchy, TSet<FName>& ProcessedFiles, const TSet<FName>& OriginalSet, const FName& FilePath, const FName& PackageFName, const TArray<FName>& FilterByClasses )
{
	bool bHasDependencies = false;
	//now walk the dependency tree for everything under this package
	TArray<FName> Dependencies;
	AssetRegistry->GetDependencies(PackageFName, Dependencies);

	TArray<FAssetData> AssetsData;
	AssetRegistry->GetAssetsByPackageName(PackageFName, AssetsData, true);
	TSet<FName> Classes;
	Classes.Reserve(AssetsData.Num());
	for (const FAssetData& AssetData : AssetsData)
	{
		Classes.Add(AssetData.AssetClass);
		TArray<FName> AncestorClasses;
		AssetRegistry->GetAncestorClassNames(AssetData.AssetClass, AncestorClasses);
		Classes.Append(AncestorClasses);
	}
	TSet<FName> FilteredClasses;
	for (const FName& FilterClass : FilterByClasses)
	{
		if (Classes.Contains(FilterClass))
		{
			FilteredClasses.Add(FilterClass);
		}
	}

	//keeping a simple path-only set around for the current hierarchy, so things don't get too slow if we end up unrolling a massive dependency tree.
	ProcessedFiles.Add(FilePath);
	FName ExtensionFName = FName(*FPaths::GetExtension(FilePath.ToString()));
	OutSortableDependencies.Add(FSortableDependencyEntry(PackageFName, FilePath, ExtensionFName, DepSet, DepHierarchy, DepOrder, Dependencies.Num()>0, MoveTemp(FilteredClasses)));

	++DepOrder;

	if (Dependencies.Num()>0)
	{
		//walk the dependencies in reverse order akin to how headers tend to be arranged in current load orders
		for (int32 DependencyIndex = Dependencies.Num() - 1; DependencyIndex >= 0; --DependencyIndex)
		{
			const FName& DepPackageName = Dependencies[DependencyIndex];
			const FString DepFilePath = FPackageName::LongPackageNameToFilename(DepPackageName.ToString(), TEXT(".uasset")).ToLower();
			const FName DepPathFName = FName(*DepFilePath);
			//if the package is in the main set, we already walked its dependencies, so we can stop early.
			if (!ProcessedFiles.Contains(DepPathFName) && OriginalSet.Contains(DepPathFName))
			{
				RecursivelyGrabDependencies(OutSortableDependencies, DepSet, DepOrder, DepHierarchy + 1, ProcessedFiles, OriginalSet, DepPathFName, DepPackageName, FilterByClasses);
			}
		}
	}
}

void UAssetRegUtilCommandlet::ReorderOrderFile(const FString& OrderFilePath, const FString& ReorderFileOutPath)
{
	UE_LOG(LogAssetRegUtil, Display, TEXT("Parsing order file: %s"), *OrderFilePath);
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *OrderFilePath))
	{
		//just parsing the list into a set while discarding order values, as we can rely on iteration order matching added order
		TSet<FName> OriginalEntrySet;

		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), true);

		//parse each entry out of the order list. adapted from UnrealPak code, might want to unify somewhere.
		OriginalEntrySet.Reserve(Lines.Num());
		for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); ++EntryIndex)
		{
			Lines[EntryIndex].ReplaceInline(TEXT("\r"), TEXT(""));
			Lines[EntryIndex].ReplaceInline(TEXT("\n"), TEXT(""));
			//discard the order number, assuming the list is in-order and has no special bits in use at this stage.
			int32 QuoteIndex;
			if (Lines[EntryIndex].FindLastChar('"', QuoteIndex))
			{
				FString ReadNum = Lines[EntryIndex].RightChop(QuoteIndex + 1);
				Lines[EntryIndex] = Lines[EntryIndex].Left(QuoteIndex + 1);
				//verify our expectations about the order, just in case something changes on the OpenOrder generation side
				ReadNum.TrimStartInline();
				if (ReadNum.IsNumeric())
				{
					const int32 ExplicitOrder = FCString::Atoi(*ReadNum);
					if (ExplicitOrder != EntryIndex + 1)
					{
						UE_LOG(LogAssetRegUtil, Warning, TEXT("Unexpected order: %i vs %i"), ExplicitOrder, EntryIndex + 1);
					}
				}
			}

			Lines[EntryIndex] = Lines[EntryIndex].TrimQuotes();

			const FString EntryPath = *Lines[EntryIndex].ToLower();
			const FName EntryFName = FName(*EntryPath);
			OriginalEntrySet.Add(EntryFName);
		}

		UE_LOG(LogAssetRegUtil, Display, TEXT("Generating new file order via Asset Registry."));

		TArray<FSortableDependencyEntry> UnsortedEntries;
		UnsortedEntries.Empty(OriginalEntrySet.Num());

		// quick elimination tset
		TSet<FName> ProcessedFiles;
		ProcessedFiles.Reserve(OriginalEntrySet.Num());

		TSet<FName> AssetExtensions;
		AssetExtensions.Add(NAME_uasset);
		AssetExtensions.Add(NAME_umap);

		TSet<FName> ExtraAssetExtensions;
		ExtraAssetExtensions.Add(NAME_uexp);
		ExtraAssetExtensions.Add(NAME_ubulk);
		ExtraAssetExtensions.Add(NAME_uptnl);

		TArray<FName> FilterByClasses;
		FilterByClasses.Add(FName(TEXT("Material")));
		FilterByClasses.Add(FName(TEXT("MaterialFunction")));
		FilterByClasses.Add(FName(TEXT("MaterialInstance")));
		FilterByClasses.Add(FName(TEXT("BlueprintCore")));
		FilterByClasses.Add(FName(TEXT("ParticleEmitter")));
		FilterByClasses.Add(FName(TEXT("ParticleModule")));


		int32 DepSet = 0; // this is the root set for the dependency (i.e files with a number probably came from the same core dependency)
		for (const FName& FilePath : OriginalEntrySet)
		{
			++DepSet;
			if (!ProcessedFiles.Contains(FilePath))
			{
				const FString FilePathExtension = FPaths::GetExtension(FilePath.ToString());
				const FName FNameExtension = FName(*FilePathExtension);
				if (AssetExtensions.Contains( FNameExtension) )
				{
					FString PackageName;
					if (FPackageName::TryConvertFilenameToLongPackageName(FilePath.ToString(), PackageName))
					{
						FName PackageFName(*PackageName);
						int32 DependencyOrderIndex = 0;
						RecursivelyGrabDependencies(UnsortedEntries, DepSet, DependencyOrderIndex, 0, ProcessedFiles, OriginalEntrySet, FilePath, PackageFName, FilterByClasses);
					}
					else
					{
						//special case for packages outside of our mounted paths, pick up the header and the export without any dependency-gathering.
						ProcessedFiles.Add(FilePath);
						UnsortedEntries.Add(FSortableDependencyEntry(NAME_UnresolvedPackageName, FilePath, FNameExtension, DepSet, 0, 0, false, TSet<FName>()));
					}
				}
				else if ( ExtraAssetExtensions.Contains(FNameExtension) == false )
				{
					//not a package, no need to do special sorting/handling for headers and exports
					UnsortedEntries.Add(FSortableDependencyEntry(FilePath, FNameExtension, DepSet));
					ProcessedFiles.Add(FilePath);
				}
			}
		}

		for (int32 I = UnsortedEntries.Num()-1; I >= 0; --I)
		{
			const FSortableDependencyEntry& DependencyEntry = UnsortedEntries[I];
			if (DependencyEntry.bIsAsset)
			{
				// find all the uexp files and ubulk files and uptnl files
				FString StringPath = DependencyEntry.FilePath.ToString();
				for (const FName& ExtraAssetExtension : ExtraAssetExtensions)
				{
					FString ExtraAssetPath = FPaths::ChangeExtension(StringPath, ExtraAssetExtension.ToString());
					FName ExtraAssetPathFName = FName(*ExtraAssetPath);
					if (OriginalEntrySet.Contains(ExtraAssetPathFName))
					{
						check(!ProcessedFiles.Contains(ExtraAssetPathFName));
						ProcessedFiles.Add(ExtraAssetPathFName);
						TSet<FName> Classes = DependencyEntry.Classes;
						UnsortedEntries.Add(FSortableDependencyEntry(DependencyEntry.LongPackageName, ExtraAssetPathFName, ExtraAssetExtension, DependencyEntry.DepSet, DependencyEntry.DepHierarchy, DependencyEntry.DepOrder, DependencyEntry.bHasDependencies, MoveTemp(Classes)));
					}
				}
			}
		}

		//if this were to fire, first guess would be that there's somehow a rogue export without a header
		check(OriginalEntrySet.Num() == ProcessedFiles.Num() && ProcessedFiles.Num() == UnsortedEntries.Num());


		TArray<FName> ShouldGroupExtensions;
		ShouldGroupExtensions.Add(NAME_ubulk);


		TMap<FName, int32> ExtensionPriority;
		ExtensionPriority.Add(NAME_umap, 0); 
		ExtensionPriority.Add(NAME_uasset, 0);
		ExtensionPriority.Add(NAME_uexp, 1);
		ExtensionPriority.Add(NAME_uptnl, 1);
		ExtensionPriority.Add(NAME_ubulk, 1);
		FSortableDependencySort DependencySortClass(ShouldGroupExtensions, FilterByClasses, ExtensionPriority);
		UnsortedEntries.Sort(DependencySortClass);


		
		UE_LOG(LogAssetRegUtil, Display, TEXT("Writing output: %s"), *ReorderFileOutPath);
		FArchive* OutArc = IFileManager::Get().CreateFileWriter(*ReorderFileOutPath);
		if (OutArc)
		{
			//base from 1, to match existing order list convention
			uint64 NewOrderIndex = 1;
			for (const FSortableDependencyEntry& SortedEntry : UnsortedEntries)
			{
				const FString& FilePath = SortedEntry.FilePath.ToString();
				FString OutputLine = FString::Printf(TEXT("\"%s\" %llu\n"), *FilePath, NewOrderIndex++);
				OutArc->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());
			}
			OutArc->Close();
			delete OutArc;
		}
		else
		{
			UE_LOG(LogAssetRegUtil, Warning, TEXT("Could not open specified output file."));
		}
	}
	else
	{
		UE_LOG(LogAssetRegUtil, Warning, TEXT("Could not load specified order file."));
	}
}

int32 UAssetRegUtilCommandlet::Main(const FString& CmdLineParams)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistry = &AssetRegistryModule.Get();

	UE_LOG(LogAssetRegUtil, Display, TEXT("Populating the Asset Registry."));
	AssetRegistry->SearchAllAssets(true);
	
	FString ReorderFile;
	if (FParse::Value(*CmdLineParams, TEXT("ReorderFile="), ReorderFile))
	{
		FString ReorderOutput;
		if (!FParse::Value(*CmdLineParams, TEXT("ReorderOutput="), ReorderOutput))
		{
			//if nothing specified, base it on the input name
			ReorderOutput = FPaths::SetExtension(FPaths::SetExtension(ReorderFile, TEXT("")) + TEXT("Reordered"), FPaths::GetExtension(ReorderFile));
		}

		ReorderOrderFile(ReorderFile, ReorderOutput);
	}

	return 0;
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Containers/SortedMap.h"
#include "Internationalization/LocKeyFuncs.h"
#include "Internationalization/LocalizedTextSourceTypes.h"
#include "Internationalization/TextLocalizationResourceId.h"

/** Utility class for working with Localization MetaData Resource (LocMeta) files. */
class CORE_API FTextLocalizationMetaDataResource
{
public:
	FString NativeCulture;
	FString NativeLocRes;

	/** Load the given LocMeta file into this resource. */
	bool LoadFromFile(const FString& FilePath);

	/** Load the given LocMeta archive into this resource. */
	bool LoadFromArchive(FArchive& Archive, const FString& LocMetaID);

	/** Save this resource to the given LocMeta file. */
	bool SaveToFile(const FString& FilePath);

	/** Save this resource to the given LocMeta archive. */
	bool SaveToArchive(FArchive& Archive, const FString& LocMetaID);
};

/** Utility class for working with Localization Resource (LocRes) files. */
class CORE_API FTextLocalizationResource
{
public:
	/** Data struct for tracking a localization entry from a localization resource. */
	struct FEntry
	{
		FTextLocalizationResourceId LocResID;
		uint32 SourceStringHash;
		FString LocalizedString;
	};

	typedef TArray<FEntry> FEntryArray;
	typedef TMap<FString, FEntryArray, FDefaultSetAllocator, FLocKeyMapFuncs<FEntryArray>> FKeysTable;
	typedef TMap<FString, FKeysTable, FDefaultSetAllocator, FLocKeyMapFuncs<FKeysTable>> FNamespacesTable;

	FNamespacesTable Namespaces;

	/** Utility to produce a hash for a string (as used by SourceStringHash) */
	static FORCEINLINE uint32 HashString(const TCHAR* InStr, const uint32 InBaseHash = 0)
	{
		return FCrc::StrCrc32<TCHAR>(InStr, InBaseHash);
	}

	/** Utility to produce a hash for a string (as used by SourceStringHash) */
	static FORCEINLINE uint32 HashString(const FString& InStr, const uint32 InBaseHash = 0)
	{
		return FCrc::StrCrc32<TCHAR>(*InStr, InBaseHash);
	}

	/** Add a single entry to this resource. */
	void AddEntry(const FString& InNamespace, const FString& InKey, const FString& InSourceString, const FString& InLocalizedString, const FTextLocalizationResourceId& InLocResID = FTextLocalizationResourceId());
	void AddEntry(const FString& InNamespace, const FString& InKey, const uint32 InSourceStringHash, const FString& InLocalizedString, const FTextLocalizationResourceId& InLocResID = FTextLocalizationResourceId());

	/** Is this resource empty? */
	bool IsEmpty() const;

	/** Load all LocRes files in the specified directory into this resource. */
	void LoadFromDirectory(const FString& DirectoryPath);

	/** Load the given LocRes file into this resource. */
	bool LoadFromFile(const FString& FilePath);

	/** Load the given LocRes archive into this resource. */
	bool LoadFromArchive(FArchive& Archive, const FTextLocalizationResourceId& LocResID);

	/** Save this resource to the given LocRes file. */
	bool SaveToFile(const FString& FilePath);

	/** Save this resource to the given LocRes archive. */
	bool SaveToArchive(FArchive& Archive, const FTextLocalizationResourceId& LocResID);

	/** Detect conflicts between loaded localization resources and log them as warnings. */
	void DetectAndLogConflicts() const;
};

/** Utility class for working with set of Localization Resource (LocRes) files. */
class CORE_API FTextLocalizationResources
{
public:
	TSharedRef<FTextLocalizationResource> EnsureResource(const FString& InCulture)
	{
		TSharedPtr<FTextLocalizationResource> Resource = TextLocalizationResourceMap.FindRef(InCulture);
		if (!Resource.IsValid())
		{
			Resource = MakeShared<FTextLocalizationResource>();
			TextLocalizationResourceMap.Add(InCulture, Resource);
		}
		return Resource.ToSharedRef();
	}

	TSharedPtr<FTextLocalizationResource> FindResource(const FString& InCulture) const
	{
		return TextLocalizationResourceMap.FindRef(InCulture);
	}

private:
	TSortedMap<FString, TSharedPtr<FTextLocalizationResource>> TextLocalizationResourceMap;
};

namespace TextLocalizationResourceUtil
{

/**
 * Given some paths to look at, get the native culture for the targets within those paths (if known).
 * @return The native culture for the targets within the given paths based on the data in the first LocMeta file, or an empty string if the native culture is unknown.
 */
CORE_API FString GetNativeCultureName(const TArray<FString>& InLocalizationPaths);

/**
 * Given a localization category, get the native culture for the targets for that category (if known).
 * @return The native culture for the given localization category, or an empty string if the native culture is unknown.
 */
CORE_API FString GetNativeCultureName(const ELocalizedTextSourceCategory InCategory);

/**
 * Get the native culture for the current project (if known).
 * @return The native culture for the current project based on the data in the game LocMeta files, or an empty string if the native culture is unknown.
 */
CORE_API FString GetNativeProjectCultureName(const bool bSkipCache = false);

/**
 * Get the native culture for the engine.
 * @return The native culture for the engine based on the data in the engine LocMeta files.
 */
CORE_API FString GetNativeEngineCultureName(const bool bSkipCache = false);

#if WITH_EDITOR
/**
 * Get the native culture for the editor.
 * @return The native culture for the editor based on the data in the editor LocMeta files.
 */
CORE_API FString GetNativeEditorCultureName(const bool bSkipCache = false);
#endif	// WITH_EDITOR

/**
 * Given some paths to look at, populate a list of culture names that we have available localization resource information for.
 */
CORE_API TArray<FString> GetLocalizedCultureNames(const TArray<FString>& InLocalizationPaths);

}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationResourceVersion.h"
#include "Internationalization/Culture.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/Optional.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationResource, Log, All);

const FGuid FTextLocalizationResourceVersion::LocMetaMagic = FGuid(0xA14CEE4F, 0x83554868, 0xBD464C6C, 0x7C50DA70);
const FGuid FTextLocalizationResourceVersion::LocResMagic = FGuid(0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3);

bool FTextLocalizationMetaDataResource::LoadFromFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Reader)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocMeta '%s' could not be opened for reading!"), *FilePath);
		return false;
	}

	bool Success = LoadFromArchive(*Reader, FilePath);
	Success &= Reader->Close();
	return Success;
}

bool FTextLocalizationMetaDataResource::LoadFromArchive(FArchive& Archive, const FString& LocMetaID)
{
	FTextLocalizationResourceVersion::ELocMetaVersion VersionNumber = FTextLocalizationResourceVersion::ELocMetaVersion::Initial;

	// Verify header
	{
		FGuid MagicNumber;
		Archive << MagicNumber;

		if (MagicNumber != FTextLocalizationResourceVersion::LocMetaMagic)
		{
			UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocMeta '%s' failed the magic number check!"), *LocMetaID);
			return false;
		}

		Archive << VersionNumber;
	}

	// Is this LocMeta file too new to load?
	if (VersionNumber > FTextLocalizationResourceVersion::ELocMetaVersion::Latest)
	{
		UE_LOG(LogTextLocalizationResource, Error, TEXT("LocMeta '%s' is too new to be loaded (File Version: %d, Loader Version: %d)"), *LocMetaID, (int32)VersionNumber, (int32)FTextLocalizationResourceVersion::ELocMetaVersion::Latest);
		return false;
	}

	Archive << NativeCulture;
	Archive << NativeLocRes;

	return true;
}

bool FTextLocalizationMetaDataResource::SaveToFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*FilePath));
	if (!Writer)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocMeta '%s' could not be opened for writing!"), *FilePath);
		return false;
	}

	bool bSaved = SaveToArchive(*Writer, FilePath);
	bSaved &= Writer->Close();
	return bSaved;
}

bool FTextLocalizationMetaDataResource::SaveToArchive(FArchive& Archive, const FString& LocMetaID)
{
	// Write the header
	{
		FGuid MagicNumber = FTextLocalizationResourceVersion::LocMetaMagic;
		Archive << MagicNumber;

		uint8 VersionNumber = (uint8)FTextLocalizationResourceVersion::ELocMetaVersion::Latest;
		Archive << VersionNumber;
	}

	// Write the native meta-data
	{
		Archive << NativeCulture;
		Archive << NativeLocRes;
	}

	return true;
}


void FTextLocalizationResource::AddEntry(const FString& InNamespace, const FString& InKey, const FString& InSourceString, const FString& InLocalizedString, const FTextLocalizationResourceId& InLocResID)
{
	AddEntry(InNamespace, InKey, HashString(InSourceString), InLocalizedString, InLocResID);
}

void FTextLocalizationResource::AddEntry(const FString& InNamespace, const FString& InKey, const uint32 InSourceStringHash, const FString& InLocalizedString, const FTextLocalizationResourceId& InLocResID)
{
	FKeysTable& KeyTable = Namespaces.FindOrAdd(InNamespace);
	FEntryArray& EntryArray = KeyTable.FindOrAdd(InKey);

	FEntry& NewEntry = EntryArray.AddDefaulted_GetRef();
	NewEntry.LocResID = InLocResID;
	NewEntry.SourceStringHash = InSourceStringHash;
	NewEntry.LocalizedString = InLocalizedString;
}

bool FTextLocalizationResource::IsEmpty() const
{
	return Namespaces.Num() == 0;
}

void FTextLocalizationResource::LoadFromDirectory(const FString& DirectoryPath)
{
	// Find resources in the specified folder.
	TArray<FString> ResourceFileNames;
	IFileManager::Get().FindFiles(ResourceFileNames, *(DirectoryPath / TEXT("*.locres")), true, false);

	for (const FString& ResourceFileName : ResourceFileNames)
	{
		LoadFromFile(FPaths::ConvertRelativePathToFull(DirectoryPath / ResourceFileName));
	}
}

bool FTextLocalizationResource::LoadFromFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Reader)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' could not be opened for reading!"), *FilePath);
		return false;
	}

	bool Success = LoadFromArchive(*Reader, FTextLocalizationResourceId(FilePath));
	Success &= Reader->Close();
	return Success;
}

bool FTextLocalizationResource::LoadFromArchive(FArchive& Archive, const FTextLocalizationResourceId& LocResID)
{
	Archive.SetForceUnicode(true);

	// Read magic number
	FGuid MagicNumber;
	
	if (Archive.TotalSize() >= sizeof(FGuid))
	{
		Archive << MagicNumber;
	}

	FTextLocalizationResourceVersion::ELocResVersion VersionNumber = FTextLocalizationResourceVersion::ELocResVersion::Legacy;
	if (MagicNumber == FTextLocalizationResourceVersion::LocResMagic)
	{
		Archive << VersionNumber;
	}
	else
	{
		// Legacy LocRes files lack the magic number, assume that's what we're dealing with, and seek back to the start of the file
		Archive.Seek(0);
		//UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' failed the magic number check! Assuming this is a legacy resource (please re-generate your localization resources!)"), *LocResID.GetString());
		UE_LOG(LogTextLocalizationResource, Log, TEXT("LocRes '%s' failed the magic number check! Assuming this is a legacy resource (please re-generate your localization resources!)"), *LocResID.GetString());
	}

	// Is this LocRes file too new to load?
	if (VersionNumber > FTextLocalizationResourceVersion::ELocResVersion::Latest)
	{
		UE_LOG(LogTextLocalizationResource, Error, TEXT("LocRes '%s' is too new to be loaded (File Version: %d, Loader Version: %d)"), *LocResID.GetString(), (int32)VersionNumber, (int32)FTextLocalizationResourceVersion::ELocResVersion::Latest);
		return false;
	}

	// Read the localized string array
	TArray<FString> LocalizedStringArray;
	if (VersionNumber >= FTextLocalizationResourceVersion::ELocResVersion::Compact)
	{
		int64 LocalizedStringArrayOffset = INDEX_NONE;
		Archive << LocalizedStringArrayOffset;

		if (LocalizedStringArrayOffset != INDEX_NONE)
		{
			const int64 CurrentFileOffset = Archive.Tell();
			Archive.Seek(LocalizedStringArrayOffset);
			Archive << LocalizedStringArray;
			Archive.Seek(CurrentFileOffset);
		}
	}

	// Read namespace count
	uint32 NamespaceCount;
	Archive << NamespaceCount;

	for (uint32 i = 0; i < NamespaceCount; ++i)
	{
		// Read namespace
		FString Namespace;
		Archive << Namespace;

		// Read key count
		uint32 KeyCount;
		Archive << KeyCount;

		FKeysTable& KeyTable = Namespaces.FindOrAdd(Namespace);

		for (uint32 j = 0; j < KeyCount; ++j)
		{
			// Read key
			FString Key;
			Archive << Key;

			FEntryArray& EntryArray = KeyTable.FindOrAdd(Key);

			FEntry& NewEntry = EntryArray.AddDefaulted_GetRef();
			NewEntry.LocResID = LocResID;

			// Read string entry.
			Archive << NewEntry.SourceStringHash;

			if (VersionNumber >= FTextLocalizationResourceVersion::ELocResVersion::Compact)
			{
				int32 LocalizedStringIndex = INDEX_NONE;
				Archive << LocalizedStringIndex;

				if (LocalizedStringArray.IsValidIndex(LocalizedStringIndex))
				{
					NewEntry.LocalizedString = LocalizedStringArray[LocalizedStringIndex];
				}
				else
				{
					UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' has an invalid localized string index for namespace '%s' and key '%s'. This entry will have no translation."), *LocResID.GetString(), *Namespace, *Key);
				}
			}
			else
			{
				Archive << NewEntry.LocalizedString;
			}
		}
	}

	return true;
}

bool FTextLocalizationResource::SaveToFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*FilePath));
	if (!Writer)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' could not be opened for writing!"), *FilePath);
		return false;
	}

	bool bSaved = SaveToArchive(*Writer, FTextLocalizationResourceId(FilePath));
	bSaved &= Writer->Close();
	return bSaved;
}

bool FTextLocalizationResource::SaveToArchive(FArchive& Archive, const FTextLocalizationResourceId& LocResID)
{
	Archive.SetForceUnicode(true);

	// Write the header
	{
		FGuid MagicNumber = FTextLocalizationResourceVersion::LocResMagic;
		Archive << MagicNumber;

		uint8 VersionNumber = (uint8)FTextLocalizationResourceVersion::ELocResVersion::Latest;
		Archive << VersionNumber;
	}

	// Write placeholder offsets for the localized string array
	const int64 LocalizedStringArrayOffset = Archive.Tell();
	{
		int64 DummyOffsetValue = INDEX_NONE;
		Archive << DummyOffsetValue;
	}

	// Arrays tracking localized strings, with a map for efficient look-up of array indices from strings
	TArray<FString> LocalizedStringArray;
	TMap<FString, int32, FDefaultSetAllocator, FLocKeyMapFuncs<int32>> LocalizedStringMap;

	auto GetLocalizedStringIndex = [&LocalizedStringArray, &LocalizedStringMap](const FString& InString) -> int32
	{
		if (const int32* FoundIndex = LocalizedStringMap.Find(InString))
		{
			return *FoundIndex;
		}

		const int32 NewIndex = LocalizedStringArray.Num();
		LocalizedStringArray.Add(InString);
		LocalizedStringMap.Add(InString, NewIndex);
		return NewIndex;
	};

	// Write namespace count
	uint32 NamespaceCount = Namespaces.Num();
	Archive << NamespaceCount;

	// Iterate through namespaces
	for (auto& NamespaceEntryPair : Namespaces)
	{
		/*const*/ FString& Namespace = NamespaceEntryPair.Key;
		/*const*/ FKeysTable& KeysTable = NamespaceEntryPair.Value;

		// Write namespace.
		Archive << Namespace;

		// Write a placeholder key count, we'll fill this in once we know how many keys were actually written
		uint32 KeyCount = 0;
		const int64 KeyCountOffset = Archive.Tell();
		Archive << KeyCount;

		// Iterate through keys and values
		for (auto& KeyEntryPair : KeysTable)
		{
			/*const*/ FString& Key = KeyEntryPair.Key;
			/*const*/ FEntryArray& EntryArray = KeyEntryPair.Value;

			// Skip this key if there are no entries.
			if (EntryArray.Num() == 0)
			{
				UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s': Archives contained no entries for key (%s)"), *LocResID.GetString(), *Key);
				continue;
			}

			// Find first valid entry.
			/*const*/ FEntry* Value = nullptr;
			for (auto& PotentialValue : EntryArray)
			{
				if (!PotentialValue.LocalizedString.IsEmpty())
				{
					Value = &PotentialValue;
					break;
				}
			}

			// Skip this key if there is no valid entry.
			if (!Value)
			{
				UE_LOG(LogTextLocalizationResource, Verbose, TEXT("LocRes '%s': Archives contained only blank entries for key (%s)"), *LocResID.GetString(), *Key);
				continue;
			}

			++KeyCount;

			// Write key.
			Archive << Key;

			// Write string entry.
			Archive << Value->SourceStringHash;

			int32 LocalizedStringIndex = GetLocalizedStringIndex(Value->LocalizedString);
			Archive << LocalizedStringIndex;
		}

		// Re-write the key value now
		{
			const int64 CurrentFileOffset = Archive.Tell();
			Archive.Seek(KeyCountOffset);
			Archive << KeyCount;
			Archive.Seek(CurrentFileOffset);
		}
	}

	// Write the localized strings array now
	{
		int64 CurrentFileOffset = Archive.Tell();
		Archive.Seek(LocalizedStringArrayOffset);
		Archive << CurrentFileOffset;
		Archive.Seek(CurrentFileOffset);
		Archive << LocalizedStringArray;
	}

	return true;
}

void FTextLocalizationResource::DetectAndLogConflicts() const
{
	for (const auto& NamespaceEntry : Namespaces)
	{
		const FString& NamespaceName = NamespaceEntry.Key;
		const FKeysTable& KeyTable = NamespaceEntry.Value;
		for (const auto& KeyEntry : KeyTable)
		{
			const FString& KeyName = KeyEntry.Key;
			const FEntryArray& EntryArray = KeyEntry.Value;

			bool WasConflictDetected = false;
			for (int32 k = 0; k < EntryArray.Num(); ++k)
			{
				const FEntry& LeftEntry = EntryArray[k];
				for (int32 l = k + 1; l < EntryArray.Num(); ++l)
				{
					const FEntry& RightEntry = EntryArray[l];
					const bool bDoesSourceStringHashDiffer = LeftEntry.SourceStringHash != RightEntry.SourceStringHash;
					const bool bDoesLocalizedStringDiffer = !LeftEntry.LocalizedString.Equals(RightEntry.LocalizedString, ESearchCase::CaseSensitive);
					WasConflictDetected = bDoesSourceStringHashDiffer || bDoesLocalizedStringDiffer;
				}
			}

			if (WasConflictDetected)
			{
				FString CollidingEntryListString;
				for (int32 k = 0; k < EntryArray.Num(); ++k)
				{
					const FEntry& Entry = EntryArray[k];

					if (!(CollidingEntryListString.IsEmpty()))
					{
						CollidingEntryListString += TEXT('\n');
					}

					CollidingEntryListString += FString::Printf(TEXT("    Localization Resource: (%s) Source String Hash: (%d) Localized String: (%s)"), *(Entry.LocResID.GetString()), Entry.SourceStringHash, *(Entry.LocalizedString));
				}

				UE_LOG(LogTextLocalizationResource, Warning, TEXT("Loaded localization resources contain conflicting entries for (Namespace:%s, Key:%s):\n%s"), *NamespaceName, *KeyName, *CollidingEntryListString);
			}
		}
	}
}


FString TextLocalizationResourceUtil::GetNativeCultureName(const TArray<FString>& InLocalizationPaths)
{
	// Use the native culture of any of the targets on the given paths (it is assumed that all targets for a particular product have the same native culture)
	for (const FString& LocalizationPath : InLocalizationPaths)
	{
		TArray<FString> LocMetaFilenames;
		IFileManager::Get().FindFiles(LocMetaFilenames, *(LocalizationPath / TEXT("*.locmeta")), true, false);

		// There should only be zero or one LocMeta file
		check(LocMetaFilenames.Num() <= 1);

		if (LocMetaFilenames.Num() == 1)
		{
			FTextLocalizationMetaDataResource LocMetaResource;
			if (LocMetaResource.LoadFromFile(LocalizationPath / LocMetaFilenames[0]))
			{
				return LocMetaResource.NativeCulture;
			}
		}
	}

	return FString();
}

FString TextLocalizationResourceUtil::GetNativeCultureName(const ELocalizedTextSourceCategory InCategory)
{
	switch (InCategory)
	{
	case ELocalizedTextSourceCategory::Game:
		return GetNativeProjectCultureName();
	case ELocalizedTextSourceCategory::Engine:
		return GetNativeEngineCultureName();
	case ELocalizedTextSourceCategory::Editor:
#if WITH_EDITOR
		return GetNativeEditorCultureName();
#else
		break;
#endif
	default:
		checkf(false, TEXT("Unknown ELocalizedTextSourceCategory!"));
		break;
	}
	return FString();
}

FString TextLocalizationResourceUtil::GetNativeProjectCultureName(const bool bSkipCache)
{
	static TOptional<FString> NativeProjectCultureName;
	if (!NativeProjectCultureName.IsSet() || bSkipCache)
	{
		NativeProjectCultureName = TextLocalizationResourceUtil::GetNativeCultureName(FPaths::GetGameLocalizationPaths());
	}
	return NativeProjectCultureName.GetValue();
}

FString TextLocalizationResourceUtil::GetNativeEngineCultureName(const bool bSkipCache)
{
	static TOptional<FString> NativeEngineCultureName;
	if (!NativeEngineCultureName.IsSet() || bSkipCache)
	{
		NativeEngineCultureName = TextLocalizationResourceUtil::GetNativeCultureName(FPaths::GetEngineLocalizationPaths());
	}
	return NativeEngineCultureName.GetValue();
}

#if WITH_EDITOR
FString TextLocalizationResourceUtil::GetNativeEditorCultureName(const bool bSkipCache)
{
	static TOptional<FString> NativeEditorCultureName;
	if (!NativeEditorCultureName.IsSet() || bSkipCache)
	{
		NativeEditorCultureName = TextLocalizationResourceUtil::GetNativeCultureName(FPaths::GetEditorLocalizationPaths());
	}
	return NativeEditorCultureName.GetValue();
}
#endif	// WITH_EDITOR

TArray<FString> TextLocalizationResourceUtil::GetLocalizedCultureNames(const TArray<FString>& InLocalizationPaths)
{
	TArray<FString> CultureNames;

	// Find all unique culture folders that exist in the given paths
	for (const FString& LocalizationPath : InLocalizationPaths)
	{
		/* Visitor class used to enumerate directories of culture */
		class FCultureEnumeratorVistor : public IPlatformFile::FDirectoryVisitor
		{
		public:
			FCultureEnumeratorVistor(TArray<FString>& OutCultureNames)
				: CultureNamesRef(OutCultureNames)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (bIsDirectory)
				{
					// UE localization resource folders use "en-US" style while ICU uses "en_US"
					const FString LocalizationFolder = FPaths::GetCleanFilename(FilenameOrDirectory);
					const FString CanonicalName = FCulture::GetCanonicalName(LocalizationFolder);
					CultureNamesRef.AddUnique(CanonicalName);
				}
				return true;
			}

			TArray<FString>& CultureNamesRef;
		};

		FCultureEnumeratorVistor CultureEnumeratorVistor(CultureNames);
		IFileManager::Get().IterateDirectory(*LocalizationPath, CultureEnumeratorVistor);
	}

	// Remove any cultures that were explicitly disallowed
	FInternationalization& I18N = FInternationalization::Get();
	CultureNames.RemoveAll([&](const FString& InCultureName) -> bool
	{
		return !I18N.IsCultureAllowed(InCultureName);
	});

	return CultureNames;
}

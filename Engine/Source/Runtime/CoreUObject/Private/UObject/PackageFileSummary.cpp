// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UObject/PackageFileSummary.h"
#include "UObject/Linker.h"
#include "Serialization/ArchiveFromStructuredArchive.h"
#include "UObject/UObjectGlobals.h"

FPackageFileSummary::FPackageFileSummary()
{
	FMemory::Memzero( this, sizeof(*this) );
}

/** Converts file version to custom version system version */
static ECustomVersionSerializationFormat::Type GetCustomVersionFormatForArchive(int32 LegacyFileVersion)
{
	ECustomVersionSerializationFormat::Type CustomVersionFormat = ECustomVersionSerializationFormat::Unknown;
	if (LegacyFileVersion == -2)
	{
		CustomVersionFormat = ECustomVersionSerializationFormat::Enums;
	}
	else if (LegacyFileVersion < -2 && LegacyFileVersion >= -5)
	{
		CustomVersionFormat = ECustomVersionSerializationFormat::Guids;
	}
	else if (LegacyFileVersion < -5)
	{
		CustomVersionFormat = ECustomVersionSerializationFormat::Optimized;
	}
	check(CustomVersionFormat != ECustomVersionSerializationFormat::Unknown);
	return CustomVersionFormat;
}

void operator<<(FStructuredArchive::FSlot Slot, FPackageFileSummary& Sum)
{
	FArchive& BaseArchive = Slot.GetUnderlyingArchive();
	bool bCanStartSerializing = true;
	int64 ArchiveSize = 0;
	if (BaseArchive.IsLoading())
	{
		// Sanity checks before we even start serializing the archive
		ArchiveSize = BaseArchive.TotalSize();
		const int64 MinimumPackageSize = 32; // That should get us safely to Sum.TotalHeaderSize
		bCanStartSerializing = ArchiveSize >= MinimumPackageSize;
		UE_CLOG(!bCanStartSerializing, LogLinker, Warning,
			TEXT("Failed to read package file summary, the file \"%s\" is too small (%lld bytes, expected at least %lld bytes)"),
			*BaseArchive.GetArchiveName(), ArchiveSize, MinimumPackageSize);
	}

	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	if (bCanStartSerializing)
	{
		Record << NAMED_ITEM("Tag", Sum.Tag);
	}
	// only keep loading if we match the magic
	if (Sum.Tag == PACKAGE_FILE_TAG || Sum.Tag == PACKAGE_FILE_TAG_SWAPPED)
	{
		// The package has been stored in a separate endianness than the linker expected so we need to force
		// endian conversion. Latent handling allows the PC version to retrieve information about cooked packages.
		if (Sum.Tag == PACKAGE_FILE_TAG_SWAPPED)
		{
			// Set proper tag.
			Sum.Tag = PACKAGE_FILE_TAG;
			// Toggle forced byte swapping.
			if (BaseArchive.ForceByteSwapping())
			{
				BaseArchive.SetByteSwapping(false);
			}
			else
			{
				BaseArchive.SetByteSwapping(true);
			}
		}
		/**
		* The package file version number when this package was saved.
		*
		* Lower 16 bits stores the UE3 engine version
		* Upper 16 bits stores the UE4/licensee version
		* For newer packages this is -7
		*		-2 indicates presence of enum-based custom versions
		*		-3 indicates guid-based custom versions
		*		-4 indicates removal of the UE3 version. Packages saved with this ID cannot be loaded in older engine versions
		*		-5 indicates the replacement of writing out the "UE3 version" so older versions of engine can gracefully fail to open newer packages
		*		-6 indicates optimizations to how custom versions are being serialized
		*		-7 indicates the texture allocation info has been removed from the summary
		*/
		const int32 CurrentLegacyFileVersion = -7;
		int32 LegacyFileVersion = CurrentLegacyFileVersion;
		Record << NAMED_ITEM("LegacyFileVersion", LegacyFileVersion);

		if (BaseArchive.IsLoading())
		{
			if (LegacyFileVersion < 0) // means we have modern version numbers
			{
				if (LegacyFileVersion < CurrentLegacyFileVersion)
				{
					// we can't safely load more than this because the legacy version code differs in ways we can not predict.
					// Make sure that the linker will fail to load with it.
					Sum.FileVersionUE4 = 0;
					Sum.FileVersionLicenseeUE4 = 0;
					return;
				}

				if (LegacyFileVersion != -4)
				{
					int32 LegacyUE3Version = 0;
					Record << NAMED_ITEM("LegacyUE3Version", LegacyUE3Version);
				}
				Record << NAMED_ITEM("FileVersionUE4", Sum.FileVersionUE4);
				Record << NAMED_ITEM("FileVersionLicenseeUE4", Sum.FileVersionLicenseeUE4);

				if (LegacyFileVersion <= -2)
				{
					Sum.CustomVersionContainer.Serialize(Record.EnterField(FIELD_NAME_TEXT("CustomVersions")), GetCustomVersionFormatForArchive(LegacyFileVersion));
				}

				if (!Sum.FileVersionUE4 && !Sum.FileVersionLicenseeUE4)
				{
#if WITH_EDITOR
					if (!GAllowUnversionedContentInEditor)
					{
						// the editor cannot safely load unversioned content
						UE_LOG(LogLinker, Warning, TEXT("Failed to read package file summary, the file \"%s\" is unversioned and we cannot safely load unversioned files in the editor."), *BaseArchive.GetArchiveName());
						return;
					}
#endif
					// this file is unversioned, remember that, then use current versions
					Sum.bUnversioned = true;
					Sum.FileVersionUE4 = GPackageFileUE4Version;
					Sum.FileVersionLicenseeUE4 = GPackageFileLicenseeUE4Version;
					Sum.CustomVersionContainer = FCustomVersionContainer::GetRegistered();
				}
			}
			else
			{
				// This is probably an old UE3 file, make sure that the linker will fail to load with it.
				Sum.FileVersionUE4 = 0;
				Sum.FileVersionLicenseeUE4 = 0;
			}
		}
		else
		{
			if (Sum.bUnversioned)
			{
				int32 Zero = 0;
				Record << NAMED_ITEM("LegacyUE3version", Zero); // LegacyUE3version
				Record << NAMED_ITEM("FileVersionUE4", Zero); // VersionUE4
				Record << NAMED_ITEM("FileVersionLicenseeUE4", Zero); // VersionLicenseeUE4

				FCustomVersionContainer NoCustomVersions;
				NoCustomVersions.Serialize(Record.EnterField(FIELD_NAME_TEXT("CustomVersions")));
			}
			else
			{
				// Must write out the last UE3 engine version, so that older versions identify it as new
				int32 LegacyUE3Version = 864;
				Record << NAMED_ITEM("LegacyUE3Version", LegacyUE3Version);
				Record << NAMED_ITEM("FileVersionUE4", Sum.FileVersionUE4);
				Record << NAMED_ITEM("FileVersionLicenseeUE4", Sum.FileVersionLicenseeUE4);

				// Serialise custom version map.
				Sum.CustomVersionContainer.Serialize(Record.EnterField(FIELD_NAME_TEXT("CustomVersions")));
			}
		}
		Record << NAMED_ITEM("TotalHeaderSize", Sum.TotalHeaderSize);
		Record << NAMED_ITEM("FolderName", Sum.FolderName);
		Record << NAMED_ITEM("PackageFlags", Sum.PackageFlags);

#if WITH_EDITOR
		if (BaseArchive.IsLoading())
		{
			// This flag should never be saved and its reused, so we need to make sure it hasn't been loaded.
			Sum.PackageFlags &= ~PKG_NewlyCreated;
		}
#endif // WITH_EDITOR

		if (Sum.PackageFlags & PKG_FilterEditorOnly)
		{
			BaseArchive.SetFilterEditorOnly(true);
		}
		Record << NAMED_ITEM("NameCount", Sum.NameCount) << NAMED_ITEM("NameOffset", Sum.NameOffset);
		if (!BaseArchive.IsFilterEditorOnly())
		{
			if (BaseArchive.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID)
			{
				Record << NAMED_ITEM("LocalizationId", Sum.LocalizationId);
			}
		}
		if (Sum.FileVersionUE4 >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
		{
			Record << NAMED_ITEM("GatherableTextDataCount", Sum.GatherableTextDataCount) << NAMED_ITEM("GatherableTextDataOffset", Sum.GatherableTextDataOffset);
		}
		Record << NAMED_ITEM("ExportCount", Sum.ExportCount) << NAMED_ITEM("ExportOffset", Sum.ExportOffset);
		Record << NAMED_ITEM("ImportCount", Sum.ImportCount) << NAMED_ITEM("ImportOffset", Sum.ImportOffset);
		Record << NAMED_ITEM("DependsOffset", Sum.DependsOffset);

		if (BaseArchive.IsLoading() && (Sum.FileVersionUE4 < VER_UE4_OLDEST_LOADABLE_PACKAGE || Sum.FileVersionUE4 > GPackageFileUE4Version))
		{
			return; // we can't safely load more than this because the below was different in older files.
		}

		if (BaseArchive.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
		{
			Record << NAMED_ITEM("SoftPackageReferencesCount", Sum.SoftPackageReferencesCount) << NAMED_ITEM("SoftPackageReferencesOffset", Sum.SoftPackageReferencesOffset);
		}

		if (BaseArchive.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_ADDED_SEARCHABLE_NAMES)
		{
			Record << NAMED_ITEM("SearchableNamesOffset", Sum.SearchableNamesOffset);
		}

		Record << NAMED_ITEM("ThumbnailTableOffset", Sum.ThumbnailTableOffset);

		int32 GenerationCount = Sum.Generations.Num();
		Record << NAMED_ITEM("Guid", Sum.Guid) << NAMED_ITEM("GenerationCount", GenerationCount);
		if (BaseArchive.IsLoading() && GenerationCount > 0)
		{
			Sum.Generations.Empty(1);
			Sum.Generations.AddUninitialized(GenerationCount);
		}

		FStructuredArchive::FStream GenerationsStream = Record.EnterStream(FIELD_NAME_TEXT("Generations"));
		for (int32 i = 0; i<GenerationCount; i++)
		{
			Sum.Generations[i].Serialize(GenerationsStream.EnterElement(), Sum);
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_ENGINE_VERSION_OBJECT)
		{
			if (BaseArchive.IsCooking() || (BaseArchive.IsSaving() && !FEngineVersion::Current().HasChangelist()))
			{
				FEngineVersion EmptyEngineVersion;
				Record << NAMED_ITEM("SavedByEngineVersion", EmptyEngineVersion);
			}
			else
			{
				Record << NAMED_ITEM("SavedByEngineVersion", Sum.SavedByEngineVersion);
			}
		}
		else
		{
			int32 EngineChangelist = 0;
			Record << NAMED_ITEM("EngineChangelist", EngineChangelist);

			if (BaseArchive.IsLoading() && EngineChangelist != 0)
			{
				Sum.SavedByEngineVersion.Set(4, 0, 0, EngineChangelist, TEXT(""));
			}
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION)
		{
			if (BaseArchive.IsCooking() || (BaseArchive.IsSaving() && !FEngineVersion::Current().HasChangelist()))
			{
				FEngineVersion EmptyEngineVersion;
				Record << NAMED_ITEM("CompatibleWithEngineVersion", EmptyEngineVersion);
			}
			else
			{
				Record << NAMED_ITEM("CompatibleWithEngineVersion", Sum.CompatibleWithEngineVersion);
			}
		}
		else
		{
			if (BaseArchive.IsLoading())
			{
				Sum.CompatibleWithEngineVersion = Sum.SavedByEngineVersion;
			}
		}

		Record << NAMED_ITEM("CompressionFlags", Sum.CompressionFlags);
		if (!FCompression::VerifyCompressionFlagsValid(Sum.CompressionFlags))
		{
			UE_LOG(LogLinker, Warning, TEXT("Failed to read package file summary, the file \"%s\" has invalid compression flags (%d)."), *BaseArchive.GetArchiveName(), Sum.CompressionFlags);
			Sum.FileVersionUE4 = VER_UE4_OLDEST_LOADABLE_PACKAGE - 1;
			return;
		}

		TArray<FCompressedChunk> CompressedChunks;
		Record << NAMED_ITEM("CompressedChunks", CompressedChunks);

		if (CompressedChunks.Num())
		{
			// this file has package level compression, we won't load it.
			UE_LOG(LogLinker, Warning, TEXT("Failed to read package file summary, the file \"%s\" is has package level compression (and is probably cooked). These old files cannot be loaded in the editor."), *BaseArchive.GetArchiveName());
			Sum.FileVersionUE4 = VER_UE4_OLDEST_LOADABLE_PACKAGE - 1;
			return; // we can't safely load more than this because we just changed the version to something it is not.
		}

		Record << NAMED_ITEM("PackageSource", Sum.PackageSource);

		// No longer used: List of additional packages that are needed to be cooked for this package (ie streaming levels)
		// Keeping the serialization code for backwards compatibility without bumping the package version
		TArray<FString>	AdditionalPackagesToCook;
		Record << NAMED_ITEM("AdditionalPackagesToCook", AdditionalPackagesToCook);

		if (LegacyFileVersion > -7)
		{
			int32 NumTextureAllocations = 0;
			Record << NAMED_ITEM("NumTextureAllocations", NumTextureAllocations);
			// We haven't used texture allocation info for ages and it's no longer supported anyway
			check(NumTextureAllocations == 0);
		}

		Record << NAMED_ITEM("AssetRegistryDataOffset", Sum.AssetRegistryDataOffset);
		Record << NAMED_ITEM("BulkDataStartOffset", Sum.BulkDataStartOffset);

		if (Sum.GetFileVersionUE4() >= VER_UE4_WORLD_LEVEL_INFO)
		{
			Record << NAMED_ITEM("WorldTileInfoDataOffset", Sum.WorldTileInfoDataOffset);
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS)
		{
			Record << NAMED_ITEM("ChunkIDs", Sum.ChunkIDs);
		}
		else if (Sum.GetFileVersionUE4() >= VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE)
		{
			// handle conversion of single ChunkID to an array of ChunkIDs
			if (BaseArchive.IsLoading())
			{
				int ChunkID = -1;
				Record << NAMED_ITEM("ChunkID", ChunkID);

				// don't load <0 entries since an empty array represents the same thing now
				if (ChunkID >= 0)
				{
					Sum.ChunkIDs.Add(ChunkID);
				}
			}
		}
		if (BaseArchive.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
		{
			Record << NAMED_ITEM("PreloadDependencyCount", Sum.PreloadDependencyCount) << NAMED_ITEM("PreloadDependencyOffset", Sum.PreloadDependencyOffset);
		}
		else
		{
			Sum.PreloadDependencyCount = -1;
			Sum.PreloadDependencyOffset = 0;
		}
	}
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	FStructuredArchiveFromArchive(Ar).GetSlot() << Sum;
	return Ar;
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Data/ManifestUObject.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Algo/Transform.h"
#include "BuildPatchManifest.h"

DECLARE_LOG_CATEGORY_EXTERN(LogManifestUObject, Log, All);
DEFINE_LOG_CATEGORY(LogManifestUObject);


// The maximum number of FNames that we expect a manifest to generate. This is not a technical limitation, just a sanity check
// and can be increased if more properties are added to our manifest UObject. FNames are only used by the UOject serialization system.
#define MANIFEST_MAX_NAMES			50

/* FCustomFieldData implementation
*****************************************************************************/
FCustomFieldData::FCustomFieldData()
	: Key(TEXT(""))
	, Value(TEXT(""))
{
}

FCustomFieldData::FCustomFieldData(const FString& InKey, const FString& InValue)
	: Key(InKey)
	, Value(InValue)
{
}

/* FSHAHashData implementation
*****************************************************************************/
FSHAHashData::FSHAHashData()
{
	FMemory::Memset(Hash, 0, FSHA1::DigestSize);
}

/* FChunkInfoData implementation
*****************************************************************************/
FChunkInfoData::FChunkInfoData()
	: Guid()
	, Hash(0)
	, ShaHash()
	, FileSize(0)
	, GroupNumber(0)
{
}

/* FChunkPartData implementation
*****************************************************************************/
FChunkPartData::FChunkPartData()
	: Guid()
	, Offset(0)
	, Size(0)
{
}

/* FFileManifestData implementation
*****************************************************************************/
FFileManifestData::FFileManifestData()
	: Filename(TEXT(""))
	, FileHash()
	, FileChunkParts()
	, bIsUnixExecutable(false)
	, SymlinkTarget(TEXT(""))
	, bIsReadOnly(false)
	, bIsCompressed(false)
{
}

/* UBuildPatchManifest implementation
*****************************************************************************/
UBuildPatchManifest::UBuildPatchManifest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ManifestFileVersion(EBuildPatchAppManifestVersion::Invalid)
	, bIsFileData(false)
	, AppID(INDEX_NONE)
	, AppName(TEXT(""))
	, BuildVersion(TEXT(""))
	, LaunchExe(TEXT(""))
	, LaunchCommand(TEXT(""))
	, PrereqIds()
	, PrereqName(TEXT(""))
	, PrereqPath(TEXT(""))
	, PrereqArgs(TEXT(""))
	, FileManifestList()
	, ChunkList()
	, CustomFields()
{
}

UBuildPatchManifest::~UBuildPatchManifest()
{
}

namespace ManifestUObjectHelpers
{
	FChunkPart FromChunkPartData(const FChunkPartData& Input)
	{
		FChunkPart Output;
		Output.Guid = Input.Guid;
		Output.Offset = Input.Offset;
		Output.Size = Input.Size;
		return Output;
	}

	FFileManifest FromFileManifestData(const FFileManifestData& Input)
	{
		FFileManifest Output;
		Output.Filename = Input.Filename;
		FMemory::Memcpy(Output.FileHash.Hash, Input.FileHash.Hash, FSHA1::DigestSize);
		Output.FileChunkParts.Empty(Input.FileChunkParts.Num());
		Algo::Transform(Input.FileChunkParts, Output.FileChunkParts, &ManifestUObjectHelpers::FromChunkPartData);
		Output.InstallTags = Input.InstallTags;
		Output.bIsUnixExecutable = Input.bIsUnixExecutable;
		Output.SymlinkTarget = Input.SymlinkTarget;
		Output.bIsReadOnly = Input.bIsReadOnly;
		Output.bIsCompressed = Input.bIsCompressed;
		return Output;
	}

	FChunkInfo FromChunkInfoData(const FChunkInfoData& Input)
	{
		FChunkInfo Output;
		Output.Guid = Input.Guid;
		Output.Hash = Input.Hash;
		FMemory::Memcpy(Output.ShaHash.Hash, Input.ShaHash.Hash, FSHA1::DigestSize);
		Output.FileSize = Input.FileSize;
		Output.GroupNumber = Input.GroupNumber;
		return Output;
	}

	FBuildPatchAppManifest::FCustomField FromCustomFieldData(const FCustomFieldData& Input)
	{
		FBuildPatchAppManifest::FCustomField Output;
		Output.Key = Input.Key;
		Output.Value = Input.Value;
		return Output;
	}

	FChunkPartData ToChunkPartData(const FChunkPart& Input)
	{
		FChunkPartData Output;
		Output.Guid = Input.Guid;
		Output.Offset = Input.Offset;
		Output.Size = Input.Size;
		return Output;
	}

	FFileManifestData ToFileManifestData(const FFileManifest& Input)
	{
		FFileManifestData Output;
		Output.Filename = Input.Filename;
		FMemory::Memcpy(Output.FileHash.Hash, Input.FileHash.Hash, FSHA1::DigestSize);
		Output.FileChunkParts.Empty(Input.FileChunkParts.Num());
		Algo::Transform(Input.FileChunkParts, Output.FileChunkParts, &ManifestUObjectHelpers::ToChunkPartData);
		Output.InstallTags = Input.InstallTags;
		Output.bIsUnixExecutable = Input.bIsUnixExecutable;
		Output.SymlinkTarget = Input.SymlinkTarget;
		Output.bIsReadOnly = Input.bIsReadOnly;
		Output.bIsCompressed = Input.bIsCompressed;
		return Output;
	}

	FChunkInfoData ToChunkInfoData(const FChunkInfo& Input)
	{
		FChunkInfoData Output;
		Output.Guid = Input.Guid;
		Output.Hash = Input.Hash;
		FMemory::Memcpy(Output.ShaHash.Hash, Input.ShaHash.Hash, FSHA1::DigestSize);
		Output.FileSize = Input.FileSize;
		Output.GroupNumber = Input.GroupNumber;
		return Output;
	}

	FCustomFieldData ToCustomFieldData(const FBuildPatchAppManifest::FCustomField& Input)
	{
		FCustomFieldData Output;
		Output.Key = Input.Key;
		Output.Value = Input.Value;
		return Output;
	}
}

/**
 * Archive for writing a manifest into memory
 */
class FManifestWriter : public FArchive
{
public:
	FManifestWriter()
		: FArchive()
		, Offset(0)
	{
		this->SetIsPersistent(false);
		this->SetIsSaving(true);
	}

	virtual void Seek(int64 InPos) override
	{
		Offset = InPos;
	}

	virtual int64 Tell() override
	{
		return Offset;
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FManifestWriter");
	}

	virtual FArchive& operator<<(FName& N) override
	{
		if (!FNameIndexLookup.Contains(N))
		{
			const int32 ArNameIndex = FNameIndexLookup.Num();
			FNameIndexLookup.Add(N, ArNameIndex);
		}
		*this << FNameIndexLookup[N];
		return *this;
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num && !ArIsError)
		{
			const int64 NumBytesToAdd = Offset + Num - Bytes.Num();
			if (NumBytesToAdd > 0)
			{
				const int64 NewArrayCount = Bytes.Num() + NumBytesToAdd;
				if (NewArrayCount >= MAX_int32)
				{
					ArIsError = true;
					return;
				}
				Bytes.AddUninitialized((int32)NumBytesToAdd);
			}
			check((Offset + Num) <= Bytes.Num());
			FMemory::Memcpy(&Bytes[Offset], Data, Num);
			Offset += Num;
		}
	}

	virtual int64 TotalSize() override
	{
		return Bytes.Num();
	}

	void Finalize()
	{
		TArray<uint8> FinalData;
		FMemoryWriter NameTableWriter(FinalData);
		int32 NumNames = FNameIndexLookup.Num();
		check(NumNames <= MANIFEST_MAX_NAMES);
		NameTableWriter << NumNames;
		for (auto& MapEntry : FNameIndexLookup)
		{
			FName& Name = MapEntry.Key;
			int32& Index = MapEntry.Value;
			NameTableWriter << Name;
			NameTableWriter << Index;
		}
		FinalData.Append(Bytes);
		Bytes = MoveTemp(FinalData);
	}

	TArray<uint8>& GetBytes()
	{
		return Bytes;
	}

private:
	int64 Offset;
	TArray<uint8> Bytes;
	TMap<FName, int32> FNameIndexLookup;
};

/**
* Archive for reading a manifest from data in memory
*/
class FManifestReader : public FArchive
{
public:
	FManifestReader(const TArray<uint8>& InBytes)
		: FArchive()
		, Offset(0)
		, Bytes(InBytes)
	{
		this->SetIsPersistent(false);
		this->SetIsLoading(true);

		// Must load table immediately
		FMemoryReader NameTableReader(InBytes);
		int32 NumNames;
		NameTableReader << NumNames;

		// Check not insane, we know to expect a small number for a manifest
		if (NumNames < MANIFEST_MAX_NAMES)
		{
			FNameLookup.Empty(NumNames);
			for (; NumNames > 0; --NumNames)
			{
				FName Name;
				int32 Index;
				NameTableReader << Name;
				NameTableReader << Index;
				FNameLookup.Add(Index, Name);
			}
		}
		else
		{
			ArIsError = true;
		}
		Offset = NameTableReader.Tell();
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FManifestReader");
	}

	virtual void Seek(int64 InPos) override
	{
		check(InPos <= Bytes.Num());
		Offset = InPos;
	}

	virtual int64 Tell() override
	{
		return Offset;
	}

	virtual FArchive& operator<<(FName& N) override
	{
		if (ArIsError)
		{
			N = NAME_None;
		}
		else
		{
			// Read index and lookup
			int32 ArNameIndex;
			*this << ArNameIndex;
			if (FNameLookup.Contains(ArNameIndex))
			{
				N = FNameLookup[ArNameIndex];
			}
			else
			{
				N = NAME_None;
				ArIsError = true;
			}
		}
		return *this;
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Num <= Bytes.Num())
			{
				FMemory::Memcpy(Data, &Bytes[Offset], Num);
				Offset += Num;
			}
			else
			{
				ArIsError = true;
			}
		}
	}

	virtual int64 TotalSize() override
	{
		return Bytes.Num();
	}

private:
	int64 Offset;
	const TArray<uint8>& Bytes;
	TMap<int32, FName> FNameLookup;
};

/* FManifestUObject implementation
*****************************************************************************/

bool FManifestUObject::LoadFromMemory(const TArray<uint8>& DataInput, FBuildPatchAppManifest& AppManifest)
{
#if !BUILDPATCHSERVICES_NOUOBJECT

	FMemoryReader ManifestFile(DataInput);
	FManifestFileHeader Header;
	ManifestFile << Header;
	const int32 SignedHeaderSize = Header.HeaderSize;
	if (Header.CheckMagic() && DataInput.Num() > SignedHeaderSize)
	{
		FSHAHash DataHash;
		FSHA1::HashBuffer(&DataInput[Header.HeaderSize], DataInput.Num() - Header.HeaderSize, DataHash.Hash);
		if (DataHash == Header.SHAHash)
		{
			TArray<uint8> UncompressedData;
			if (Header.StoredAs == EManifestFileHeader::STORED_COMPRESSED && (Header.CompressedSize + Header.HeaderSize) == DataInput.Num())
			{
				UncompressedData.AddUninitialized(Header.DataSize);
				if (!FCompression::UncompressMemory(
					static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
					UncompressedData.GetData(),
					Header.DataSize,
					&DataInput[Header.HeaderSize],
					DataInput.Num() - Header.HeaderSize))
				{
					return false;
				}
			}
			else if ((Header.DataSize + Header.HeaderSize) == DataInput.Num())
			{
				UncompressedData.Append(&DataInput[Header.HeaderSize], Header.DataSize);
			}
			else
			{
				return false;
			}
			FManifestReader ManifestData(UncompressedData);
			return LoadInternal(ManifestData, AppManifest);
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

#else // !BUILDPATCHSERVICES_NOUOBJECT

	UE_LOG(LogManifestUObject, Error, TEXT("FManifestUObject::LoadFromMemory called but UObjects are disabled for BuildPatchServices"));
	return false;

#endif // !BUILDPATCHSERVICES_NOUOBJECT
}

bool FManifestUObject::SaveToArchive(FArchive& Ar, const FBuildPatchAppManifest& AppManifest)
{
#if !BUILDPATCHSERVICES_NOUOBJECT

	if (Ar.IsSaving())
	{
		FManifestWriter ManifestData;
		bool bSaveOk = SaveInternal(ManifestData, AppManifest);
		ManifestData.Finalize();
		if (bSaveOk && !ManifestData.IsError())
		{
			int32 DataSize = ManifestData.TotalSize();
			TArray<uint8> TempCompressed;
			TempCompressed.AddUninitialized(DataSize);
			int32 CompressedSize = DataSize;
			bool bDataIsCompressed = FCompression::CompressMemory(
				static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				TempCompressed.GetData(),
				CompressedSize,
				ManifestData.GetBytes().GetData(),
				DataSize);
			TempCompressed.SetNum(CompressedSize);

			TArray<uint8>& FileData = bDataIsCompressed ? TempCompressed : ManifestData.GetBytes();

			FManifestFileHeader Header;
			Header.StoredAs = bDataIsCompressed ? EManifestFileHeader::STORED_COMPRESSED : EManifestFileHeader::STORED_RAW;
			Header.DataSize = DataSize;
			Header.CompressedSize = bDataIsCompressed ? CompressedSize : 0;
			FSHA1::HashBuffer(FileData.GetData(), FileData.Num(), Header.SHAHash.Hash);

			// Write to provided archive
			Ar << Header;
			Header.HeaderSize = Ar.Tell();
			Ar.Seek(0);
			Ar << Header;
			Ar.Serialize(FileData.GetData(), FileData.Num());
		}
		else
		{
			Ar.SetError();
		}
		return !Ar.IsError();
	}
	Ar.SetError();
	return false;

#else // !BUILDPATCHSERVICES_NOUOBJECT

	UE_LOG(LogManifestUObject, Error, TEXT("FManifestUObject::SaveToArchive called but UObjects are disabled for BuildPatchServices"));
	Ar.SetError();
	return false;

#endif // !BUILDPATCHSERVICES_NOUOBJECT
}

bool FManifestUObject::LoadInternal(FArchive& Ar, FBuildPatchAppManifest& AppManifest)
{
#if !BUILDPATCHSERVICES_NOUOBJECT

	UBuildPatchManifest* Data = NewObject<UBuildPatchManifest>();
	Data->AddToRoot();

	// Make sure we use the correct serialization version, this is now fixed and must never use a newer version,
	// because the property tag has changed in structure meaning older clients would not read correctly.
	Ar.SetUE4Ver(VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG - 1);

	if (Ar.IsLoading())
	{
		Data->Serialize(Ar);

		AppManifest.DestroyData();
		AppManifest.ManifestFileVersion = MoveTemp(Data->ManifestFileVersion);
		AppManifest.bIsFileData = MoveTemp(Data->bIsFileData);
		AppManifest.AppID = MoveTemp(Data->AppID);
		AppManifest.AppName = MoveTemp(Data->AppName);
		AppManifest.BuildVersion = MoveTemp(Data->BuildVersion);
		AppManifest.LaunchExe = MoveTemp(Data->LaunchExe);
		AppManifest.LaunchCommand = MoveTemp(Data->LaunchCommand);
		AppManifest.PrereqIds = MoveTemp(Data->PrereqIds);
		AppManifest.PrereqName = MoveTemp(Data->PrereqName);
		AppManifest.PrereqPath = MoveTemp(Data->PrereqPath);
		AppManifest.PrereqArgs = MoveTemp(Data->PrereqArgs);

		AppManifest.FileManifestList.Empty(Data->FileManifestList.Num());
		Algo::Transform(Data->FileManifestList, AppManifest.FileManifestList, &ManifestUObjectHelpers::FromFileManifestData);

		AppManifest.ChunkList.Empty(Data->ChunkList.Num());
		Algo::Transform(Data->ChunkList, AppManifest.ChunkList, &ManifestUObjectHelpers::FromChunkInfoData);

		AppManifest.CustomFields.Empty(Data->CustomFields.Num());
		Algo::Transform(Data->CustomFields, AppManifest.CustomFields, &ManifestUObjectHelpers::FromCustomFieldData);

		// If we didn't load the version number, we know it was skipped when saving therefore must be
		// the first UObject version
		if (AppManifest.ManifestFileVersion == static_cast<uint8>(EBuildPatchAppManifestVersion::Invalid))
		{
			AppManifest.ManifestFileVersion = EBuildPatchAppManifestVersion::StoredAsCompressedUClass;
		}

		// Setup internal lookups
		AppManifest.InitLookups();
	}
	else
	{
		Ar.SetError();
	}

	// Clear data to reduce memory usages before GC occurs
	Data->AppName.Empty();
	Data->BuildVersion.Empty();
	Data->LaunchExe.Empty();
	Data->LaunchCommand.Empty();
	Data->PrereqIds.Empty();
	Data->PrereqName.Empty();
	Data->PrereqPath.Empty();
	Data->PrereqArgs.Empty();
	Data->FileManifestList.Empty();
	Data->ChunkList.Empty();
	Data->CustomFields.Empty();

	Data->RemoveFromRoot();
	return !Ar.IsError();

#else // !BUILDPATCHSERVICES_NOUOBJECT

	UE_LOG(LogManifestUObject, Error, TEXT("FManifestUObject::SerializeInternal called but UObjects are disabled for BuildPatchServices"));
	return false;

#endif // !BUILDPATCHSERVICES_NOUOBJECT
}

bool FManifestUObject::SaveInternal(FArchive& Ar, const FBuildPatchAppManifest& AppManifest)
{
#if !BUILDPATCHSERVICES_NOUOBJECT

	UBuildPatchManifest* Data = NewObject<UBuildPatchManifest>();
	Data->AddToRoot();

	// Make sure we use the correct serialization version, this is now fixed and must never use a newer version,
	// because the property tag has changed in structure meaning older clients would not read correctly.
	Ar.SetUE4Ver(VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG - 1);

	if (Ar.IsLoading())
	{
		Ar.SetError();
	}
	else
	{
		Data->ManifestFileVersion = AppManifest.ManifestFileVersion;
		Data->bIsFileData = AppManifest.bIsFileData;
		Data->AppID = AppManifest.AppID;
		Data->AppName = AppManifest.AppName;
		Data->BuildVersion = AppManifest.BuildVersion;
		Data->LaunchExe = AppManifest.LaunchExe;
		Data->LaunchCommand = AppManifest.LaunchCommand;
		Data->PrereqIds = AppManifest.PrereqIds;
		Data->PrereqName = AppManifest.PrereqName;
		Data->PrereqPath = AppManifest.PrereqPath;
		Data->PrereqArgs = AppManifest.PrereqArgs;

		Data->FileManifestList.Empty(AppManifest.FileManifestList.Num());
		Algo::Transform(AppManifest.FileManifestList, Data->FileManifestList, &ManifestUObjectHelpers::ToFileManifestData);

		Data->ChunkList.Empty(AppManifest.ChunkList.Num());
		Algo::Transform(AppManifest.ChunkList, Data->ChunkList, &ManifestUObjectHelpers::ToChunkInfoData);

		Data->CustomFields.Empty(AppManifest.CustomFields.Num());
		Algo::Transform(AppManifest.CustomFields, Data->CustomFields, &ManifestUObjectHelpers::ToCustomFieldData);

		Data->Serialize(Ar);
	}

	// Clear data to reduce memory usages before GC occurs
	Data->AppName.Empty();
	Data->BuildVersion.Empty();
	Data->LaunchExe.Empty();
	Data->LaunchCommand.Empty();
	Data->PrereqIds.Empty();
	Data->PrereqName.Empty();
	Data->PrereqPath.Empty();
	Data->PrereqArgs.Empty();
	Data->FileManifestList.Empty();
	Data->ChunkList.Empty();
	Data->CustomFields.Empty();

	Data->RemoveFromRoot();
	return !Ar.IsError();

#else // !BUILDPATCHSERVICES_NOUOBJECT

	UE_LOG(LogManifestUObject, Error, TEXT("FManifestUObject::SerializeInternal called but UObjects are disabled for BuildPatchServices"));
	return false;

#endif // !BUILDPATCHSERVICES_NOUOBJECT
}

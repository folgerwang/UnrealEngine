// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertPackageLedger.h"

#include "ConcertWorkspaceData.h"
#include "ConcertFileCache.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace ConcertPackageLedgerUtil
{

const int32 MinLedgerFilesToCache = 10;
const uint64 MaxLedgerFileSizeBytesToCache = 200 * 1024 * 1024;

const FString LedgerEntryExtension = TEXT("upackage");
const FGuid LedgerEntryFooter = FGuid(0x2EFC8CDD, 0x748E46C0, 0xA5485769, 0x13A3C354);

FString GetPackageFilename(const FString& InLedgerPath, const FString InPackageName, const uint32 InRevision)
{
	return InLedgerPath / InPackageName + FString::Printf(TEXT("_%s.%s"), *LexToString(InRevision), *LedgerEntryExtension);
}

FString GetPackageFilename(const FString& InLedgerPath, const FName InPackageName, const uint32 InRevision)
{
	return GetPackageFilename(InLedgerPath, InPackageName.ToString(), InRevision);
}

bool WritePackage(const FConcertPackageInfo& InPackageInfo, const TArray<uint8>& InPackageData, TArray<uint8>& OutSerializedPackageData)
{
	FMemoryWriter Ar(OutSerializedPackageData);

	// Serialize the info (header)
	int64 BodyOffset = 0;
	Ar << BodyOffset;
	FConcertPackageInfo::StaticStruct()->SerializeItem(Ar, const_cast<FConcertPackageInfo*>(&InPackageInfo), nullptr);

	// Serialize the raw data (body)
	BodyOffset = Ar.Tell();
	Ar.Seek(0);
	Ar << BodyOffset;
	Ar.Seek(BodyOffset);
	uint32 UncompressedPackageSize = InPackageData.Num();
	Ar.SerializeIntPacked(UncompressedPackageSize);
	if (UncompressedPackageSize > 0)
	{
		Ar.SerializeCompressed((uint8*)InPackageData.GetData(), UncompressedPackageSize, NAME_Zlib);
	}

	// Serialize the footer so we know we didn't crash mid-write
	FGuid SerializedFooter = LedgerEntryFooter;
	Ar << SerializedFooter;

	return !Ar.IsError();
}

bool ReadPackage(const TArray<uint8>& InSerializedPackageData, FConcertPackageInfo* OutPackageInfo, TArray<uint8>* OutPackageData)
{
	FMemoryReader Ar(InSerializedPackageData);

	// Test the footer is in place so we know we didn't crash mid-write
	bool bParsedFooter = false;
	{
		const int64 SerializedTransactionSize = Ar.TotalSize();
		if (SerializedTransactionSize >= sizeof(FGuid))
		{
			FGuid SerializedFooter;
			Ar.Seek(SerializedTransactionSize - sizeof(FGuid));
			Ar << SerializedFooter;
			Ar.Seek(0);
			bParsedFooter = SerializedFooter == LedgerEntryFooter;
		}
	}
	if (!bParsedFooter)
	{
		return false;
	}

	// Deserialize the info (header)
	int64 BodyOffset = 0;
	Ar << BodyOffset;
	if (OutPackageInfo)
	{
		FConcertPackageInfo::StaticStruct()->SerializeItem(Ar, OutPackageInfo, nullptr);
	}

	// Deserialize the raw data (body)
	if (OutPackageData)
	{
		Ar.Seek(BodyOffset);

		uint32 UncompressedPackageSize = 0;
		Ar.SerializeIntPacked(UncompressedPackageSize);
		OutPackageData->Reset(UncompressedPackageSize);
		OutPackageData->AddZeroed(UncompressedPackageSize);
		if (UncompressedPackageSize > 0)
		{
			Ar.SerializeCompressed(OutPackageData->GetData(), UncompressedPackageSize, NAME_Zlib);
		}
	}

	return !Ar.IsError();
}

}


FConcertPackageLedger::FConcertPackageLedger(const EConcertPackageLedgerType InLedgerType, const FString& InLedgerPath)
	: LedgerType(InLedgerType)
	, LedgerPath(InLedgerPath / TEXT("Packages"))
	, LedgerFileCache(MakeShared<FConcertFileCache>(ConcertPackageLedgerUtil::MinLedgerFilesToCache, ConcertPackageLedgerUtil::MaxLedgerFileSizeBytesToCache))
{
	checkf(!InLedgerPath.IsEmpty(), TEXT("Ledger Path must not be empty!"));

	if (InLedgerType == EConcertPackageLedgerType::Transient)
	{
		ClearLedger();
	}
	else if (LedgerType != EConcertPackageLedgerType::Persistent)
	{
		checkf(false, TEXT("Unknown EConcertPackageLedgerType!"));
	}
}

FConcertPackageLedger::~FConcertPackageLedger()
{
	if (LedgerType == EConcertPackageLedgerType::Transient)
	{
		ClearLedger();
	}
}

const FString& FConcertPackageLedger::GetLedgerPath() const
{
	return LedgerPath;
}

const FString& FConcertPackageLedger::GetLedgerEntryExtension() const
{
	return ConcertPackageLedgerUtil::LedgerEntryExtension;
}

bool FConcertPackageLedger::LoadLedger()
{
	class FConcertPackageVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FConcertPackageVisitor(FConcertPackageLedger* InThis)
			: This(InThis)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				const FString Filename = FilenameOrDirectory;
				if (FPaths::GetExtension(Filename) == ConcertPackageLedgerUtil::LedgerEntryExtension)
				{
					// Extract the revision from the filename
					uint32 Revision = 0;
					{
						const FString BaseFilename = FPaths::GetBaseFilename(Filename);

						int32 RevisionSeparatorIndex = 0;
						if (BaseFilename.FindLastChar(TEXT('_'), RevisionSeparatorIndex))
						{
							LexFromString(Revision, &BaseFilename[RevisionSeparatorIndex + 1]);
						}
					}

					// Track which package this revision belongs to
					{
						TArray<uint8> SerializedPackageData;
						if (This->LedgerFileCache->FindOrCacheFile(Filename, SerializedPackageData))
						{
							FConcertPackageInfo PackageInfo;
							if (ConcertPackageLedgerUtil::ReadPackage(SerializedPackageData, &PackageInfo, nullptr))
							{
								uint32& HeadRevision = This->PackageHeadRevisions.FindOrAdd(PackageInfo.PackageName);
								HeadRevision = FMath::Max(HeadRevision, Revision);
							}
						}
					}
				}
			}
			return true;
		}
	private:
		FConcertPackageLedger* This;
	};

	PackageHeadRevisions.Reset();

	FConcertPackageVisitor ConcertPackageVisitor(this);
	IFileManager::Get().IterateDirectoryRecursively(*LedgerPath, ConcertPackageVisitor);

	return PackageHeadRevisions.Num() > 0;
}

void FConcertPackageLedger::ClearLedger()
{
	IFileManager::Get().DeleteDirectory(*LedgerPath, false, true);
}

uint32 FConcertPackageLedger::AddPackage(const FConcertPackage& InPackage)
{
	return AddPackage(InPackage.Info, InPackage.PackageData);
}

uint32 FConcertPackageLedger::AddPackage(const FConcertPackageInfo& InPackageInfo, const TArray<uint8>& InPackageData)
{
	uint32 RevisionToAdd = 0;
	if (const uint32* FoundHeadRevision = PackageHeadRevisions.Find(InPackageInfo.PackageName))
	{
		RevisionToAdd = (*FoundHeadRevision) + 1;
	}

	AddPackage(RevisionToAdd, InPackageInfo, InPackageData);
	
	return RevisionToAdd;
}

void FConcertPackageLedger::AddPackage(const uint32 InRevision, const FConcertPackage& InPackage)
{
	return AddPackage(InRevision, InPackage.Info, InPackage.PackageData);
}

void FConcertPackageLedger::AddPackage(const uint32 InRevision, const FConcertPackageInfo& InPackageInfo, const TArray<uint8>& InPackageData)
{
	uint32& HeadRevision = PackageHeadRevisions.FindOrAdd(InPackageInfo.PackageName);
	HeadRevision = FMath::Max(HeadRevision, InRevision);

	TArray<uint8> SerializedPackageData;
	if (ConcertPackageLedgerUtil::WritePackage(InPackageInfo, InPackageData, SerializedPackageData))
	{
		LedgerFileCache->SaveAndCacheFile(ConcertPackageLedgerUtil::GetPackageFilename(LedgerPath, InPackageInfo.PackageName, InRevision), MoveTemp(SerializedPackageData));
	}
}

bool FConcertPackageLedger::FindPackage(const FName InPackageName, FConcertPackage& OutPackage, const uint32* InRevision) const
{
	return FindPackage(InPackageName, &OutPackage.Info, &OutPackage.PackageData, InRevision);
}

bool FConcertPackageLedger::FindPackage(const FName InPackageName, FConcertPackageInfo* OutPackageInfo, TArray<uint8>* OutPackageData, const uint32* InRevision) const
{
	const uint32 RevisionToFind = InRevision ? *InRevision : PackageHeadRevisions.FindRef(InPackageName);

	TArray<uint8> SerializedPackageData;
	return LedgerFileCache->FindOrCacheFile(ConcertPackageLedgerUtil::GetPackageFilename(LedgerPath, InPackageName, RevisionToFind), SerializedPackageData) 
		&& ConcertPackageLedgerUtil::ReadPackage(SerializedPackageData, OutPackageInfo, OutPackageData);
}

TArray<FName> FConcertPackageLedger::GetAllPackageNames() const
{
	TArray<FName> PackageNames;
	PackageHeadRevisions.GenerateKeyArray(PackageNames);
	return PackageNames;
}

bool FConcertPackageLedger::GetPackageHeadRevision(const FName InPackageName, uint32& OutRevision) const
{
	if (const uint32* FoundHeadRevision = PackageHeadRevisions.Find(InPackageName))
	{
		OutRevision = *FoundHeadRevision;
		return true;
	}
	return false;
}

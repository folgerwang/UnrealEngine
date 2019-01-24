// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertTransactionLedger.h"

#include "ConcertFileCache.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/StructOnScope.h"

namespace ConcertTransactionLedgerUtil
{

const int32 MinLedgerFilesToCache = 10;
const uint64 MaxLedgerFileSizeBytesToCache = 50 * 1024 * 1024;

const FString LedgerEntryExtension = TEXT("utrans");
const FGuid LedgerEntryFooter = FGuid(0xE473C070, 0x65DA42BF, 0xA0607C78, 0xE0DC47CF);

FString GetTransactionFilename(const FString& InLedgerPath, const uint64 InIndex)
{
	return InLedgerPath / FString::Printf(TEXT("%s.%s"), *LexToString(InIndex), *LedgerEntryExtension);
}

bool WriteTransactionData(const FStructOnScope& InTransaction, TArray<uint8>& OutSerializedTransactionData)
{
	FMemoryWriter Ar(OutSerializedTransactionData);

	const UScriptStruct* TransactionType = CastChecked<const UScriptStruct>(InTransaction.GetStruct());

	FString TransactionTypeStr = TransactionType->GetPathName();
	Ar << TransactionTypeStr;
	const_cast<UScriptStruct*>(TransactionType)->SerializeItem(Ar, (void*)InTransaction.GetStructMemory(), nullptr);

	return !Ar.IsError();
}

bool WriteTransaction(const FStructOnScope& InTransaction, TArray<uint8>& OutSerializedTransactionData)
{
	check(InTransaction.IsValid());

	FMemoryWriter Ar(OutSerializedTransactionData);

	// Write the raw transaction data
	TArray<uint8> UncompressedTransaction;
	if (!WriteTransactionData(InTransaction, UncompressedTransaction))
	{
		return false;
	}

	// Serialize the raw transaction
	uint32 UncompressedTransactionSize = UncompressedTransaction.Num();
	Ar.SerializeIntPacked(UncompressedTransactionSize);
	if (UncompressedTransactionSize > 0)
	{
		Ar.SerializeCompressed(UncompressedTransaction.GetData(), UncompressedTransactionSize, NAME_Zlib);
	}

	// Serialize the footer so we know we didn't crash mid-write
	FGuid SerializedFooter = LedgerEntryFooter;
	Ar << SerializedFooter;

	return !Ar.IsError();
}

bool ReadTransactionData(const TArray<uint8>& InSerializedTransactionData, FStructOnScope& OutTransaction)
{
	FMemoryReader Ar(InSerializedTransactionData);

	// Deserialize the transaction
	UScriptStruct* TransactionType = nullptr;
	{
		FString TransactionTypeStr;
		Ar << TransactionTypeStr;
		TransactionType = LoadObject<UScriptStruct>(nullptr, *TransactionTypeStr);
		if (!TransactionType)
		{
			return false;
		}
	}
	if (OutTransaction.IsValid())
	{
		// If were given an existing transaction to fill with data, then the type must match
		if (TransactionType != OutTransaction.GetStruct())
		{
			return false;
		}
	}
	else
	{
		OutTransaction.Initialize(TransactionType);
	}
	TransactionType->SerializeItem(Ar, OutTransaction.GetStructMemory(), nullptr);

	return !Ar.IsError();
}

bool ReadTransaction(const TArray<uint8>& InSerializedTransactionData, FStructOnScope& OutTransaction)
{
	FMemoryReader Ar(InSerializedTransactionData);

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

	// Deserialize the raw transaction
	uint32 UncompressedTransactionSize = 0;
	Ar.SerializeIntPacked(UncompressedTransactionSize);
	TArray<uint8> UncompressedTransaction;
	UncompressedTransaction.AddZeroed(UncompressedTransactionSize);
	if (UncompressedTransactionSize > 0)
	{
		Ar.SerializeCompressed(UncompressedTransaction.GetData(), UncompressedTransactionSize, NAME_Zlib);
	}

	// Read the raw transaction data
	if (!ReadTransactionData(UncompressedTransaction, OutTransaction))
	{
		return false;
	}

	return !Ar.IsError();
}

}


FConcertTransactionLedger::FConcertTransactionLedger(const EConcertTransactionLedgerType InLedgerType, const FString& InLedgerPath)
	: LedgerType(InLedgerType)
	, LedgerPath(InLedgerPath / TEXT("Transactions"))
	, NextTransactionIndex(0)
	, LedgerFileCache(MakeUnique<FConcertFileCache>(ConcertTransactionLedgerUtil::MinLedgerFilesToCache, ConcertTransactionLedgerUtil::MaxLedgerFileSizeBytesToCache))
{
	checkf(!LedgerPath.IsEmpty(), TEXT("Ledger Path must not be empty!"));

	if (LedgerType == EConcertTransactionLedgerType::Transient)
	{
		ClearLedger();
	}
	else if (LedgerType != EConcertTransactionLedgerType::Persistent)
	{
		checkf(false, TEXT("Unknown EConcertTransactionLedgerType!"));
	}
}

FConcertTransactionLedger::~FConcertTransactionLedger()
{
	if (LedgerType == EConcertTransactionLedgerType::Transient)
	{
		ClearLedger();
	}
}

const FString& FConcertTransactionLedger::GetLedgerPath() const
{
	return LedgerPath;
}

const FString& FConcertTransactionLedger::GetLedgerEntryExtension() const
{
	return ConcertTransactionLedgerUtil::LedgerEntryExtension;
}

uint64 FConcertTransactionLedger::GetNextTransactionIndex() const
{
	return NextTransactionIndex;
}

bool FConcertTransactionLedger::LoadLedger()
{
	class FConcertTransactionVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FConcertTransactionVisitor(FConcertTransactionLedger* InThis)
			: This(InThis)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				const FString Filename = FilenameOrDirectory;
				if (FPaths::GetExtension(Filename) == ConcertTransactionLedgerUtil::LedgerEntryExtension)
				{
					const FString CurrentTransactionIndexStr = FPaths::GetBaseFilename(Filename);
					uint64 CurrentTransactionIndex = 0;
					LexFromString(CurrentTransactionIndex, *CurrentTransactionIndexStr);
					This->NextTransactionIndex = FMath::Max(This->NextTransactionIndex, CurrentTransactionIndex + 1);

					// Track which packages this transaction belongs to
					{
						FStructOnScope Transaction;
						if (This->LoadTransaction(Filename, Transaction))
						{
							This->TrackLiveTransaction(CurrentTransactionIndex, (const FConcertTransactionEventBase*)Transaction.GetStructMemory());
						}
					}
				}
			}
			return true;
		}
	private:
		FConcertTransactionLedger* This;
	};

	NextTransactionIndex = 0;
	LivePackageTransactions.Reset();

	FConcertTransactionVisitor ConcertTransactionVisitor(this);
	IFileManager::Get().IterateDirectory(*LedgerPath, ConcertTransactionVisitor);
	
	return NextTransactionIndex > 0;
}

void FConcertTransactionLedger::ClearLedger()
{
	NextTransactionIndex = 0;
	IFileManager::Get().DeleteDirectory(*LedgerPath, false, true);
}

FOnAddFinalizedTransaction& FConcertTransactionLedger::OnAddFinalizedTransaction()
{
	return OnAddFinalizedTransactionDelegate;
}

FOnLiveTransactionsTrimmed& FConcertTransactionLedger::OnLiveTransactionsTrimmed()
{
	return OnLiveTransactionsTrimmedDelegate;
}

uint64 FConcertTransactionLedger::AddTransaction(const UScriptStruct* InTransactionType, const void* InTransactionData)
{
	checkf(InTransactionType->IsChildOf(FConcertTransactionEventBase::StaticStruct()), TEXT("AddTransaction can only be used with types deriving from FConcertTransactionEventBase"));

	const uint64 TransactionIndex = NextTransactionIndex++;
	AddTransaction(TransactionIndex, InTransactionType, InTransactionData);
	return TransactionIndex;
}

uint64 FConcertTransactionLedger::AddSerializedTransaction(const TArray<uint8>& InTransactionData)
{
	const uint64 TransactionIndex = NextTransactionIndex++;
	AddSerializedTransaction(TransactionIndex, InTransactionData);
	return TransactionIndex;
}

void FConcertTransactionLedger::AddTransaction(const uint64 InIndex, const UScriptStruct* InTransactionType, const void* InTransactionData)
{
	checkf(InTransactionType->IsChildOf(FConcertTransactionEventBase::StaticStruct()), TEXT("AddTransaction can only be used with types deriving from FConcertTransactionEventBase"));

	// Track which packages this transaction belongs to
	TrackLiveTransaction(InIndex, (const FConcertTransactionEventBase*)InTransactionData);

	NextTransactionIndex = FMath::Max(NextTransactionIndex, InIndex + 1);

	if (InTransactionType->IsChildOf(FConcertTransactionFinalizedEvent::StaticStruct()))
	{
		OnAddFinalizedTransactionDelegate.Broadcast(*(FConcertTransactionFinalizedEvent*) InTransactionData, InIndex);
	}

	FStructOnScope Transaction(InTransactionType, (uint8*)InTransactionData);
	SaveTransaction(ConcertTransactionLedgerUtil::GetTransactionFilename(LedgerPath, InIndex), Transaction);
}

void FConcertTransactionLedger::AddSerializedTransaction(const uint64 InIndex, const TArray<uint8>& InTransactionData)
{
	FStructOnScope Transaction;
	if (ConcertTransactionLedgerUtil::ReadTransaction(InTransactionData, Transaction))
	{
		if (ensureAlwaysMsgf(Transaction.GetStruct()->IsChildOf(FConcertTransactionEventBase::StaticStruct()), TEXT("AddSerializedTransaction can only be used with types deriving from FConcertTransactionEventBase")))
		{
			// Track which packages this transaction belongs to
			TrackLiveTransaction(InIndex, (const FConcertTransactionEventBase*)Transaction.GetStructMemory());

			NextTransactionIndex = FMath::Max(NextTransactionIndex, InIndex + 1);

			if (Transaction.GetStruct()->IsChildOf(FConcertTransactionFinalizedEvent::StaticStruct()))
			{
				OnAddFinalizedTransactionDelegate.Broadcast(*(FConcertTransactionFinalizedEvent*)Transaction.GetStructMemory(), InIndex);
			}

			LedgerFileCache->SaveAndCacheFile(ConcertTransactionLedgerUtil::GetTransactionFilename(LedgerPath, InIndex), CopyTemp(InTransactionData));
		}
	}
}

bool FConcertTransactionLedger::FindTransaction(const uint64 InIndex, const UScriptStruct* InTransactionType, void* OutTransactionData) const
{
	checkf(InTransactionType->IsChildOf(FConcertTransactionEventBase::StaticStruct()), TEXT("FindTransaction can only be used with types deriving from FConcertTransactionEventBase"));

	FStructOnScope Transaction(InTransactionType, (uint8*)OutTransactionData);
	return LoadTransaction(ConcertTransactionLedgerUtil::GetTransactionFilename(LedgerPath, InIndex), Transaction);
}

bool FConcertTransactionLedger::FindTransaction(const uint64 InIndex, FStructOnScope& OutTransaction) const
{
	OutTransaction.Reset();
	return LoadTransaction(ConcertTransactionLedgerUtil::GetTransactionFilename(LedgerPath, InIndex), OutTransaction);
}

bool FConcertTransactionLedger::FindSerializedTransaction(const uint64 InIndex, TArray<uint8>& OutTransactionData) const
{
	return LedgerFileCache->FindOrCacheFile(ConcertTransactionLedgerUtil::GetTransactionFilename(LedgerPath, InIndex), OutTransactionData);
}

TArray<uint64> FConcertTransactionLedger::GetAllLiveTransactions() const
{
	TArray<uint64> LiveTransactionIndices;

	{
		TSet<uint64> LiveTransactionIndicesSet;
		for (const auto& LivePackageTransactionPair : LivePackageTransactions)
		{
			LiveTransactionIndicesSet.Append(LivePackageTransactionPair.Value);
		}

		LiveTransactionIndices = LiveTransactionIndicesSet.Array();
		LiveTransactionIndices.Sort();
	}

	return LiveTransactionIndices;
}

TArray<uint64> FConcertTransactionLedger::GetLiveTransactions(const FName InPackageName) const
{
	TArray<uint64> LiveTransactionIndices;

	if (const TArray<uint64>* PackageTransactionIndicesPtr = LivePackageTransactions.Find(InPackageName))
	{
		LiveTransactionIndices = *PackageTransactionIndicesPtr;
	}

	return LiveTransactionIndices;
}

TArray<FName> FConcertTransactionLedger::GetPackagesNamesWithLiveTransactions() const
{
	TArray<FName> PackagesWithLiveTransactions;
	LivePackageTransactions.GenerateKeyArray(PackagesWithLiveTransactions);
	return PackagesWithLiveTransactions;
}

void FConcertTransactionLedger::TrimLiveTransactions(const uint64 InIndex, const FName InPackageName)
{
	if (TArray<uint64>* PackageTransactionIndicesPtr = LivePackageTransactions.Find(InPackageName))
	{
		PackageTransactionIndicesPtr->RemoveAll([InIndex](const uint64 InPackageTransactionIndex)
		{
			return InPackageTransactionIndex < InIndex;
		});

		if (PackageTransactionIndicesPtr->Num() == 0)
		{
			LivePackageTransactions.Remove(InPackageName);
		}

		OnLiveTransactionsTrimmedDelegate.Broadcast(InPackageName, InIndex);
	}
}

void FConcertTransactionLedger::TrackLiveTransaction(const uint64 InIndex, const FConcertTransactionEventBase* InTransactionEvent)
{
	// Track which packages this transaction belongs to
	for (const FName ModifiedPackage : InTransactionEvent->ModifiedPackages)
	{
		TArray<uint64>& PackageTransactionIndices = LivePackageTransactions.FindOrAdd(ModifiedPackage);
		PackageTransactionIndices.Add(InIndex);
	}
}

bool FConcertTransactionLedger::SaveTransaction(const FString& InTransactionFilename, const FStructOnScope& InTransaction) const
{
	TArray<uint8> SerializedTransactionData;
	return ConcertTransactionLedgerUtil::WriteTransaction(InTransaction, SerializedTransactionData) && LedgerFileCache->SaveAndCacheFile(InTransactionFilename, MoveTemp(SerializedTransactionData));
}

bool FConcertTransactionLedger::LoadTransaction(const FString& InTransactionFilename, FStructOnScope& OutTransaction) const
{
	TArray<uint8> SerializedTransactionData;
	if (LedgerFileCache->FindOrCacheFile(InTransactionFilename, SerializedTransactionData) && ConcertTransactionLedgerUtil::ReadTransaction(SerializedTransactionData, OutTransaction))
	{
		if (ensureAlwaysMsgf(OutTransaction.GetStruct()->IsChildOf(FConcertTransactionEventBase::StaticStruct()), TEXT("LoadTransaction can only be used with types deriving from FConcertTransactionEventBase")))
		{
			return true;
		}
	}
	return false;
}

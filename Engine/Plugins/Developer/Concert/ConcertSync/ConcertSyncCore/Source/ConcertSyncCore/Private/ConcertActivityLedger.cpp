// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertActivityLedger.h"

#include "ConcertFileCache.h"
#include "ConcertLogGlobal.h"
#include "ConcertMessageData.h"
#include "ConcertTransactionEvents.h"
#include "HAL/FileManager.h"
#include "IConcertSession.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/PackageName.h"

namespace ConcertActivityLedgerUtil
{
	const int32 MinLedgerFilesToCache = 10;
	const uint64 MaxLedgerFileSizeBytesToCache = 50 * 1024 * 1024;

	const FString LedgerEntryExtension = TEXT("uacti");
	const FGuid LedgerEntryFooter = FGuid(0x6CFF269F, 0xCB53445F, 0xBD5796C2, 0x8FD2C45F);

	FString GetActivityFilename(const FString& InLedgerPath, const uint64 InIndex)
	{
		return InLedgerPath / FString::Printf(TEXT("%s.%s"), *LexToString(InIndex), *LedgerEntryExtension);
	}

	bool WriteActivityData(const FStructOnScope& InActivity, TArray<uint8>& OutSerializedActivityData)
	{
		FMemoryWriter Ar(OutSerializedActivityData);

		const UScriptStruct* ActivityType = CastChecked<const UScriptStruct>(InActivity.GetStruct());

		FString ActivityTypeStr = ActivityType->GetPathName();
		Ar << ActivityTypeStr;
		const_cast<UScriptStruct*>(ActivityType)->SerializeItem(Ar, (void*)InActivity.GetStructMemory(), nullptr);

		return !Ar.IsError();
	}

	bool WriteActivity(const FStructOnScope& InActivity, TArray<uint8>& OutSerializedActivityData)
	{
		check(InActivity.IsValid());

		FMemoryWriter Ar(OutSerializedActivityData);

		// Write the raw activity data
		TArray<uint8> UncompressedActivity;
		if (!WriteActivityData(InActivity, UncompressedActivity))
		{
			return false;
		}

		// Serialize the raw activity
		uint32 UncompressedActivitySize = UncompressedActivity.Num();
		Ar.SerializeIntPacked(UncompressedActivitySize);
		if (UncompressedActivitySize > 0)
		{
			Ar.SerializeCompressed(UncompressedActivity.GetData(), UncompressedActivitySize, NAME_Zlib);
		}

		// Serialize the footer so we know we didn't crash mid-write
		FGuid SerializedFooter = LedgerEntryFooter;
		Ar << SerializedFooter;

		return !Ar.IsError();
	}
	

	bool ReadActivityData(const TArray<uint8>& InSerializedActivityData, FStructOnScope& OutActivity)
	{
		FMemoryReader Ar(InSerializedActivityData);

		// Deserialize the activity
		UScriptStruct* ActivityType = nullptr;
		{
			FString ActivityTypeStr;
			Ar << ActivityTypeStr;
			ActivityType = LoadObject<UScriptStruct>(nullptr, *ActivityTypeStr);
			if (!ActivityType)
			{
				return false;
			}
		}
		if (OutActivity.IsValid())
		{
			// If were given an existing activity to fill with data, then the type must match
			if (ActivityType != OutActivity.GetStruct())
			{
				return false;
			}
		}
		else
		{
			OutActivity.Initialize(ActivityType);
		}
		ActivityType->SerializeItem(Ar, OutActivity.GetStructMemory(), nullptr);

		return !Ar.IsError();
	}

	bool ReadActivity(const TArray<uint8>& InSerializedActivityData, FStructOnScope& OutActivity)
	{
		FMemoryReader Ar(InSerializedActivityData);

		// Test the footer is in place so we know we didn't crash mid-write
		bool bParsedFooter = false;
		{
			const int64 SerializedActivitySize = Ar.TotalSize();
			if (SerializedActivitySize >= sizeof(FGuid))
			{
				FGuid SerializedFooter;
				Ar.Seek(SerializedActivitySize - sizeof(FGuid));
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
		uint32 UncompressedActivitySize = 0;
		Ar.SerializeIntPacked(UncompressedActivitySize);
		TArray<uint8> UncompressedActivity;
		UncompressedActivity.AddZeroed(UncompressedActivitySize);
		if (UncompressedActivitySize > 0)
		{
			Ar.SerializeCompressed(UncompressedActivity.GetData(), UncompressedActivitySize, NAME_Zlib);
		}

		// Read the raw transaction data
		if (!ReadActivityData(UncompressedActivity, OutActivity))
		{
			return false;
		}

		return !Ar.IsError();
	}
	
	bool WasNameEncountered(TSet<FName>& NamesEncounted, const FName& Name)
	{
		bool bWasNameEncountered;
		NamesEncounted.Add(Name, &bWasNameEncountered);
		return bWasNameEncountered;
	}

	void FillTransactionActivity(FConcertTransactionActivityEvent& OutActivity, const FConcertClientInfo& InClientInfo, const FText& TransactionTitle, const uint32 TransactionIndex, const FName ObjectName, const FName PackageName, const FDateTime& InTimeStamp)
	{
		OutActivity.ClientInfo = InClientInfo;
		OutActivity.TimeStamp = InTimeStamp;
		OutActivity.TransactionTitle = TransactionTitle;
		OutActivity.TransactionIndex = TransactionIndex;
		OutActivity.ObjectName = ObjectName;
		OutActivity.PackageName = PackageName;
	}

	void FillPackageUpdatedActivity(FConcertPackageUpdatedActivityEvent& OutActivity, const FConcertClientInfo& InClientInfo, const uint32 Revision, const FName PackageName, const FDateTime& InTimeStamp)
	{
		OutActivity.ClientInfo = InClientInfo;
		OutActivity.TimeStamp = InTimeStamp;
		OutActivity.Revision = Revision;
		OutActivity.PackageName = PackageName;
	}
}

FConcertActivityLedger::FConcertActivityLedger(EConcertActivityLedgerType InLedgerType, const FString& InLedgerPath)
	: LedgerType(InLedgerType)
	, LedgerPath(InLedgerPath / TEXT("Activities"))
	, LedgerFileCache(MakeUnique<FConcertFileCache>(ConcertActivityLedgerUtil::MinLedgerFilesToCache, ConcertActivityLedgerUtil::MaxLedgerFileSizeBytesToCache))
{
	checkf(!LedgerPath.IsEmpty(), TEXT("Ledger Path must not be empty!"));

	if (LedgerType == EConcertActivityLedgerType::Transient)
	{
		ClearLedger();
	}
	else if (LedgerType != EConcertActivityLedgerType::Persistent)
	{
		checkf(false, TEXT("Unknown EConcertActivityLedgerType!"));
	}
}

FConcertActivityLedger::~FConcertActivityLedger()
{
	if (LedgerType == EConcertActivityLedgerType::Transient)
	{
		ClearLedger();
	}
}

bool FConcertActivityLedger::LoadLedger()
{
	auto VisitorFunction = [&ActivityCount = this->ActivityCount](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (!bIsDirectory)
		{
			const FString Filename = FilenameOrDirectory;
			if (FPaths::GetExtension(Filename) == ConcertActivityLedgerUtil::LedgerEntryExtension)
			{
				const FString CurrentActivityIndexStr = FPaths::GetBaseFilename(Filename);
				uint64 CurrentActivityIndex = 0;
				LexFromString(CurrentActivityIndex, *CurrentActivityIndexStr);
				ActivityCount = FMath::Max(ActivityCount, CurrentActivityIndex + 1);
			}
		}
		return true;
	};

	ActivityCount = 0;

	IFileManager::Get().IterateDirectory(*LedgerPath, VisitorFunction);

	return ActivityCount > 0;
}

bool FConcertActivityLedger::FindActivity(const uint64 ActivityIndex, FStructOnScope& OutActivity) const
{
	return LoadActivity(ConcertActivityLedgerUtil::GetActivityFilename(LedgerPath, ActivityIndex), OutActivity);
}

uint64 FConcertActivityLedger::GetLastActivities(const uint32 Limit, TArray<FStructOnScope>& OutActivities) const
{
	check(Limit != 0);
	uint64 Offset = 0;

	if (ActivityCount > Limit)
	{
		Offset = ActivityCount - Limit;
	}

	GetActivities(Offset, Limit, OutActivities);
	return Offset;
}

void FConcertActivityLedger::GetActivities(uint64 Offset, int32 Limit, TArray<FStructOnScope>& OutActivities) const
{
	uint64 EndIndex = FMath::Min(Offset + Limit, ActivityCount);

	// Check for overflow.
	if (Limit != 0)
	{
		checkf(EndIndex >= Offset, TEXT("The PageIndex or PageSize argument was invalid."));
		checkf(EndIndex - Offset == Limit || EndIndex == ActivityCount, TEXT("A buffer overflow occured while computing the end index."));
	}

	OutActivities.Reset(Limit);
	FStructOnScope CopiedActivity;
	for (uint64 Index = Offset; Index < EndIndex; Index++)
	{
		FStructOnScope Activity;
		if (ensureAlwaysMsgf(FindActivity(Index, Activity), TEXT("Could not find activity at index %d."), Index) && Activity.IsValid())
		{
			OutActivities.Emplace(MoveTemp(Activity));
		}
	}
}

void FConcertActivityLedger::ClearLedger()
{
	ActivityCount = 0;
	IFileManager::Get().DeleteDirectory(*LedgerPath, false, true);
}

void FConcertActivityLedger::RecordClientConectionStatusChanged(EConcertClientStatus ClientStatus, const FConcertClientInfo& InClientInfo)
{
	FDateTime TimeStamp = FDateTime::UtcNow();
	if (ClientStatus == EConcertClientStatus::Connected)
	{
		FConcertConnectionActivityEvent ConnectionActivity;
		ConnectionActivity.ClientInfo = InClientInfo;
		ConnectionActivity.TimeStamp = TimeStamp;
		AddActivity(ConnectionActivity);
	}
	else if (ClientStatus == EConcertClientStatus::Disconnected)
	{
		FConcertDisconnectionActivityEvent ConnectionActivity;
		ConnectionActivity.ClientInfo = InClientInfo;
		ConnectionActivity.TimeStamp = TimeStamp;
		AddActivity(ConnectionActivity);
	}
}

void FConcertActivityLedger::RecordFinalizedTransaction(const FConcertTransactionFinalizedEvent& InTransactionFinalizedEvent, uint64 TransactionIndex, const FConcertClientInfo& InClientInfo)
{
	bool bAcceptTransactionActivity = true;
	TSet<FName> EncounteredObjectName;
	FDateTime TimeStamp = FDateTime::UtcNow();

	// We don't want to collect an activity from the persistent level object.
	EncounteredObjectName.Add(FName(TEXT("PersistentLevel")));

	// This loop tries to extract the relevant information from the exported objects.
	for (const FConcertExportedObject& Object : InTransactionFinalizedEvent.ExportedObjects)
	{
		const FName PackageName= FName(*FPackageName::ObjectPathToPackageName(Object.ObjectId.ObjectOuterPathName.ToString()));

		if (Object.ObjectData.bIsPendingKill)
		{
			const FName AffectedObject = FName(*FPackageName::ObjectPathToObjectName(FPackageName::ObjectPathToObjectName(Object.ObjectId.ObjectOuterPathName.ToString())));
			if (AffectedObject.IsNone() 
				|| ConcertActivityLedgerUtil::WasNameEncountered(EncounteredObjectName, AffectedObject))
			{
				// The activity ledger has already a record of the activity for this object.
				continue;
			}

			// This transaction is a delete of an object.
			FConcertTransactionDeleteActivityEvent DeleteObjectActivity;
			ConcertActivityLedgerUtil::FillTransactionActivity(DeleteObjectActivity, InClientInfo, InTransactionFinalizedEvent.Title, TransactionIndex, AffectedObject, PackageName, TimeStamp);
			AddActivity(DeleteObjectActivity);
			// The rest of the transaction is only relevant for the other deleted objects.
			bAcceptTransactionActivity = false;
			continue;
		}

		if (!Object.ObjectData.NewOuterPathName.IsNone())
		{
			// The rest of the transaction is only pertinent for the other renamed objects.
			bAcceptTransactionActivity = false;
			// The activity ledger will record the rename in another exported object (The persistent level).
			continue;
		}

		if (Object.ObjectData.bAllowCreate)
		{
			FName ObjectName = Object.ObjectData.NewName;
			if (ObjectName.IsNone())
			{
				// The function objectPathToObjectName is called two time to be sure that we have the top level object.
				ObjectName = FName(*FPackageName::ObjectPathToObjectName(FPackageName::ObjectPathToObjectName(Object.ObjectId.ObjectOuterPathName.ToString())));
				if (ObjectName.IsNone() || ConcertActivityLedgerUtil::WasNameEncountered(EncounteredObjectName, ObjectName))
				{
					//The activity ledger has already a record of the activity for this object.
					continue;
				}
			}

			// This transaction is the creation of a new object.
			FConcertTransactionCreateActivityEvent CreateObjectActivity;
			ConcertActivityLedgerUtil::FillTransactionActivity(CreateObjectActivity, InClientInfo, InTransactionFinalizedEvent.Title, TransactionIndex, ObjectName, PackageName, TimeStamp);
			AddActivity(CreateObjectActivity);

			// The rest of the transacted objects only matter for the other objects created.
			bAcceptTransactionActivity = false;
			continue;
		}

		if (!Object.ObjectData.NewName.IsNone())
		{
			// This transaction is a rename of a object.
			FConcertTransactionRenameActivityEvent RenameObjectActivity;
			ConcertActivityLedgerUtil::FillTransactionActivity(RenameObjectActivity, InClientInfo, InTransactionFinalizedEvent.Title, TransactionIndex, Object.ObjectId.ObjectName, PackageName, TimeStamp);
			RenameObjectActivity.NewObjectName = Object.ObjectData.NewName;
			AddActivity(RenameObjectActivity);
			// The rest of the transaction is not pertinent for the activity ledger.
			break;
		}

		if (bAcceptTransactionActivity)
		{
			// The function ObjectPathToObjectName is called two time to be sure that we have the top level object.
			const FName ObjectName = FName(*FPackageName::ObjectPathToObjectName(FPackageName::ObjectPathToObjectName(Object.ObjectId.ObjectOuterPathName.ToString())));
			if (ConcertActivityLedgerUtil::WasNameEncountered(EncounteredObjectName, ObjectName))
			{
				// The activity ledger has already a record of the activity for this object.
				continue;
			}

			FConcertTransactionActivityEvent Activity;
			ConcertActivityLedgerUtil::FillTransactionActivity(Activity, InClientInfo, InTransactionFinalizedEvent.Title, TransactionIndex, ObjectName, PackageName, TimeStamp);
			AddActivity(Activity);
		}
		else
		{
			continue;
		}
	}
}

void FConcertActivityLedger::RecordPackageUpdate(const uint32 Revision, const FConcertPackageInfo& InPackageInfo, const FConcertClientInfo& InClientInfo)
{
	FDateTime TimeStamp = FDateTime::UtcNow();
	switch (InPackageInfo.PackageUpdateType)
	{
		case EConcertPackageUpdateType::Saved:
			{
				FConcertPackageUpdatedActivityEvent UpdatedPackageActivity;
				ConcertActivityLedgerUtil::FillPackageUpdatedActivity(UpdatedPackageActivity, InClientInfo, Revision, InPackageInfo.PackageName, TimeStamp);
				AddActivity(UpdatedPackageActivity);
			}
			break;
		case EConcertPackageUpdateType::Added:
			{
				FConcertPackageAddedActivityEvent AddedPackageActivity;
				ConcertActivityLedgerUtil::FillPackageUpdatedActivity(AddedPackageActivity, InClientInfo, Revision, InPackageInfo.PackageName, TimeStamp);
				AddActivity(AddedPackageActivity);
			}
			break;
		case EConcertPackageUpdateType::Deleted:
			{
				FConcertPackageDeletedActivityEvent DeletedPackageActivity;
				ConcertActivityLedgerUtil::FillPackageUpdatedActivity(DeletedPackageActivity, InClientInfo, Revision, InPackageInfo.PackageName, TimeStamp);
				AddActivity(DeletedPackageActivity);
			}
			break;
		case EConcertPackageUpdateType::Renamed:
			{
				FConcertPackageRenamedActivityEvent RenamePackageActivity;
				ConcertActivityLedgerUtil::FillPackageUpdatedActivity(RenamePackageActivity, InClientInfo, Revision, InPackageInfo.PackageName, TimeStamp);
				RenamePackageActivity.NewPackageName = InPackageInfo.NewPackageName;
				AddActivity(RenamePackageActivity);
			}
			break;
		default:
			// Do nothing, those update aren't important for the Activity Ledger.
			break;
	}
}

bool FConcertActivityLedger::AddActivity(const UScriptStruct* InActivityType, const void* InActivityData)
{
	checkf(InActivityType->IsChildOf(FConcertActivityEvent::StaticStruct()), TEXT("AddActivity can only be used with types deriving from FConcertActivityEvent"));

	const FString FilePath = ConcertActivityLedgerUtil::GetActivityFilename(LedgerPath, ActivityCount);
	uint64 ActivityIndex = ActivityCount++;

	FStructOnScope Activity(InActivityType, (uint8*)InActivityData);

	TArray<uint8> SerializedActivityData;
	ConcertActivityLedgerUtil::WriteActivity(Activity, SerializedActivityData);

	OnAddActivityDelegate.Broadcast(Activity, ActivityIndex);

	return LedgerFileCache->SaveAndCacheFile(FilePath, MoveTemp(SerializedActivityData));
}

bool FConcertActivityLedger::LoadActivity(const FString& InActivityFilename, FStructOnScope& OutActivity) const
{
	TArray<uint8> SerializedTransactionData;
	if (LedgerFileCache->FindOrCacheFile(InActivityFilename, SerializedTransactionData) && ConcertActivityLedgerUtil::ReadActivity(SerializedTransactionData, OutActivity))
	{
		return ensureAlwaysMsgf(OutActivity.GetStruct()->IsChildOf(FConcertActivityEvent::StaticStruct()), TEXT("LoadActivity can only be used with types deriving from FConcertActivityEvent"));
	}

	return false;
}

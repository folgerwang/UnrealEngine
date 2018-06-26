// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineHotfixManager.h"
#include "OnlineSubsystemUtils.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Internationalization/Culture.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Http.h"

#include "Logging/LogSuppressionInterface.h"

#include "Misc/NetworkVersion.h"

#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"

#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Curves/CurveFloat.h"

DEFINE_LOG_CATEGORY(LogHotfixManager);

/** This character must be between important pieces of file information (platform, initype, version) */
#define HOTFIX_SEPARATOR TEXT("_")
/** The prefix for any hotfix file that expects to indicate version information */
#define HOTFIX_VERSION_TAG TEXT("Ver-")

FName NAME_HotfixManager(TEXT("HotfixManager"));

class FPakFileVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			Files.Add(FilenameOrDirectory);
		}
		return true;
	}

	TArray<FString> Files;
};

namespace
{
	/** @return the expected network version for hotfix files determined at compile time */
	FString GetNetworkVersion()
	{
		static FString NetVerStr;
		if (NetVerStr.IsEmpty())
		{
			uint32 NetVer = FNetworkVersion::GetNetworkCompatibleChangelist();
			NetVerStr = FString::Printf(TEXT("%s%d"), HOTFIX_VERSION_TAG, NetVer);
		}
		return NetVerStr;
	}

	/**
	 * Given a hotfix file name, return the file name with version stripped out and exposed separately
	 *
	 * @param InFilename name of file to search for version information
	 * @param OutFilename name with version information removed
	 * @param OutVersion version of the hotfix file it present in the name
	 */
	void GetFilenameAndVersion(const FString& InFilename, FString& OutFilename, FString& OutVersion)
	{
		TArray<FString> FileParts;
		int32 NumTokens = InFilename.ParseIntoArray(FileParts, HOTFIX_SEPARATOR);
		if (NumTokens > 0)
		{
			for (int i = 0; i < FileParts.Num(); i++)
			{
				if (FileParts[i].StartsWith(HOTFIX_VERSION_TAG))
				{
					OutVersion = FileParts[i];
				}
				else
				{
					OutFilename += FileParts[i];
					if (i < FileParts.Num() - 1)
					{
						OutFilename += HOTFIX_SEPARATOR;
					}
				}
			}
		}
	}

	/**
	 * Is this hotfix file compatible with the current build
	 * If the file has version information it is compared with compatibility
	 * If the file has NO version information it is assumed compatible
	 *
	 * @param InFilename name of the file to check 
	 * @param OutFilename name of file with version information stripped
	 * 
	 * @return true if file is compatible, false otherwise
	 */
	bool IsCompatibleHotfixFile(const FString& InFilename, FString& OutFilename)
	{
		bool bHasVersion = false;
		bool bCompatibleHotfix = false;
		const FString NetworkVersion = GetNetworkVersion();
		FString OutVersion;
		GetFilenameAndVersion(InFilename, OutFilename, OutVersion);

		if (!OutVersion.IsEmpty())
		{
			bHasVersion = true;
			if (OutVersion == NetworkVersion)
			{
				bCompatibleHotfix = true;
			}
		}

		return bCompatibleHotfix || !bHasVersion;
	}
}

UOnlineHotfixManager::UOnlineHotfixManager() :
	Super(),
	TotalFiles(0),
	NumDownloaded(0),
	TotalBytes(0),
	NumBytes(0),
	bHotfixingInProgress(false),
	bHotfixNeedsMapReload(false),
	ChangedOrRemovedPakCount(0)
{
	OnEnumerateFilesCompleteDelegate = FOnEnumerateFilesCompleteDelegate::CreateUObject(this, &UOnlineHotfixManager::OnEnumerateFilesComplete);
	OnReadFileProgressDelegate = FOnReadFileProgressDelegate::CreateUObject(this, &UOnlineHotfixManager::OnReadFileProgress);
	OnReadFileCompleteDelegate = FOnReadFileCompleteDelegate::CreateUObject(this, &UOnlineHotfixManager::OnReadFileComplete);
#if !UE_BUILD_SHIPPING
	bLogMountedPakContents = FParse::Param(FCommandLine::Get(), TEXT("LogHotfixPakContents"));
#endif
	GameContentPath = FString() / FApp::GetProjectName() / TEXT("Content");
}

UOnlineHotfixManager* UOnlineHotfixManager::Get(UWorld* World)
{
	UOnlineHotfixManager* DefaultObject = UOnlineHotfixManager::StaticClass()->GetDefaultObject<UOnlineHotfixManager>();
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World, DefaultObject->OSSName.Len() > 0 ? FName(*DefaultObject->OSSName) : NAME_None);
	if (OnlineSub != nullptr)
	{
		UOnlineHotfixManager* HotfixManager = Cast<UOnlineHotfixManager>(OnlineSub->GetNamedInterface(NAME_HotfixManager));
		if (HotfixManager == nullptr)
		{
			FString HotfixManagerClassName = DefaultObject->HotfixManagerClassName;
			UClass* HotfixManagerClass = LoadClass<UOnlineHotfixManager>(nullptr, *HotfixManagerClassName, nullptr, LOAD_None, nullptr);
			if (HotfixManagerClass == nullptr)
			{
				// Just use the default class if it couldn't load what was specified
				HotfixManagerClass = UOnlineHotfixManager::StaticClass();
			}
			// Create it and store it
			HotfixManager = NewObject<UOnlineHotfixManager>(GetTransientPackage(), HotfixManagerClass);
			OnlineSub->SetNamedInterface(NAME_HotfixManager, HotfixManager);
		}
		return HotfixManager;
	}
	return nullptr;
}

void UOnlineHotfixManager::PostInitProperties()
{
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("HOTFIXPREFIX="), DebugPrefix);
	if (!DebugPrefix.IsEmpty())
	{
		DebugPrefix += HOTFIX_SEPARATOR;
	}
#endif
	// So we only try to apply files for this platform
	PlatformPrefix = DebugPrefix + ANSI_TO_TCHAR(FPlatformProperties::PlatformName());
	PlatformPrefix += HOTFIX_SEPARATOR;
	// Server prefix
	ServerPrefix = DebugPrefix + TEXT("DedicatedServer");
	// Build the default prefix too
	DefaultPrefix = DebugPrefix + TEXT("Default");

	Super::PostInitProperties();
}

void UOnlineHotfixManager::Init()
{
	bHotfixingInProgress = true;
	bHotfixNeedsMapReload = false;
	TotalFiles = 0;
	NumDownloaded = 0;
	TotalBytes = 0;
	NumBytes = 0;
	ChangedOrRemovedPakCount = 0;
	// Build the name of the loc file that we'll care about
	// It can change at runtime so build it just before fetching the data
	GameLocName = DebugPrefix + FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() + TEXT("_Game.locres");
	OnlineTitleFile = Online::GetTitleFileInterface(OSSName.Len() ? FName(*OSSName, FNAME_Find) : NAME_None);
	if (OnlineTitleFile.IsValid())
	{
		OnEnumerateFilesCompleteDelegateHandle = OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesCompleteDelegate);
		OnReadFileProgressDelegateHandle = OnlineTitleFile->AddOnReadFileProgressDelegate_Handle(OnReadFileProgressDelegate);
		OnReadFileCompleteDelegateHandle = OnlineTitleFile->AddOnReadFileCompleteDelegate_Handle(OnReadFileCompleteDelegate);
	}
}

void UOnlineHotfixManager::Cleanup()
{
	PendingHotfixFiles.Empty();
	if (OnlineTitleFile.IsValid())
	{
		// Make sure to give back the memory used when reading the hotfix files
		OnlineTitleFile->ClearFiles();
		OnlineTitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesCompleteDelegateHandle);
		OnlineTitleFile->ClearOnReadFileProgressDelegate_Handle(OnReadFileProgressDelegateHandle);
		OnlineTitleFile->ClearOnReadFileCompleteDelegate_Handle(OnReadFileCompleteDelegateHandle);
	}
	OnlineTitleFile = nullptr;
	bHotfixingInProgress = false;
}

void UOnlineHotfixManager::StartHotfixProcess()
{
	// Patching the editor this way seems like a bad idea
	bool bShouldHotfix = IsRunningGame() || IsRunningDedicatedServer() || IsRunningClientOnly();
	if (!bShouldHotfix)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing skipped when not running game/server"));
		TriggerHotfixComplete(EHotfixResult::SuccessNoChange);
		return;
	}

	if (bHotfixingInProgress)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing already in progress"));
		return;
	}

	Init();
	// Kick off an enumeration of the files that are available to download
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->EnumerateFiles();
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Failed to start the hotfixing process due to no OnlineTitleInterface present for OSS(%s)"), *OSSName);
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

struct FHotfixFileSortPredicate
{
	struct FHotfixFileNameSortPredicate
	{
		const FString PlatformPrefix;
		const FString ServerPrefix;
		const FString DefaultPrefix;

		FHotfixFileNameSortPredicate(const FString& InPlatformPrefix, const FString& InServerPrefix, const FString& InDefaultPrefix) :
			PlatformPrefix(InPlatformPrefix),
			ServerPrefix(InServerPrefix),
			DefaultPrefix(InDefaultPrefix)
		{
		}

		int32 GetPriorityForCompare(const FString& InHotfixName) const
		{
			// Non-ini files are applied last
			int32 Priority = 50;
			
			if (InHotfixName.EndsWith(TEXT("INI")))
			{
				FString HotfixName, NetworkVersion;
				GetFilenameAndVersion(InHotfixName, HotfixName, NetworkVersion);

				// Defaults are applied first
				if (HotfixName.StartsWith(DefaultPrefix))
				{
					Priority = 10;
				}
				// Server trumps default
				else if (HotfixName.StartsWith(ServerPrefix))
				{
					Priority = 20;
				}
				// Platform trumps server
				else if (HotfixName.StartsWith(PlatformPrefix))
				{
					Priority = 30;
				}
				// Other INIs whitelisted in game override of WantsHotfixProcessing will trump all other INIs
				else
				{
					Priority = 40;
				}

				if (!NetworkVersion.IsEmpty())
				{
					// Versioned hotfixes apply last within their type
					Priority += 5;
				}
			}

			return Priority;
		}

		bool Compare(const FString& A, const FString& B) const
		{
			int32 APriority = GetPriorityForCompare(A);
			int32 BPriority = GetPriorityForCompare(B);
			if (APriority != BPriority)
			{
				return APriority < BPriority;
			}
			else
			{
				// Fall back to sort by the string order if both have same priority
				return A < B;
			}
		}
	};
	FHotfixFileNameSortPredicate FileNameSorter;

	FHotfixFileSortPredicate(const FString& InPlatformPrefix, const FString& InServerPrefix, const FString& InDefaultPrefix) :
		FileNameSorter(InPlatformPrefix, InServerPrefix, InDefaultPrefix)
	{
	}

	bool operator()(const FCloudFileHeader &A, const FCloudFileHeader &B) const
	{
		return FileNameSorter.Compare(A.FileName, B.FileName);
	}

	bool operator()(const FString& A, const FString& B) const
	{
		return FileNameSorter.Compare(FPaths::GetCleanFilename(A), FPaths::GetCleanFilename(B));
	}
};

void UOnlineHotfixManager::OnEnumerateFilesComplete(bool bWasSuccessful, const FString& ErrorStr)
{
	if (bWasSuccessful)
	{
		check(OnlineTitleFile.IsValid());
		// Cache our current set so we can compare for differences
		LastHotfixFileList = HotfixFileList;
		HotfixFileList.Empty();
		// Get the new header data
		OnlineTitleFile->GetFileList(HotfixFileList);
		FilterHotfixFiles();
		// Reduce the set of work to just the files that changed since last run
		BuildHotfixFileListDeltas();
		// Sort after filtering so that the comparison below doesn't fail to different order from the server
		ChangedHotfixFileList.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(PlatformPrefix, ServerPrefix, DefaultPrefix));
		// Read any changed files
		if (ChangedHotfixFileList.Num() > 0)
		{
			// Update our totals for our progress delegates
			TotalFiles = ChangedHotfixFileList.Num();
			for (const FCloudFileHeader& FileHeader : ChangedHotfixFileList)
			{
				TotalBytes += FileHeader.FileSize;
			}
			ReadHotfixFiles();
		}
		else
		{
			if (RemovedHotfixFileList.Num() > 0)
			{
				// No changes, just reverts
				// Perform any undo operations needed
				RestoreBackupIniFiles();
				UnmountHotfixFiles();
			}

			UE_LOG(LogHotfixManager, Display, TEXT("Returned hotfix data is the same as last application, skipping the apply phase"));
			TriggerHotfixComplete(EHotfixResult::SuccessNoChange);
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Enumeration of hotfix files failed"));
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

void UOnlineHotfixManager::CheckAvailability(FOnHotfixAvailableComplete& InCompletionDelegate)
{
	// Checking for hotfixes in editor is not supported
	bool bShouldHotfix = IsRunningGame() || IsRunningDedicatedServer() || IsRunningClientOnly();
	if (!bShouldHotfix)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing availability skipped when not running game/server"));
		InCompletionDelegate.ExecuteIfBound(EHotfixResult::SuccessNoChange);
		return;
	}

	if (bHotfixingInProgress)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing availability skipped because hotfix in progress"));
		InCompletionDelegate.ExecuteIfBound(EHotfixResult::Failed);
		return;
	}

	OnlineTitleFile = Online::GetTitleFileInterface(OSSName.Len() ? FName(*OSSName, FNAME_Find) : NAME_None);

	FOnEnumerateFilesCompleteDelegate OnEnumerateFilesForAvailabilityCompleteDelegate;
	OnEnumerateFilesForAvailabilityCompleteDelegate.BindUObject(this, &UOnlineHotfixManager::OnEnumerateFilesForAvailabilityComplete, InCompletionDelegate);
	OnEnumerateFilesForAvailabilityCompleteDelegateHandle = OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesForAvailabilityCompleteDelegate);

	bHotfixingInProgress = true;

	// Kick off an enumeration of the files that are available to download
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->EnumerateFiles();
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Failed to start the hotfix check process due to no OnlineTitleInterface present for OSS(%s)"), *OSSName);
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

void UOnlineHotfixManager::OnEnumerateFilesForAvailabilityComplete(bool bWasSuccessful, const FString& ErrorStr, FOnHotfixAvailableComplete InCompletionDelegate)
{
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesForAvailabilityCompleteDelegateHandle);
	}
	
	EHotfixResult Result = EHotfixResult::Failed;
	if (bWasSuccessful)
	{
		TArray<FCloudFileHeader> TmpHotfixFileList;
		TArray<FCloudFileHeader> TmpLastHotfixFileList;

		TmpHotfixFileList = HotfixFileList;
		TmpLastHotfixFileList = LastHotfixFileList;

		// Cache our current set so we can compare for differences
		LastHotfixFileList = HotfixFileList;
		HotfixFileList.Empty();
		// Get the new header data
		OnlineTitleFile->GetFileList(HotfixFileList);
		FilterHotfixFiles();
		// Reduce the set of work to just the files that changed since last run
		BuildHotfixFileListDeltas();

		// Read any changed files
		if (ChangedHotfixFileList.Num() > 0 || RemovedHotfixFileList.Num() > 0)
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Hotfix files available"));
			Result = EHotfixResult::Success;
		}
		else
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Returned hotfix data is the same as last application, returning nothing to do"));
			Result = EHotfixResult::SuccessNoChange;
		}

		// Restore state to before the check
		RemovedHotfixFileList.Empty();
		ChangedHotfixFileList.Empty();
		HotfixFileList = TmpHotfixFileList;
		LastHotfixFileList = TmpLastHotfixFileList;
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Enumeration of hotfix files failed"));
	}

	OnlineTitleFile = nullptr;
	bHotfixingInProgress = false;
	InCompletionDelegate.ExecuteIfBound(Result);
}

void UOnlineHotfixManager::BuildHotfixFileListDeltas()
{
	RemovedHotfixFileList.Empty();
	ChangedHotfixFileList.Empty();
	// Go through the current list and see if it's changed from the previous attempt
	TSet<FString> DirtyIniCategories;
	for (const FCloudFileHeader& CurrentHeader : HotfixFileList)
	{
		bool bFoundMatch = LastHotfixFileList.Contains(CurrentHeader);
		if (!bFoundMatch)
		{
			// All NEW or CHANGED ini files will be added to the process list
			ChangedHotfixFileList.Add(CurrentHeader);

			if (CurrentHeader.FileName.EndsWith(TEXT(".INI"), ESearchCase::IgnoreCase))
			{
				// Make sure that ALL INIs of this "category" get marked for inclusion below
				DirtyIniCategories.Add(GetStrippedConfigFileName(CurrentHeader.FileName));
			}
		}
	}
	// Find any files that have been removed from the set of hotfix files
	for (const FCloudFileHeader& LastHeader : LastHotfixFileList)
	{
		bool bFoundMatch = HotfixFileList.ContainsByPredicate(
			[&LastHeader](const FCloudFileHeader& CurrentHeader)
			{
				return LastHeader.FileName == CurrentHeader.FileName;
			});
		if (!bFoundMatch)
		{
			// We've been removed so add to the removed list
			RemovedHotfixFileList.Add(LastHeader);

			if (LastHeader.FileName.EndsWith(TEXT(".INI"), ESearchCase::IgnoreCase))
			{
				// Make sure that ALL INIs of this "category" get marked for inclusion below
				DirtyIniCategories.Add(GetStrippedConfigFileName(LastHeader.FileName));
			}
		}
	}

	// Apply all hotfix files for each ini file if the category has been marked dirty
	// For example, if DefaultGame.ini has changed, also consider XboxOne_Game.ini changed
	// This is necessary because we revert the ini file to the pre-hotfix state
	if (DirtyIniCategories.Num() > 0)
	{
		for (const FCloudFileHeader& CurrentHeader : HotfixFileList)
		{
			if (CurrentHeader.FileName.EndsWith(TEXT(".INI"), ESearchCase::IgnoreCase))
			{
				for (const FString& StrippedIniName : DirtyIniCategories)
				{
					if (CurrentHeader.FileName.EndsWith(StrippedIniName, ESearchCase::IgnoreCase))
					{
						// Be sure to include any ini in a "dirty" category that remains in the latest HotfixFileList
						ChangedHotfixFileList.AddUnique(CurrentHeader);
					}
				}
			}
		}
	}
}

void UOnlineHotfixManager::FilterHotfixFiles()
{
	for (int32 Idx = 0; Idx < HotfixFileList.Num(); Idx++)
	{
		if (!WantsHotfixProcessing(HotfixFileList[Idx]))
		{
			HotfixFileList.RemoveAt(Idx, 1, false);
			Idx--;
		}
	}
}

void UOnlineHotfixManager::ReadHotfixFiles()
{
	if (ChangedHotfixFileList.Num())
	{
		check(OnlineTitleFile.IsValid());
		// Kick off a read for each file
		// Do this in two passes so already cached files don't trigger completion
		for (const FCloudFileHeader& FileHeader : ChangedHotfixFileList)
		{
			UE_LOG(LogHotfixManager, VeryVerbose, TEXT("HF: %s %s %d "), *FileHeader.DLName, *FileHeader.FileName, FileHeader.FileSize);
			PendingHotfixFiles.Add(FileHeader.DLName, FPendingFileDLProgress());
		}
		for (const FCloudFileHeader& FileHeader : ChangedHotfixFileList)
		{
			OnlineTitleFile->ReadFile(FileHeader.DLName);
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Display, TEXT("No hotfix files need to be downloaded"));
		TriggerHotfixComplete(EHotfixResult::Success);
	}
}

void UOnlineHotfixManager::OnReadFileComplete(bool bWasSuccessful, const FString& FileName)
{
	if (PendingHotfixFiles.Contains(FileName))
	{
		if (bWasSuccessful)
		{
			FCloudFileHeader* Header = GetFileHeaderFromDLName(FileName);
			check(Header != nullptr);
			UE_LOG(LogHotfixManager, Log, TEXT("Hotfix file (%s) downloaded. Size was (%d)"), *GetFriendlyNameFromDLName(FileName), Header->FileSize);
			// Completion updates the file count and progress updates the byte count
			UpdateProgress(1, 0);
			PendingHotfixFiles.Remove(FileName);
			if (PendingHotfixFiles.Num() == 0)
			{
				// All files have been downloaded so now apply the files
				ApplyHotfix();
			}
		}
		else
		{
			UE_LOG(LogHotfixManager, Error, TEXT("Hotfix file (%s) failed to download"), *GetFriendlyNameFromDLName(FileName));
			TriggerHotfixComplete(EHotfixResult::Failed);
		}
	}
}

void UOnlineHotfixManager::UpdateProgress(uint32 FileCount, uint64 UpdateSize)
{
	NumDownloaded += FileCount;
	NumBytes += UpdateSize;
	// Update our progress
	TriggerOnHotfixProgressDelegates(NumDownloaded, TotalFiles, NumBytes, TotalBytes);
}

void UOnlineHotfixManager::ApplyHotfix()
{
	// Perform any undo operations needed
	// This occurs same frame as the application of new hotfixes
	RestoreBackupIniFiles();
	UnmountHotfixFiles();

	for (const FCloudFileHeader& FileHeader : ChangedHotfixFileList)
	{
		if (!ApplyHotfixProcessing(FileHeader))
		{
			UE_LOG(LogHotfixManager, Error, TEXT("Couldn't apply hotfix file (%s)"), *FileHeader.FileName);
			TriggerHotfixComplete(EHotfixResult::Failed);
			return;
		}
		// Let anyone listening know we just processed this file
		TriggerOnHotfixProcessedFileDelegates(FileHeader.FileName, GetCachedDirectory() / FileHeader.DLName);
	}
	UE_LOG(LogHotfixManager, Display, TEXT("Hotfix data has been successfully applied"));
	EHotfixResult Result = EHotfixResult::Success;
	if (ChangedOrRemovedPakCount > 0)
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Hotfix has changed or removed PAK files so a relaunch of the app is needed"));
		Result = EHotfixResult::SuccessNeedsRelaunch;
	}
	else if (bHotfixNeedsMapReload)
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Hotfix has detected PAK files containing currently loaded maps, so a level load is needed"));
		Result = EHotfixResult::SuccessNeedsReload;
	}
	TriggerHotfixComplete(Result);
}

void UOnlineHotfixManager::TriggerHotfixComplete(EHotfixResult HotfixResult)
{
	if( HotfixResult != EHotfixResult::Failed )
	{
		PatchAssetsFromIniFiles();
	}

	TriggerOnHotfixCompleteDelegates(HotfixResult);
	if (HotfixResult == EHotfixResult::Failed)
	{
		HotfixFileList.Empty();
		UnmountHotfixFiles();
	}
	Cleanup();
}

bool UOnlineHotfixManager::WantsHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("INI"))
	{
		FString CloudFilename;
		if (IsCompatibleHotfixFile(FileHeader.FileName, CloudFilename))
		{
			bool bIsServerHotfix = CloudFilename.StartsWith(ServerPrefix);
			bool bWantsServerHotfix = IsRunningDedicatedServer() && bIsServerHotfix;
			bool bWantsDefaultHotfix = CloudFilename.StartsWith(DefaultPrefix);
			bool bWantsPlatformHotfix = CloudFilename.StartsWith(PlatformPrefix);

			if (bWantsPlatformHotfix)
			{
				UE_LOG(LogHotfixManager, Verbose, TEXT("Using platform hotfix %s"), *FileHeader.FileName);
			}
			else if (bWantsServerHotfix)
			{
				UE_LOG(LogHotfixManager, Verbose, TEXT("Using server hotfix %s"), *FileHeader.FileName);
			}
			else if (bWantsDefaultHotfix)
			{
				UE_LOG(LogHotfixManager, Verbose, TEXT("Using default hotfix %s"), *FileHeader.FileName);
			}

			return bWantsPlatformHotfix || bWantsServerHotfix || bWantsDefaultHotfix;
		}
		else
		{
			UE_LOG(LogHotfixManager, Verbose, TEXT("File not compatible %s, skipping."), *FileHeader.FileName);
			return false;
		}
	}
	else if (Extension == TEXT("PAK"))
	{
		return FileHeader.FileName.Find(PlatformPrefix) != -1;
	}
	return FileHeader.FileName == GameLocName;
}

bool UOnlineHotfixManager::ApplyHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	bool bSuccess = false;
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("INI"))
	{
		TArray<uint8> FileData;
		if (OnlineTitleFile->GetFileContents(FileHeader.DLName, FileData))
		{
			UE_LOG(LogHotfixManager, Verbose, TEXT("Applying hotfix %s"), *FileHeader.FileName);
			// Convert to a FString
			FileData.Add(0);
			FString HotfixStr;
			FFileHelper::BufferToString(HotfixStr, FileData.GetData(), FileData.Num());
			bSuccess = HotfixIniFile(FileHeader.FileName, HotfixStr);
		}
		else
		{
			UE_LOG(LogHotfixManager, Warning, TEXT("Failed to get contents of %s"), *FileHeader.FileName);
		}
	}
	else if (Extension == TEXT("LOCRES"))
	{
		HotfixLocFile(FileHeader);
		// Currently no failure case for this
		bSuccess = true;
	}
	else if (Extension == TEXT("PAK"))
	{
		bSuccess = HotfixPakFile(FileHeader);
	}
	OnlineTitleFile->ClearFile(FileHeader.FileName);
	return bSuccess;
}

FString UOnlineHotfixManager::GetStrippedConfigFileName(const FString& IniName)
{
	FString StrippedIniName;
	FString NetworkVersion;
	GetFilenameAndVersion(IniName, StrippedIniName, NetworkVersion);

	if (StrippedIniName.StartsWith(PlatformPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - PlatformPrefix.Len());
	}
	else if (StrippedIniName.StartsWith(ServerPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - ServerPrefix.Len());
	}
	else if (StrippedIniName.StartsWith(DefaultPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - DefaultPrefix.Len());
	}
	else if (StrippedIniName.StartsWith(DebugPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - DebugPrefix.Len());
	}
	return StrippedIniName;
}

FString UOnlineHotfixManager::GetConfigFileNamePath(const FString& IniName)
{
	return FPaths::GeneratedConfigDir() + ANSI_TO_TCHAR(FPlatformProperties::PlatformName()) / IniName;
}

FConfigFile* UOnlineHotfixManager::GetConfigFile(const FString& IniName)
{
	FString StrippedIniName(GetStrippedConfigFileName(IniName));
	FConfigFile* ConfigFile = nullptr;
	// Look for the first matching INI file entry
	for (TMap<FString, FConfigFile>::TIterator It(*GConfig); It; ++It)
	{
		if (It.Key().EndsWith(StrippedIniName))
		{
			ConfigFile = &It.Value();
			break;
		}
	}
	// If not found, add this file to the config cache
	if (ConfigFile == nullptr)
	{
		const FString IniNameWithPath = GetConfigFileNamePath(StrippedIniName);
		FConfigFile Empty;
		GConfig->SetFile(IniNameWithPath, &Empty);
		ConfigFile = GConfig->Find(IniNameWithPath, false);
	}
	check(ConfigFile);
	// We never want to save these merged files
	ConfigFile->NoSave = true;
	return ConfigFile;
}

bool UOnlineHotfixManager::HotfixIniFile(const FString& FileName, const FString& IniData)
{
	const bool bIsEngineIni = FileName.Contains(TEXT("Engine.ini"));

	FConfigFile* ConfigFile = GetConfigFile(FileName);
	// Store the original file so we can undo this later
	FConfigFileBackup& BackupFile = BackupIniFile(FileName, ConfigFile);
	// Merge the string into the config file
	ConfigFile->CombineFromBuffer(IniData);
	TArray<UClass*> Classes;
	TArray<UObject*> PerObjectConfigObjects;
	int32 StartIndex = 0;
	int32 EndIndex = 0;
	bool bUpdateLogSuppression = false;
	bool bUpdateConsoleVariables = false;
	bool bUpdateHttpConfigs = false;
	TSet<FString> OnlineSubSections;
	// Find the set of object classes that were affected
	while (StartIndex >= 0 && StartIndex < IniData.Len() && EndIndex >= StartIndex)
	{
		// Find the next section header
		StartIndex = IniData.Find(TEXT("["), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
		if (StartIndex > -1)
		{
			// Find the ending section identifier
			EndIndex = IniData.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
			if (EndIndex > StartIndex)
			{
				// Ignore square brackets in the middle of string
				// - per object section starts with new line
				// - there's no " character between opening bracket and line start
				const bool bStartsWithNewLine = (StartIndex == 0) || (IniData[StartIndex - 1] == TEXT('\n'));
				if (!bStartsWithNewLine)
				{
					bool bStartsInsideString = false;
					for (int32 CharIdx = StartIndex - 1; CharIdx >= 0; CharIdx--)
					{
						const bool bHasStringMarker = (IniData[CharIdx] == TEXT('"'));
						if (bHasStringMarker)
						{
							bStartsInsideString = true;
							break;
						}

						const bool bHasNewLineMarker = (IniData[CharIdx] == TEXT('\n'));
						if (bHasNewLineMarker)
						{
							break;
						}
					}

					if (bStartsInsideString)
					{
						StartIndex = EndIndex;
						continue;
					}
				}

				int32 PerObjectNameIndex = IniData.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);

				const TCHAR* AssetHotfixIniHACK = TEXT("[AssetHotfix]");
				if (FCString::Strnicmp(*IniData + StartIndex, AssetHotfixIniHACK, FCString::Strlen(AssetHotfixIniHACK)) == 0)
				{
					// HACK - Make AssetHotfix the last element in the ini file so that this parsing isn't affected by it for now
					break;
				}

				if (bIsEngineIni)
				{
					const TCHAR* LogConfigSection = TEXT("[Core.Log]");
					const TCHAR* ConsoleVariableSection = TEXT("[ConsoleVariables]");
					const TCHAR* HttpSection = TEXT("[HTTP]");
					const TCHAR* OnlineSubSectionKey = TEXT("[OnlineSubsystem"); // note "]" omitted on purpose since we want a partial match
					if (!bUpdateLogSuppression && FCString::Strnicmp(*IniData + StartIndex, LogConfigSection, FCString::Strlen(LogConfigSection)) == 0)
					{
						bUpdateLogSuppression = true;
					}
					else if (!bUpdateConsoleVariables && FCString::Strnicmp(*IniData + StartIndex, ConsoleVariableSection, FCString::Strlen(ConsoleVariableSection)) == 0)
					{
						bUpdateConsoleVariables = true;
					}
					else if (!bUpdateHttpConfigs &&	FCString::Strnicmp(*IniData + StartIndex, HttpSection, FCString::Strlen(HttpSection)) == 0)
					{
						bUpdateHttpConfigs = true;
					}
					else if (FCString::Strnicmp(*IniData + StartIndex, OnlineSubSectionKey, FCString::Strlen(OnlineSubSectionKey)) == 0)
					{
						const FString SectionStr = IniData.Mid(StartIndex, EndIndex - StartIndex + 1);
						OnlineSubSections.Add(SectionStr);
					}
				}

				// Per object config entries will have a space in the name, but classes won't
				if (PerObjectNameIndex == -1 || PerObjectNameIndex > EndIndex)
				{
					const TCHAR* ScriptHeader = TEXT("[/Script/");
					if (FCString::Strnicmp(*IniData + StartIndex, ScriptHeader, FCString::Strlen(ScriptHeader)) == 0)
					{
						const int32 ScriptSectionTag = 9;
						// Snip the text out and try to find the class for that
						const FString PackageClassName = IniData.Mid(StartIndex + ScriptSectionTag, EndIndex - StartIndex - ScriptSectionTag);
						// Find the class for this so we know what to update
						UClass* Class = FindObject<UClass>(nullptr, *PackageClassName, true);
						if (Class)
						{
							// Add this to the list to check against
							Classes.Add(Class);
							BackupFile.ClassesReloaded.AddUnique(Class->GetPathName());
						}
					}
				}
				// Handle the per object config case by finding the object for reload
				else
				{
					const int32 ClassNameStart = PerObjectNameIndex + 1;
					const FString ClassName = IniData.Mid(ClassNameStart, EndIndex - ClassNameStart);

					// Look up the class to search for
					UClass* ObjectClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);

					if (ObjectClass)
					{
						const int32 Count = PerObjectNameIndex - StartIndex - 1;
						const FString PerObjectName = IniData.Mid(StartIndex + 1, Count);

						// Explicitly search the transient package (won't update non-transient objects)
						UObject* PerObject = StaticFindObject(ObjectClass, ANY_PACKAGE, *PerObjectName, false);
						if (PerObject != nullptr)
						{
							PerObjectConfigObjects.Add(PerObject);
							BackupFile.ClassesReloaded.AddUnique(ObjectClass->GetPathName());
						}
					}
					else
					{
						UE_LOG(LogHotfixManager, Warning, TEXT("Specified per-object class %s was not found"), *ClassName);
					}
				}
				StartIndex = EndIndex;
			}
		}
	}

	int32 NumObjectsReloaded = 0;
	const double StartTime = FPlatformTime::Seconds();
	// Now that we have a list of classes to update, we can iterate objects and reload
	for (UClass* Class : Classes)
	{
		if (Class->HasAnyClassFlags(CLASS_Config))
		{
			TArray<UObject*> Objects;
			GetObjectsOfClass(Class, Objects, true, RF_NoFlags);
			for (UObject* Object : Objects)
			{
				if (!Object->IsPendingKill())
				{
					// Force a reload of the config vars
					UE_LOG(LogHotfixManager, Verbose, TEXT("Reloading %s"), *Object->GetPathName());
					Object->ReloadConfig();
					NumObjectsReloaded++;
				}
			}
		}
	}

	// Reload any PerObjectConfig objects that were affected
	for (auto ReloadObject : PerObjectConfigObjects)
	{
		UE_LOG(LogHotfixManager, Verbose, TEXT("Reloading %s"), *ReloadObject->GetPathName());
		ReloadObject->ReloadConfig();
		NumObjectsReloaded++;
	}

	// Reload log suppression if configs changed
	if (bUpdateLogSuppression)
	{
		FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();
	}

	// Reload console variables if configs changed
	if (bUpdateConsoleVariables)
	{
		FConfigCacheIni::LoadConsoleVariablesFromINI();
	}

	// Reload configs relevant to the HTTP module
	if (bUpdateHttpConfigs)
	{
		FHttpModule::Get().UpdateConfigs();
	}

	// Reload configs relevant to OSS config sections that were updated
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(OSSName.Len() ? FName(*OSSName, FNAME_Find) : NAME_None);
	if (OnlineSub != nullptr)
	{
		OnlineSub->ReloadConfigs(OnlineSubSections);
	}

	UE_LOG(LogHotfixManager, Log, TEXT("Updating config from %s took %f seconds and reloaded %d objects"),
		*FileName, FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
	return true;
}

void UOnlineHotfixManager::HotfixLocFile(const FCloudFileHeader& FileHeader)
{
	const double StartTime = FPlatformTime::Seconds();
	FString LocFilePath = FString::Printf(TEXT("%s/%s"), *GetCachedDirectory(), *FileHeader.DLName);
	FTextLocalizationManager::Get().UpdateFromLocalizationResource(LocFilePath);
	UE_LOG(LogHotfixManager, Log, TEXT("Updating loc from %s took %f seconds"), *FileHeader.FileName, FPlatformTime::Seconds() - StartTime);
}

bool UOnlineHotfixManager::HotfixPakFile(const FCloudFileHeader& FileHeader)
{
	if (!FCoreDelegates::OnMountPak.IsBound())
	{
		UE_LOG(LogHotfixManager, Error, TEXT("PAK file (%s) could not be mounted because OnMountPak is not bound"), *FileHeader.FileName);
		return false;
	}
	FString PakLocation = FString::Printf(TEXT("%s/%s"), *GetCachedDirectory(), *FileHeader.DLName);
	FPakFileVisitor Visitor;
	if (FCoreDelegates::OnMountPak.Execute(PakLocation, 0, &Visitor))
	{
		MountedPakFiles.Add(FileHeader.DLName);
		UE_LOG(LogHotfixManager, Log, TEXT("Hotfix mounted PAK file (%s)"), *FileHeader.FileName);
		int32 NumInisReloaded = 0;
		const double StartTime = FPlatformTime::Seconds();
		TArray<FString> IniList;
		// Iterate through the pak file's contents for INI and asset reloading
		for (const FString& InternalPakFileName : Visitor.Files)
		{
			if (InternalPakFileName.EndsWith(TEXT(".ini")))
			{
				IniList.Add(InternalPakFileName);
			}
			else if (!bHotfixNeedsMapReload && InternalPakFileName.EndsWith(FPackageName::GetMapPackageExtension()))
			{
				bHotfixNeedsMapReload = IsMapLoaded(InternalPakFileName);
			}
		}
		// Sort the INIs so they are processed consistently
		IniList.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(PlatformPrefix, ServerPrefix, DefaultPrefix));
		// Now process the INIs in sorted order
		for (const FString& IniName : IniList)
		{
			HotfixPakIniFile(IniName);
			NumInisReloaded++;
		}
		UE_LOG(LogHotfixManager, Log, TEXT("Processing pak file (%s) took %f seconds and resulted in (%d) INIs being reloaded"),
			*FileHeader.FileName, FPlatformTime::Seconds() - StartTime, NumInisReloaded);
#if !UE_BUILD_SHIPPING
		if (bLogMountedPakContents)
		{
			UE_LOG(LogHotfixManager, Log, TEXT("Files in pak file (%s):"), *FileHeader.FileName);
			for (const FString& FileName : Visitor.Files)
			{
				UE_LOG(LogHotfixManager, Log, TEXT("\t\t%s"), *FileName);
			}
		}
#endif
		return true;
	}
	return false;
}

bool UOnlineHotfixManager::IsMapLoaded(const FString& MapName)
{
	FString MapPackageName(MapName.Left(MapName.Len() - 5));
	MapPackageName = MapPackageName.Replace(*GameContentPath, TEXT("/Game"));
	// If this map's UPackage exists, it is currently loaded
	UPackage* MapPackage = FindObject<UPackage>(ANY_PACKAGE, *MapPackageName, true);
	return MapPackage != nullptr;
}

bool UOnlineHotfixManager::HotfixPakIniFile(const FString& FileName)
{
	FString StrippedName;
	const double StartTime = FPlatformTime::Seconds();
	// Need to strip off the PAK path
	FileName.Split(TEXT("/"), nullptr, &StrippedName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FConfigFile* ConfigFile = GetConfigFile(StrippedName);
	if (!ConfigFile->Combine(FString(TEXT("../../../")) + FileName.Replace(*GameContentPath, TEXT("/Game"))))
	{
		UE_LOG(LogHotfixManager, Log, TEXT("Hotfix failed to merge INI (%s) found in a PAK file"), *FileName);
		return false;
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Hotfix merged INI (%s) found in a PAK file"), *FileName);
	int32 NumObjectsReloaded = 0;
	// Now that we have a list of classes to update, we can iterate objects and
	// reload if they match the INI file that was changed
	TArray<UObject*> Classes;
	GetObjectsOfClass(UClass::StaticClass(), Classes, true, RF_NoFlags);
	TArray<UClass*> ClassesToReload;
	for (UObject* ClassObject : Classes)
	{
		if (UClass* const Class = Cast<UClass>(ClassObject))
		{
			if (Class->HasAnyClassFlags(CLASS_Config) &&
				Class->ClassConfigName == ConfigFile->Name)
			{
				TArray<UObject*> Objects;
				GetObjectsOfClass(Class, Objects, true, RF_NoFlags);
				for (UObject* Object : Objects)
				{
					if (!Object->IsPendingKill())
					{
						// Force a reload of the config vars
						Object->ReloadConfig();
						NumObjectsReloaded++;
					}
				}
			}
		}
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Updating config from %s took %f seconds reloading %d objects"),
		*FileName, FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
	return true;
}

const FString UOnlineHotfixManager::GetFriendlyNameFromDLName(const FString& DLName) const
{
	for (const FCloudFileHeader& Header : HotfixFileList)
	{
		if (Header.DLName == DLName)
		{
			return Header.FileName;
		}
	}
	return FString();
}

void UOnlineHotfixManager::UnmountHotfixFiles()
{
	if (MountedPakFiles.Num() == 0)
	{
		return;
	}
	// Unmount any changed hotfix files since we need to download them again
	for (const FCloudFileHeader& FileHeader : ChangedHotfixFileList)
	{
		for (int32 Index = 0; Index < MountedPakFiles.Num(); Index++)
		{
			if (MountedPakFiles[Index] == FileHeader.DLName)
			{
				FCoreDelegates::OnUnmountPak.Execute(MountedPakFiles[Index]);
				MountedPakFiles.RemoveAt(Index);
				ChangedOrRemovedPakCount++;
				UE_LOG(LogHotfixManager, Log, TEXT("Hotfix unmounted PAK file (%s) so it can be redownloaded"), *FileHeader.FileName);
				break;
			}
		}
	}
	// Unmount any removed hotfix files
	for (const FCloudFileHeader& FileHeader : RemovedHotfixFileList)
	{
		for (int32 Index = 0; Index < MountedPakFiles.Num(); Index++)
		{
			if (MountedPakFiles[Index] == FileHeader.DLName)
			{
				FCoreDelegates::OnUnmountPak.Execute(MountedPakFiles[Index]);
				MountedPakFiles.RemoveAt(Index);
				ChangedOrRemovedPakCount++;
				UE_LOG(LogHotfixManager, Log, TEXT("Hotfix unmounted PAK file (%s) since it was removed from the hotfix set"), *FileHeader.FileName);
				break;
			}
		}
	}
}

FCloudFileHeader* UOnlineHotfixManager::GetFileHeaderFromDLName(const FString& FileName)
{
	for (int32 Index = 0; Index < HotfixFileList.Num(); Index++)
	{
		if (HotfixFileList[Index].DLName == FileName)
		{
			return &HotfixFileList[Index];
		}
	}
	return nullptr;
}

void UOnlineHotfixManager::OnReadFileProgress(const FString& FileName, uint64 BytesRead)
{
	if (PendingHotfixFiles.Contains(FileName))
	{
		// Since the title file is reporting absolute numbers subtract out the last update so we can add a delta
		uint64 Delta = BytesRead - PendingHotfixFiles[FileName].Progress;
		PendingHotfixFiles[FileName].Progress = BytesRead;
		// Completion updates the file count and progress updates the byte count
		UpdateProgress(0, Delta);
	}
}

UOnlineHotfixManager::FConfigFileBackup& UOnlineHotfixManager::BackupIniFile(const FString& IniName, const FConfigFile* ConfigFile)
{
	FString BackupIniName = GetConfigFileNamePath(GetStrippedConfigFileName(IniName));
	if (FConfigFileBackup* Backup = IniBackups.FindByPredicate([&BackupIniName](const FConfigFileBackup& Entry){ return Entry.IniName == BackupIniName; }))
	{
		// Only store one copy of each ini file, consisting of the original state
		return *Backup;
	}

	int32 AddAt = IniBackups.AddDefaulted();
	FConfigFileBackup& NewBackup = IniBackups[AddAt];
	NewBackup.IniName = BackupIniName;
	NewBackup.ConfigData = *ConfigFile;
	// There's a lack of deep copy related to the SourceConfigFile so null it out
	NewBackup.ConfigData.SourceConfigFile = nullptr;
	return NewBackup;
}

void UOnlineHotfixManager::RestoreBackupIniFiles()
{
	if (IniBackups.Num() == 0)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	TArray<FString> ClassesToRestore;

	// Restore any changed INI files and build a list of which ones changed for UObject reloading below
	for (const FCloudFileHeader& FileHeader : ChangedHotfixFileList)
	{
		if (FileHeader.FileName.EndsWith(TEXT(".INI")))
		{
			const FString ProcessedName = GetConfigFileNamePath(GetStrippedConfigFileName(FileHeader.FileName));
			for (int32 Index = 0; Index < IniBackups.Num(); Index++)
			{
				const FConfigFileBackup& BackupFile = IniBackups[Index];
				if (IniBackups[Index].IniName == ProcessedName)
				{
					ClassesToRestore.Append(BackupFile.ClassesReloaded);

					GConfig->SetFile(BackupFile.IniName, &BackupFile.ConfigData);
					IniBackups.RemoveAt(Index);
					break;
				}
			}
		}
	}

	// Also restore any files that were previously part of the hotfix and now are not
	for (const FCloudFileHeader& FileHeader : RemovedHotfixFileList)
	{
		if (FileHeader.FileName.EndsWith(TEXT(".INI")))
		{
			const FString ProcessedName = GetConfigFileNamePath(GetStrippedConfigFileName(FileHeader.FileName));
			for (int32 Index = 0; Index < IniBackups.Num(); Index++)
			{
				const FConfigFileBackup& BackupFile = IniBackups[Index];
				if (BackupFile.IniName == ProcessedName)
				{
					ClassesToRestore.Append(BackupFile.ClassesReloaded);

					GConfig->SetFile(BackupFile.IniName, &BackupFile.ConfigData);
					IniBackups.RemoveAt(Index);
					break;
				}
			}
		}
	}

	uint32 NumObjectsReloaded = 0;
	if (ClassesToRestore.Num() > 0)
	{
		TArray<UClass*> RestoredClasses;
		RestoredClasses.Reserve(ClassesToRestore.Num());
		for (int32 Index = 0; Index < ClassesToRestore.Num(); Index++)
		{
			UClass* Class = FindObject<UClass>(nullptr, *ClassesToRestore[Index], true);
			if (Class != nullptr)
			{
				// Add this to the list to check against
				RestoredClasses.Add(Class);
			}
		}

		for (UClass* Class : RestoredClasses)
		{
			if (Class->HasAnyClassFlags(CLASS_Config))
			{
				TArray<UObject*> Objects;
				GetObjectsOfClass(Class, Objects, true, RF_NoFlags);
				for (UObject* Object : Objects)
				{
					if (!Object->IsPendingKill())
					{
						UE_LOG(LogHotfixManager, Verbose, TEXT("Restoring %s"), *Object->GetPathName());
						Object->ReloadConfig();
						NumObjectsReloaded++;
					}
				}
			}
		}
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Restoring config for %d changed classes took %f seconds reloading %d objects"),
		ClassesToRestore.Num(), FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
}

void UOnlineHotfixManager::PatchAssetsFromIniFiles()
{
	UE_LOG( LogHotfixManager, Display, TEXT( "Checking for assets to be patched using data from 'AssetHotfix' section in the Game .ini file" ) );

	int32 TotalPatchableAssets = 0;
	AssetsHotfixedFromIniFiles.Reset();

	// Everything should be under the 'AssetHotfix' section in Game.ini
	FConfigSection* AssetHotfixConfigSection = GConfig->GetSectionPrivate( TEXT( "AssetHotfix" ), false, true, GGameIni );
	if( AssetHotfixConfigSection != nullptr )
	{
		for( FConfigSection::TIterator It( *AssetHotfixConfigSection ); It; ++It )
		{
			++TotalPatchableAssets;

			TArray<UClass*> PatchableAssetClasses;
			{
				// These are the asset types we support patching right now
				PatchableAssetClasses.Add(UCurveTable::StaticClass());
				PatchableAssetClasses.Add(UDataTable::StaticClass());
				PatchableAssetClasses.Add(UCurveFloat::StaticClass());
			}

			// Make sure the entry has a valid class name that we supprt
			UClass* AssetClass = nullptr;
			for( UClass* PatchableAssetClass : PatchableAssetClasses )
			{
				if( PatchableAssetClass && (It.Key() == PatchableAssetClass->GetFName()) )
				{
					AssetClass = PatchableAssetClass;
					break;
				}
			}

			if (AssetClass != nullptr)
			{
				TArray<FString> ProblemStrings;

				FString DataLine(*It.Value().GetValue());

				if (!DataLine.IsEmpty())
				{
					TArray<FString> Tokens;
					DataLine.ParseIntoArray(Tokens, TEXT(";"));
					if (Tokens.Num() == 3 || Tokens.Num() == 5)
					{
						const FString& AssetPath(Tokens[0]);
						const FString& HotfixType(Tokens[1]);

						// Find or load the asset
						UObject* Asset = StaticLoadObject(AssetClass, nullptr, *AssetPath);
						if (Asset != nullptr)
						{
							const FString RowUpdate(TEXT("RowUpdate"));
							const FString TableUpdate(TEXT("TableUpdate"));

							if (HotfixType == RowUpdate && Tokens.Num() == 5)
							{
								// The hotfix line should be
								//	+DataTable=<data table path>;RowUpdate;<row name>;<column name>;<new value>
								//	+CurveTable=<curve table path>;RowUpdate;<row name>;<column name>;<new value>
								//	+CurveFloat=<curve float path>;RowUpdate;None;<column name>;<new value>
								HotfixRowUpdate(Asset, AssetPath, Tokens[2], Tokens[3], Tokens[4], ProblemStrings);
							}
							else if (HotfixType == TableUpdate && Tokens.Num() == 3)
							{
								// The hotfix line should be
								//	+DataTable=<data table path>;TableUpdate;"<json data>"
								//	+CurveTable=<curve table path>;TableUpdate;"<json data>"
								
								// We have to read json data as quoted string because tokenizing it creates extra unwanted characters.
								FString JsonData;
								if (FParse::QuotedString(*Tokens[2], JsonData))
								{
									HotfixTableUpdate(Asset, AssetPath, JsonData, ProblemStrings);
								}
								else
								{
									ProblemStrings.Add(TEXT("Json data wasn't able to be parsed as a quoted string. Check that we have opening and closing quotes around the json data."));
								}
							}
							else
							{
								ProblemStrings.Add(TEXT("Expected a hotfix type of RowUpdate with 5 tokens or TableUpdate with 3 tokens."));
							}
						}
						else
						{
							const FString Problem(FString::Printf(TEXT("Couldn't find or load asset '%s' (class '%s').  This asset will not be patched.  Double check that your asset type and path string is correct."), *AssetPath, *AssetClass->GetPathName()));
							ProblemStrings.Add(Problem);
						}

						if (ProblemStrings.Num() > 0)
						{
							for (const FString& ProblemString : ProblemStrings)
							{
								UE_LOG(LogHotfixManager, Error, TEXT("%s: %s"), *GetPathNameSafe(Asset), *ProblemString);
							}
						}
						else
						{
							// We'll keep a reference to the successfully patched asset.  We want to make sure our changes survive throughout
							// this session, so we reference it to prevent it from being evicted from memory.  It's OK if we end up re-patching
							// the same asset multiple times per session.
							AssetsHotfixedFromIniFiles.Add(Asset);
						}
					}
					else
					{
						UE_LOG(LogHotfixManager, Error, TEXT("Wasn't able to parse the data with semicolon separated values. Expecting 3 or 5 arguments."));
					}
				}
			}
		}
	}

	if( TotalPatchableAssets == 0 )
	{
		UE_LOG( LogHotfixManager, Display, TEXT( "No assets were found in the 'AssetHotfix' section in the Game .ini file.  No patching needed." ) );
	}
	else if( TotalPatchableAssets == AssetsHotfixedFromIniFiles.Num() )
	{
		UE_LOG( LogHotfixManager, Display, TEXT( "Successfully patched all %i assets from the 'AssetHotfix' section in the Game .ini file.  These assets will be forced to remain loaded." ), AssetsHotfixedFromIniFiles.Num() );
	}
	else
	{
		UE_LOG( LogHotfixManager, Error, TEXT( "Only %i of %i assets were successfully patched from 'AssetHotfix' section in the Game .ini file.  The patched assets will be forced to remain loaded.  Any assets that failed to patch may be left in an invalid state!" ), AssetsHotfixedFromIniFiles.Num(), TotalPatchableAssets );
	}
}


void UOnlineHotfixManager::HotfixRowUpdate(UObject* Asset, const FString& AssetPath, const FString& RowName, const FString& ColumnName, const FString& NewValue, TArray<FString>& ProblemStrings)
{
	if (AssetPath.IsEmpty())
	{
		ProblemStrings.Add(TEXT("The table's path is empty. We cannot continue the hotfix."));
		return;
	}
	if (RowName.IsEmpty())
	{
		ProblemStrings.Add(TEXT("The row name is empty. We cannot continue the hotfix."));
		return;
	}
	if (ColumnName.IsEmpty())
	{
		ProblemStrings.Add(TEXT("The column name is empty. We cannot continue the hotfix."));
		return;
	}
	if (NewValue.IsEmpty())
	{
		ProblemStrings.Add(TEXT("The new value is empty. We cannot continue the hotfix."));
		return;
	}

	UDataTable* DataTable = Cast<UDataTable>(Asset);
	UCurveTable* CurveTable = Cast<UCurveTable>(Asset);
	UCurveFloat* CurveFloat = Cast<UCurveFloat>(Asset);
	if (DataTable != nullptr)
	{
		// Edit the row with the new value.
		UProperty* DataTableRowProperty = DataTable->RowStruct->FindPropertyByName(FName(*ColumnName));
		if (DataTableRowProperty)
		{
			UNumericProperty* NumProp = Cast<UNumericProperty>(DataTableRowProperty);
			if (NumProp)
			{
				if (NewValue.IsNumeric())
				{
					// Get the row data by name.
					static const FString Context = FString(TEXT("UOnlineHotfixManager::PatchAssetsFromIniFiles"));
					FTableRowBase* DataTableRow = DataTable->FindRow<FTableRowBase>(FName(*RowName), Context);

					if (DataTableRow)
					{
						void* RowData = NumProp->ContainerPtrToValuePtr<void>(DataTableRow, 0);

						if (RowData)
						{
							if (NumProp->IsInteger())
							{
								const int64 OldPropertyValue = NumProp->GetSignedIntPropertyValue(RowData);
								const int64 NewPropertyValue = FCString::Atoi(*NewValue);
								NumProp->SetIntPropertyValue(RowData, NewPropertyValue);
								UE_LOG(LogHotfixManager, Display, TEXT("Data table %s row %s updated column %s from %i to %i."), *AssetPath, *RowName, *ColumnName, OldPropertyValue, NewPropertyValue);
							}
							else
							{
								const double OldPropertyValue = NumProp->GetFloatingPointPropertyValue(RowData);
								const double NewPropertyValue = FCString::Atod(*NewValue);
								NumProp->SetFloatingPointPropertyValue(RowData, NewPropertyValue);
								UE_LOG(LogHotfixManager, Display, TEXT("Data table %s row %s updated column %s from %.2f to %.2f."), *AssetPath, *RowName, *ColumnName, OldPropertyValue, NewPropertyValue);
							}
						}
						else
						{
							const FString Problem(FString::Printf(TEXT("The data table row data for row %s was not found."), *RowName));
							ProblemStrings.Add(Problem);
						}
					}
					else
					{
						const FString Problem(FString::Printf(TEXT("The data table row %s was not found."), *RowName));
						ProblemStrings.Add(Problem);
					}
				}
				else
				{
					const FString Problem(FString::Printf(TEXT("The new value %s is not a number when it should be."), *NewValue));
					ProblemStrings.Add(Problem);
				}
			}
			else
			{
				const FString Problem(FString::Printf(TEXT("The data table row property named %s is not a numeric property and it should be."), *ColumnName));
				ProblemStrings.Add(Problem);
			}
		}
		else
		{
			const FString Problem(FString::Printf(TEXT("Couldn't find the data table property named %s. Check the spelling."), *ColumnName));
			ProblemStrings.Add(Problem);
		}
	}
	else if (CurveTable)
	{
		if (ColumnName.IsNumeric())
		{
			// Get the row data by name.
			static const FString Context = FString(TEXT("UOnlineHotfixManager::PatchAssetsFromIniFiles"));
			FRichCurve* CurveTableRow = CurveTable->FindCurve(FName(*RowName), Context);
			
			if (CurveTableRow)
			{
				// Edit the row with the new value.
				const float KeyTime = FCString::Atof(*ColumnName);
				FKeyHandle Key = CurveTableRow->FindKey(KeyTime);
				if (CurveTableRow->IsKeyHandleValid(Key))
				{
					if (NewValue.IsNumeric())
					{
						const float OldPropertyValue = CurveTableRow->GetKeyValue(Key);
						const float NewPropertyValue = FCString::Atof(*NewValue);
						CurveTableRow->SetKeyValue(Key, NewPropertyValue);

						UE_LOG(LogHotfixManager, Display, TEXT("Curve table %s row %s updated column %s from %.2f to %.2f."), *AssetPath, *RowName, *ColumnName, OldPropertyValue, NewPropertyValue);
					}
					else
					{
						const FString Problem(FString::Printf(TEXT("The new value %s is not a number when it should be."), *NewValue));
						ProblemStrings.Add(Problem);
					}
				}
				else
				{
					const FString Problem(FString::Printf(TEXT("The column name %s isn't a valid key into the curve table."), *ColumnName));
					ProblemStrings.Add(Problem);
				}
			}
			else
			{
				const FString Problem(FString::Printf(TEXT("The curve table row for row name %s was not found."), *RowName));
				ProblemStrings.Add(Problem);
			}
		}
		else
		{
			const FString Problem(FString::Printf(TEXT("The column name %s is not a number when it should be."), *ColumnName));
			ProblemStrings.Add(Problem);
		}
	}
	else if (CurveFloat)
	{
		if (ColumnName.IsNumeric())
		{
			// Edit the curve with the new value.
			const float KeyTime = FCString::Atof(*ColumnName);
			FKeyHandle Key = CurveFloat->FloatCurve.FindKey(KeyTime);
			if (CurveFloat->FloatCurve.IsKeyHandleValid(Key))
			{
				if (NewValue.IsNumeric())
				{
					const float OldPropertyValue = CurveFloat->FloatCurve.GetKeyValue(Key);
					const float NewPropertyValue = FCString::Atof(*NewValue);
					CurveFloat->FloatCurve.SetKeyValue(Key, NewPropertyValue);

					UE_LOG(LogHotfixManager, Display, TEXT("Curve float %s updated column %s from %.2f to %.2f."), *AssetPath, *ColumnName, OldPropertyValue, NewPropertyValue);
				}
				else
				{
					const FString Problem(FString::Printf(TEXT("The new value %s is not a number when it should be."), *NewValue));
					ProblemStrings.Add(Problem);
				}
			}
			else
			{
				const FString Problem(FString::Printf(TEXT("The column name %s isn't a valid key into the curve float."), *ColumnName));
				ProblemStrings.Add(Problem);
			}
		}
		else
		{
			const FString Problem(FString::Printf(TEXT("The column name %s is not a number when it should be."), *ColumnName));
			ProblemStrings.Add(Problem);
		}
	}
	else
	{
		ProblemStrings.Add(TEXT("The Asset isn't a Data Table, Curve Table, or Curve Float."));
	}
}

void UOnlineHotfixManager::HotfixTableUpdate(UObject* Asset, const FString& AssetPath, const FString& JsonData, TArray<FString>& ProblemStrings)
{
	if (AssetPath.IsEmpty())
	{
		ProblemStrings.Add(TEXT("The table's path is empty. We cannot continue the hotfix."));
		return;
	}
	if (JsonData.IsEmpty())
	{
		ProblemStrings.Add(TEXT("The JSON data is empty. We cannot continue the hotfix."));
		return;
	}

	// Let's import over the object in place.
	UCurveTable* CurveTable = Cast<UCurveTable>(Asset);
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (CurveTable != nullptr)
	{
		ProblemStrings.Append(CurveTable->CreateTableFromJSONString(JsonData));
		UE_LOG(LogHotfixManager, Display, TEXT("Curve table %s updated."), *AssetPath);
	}
	else if (DataTable != nullptr)
	{
		ProblemStrings.Append(DataTable->CreateTableFromJSONString(JsonData));
		UE_LOG(LogHotfixManager, Display, TEXT("Data table %s updated."), *AssetPath);
	}
	else
	{
		ProblemStrings.Add(TEXT("We can't do a table update on this asset (for example, Curve Float cannot be table updated)."));
	}
}

struct FHotfixManagerExec :
	public FSelfRegisteringExec
{
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("HOTFIX")))
		{
			UOnlineHotfixManager* HotfixManager = UOnlineHotfixManager::Get(InWorld);
			if (HotfixManager != nullptr)
			{
				HotfixManager->StartHotfixProcess();
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("TESTHOTFIXSORT")))
		{
			TArray<FCloudFileHeader> TestList;
			FCloudFileHeader Header;
			Header.FileName = TEXT("SomeRandom.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("DedicatedServerGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("pakchunk1-PS4_P.pak");
			TestList.Add(Header);
			Header.FileName = TEXT("EN_Game.locres");
			TestList.Add(Header);
			Header.FileName = TEXT("DefaultGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("Ver-1234_DefaultEngine.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("PS4_DefaultEngine.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("DefaultEngine.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("pakchunk0-PS4_P.pak");
			TestList.Add(Header);
			Header.FileName = TEXT("PS4_DefaultGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("Ver-1234_PS4_DefaultGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("PS4_Ver-1234_DefaultGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("AnotherRandom.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("DedicatedServerEngine.ini");
			TestList.Add(Header);
			TestList.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(TEXT("PS4_"), TEXT("DedicatedServer"), TEXT("Default")));

			UE_LOG(LogHotfixManager, Log, TEXT("Hotfixing sort is:"));
			for (const FCloudFileHeader& FileHeader : TestList)
			{
				UE_LOG(LogHotfixManager, Log, TEXT("\t%s"), *FileHeader.FileName);
			}

			TArray<FString> TestList2;
			TestList2.Add(TEXT("SomeRandom.ini"));
			TestList2.Add(TEXT("DefaultGame.ini"));
			TestList2.Add(TEXT("PS4_DefaultEngine.ini"));
			TestList2.Add(TEXT("DedicatedServerEngine.ini"));
			TestList2.Add(TEXT("DedicatedServerGame.ini"));
			TestList2.Add(TEXT("DefaultEngine.ini"));
			TestList2.Add(TEXT("PS4_DefaultGame.ini"));
			TestList2.Add(TEXT("AnotherRandom.ini"));
			TestList2.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(TEXT("PS4_"), TEXT("DedicatedServer"), TEXT("Default")));

			UE_LOG(LogHotfixManager, Log, TEXT("Hotfixing PAK INI file sort is:"));
			for (const FString& IniName : TestList2)
			{
				UE_LOG(LogHotfixManager, Log, TEXT("\t%s"), *IniName);
			}
			return true;
		}
		return false;
	}
};
static FHotfixManagerExec HotfixManagerExec;

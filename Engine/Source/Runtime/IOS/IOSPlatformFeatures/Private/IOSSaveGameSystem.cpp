// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IOSSaveGameSystem.h"
#include "GameDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogIOSSaveGame, Log, All);

//
// Implementation members
//

FIOSSaveGameSystem::FIOSSaveGameSystem()
{
	Initialize();
}

FIOSSaveGameSystem::~FIOSSaveGameSystem()
{
	Shutdown();
}

void FIOSSaveGameSystem::Initialize()
{
}

void FIOSSaveGameSystem::Shutdown()
{
}

ISaveGameSystem::ESaveExistsResult FIOSSaveGameSystem::DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex)
{
	if (IFileManager::Get().FileSize(*GetSaveGamePath(Name)) >= 0)
	{
		return ESaveExistsResult::OK;
	}
	return ESaveExistsResult::DoesNotExist;
}

bool FIOSSaveGameSystem::SaveGameNoCloud(const TCHAR* Name, const TArray<uint8>& Data)
{
	return FFileHelper::SaveArrayToFile(Data, *GetSaveGamePath(Name));
}

bool FIOSSaveGameSystem::SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data)
{
	// send to the iCloud, if enabled
	OnWriteUserCloudFileBeginDelegate.ExecuteIfBound(FString(Name), Data);

	return FFileHelper::SaveArrayToFile(Data, *GetSaveGamePath(Name));
}

bool FIOSSaveGameSystem::LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data)
{
	// try to read it from the iCloud, if enabled
	OnReadUserCloudFileBeginDelegate.ExecuteIfBound(FString(Name), Data);

	if (Data.Num() > 0)
	{
		// we've received data from the iCloud, the save file was overwritten
		return true;
	}

	// no iCloud data, read from the local storage
	return FFileHelper::LoadFileToArray(Data, *GetSaveGamePath(Name));
}

bool FIOSSaveGameSystem::DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex)
{
	// delete the file from the iCloud
	OnDeleteUserCloudFileBeginDelegate.ExecuteIfBound(FString(Name));

	// delete the file from the local storage
	return IFileManager::Get().Delete(*GetSaveGamePath(Name), true, false, !bAttemptToUseUI);
}

FString FIOSSaveGameSystem::GetSaveGamePath(const TCHAR* Name)
{
	return FString::Printf(TEXT("%s""SaveGames/%s.sav"), *FPaths::ProjectSavedDir(), Name);
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SaveGameSystem.h"

class FIOSSaveGameSystem : public ISaveGameSystem
{
public:
	FIOSSaveGameSystem();
	virtual ~FIOSSaveGameSystem();

	// ISaveGameSystem interface
	virtual bool PlatformHasNativeUI() override
	{
		return false;
	}

	virtual bool DoesSaveGameExist(const TCHAR* Name, const int32 UserIndex) override
	{
		return ESaveExistsResult::OK == DoesSaveGameExistWithResult(Name, UserIndex);
	}

	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex) override;

	/**
	 * Called on the initial iCloud sync
	 */
	virtual bool SaveGameNoCloud(const TCHAR* Name, const TArray<uint8>& Data);
	
	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data) override;
	/**
	 * Called when writing the savegame file
	 * send the file to iCloud, if enabled
	 */
	DECLARE_DELEGATE_TwoParams(FOnWriteUserCloudFileBegin, const FString&, const TArray<uint8>&);
	FOnWriteUserCloudFileBegin OnWriteUserCloudFileBeginDelegate;

	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data) override;
	/**
	 * Called when reading the savegame file
	 * read the file from iCloud, if enabled
	 */
	DECLARE_DELEGATE_TwoParams(FOnReadUserCloudFileBegin, const FString&, TArray<uint8>&);
	FOnReadUserCloudFileBegin OnReadUserCloudFileBeginDelegate;

	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex) override;
	/**
	 * Called when deleting the savegame file
	 * delete the file from iCloud, if enabled
	 */
	DECLARE_DELEGATE_OneParam(FOnDeleteUserCloudFileBegin, const FString&);
	FOnDeleteUserCloudFileBegin OnDeleteUserCloudFileBeginDelegate;

private:
	/**
	 * Initializes the SaveData library then loads and initializes the SaveDialog library
	 */
	void Initialize();

	/**
	 * Terminates and unloads the SaveDialog library then terminates the SaveData library
	 */
	void Shutdown();

	/**
	 * Get the path to save game file for the given name
	 */
	virtual FString GetSaveGamePath(const TCHAR* Name);
};

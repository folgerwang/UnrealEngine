// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SQLiteDatabase.h"
#include "SQLiteCore.h"
#include "IncludeSQLite.h"

#include "Misc/AssertionMacros.h"
#include "Containers/StringConv.h"

FSQLiteDatabase::FSQLiteDatabase()
	: Database(nullptr)
{
	// Ensure SQLite is initialized (as our module may not have loaded yet)
	FSQLiteCore::StaticInitializeSQLite();
}

FSQLiteDatabase::~FSQLiteDatabase()
{
	checkf(!Database, TEXT("Destructor called while an SQLite database was still open. Did you forget to call Close?"));
}

FSQLiteDatabase::FSQLiteDatabase(FSQLiteDatabase&& Other)
	: Database(Other.Database)
{
	Other.Database = nullptr;
}

FSQLiteDatabase& FSQLiteDatabase::operator=(FSQLiteDatabase&& Other)
{
	if (this != &Other)
	{
		Close();

		Database = Other.Database;
		Other.Database = nullptr;
	}
	return *this;
}

bool FSQLiteDatabase::IsValid() const
{
	return Database != nullptr;
}

bool FSQLiteDatabase::Open(const TCHAR* InFilename, const ESQLiteDatabaseOpenMode InOpenMode)
{
	if (Database)
	{
		return false;
	}

	int32 OpenFlags = 0;
	switch (InOpenMode)
	{
	case ESQLiteDatabaseOpenMode::ReadOnly:
		OpenFlags = SQLITE_OPEN_READONLY;
		break;
	case ESQLiteDatabaseOpenMode::ReadWrite:
		OpenFlags = SQLITE_OPEN_READWRITE;
		break;
	case ESQLiteDatabaseOpenMode::ReadWriteCreate:
		OpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
		break;
	}
	checkf(OpenFlags != 0, TEXT("SQLite open flags were zero! Unhandled ESQLiteDatabaseOpenMode?"));

	return sqlite3_open_v2(TCHAR_TO_UTF8(InFilename), &Database, OpenFlags, nullptr) == SQLITE_OK;
}

bool FSQLiteDatabase::Close()
{
	if (!Database)
	{
		return false;
	}

	const int32 Result = sqlite3_close(Database);
	if (Result != SQLITE_OK)
	{
		return false;
	}
	
	Database = nullptr;
	return true;
}

bool FSQLiteDatabase::Execute(const TCHAR* InStatement)
{
	if (!Database)
	{
		return false;
	}

	// Create a prepared statement
	FSQLitePreparedStatement Statement(*this, InStatement);
	if (!Statement.IsValid())
	{
		return false;
	}

	// Step it to completion (or error)
	ESQLitePreparedStatementStepResult StepResult = ESQLitePreparedStatementStepResult::Row;
	while (StepResult == ESQLitePreparedStatementStepResult::Row)
	{
		StepResult = Statement.Step();
	}

	return StepResult != ESQLitePreparedStatementStepResult::Error;
}

FSQLitePreparedStatement FSQLiteDatabase::PrepareStatement(const TCHAR* InStatement, const ESQLitePreparedStatementFlags InFlags)
{
	return Database
		? FSQLitePreparedStatement(*this, InStatement, InFlags)
		: FSQLitePreparedStatement();
}

FString FSQLiteDatabase::GetLastError() const
{
	const char* ErrorStr = Database ? sqlite3_errmsg(Database) : nullptr;
	if (ErrorStr)
	{
		return UTF8_TO_TCHAR(ErrorStr);
	}
	return FString();
}

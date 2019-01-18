// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SQLitePreparedStatement.h"

/**
 * Modes used when opening a database.
 */
enum class ESQLiteDatabaseOpenMode : uint8
{
	/** Open the database in read-only mode. Fails if the database doesn't exist. */
	ReadOnly,

	/** Open the database in read-write mode if possible, or read-only mode if the database is write protected. Fails if the database doesn't exist. */
	ReadWrite,

	/** Open the database in read-write mode if possible, or read-only mode if the database is write protected. Attempts to create the database if it doesn't exist. */
	ReadWriteCreate,
};

/**
 * Wrapper around an SQLite database.
 * @see sqlite3.
 */
class SQLITECORE_API FSQLiteDatabase
{
public:
	/** Construction/Destruction */
	FSQLiteDatabase();
	~FSQLiteDatabase();

	/** Non-copyable */
	FSQLiteDatabase(const FSQLiteDatabase&) = delete;
	FSQLiteDatabase& operator=(const FSQLiteDatabase&) = delete;

	/** Movable */
	FSQLiteDatabase(FSQLiteDatabase&& Other);
	FSQLiteDatabase& operator=(FSQLiteDatabase&& Other);

	/**
	 * Is this a valid SQLite database? (ie, has been successfully opened).
	 */
	bool IsValid() const;

	/**
	 * Open (or create) an SQLite database file.
	 */
	bool Open(const TCHAR* InFilename, const ESQLiteDatabaseOpenMode InOpenMode = ESQLiteDatabaseOpenMode::ReadWriteCreate);

	/**
	 * Close an open SQLite database file.
	 */
	bool Close();

	/**
	 * Execute a statement that requires no result state.
	 * @note For statements that require a result, or that you wish to reuse repeatedly (including using binding), you should consider using FSQLitePreparedStatement directly.
	 * @return true if the execution was a success.
	 */
	bool Execute(const TCHAR* InStatement);

	/**
	 * Prepare a statement for manual processing.
	 * @note This is the same as using the FSQLitePreparedStatement constructor, but won't assert if the current database is invalid (not open).
	 * @return A prepared statement object (check IsValid on the result).
	 */
	FSQLitePreparedStatement PrepareStatement(const TCHAR* InStatement, const ESQLitePreparedStatementFlags InFlags = ESQLitePreparedStatementFlags::None);

	/**
	 * Get the last error reported by this database.
	 */
	FString GetLastError() const;

private:
	friend class FSQLitePreparedStatement;

	/** Internal SQLite database handle */
	struct sqlite3* Database;
};

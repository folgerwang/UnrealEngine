// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SQLiteTypes.h"
#include "Misc/EnumClassFlags.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

class FSQLiteDatabase;

/**
 * Flags used when creating a prepared statement.
 */
enum class ESQLitePreparedStatementFlags : uint8
{
	/** No special flags. */
	None = 0,

	/** Hints that this prepared statement will be retained for a long period and reused many times. */
	Persistent = 1<<0,
};
ENUM_CLASS_FLAGS(ESQLitePreparedStatementFlags);

/**
 * Result codes returned from stepping an SQLite prepared statement.
 */
enum class ESQLitePreparedStatementStepResult : uint8
{
	/** The step was unsuccessful and enumeration should be aborted. */
	Error,

	/** The step was unsuccessful as the required locks could not be acquired. If the statement was outside a transaction (or committing a pending transaction) then you can retry it, otherwise enumeration should be aborted and you should rollback any pending transaction. */
	Busy,

	/** The step was successful and we're on a database row. */
	Row,

	/** The step was successful, but we've reached the end of the rows and enumeration should be aborted. */
	Done,
};

/**
 * Wrapper around an SQLite prepared statement.
 * @see sqlite3_stmt.
 */
class SQLITECORE_API FSQLitePreparedStatement
{
public:
	/** Construction/Destruction */
	FSQLitePreparedStatement();
	FSQLitePreparedStatement(FSQLiteDatabase& InDatabase, const TCHAR* InStatement, const ESQLitePreparedStatementFlags InFlags = ESQLitePreparedStatementFlags::None);
	~FSQLitePreparedStatement();

	/** Non-copyable */
	FSQLitePreparedStatement(const FSQLitePreparedStatement&) = delete;
	FSQLitePreparedStatement& operator=(const FSQLitePreparedStatement&) = delete;

	/** Movable */
	FSQLitePreparedStatement(FSQLitePreparedStatement&& Other);
	FSQLitePreparedStatement& operator=(FSQLitePreparedStatement&& Other);

	/**
	 * Is this a valid SQLite prepared statement? (ie, has been successfully compiled).
	 */
	bool IsValid() const;

	/**
	 * Is this SQLite prepared statement active? ("busy" in SQLite parlance).
	 * @return true for statements that have been partially stepped and haven't been reset.
	 */
	bool IsActive() const;

	/**
	 * Is this SQLite prepared statement read-only? (ie, will it only read from the database contents?).
	 */
	bool IsReadOnly() const;

	/**
	 * Create a new SQLite prepared statement.
	 */
	bool Create(FSQLiteDatabase& InDatabase, const TCHAR* InStatement, const ESQLitePreparedStatementFlags InFlags = ESQLitePreparedStatementFlags::None);

	/**
	 * Destroy the existing SQLite prepared statement.
	 */
	bool Destroy();

	/**
	 * Reset the SQLite prepared statement so that it can be used again.
	 * @note This doesn't remove any existing bindings (see ClearBindings).
	 */
	void Reset();

	/**
	 * Clear any bindings that have been applied to this prepared statement.
	 */
	void ClearBindings();

	/**
	 * Get the index of a given binding from its name.
	 * @return The binding index, or 0 if it could not be found.
	 */
	int32 GetBindingIndexByName(const TCHAR* InBindingName) const;

	/**
	 * Set the given integer binding from its name or index.
	 */
	bool SetBindingValueByName(const TCHAR* InBindingName, const int8 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const int8 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const uint8 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const uint8 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const int16 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const int16 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const uint16 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const uint16 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const int32 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const int32 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const uint32 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const uint32 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const int64 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const int64 InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const uint64 InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const uint64 InValue);

	/**
	 * Set the given float binding from its name or index.
	 */
	bool SetBindingValueByName(const TCHAR* InBindingName, const float InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const float InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const double InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const double InValue);

	/**
	 * Set the given string binding from its name or index.
	 */
	bool SetBindingValueByName(const TCHAR* InBindingName, const TCHAR* InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const TCHAR* InValue);
	bool SetBindingValueByName(const TCHAR* InBindingName, const FString& InValue);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const FString& InValue);

	/**
	 * Set the given blob binding from its name or index.
	 * @note If bCopy is set to false, then you must ensure the memory bound remains valid for the duration that the prepared statement is using it.
	 */
	bool SetBindingValueByName(const TCHAR* InBindingName, TArrayView<const uint8> InBlobData, const bool bCopy = true);
	bool SetBindingValueByIndex(const int32 InBindingIndex, TArrayView<const uint8> InBlobData, const bool bCopy = true);
	bool SetBindingValueByName(const TCHAR* InBindingName, const void* InBlobData, const int32 InBlobDataSizeBytes, const bool bCopy = true);
	bool SetBindingValueByIndex(const int32 InBindingIndex, const void* InBlobData, const int32 InBlobDataSizeBytes, const bool bCopy = true);

	/**
	 * Set the given null binding from its name or index.
	 */
	bool SetBindingValueByName(const TCHAR* InBindingName);
	bool SetBindingValueByIndex(const int32 InBindingIndex);

	/**
	 * Step the SQLite prepared statement to try and move on to the next result from the statement.
	 * @note See FSQLiteDatabase::Execute for a simple example of stepping a statement.
	 */
	ESQLitePreparedStatementStepResult Step();

	/**
	 * Get the index of a column from its name.
	 * @note It's better to look-up a column index once rather than look it up for each access request.
	 * @return The column index, or INDEX_NONE if it couldn't be found.
	 */
	int32 GetColumnIndexByName(const TCHAR* InColumnName) const;

	/**
	 * Get the integer value of a column from its name or index.
	 */
	bool GetColumnValueByName(const TCHAR* InColumnName, int8& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, int8& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, uint8& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, uint8& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, int16& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, int16& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, uint16& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, uint16& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, int32& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, int32& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, uint32& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, uint32& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, int64& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, int64& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, uint64& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, uint64& OutValue) const;

	/**
	 * Get the float value of a column from its name or index.
	 */
	bool GetColumnValueByName(const TCHAR* InColumnName, float& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, float& OutValue) const;
	bool GetColumnValueByName(const TCHAR* InColumnName, double& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, double& OutValue) const;

	/**
	 * Get the string value of a column from its name or index.
	 */
	bool GetColumnValueByName(const TCHAR* InColumnName, FString& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, FString& OutValue) const;

	/**
	 * Get the blob value of a column from its name or index.
	 */
	bool GetColumnValueByName(const TCHAR* InColumnName, TArray<uint8>& OutValue) const;
	bool GetColumnValueByIndex(const int32 InColumnIndex, TArray<uint8>& OutValue) const;

	/**
	 * Get the type of a column from its name or index.
	 * @note Column types in SQLite are somewhat arbitrary are not enforced, nor need to be consistent between the same column in different rows.
	 */
	bool GetColumnTypeByName(const TCHAR* InColumnName, ESQLiteColumnType& OutColumnType) const;
	bool GetColumnTypeByIndex(const int32 InColumnIndex, ESQLiteColumnType& OutColumnType) const;

	/**
	 * Get the column names affected by this statement.
	 */
	const TArray<FString>& GetColumnNames() const;

private:
	/** Attempt to cache the column names, if required and possible */
	void CacheColumnNames() const;

	/** Check whether the given column index is within the range of available columns */
	bool IsValidColumnIndex(const int32 InColumnIndex) const;

	/**
	 * Set the given integer binding from its name or index.
	 */
	template <typename T>
	bool SetBindingValueByName_Integer(const TCHAR* InBindingName, const T InValue);
	template <typename T>
	bool SetBindingValueByIndex_Integer(const int32 InBindingIndex, const T InValue);

	/**
	 * Set the given float binding from its name or index.
	 */
	template <typename T>
	bool SetBindingValueByName_Real(const TCHAR* InBindingName, const T InValue);
	template <typename T>
	bool SetBindingValueByIndex_Real(const int32 InBindingIndex, const T InValue);

	/**
	 * Get the integer value of a column from its name or index.
	 */
	template <typename T>
	bool GetColumnValueByName_Integer(const TCHAR* InColumnName, T& OutValue) const;
	template <typename T>
	bool GetColumnValueByIndex_Integer(const int32 InColumnIndex, T& OutValue) const;

	/**
	 * Get the float value of a column from its name or index.
	 */
	template <typename T>
	bool GetColumnValueByName_Real(const TCHAR* InColumnName, T& OutValue) const;
	template <typename T>
	bool GetColumnValueByIndex_Real(const int32 InColumnIndex, T& OutValue) const;

	/** Internal SQLite prepared statement handle */
	struct sqlite3_stmt* Statement;

	/** Cached array of column names (generated on-demand when needed by the API) */
	mutable TArray<FString> CachedColumnNames;
};

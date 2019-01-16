// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

/**
 * Optimized representation of a case-sensitive string, as used by localization keys.
 * This references an entry within a internal table to avoid memory duplication, as well as offering optimized comparison and hashing performance.
 */
class CORE_API FTextKey
{
public:
	FTextKey();
	FTextKey(const TCHAR* InStr);
	FTextKey(const FString& InStr);
	FTextKey(FString&& InStr);

	/** Get the underlying chars buffer this text key represents */
	FORCEINLINE const TCHAR* GetChars() const
	{
		return StrPtr;
	}

	/** Compare for equality */
	friend FORCEINLINE bool operator==(const FTextKey& A, const FTextKey& B)
	{
		return A.StrPtr == B.StrPtr;
	}

	/** Compare for inequality */
	friend FORCEINLINE bool operator!=(const FTextKey& A, const FTextKey& B)
	{
		return A.StrPtr != B.StrPtr;
	}

	/** Get the hash of this text key */
	friend FORCEINLINE uint32 GetTypeHash(const FTextKey& A)
	{
		return A.StrHash;
	}

	/** Serialize this text key */
	friend FArchive& operator<<(FArchive& Ar, FTextKey& A)
	{
		A.Serialize(Ar);
		return Ar;
	}

	/** Serialize this text key */
	void Serialize(FArchive& Ar);

	/** Serialize this text key as if it were an FString */
	void SerializeAsString(FArchive& Ar);

	/** Is this text key empty? */
	FORCEINLINE bool IsEmpty() const
	{
		return *StrPtr == 0;
	}

	/** Reset this text key to be empty */
	void Reset();

	/** Compact any slack within the internal table */
	static void CompactDataStructures();

private:
	/** Pointer to the string buffer we reference from the internal table */
	const TCHAR* StrPtr;

	/** Hash of this text key */
	uint32 StrHash;
};

/**
 * Optimized representation of a text identity (a namespace and key pair).
 */
class CORE_API FTextId
{
public:
	FTextId() = default;

	FTextId(const FTextKey& InNamespace, const FTextKey& InKey)
		: Namespace(InNamespace)
		, Key(InKey)
	{
	}

	/** Get the namespace component of this text identity */
	FORCEINLINE const FTextKey& GetNamespace() const
	{
		return Namespace;
	}

	/** Get the key component of this text identity */
	FORCEINLINE const FTextKey& GetKey() const
	{
		return Key;
	}

	/** Compare for equality */
	friend FORCEINLINE bool operator==(const FTextId& A, const FTextId& B)
	{
		return A.Namespace == B.Namespace && A.Key == B.Key;
	}

	/** Compare for inequality */
	friend FORCEINLINE bool operator!=(const FTextId& A, const FTextId& B)
	{
		return A.Namespace != B.Namespace || A.Key != B.Key;
	}

	/** Get the hash of this text identity */
	friend FORCEINLINE uint32 GetTypeHash(const FTextId& A)
	{
		return HashCombine(GetTypeHash(A.Namespace), GetTypeHash(A.Key));
	}

	/** Serialize this text identity */
	friend FArchive& operator<<(FArchive& Ar, FTextId& A)
	{
		Ar << A.Namespace;
		Ar << A.Key;
		return Ar;
	}

	/** Is this text identity empty? */
	FORCEINLINE bool IsEmpty() const
	{
		return Namespace.IsEmpty() && Key.IsEmpty();
	}

	/** Reset this text identity to be empty */
	FORCEINLINE void Reset()
	{
		Namespace.Reset();
		Key.Reset();
	}

private:
	/** Namespace component of this text identity */
	FTextKey Namespace;

	/** Key component of this text identity */
	FTextKey Key;
};

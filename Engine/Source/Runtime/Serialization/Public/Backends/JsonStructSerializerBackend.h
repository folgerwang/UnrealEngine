// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IStructSerializerBackend.h"
#include "Serialization/JsonWriter.h"

/**
 * Implements a writer for UStruct serialization using Json.
 *
 * Note: The underlying Json serializer is currently hard-coded to use UCS2CHAR and pretty-print.
 * This is because the current JsonWriter API does not allow writers to be substituted since it's
 * all based on templates. At some point we will refactor the low-level Json API to provide more
 * flexibility for serialization.
 */
class SERIALIZATION_API FJsonStructSerializerBackend
	: public IStructSerializerBackend
{
public:

	/**
	 * Creates and initializes a new legacy instance.
	 * @note Deprecated, use the two-parameter constructor with EStructSerializerBackendFlags::Legacy if you need backwards compatibility with code compiled prior to 4.22.
	 *
	 * @param InArchive The archive to serialize into.
	 */
	UE_DEPRECATED(4.22, "Use the two-parameter constructor with EStructSerializerBackendFlags::Legacy only if you need backwards compatibility with code compiled prior to 4.22; otherwise use EStructSerializerBackendFlags::Default.")
	FJsonStructSerializerBackend( FArchive& InArchive )
		: JsonWriter(TJsonWriter<UCS2CHAR>::Create(&InArchive))
		, Flags(EStructSerializerBackendFlags::Legacy)
	{ }

	/**
	 * Creates and initializes a new instance with the given flags.
	 *
	 * @param InArchive The archive to serialize into.
	 * @param InFlags The flags that control the serialization behavior (typically EStructSerializerBackendFlags::Default).
	 */
	FJsonStructSerializerBackend( FArchive& InArchive, const EStructSerializerBackendFlags InFlags )
		: JsonWriter(TJsonWriter<UCS2CHAR>::Create(&InArchive))
		, Flags(InFlags)
	{ }

public:

	// IStructSerializerBackend interface

	virtual void BeginArray(const FStructSerializerState& State) override;
	virtual void BeginStructure(const FStructSerializerState& State) override;
	virtual void EndArray(const FStructSerializerState& State) override;
	virtual void EndStructure(const FStructSerializerState& State) override;
	virtual void WriteComment(const FString& Comment) override;
	virtual void WriteProperty(const FStructSerializerState& State, int32 ArrayIndex = 0) override;

protected:

	// Allow access to the internal JsonWriter to subclasses
	TSharedRef<TJsonWriter<UCS2CHAR>>& GetWriter()
	{
		return JsonWriter;
	}

private:

	/** Holds the Json writer used for the actual serialization. */
	TSharedRef<TJsonWriter<UCS2CHAR>> JsonWriter;

	/** Flags controlling the serialization behavior. */
	EStructSerializerBackendFlags Flags;
};

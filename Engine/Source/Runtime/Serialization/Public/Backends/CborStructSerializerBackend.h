// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IStructSerializerBackend.h"
#include "CborWriter.h"

/**
 * Implements a writer for UStruct serialization using Cbor.

 */
class SERIALIZATION_API FCborStructSerializerBackend
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
		FCborStructSerializerBackend( FArchive& InArchive )
		: CborWriter(&InArchive)
		, Flags(EStructSerializerBackendFlags::Legacy)
	{ }

	/**
	 * Creates and initializes a new instance with the given flags.
	 *
	 * @param InArchive The archive to serialize into.
	 * @param InFlags The flags that control the serialization behavior (typically EStructSerializerBackendFlags::Default).
	 */
	FCborStructSerializerBackend( FArchive& InArchive, const EStructSerializerBackendFlags InFlags )
		: CborWriter(&InArchive)
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

private:
	/** Holds the Cbor writer used for the actual serialization. */
	FCborWriter CborWriter;

	/** Flags controlling the serialization behavior. */
	EStructSerializerBackendFlags Flags;
};

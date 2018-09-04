// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CborReader.h"
#include "IStructDeserializerBackend.h"

/**
 * Implements a reader for UStruct deserialization using Cbor.
 */
class SERIALIZATION_API FCborStructDeserializerBackend
	: public IStructDeserializerBackend
{
public:

	/**
	 * Creates and initializes a new instance.
	 * @param Archive The archive to deserialize from.
	 */
	FCborStructDeserializerBackend( FArchive& Archive )
		: CborReader(&Archive)
	{}

public:

	// IStructDeserializerBackend interface
	virtual const FString& GetCurrentPropertyName() const override;
	virtual FString GetDebugString() const override;
	virtual const FString& GetLastErrorMessage() const override;
	virtual bool GetNextToken(EStructDeserializerBackendTokens& OutToken) override;
	virtual bool ReadProperty(UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex) override;
	virtual void SkipArray() override;
	virtual void SkipStructure() override;

private:
	/** Holds the Cbor reader used for the actual reading of the archive. */
	FCborReader CborReader;

	/** Holds the last read Cbor Context. */
	FCborContext LastContext;

	/** Holds the last map key. */
	FString LastMapKey;
};

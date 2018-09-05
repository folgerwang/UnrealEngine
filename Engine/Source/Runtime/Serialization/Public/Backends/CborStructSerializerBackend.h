// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	 * Creates and initializes a new instance.
	 * @param Archive The archive to serialize into.
	 */
	FCborStructSerializerBackend( FArchive& Archive )
		: CborWriter(&Archive)
	{}

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
};

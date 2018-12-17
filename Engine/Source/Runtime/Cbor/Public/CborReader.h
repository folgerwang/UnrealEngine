// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CborTypes.h"

/**
 * Reader for a the cbor protocol encoded stream
 * @see http://cbor.io
 */
class CBOR_API FCborReader
{
public:
	FCborReader(FArchive* InStream);
	~FCborReader();

	/** @return the archive we are reading from. */
	const FArchive* GetArchive() const;

	/** @return true if the reader is in error. */
	bool IsError() const;

	/** @return A cbor Header containing an error code as its raw code. */
	FCborHeader GetError() const;

	/**
	 * The cbor context of the reader can either be
	 * a container context or a dummy.
	 * A reference to the context shouldn't be held while calling ReadNext.
	 * @return The current cbor context. */
	const FCborContext& GetContext() const;

	/**
	 * Read the next value from the cbor stream.
	 * @param OutContext the context to read the value into.
	 * @return true if successful, false if an error was returned or the end of the stream was reached.
	 */
	bool ReadNext(FCborContext& OutContext);

	/**
	 * Skip a container of ContainerType type
	 * @param ContainerType the container we expect to skip.
	 * @return true if successful, false if the current container wasn't a ContainerType or an error occurred.
	 */
	bool SkipContainer(ECborCode ContainerType);
	
private:
	/** Read a uint value from Ar into OutContext and also return it. */
	static uint64 ReadUIntValue(FCborContext& OutContext, FArchive& Ar);
	/** Read a Prim value from Ar into OutContext. */
	static void ReadPrimValue(FCborContext& OutContext, FArchive& Ar);

	/** Set an error in the reader and return it. */
	FCborHeader SetError(ECborCode ErrorCode);

	/** The archive we are reading from. */
	FArchive* Stream;
	/** Holds the context stack for the reader. */
	TArray<FCborContext> ContextStack;
};
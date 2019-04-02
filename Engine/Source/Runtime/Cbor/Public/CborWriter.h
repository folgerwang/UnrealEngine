// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CborTypes.h"

/**
* Writer for encoding a stream with the cbor protocol
* @see http://cbor.io
*/
class CBOR_API FCborWriter
{
public:
	FCborWriter(FArchive* InStream);
	~FCborWriter();

public:
	/** @return the archive we are writing to. */
	const FArchive* GetArchive() const;

	/**
	 * Write a container start code.
	 * @param ContainerType container major type, either array or map.
	 * @param NbItem the number of item in the container or negative to indicate indefinite containers.
	 */
	void WriteContainerStart(ECborCode ContainerType, int64 NbItem);

	/** Write a container break code, need a indefinite container context. */
	void WriteContainerEnd();

	/** Write a value. */

	void WriteNull();
	void WriteValue(uint64 Value);
	void WriteValue(int64 Value);
	void WriteValue(bool Value);
	void WriteValue(float Value);
	void WriteValue(double Value);
	void WriteValue(const FString& Value);
	void WriteValue(const char* CString, uint64 Length);

private:
	/** Write a uint Value for Header in Ar and return the final generated cbor Header. */
	static FCborHeader WriteUIntValue(FCborHeader Header, FArchive& Ar, uint64 Value);

	/** Validate the current writer context for MajorType. */
	void CheckContext(ECborCode MajorType);

	/** The archive being written to. */
	FArchive* Stream;
	/** The writer context stack. */
	TArray<FCborContext> ContextStack;
};


// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpRequestPayload.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformFilemanager.h"

bool FGenericPlatformHttp::IsURLEncoded(const TArray<uint8>& Payload)
{
	static char AllowedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
	static bool bTableFilled = false;
	static bool AllowedTable[256] = { false };

	if (!bTableFilled)
	{
		for (int32 Idx = 0; Idx < ARRAY_COUNT(AllowedChars) - 1; ++Idx)	// -1 to avoid trailing 0
		{
			uint8 AllowedCharIdx = static_cast<uint8>(AllowedChars[Idx]);
			check(AllowedCharIdx < ARRAY_COUNT(AllowedTable));
			AllowedTable[AllowedCharIdx] = true;
		}

		bTableFilled = true;
	}

	const int32 Num = Payload.Num();
	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		if (!AllowedTable[Payload[Idx]])
			return false;
	}

	return true;
}

FRequestPayloadInFileStream::FRequestPayloadInFileStream(TSharedRef<FArchive, ESPMode::ThreadSafe> InFile) : File(InFile)
{
}

FRequestPayloadInFileStream::~FRequestPayloadInFileStream()
{
}

int32 FRequestPayloadInFileStream::GetContentLength() const
{
	return static_cast<int32>(File->TotalSize());
}

const TArray<uint8>& FRequestPayloadInFileStream::GetContent() const
{
	ensureMsgf(false, TEXT("GetContent() on a streaming request payload is not allowed"));
	static const TArray<uint8> NotSupported;
	return NotSupported;
}

bool FRequestPayloadInFileStream::IsURLEncoded() const
{
	// Assume that files are not URL encoded, because they probably aren't.
	// This implies that POST requests with streamed files will need the caller to set a Content-Type.
	return false;
}

size_t FRequestPayloadInFileStream::FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent)
{
	size_t ContentLength = static_cast<size_t>(GetContentLength());
	check(SizeAlreadySent <= ContentLength);
	size_t SizeToSend = ContentLength - SizeAlreadySent;
	size_t SizeToSendThisTime = 0;
	SizeToSendThisTime = FMath::Min(SizeToSend, MaxOutputBufferSize);
	if (SizeToSendThisTime != 0)
	{
		if(File->Tell() != SizeAlreadySent)
		{
			File->Seek(SizeAlreadySent);
		}
		File->Serialize(OutputBuffer, static_cast<int64>(SizeToSendThisTime));
	}
	return SizeToSendThisTime;
}

FRequestPayloadInMemory::FRequestPayloadInMemory(const TArray<uint8>& Array) : Buffer(Array)
{
}

FRequestPayloadInMemory::~FRequestPayloadInMemory()
{
}

int32 FRequestPayloadInMemory::GetContentLength() const
{
	return Buffer.Num();
}

const TArray<uint8>& FRequestPayloadInMemory::GetContent() const
{
	return Buffer;
}

bool FRequestPayloadInMemory::IsURLEncoded() const
{
	return FGenericPlatformHttp::IsURLEncoded(Buffer);
}

size_t FRequestPayloadInMemory::FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent)
{
	size_t ContentLength = static_cast<size_t>(Buffer.Num());
	check(SizeAlreadySent <= ContentLength);
	size_t SizeToSend = ContentLength - SizeAlreadySent;
	size_t SizeToSendThisTime = 0;
	SizeToSendThisTime = FMath::Min(SizeToSend, MaxOutputBufferSize);
	if (SizeToSendThisTime != 0)
	{
		// static cast just ensures that this is uint8* in fact
		FMemory::Memcpy(OutputBuffer, static_cast<uint8*>(Buffer.GetData()) + SizeAlreadySent, SizeToSendThisTime);
	}
	return SizeToSendThisTime;
}

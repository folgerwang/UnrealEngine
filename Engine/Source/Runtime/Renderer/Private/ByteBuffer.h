// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

void MemsetBuffer(FRHICommandList& RHICmdList, const FRWBufferStructured& DstBuffer, const FVector4& Value, uint32 NumFloat4s, uint32 DstOffsetInFloat4s);
void MemcpyBuffer(FRHICommandList& RHICmdList, const FRWBufferStructured& SrcBuffer, const FRWBufferStructured& DstBuffer, uint32 NumFloat4s, uint32 SrcOffset = 0, uint32 DstOffset = 0);
bool ResizeBufferIfNeeded(FRHICommandList& RHICmdList, FRWBufferStructured& Buffer, uint32 NumFloat4s);

class FScatterUploadBuilder
{
public:

	FReadBuffer& ScatterBuffer;
	FReadBuffer& UploadBuffer;

	uint32* ScatterData;
	FVector4* UploadData;

	uint32 AllocatedNumScatters;
	uint32 NumScatters;
	uint32 StrideInFloat4s;

public:
	FScatterUploadBuilder(uint32 NumUploads, uint32 InStrideInFloat4s, FReadBuffer& InScatterBuffer, FReadBuffer& InUploadBuffer);

	void UploadTo(FRHICommandList& RHICmdList, FRWBufferStructured& DstBuffer);

	void UploadTo_Flush(FRHICommandList& RHICmdList, FRWBufferStructured& DstBuffer);

	void Add(uint32 Index, const FVector4* Data)
	{
		checkSlow(NumScatters < AllocatedNumScatters);
		checkSlow( ScatterData != nullptr );
		checkSlow( UploadData != nullptr );

		for (uint32 i = 0; i < StrideInFloat4s; i++)
		{
			ScatterData[i] = Index * StrideInFloat4s + i;
			UploadData[i] = Data[i];
		}

		ScatterData += StrideInFloat4s;
		UploadData += StrideInFloat4s;
		NumScatters += StrideInFloat4s;
	}
};
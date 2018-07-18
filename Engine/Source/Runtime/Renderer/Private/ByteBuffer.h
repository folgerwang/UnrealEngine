// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

// Must be aligned to 4 bytes
void MemcpyBuffer( FRHICommandList& RHICmdList, const FRWByteAddressBuffer& SrcBuffer, const FRWByteAddressBuffer& DstBuffer, uint32 NumBytes, uint32 SrcOffset = 0, uint32 DstOffset = 0 );
void ResizeBuffer( FRHICommandList& RHICmdList, FRWByteAddressBuffer& Buffer, uint32 NumBytes );


class FScatterUploadBuffer
{
public:
	FByteAddressBuffer	ScatterBuffer;
	FByteAddressBuffer	UploadBuffer;
	
	uint32*				ScatterData;
	uint32*				UploadData;

	uint32				NumScatters;
	uint32				CopyNum;
	uint32				CopySize;

public:
			FScatterUploadBuffer();

	void	Init( uint32 NumUploads, uint32 Stride );
	void	UploadTo( FRHICommandList& RHICmdList, FRWByteAddressBuffer& DstBuffer );

	void	Add( uint32 Index, const uint32* Data )
	{
		checkSlow( ScatterData != nullptr );
		checkSlow( UploadData != nullptr );

		for( uint32 i = 0; i < CopyNum; i++ )
		{
			ScatterData[i] = Index * CopyNum + i;
			for( uint32 c = 0; c < CopySize; c++ )
				UploadData[ i * CopySize + c ] = Data[ i * CopySize + c ];
		}

		ScatterData += CopyNum;
		UploadData += CopyNum * CopySize;
		NumScatters += CopyNum;
	}
};
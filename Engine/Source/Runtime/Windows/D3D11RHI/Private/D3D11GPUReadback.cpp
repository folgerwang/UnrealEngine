// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D11GPUReadback.cpp: Convenience function implementations for async GPU
memory updates and readbacks
=============================================================================*/

#include "D3D11RHIPrivate.h"

FStagingBufferRHIRef FD3D11DynamicRHI::RHICreateStagingBuffer()
{
	return new FD3D11StagingBuffer();
}



/*
FRHIGPUMemoryReadback *RHIScheduleGPUMemoryReadback(FRHICommandList& CmdList, FVertexBufferRHIRef GPUBuffer, FName RequestName)
{
	FRHIStagingBuffer *StagingBuffer = RHICreateStagingBuffer(); //new FD3D11StagingBuffer();
	FRHIGPUMemoryReadback *Readback = new FRHIGPUMemoryReadback(StagingBuffer, GPUBuffer, RequestName);
	return Readback;
}

FRHIGPUMemoryUpdate *RHIScheduleGPUMemoryUpdate(FRHICommandList& CmdList, FVertexBufferRHIRef GPUBuffer, FName RequestName)
{
	FRHIStagingBuffer *StagingBuffer = RHICreateStagingBuffer(); //new FD3D11StagingBuffer();
	FRHIGPUMemoryUpdate *Update = new FRHIGPUMemoryUpdate(StagingBuffer, GPUBuffer, RequestName);

	return Update;
}
*/

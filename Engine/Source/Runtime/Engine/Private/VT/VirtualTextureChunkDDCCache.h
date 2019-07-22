// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

class FVirtualTextureChunkDDCCache
{
public:
	void Initialize();
	void ShutDown();
	void UpdateRequests();

	bool MakeChunkAvailable(struct FVirtualTextureDataChunk* Chunk, FString& ChunkFileName, bool bAsync);

private:
	TArray<struct FVirtualTextureDataChunk*> ActiveChunks;
	FString AbsoluteCachePath;
};

FVirtualTextureChunkDDCCache* GetVirtualTextureChunkDDCCache();

#endif
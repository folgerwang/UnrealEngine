// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

class FVirtualTextureSpace;
class FUniquePageList;

class FVirtualTextureSystem
{
public:
			FVirtualTextureSystem();
			~FVirtualTextureSystem();

	void	Update( FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel );

	void	RegisterSpace( FVirtualTextureSpace* Space );
	void	UnregisterSpace( FVirtualTextureSpace* Space );

	FVirtualTextureSpace*	GetSpace(uint8 ID) { if (ID >= MaxSpaces) return nullptr; return Spaces[ID]; }

private:
	void	FeedbackAnalysis( FUniquePageList* RESTRICT RequestedPageList, const uint32* RESTRICT Buffer, uint32 Width, uint32 Height, uint32 Pitch );
	
	uint32	Frame;
	
	static const uint32 MaxSpaces = 16;
	FVirtualTextureSpace*	Spaces[MaxSpaces];

	bool bFlushCaches;
	void FlushCachesFromConsole();
	FAutoConsoleCommand FlushCachesCommand;

	void DumpFromConsole();
	FAutoConsoleCommand DumpCommand;
};

FVirtualTextureSystem *GetVirtualTextureSystem();
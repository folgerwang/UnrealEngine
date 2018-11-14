// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Templates/Tuple.h"
#include "Core/BlockRange.h"

namespace BuildPatchServices
{
	struct FChunkMatch;
	class FBlockStructure;

	class IChunkMatchProcessor
	{
	public:
		virtual ~IChunkMatchProcessor() {}
		virtual void ProcessMatch(const int32 Layer, const FChunkMatch& Match, FBlockStructure BuildSpace) = 0;
		virtual void FlushLayer(const int32 Layer, const uint64 UpToByteOffset) = 0;
		virtual FBlockRange CollectLayer(const int32 Layer, TArray<TTuple<FChunkMatch, FBlockStructure>>& OutData) = 0;
	};

	class FChunkMatchProcessorFactory
	{
	public:
		static IChunkMatchProcessor* Create();
	};
}

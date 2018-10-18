// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Rendering/ShaderPipelineStateCache.h"
#include "ShaderPipelineCache.h"

void FShaderPipelineStateCache::Initialize(EShaderPlatform Platform)
{
	FShaderPipelineCache::Initialize(Platform);
}

void FShaderPipelineStateCache::Shutdown()
{
	FShaderPipelineCache::Shutdown();
}

FShaderPipelineStateCache::FShaderPipelineStateCache(EShaderPlatform Platform)
	: FShaderPipelineCache(Platform)
	, FTickableObjectRenderThread(true, false) // (RegisterNow, HighFrequency)
{
}

bool FShaderPipelineStateCache::IsTickable() const
{
	return FShaderPipelineCache::IsTickable();
}

void FShaderPipelineStateCache::Tick(float DeltaTime)
{
	return FShaderPipelineCache::Tick(DeltaTime);
}

bool FShaderPipelineStateCache::NeedsRenderingResumedForRenderingThreadTick() const
{
	return FShaderPipelineCache::NeedsRenderingResumedForRenderingThreadTick();
}

TStatId FShaderPipelineStateCache::GetStatId() const
{
	return FShaderPipelineCache::GetStatId();
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "TickableObjectRenderThread.h"
#include "ShaderPipelineCache.h"

/**
 * Wrapper class to decouple FShaderPipelineCache from RenderCore.
 */
class ENGINE_API FShaderPipelineStateCache : public FShaderPipelineCache, public FTickableObjectRenderThread
{
public:
	/**
	 * Initializes the shader pipeline cache for the desired platform, called by the engine.
	 * The shader platform is used only to load the initial pipeline cache and can be changed by closing & reopening the cache if necessary.
	 */
	static void Initialize(EShaderPlatform Platform);

	/** Terminates the shader pipeline cache, called by the engine. */
	static void Shutdown();

	FShaderPipelineStateCache(EShaderPlatform Platform);

	virtual ~FShaderPipelineStateCache()
	{
	}

	virtual bool IsTickable() const override final;

	virtual void Tick( float DeltaTime ) override final;

	virtual bool NeedsRenderingResumedForRenderingThreadTick() const override final;

	virtual TStatId GetStatId() const override final;
};

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RendererInterface.h"

class RENDERER_API FRenderTargetWriteMask
{
public:
	static void Decode(FRHICommandListImmediate& RHICmdList, TShaderMap<FGlobalShaderType>* ShaderMap, const TArray<TRefCountPtr<IPooledRenderTarget> >& InRenderTargets, TRefCountPtr<IPooledRenderTarget>& OutRTWriteMask, uint32 RTWriteMaskFastVRamConfig, const TCHAR* RTWriteMaskDebugName);
};
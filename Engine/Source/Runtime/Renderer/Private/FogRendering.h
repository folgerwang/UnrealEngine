// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FogRendering.h: 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "SceneRendering.h"
#include "VolumetricFog.h"

BEGIN_UNIFORM_BUFFER_STRUCT(FFogUniformParameters,)
	UNIFORM_MEMBER(FVector4, ExponentialFogParameters)
	UNIFORM_MEMBER(FVector4, ExponentialFogColorParameter)
	UNIFORM_MEMBER(FVector4, ExponentialFogParameters3)
	UNIFORM_MEMBER(FVector2D, SinCosInscatteringColorCubemapRotation)
	UNIFORM_MEMBER(FVector, FogInscatteringTextureParameters)
	UNIFORM_MEMBER(FVector4, InscatteringLightDirection)
	UNIFORM_MEMBER(FVector4, DirectionalInscatteringColor)
	UNIFORM_MEMBER(float, DirectionalInscatteringStartDistance)
	UNIFORM_MEMBER(float, ApplyVolumetricFog)
	UNIFORM_MEMBER_TEXTURE(TextureCube, FogInscatteringColorCubemap)
	UNIFORM_MEMBER_SAMPLER(SamplerState, FogInscatteringColorSampler)
	UNIFORM_MEMBER_TEXTURE(Texture3D, IntegratedLightScattering)
	UNIFORM_MEMBER_SAMPLER(SamplerState, IntegratedLightScatteringSampler)
END_UNIFORM_BUFFER_STRUCT(FFogUniformParameters)

extern void SetupFogUniformParameters(const class FViewInfo& View, FFogUniformParameters& OutParameters);

extern bool ShouldRenderFog(const FSceneViewFamily& Family);

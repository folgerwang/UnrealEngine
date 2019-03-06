// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "ShaderParameterMacros.h"
#include "SceneRendering.h"
#include "LightSceneInfo.h"

#if RHI_RAYTRACING

const static uint32 GRaytracingLightCountMaximum = 64;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRaytracingLightData, )
	SHADER_PARAMETER(uint32, Count)
	SHADER_PARAMETER_ARRAY(uint32, Type, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, LightPosition, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, LightInvRadius, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, LightColor, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, LightFalloffExponent, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, Direction, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, Tangent, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector2D, SpotAngles, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, SpecularScale, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, SourceRadius, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, SourceLength, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, SoftSourceRadius, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(uint32, LightProfileIndex, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector2D, DistanceFadeMAD, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(uint32, RectLightTextureIndex, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, RectLightBarnCosAngle, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, RectLightBarnLength, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_TEXTURE(Texture2D, LTCMatTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, LTCMatSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, LTCAmpTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, LTCAmpSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture0)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture1)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture2)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture3)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture4)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture5)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture6)
	SHADER_PARAMETER_TEXTURE(Texture2D, RectLightTexture7)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

void SetupRaytracingLightData(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FRaytracingLightData* LightData);

TUniformBufferRef<FRaytracingLightData> CreateLightDataUniformBuffer(const TSparseArray<FLightSceneInfoCompact>& Lights, const class FViewInfo& View, EUniformBufferUsage Usage);

#endif
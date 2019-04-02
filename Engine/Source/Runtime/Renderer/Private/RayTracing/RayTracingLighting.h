// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "ShaderParameterMacros.h"
#include "SceneRendering.h"
#include "LightSceneInfo.h"

#if RHI_RAYTRACING

const static uint32 GRaytracingLightCountMaximum = 64;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRaytracingLightDataPacked, )
	SHADER_PARAMETER(uint32, Count)
	SHADER_PARAMETER_ARRAY(FIntVector, Type_LightProfileIndex_RectLightTextureIndex, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector4, LightPosition_InvRadius, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector4, LightColor_SpecularScale, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector4, Direction_FalloffExponent, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector4, Tangent_SourceRadius, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector4, SpotAngles_SourceLength_SoftSourceRadius, [GRaytracingLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector4, DistanceFadeMAD_RectLightBarnCosAngle_RectLightBarnLength, [GRaytracingLightCountMaximum])
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

void SetupRaytracingLightDataPacked(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FRaytracingLightDataPacked* LightData);

TUniformBufferRef<FRaytracingLightDataPacked> CreateLightDataPackedUniformBuffer(const TSparseArray<FLightSceneInfoCompact>& Lights, const class FViewInfo& View, EUniformBufferUsage Usage);

#endif
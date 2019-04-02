// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFogShared.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "SceneView.h"
#include "SceneRendering.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FVoxelizeVolumePassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FSceneTexturesUniformParameters, SceneTextures)
	SHADER_PARAMETER(FMatrix, ViewToVolumeClip)
	SHADER_PARAMETER(FVector4, FrameJitterOffset0)
	SHADER_PARAMETER_STRUCT(FVolumetricFogGlobalData, VolumetricFog)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern FVector VolumetricFogTemporalRandom(uint32 FrameNumber);

struct FVolumetricFogIntegrationParameterData
{
	FVolumetricFogIntegrationParameterData() :
		bTemporalHistoryIsValid(false)
	{}

	bool bTemporalHistoryIsValid;
	TArray<FVector4, TInlineAllocator<16>> FrameJitterOffsetValues;
	const FRDGTexture* VBufferA;
	const FRDGTexture* VBufferB;
	const FRDGTextureUAV* VBufferA_UAV;
	const FRDGTextureUAV* VBufferB_UAV;

	const FRDGTexture* LightScattering;
	const FRDGTextureUAV* LightScatteringUAV;
};

/**  */
class FVolumetricFogIntegrationParameters
{
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		VBufferA.Bind(ParameterMap, TEXT("VBufferA"));
		VBufferB.Bind(ParameterMap, TEXT("VBufferB"));
		LightScattering.Bind(ParameterMap, TEXT("LightScattering"));
		IntegratedLightScattering.Bind(ParameterMap, TEXT("IntegratedLightScattering"));
		IntegratedLightScatteringSampler.Bind(ParameterMap, TEXT("IntegratedLightScatteringSampler"));
		VolumetricFogData.Bind(ParameterMap, TEXT("VolumetricFog"));
		UnjitteredClipToTranslatedWorld.Bind(ParameterMap, TEXT("UnjitteredClipToTranslatedWorld")); 
		UnjitteredPrevWorldToClip.Bind(ParameterMap, TEXT("UnjitteredPrevWorldToClip")); 
		FrameJitterOffsets.Bind(ParameterMap, TEXT("FrameJitterOffsets")); 
		HistoryWeight.Bind(ParameterMap, TEXT("HistoryWeight")); 
		HistoryMissSuperSampleCount.Bind(ParameterMap, TEXT("HistoryMissSuperSampleCount")); 
	}

	template<typename ShaderRHIParamRef>
	void Set(
		FRHICommandList& RHICmdList, 
		const ShaderRHIParamRef& ShaderRHI, 
		const FViewInfo& View, 
		const FVolumetricFogIntegrationParameterData& IntegrationData) const
	{
		SetSamplerParameter(RHICmdList, ShaderRHI, IntegratedLightScatteringSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		
		if (VolumetricFogData.IsBound())
		{
			SetUniformBufferParameter(RHICmdList, ShaderRHI, VolumetricFogData, View.VolumetricFogResources.VolumetricFogGlobalData);
		}

		if (UnjitteredClipToTranslatedWorld.IsBound())
		{
			FMatrix UnjitteredInvTranslatedViewProjectionMatrix = View.ViewMatrices.ComputeInvProjectionNoAAMatrix() * View.ViewMatrices.GetTranslatedViewMatrix().GetTransposed();
			SetShaderValue(RHICmdList, ShaderRHI, UnjitteredClipToTranslatedWorld, UnjitteredInvTranslatedViewProjectionMatrix);
		}

		if (UnjitteredPrevWorldToClip.IsBound())
		{
			FMatrix UnjitteredViewProjectionMatrix = View.PrevViewInfo.ViewMatrices.GetViewMatrix() * View.PrevViewInfo.ViewMatrices.ComputeProjectionNoAAMatrix();
			SetShaderValue(RHICmdList, ShaderRHI, UnjitteredPrevWorldToClip, UnjitteredViewProjectionMatrix);
		}

		if (FrameJitterOffsets.IsBound())
		{
			SetShaderValueArray(RHICmdList, ShaderRHI, FrameJitterOffsets, IntegrationData.FrameJitterOffsetValues.GetData(), IntegrationData.FrameJitterOffsetValues.Num());
		}

		extern float GVolumetricFogHistoryWeight;
		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, IntegrationData.bTemporalHistoryIsValid ? GVolumetricFogHistoryWeight : 0.0f);

		extern int32 GVolumetricFogHistoryMissSupersampleCount;
		SetShaderValue(RHICmdList, ShaderRHI, HistoryMissSuperSampleCount, FMath::Clamp(GVolumetricFogHistoryMissSupersampleCount, 1, 16));
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FVolumetricFogIntegrationParameters& P)
	{
		Ar << P.VBufferA;
		Ar << P.VBufferB;
		Ar << P.LightScattering;
		Ar << P.IntegratedLightScattering;
		Ar << P.IntegratedLightScatteringSampler;
		Ar << P.VolumetricFogData;
		Ar << P.UnjitteredClipToTranslatedWorld;
		Ar << P.UnjitteredPrevWorldToClip;
		Ar << P.FrameJitterOffsets;
		Ar << P.HistoryWeight;
		Ar << P.HistoryMissSuperSampleCount;
		return Ar;
	}

private:

	FRWShaderParameter VBufferA;
	FRWShaderParameter VBufferB;
	FRWShaderParameter LightScattering;
	FRWShaderParameter IntegratedLightScattering;
	FShaderResourceParameter IntegratedLightScatteringSampler;
	FShaderUniformBufferParameter VolumetricFogData;
	FShaderParameter UnjitteredClipToTranslatedWorld;
	FShaderParameter UnjitteredPrevWorldToClip;
	FShaderParameter FrameJitterOffsets;
	FShaderParameter HistoryWeight;
	FShaderParameter HistoryMissSuperSampleCount;
};

inline int32 ComputeZSliceFromDepth(float SceneDepth, FVector GridZParams)
{
	return FMath::TruncToInt(FMath::Log2(SceneDepth * GridZParams.X + GridZParams.Y) * GridZParams.Z);
}

extern FVector GetVolumetricFogGridZParams(float NearPlane, float FarPlane, int32 GridSizeZ);

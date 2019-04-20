// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PrimitiveUniformShaderParameters.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveSceneInfo.h"

void FSinglePrimitiveStructuredBuffer::InitRHI() 
{
	if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5))
	{
		FRHIResourceCreateInfo CreateInfo;
		PrimitiveSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4), FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s * sizeof(FVector4), BUF_Static | BUF_ShaderResource, CreateInfo);

		void* LockedData = RHILockStructuredBuffer(PrimitiveSceneDataBufferRHI, 0, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s * sizeof(FVector4), RLM_WriteOnly);

		FPlatformMemory::Memcpy(LockedData, PrimitiveSceneData.Data, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s * sizeof(FVector4));

		RHIUnlockStructuredBuffer(PrimitiveSceneDataBufferRHI);

		PrimitiveSceneDataBufferSRV = RHICreateShaderResourceView(PrimitiveSceneDataBufferRHI);
	}

	if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5))
	{
		FRHIResourceCreateInfo CreateInfo;
		LightmapSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4), FLightmapSceneShaderData::LightmapDataStrideInFloat4s * sizeof(FVector4), BUF_Static | BUF_ShaderResource, CreateInfo);

		void* LockedData = RHILockStructuredBuffer(LightmapSceneDataBufferRHI, 0, FLightmapSceneShaderData::LightmapDataStrideInFloat4s * sizeof(FVector4), RLM_WriteOnly);

		FPlatformMemory::Memcpy(LockedData, LightmapSceneData.Data, FLightmapSceneShaderData::LightmapDataStrideInFloat4s * sizeof(FVector4));

		RHIUnlockStructuredBuffer(LightmapSceneDataBufferRHI);

		LightmapSceneDataBufferSRV = RHICreateShaderResourceView(LightmapSceneDataBufferRHI);
	}
}

TGlobalResource<FSinglePrimitiveStructuredBuffer> GIdentityPrimitiveBuffer;

FPrimitiveSceneShaderData::FPrimitiveSceneShaderData(const FPrimitiveSceneProxy* RESTRICT Proxy)
{
	bool bHasPrecomputedVolumetricLightmap;
	FMatrix PreviousLocalToWorld;
	int32 SingleCaptureIndex;

	Proxy->GetScene().GetPrimitiveUniformShaderParameters_RenderThread(Proxy->GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex);

	Setup(GetPrimitiveUniformShaderParameters(
		Proxy->GetLocalToWorld(),
		PreviousLocalToWorld,
		Proxy->GetActorPosition(), 
		Proxy->GetBounds(), 
		Proxy->GetLocalBounds(), 
		Proxy->ReceivesDecals(), 
		Proxy->HasDistanceFieldRepresentation(), 
		Proxy->HasDynamicIndirectShadowCasterRepresentation(), 
		Proxy->UseSingleSampleShadowFromStationaryLights(),
		bHasPrecomputedVolumetricLightmap,
		Proxy->UseEditorDepthTest(), 
		Proxy->GetLightingChannelMask(),
		Proxy->GetLpvBiasMultiplier(),
		Proxy->GetPrimitiveSceneInfo()->GetLightmapDataOffset(),
		SingleCaptureIndex));
}

void FPrimitiveSceneShaderData::Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
{
	static_assert(sizeof(FPrimitiveUniformShaderParameters) == sizeof(FPrimitiveSceneShaderData), "The FPrimitiveSceneShaderData manual layout below and in usf must match FPrimitiveUniformShaderParameters.  Update this assert when adding a new member.");
	
	// Note: layout must match GetPrimitiveData in usf
	Data[0] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[0][0];
	Data[1] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[1][0];
	Data[2] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[2][0];
	Data[3] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[3][0];

	Data[4] = PrimitiveUniformShaderParameters.InvNonUniformScaleAndDeterminantSign;
	Data[5] = PrimitiveUniformShaderParameters.ObjectWorldPositionAndRadius;

	Data[6] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[0][0];
	Data[7] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[1][0];
	Data[8] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[2][0];
	Data[9] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[3][0];
	Data[10] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[0][0];
	Data[11] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[1][0];
	Data[12] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[2][0];
	Data[13] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[3][0];
	Data[14] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[0][0];
	Data[15] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[1][0];
	Data[16] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[2][0];
	Data[17] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[3][0];

	Data[18] = FVector4(PrimitiveUniformShaderParameters.ActorWorldPosition, PrimitiveUniformShaderParameters.UseSingleSampleShadowFromStationaryLights);
	Data[19] = FVector4(PrimitiveUniformShaderParameters.ObjectBounds, PrimitiveUniformShaderParameters.LpvBiasMultiplier);

	Data[20] = FVector4(
		PrimitiveUniformShaderParameters.DecalReceiverMask, 
		PrimitiveUniformShaderParameters.PerObjectGBufferData, 
		PrimitiveUniformShaderParameters.UseVolumetricLightmapShadowFromStationaryLights, 
		PrimitiveUniformShaderParameters.UseEditorDepthTest);
	Data[21] = PrimitiveUniformShaderParameters.ObjectOrientation;
	Data[22] = PrimitiveUniformShaderParameters.NonUniformScale;

	// Set W directly in order to bypass NaN check, when passing int through FVector to shader.
	Data[23] = FVector4(PrimitiveUniformShaderParameters.LocalObjectBoundsMin, 0.0f);
	Data[23].W = *(const float*)&PrimitiveUniformShaderParameters.LightingChannelMask;

	Data[24] = FVector4(PrimitiveUniformShaderParameters.LocalObjectBoundsMax, 0.0f);
	Data[24].W = *(const float*)&PrimitiveUniformShaderParameters.LightmapDataIndex;

	Data[25] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	Data[25].X = *(const float*)&PrimitiveUniformShaderParameters.SingleCaptureIndex;
}
// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "LightmapUniformShaderParameters.h"

/** 
 * The uniform shader parameters associated with a primitive. 
 * Note: Must match FPrimitiveSceneData in shaders.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FPrimitiveUniformShaderParameters,ENGINE_API)
	SHADER_PARAMETER(FMatrix,LocalToWorld)		// always needed
	SHADER_PARAMETER_EX(FVector4,InvNonUniformScaleAndDeterminantSign,EShaderPrecisionModifier::Half) //often needed
	SHADER_PARAMETER(FVector4,ObjectWorldPositionAndRadius)	// needed by some materials
	SHADER_PARAMETER(FMatrix,WorldToLocal)		// rarely needed
	SHADER_PARAMETER(FMatrix,PreviousLocalToWorld)	// Used to calculate velocity
	SHADER_PARAMETER(FMatrix,PreviousWorldToLocal)	// rarely used when calculating velocity, if material uses vertex offset along with world->local transform
	SHADER_PARAMETER(FVector,ActorWorldPosition)
	SHADER_PARAMETER_EX(float,UseSingleSampleShadowFromStationaryLights,EShaderPrecisionModifier::Half)	
	SHADER_PARAMETER(FVector,ObjectBounds)		// only needed for editor/development
	SHADER_PARAMETER(float,LpvBiasMultiplier)
	SHADER_PARAMETER_EX(float,DecalReceiverMask,EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX(float,PerObjectGBufferData,EShaderPrecisionModifier::Half)		// 0..1, 2 bits, bDistanceFieldRepresentation, bHeightfieldRepresentation
	SHADER_PARAMETER_EX(float,UseVolumetricLightmapShadowFromStationaryLights,EShaderPrecisionModifier::Half)		
	SHADER_PARAMETER_EX(float,UseEditorDepthTest,EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX(FVector4,ObjectOrientation,EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX(FVector4,NonUniformScale,EShaderPrecisionModifier::Half)
	SHADER_PARAMETER(FVector, LocalObjectBoundsMin)		// This is used in a custom material function (ObjectLocalBounds.uasset)
	SHADER_PARAMETER(FVector, LocalObjectBoundsMax)		// This is used in a custom material function (ObjectLocalBounds.uasset)
	SHADER_PARAMETER(uint32,LightingChannelMask)
	SHADER_PARAMETER(uint32,LightmapDataIndex)
	SHADER_PARAMETER(int32, SingleCaptureIndex)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

/** Initializes the primitive uniform shader parameters. */
inline FPrimitiveUniformShaderParameters GetPrimitiveUniformShaderParameters(
	const FMatrix& LocalToWorld,
	const FMatrix& PreviousLocalToWorld,
	FVector ActorPosition,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	bool bReceivesDecals,
	bool bHasDistanceFieldRepresentation,
	bool bHasCapsuleRepresentation,
	bool bUseSingleSampleShadowFromStationaryLights,
	bool bUseVolumetricLightmap,
	bool bUseEditorDepthTest,
	uint32 LightingChannelMask,
	float LpvBiasMultiplier,
	uint32 LightmapDataIndex,
	int32 SingleCaptureIndex
)
{
	FPrimitiveUniformShaderParameters Result;
	Result.LocalToWorld = LocalToWorld;
	Result.WorldToLocal = LocalToWorld.Inverse();
	Result.PreviousLocalToWorld = PreviousLocalToWorld;
	Result.PreviousWorldToLocal = PreviousLocalToWorld.Inverse();
	Result.ObjectWorldPositionAndRadius = FVector4(WorldBounds.Origin, WorldBounds.SphereRadius);
	Result.ObjectBounds = WorldBounds.BoxExtent;
	Result.LocalObjectBoundsMin = LocalBounds.GetBoxExtrema(0); // 0 == minimum
	Result.LocalObjectBoundsMax = LocalBounds.GetBoxExtrema(1); // 1 == maximum
	Result.ObjectOrientation = LocalToWorld.GetUnitAxis( EAxis::Z );
	Result.ActorWorldPosition = ActorPosition;
	Result.LightingChannelMask = LightingChannelMask;
	Result.LpvBiasMultiplier = LpvBiasMultiplier;

	{
		// Extract per axis scales from LocalToWorld transform
		FVector4 WorldX = FVector4(LocalToWorld.M[0][0],LocalToWorld.M[0][1],LocalToWorld.M[0][2],0);
		FVector4 WorldY = FVector4(LocalToWorld.M[1][0],LocalToWorld.M[1][1],LocalToWorld.M[1][2],0);
		FVector4 WorldZ = FVector4(LocalToWorld.M[2][0],LocalToWorld.M[2][1],LocalToWorld.M[2][2],0);
		float ScaleX = FVector(WorldX).Size();
		float ScaleY = FVector(WorldY).Size();
		float ScaleZ = FVector(WorldZ).Size();
		Result.NonUniformScale = FVector4(ScaleX,ScaleY,ScaleZ,0);
		Result.InvNonUniformScaleAndDeterminantSign = FVector4(
			ScaleX > KINDA_SMALL_NUMBER ? 1.0f/ScaleX : 0.0f,
			ScaleY > KINDA_SMALL_NUMBER ? 1.0f/ScaleY : 0.0f,
			ScaleZ > KINDA_SMALL_NUMBER ? 1.0f/ScaleZ : 0.0f,
			FMath::FloatSelect(LocalToWorld.RotDeterminant(),1,-1)
			);
	}
	Result.DecalReceiverMask = bReceivesDecals ? 1 : 0;
	Result.PerObjectGBufferData = (2 * (int32)bHasCapsuleRepresentation + (int32)bHasDistanceFieldRepresentation) / 3.0f;
	Result.UseSingleSampleShadowFromStationaryLights = bUseSingleSampleShadowFromStationaryLights ? 1.0f : 0.0f;
	Result.UseVolumetricLightmapShadowFromStationaryLights = bUseVolumetricLightmap && bUseSingleSampleShadowFromStationaryLights ? 1.0f : 0.0f;
	Result.UseEditorDepthTest = bUseEditorDepthTest ? 1 : 0;
	Result.LightmapDataIndex = LightmapDataIndex;
	Result.SingleCaptureIndex = SingleCaptureIndex;
	return Result;
}

inline TUniformBufferRef<FPrimitiveUniformShaderParameters> CreatePrimitiveUniformBufferImmediate(
	const FMatrix& LocalToWorld,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	bool bReceivesDecals,
	bool bUseEditorDepthTest,
	float LpvBiasMultiplier = 1.0f
	)
{
	check(IsInRenderingThread());
	return TUniformBufferRef<FPrimitiveUniformShaderParameters>::CreateUniformBufferImmediate(
		GetPrimitiveUniformShaderParameters(LocalToWorld, LocalToWorld, WorldBounds.Origin, WorldBounds, LocalBounds, bReceivesDecals, false, false, false, false, bUseEditorDepthTest, GetDefaultLightingChannelMask(), LpvBiasMultiplier, INDEX_NONE, INDEX_NONE),
		UniformBuffer_MultiFrame
		);
}

inline FPrimitiveUniformShaderParameters GetIdentityPrimitiveParameters()
{
	//don't use FMatrix::Identity here as GetIdentityPrimitiveParameters is used by TGlobalResource<FIdentityPrimitiveUniformBuffer> and because static initialization order is undefined
	//FMatrix::Identiy might be all 0's or random data the first time this is called.
	return GetPrimitiveUniformShaderParameters(
		FMatrix(FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 1)),
		FMatrix(FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 1)),
		FVector(0.0f, 0.0f, 0.0f),
		FBoxSphereBounds(EForceInit::ForceInit),
		FBoxSphereBounds(EForceInit::ForceInit),
		true,
		false,
		false,
		false,
		false,
		true,
		GetDefaultLightingChannelMask(),
		1.0f,		// LPV bias
		INDEX_NONE,
		INDEX_NONE
	);
}

/**
 * Primitive uniform buffer containing only identity transforms.
 */
class FIdentityPrimitiveUniformBuffer : public TUniformBuffer<FPrimitiveUniformShaderParameters>
{
public:

	/** Default constructor. */
	FIdentityPrimitiveUniformBuffer()
	{
		SetContents(GetIdentityPrimitiveParameters());
	}
};

/** Global primitive uniform buffer resource containing identity transformations. */
extern ENGINE_API TGlobalResource<FIdentityPrimitiveUniformBuffer> GIdentityPrimitiveUniformBuffer;

struct FPrimitiveSceneShaderData
{
	// Must match usf
	enum { PrimitiveDataStrideInFloat4s = 26 };

	FVector4 Data[PrimitiveDataStrideInFloat4s];

	FPrimitiveSceneShaderData()
	{
		Setup(GetIdentityPrimitiveParameters());
	}

	explicit FPrimitiveSceneShaderData(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
	{
		Setup(PrimitiveUniformShaderParameters);
	}

	ENGINE_API FPrimitiveSceneShaderData(const class FPrimitiveSceneProxy* RESTRICT Proxy);

	ENGINE_API void Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters);
};

class FSinglePrimitiveStructuredBuffer : public FRenderResource
{
public:

	ENGINE_API virtual void InitRHI() override;

	virtual void ReleaseRHI() override
	{
		PrimitiveSceneDataBufferRHI.SafeRelease();
		PrimitiveSceneDataBufferSRV.SafeRelease();
		LightmapSceneDataBufferRHI.SafeRelease();
		LightmapSceneDataBufferSRV.SafeRelease();
	}

	FPrimitiveSceneShaderData PrimitiveSceneData;
	FLightmapSceneShaderData LightmapSceneData;

	FStructuredBufferRHIRef PrimitiveSceneDataBufferRHI;
	FShaderResourceViewRHIRef PrimitiveSceneDataBufferSRV;

	FStructuredBufferRHIRef LightmapSceneDataBufferRHI;
	FShaderResourceViewRHIRef LightmapSceneDataBufferSRV;
};

/**
* Default Primitive data buffer.  
* This is used when the VF is used for rendering outside normal mesh passes, where there is no valid scene.
*/
extern ENGINE_API TGlobalResource<FSinglePrimitiveStructuredBuffer> GIdentityPrimitiveBuffer;

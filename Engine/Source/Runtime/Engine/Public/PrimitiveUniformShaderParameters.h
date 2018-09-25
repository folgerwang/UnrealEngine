// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"


/** The uniform shader parameters associated with a primitive. */
BEGIN_UNIFORM_BUFFER_STRUCT(FPrimitiveUniformShaderParameters,ENGINE_API)
	UNIFORM_MEMBER(FMatrix,LocalToWorld)		// always needed
	UNIFORM_MEMBER_EX(FVector4,InvNonUniformScaleAndDeterminantSign,EShaderPrecisionModifier::Half) //often needed
	UNIFORM_MEMBER(FVector4,ObjectWorldPositionAndRadius)	// needed by some materials
	UNIFORM_MEMBER(FMatrix,WorldToLocal)		// rarely needed
	UNIFORM_MEMBER(FVector,ActorWorldPosition)
	UNIFORM_MEMBER_EX(float,UseSingleSampleShadowFromStationaryLights,EShaderPrecisionModifier::Half)	
	UNIFORM_MEMBER(FVector,ObjectBounds)		// only needed for editor/development
	UNIFORM_MEMBER(float,LpvBiasMultiplier)
	UNIFORM_MEMBER_EX(float,DecalReceiverMask,EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX(float,PerObjectGBufferData,EShaderPrecisionModifier::Half)		// 0..1, 2 bits, bDistanceFieldRepresentation, bHeightfieldRepresentation
	UNIFORM_MEMBER_EX(float,UseVolumetricLightmapShadowFromStationaryLights,EShaderPrecisionModifier::Half)		
	UNIFORM_MEMBER_EX(float,UseEditorDepthTest,EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX(FVector4,ObjectOrientation,EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX(FVector4,NonUniformScale,EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER(FVector, LocalObjectBoundsMin)		// This is used in a custom material function (ObjectLocalBounds.uasset)
	UNIFORM_MEMBER(FVector, LocalObjectBoundsMax)		// This is used in a custom material function (ObjectLocalBounds.uasset)
	UNIFORM_MEMBER(uint32,LightingChannelMask)
END_UNIFORM_BUFFER_STRUCT(FPrimitiveUniformShaderParameters)

/** Initializes the primitive uniform shader parameters. */
inline FPrimitiveUniformShaderParameters GetPrimitiveUniformShaderParameters(
	const FMatrix& LocalToWorld,
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
	float LpvBiasMultiplier = 1.0f
)
{
	FPrimitiveUniformShaderParameters Result;
	Result.LocalToWorld = LocalToWorld;
	Result.WorldToLocal = LocalToWorld.Inverse();
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
		GetPrimitiveUniformShaderParameters(LocalToWorld, WorldBounds.Origin, WorldBounds, LocalBounds, bReceivesDecals, false, false, false, false, bUseEditorDepthTest, GetDefaultLightingChannelMask(), LpvBiasMultiplier ),
		UniformBuffer_MultiFrame
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
		SetContents(GetPrimitiveUniformShaderParameters(
			FMatrix(
				FPlane(1.0f,0.0f,0.0f,0.0f),
				FPlane(0.0f,1.0f,0.0f,0.0f),
				FPlane(0.0f,0.0f,1.0f,0.0f),
				FPlane(0.0f,0.0f,0.0f,1.0f)
				),
			FVector(0.0f,0.0f,0.0f),
			FBoxSphereBounds(EForceInit::ForceInit),
			FBoxSphereBounds(EForceInit::ForceInit),
			true,
			false,
			false,
			false,
			false,
			true,
			GetDefaultLightingChannelMask(),
			1.0f		// LPV bias
			));
	}
};

/** Global primitive uniform buffer resource containing identity transformations. */
extern ENGINE_API TGlobalResource<FIdentityPrimitiveUniformBuffer> GIdentityPrimitiveUniformBuffer;

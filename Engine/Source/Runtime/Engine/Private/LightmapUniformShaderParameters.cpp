// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LightmapUniformShaderParameters.h"
#include "SceneManagement.h"
#include "UnrealEngine.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FPrecomputedLightingUniformParameters, "PrecomputedLightingBuffer");

FLightmapSceneShaderData::FLightmapSceneShaderData(const class FLightCacheInterface* LCI, ERHIFeatureLevel::Type FeatureLevel)
{
	FPrecomputedLightingUniformParameters Parameters;
	GetPrecomputedLightingParameters(FeatureLevel, Parameters, LCI);
	Setup(Parameters);
}

void FLightmapSceneShaderData::Setup(const FPrecomputedLightingUniformParameters& ShaderParameters)
{
	static_assert(sizeof(FPrecomputedLightingUniformParameters) == 128, "The FLightmapSceneShaderData manual layout below and in usf must match FPrecomputedLightingUniformParameters.  Update this assert when adding a new member.");
	// Note: layout must match GetLightmapData in usf

	Data[0] = ShaderParameters.StaticShadowMapMasks;
	Data[1] = ShaderParameters.InvUniformPenumbraSizes;
	Data[2] = ShaderParameters.LightMapCoordinateScaleBias;
	Data[3] = ShaderParameters.ShadowMapCoordinateScaleBias;
	Data[4] = ShaderParameters.LightMapScale[0];
	Data[5] = ShaderParameters.LightMapScale[1];
	Data[6] = ShaderParameters.LightMapAdd[0];
	Data[7] = ShaderParameters.LightMapAdd[1];
}

void GetDefaultPrecomputedLightingParameters(FPrecomputedLightingUniformParameters& Parameters)
{
	Parameters.StaticShadowMapMasks = FVector4(1, 1, 1, 1);
	Parameters.InvUniformPenumbraSizes = FVector4(0, 0, 0, 0);
	Parameters.LightMapCoordinateScaleBias = FVector4(1, 1, 0, 0);
	Parameters.ShadowMapCoordinateScaleBias = FVector4(1, 1, 0, 0);
	 
	const uint32 NumCoef = FMath::Max<uint32>(NUM_HQ_LIGHTMAP_COEF, NUM_LQ_LIGHTMAP_COEF);
	for (uint32 CoefIndex = 0; CoefIndex < NumCoef; ++CoefIndex)
	{
		Parameters.LightMapScale[CoefIndex] = FVector4(1, 1, 1, 1);
		Parameters.LightMapAdd[CoefIndex] = FVector4(0, 0, 0, 0);
	}
}

void GetPrecomputedLightingParameters(
	ERHIFeatureLevel::Type FeatureLevel,
	FPrecomputedLightingUniformParameters& Parameters,
	const FLightCacheInterface* LCI
)
{
	// TDistanceFieldShadowsAndLightMapPolicy
	const FShadowMapInteraction ShadowMapInteraction = LCI ? LCI->GetShadowMapInteraction() : FShadowMapInteraction();
	if (ShadowMapInteraction.GetType() == SMIT_Texture)
	{
		Parameters.ShadowMapCoordinateScaleBias = FVector4(ShadowMapInteraction.GetCoordinateScale(), ShadowMapInteraction.GetCoordinateBias());
		Parameters.StaticShadowMapMasks = FVector4(ShadowMapInteraction.GetChannelValid(0), ShadowMapInteraction.GetChannelValid(1), ShadowMapInteraction.GetChannelValid(2), ShadowMapInteraction.GetChannelValid(3));
		Parameters.InvUniformPenumbraSizes = ShadowMapInteraction.GetInvUniformPenumbraSize();
	}
	else
	{
		Parameters.ShadowMapCoordinateScaleBias = FVector4(1, 1, 0, 0);
		Parameters.StaticShadowMapMasks = FVector4(1, 1, 1, 1);
		Parameters.InvUniformPenumbraSizes = FVector4(0, 0, 0, 0);
	}

	// TLightMapPolicy
	const FLightMapInteraction LightMapInteraction = LCI ? LCI->GetLightMapInteraction(FeatureLevel) : FLightMapInteraction();
	if (LightMapInteraction.GetType() == LMIT_Texture)
	{
		const bool bAllowHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel) && LightMapInteraction.AllowsHighQualityLightmaps();

		// Vertex Shader
		const FVector2D LightmapCoordinateScale = LightMapInteraction.GetCoordinateScale();
		const FVector2D LightmapCoordinateBias = LightMapInteraction.GetCoordinateBias();
		Parameters.LightMapCoordinateScaleBias = FVector4(LightmapCoordinateScale.X, LightmapCoordinateScale.Y, LightmapCoordinateBias.X, LightmapCoordinateBias.Y);

		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
		if (CVar->GetValueOnRenderThread() == 0)
		{

		}
		else
		{
			checkf(0, TEXT("VT needs to be implemented with Mesh Draw Command pipeline"));
		}

		const uint32 NumCoef = bAllowHighQualityLightMaps ? NUM_HQ_LIGHTMAP_COEF : NUM_LQ_LIGHTMAP_COEF;
		const FVector4* Scales = LightMapInteraction.GetScaleArray();
		const FVector4* Adds = LightMapInteraction.GetAddArray();
		for (uint32 CoefIndex = 0; CoefIndex < NumCoef; ++CoefIndex)
		{
			Parameters.LightMapScale[CoefIndex] = Scales[CoefIndex];
			Parameters.LightMapAdd[CoefIndex] = Adds[CoefIndex];
		}
	}
	else
	{
		// Vertex Shader
		Parameters.LightMapCoordinateScaleBias = FVector4(1, 1, 0, 0);

		// Pixel Shader
		const uint32 NumCoef = FMath::Max<uint32>(NUM_HQ_LIGHTMAP_COEF, NUM_LQ_LIGHTMAP_COEF);
		for (uint32 CoefIndex = 0; CoefIndex < NumCoef; ++CoefIndex)
		{
			Parameters.LightMapScale[CoefIndex] = FVector4(1, 1, 1, 1);
			Parameters.LightMapAdd[CoefIndex] = FVector4(0, 0, 0, 0);
		}
	}
}
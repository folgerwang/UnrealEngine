// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RayTracingLighting.h"
#include "RHI/Public/RHIDefinitions.h"
#include "RendererPrivate.h"

#if RHI_RAYTRACING

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRaytracingLightDataPacked, "RaytracingLightsDataPacked");

void SetupRaytracingLightDataPacked(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FRaytracingLightDataPacked* LightData)
{
	TMap<UTextureLightProfile*, int32> IESLightProfilesMap;
	TMap<FTextureRHIParamRef, uint32> RectTextureMap;

	LightData->Count = 0;
	LightData->LTCMatTexture = GSystemTextures.LTCMat->GetRenderTargetItem().ShaderResourceTexture;
	LightData->LTCMatSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	LightData->LTCAmpTexture = GSystemTextures.LTCAmp->GetRenderTargetItem().ShaderResourceTexture;
	LightData->LTCAmpSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FTextureRHIRef DymmyWhiteTexture = GWhiteTexture->TextureRHI;
	LightData->RectLightTexture0 = DymmyWhiteTexture;
	LightData->RectLightTexture1 = DymmyWhiteTexture;
	LightData->RectLightTexture2 = DymmyWhiteTexture;
	LightData->RectLightTexture3 = DymmyWhiteTexture;
	LightData->RectLightTexture4 = DymmyWhiteTexture;
	LightData->RectLightTexture5 = DymmyWhiteTexture;
	LightData->RectLightTexture6 = DymmyWhiteTexture;
	LightData->RectLightTexture7 = DymmyWhiteTexture;
	static constexpr uint32 MaxRectLightTextureSlos = 8;
	static constexpr uint32 InvalidTextureIndex = 99; // #dxr_todo: share this definition with ray tracing shaders

	for (auto Light : Lights)
	{
		const bool bHasStaticLighting = Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid();
		const bool bAffectReflection = Light.LightSceneInfo->Proxy->AffectReflection();
		if (bHasStaticLighting || !bAffectReflection) continue;

		FLightShaderParameters LightParameters;
		Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightParameters);

		if (Light.LightSceneInfo->Proxy->IsInverseSquared())
		{
			LightParameters.FalloffExponent = 0;
		}

		int32 IESLightProfileIndex = INDEX_NONE;
		if (View.Family->EngineShowFlags.TexturedLightProfiles)
		{
			UTextureLightProfile* IESLightProfileTexture = Light.LightSceneInfo->Proxy->GetIESTexture();
			if (IESLightProfileTexture)
			{
				int32* IndexFound = IESLightProfilesMap.Find(IESLightProfileTexture);
				if (!IndexFound)
				{
					IESLightProfileIndex = IESLightProfilesMap.Add(IESLightProfileTexture, IESLightProfilesMap.Num());
				}
				else
				{
					IESLightProfileIndex = *IndexFound;
				}
			}
		}

		LightData->Type_LightProfileIndex_RectLightTextureIndex[LightData->Count].X = Light.LightType;
		LightData->Type_LightProfileIndex_RectLightTextureIndex[LightData->Count].Y = IESLightProfileIndex;
		LightData->Type_LightProfileIndex_RectLightTextureIndex[LightData->Count].Z = InvalidTextureIndex;

		LightData->LightPosition_InvRadius[LightData->Count] = LightParameters.Position;
		LightData->LightPosition_InvRadius[LightData->Count].W = LightParameters.InvRadius;

		LightData->LightColor_SpecularScale[LightData->Count] = LightParameters.Color;
		LightData->LightColor_SpecularScale[LightData->Count].W = LightParameters.SpecularScale;

		LightData->Direction_FalloffExponent[LightData->Count] = LightParameters.Direction;
		LightData->Direction_FalloffExponent[LightData->Count].W = LightParameters.FalloffExponent;

		LightData->Tangent_SourceRadius[LightData->Count] = LightParameters.Tangent;
		LightData->Tangent_SourceRadius[LightData->Count].W = LightParameters.SourceRadius;

		FVector4 SpotAngles_SourceLength_SoftSourceRadius = FVector4(LightParameters.SpotAngles, FVector2D(LightParameters.SourceLength, LightParameters.SoftSourceRadius));
		LightData->SpotAngles_SourceLength_SoftSourceRadius[LightData->Count] = SpotAngles_SourceLength_SoftSourceRadius;
		
		const FVector2D FadeParams = Light.LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), Light.LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);

		FVector4 DistanceFadeMAD_RectLightBarnCosAngle_RectLightBarnLength = FVector4(FadeParams.Y, -FadeParams.X * FadeParams.Y, LightParameters.RectLightBarnCosAngle, LightParameters.RectLightBarnLength);
		LightData->DistanceFadeMAD_RectLightBarnCosAngle_RectLightBarnLength[LightData->Count] = DistanceFadeMAD_RectLightBarnCosAngle_RectLightBarnLength;

		const bool bRequireTexture = Light.LightType == ELightComponentType::LightType_Rect && LightParameters.SourceTexture;
		uint32 RectLightTextureIndex = InvalidTextureIndex;
		if (bRequireTexture)
		{
			const uint32* IndexFound = RectTextureMap.Find(LightParameters.SourceTexture);
			if (!IndexFound)
			{
				if (RectTextureMap.Num() < MaxRectLightTextureSlos)
				{
					RectLightTextureIndex = RectTextureMap.Num();
					RectTextureMap.Add(LightParameters.SourceTexture, RectLightTextureIndex);
				}
			}
			else
			{
				RectLightTextureIndex = *IndexFound;
			}
		}

		if (RectLightTextureIndex != InvalidTextureIndex)
		{
			LightData->Type_LightProfileIndex_RectLightTextureIndex[LightData->Count].Z = RectLightTextureIndex;
			switch (RectLightTextureIndex)
			{
			case 0: LightData->RectLightTexture0 = LightParameters.SourceTexture; break;
			case 1: LightData->RectLightTexture1 = LightParameters.SourceTexture; break;
			case 2: LightData->RectLightTexture2 = LightParameters.SourceTexture; break;
			case 3: LightData->RectLightTexture3 = LightParameters.SourceTexture; break;
			case 4: LightData->RectLightTexture4 = LightParameters.SourceTexture; break;
			case 5: LightData->RectLightTexture5 = LightParameters.SourceTexture; break;
			case 6: LightData->RectLightTexture6 = LightParameters.SourceTexture; break;
			case 7: LightData->RectLightTexture7 = LightParameters.SourceTexture; break;
			}
		}

		LightData->Count++;

		if (LightData->Count >= GRaytracingLightCountMaximum) break;
	}

	// Update IES light profiles texture 
	// TODO (Move to a shared place)
	if (View.IESLightProfileResource != nullptr && IESLightProfilesMap.Num() > 0)
	{
		TArray<UTextureLightProfile*, SceneRenderingAllocator> IESProfilesArray;
		IESProfilesArray.AddUninitialized(IESLightProfilesMap.Num());
		for (auto It = IESLightProfilesMap.CreateIterator(); It; ++It)
		{
			IESProfilesArray[It->Value] = It->Key;
		}

		View.IESLightProfileResource->BuildIESLightProfilesTexture(IESProfilesArray);
	}
}

TUniformBufferRef<FRaytracingLightDataPacked> CreateLightDataPackedUniformBuffer(const TSparseArray<FLightSceneInfoCompact>& Lights, const class FViewInfo& View, EUniformBufferUsage Usage)
{
	FRaytracingLightDataPacked LightData;
	SetupRaytracingLightDataPacked(Lights, View, &LightData);
	return CreateUniformBufferImmediate(LightData, Usage);
}

#endif // RHI_RAYTRACING
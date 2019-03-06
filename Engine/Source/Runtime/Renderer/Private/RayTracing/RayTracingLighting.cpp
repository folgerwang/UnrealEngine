// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RayTracingLighting.h"
#include "RHI/Public/RHIDefinitions.h"
#include "RendererPrivate.h"

#if RHI_RAYTRACING

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRaytracingLightData, "RaytracingLightsData");

void SetupRaytracingLightData(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FRaytracingLightData* LightData)
{
	TMap<UTextureLightProfile*, int32> IESLightProfilesMap;

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
	const uint32 RectLightTextureSlotCount = 8;
	uint32 CurrentRectLightIndex = 0;

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

		LightData->Type[LightData->Count] = Light.LightType;
		LightData->LightPosition[LightData->Count] = LightParameters.Position;
		LightData->LightInvRadius[LightData->Count] = LightParameters.InvRadius;
		LightData->LightColor[LightData->Count] = LightParameters.Color;
		LightData->LightFalloffExponent[LightData->Count] = LightParameters.FalloffExponent;
		LightData->Direction[LightData->Count] = LightParameters.Direction;
		LightData->Tangent[LightData->Count] = LightParameters.Tangent;
		LightData->SpotAngles[LightData->Count] = LightParameters.SpotAngles;
		LightData->SpecularScale[LightData->Count] = LightParameters.SpecularScale;
		LightData->SourceRadius[LightData->Count] = LightParameters.SourceRadius;
		LightData->SourceLength[LightData->Count] = LightParameters.SourceLength;
		LightData->SoftSourceRadius[LightData->Count] = LightParameters.SoftSourceRadius;
		LightData->LightProfileIndex[LightData->Count] = IESLightProfileIndex;
		LightData->RectLightTextureIndex[LightData->Count] = 99;
		LightData->RectLightBarnCosAngle[LightData->Count] = LightParameters.RectLightBarnCosAngle;
		LightData->RectLightBarnLength[LightData->Count] = LightParameters.RectLightBarnLength;

		const bool bAllocateRectTextureSlot = Light.LightType == ELightComponentType::LightType_Rect && LightParameters.SourceTexture && CurrentRectLightIndex < RectLightTextureSlotCount;
		if (bAllocateRectTextureSlot)
		{
			LightData->RectLightTextureIndex[LightData->Count] = CurrentRectLightIndex;
			switch (CurrentRectLightIndex)
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
			++CurrentRectLightIndex;
			
		}

		const FVector2D FadeParams = Light.LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), Light.LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);
		LightData->DistanceFadeMAD[LightData->Count] = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

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

TUniformBufferRef<FRaytracingLightData> CreateLightDataUniformBuffer(const TSparseArray<FLightSceneInfoCompact>& Lights, const class FViewInfo& View, EUniformBufferUsage Usage)
{
	FRaytracingLightData LightData;
	SetupRaytracingLightData(Lights, View, &LightData);
	return CreateUniformBufferImmediate(LightData, Usage);
}

#endif // RHI_RAYTRACING
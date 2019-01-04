// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.cpp
=============================================================================*/

#include "DecalRenderingShared.h"
#include "StaticBoundShaderState.h"
#include "Components/DecalComponent.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "DebugViewModeRendering.h"
#include "ScenePrivate.h"
#include "PipelineStateCache.h"

static TAutoConsoleVariable<float> CVarDecalFadeScreenSizeMultiplier(
	TEXT("r.Decal.FadeScreenSizeMult"),
	1.0f,
	TEXT("Control the per decal fade screen size. Multiplies with the per-decal screen size fade threshold.")
	TEXT("  Smaller means decals fade less aggressively.")
	);

static bool IsBlendModeSupported(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode)
{
	if (IsMobilePlatform(Platform))
	{
		switch (DecalBlendMode)
		{
			case DBM_Stain:			 // Modulate
			case DBM_Emissive:		 // Additive
			case DBM_Translucent:	 // Translucent
			case DBM_AlphaComposite: // Premultiplied Alpha
				break;
			default:
				return false;
		}
	}

	return true;
}

FTransientDecalRenderData::FTransientDecalRenderData(const FScene& InScene, const FDeferredDecalProxy* InDecalProxy, float InConservativeRadius)
	: DecalProxy(InDecalProxy)
	, FadeAlpha(1.0f)
	, ConservativeRadius(InConservativeRadius)
{
	MaterialProxy = InDecalProxy->DecalMaterial->GetRenderProxy();
	MaterialResource = MaterialProxy->GetMaterial(InScene.GetFeatureLevel());
	check(MaterialProxy && MaterialResource);
	bHasNormal = MaterialResource->HasNormalConnected();
	FinalDecalBlendMode = FDecalRenderingCommon::ComputeFinalDecalBlendMode(InScene.GetShaderPlatform(), (EDecalBlendMode)MaterialResource->GetDecalBlendMode(), bHasNormal);
}

/**
 * A vertex shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalVS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FDeferredDecalVS( )	{ }
	FDeferredDecalVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FrustumComponentToClip.Bind(Initializer.ParameterMap, TEXT("FrustumComponentToClip"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FUniformBufferRHIParamRef ViewUniformBuffer, const FMatrix& InFrustumComponentToClip)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, ViewUniformBuffer);
		SetShaderValue(RHICmdList, ShaderRHI, FrustumComponentToClip, InFrustumComponentToClip);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FrustumComponentToClip;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter FrustumComponentToClip;
};

IMPLEMENT_SHADER_TYPE(,FDeferredDecalVS,TEXT("/Engine/Private/DeferredDecal.usf"),TEXT("MainVS"),SF_Vertex);

/**
 * A pixel shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalPS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsDeferredDecal' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsDeferredDecal();
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FDecalRendering::SetDecalCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FDeferredDecalPS() {}
	FDeferredDecalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		SvPositionToDecal.Bind(Initializer.ParameterMap,TEXT("SvPositionToDecal"));
		DecalToWorld.Bind(Initializer.ParameterMap,TEXT("DecalToWorld"));
		WorldToDecal.Bind(Initializer.ParameterMap,TEXT("WorldToDecal"));
		DecalOrientation.Bind(Initializer.ParameterMap,TEXT("DecalOrientation"));
		DecalParams.Bind(Initializer.ParameterMap, TEXT("DecalParams"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FDeferredDecalProxy& DecalProxy, const float FadeAlphaValue=1.0f)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneTextureSetupMode::All);

		FTransform ComponentTrans = DecalProxy.ComponentTrans;

		FMatrix WorldToComponent = ComponentTrans.ToInverseMatrixWithScale();

		// Set the transform from screen space to light space.
		if(SvPositionToDecal.IsBound())
		{
			FVector2D InvViewSize = FVector2D(1.0f / View.ViewRect.Width(), 1.0f / View.ViewRect.Height());

			// setup a matrix to transform float4(SvPosition.xyz,1) directly to Decal (quality, performance as we don't need to convert or use interpolator)

			//	new_xy = (xy - ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

			//  transformed into one MAD:  new_xy = xy * ViewSizeAndInvSize.zw * float2(2,-2)      +       (-ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

			float Mx = 2.0f * InvViewSize.X;
			float My = -2.0f * InvViewSize.Y;
			float Ax = -1.0f - 2.0f * View.ViewRect.Min.X * InvViewSize.X;
			float Ay = 1.0f + 2.0f * View.ViewRect.Min.Y * InvViewSize.Y;

			// todo: we could use InvTranslatedViewProjectionMatrix and TranslatedWorldToComponent for better quality
			const FMatrix SvPositionToDecalValue = 
				FMatrix(
					FPlane(Mx,  0,   0,  0),
					FPlane( 0, My,   0,  0),
					FPlane( 0,  0,   1,  0),
					FPlane(Ax, Ay,   0,  1)
				) * View.ViewMatrices.GetInvViewProjectionMatrix() * WorldToComponent;

			SetShaderValue(RHICmdList, ShaderRHI, SvPositionToDecal, SvPositionToDecalValue);
		}

		// Set the transform from light space to world space
		if(DecalToWorld.IsBound())
		{
			const FMatrix DecalToWorldValue = ComponentTrans.ToMatrixWithScale();
			
			SetShaderValue(RHICmdList, ShaderRHI, DecalToWorld, DecalToWorldValue);
		}

		SetShaderValue(RHICmdList, ShaderRHI, WorldToDecal, WorldToComponent);

		if (DecalOrientation.IsBound())
		{
			// can get DecalOrientation form DecalToWorld matrix, but it will require binding whole matrix and normalizing axis in the shader
			SetShaderValue(RHICmdList, ShaderRHI, DecalOrientation, ComponentTrans.GetUnitAxis(EAxis::X));
		}
		
		float LifetimeAlpha = FMath::Clamp(FMath::Min(View.Family->CurrentWorldTime * -DecalProxy.InvFadeDuration + DecalProxy.FadeStartDelayNormalized, View.Family->CurrentWorldTime * DecalProxy.InvFadeInDuration + DecalProxy.FadeInStartDelayNormalized), 0.0f, 1.0f);
		SetShaderValue(RHICmdList, ShaderRHI, DecalParams, FVector2D(FadeAlphaValue, LifetimeAlpha));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << SvPositionToDecal << DecalToWorld << WorldToDecal << DecalOrientation << DecalParams;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter SvPositionToDecal;
	FShaderParameter DecalToWorld;
	FShaderParameter WorldToDecal;
	FShaderParameter DecalOrientation;
	FShaderParameter DecalParams;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDeferredDecalPS,TEXT("/Engine/Private/DeferredDecal.usf"),TEXT("MainPS"),SF_Pixel);

class FDeferredDecalEmissivePS : public FDeferredDecalPS
{
	DECLARE_SHADER_TYPE(FDeferredDecalEmissivePS, Material);
public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material)
	{
		return FDeferredDecalPS::ShouldCompilePermutation(Platform, Material)
			&& Material->HasEmissiveColorConnected()
			&& IsDBufferDecalBlendMode(FDecalRenderingCommon::ComputeFinalDecalBlendMode(Platform, Material));
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FDeferredDecalPS::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FDecalRendering::SetEmissiveDBufferDecalCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FDeferredDecalEmissivePS() {}
	FDeferredDecalEmissivePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FDeferredDecalPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FDeferredDecalEmissivePS, TEXT("/Engine/Private/DeferredDecal.usf"), TEXT("MainPS"), SF_Pixel);

bool FDecalRendering::BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList* OutVisibleDecals)
{
	QUICK_SCOPE_CYCLE_COUNTER(BuildVisibleDecalList);

	if (OutVisibleDecals)
	{
		OutVisibleDecals->Empty(Scene.Decals.Num());
	}

	const float FadeMultiplier = CVarDecalFadeScreenSizeMultiplier.GetValueOnRenderThread();
	const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();

	const bool bIsPerspectiveProjection = View.IsPerspectiveProjection();

	// Build a list of decals that need to be rendered for this view in SortedDecals
	for (const FDeferredDecalProxy* DecalProxy : Scene.Decals)
	{
		if (!DecalProxy->DecalMaterial || !DecalProxy->DecalMaterial->IsValidLowLevelFast())
		{
			continue;
		}

		bool bIsShown = true;

		if (!DecalProxy->IsShown(&View))
		{
			bIsShown = false;
		}

		const FMatrix ComponentToWorldMatrix = DecalProxy->ComponentTrans.ToMatrixWithScale();

		// can be optimized as we test against a sphere around the box instead of the box itself
		const float ConservativeRadius = FMath::Sqrt(
				ComponentToWorldMatrix.GetScaledAxis(EAxis::X).SizeSquared() +
				ComponentToWorldMatrix.GetScaledAxis(EAxis::Y).SizeSquared() +
				ComponentToWorldMatrix.GetScaledAxis(EAxis::Z).SizeSquared());

		// can be optimized as the test is too conservative (sphere instead of OBB)
		if(ConservativeRadius < SMALL_NUMBER || !View.ViewFrustum.IntersectSphere(ComponentToWorldMatrix.GetOrigin(), ConservativeRadius))
		{
			bIsShown = false;
		}

		if (bIsShown)
		{
			FTransientDecalRenderData Data(Scene, DecalProxy, ConservativeRadius);
			
			// filter out decals with blend modes that are not supported on current platform
			if (IsBlendModeSupported(ShaderPlatform, Data.FinalDecalBlendMode))
			{
				if (bIsPerspectiveProjection && Data.DecalProxy->FadeScreenSize != 0.0f)
				{
					float Distance = (View.ViewMatrices.GetViewOrigin() - ComponentToWorldMatrix.GetOrigin()).Size();
					float Radius = ComponentToWorldMatrix.GetMaximumAxisScale();
					float CurrentScreenSize = ((Radius / Distance) * FadeMultiplier);

					// fading coefficient needs to increase with increasing field of view and decrease with increasing resolution
					// FadeCoeffScale is an empirically determined constant to bring us back roughly to fraction of screen size for FadeScreenSize
					const float FadeCoeffScale = 600.0f;
					float FOVFactor = ((2.0f/View.ViewMatrices.GetProjectionMatrix().M[0][0]) / View.ViewRect.Width()) * FadeCoeffScale;
					float FadeCoeff = Data.DecalProxy->FadeScreenSize * FOVFactor;
					float FadeRange = FadeCoeff * 0.5f;

					float Alpha = (CurrentScreenSize - FadeCoeff) / FadeRange;
					Data.FadeAlpha = FMath::Min(Alpha, 1.0f);
				}

				EDecalRenderStage LocalDecalRenderStage = FDecalRenderingCommon::ComputeRenderStage(ShaderPlatform, Data.FinalDecalBlendMode);

				const bool bShouldRender = Data.FadeAlpha > 0.0f &&
					FDecalRenderingCommon::IsCompatibleWithRenderStage(
						DecalRenderStage,
						LocalDecalRenderStage,
						Data.FinalDecalBlendMode,
						Data.MaterialResource);

				// we could do this test earlier to avoid the decal intersection but getting DecalBlendMode also costs
				if (View.Family->EngineShowFlags.ShaderComplexity || bShouldRender)
				{
					if (!OutVisibleDecals)
					{
						return true;
					}
					OutVisibleDecals->Add(Data);
				}
			}
		}
	}

	if (!OutVisibleDecals)
	{
		return false;
	}

	if (OutVisibleDecals->Num() > 0)
	{
		// Sort by sort order to allow control over composited result
		// Then sort decals by state to reduce render target switches
		// Also sort by component since Sort() is not stable
		struct FCompareFTransientDecalRenderData
		{
			FORCEINLINE bool operator()(const FTransientDecalRenderData& A, const FTransientDecalRenderData& B) const
			{
				if (B.DecalProxy->SortOrder != A.DecalProxy->SortOrder)
				{ 
					return A.DecalProxy->SortOrder < B.DecalProxy->SortOrder;
				}
				// bHasNormal here is more important then blend mode because we want to render every decals that output normals before those that read normal.
				if (B.bHasNormal != A.bHasNormal)
				{
					return B.bHasNormal < A.bHasNormal; // < so that those outputting normal are first.
				}
				if (B.FinalDecalBlendMode != A.FinalDecalBlendMode)
				{
					return (int32)B.FinalDecalBlendMode < (int32)A.FinalDecalBlendMode;
				}
				// Batch decals with the same material together
				if (B.MaterialProxy != A.MaterialProxy)
				{
					return B.MaterialProxy < A.MaterialProxy;
				}
				return (PTRINT)B.DecalProxy->Component < (PTRINT)A.DecalProxy->Component;
			}
		};

		// Sort decals by blend mode to reduce render target switches
		OutVisibleDecals->Sort(FCompareFTransientDecalRenderData());

		return true;
	}

	return false;
}

FMatrix FDecalRendering::ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld)
{
	FMatrix ComponentToWorldMatrixTrans = DecalComponentToWorld.ConcatTranslation(View.ViewMatrices.GetPreViewTranslation());
	return ComponentToWorldMatrixTrans * View.ViewMatrices.GetTranslatedViewProjectionMatrix();
}

void FDecalRendering::SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View,
	const FTransientDecalRenderData& DecalData, EDecalRenderStage DecalRenderStage, const FMatrix& FrustumComponentToClip)
{
	const FMaterialShaderMap* MaterialShaderMap = DecalData.MaterialResource->GetRenderingThreadShaderMap();
	const EDebugViewShaderMode DebugViewMode = View.Family->GetDebugViewShaderMode();

	// When in shader complexity, decals get rendered as emissive even though there might not be emissive decals.
	// FDeferredDecalEmissivePS might not be available depending on the decal blend mode.
	FDeferredDecalPS* PixelShader = (DecalRenderStage == DRS_Emissive && DebugViewMode == DVSM_None)
		? MaterialShaderMap->GetShader<FDeferredDecalEmissivePS>()
		: MaterialShaderMap->GetShader<FDeferredDecalPS>();

	TShaderMapRef<FDeferredDecalVS> VertexShader(View.ShaderMap);

	{
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		PixelShader->SetParameters(RHICmdList, View, DecalData.MaterialProxy, *DecalData.DecalProxy, DecalData.FadeAlpha);
	}

	// SetUniformBufferParameter() need to happen after the shader has been set otherwise a DebugBreak could occur.

	// we don't have the Primitive uniform buffer setup for decals (later we want to batch)
	{
		auto& PrimitiveVS = VertexShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		auto& PrimitivePS = PixelShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();

		// uncomment to track down usage of the Primitive uniform buffer
		//	check(!PrimitiveVS.IsBound());
		//	check(!PrimitivePS.IsBound());

		// to prevent potential shader error (UE-18852 ElementalDemo crashes due to nil constant buffer)
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), PrimitiveVS, GIdentityPrimitiveUniformBuffer);

		if (DebugViewMode == DVSM_None)
		{
			SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PrimitivePS, GIdentityPrimitiveUniformBuffer);
		}
	}

	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer, FrustumComponentToClip);

	// Set stream source after updating cached strides
	RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);
}

void FDecalRendering::SetVertexShaderOnly(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FMatrix& FrustumComponentToClip)
{
	TShaderMapRef<FDeferredDecalVS> VertexShader(View.ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer, FrustumComponentToClip);
}

// @return e.g. 1+2+4 means DBufferA(1) + DBufferB(2) + DBufferC(4) is used
static uint8 ComputeDBufferMRTMask(EDecalBlendMode DecalBlendMode)
{
	switch (DecalBlendMode)
	{
	case DBM_DBuffer_AlphaComposite:
		return 1 + 4; // AlphaComposite mode does not touch normals (DBufferB)
	case DBM_DBuffer_ColorNormalRoughness:
		return 1 + 2 + 4;
	case DBM_DBuffer_Emissive:
	case DBM_DBuffer_EmissiveAlphaComposite:
	case DBM_DBuffer_Color:
		return 1;
	case DBM_DBuffer_ColorNormal:
		return 1 + 2;
	case DBM_DBuffer_ColorRoughness:
		return 1 + 4;
	case DBM_DBuffer_Normal:
		return 2;
	case DBM_DBuffer_NormalRoughness:
		return 2 + 4;
	case DBM_DBuffer_Roughness:
		return 4;
	}

	return 0;
}

void FDecalRendering::SetDecalCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	bool bHasNormalConnected = Material->HasNormalConnected();
	EDecalBlendMode FinalDecalBlendMode = FDecalRenderingCommon::ComputeFinalDecalBlendMode(Platform, (EDecalBlendMode)Material->GetDecalBlendMode(), bHasNormalConnected);
	EDecalRenderStage DecalRenderStage = FDecalRenderingCommon::ComputeRenderStage(Platform, FinalDecalBlendMode);
	FDecalRenderingCommon::ERenderTargetMode RenderTargetMode = FDecalRenderingCommon::ComputeRenderTargetMode(Platform, FinalDecalBlendMode, bHasNormalConnected);
	uint32 RenderTargetCount = FDecalRenderingCommon::ComputeRenderTargetCount(Platform, RenderTargetMode);

	uint32 BindTarget1 = (RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferNoNormal || RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteNoNormal) ? 0 : 1;
	OutEnvironment.SetDefine(TEXT("BIND_RENDERTARGET1"), BindTarget1);

	// avoid using the index directly, better use DECALBLENDMODEID_VOLUMETRIC, DECALBLENDMODEID_STAIN, ...
	OutEnvironment.SetDefine(TEXT("DECAL_BLEND_MODE"), (uint32)FinalDecalBlendMode);
	OutEnvironment.SetDefine(TEXT("DECAL_PROJECTION"), 1);
	OutEnvironment.SetDefine(TEXT("DECAL_RENDERTARGET_COUNT"), RenderTargetCount);
	OutEnvironment.SetDefine(TEXT("DECAL_RENDERSTAGE"), (uint32)DecalRenderStage);

	uint8 bDBufferMask = ComputeDBufferMRTMask(FinalDecalBlendMode);

	OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERA"), (bDBufferMask & 0x1) != 0);
	OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERB"), (bDBufferMask & 0x2) != 0);
	OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERC"), (bDBufferMask & 0x4) != 0);

}

void FDecalRendering::SetEmissiveDBufferDecalCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("BIND_RENDERTARGET1"), 0);
	OutEnvironment.SetDefine(TEXT("DECAL_BLEND_MODE"), (uint32)DBM_DBuffer_Emissive);
	OutEnvironment.SetDefine(TEXT("DECAL_RENDERTARGET_COUNT"), 1);
	OutEnvironment.SetDefine(TEXT("DECAL_RENDERSTAGE"), (uint32)DRS_Emissive);

	OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERA"), 0);
	OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERB"), 0);
	OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERC"), 0);
}

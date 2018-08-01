// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OpenCVLensDistortionParameters.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "IOpenCVLensDistortionModule.h"
#include "Logging/LogMacros.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "SceneInterface.h"
#include "ShaderParameterUtils.h"


//Parameters for the grid we'll use to get the reciprocal of our undistortion map
static const uint32 kGridSubdivisionX = 32;
static const uint32 kGridSubdivisionY = 16;

#if WITH_OPENCV
class FLensDistortionDisplacementMapGenerationShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GRID_SUBDIVISION_X"), kGridSubdivisionX);
		OutEnvironment.SetDefine(TEXT("GRID_SUBDIVISION_Y"), kGridSubdivisionY);
	}

	FLensDistortionDisplacementMapGenerationShader() {}

	FLensDistortionDisplacementMapGenerationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PixelUVSize.Bind(Initializer.ParameterMap, TEXT("PixelUVSize"));
		UndistortDisplacementMap.Bind(Initializer.ParameterMap, TEXT("UndistortDisplacementMap"));
		BilinearSampler.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSampler"));
	}

	template<typename TShaderRHIParamRef>
	void SetParameters(FRHICommandListImmediate& RHICmdList, const TShaderRHIParamRef ShaderRHI, const FTextureResource* PreComputedDisplacementMap, const FIntPoint& DisplacementMapResolution)
	{
		FVector2D PixelUVSizeValue(1.f / float(DisplacementMapResolution.X), 1.f / float(DisplacementMapResolution.Y));

		SetShaderValue(RHICmdList, ShaderRHI, PixelUVSize, PixelUVSizeValue);
		SetTextureParameter(RHICmdList, ShaderRHI, UndistortDisplacementMap, BilinearSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), PreComputedDisplacementMap->TextureRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PixelUVSize << UndistortDisplacementMap << BilinearSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter PixelUVSize;
	FShaderResourceParameter UndistortDisplacementMap;
	FShaderResourceParameter BilinearSampler;
};


class FLensDistortionDisplacementMapGenerationVS : public FLensDistortionDisplacementMapGenerationShader
{
	DECLARE_SHADER_TYPE(FLensDistortionDisplacementMapGenerationVS, Global);

public:

	/** Default constructor. */
	FLensDistortionDisplacementMapGenerationVS() {}

	/** Initialization constructor. */
	FLensDistortionDisplacementMapGenerationVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FLensDistortionDisplacementMapGenerationShader(Initializer)
	{
	}
};


class FLensDistortionDisplacementMapGenerationPS : public FLensDistortionDisplacementMapGenerationShader
{
	DECLARE_SHADER_TYPE(FLensDistortionDisplacementMapGenerationPS, Global);

public:

	/** Default constructor. */
	FLensDistortionDisplacementMapGenerationPS() {}

	/** Initialization constructor. */
	FLensDistortionDisplacementMapGenerationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FLensDistortionDisplacementMapGenerationShader(Initializer)
	{ }
};


IMPLEMENT_SHADER_TYPE(, FLensDistortionDisplacementMapGenerationVS, TEXT("/Plugin/OpenCVLensDistortion/Private/DisplacementMapGeneration.usf"), TEXT("MainVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FLensDistortionDisplacementMapGenerationPS, TEXT("/Plugin/OpenCVLensDistortion/Private/DisplacementMapGeneration.usf"), TEXT("MainPS"), SF_Pixel)
#endif

static void DrawUVDisplacementToRenderTargetFromPreComputedDisplacementMap_RenderThread(FRHICommandListImmediate& RHICmdList
																			, const FTextureResource* PreComputedDisplacementMap
																			, const FName& TextureRenderTargetName
																			, FTextureRenderTargetResource* OutTextureRenderTargetResource
																			, ERHIFeatureLevel::Type FeatureLevel)
{
#if WITH_OPENCV
	check(IsInRenderingThread());
	check(PreComputedDisplacementMap);

#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	TextureRenderTargetName.ToString(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, DrawUVDisplacementToRenderTargetFromPreComputedDisplacementMap, TEXT("OpenCVLensDistortionDisplacementMapGeneration %s"), *EventName);
#else
	SCOPED_DRAW_EVENT(RHICmdList, DrawUVDisplacementToRenderTargetFromPreComputedDisplacementMap);
#endif

	// Set render target.
	SetRenderTarget(RHICmdList, OutTextureRenderTargetResource->GetRenderTargetTexture(), FTextureRHIRef(),	ESimpleRenderTargetMode::EClearColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);

	// Update viewport.
	const FIntPoint DisplacementMapResolution(OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY());
	RHICmdList.SetViewport(0, 0, 0.f, DisplacementMapResolution.X, DisplacementMapResolution.Y, 1.f);
	
	// Get shaders.
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef< FLensDistortionDisplacementMapGenerationVS > VertexShader(GlobalShaderMap);
	TShaderMapRef< FLensDistortionDisplacementMapGenerationPS > PixelShader(GlobalShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Update shader uniform parameters.
	VertexShader->SetParameters(RHICmdList, VertexShader->GetVertexShader(), PreComputedDisplacementMap, DisplacementMapResolution);
	PixelShader->SetParameters(RHICmdList, PixelShader->GetPixelShader(), PreComputedDisplacementMap, DisplacementMapResolution);

	// Draw grid.
	const uint32 PrimitiveCount = kGridSubdivisionX * kGridSubdivisionY * 2;
	RHICmdList.DrawPrimitive(PT_TriangleList, 0, PrimitiveCount, 1);

	// Resolve render target.
	RHICmdList.CopyToResolveTarget(OutTextureRenderTargetResource->GetRenderTargetTexture(), OutTextureRenderTargetResource->TextureRHI, FResolveParams());
#endif
}

void FOpenCVLensDistortionParameters::DrawDisplacementMapToRenderTarget(UWorld* InWorld, UTextureRenderTarget2D* InOutputRenderTarget, UTexture2D* InPreComputedUndistortDisplacementMap)
{
#if WITH_OPENCV
	check(IsInGameThread());

	if (!InOutputRenderTarget)
	{
		UE_LOG(LogOpenCVLensDistortion, Error, TEXT("Invalid render target to draw on."));
		return;
	}

	if(InPreComputedUndistortDisplacementMap == nullptr || InPreComputedUndistortDisplacementMap->Resource == nullptr)
	{
		UE_LOG(LogOpenCVLensDistortion, Error, TEXT("Precomputed displacement map is required to generate final displacement maps."));
		return;
	}

	//Prepare parameters for render command
	const FName TextureRenderTargetName = InOutputRenderTarget->GetFName();
	FTextureRenderTargetResource* TextureRenderTargetResource = InOutputRenderTarget->GameThread_GetRenderTargetResource();
	const FTextureResource* PreComputedMapResource = InPreComputedUndistortDisplacementMap->Resource;
	ERHIFeatureLevel::Type FeatureLevel = InWorld->Scene->GetFeatureLevel();

	ENQUEUE_RENDER_COMMAND(CaptureCommand)
	(
		[PreComputedMapResource, TextureRenderTargetResource, TextureRenderTargetName, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			DrawUVDisplacementToRenderTargetFromPreComputedDisplacementMap_RenderThread(RHICmdList, PreComputedMapResource, TextureRenderTargetName,	TextureRenderTargetResource, FeatureLevel);
		}
	);
#endif
}

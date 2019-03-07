// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIORendering.h"

#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "IOpenColorIOModule.h"
#include "Logging/LogMacros.h"
#include "Logging/MessageLog.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOShader.h"
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "SceneInterface.h"
#include "SceneUtils.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"



class FOpenColorIOVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOpenColorIOVertexShader, Global);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	/** Default constructor. */
	FOpenColorIOVertexShader() {}

	/** Initialization constructor. */
	FOpenColorIOVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

public:

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};


IMPLEMENT_SHADER_TYPE(, FOpenColorIOVertexShader, TEXT("/Plugin/OpenColorIO/Private/OpenColorIOBaseVS.usf"), TEXT("MainVS"), SF_Vertex)


static void ProcessOCIOColorSpaceTransform_RenderThread(
	FRHICommandListImmediate& InRHICmdList
	, ERHIFeatureLevel::Type InFeatureLevel
	, FOpenColorIOTransformResource* InOCIOColorTransformResource
	, FTextureResource* InLUT3dResource
	, FTextureResource* InputSpaceColorResource
	, FTextureResource* OutputSpaceColorResource)
{
	check(IsInRenderingThread());

	SCOPED_DRAW_EVENT(InRHICmdList, ProcessOCIOColorSpaceTransform);

	FRHIRenderPassInfo RPInfo(OutputSpaceColorResource->TextureRHI, ERenderTargetActions::DontLoad_Store);
	InRHICmdList.BeginRenderPass(RPInfo, TEXT("ProcessOCIOColorSpaceXfrm"));

	FIntPoint Resolution(OutputSpaceColorResource->GetSizeX(), OutputSpaceColorResource->GetSizeY());

	// Set viewport.
	InRHICmdList.SetViewport(0, 0, 0.f, Resolution.X, Resolution.Y, 1.f);

	// Get shader from shader map.
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(InFeatureLevel);
	TShaderMapRef<FOpenColorIOVertexShader> VertexShader(GlobalShaderMap);
	FOpenColorIOPixelShader* OCIOPixelShader = InOCIOColorTransformResource->GetShader();

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(OCIOPixelShader);
	SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit);

	// Update pixel shader parameters
	{
		OCIOPixelShader->SetParameters(InRHICmdList, InputSpaceColorResource);

		if (InLUT3dResource != nullptr)
		{
			OCIOPixelShader->SetLUTParameter(InRHICmdList, InLUT3dResource);
		}
	}

	// Draw grid.
	InRHICmdList.DrawPrimitive(0, 2, 1);

	// Resolve render target.
	InRHICmdList.EndRenderPass();
}

// static
bool FOpenColorIORendering::ApplyColorTransform(UWorld* InWorld, const FOpenColorIOColorConversionSettings& InSettings, UTexture* InTexture, UTextureRenderTarget2D* OutRenderTarget)
{
	check(IsInGameThread());

	if (InSettings.ConfigurationSource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid config asset"));
		return false;
	}

	if (InTexture == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Input Texture"));
		return false;
	}

	if (OutRenderTarget == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Output Texture"));
		return false;
	}


	FTextureResource* InputResource = InTexture->Resource;
	FTextureResource* OutputResource = OutRenderTarget->Resource;
	if (InputResource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Input Texture resource"));
		return false;
	}

	if (OutputResource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Output Texture resource"));
		return false;
	}

	const ERHIFeatureLevel::Type FeatureLevel = InWorld->Scene->GetFeatureLevel();
	FOpenColorIOTransformResource* ShaderResource = nullptr;
	FTextureResource* LUT3dResource = nullptr;
	bool bFoundTransform = InSettings.ConfigurationSource->GetShaderAndLUTResources(FeatureLevel, InSettings.SourceColorSpace.ColorSpaceName, InSettings.DestinationColorSpace.ColorSpaceName, ShaderResource, LUT3dResource);
	if (!bFoundTransform)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Couldn't find shader to transform from %s to %s"), *InSettings.SourceColorSpace.ColorSpaceName, *InSettings.DestinationColorSpace.ColorSpaceName);
		return false;
	}

	check(ShaderResource);

	if (ShaderResource->GetShaderGameThread() == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("OCIOPass - Shader was invalid for Resource %s"), *ShaderResource->GetFriendlyName());
		return false;
	}


	ENQUEUE_RENDER_COMMAND(ProcessColorSpaceTransform)(
		[FeatureLevel, InputResource, OutputResource, ShaderResource, LUT3dResource](FRHICommandListImmediate& RHICmdList)
	{
		ProcessOCIOColorSpaceTransform_RenderThread(
			RHICmdList,
			FeatureLevel,
			ShaderResource,
			LUT3dResource,
			InputResource,
			OutputResource);
	}
	);
	return true;
}


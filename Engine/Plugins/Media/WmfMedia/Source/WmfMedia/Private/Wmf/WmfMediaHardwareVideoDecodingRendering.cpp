// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaHardwareVideoDecodingRendering.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "MediaShaders.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "SceneInterface.h"
#include "ShaderParameterUtils.h"

#include "D3D11RHIPrivate.h"
#include "DynamicRHI.h"

#include "WmfMediaHardwareVideoDecodingTextureSample.h"
#include "WmfMediaPrivate.h"

#include "dxgi.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d11.h"
#include "Windows/HideWindowsPlatformTypes.h"

class FWmfMediaHardwareVideoDecodingShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FWmfMediaHardwareVideoDecodingShader() {}

	FWmfMediaHardwareVideoDecodingShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TextureY.Bind(Initializer.ParameterMap, TEXT("TextureY"));
		TextureUV.Bind(Initializer.ParameterMap, TEXT("TextureUV"));

		PointClampedSamplerY.Bind(Initializer.ParameterMap, TEXT("PointClampedSamplerY"));
		BilinearClampedSamplerUV.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSamplerUV"));
		
		ColorTransform.Bind(Initializer.ParameterMap, TEXT("ColorTransform"));
		SrgbToLinear.Bind(Initializer.ParameterMap, TEXT("SrgbToLinear"));
	}

	template<typename TShaderRHIParamRef>
	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		const TShaderRHIParamRef ShaderRHI,
		const FShaderResourceViewRHIRef& InTextureY,
		const FShaderResourceViewRHIRef& InTextureUV,
		const bool InIsOutputSrgb
		)
	{
		SetSRVParameter(RHICmdList, ShaderRHI, TextureY, InTextureY);
		SetSRVParameter(RHICmdList, ShaderRHI, TextureUV, InTextureUV);
		
		SetSamplerParameter(RHICmdList, ShaderRHI, PointClampedSamplerY, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSamplerUV, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetShaderValue(RHICmdList, ShaderRHI, ColorTransform, MediaShaders::CombineColorTransformAndOffset(MediaShaders::YuvToSrgbDefault, MediaShaders::YUVOffset8bits));
		SetShaderValue(RHICmdList, ShaderRHI, SrgbToLinear, InIsOutputSrgb);
	}


	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TextureY << TextureUV << PointClampedSamplerY << BilinearClampedSamplerUV << ColorTransform << SrgbToLinear;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter TextureY;
	FShaderResourceParameter TextureUV;
	FShaderResourceParameter PointClampedSamplerY;
	FShaderResourceParameter BilinearClampedSamplerUV;
	FShaderParameter ColorTransform;
	FShaderParameter SrgbToLinear;
};

class FHardwareVideoDecodingVS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingVS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingVS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{
	}
};

class FHardwareVideoDecodingPS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingPS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingPS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{ }
};

IMPLEMENT_SHADER_TYPE(, FHardwareVideoDecodingVS, TEXT("/Plugin/WmfMedia/Private/MediaHardwareVideoDecoding.usf"), TEXT("MainVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FHardwareVideoDecodingPS, TEXT("/Plugin/WmfMedia/Private/MediaHardwareVideoDecoding.usf"), TEXT("NV12ConvertPS"), SF_Pixel)

bool FWmfMediaHardwareVideoDecodingParameters::ConvertTextureFormat_RenderThread(FWmfMediaHardwareVideoDecodingTextureSample* InSample, FTexture2DRHIRef InDstTexture)
{
	if (InSample == nullptr || !InDstTexture.IsValid())
	{
		return false;
	}

	check(IsInRenderingThread());
	check(InSample);

	TComPtr<ID3D11Texture2D> SampleTexture = InSample->GetSourceTexture();

	ID3D11Device* D3D11Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	ID3D11DeviceContext* D3D11DeviceContext = nullptr;
			
	// Must access rendering device context to copy shared resource.
	D3D11Device->GetImmediateContext(&D3D11DeviceContext);
	if (D3D11DeviceContext)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		FRHIRenderPassInfo RPInfo(InDstTexture, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("ConvertTextureFormat"));

		// Update viewport.
		RHICmdList.SetViewport(0, 0, 0.f, InSample->GetDim().X, InSample->GetDim().Y, 1.f);

		// Get shaders.
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef< FHardwareVideoDecodingPS > PixelShader(GlobalShaderMap);
		TShaderMapRef< FHardwareVideoDecodingVS > VertexShader(GlobalShaderMap);

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

		FTexture2DRHIRef SampleDestinationTexture = InSample->GetDestinationTexture();
		if (!SampleDestinationTexture.IsValid())
		{
			FRHIResourceCreateInfo CreateInfo;
			const uint32 CreateFlags = TexCreate_Dynamic | TexCreate_DisableSRVCreation;
			FTexture2DRHIRef Texture = RHICreateTexture2D(
				InSample->GetDim().X,
				InSample->GetDim().Y,
				PF_NV12,
				1,
				1,
				CreateFlags,
				CreateInfo);

			InSample->SetDestinationTexture(Texture);
			SampleDestinationTexture = Texture;
		}

		ID3D11Resource* DestinationTexture = reinterpret_cast<ID3D11Resource*>(SampleDestinationTexture->GetNativeResource());
		if (DestinationTexture)
		{
			TComPtr<IDXGIResource> OtherResource(nullptr);
			SampleTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&OtherResource);

			if (OtherResource)
			{
				HANDLE SharedHandle = nullptr;
				if (OtherResource->GetSharedHandle(&SharedHandle) == S_OK)
				{
					if (SharedHandle != 0)
					{
						TComPtr<ID3D11Resource> SharedResource;
						D3D11Device->OpenSharedResource(SharedHandle, __uuidof(ID3D11Texture2D), (void**)&SharedResource);

						if (SharedResource)
						{
							TComPtr<IDXGIKeyedMutex> KeyedMutex;
							SharedResource->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);

							if (KeyedMutex)
							{
								// Key is 1 : Texture as just been updated
								// Key is 2 : Texture as already been updated.
								// Do not wait to acquire key 1 since there is race no condition between writer and reader.
								if (KeyedMutex->AcquireSync(1, 0) == S_OK)
								{
									// Copy from shared texture of FWmfMediaSink device to Rendering device
									D3D11DeviceContext->CopyResource(DestinationTexture, SharedResource);
									KeyedMutex->ReleaseSync(2);
								}
								else
								{
									// If key 1 cannot be acquired, another reader is already copying the resource
									// and will release key with 2. 
									// Wait to acquire key 2.
									if (KeyedMutex->AcquireSync(2, INFINITE) == S_OK)
									{
										KeyedMutex->ReleaseSync(2);
									}
								}
							}
						}
					}
				}
			}
		}

		FShaderResourceViewRHIRef Y_SRV = RHICreateShaderResourceView(SampleDestinationTexture, 0, 1, PF_G8);
		FShaderResourceViewRHIRef UV_SRV = RHICreateShaderResourceView(SampleDestinationTexture, 0, 1, PF_R8G8);

		// Update shader uniform parameters.
		VertexShader->SetParameters(RHICmdList, VertexShader->GetVertexShader(), Y_SRV, UV_SRV, InSample->IsOutputSrgb());
		PixelShader->SetParameters(RHICmdList, PixelShader->GetPixelShader(), Y_SRV, UV_SRV, InSample->IsOutputSrgb());
		RHICmdList.DrawPrimitive(0, 2, 1);
		RHICmdList.EndRenderPass();

		D3D11DeviceContext->Release();
	}

	return true;
}

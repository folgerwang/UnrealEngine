// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VisualizeTexture.cpp: Post processing visualize texture.
=============================================================================*/

#include "VisualizeTexture.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
//#include "HAL/FileManager.h"
//#include "Misc/FileHelper.h"
//#include "Misc/Paths.h"
//#include "Misc/App.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"


FVisualizeTexture::FVisualizeTexture()
{
	Mode = 0;
	RGBMul = 1.0f;
	SingleChannelMul = 0.0f;
	SingleChannel = -1;
	AMul = 0.0f;
	UVInputMapping = 3;
	Flags = 0;
	ObservedDebugNameReusedGoal = 0xffffffff;
	ArrayIndex = 0;
	CustomMip = 0;
	bSaveBitmap = false;
	bOutputStencil = false;
	bFullList = false;
	SortOrder = -1;
	bEnabled = true;
}


#if WITH_ENGINE

enum class EVisualisePSType
{
	Cube = 0,
	Texture1D = 1, //not supported
	Texture2DNoMSAA = 2,
	Texture3D = 3,
	CubeArray = 4,
	Texture2DMSAA = 5,
	Texture2DDepthStencilNoMSAA = 6,
	Texture2DUINT8 = 7,
	MAX
};

#endif

TGlobalResource<FVisualizeTexture> GVisualizeTexture;


#if WITH_ENGINE
/** A pixel shader which filters a texture. */
// @param TextureType 0:Cube, 1:1D(not yet supported), 2:2D no MSAA, 3:3D, 4:Cube[], 5:2D MSAA, 6:2D DepthStencil no MSAA (needed to avoid D3DDebug error)
class FVisualizeTexturePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeTexturePS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeTexturePS, FGlobalShader);

	class FVisualisePSTypeDim : SHADER_PERMUTATION_ENUM_CLASS("TEXTURE_TYPE", EVisualisePSType);

	using FPermutationDomain = TShaderPermutationDomain<FVisualisePSTypeDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return PermutationVector.Get<FVisualisePSTypeDim>() != EVisualisePSType::Texture1D;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector, TextureExtent)
		SHADER_PARAMETER_ARRAY(FVector4, VisualizeParam, [3])

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VisualizeTexture2D)
		SHADER_PARAMETER_SAMPLER(SamplerState, VisualizeTexture2DSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VisualizeTexture3D)
		SHADER_PARAMETER_SAMPLER(SamplerState, VisualizeTexture3DSampler)
		SHADER_PARAMETER_RDG_TEXTURE(TextureCube, VisualizeTextureCube)
		SHADER_PARAMETER_SAMPLER(SamplerState, VisualizeTextureCubeSampler)
		SHADER_PARAMETER_RDG_TEXTURE(TextureCubeArray, VisualizeTextureCubeArray)
		SHADER_PARAMETER_SAMPLER(SamplerState, VisualizeTextureCubeArraySampler)
		SHADER_PARAMETER_SRV(Texture2D<uint4>, VisualizeDepthStencilTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS<float4>, VisualizeTexture2DMS)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, VisualizeUINT8Texture2D)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeTexturePS, "/Engine/Private/Tools/VisualizeTexture.usf", "VisualizeTexturePS", SF_Pixel);


static EVisualisePSType GetVisualizePSType(const FRDGTextureDesc& Desc)
{
	if(Desc.Is2DTexture())
	{
		// 2D		
		if(Desc.NumSamples > 1)
		{
			// MSAA
			return EVisualisePSType::Texture2DMSAA;
		}
		else
		{
			if(Desc.Format == PF_DepthStencil)
			{
				// DepthStencil non MSAA (needed to avoid D3DDebug error)
				return EVisualisePSType::Texture2DDepthStencilNoMSAA;
			}
			else if (Desc.Format == PF_R8_UINT)
			{
				return EVisualisePSType::Texture2DUINT8;
			}
			else
			{
				// non MSAA
				return EVisualisePSType::Texture2DNoMSAA;
			}
		}
	}
	else if(Desc.IsCubemap())
	{		
		if(Desc.IsArray())
		{
			// Cube[]
			return EVisualisePSType::CubeArray;
		}
		else
		{
			// Cube
			return EVisualisePSType::Cube;
		}
	}

	check(Desc.Is3DTexture());
	return EVisualisePSType::Texture3D;
}

void FVisualizeTexture::ReleaseDynamicRHI()
{
	VisualizeTextureContent.SafeRelease();
	StencilSRV.SafeRelease();
}

void FVisualizeTexture::CreateContentCapturePass(FRDGBuilder& GraphBuilder, const FRDGTextureRef SrcTexture)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!SrcTexture || !SrcTexture->Desc.IsValid())
	{
		// todo: improve
		return;
	}

	const FRDGTextureDesc& SrcDesc = SrcTexture->Desc;

	if ((SrcDesc.Flags & TexCreate_CPUReadback))
	{
		// We cannot make a texture lookup on such elements
		return;
	}

	FRDGTextureRef CopyTexture;
	{
		FIntPoint Size = SrcTexture->Desc.Extent;

		// clamp to reasonable value to prevent crash
		Size.X = FMath::Max(Size.X, 1);
		Size.Y = FMath::Max(Size.Y, 1);

		FRDGTextureDesc CopyDesc = FRDGTextureDesc::Create2DDesc(
			Size, PF_B8G8R8A8, FClearValueBinding(FLinearColor(1, 1, 0, 1)),
			TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource, false);

		CopyTexture = GraphBuilder.CreateTexture(CopyDesc, TEXT("VisualizeTexture"));
	}

	FIntPoint RTExtent = SrcTexture->Desc.Extent;

	uint32 LocalVisualizeTextureInputMapping = UVInputMapping;

	if (!SrcDesc.Is2DTexture())
	{
		LocalVisualizeTextureInputMapping = 1;
	}

	// distinguish between standard depth and shadow depth to produce more reasonable default value mapping in the pixel shader.
	const bool bDepthTexture = (SrcDesc.TargetableFlags & TexCreate_DepthStencilTargetable) != 0;
	const bool bShadowDepth = (SrcDesc.Format == PF_ShadowDepth);

	bool bSaturateInsteadOfFrac = (Flags & 1) != 0;
	int32 InputValueMapping = bShadowDepth ? 2 : (bDepthTexture ? 1 : 0);

	FVisualizeTexturePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeTexturePS::FParameters>();
	{
		PassParameters->TextureExtent = FVector(SrcDesc.Extent.X, SrcDesc.Extent.Y, SrcDesc.Depth);

		{
			// alternates between 0 and 1 with a short pause
			const float FracTimeScale = 2.0f;
			float FracTime = FApp::GetCurrentTime() * FracTimeScale - floor(FApp::GetCurrentTime() * FracTimeScale);
			float BlinkState = (FracTime > 0.5f) ? 1.0f : 0.0f;

			FVector4 VisualizeParamValue[3];

			float Add = 0.0f;
			float FracScale = 1.0f;

			// w * almost_1 to avoid frac(1) => 0
			PassParameters->VisualizeParam[0] = FVector4(RGBMul, SingleChannelMul, Add, FracScale * 0.9999f);
			PassParameters->VisualizeParam[1] = FVector4(BlinkState, bSaturateInsteadOfFrac ? 1.0f : 0.0f, ArrayIndex, CustomMip);
			PassParameters->VisualizeParam[2] = FVector4(InputValueMapping, 0.0f, SingleChannel);
		}

		FSamplerStateRHIParamRef PointSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		PassParameters->VisualizeTexture2D = SrcTexture;
		PassParameters->VisualizeTexture2DSampler = PointSampler;
		PassParameters->VisualizeTexture3D = SrcTexture;
		PassParameters->VisualizeTexture3DSampler = PointSampler;
		PassParameters->VisualizeTextureCube = SrcTexture;
		PassParameters->VisualizeTextureCubeSampler = PointSampler;
		PassParameters->VisualizeTextureCubeArray = SrcTexture;
		PassParameters->VisualizeTextureCubeArraySampler = PointSampler;

		PassParameters->VisualizeDepthStencilTexture = nullptr; // TODO: StencilSRV
		PassParameters->VisualizeTexture2DMS = SrcTexture;
		PassParameters->VisualizeUINT8Texture2D = SrcTexture;

		PassParameters->RenderTargets[0] = FRenderTargetBinding(CopyTexture, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
	}

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	FVisualizeTexturePS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FVisualizeTexturePS::FVisualisePSTypeDim>(GetVisualizePSType(SrcDesc));

	TShaderMapRef<FVisualizeTexturePS> PixelShader(ShaderMap, PermutationVector);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("VisualizeTextureCapture(%s)", SrcTexture->Name),
		PassParameters,
		ERenderGraphPassFlags::None,
		[this, PassParameters, ShaderMap, PixelShader, RTExtent](FRHICommandList& RHICmdList)
	{
		FVisualizeTexturePS::FParameters ShaderParameter = *PassParameters;

		// TODO(RDG): technically could use FPixelShaderUtils::AddPass(), but there is lot of work to support creating arbitrary number of SRV for a FRDGTexture,
		// so the VisualizeDepthStencilTexture has to be hacked in the lambda...
		{
			// Some RHI might be unhappy with RHICreateShaderResourceView() inside renderpass.
			check(RHICmdList.IsInsideRenderPass());
			RHICmdList.EndRenderPass();
			check(RHICmdList.IsOutsideRenderPass());

			const FRDGTextureDesc& SrcDesc2 = PassParameters->VisualizeTexture2D->Desc;
			FSceneRenderTargetItem& RenderTargetItem = PassParameters->VisualizeTexture2D->GetPooledRenderTarget()->GetRenderTargetItem();

			bool bIsDefault = this->StencilSRVSrc == GBlackTexture->TextureRHI;
			bool bDepthStencil = SrcDesc2.Is2DTexture() && SrcDesc2.Format == PF_DepthStencil;

			//clear if this is a new different Stencil buffer, or it's not a stencil buffer and we haven't switched to the default yet.
			bool bNeedsClear = bDepthStencil && (this->StencilSRVSrc != RenderTargetItem.TargetableTexture);
			bNeedsClear |= !bDepthStencil && !bIsDefault;
			if (bNeedsClear)
			{
				this->StencilSRVSrc = nullptr;
				this->StencilSRV.SafeRelease();
			}

			//always set something into the StencilSRV slot for platforms that require a full resource binding, even if
			//dynamic branching will cause them not to be used.	
			if (bDepthStencil && !GVisualizeTexture.StencilSRVSrc)
			{
				this->StencilSRVSrc = RenderTargetItem.TargetableTexture;
				this->StencilSRV = RHICreateShaderResourceView((FTexture2DRHIRef&)RenderTargetItem.TargetableTexture, 0, 1, PF_X24_G8);
			}
			else if (!GVisualizeTexture.StencilSRVSrc)
			{
				this->StencilSRVSrc = GBlackTexture->TextureRHI;
				this->StencilSRV = RHICreateShaderResourceView((FTexture2DRHIRef&)GBlackTexture->TextureRHI, 0, 1, PF_B8G8R8A8);
			}

			ShaderParameter.VisualizeDepthStencilTexture = this->StencilSRV;

			// Rebind the render targets.
			FRHIRenderPassInfo RPInfo;
			RPInfo.ColorRenderTargets[0].RenderTarget = PassParameters->RenderTargets[0].GetTexture()->GetPooledRenderTarget()->GetRenderTargetItem().TargetableTexture;
			RPInfo.ColorRenderTargets[0].ResolveTarget = nullptr;
			RPInfo.ColorRenderTargets[0].ArraySlice = -1;
			RPInfo.ColorRenderTargets[0].MipIndex = 0;
			RPInfo.ColorRenderTargets[0].Action = MakeRenderTargetActions(ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);

			RHICmdList.BeginRenderPass(RPInfo, TEXT("VisualizeTextureCapture"));
			check(RHICmdList.IsInsideRenderPass());
		}

		FPixelShaderUtils::DrawFullscreenPixelShader(RHICmdList, ShaderMap, *PixelShader, ShaderParameter, FIntRect(0, 0, RTExtent.X, RTExtent.Y));
	});

	// Save the copied texture and descriptor about original informations.
	{
		VisualizeTextureDesc = SrcDesc;
		VisualizeTextureContent = nullptr;
		GraphBuilder.QueueTextureExtraction(CopyTexture, &VisualizeTextureContent);
	}

#if 0 // TODO(RDG): requires some kind of CPU readback pass.
	// save to disk
	if (bSaveBitmap)
	{
		bSaveBitmap = false;

		uint32 MipAdjustedExtentX = FMath::Clamp(Desc.Extent.X >> CustomMip, 0, Desc.Extent.X);
		uint32 MipAdjustedExtentY = FMath::Clamp(Desc.Extent.Y >> CustomMip, 0, Desc.Extent.Y);
		FIntPoint Extent(MipAdjustedExtentX, MipAdjustedExtentY);

		FReadSurfaceDataFlags ReadDataFlags;
		ReadDataFlags.SetLinearToGamma(false);
		ReadDataFlags.SetOutputStencil(bOutputStencil);
		ReadDataFlags.SetMip(CustomMip);

		FTextureRHIRef Texture = RenderTargetItem.TargetableTexture ? RenderTargetItem.TargetableTexture : RenderTargetItem.ShaderResourceTexture;

		check(Texture);

		TArray<FColor> Bitmap;



		RHICmdList.ReadSurfaceData(Texture, FIntRect(0, 0, Extent.X, Extent.Y), Bitmap, ReadDataFlags);

		// if the format and texture type is supported
		if (Bitmap.Num())
		{
			// Create screenshot folder if not already present.
			IFileManager::Get().MakeDirectory(*FPaths::ScreenShotDir(), true);

			const FString ScreenFileName(FPaths::ScreenShotDir() / TEXT("VisualizeTexture"));

			uint32 ExtendXWithMSAA = Bitmap.Num() / Extent.Y;

			// Save the contents of the array to a bitmap file. (24bit only so alpha channel is dropped)
			FFileHelper::CreateBitmap(*ScreenFileName, ExtendXWithMSAA, Extent.Y, Bitmap.GetData());

			UE_LOG(LogConsoleResponse, Display, TEXT("Content was saved to \"%s\""), *FPaths::ScreenShotDir());
		}
		else
		{
			UE_LOG(LogConsoleResponse, Error, TEXT("Failed to save BMP for VisualizeTexture, format or texture type is not supported"));
		}
	}
#endif


#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool FVisualizeTexture::ShouldCapture(const TCHAR* DebugName)
{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return false;
#else
	if (!bEnabled)
	{
		return false;
	}


	uint32* UsageCountPtr = VisualizeTextureCheckpoints.Find(DebugName);

	if (!UsageCountPtr)
	{
		// create a new element with count 0
		UsageCountPtr = &VisualizeTextureCheckpoints.Add(DebugName, 0);
	}

	// is this is the name we are observing with visualize texture?
	// First check if we need to find anything to avoid string the comparison
	if (!ObservedDebugName.IsEmpty() && ObservedDebugName == DebugName)
	{
		// if multiple times reused during the frame, is that the one we want to look at?
		if (*UsageCountPtr == ObservedDebugNameReusedGoal || ObservedDebugNameReusedGoal == 0xffffffff)
		{
			*UsageCountPtr = *UsageCountPtr + 1;
			return true;
		}
	}
	// only needed for VisualizeTexture (todo: optimize out when possible)
	*UsageCountPtr = *UsageCountPtr + 1;
	return false;
#endif
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void FVisualizeTexture::SetCheckPoint(FRHICommandList& RHICmdList, const IPooledRenderTarget* PooledRenderTarget)
{
	check(IsInRenderingThread());
	if (!PooledRenderTarget)
	{
		return;
	}

	const TCHAR* DebugName = PooledRenderTarget->GetDesc().DebugName;

	if (!ShouldCapture(DebugName))
	{
		return;
	}

	FRHICommandListImmediate& RHICmdListIm = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdListIm.IsExecuting())
	{
		UE_LOG(LogConsoleResponse, Fatal, TEXT("We can't create a checkpoint because that requires the immediate commandlist, which is currently executing. You might try disabling parallel rendering."));
	}

	if (&RHICmdList != &RHICmdListIm)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Attempt to checkpoint a render target from a non-immediate command list. We will flush it and hope that works. If it doesn't you might try disabling parallel rendering."));
		RHICmdList.Flush();
	}

	FRDGBuilder GraphBuilder(RHICmdListIm);

	// Sorry for the const cast here only required for reference count of the pooled render target the graph needs to do.
	// Long therm this SetCheckPoint() method should no longer since it is done exclusively by render graph automatically. 
	TRefCountPtr<IPooledRenderTarget> PooledRenderTargetRef(const_cast<IPooledRenderTarget*>(PooledRenderTarget));
	FRDGTextureRef TextureToCapture = GraphBuilder.RegisterExternalTexture(PooledRenderTargetRef, DebugName);

	CreateContentCapturePass(GraphBuilder, TextureToCapture);
	GraphBuilder.Execute();

	if (&RHICmdList != &RHICmdListIm)
	{
		RHICmdListIm.Flush();
	}
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void FVisualizeTexture::QueryInfo_GameThread(FQueryVisualizeTexureInfo& Out)
{
	check(IsInGameThread());
	FlushRenderingCommands();

	for(uint32 i = 0, Num = GRenderTargetPool.GetElementCount(); i < Num; ++i)
	{
		FPooledRenderTarget* RT = GRenderTargetPool.GetElementById(i);

		if(!RT)
		{
			continue;
		}

		FPooledRenderTargetDesc Desc = RT->GetDesc();
		uint32 SizeInKB = (RT->ComputeMemorySize() + 1023) / 1024;
		FString Entry = FString::Printf(TEXT("%s %d %s %d"),
				*Desc.GenerateInfoString(),
				i + 1,
				Desc.DebugName ? Desc.DebugName : TEXT("<Unnamed>"),
				SizeInKB);
		Out.Entries.Add(Entry);
	}
}

void FVisualizeTexture::SetRenderTargetNameToObserve(const FString& InObservedDebugName, uint32 InObservedDebugNameReusedGoal)
{
	ObservedDebugName = InObservedDebugName;
	ObservedDebugNameReusedGoal = InObservedDebugNameReusedGoal;
}

#endif // WITH_ENGINE

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RendererUtils.h"
#include "RenderTargetPool.h"

class FRTWriteMaskDecodeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRTWriteMaskDecodeCS, Global);

public:
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FRTWriteMaskDecodeCS() {}

	FShaderParameter RTWriteMaskDimensions;
	FShaderParameter OutCombinedRTWriteMask;	// UAV
	FShaderResourceParameter RTWriteMaskInput0;	// SRV 

	FRTWriteMaskDecodeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		RTWriteMaskDimensions.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskDimensions"));
		OutCombinedRTWriteMask.Bind(Initializer.ParameterMap, TEXT("OutCombinedRTWriteMask"));
		RTWriteMaskInput0.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput0"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << RTWriteMaskDimensions;
		Ar << OutCombinedRTWriteMask;
		Ar << RTWriteMaskInput0;
		return bShaderHasOutdatedParameters;
	}

	virtual void SetParameters(FRHICommandListImmediate& RHICmdList, FIntPoint RTWriteMaskDims, const TArray<TRefCountPtr<IPooledRenderTarget> >& InRenderTargets)
	{
		check(InRenderTargets.Num() >= 1);
		SetShaderValue(RHICmdList, GetComputeShader(), RTWriteMaskDimensions, RTWriteMaskDims);
		SetSRVParameter(RHICmdList, GetComputeShader(), RTWriteMaskInput0, InRenderTargets[0]->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
	}
};

IMPLEMENT_SHADER_TYPE(, FRTWriteMaskDecodeCS, TEXT("/Engine/Private/RTWriteMaskDecode.usf"), TEXT("RTWriteMaskDecodeSingleMain"), SF_Compute);

class FRTWriteMaskCombineCS : public FRTWriteMaskDecodeCS
{
	DECLARE_SHADER_TYPE(FRTWriteMaskCombineCS, Global);

public:

	FRTWriteMaskCombineCS() {}

	FShaderResourceParameter RTWriteMaskInput1;	// SRV 
	FShaderResourceParameter RTWriteMaskInput2;	// SRV 

	FRTWriteMaskCombineCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FRTWriteMaskDecodeCS(Initializer)
	{
		RTWriteMaskInput1.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput1"));
		RTWriteMaskInput2.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput2"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FRTWriteMaskDecodeCS::Serialize(Ar);
		Ar << RTWriteMaskInput1;
		Ar << RTWriteMaskInput2;
		return bShaderHasOutdatedParameters;
	}

	virtual void SetParameters(FRHICommandListImmediate& RHICmdList, FIntPoint RTWriteMaskDims, const TArray<TRefCountPtr<IPooledRenderTarget> >& InRenderTargets) override
	{
		check(InRenderTargets.Num() >= 3);
		FRTWriteMaskDecodeCS::SetParameters(RHICmdList, RTWriteMaskDims, InRenderTargets);
		SetSRVParameter(RHICmdList, GetComputeShader(), RTWriteMaskInput1, InRenderTargets[1]->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		SetSRVParameter(RHICmdList, GetComputeShader(), RTWriteMaskInput2, InRenderTargets[2]->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
	}
};

IMPLEMENT_SHADER_TYPE(, FRTWriteMaskCombineCS, TEXT("/Engine/Private/RTWriteMaskDecode.usf"), TEXT("RTWriteMaskCombineMain"), SF_Compute);

void FRenderTargetWriteMask::Decode(FRHICommandListImmediate& RHICmdList, TShaderMap<FGlobalShaderType>* ShaderMap, const TArray<TRefCountPtr<IPooledRenderTarget> >& InRenderTargets, TRefCountPtr<IPooledRenderTarget>& OutRTWriteMask, uint32 RTWriteMaskFastVRamConfig, const TCHAR* RTWriteMaskDebugName)
{
	// @todo: get these values from the RHI?
	const uint32 MaskTileSizeX = 8;
	const uint32 MaskTileSizeY = 8;

	check(GSupportsRenderTargetWriteMask);
	checkf(InRenderTargets.Num() == 1 || InRenderTargets.Num() == 3, TEXT("Unsupported number of write masks (%d)"), InRenderTargets.Num());

	FTextureRHIRef RenderTarget0Texture = InRenderTargets[0]->GetRenderTargetItem().TargetableTexture;

	FIntPoint RTWriteMaskDims(
		FMath::DivideAndRoundUp(RenderTarget0Texture->GetTexture2D()->GetSizeX(), MaskTileSizeX),
		FMath::DivideAndRoundUp(RenderTarget0Texture->GetTexture2D()->GetSizeY(), MaskTileSizeY));
	
	// The combine shader can fit two RT masks per pixel.
	FIntPoint CombinedRTWriteMaskDims(RTWriteMaskDims.X * FMath::DivideAndRoundUp(InRenderTargets.Num(), 2), RTWriteMaskDims.Y);

	// allocate the Mask from the render target pool.
	FPooledRenderTargetDesc MaskDesc(FPooledRenderTargetDesc::Create2DDesc(CombinedRTWriteMaskDims,
		PF_R8_UINT,
		FClearValueBinding::White,
		TexCreate_None | RTWriteMaskFastVRamConfig,
		TexCreate_UAV | TexCreate_RenderTargetable,
		false));

	GRenderTargetPool.FindFreeElement(RHICmdList, MaskDesc, OutRTWriteMask, RTWriteMaskDebugName);

	FRTWriteMaskDecodeCS* ComputeShader = nullptr;
	if (InRenderTargets.Num() > 1)
	{
		ComputeShader = dynamic_cast<FRTWriteMaskDecodeCS*>(*TShaderMapRef<FRTWriteMaskCombineCS>(ShaderMap));
	}
	else
	{
		ComputeShader = *TShaderMapRef<FRTWriteMaskDecodeCS>(ShaderMap);
	}

	check(ComputeShader != nullptr);
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutCombinedRTWriteMask.GetBaseIndex(), OutRTWriteMask->GetRenderTargetItem().UAV);
	ComputeShader->SetParameters(RHICmdList, RTWriteMaskDims, InRenderTargets);

	const FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, OutRTWriteMask->GetRenderTargetItem().UAV);
	{
		FIntPoint ThreadGroupCountValue(
			FMath::DivideAndRoundUp((uint32)RTWriteMaskDims.X, FRTWriteMaskCombineCS::ThreadGroupSizeX),
			FMath::DivideAndRoundUp((uint32)RTWriteMaskDims.Y, FRTWriteMaskCombineCS::ThreadGroupSizeY));

		DispatchComputeShader(RHICmdList, ComputeShader, ThreadGroupCountValue.X, ThreadGroupCountValue.Y, 1);
	}

	//	void FD3D11DynamicRHI::RHIGraphicsWaitOnAsyncComputeJob( uint32 FenceIndex )
	RHICmdList.FlushComputeShaderCache();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutRTWriteMask->GetRenderTargetItem().UAV);

	// un-set destination
	RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutCombinedRTWriteMask.GetBaseIndex(), NULL);
}
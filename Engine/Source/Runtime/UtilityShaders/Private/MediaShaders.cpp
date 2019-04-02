// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaShaders.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"


TGlobalResource<FMediaVertexDeclaration> GMediaVertexDeclaration;


/* MediaShaders namespace
 *****************************************************************************/

namespace MediaShaders
{
	const FMatrix YuvToSrgbJpeg = FMatrix(
		FPlane(1.000000f,  0.000000f,  1.402000f, 0.000000f),
		FPlane(1.000000f, -0.344140f, -0.714140f, 0.000000f),
		FPlane(1.000000f,  1.772000f,  0.000000f, 0.000000f),
		FPlane(0.000000f,  0.000000f,  0.000000f, 0.000000f)
	);

	const FMatrix YuvToSrgbDefault = FMatrix(
		FPlane(1.164383f,  0.000000f,  1.596027f, 0.000000f),
		FPlane(1.164383f, -0.391762f, -0.812968f, 0.000000f),
		FPlane(1.164383f,  2.017232f,  0.000000f, 0.000000f),
		FPlane(0.000000f,  0.000000f,  0.000000f, 0.000000f)
	);

	const FMatrix YuvToSrgbPs4 = FMatrix(
		FPlane(1.164400f,  0.000000f,  1.792700f, 0.000000f),
		FPlane(1.164400f, -0.213300f, -0.532900f, 0.000000f),
		FPlane(1.164400f,  2.112400f,  0.000000f, 0.000000f),
		FPlane(0.000000f,  0.000000f,  0.000000f, 0.000000f)
	);

	const FMatrix YuvToSrgbRec601 = FMatrix(
		FPlane(1.000000f, 0.000000f, 1.139830f, 0.000000f),
		FPlane(1.000000f, -0.394650f, -0.580600f, 0.000000f),
		FPlane(1.000000f, 2.032110, 0.000000, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec709 = FMatrix(
		FPlane(1.000000f, 0.000000f, 1.280330f, 0.000000f),
		FPlane(1.000000f, -0.214820f, -0.380590f, 0.000000f),
		FPlane(1.000000f, 2.127980f, 0.000000f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec709Full = FMatrix(
		FPlane(1.164400f, 0.000000f, 1.792700f, 0.000000f),
		FPlane(1.164400f, -0.213300f, -0.532900f, 0.000000f),
		FPlane(1.164400f, 2.112400f, 0.000000f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FMatrix RgbToYuvRec709Full = FMatrix(
		FPlane(0.182581f, 0.614210f, 0.062020f, 0.000000f),
		FPlane(-0.100642f, -0.338566f, 0.439208f, 0.000000f),
		FPlane(0.439227f, -0.398944f, -0.040283f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FVector YUVOffset8bits = FVector(0.06274509803921568627f, 0.5019607843137254902f, 0.5019607843137254902f);

	const FVector YUVOffset10bits = FVector(0.06256109481915933529f, 0.50048875855327468231f, 0.50048875855327468231f);

	/** Setup YUV Offset in matrix */
	FMatrix CombineColorTransformAndOffset(const FMatrix& InMatrix, const FVector& InYUVOffset)
	{
		FMatrix Result = InMatrix;
		// Offset in last column:
		// 1) to allow for 4x4 matrix multiplication optimization when going from RGB to YUV (hence the 1.0 in the [3][3] matrix location)
		// 2) stored in empty space when going from YUV to RGB
		Result.M[0][3] = InYUVOffset.X;
		Result.M[1][3] = InYUVOffset.Y;
		Result.M[2][3] = InYUVOffset.Z;
		Result.M[3][3] = 1.0f;
		return Result;
	}
}

/* FMediaShadersVS shader
 *****************************************************************************/

IMPLEMENT_SHADER_TYPE(, FMediaShadersVS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("MainVertexShader"), SF_Vertex);


/* FAYUVConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAYUVConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FAYUVConvertUB, "AYUVConvertUB");
IMPLEMENT_SHADER_TYPE(, FAYUVConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("AYUVConvertPS"), SF_Pixel);


void FAYUVConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> AYUVTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FAYUVConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = AYUVTexture;
	}

	TUniformBufferRef<FAYUVConvertUB> Data = TUniformBufferRef<FAYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FAYUVConvertUB>(), Data);
}


/* FRGBConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FBMPConvertUB, )
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2D, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FBMPConvertUB, "BMPConvertUB");
IMPLEMENT_SHADER_TYPE(, FBMPConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("BMPConvertPS"), SF_Pixel);


void FBMPConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> BMPTexture, const FIntPoint& OutputDimensions, bool SrgbToLinear)
{
	FBMPConvertUB UB;
	{
		UB.SrgbToLinear = SrgbToLinear;
		UB.UVScale = FVector2D((float)OutputDimensions.X / (float)BMPTexture->GetSizeX(), (float)OutputDimensions.Y / (float)BMPTexture->GetSizeY());
		UB.Texture = BMPTexture;
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FBMPConvertUB> Data = TUniformBufferRef<FBMPConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FBMPConvertUB>(), Data);
}


/* FNV12ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNV12ConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2D, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNV12ConvertUB, "NV12ConvertUB");
IMPLEMENT_SHADER_TYPE(, FNV12ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV12ConvertPS"), SF_Pixel);


void FNV12ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> NV12Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FNV12ConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.X;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = NV12Texture;
		UB.UVScale = FVector2D((float)OutputDimensions.X / (float)NV12Texture->GetSizeX(), (float)OutputDimensions.Y / (float)NV12Texture->GetSizeY());
	}

	TUniformBufferRef<FNV12ConvertUB> Data = TUniformBufferRef<FNV12ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FNV12ConvertUB>(), Data);
}


/* FNV21ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNV21ConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2D, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNV21ConvertUB, "NV21ConvertUB");
IMPLEMENT_SHADER_TYPE(, FNV21ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV21ConvertPS"), SF_Pixel);


void FNV21ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> NV21Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FNV21ConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.Y;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = NV21Texture;
		UB.UVScale = FVector2D((float)OutputDimensions.X / (float)NV21Texture->GetSizeX(), (float)OutputDimensions.Y / (float)NV21Texture->GetSizeY());
	}

	TUniformBufferRef<FNV21ConvertUB> Data = TUniformBufferRef<FNV21ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FNV21ConvertUB>(), Data);
}


/* FRGBConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRGBConvertUB, )
SHADER_PARAMETER(FVector2D, UVScale)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRGBConvertUB, "RGBConvertUB");
IMPLEMENT_SHADER_TYPE(, FRGBConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("RGBConvertPS"), SF_Pixel);


void FRGBConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBTexture, const FIntPoint& OutputDimensions, bool bSrgbToLinear)
{
	FRGBConvertUB UB;
	{
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SrgbToLinear = bSrgbToLinear;
		UB.Texture = RGBTexture;
		UB.UVScale = FVector2D((float)OutputDimensions.X / (float)RGBTexture->GetSizeX(), (float)OutputDimensions.Y / (float)RGBTexture->GetSizeY());
	}

	TUniformBufferRef<FRGBConvertUB> Data = TUniformBufferRef<FRGBConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FRGBConvertUB>(), Data);
}


/* FYCbCrConvertUB shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYCbCrConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_TEXTURE(Texture2D, LumaTexture)
SHADER_PARAMETER_TEXTURE(Texture2D, CbCrTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, LumaSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, CbCrSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYCbCrConvertUB, "YCbCrConvertUB");
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YCbCrConvertPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS_4x4Matrix, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YCbCrConvertPS_4x4Matrix"), SF_Pixel);


void FYCbCrConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> LumaTexture, TRefCountPtr<FRHITexture2D> CbCrTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYCbCrConvertUB UB;
	{
		// Chroma is not usually 1:1 with the output textxure
		UB.CbCrSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.CbCrTexture = CbCrTexture;
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		
		// Luma should be 1:1 with the output texture and needs to be point sampled
		UB.LumaSampler = TStaticSamplerState<SF_Point>::GetRHI();
		UB.LumaTexture = LumaTexture;
		UB.SrgbToLinear = SrgbToLinear;
	}

	TUniformBufferRef<FYCbCrConvertUB> Data = TUniformBufferRef<FYCbCrConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYCbCrConvertUB>(), Data);	
}


/* FUYVYConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FUYVYConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(uint32, Width)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FUYVYConvertUB, "UYVYConvertUB");
IMPLEMENT_SHADER_TYPE(, FUYVYConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("UYVYConvertPS"), SF_Pixel);


void FUYVYConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> UYVYTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FUYVYConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = UYVYTexture;
		UB.Width = UYVYTexture->GetSizeX();
	}

	TUniformBufferRef<FUYVYConvertUB> Data = TUniformBufferRef<FUYVYConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FUYVYConvertUB>(), Data);
}


/* FYUVConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2D, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, YTexture)
SHADER_PARAMETER_TEXTURE(Texture2D, UTexture)
SHADER_PARAMETER_TEXTURE(Texture2D, VTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, YSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, USampler)
SHADER_PARAMETER_SAMPLER(SamplerState, VSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVConvertUB, "YUVConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUVConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUVConvertPS"), SF_Pixel);


void FYUVConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YTexture, TRefCountPtr<FRHITexture2D> UTexture, TRefCountPtr<FRHITexture2D> VTexture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUVConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SrgbToLinear = SrgbToLinear;
		UB.YTexture = YTexture;
		UB.UTexture = UTexture;
		UB.VTexture = VTexture;
		UB.YSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.USampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.VSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.UVScale = FVector2D((float) OutputDimensions.X / (float) YTexture->GetSizeX(), (float) OutputDimensions.Y / (float) YTexture->GetSizeY());
	}

	TUniformBufferRef<FYUVConvertUB> Data = TUniformBufferRef<FYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYUVConvertUB>(), Data);
}


/* FYUVv210ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVv210ConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(uint32, OutputDimX)
SHADER_PARAMETER(uint32, OutputDimY)
SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, YUVTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVv210ConvertUB, "YUVv210ConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUVv210ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUVv210ConvertPS"), SF_Pixel);


void FYUVv210ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YUVTexture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUVv210ConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SrgbToLinear = SrgbToLinear;
		UB.OutputDimX = OutputDimensions.X;
		UB.OutputDimY = OutputDimensions.Y;
		UB.YUVTexture = YUVTexture;
	}

	TUniformBufferRef<FYUVv210ConvertUB> Data = TUniformBufferRef<FYUVv210ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYUVv210ConvertUB>(), Data);
}


/* FYUY2ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUY2ConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2D, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUY2ConvertUB, "YUY2ConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUY2ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUY2ConvertPS"), SF_Pixel);


void FYUY2ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YUY2Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUY2ConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.X;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = YUY2Texture;
		UB.UVScale = FVector2D((float)OutputDimensions.X / (2.0f * YUY2Texture->GetSizeX()), (float)OutputDimensions.Y / (float)YUY2Texture->GetSizeY());
	}

	TUniformBufferRef<FYUY2ConvertUB> Data = TUniformBufferRef<FYUY2ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYUY2ConvertUB>(), Data);
}


/* FYVYUConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYVYUConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(uint32, Width)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYVYUConvertUB, "YVYUConvertUB");
IMPLEMENT_SHADER_TYPE(, FYVYUConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YVYUConvertPS"), SF_Pixel);


void FYVYUConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YVYUTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYVYUConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = YVYUTexture;
		UB.Width = YVYUTexture->GetSizeX();
	}

	TUniformBufferRef<FYVYUConvertUB> Data = TUniformBufferRef<FYVYUConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYVYUConvertUB>(), Data);
}


/* FRGB8toUYVY8ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRGB8toUYVY8ConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, LinearToSrgb)
SHADER_PARAMETER(float, OnePixelDeltaX)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRGB8toUYVY8ConvertUB, "RGB8toUYVY8ConvertUB");
IMPLEMENT_SHADER_TYPE(, FRGB8toUYVY8ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("RGB8toUYVY8ConvertPS"), SF_Pixel);


void FRGB8toUYVY8ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBATexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool LinearToSrgb)
{
	FRGB8toUYVY8ConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.LinearToSrgb = LinearToSrgb;
		UB.Texture = RGBATexture;
		UB.OnePixelDeltaX = 1.0f / float(RGBATexture->GetSizeX());
	}

	TUniformBufferRef<FRGB8toUYVY8ConvertUB> Data = TUniformBufferRef<FRGB8toUYVY8ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FRGB8toUYVY8ConvertUB>(), Data);
}


/* FRGB10toYUVv210ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRGB10toYUVv210ConvertUB, )
SHADER_PARAMETER(FMatrix, ColorTransform)
SHADER_PARAMETER(uint32, LinearToSrgb)
SHADER_PARAMETER(float, OnePixelDeltaX)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRGB10toYUVv210ConvertUB, "RGB10toYUVv210ConvertUB");
IMPLEMENT_SHADER_TYPE(, FRGB10toYUVv210ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("RGB10toYUVv210ConvertPS"), SF_Pixel);


void FRGB10toYUVv210ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBATexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool LinearToSrgb)
{
	FRGB10toYUVv210ConvertUB UB;
	{
		UB.ColorTransform = MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.LinearToSrgb = LinearToSrgb;
		UB.Texture = RGBATexture;
		UB.OnePixelDeltaX = 1.0f / float(RGBATexture->GetSizeX());
	}

	TUniformBufferRef<FRGB10toYUVv210ConvertUB> Data = TUniformBufferRef<FRGB10toYUVv210ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FRGB10toYUVv210ConvertUB>(), Data);
}


/* FInvertAlphaPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FInvertAlphaUB, )
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FInvertAlphaUB, "InvertAlphaUB");
IMPLEMENT_SHADER_TYPE(, FInvertAlphaPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("InvertAlphaPS"), SF_Pixel);


void FInvertAlphaPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBATexture)
{
	FInvertAlphaUB UB;
	{
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.Texture = RGBATexture;
	}

	TUniformBufferRef<FInvertAlphaUB> Data = TUniformBufferRef<FInvertAlphaUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FInvertAlphaUB>(), Data);
}


/* FSetAlphaOnePS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSetAlphaOneUB, )
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSetAlphaOneUB, "SetAlphaOneUB");
IMPLEMENT_SHADER_TYPE(, FSetAlphaOnePS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("SetAlphaOnePS"), SF_Pixel);


void FSetAlphaOnePS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBATexture)
{
	FSetAlphaOneUB UB;
	{
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.Texture = RGBATexture;
	}

	TUniformBufferRef<FSetAlphaOneUB> Data = TUniformBufferRef<FSetAlphaOneUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FSetAlphaOneUB>(), Data);
}

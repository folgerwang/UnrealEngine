// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
}


/* FMediaShadersVS shader
 *****************************************************************************/

IMPLEMENT_SHADER_TYPE(, FMediaShadersVS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("MainVertexShader"), SF_Vertex);


/* FAYUVConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FAYUVConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, Sampler)
END_UNIFORM_BUFFER_STRUCT(FAYUVConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FAYUVConvertUB, TEXT("AYUVConvertUB"));
IMPLEMENT_SHADER_TYPE(, FAYUVConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("AYUVConvertPS"), SF_Pixel);


void FAYUVConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> AYUVTexture, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FAYUVConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = AYUVTexture;
	}

	TUniformBufferRef<FAYUVConvertUB> Data = TUniformBufferRef<FAYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FAYUVConvertUB>(), Data);
}


/* FRGBConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FBMPConvertUB, )
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER(FVector2D, UVScale)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, Sampler)
END_UNIFORM_BUFFER_STRUCT(FBMPConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBMPConvertUB, TEXT("BMPConvertUB"));
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

BEGIN_UNIFORM_BUFFER_STRUCT(FNV12ConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, OutputWidth)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER(FVector2D, UVScale)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerB)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerP)
END_UNIFORM_BUFFER_STRUCT(FNV12ConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNV12ConvertUB, TEXT("NV12ConvertUB"));
IMPLEMENT_SHADER_TYPE(, FNV12ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV12ConvertPS"), SF_Pixel);


void FNV12ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> NV12Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FNV12ConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
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

BEGIN_UNIFORM_BUFFER_STRUCT(FNV21ConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, OutputWidth)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER(FVector2D, UVScale)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerB)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerP)
END_UNIFORM_BUFFER_STRUCT(FNV21ConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNV21ConvertUB, TEXT("NV21ConvertUB"));
IMPLEMENT_SHADER_TYPE(, FNV21ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV21ConvertPS"), SF_Pixel);


void FNV21ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> NV21Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FNV21ConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
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

BEGIN_UNIFORM_BUFFER_STRUCT(FRGBConvertUB, )
UNIFORM_MEMBER(FVector2D, UVScale)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, Sampler)
END_UNIFORM_BUFFER_STRUCT(FRGBConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FRGBConvertUB, TEXT("RGBConvertUB"));
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

BEGIN_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER_TEXTURE(Texture2D, LumaTexture)
UNIFORM_MEMBER_TEXTURE(Texture2D, CbCrTexture)
UNIFORM_MEMBER_SAMPLER(SamplerState, LumaSampler)
UNIFORM_MEMBER_SAMPLER(SamplerState, CbCrSampler)
END_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB, TEXT("YCbCrConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YCbCrConvertPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS_4x4Matrix, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YCbCrConvertPS_4x4Matrix"), SF_Pixel);


void FYCbCrConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> LumaTexture, TRefCountPtr<FRHITexture2D> CbCrTexture, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FYCbCrConvertUB UB;
	{
		UB.CbCrSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.CbCrTexture = CbCrTexture;
		UB.ColorTransform = ColorTransform;
		UB.LumaSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.LumaTexture = LumaTexture;
		UB.SrgbToLinear = SrgbToLinear;
	}

	TUniformBufferRef<FYCbCrConvertUB> Data = TUniformBufferRef<FYCbCrConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYCbCrConvertUB>(), Data);	
}


/* FUYVYConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FUYVYConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER(uint32, Width)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerB)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerP)
END_UNIFORM_BUFFER_STRUCT(FUYVYConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FUYVYConvertUB, TEXT("UYVYConvertUB"));
IMPLEMENT_SHADER_TYPE(, FUYVYConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("UYVYConvertPS"), SF_Pixel);


void FUYVYConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> UYVYTexture, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FUYVYConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
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

BEGIN_UNIFORM_BUFFER_STRUCT(FYUVConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER_TEXTURE(Texture2D, YTexture)
UNIFORM_MEMBER_TEXTURE(Texture2D, UTexture)
UNIFORM_MEMBER_TEXTURE(Texture2D, VTexture)
UNIFORM_MEMBER_SAMPLER(SamplerState, YSampler)
UNIFORM_MEMBER_SAMPLER(SamplerState, USampler)
UNIFORM_MEMBER_SAMPLER(SamplerState, VSampler)
END_UNIFORM_BUFFER_STRUCT(FYUVConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYUVConvertUB, TEXT("YUVConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYUVConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUVConvertPS"), SF_Pixel);


void FYUVConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YTexture, TRefCountPtr<FRHITexture2D> UTexture, TRefCountPtr<FRHITexture2D> VTexture, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FYUVConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
		UB.SrgbToLinear = SrgbToLinear;
		UB.YTexture = YTexture;
		UB.UTexture = UTexture;
		UB.VTexture = VTexture;
		UB.YSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.USampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.VSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FYUVConvertUB> Data = TUniformBufferRef<FYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYUVConvertUB>(), Data);
}


/* FYUY2ConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FYUY2ConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, OutputWidth)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER(FVector2D, UVScale)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerB)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerP)
END_UNIFORM_BUFFER_STRUCT(FYUY2ConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYUY2ConvertUB, TEXT("YUY2ConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYUY2ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUY2ConvertPS"), SF_Pixel);


void FYUY2ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YUY2Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FYUY2ConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
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

BEGIN_UNIFORM_BUFFER_STRUCT(FYVYUConvertUB, )
UNIFORM_MEMBER(FMatrix, ColorTransform)
UNIFORM_MEMBER(uint32, SrgbToLinear)
UNIFORM_MEMBER(uint32, Width)
UNIFORM_MEMBER_TEXTURE(Texture2D, Texture)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerB)
UNIFORM_MEMBER_SAMPLER(SamplerState, SamplerP)
END_UNIFORM_BUFFER_STRUCT(FYVYUConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYVYUConvertUB, TEXT("YVYUConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYVYUConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YVYUConvertPS"), SF_Pixel);


void FYVYUConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YVYUTexture, const FMatrix& ColorTransform, bool SrgbToLinear)
{
	FYVYUConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = YVYUTexture;
		UB.Width = YVYUTexture->GetSizeX();
	}

	TUniformBufferRef<FYVYUConvertUB> Data = TUniformBufferRef<FYVYUConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, GetPixelShader(), GetUniformBufferParameter<FYVYUConvertUB>(), Data);
}

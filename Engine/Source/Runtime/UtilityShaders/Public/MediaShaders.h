// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"


namespace MediaShaders
{
	/** Color transform from YUV to sRGB (using values from MSDN). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbDefault;

	/** Color transform from YUV to sRGB (in JPEG color space). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbJpeg;

	/** Color transform from YUV to sRGB (using values from PS4 AvPlayer codec). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbPs4;

	/** Color transform from YUV to sRGB (in Rec. 601 color space). */
	UTILITYSHADERS_API extern const FMatrix YuvToSrgbRec601;

	/** Color transform from YUV to sRGB (in Rec. 709 color space). */
	UTILITYSHADERS_API extern const FMatrix YuvToRgbRec709;

	/** Color transform from YUV to RGB (in Rec. 709 color space, RGB full range) */
	UTILITYSHADERS_API extern const FMatrix YuvToRgbRec709Full;

	/** Color transform from RGB to YUV (in Rec. 709 color space, RGB full range) */
	UTILITYSHADERS_API extern const FMatrix RgbToYuvRec709Full;

	/** YUV Offset for 8 bit conversion (Computed as 16/255, 128/255, 128/255) */
	UTILITYSHADERS_API extern const FVector YUVOffset8bits;

	/** YUV Offset for 10 bit conversion (Computed as 64/1023, 512/1023, 512/1023) */
	UTILITYSHADERS_API extern const FVector YUVOffset10bits;

	/** Combine color transform matrix with yuv offset in a single matrix */
	UTILITYSHADERS_API FMatrix CombineColorTransformAndOffset(const FMatrix& InMatrix, const FVector& InYUVOffset);
}


/**
 * Stores media drawing vertices.
 */
struct FMediaElementVertex
{
	FVector4 Position;
	FVector2D TextureCoordinate;

	FMediaElementVertex() { }

	FMediaElementVertex(const FVector4& InPosition, const FVector2D& InTextureCoordinate)
		: Position(InPosition)
		, TextureCoordinate(InTextureCoordinate)
	{ }
};

inline FVertexBufferRHIRef CreateTempMediaVertexBuffer(float ULeft = 0.0f, float URight = 1.0f, float VTop = 0.0f, float VBottom = 1.0f)
{
	FRHIResourceCreateInfo CreateInfo;
	FVertexBufferRHIRef VertexBufferRHI = RHICreateVertexBuffer(sizeof(FMediaElementVertex) * 4, BUF_Volatile, CreateInfo);
	void* VoidPtr = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FMediaElementVertex) * 4, RLM_WriteOnly);

	FMediaElementVertex* Vertices = (FMediaElementVertex*)VoidPtr;
	Vertices[0].Position.Set(-1.0f, 1.0f, 1.0f, 1.0f); // Top Left
	Vertices[1].Position.Set(1.0f, 1.0f, 1.0f, 1.0f); // Top Right
	Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f); // Bottom Left
	Vertices[3].Position.Set(1.0f, -1.0f, 1.0f, 1.0f); // Bottom Right

	Vertices[0].TextureCoordinate.Set(ULeft, VTop);
	Vertices[1].TextureCoordinate.Set(URight, VTop);
	Vertices[2].TextureCoordinate.Set(ULeft, VBottom);
	Vertices[3].TextureCoordinate.Set(URight, VBottom);
	RHIUnlockVertexBuffer(VertexBufferRHI);

	return VertexBufferRHI;
}

/**
 * The simple element vertex declaration resource type.
 */
class FMediaVertexDeclaration
	: public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FMediaVertexDeclaration() { }

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FMediaElementVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, TextureCoordinate), VET_Float2, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};


UTILITYSHADERS_API extern TGlobalResource<FMediaVertexDeclaration> GMediaVertexDeclaration;


/**
 * Media vertex shader (shared by all media shaders).
 */
class FMediaShadersVS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FMediaShadersVS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	/** Default constructor. */
	FMediaShadersVS() { }

	/** Initialization constructor. */
	FMediaShadersVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};


/**
 * Pixel shader to convert an AYUV texture to RGBA.
 *
 * This shader expects a single texture consisting of a N x M array of pixels
 * in AYUV format. Each pixel is encoded as four consecutive unsigned chars
 * with the following layout: [V0 U0 Y0 A0][V1 U1 Y1 A1]..
 */
class FAYUVConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FAYUVConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FAYUVConvertPS() { }

	FAYUVConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> AYUVTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a Windows Bitmap texture.
 *
 * This shader expects a BMP frame packed into a single texture in PF_B8G8R8A8 format.
 */
class FBMPConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FBMPConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FBMPConvertPS() { }

	FBMPConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> BMPTexture, const FIntPoint& OutputDimensions, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a NV12 frame to RGBA.
 *
 * This shader expects an NV12 frame packed into a single texture in PF_G8 format.
 *
 * @see http://www.fourcc.org/yuv.php#NV12
 */
class FNV12ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FNV12ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FNV12ConvertPS() { }

	FNV12ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> NV12Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a NV21 frame to RGBA.
 *
 * This shader expects an NV21 frame packed into a single texture in PF_G8 format.
 *
 * @see http://www.fourcc.org/yuv.php#NV21
 */
class FNV21ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FNV21ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FNV21ConvertPS() { }

	FNV21ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> NV21Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to resize an RGB texture.
 *
 * This shader expects an RGB or RGBA frame packed into a single texture
 * in PF_B8G8R8A8 or PF_FloatRGB format.
 */
class FRGBConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FRGBConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FRGBConvertPS() { }

	FRGBConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> RGBTexture, const FIntPoint& OutputDimensions, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a PS4 YCbCr texture to RGBA.
 *
 * This shader expects a separate chroma and luma plane stored in two textures
 * in PF_B8G8R8A8 format. The full-size luma plane contains the Y-components.
 * The half-size chroma plane contains the UV components in the following
 * memory layout: [U0, V0][U1, V1]
 * 
 */
class FYCbCrConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYCbCrConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FYCbCrConvertPS() { }

	FYCbCrConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);		
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> LumaTexture, TRefCountPtr<FRHITexture2D> CbCrTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};

class FYCbCrConvertPS_4x4Matrix : public FYCbCrConvertPS
{
    DECLARE_EXPORTED_SHADER_TYPE(FYCbCrConvertPS_4x4Matrix, Global, UTILITYSHADERS_API);
    
public:
    
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
    }
    
    FYCbCrConvertPS_4x4Matrix() { }
    
    FYCbCrConvertPS_4x4Matrix(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FYCbCrConvertPS(Initializer)
    { }
};


/**
 * Pixel shader to convert a UYVY (Y422, UYNV) frame to RGBA.
 *
 * This shader expects a UYVY frame packed into a single texture in PF_B8G8R8A8
 * format with the following memory layout: [U0, Y0, V1, Y1][U1, Y2, V1, Y3]..
 *
 * @see http://www.fourcc.org/yuv.php#UYVY
 */
class FUYVYConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FUYVYConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FUYVYConvertPS() { }

	FUYVYConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> UYVYTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to convert Y, U, and V planes to RGBA.
 *
 * This shader expects three textures in PF_G8 format,
 * one for each plane of Y, U, and V components.
 */
class FYUVConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYUVConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FYUVConvertPS() { }

	FYUVConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YTexture, TRefCountPtr<FRHITexture2D> UTexture, TRefCountPtr<FRHITexture2D> VTexture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to convert YUV v210 to RGB
 *
 * This shader expects a single texture in PF_R32G32B32A32_UINT format.
 */
class FYUVv210ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYUVv210ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FYUVv210ConvertPS() { }

	FYUVv210ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YUVTexture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};



/**
 * Pixel shader to convert a YUY2 frame to RGBA.
 *
 * This shader expects an YUY2 frame packed into a single texture in PF_B8G8R8A8
 * format with the following memory layout: [Y0, U0, Y1, V0][Y2, U1, Y3, V1]...
 *
 * @see http://www.fourcc.org/yuv.php#YUY2
 */
class FYUY2ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYUY2ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FYUY2ConvertPS() { }

	FYUY2ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YUY2Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to convert a YVYU frame to RGBA.
 *
 * This shader expects a YVYU frame packed into a single texture in PF_B8G8R8A8
 * format with the following memory layout: [Y0, V0, Y1, U0][Y2, V1, Y3, U1]..
 *
 * @see http://www.fourcc.org/yuv.php#YVYU
 */
class FYVYUConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FYVYUConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FYVYUConvertPS() { }

	FYVYUConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YVYUTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear);
};


/**
 * Pixel shader to convert RGB 8 bits to UYVY 8 bits
 *
 * This shader expects a single texture in PF_B8G8R8A8 format.
 */
class FRGB8toUYVY8ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FRGB8toUYVY8ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FRGB8toUYVY8ConvertPS() { }

	FRGB8toUYVY8ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> RGBATexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool LinearToSrgb);
};


/**
 * Pixel shader to convert RGB 10 bits to YUV v210
 *
 * This shader expects a single texture in PF_A2B10G10R10 format.
 */
class FRGB10toYUVv210ConvertPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FRGB10toYUVv210ConvertPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FRGB10toYUVv210ConvertPS() { }

	FRGB10toYUVv210ConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> RGBATexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool LinearToSrgb);
};


/**
 * Pixel shader to inverse alpha component 
 *
 * This shader expects a single texture in RGBA 8 or 10 bits format.
 */
class FInvertAlphaPS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FInvertAlphaPS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FInvertAlphaPS() { }

	FInvertAlphaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> RGBATexture);
};


/**
 * Pixel shader to set alpha component to 1.0f
 *
 * This shader expects a single texture in RGBA 8 or 10 bits format.
 */
class FSetAlphaOnePS
	: public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FSetAlphaOnePS, Global, UTILITYSHADERS_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2);
	}

	FSetAlphaOnePS() { }

	FSetAlphaOnePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> RGBATexture);
};

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "PackedNormal.h"
#include "RenderResource.h"

extern RENDERCORE_API void RenderUtilsInit();

/**
* Constructs a basis matrix for the axis vectors and returns the sign of the determinant
*
* @param XAxis - x axis (tangent)
* @param YAxis - y axis (binormal)
* @param ZAxis - z axis (normal)
* @return sign of determinant either -1 or +1 
*/
FORCEINLINE float GetBasisDeterminantSign( const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis )
{
	FMatrix Basis(
		FPlane(XAxis,0),
		FPlane(YAxis,0),
		FPlane(ZAxis,0),
		FPlane(0,0,0,1)
		);
	return (Basis.Determinant() < 0) ? -1.0f : +1.0f;
}

/**
* Constructs a basis matrix for the axis vectors and returns the sign of the determinant
*
* @param XAxis - x axis (tangent)
* @param YAxis - y axis (binormal)
* @param ZAxis - z axis (normal)
* @return sign of determinant either -127 (-1) or +1 (127)
*/
FORCEINLINE int8 GetBasisDeterminantSignByte( const FPackedNormal& XAxis, const FPackedNormal& YAxis, const FPackedNormal& ZAxis )
{
	return GetBasisDeterminantSign(XAxis.ToFVector(),YAxis.ToFVector(),ZAxis.ToFVector()) < 0 ? -127 : 127;
}

/**
 * Given 2 axes of a basis stored as a packed type, regenerates the y-axis tangent vector and scales by z.W
 * @param XAxis - x axis (tangent)
 * @param ZAxis - z axis (normal), the sign of the determinant is stored in ZAxis.W
 * @return y axis (binormal)
 */
template<typename VectorType>
FORCEINLINE FVector GenerateYAxis(const VectorType& XAxis, const VectorType& ZAxis)
{
	static_assert(	ARE_TYPES_EQUAL(VectorType, FPackedNormal) ||
					ARE_TYPES_EQUAL(VectorType, FPackedRGBA16N), "ERROR: Must be FPackedNormal or FPackedRGBA16N");
	FVector  x = XAxis.ToFVector();
	FVector4 z = ZAxis.ToFVector4();
	return (FVector(z) ^ x) * z.W;
}

/** Information about a pixel format. */
struct FPixelFormatInfo
{
	const TCHAR*	Name;
	int32				BlockSizeX,
					BlockSizeY,
					BlockSizeZ,
					BlockBytes,
					NumComponents;
	/** Platform specific token, e.g. D3DFORMAT with D3DDrv										*/
	uint32			PlatformFormat;
	/** Whether the texture format is supported on the current platform/ rendering combination	*/
	bool			Supported;
	EPixelFormat	UnrealFormat;
};

extern RENDERCORE_API FPixelFormatInfo GPixelFormats[PF_MAX];		// Maps members of EPixelFormat to a FPixelFormatInfo describing the format.

#define NUM_DEBUG_UTIL_COLORS (32)
static const FColor DebugUtilColor[NUM_DEBUG_UTIL_COLORS] = 
{
	FColor(20,226,64),
	FColor(210,21,0),
	FColor(72,100,224),
	FColor(14,153,0),
	FColor(186,0,186),
	FColor(54,0,175),
	FColor(25,204,0),
	FColor(15,189,147),
	FColor(23,165,0),
	FColor(26,206,120),
	FColor(28,163,176),
	FColor(29,0,188),
	FColor(130,0,50),
	FColor(31,0,163),
	FColor(147,0,190),
	FColor(1,0,109),
	FColor(2,126,203),
	FColor(3,0,58),
	FColor(4,92,218),
	FColor(5,151,0),
	FColor(18,221,0),
	FColor(6,0,131),
	FColor(7,163,176),
	FColor(8,0,151),
	FColor(102,0,216),
	FColor(10,0,171),
	FColor(11,112,0),
	FColor(12,167,172),
	FColor(13,189,0),
	FColor(16,155,0),
	FColor(178,161,0),
	FColor(19,25,126)
};

//
//	CalculateImageBytes
//

extern RENDERCORE_API SIZE_T CalculateImageBytes(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format);

/** A global white texture. */
extern RENDERCORE_API class FTexture* GWhiteTexture;

/** A global black texture. */
extern RENDERCORE_API class FTexture* GBlackTexture;

/** A global black array texture. */
extern RENDERCORE_API class FTexture* GBlackArrayTexture;

/** A global black volume texture. */
extern RENDERCORE_API class FTexture* GBlackVolumeTexture;

/** A global black volume texture<uint>  */
extern RENDERCORE_API class FTexture* GBlackUintVolumeTexture;

/** A global white cube texture. */
extern RENDERCORE_API class FTexture* GWhiteTextureCube;

/** A global black cube texture. */
extern RENDERCORE_API class FTexture* GBlackTextureCube;

/** A global black cube depth texture. */
extern RENDERCORE_API class FTexture* GBlackTextureDepthCube;

/** A global black cube array texture. */
extern RENDERCORE_API class FTexture* GBlackCubeArrayTexture;

/** A global texture that has a different solid color in each mip-level. */
extern RENDERCORE_API class FTexture* GMipColorTexture;

/** Number of mip-levels in 'GMipColorTexture' */
extern RENDERCORE_API int32 GMipColorTextureMipLevels;

// 4: 8x8 cubemap resolution, shader needs to use the same value as preprocessing
extern RENDERCORE_API const uint32 GDiffuseConvolveMipLevel;

#define NUM_CUBE_VERTICES 36
/** The indices for drawing a cube. */
extern RENDERCORE_API const uint16 GCubeIndices[36];

class FCubeIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * NUM_CUBE_VERTICES, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockIndexBuffer(IndexBufferRHI, 0, sizeof(uint16) * NUM_CUBE_VERTICES, RLM_WriteOnly);
		FMemory::Memcpy(VoidPtr, GCubeIndices, NUM_CUBE_VERTICES * sizeof(uint16));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};
extern RENDERCORE_API TGlobalResource<FCubeIndexBuffer> GCubeIndexBuffer;

class FTwoTrianglesIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * 6, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockIndexBuffer(IndexBufferRHI, 0, sizeof(uint16) * 6, RLM_WriteOnly);
		static const uint16 Indices[] = { 0, 1, 3, 0, 3, 2 };
		FMemory::Memcpy(VoidPtr, Indices, 6 * sizeof(uint16));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};
extern RENDERCORE_API TGlobalResource<FTwoTrianglesIndexBuffer> GTwoTrianglesIndexBuffer;

class FScreenSpaceVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FVector2D) * 4, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FVector2D) * 4, RLM_WriteOnly);
		static const FVector2D Vertices[4] =
		{
			FVector2D(-1,-1),
			FVector2D(-1,+1),
			FVector2D(+1,-1),
			FVector2D(+1,+1),
		};
		FMemory::Memcpy(VoidPtr, Vertices, sizeof(FVector2D) * 4);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};
extern RENDERCORE_API TGlobalResource<FScreenSpaceVertexBuffer> GScreenSpaceVertexBuffer;

/**
 * Maps from an X,Y,Z cube vertex coordinate to the corresponding vertex index.
 */
inline uint16 GetCubeVertexIndex(uint32 X,uint32 Y,uint32 Z) { return X * 4 + Y * 2 + Z; }

/**
* A 3x1 of xyz(11:11:10) format.
*/
struct FPackedPosition
{
	union
	{
		struct
		{
#if PLATFORM_LITTLE_ENDIAN
			int32	X :	11;
			int32	Y : 11;
			int32	Z : 10;
#else
			int32	Z : 10;
			int32	Y : 11;
			int32	X : 11;
#endif
		} Vector;

		uint32		Packed;
	};

	// Constructors.
	FPackedPosition() : Packed(0) {}
	FPackedPosition(const FVector& Other) : Packed(0) 
	{
		Set(Other);
	}
	
	// Conversion operators.
	FPackedPosition& operator=( FVector Other )
	{
		Set( Other );
		return *this;
	}

	operator FVector() const;
	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set( const FVector& InVector );

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar,FPackedPosition& N);
};


/** Flags that control ConstructTexture2D */
enum EConstructTextureFlags
{
	/** Compress RGBA8 to DXT */
	CTF_Compress =				0x01,
	/** Don't actually compress until the pacakge is saved */
	CTF_DeferCompression =		0x02,
	/** Enable SRGB on the texture */
	CTF_SRGB =					0x04,
	/** Generate mipmaps for the texture */
	CTF_AllowMips =				0x08,
	/** Use DXT1a to get 1 bit alpha but only 4 bits per pixel (note: color of alpha'd out part will be black) */
	CTF_ForceOneBitAlpha =		0x10,
	/** When rendering a masked material, the depth is in the alpha, and anywhere not rendered will be full depth, which should actually be alpha of 0, and anything else is alpha of 255 */
	CTF_RemapAlphaAsMasked =	0x20,
	/** Ensure the alpha channel of the texture is opaque white (255). */
	CTF_ForceOpaque =			0x40,

	/** Default flags (maps to previous defaults to ConstructTexture2D) */
	CTF_Default = CTF_Compress | CTF_SRGB,
};

/**
 * Calculates the amount of memory used for a single mip-map of a texture 3D.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param TextureSizeZ		Number of slices (for the base mip-level)
 * @param Format	Texture format
 * @param MipIndex	The index of the mip-map to compute the size of.
 */
RENDERCORE_API SIZE_T CalcTextureMipMapSize3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex);

/**
 * Calculates the extent of a mip.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param TextureSizeZ		Number of depth texels (for the base mip-level)
 * @param Format			Texture format
 * @param MipIndex			The index of the mip-map to compute the size of.
 * @param OutXExtent		The extent X of the mip
 * @param OutYExtent		The extent Y of the mip
 * @param OutZExtent		The extent Z of the mip
 */
RENDERCORE_API void CalcMipMapExtent3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex, uint32& OutXExtent, uint32& OutYExtent, uint32& OutZExtent );

/**
 * Calculates the extent of a mip.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipIndex	The index of the mip-map to compute the size of.
 */
RENDERCORE_API FIntPoint CalcMipMapExtent( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex );

/**
 * Calculates the width of a mip, in blocks.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param Format			Texture format
 * @param MipIndex			The index of the mip-map to compute the size of.
 */
RENDERCORE_API SIZE_T CalcTextureMipWidthInBlocks(uint32 TextureSizeX, EPixelFormat Format, uint32 MipIndex);

/**
 * Calculates the height of a mip, in blocks.
 *
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param Format			Texture format
 * @param MipIndex			The index of the mip-map to compute the size of.
 */
RENDERCORE_API SIZE_T CalcTextureMipHeightInBlocks(uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex);

/**
 * Calculates the amount of memory used for a single mip-map of a texture.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipIndex	The index of the mip-map to compute the size of.
 */
RENDERCORE_API SIZE_T CalcTextureMipMapSize( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex );

/**
 * Calculates the amount of memory used for a texture.
 *
 * @param SizeX		Number of horizontal texels (for the base mip-level)
 * @param SizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipCount	Number of mip-levels (including the base mip-level)
 */
RENDERCORE_API SIZE_T CalcTextureSize( uint32 SizeX, uint32 SizeY, EPixelFormat Format, uint32 MipCount );

/**
 * Calculates the amount of memory used for a texture.
 *
 * @param SizeX		Number of horizontal texels (for the base mip-level)
 * @param SizeY		Number of vertical texels (for the base mip-level)
 * @param SizeY		Number of depth texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipCount	Number of mip-levels (including the base mip-level)
 */
RENDERCORE_API SIZE_T CalcTextureSize3D( uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, uint32 MipCount );

/**
 * Copies the data for a 2D texture between two buffers with potentially different strides.
 * @param Source       - The source buffer
 * @param Dest         - The destination buffer.
 * @param SizeY        - The height of the texture data to copy in pixels.
 * @param Format       - The format of the texture being copied.
 * @param SourceStride - The stride of the source buffer.
 * @param DestStride   - The stride of the destination buffer.
 */
RENDERCORE_API void CopyTextureData2D(const void* Source,void* Dest,uint32 SizeY,EPixelFormat Format,uint32 SourceStride,uint32 DestStride);

/**
 * enum to string
 *
 * @return e.g. "PF_B8G8R8A8"
 */
RENDERCORE_API const TCHAR* GetPixelFormatString(EPixelFormat InPixelFormat);
/**
 * string to enum (not case sensitive)
 *
 * @param InPixelFormatStr e.g. "PF_B8G8R8A8", must not not be 0
 */
RENDERCORE_API EPixelFormat GetPixelFormatFromString(const TCHAR* InPixelFormatStr);

/**
 * Convert from ECubeFace to text string
 * @param Face - ECubeFace type to convert
 * @return text string for cube face enum value
 */
RENDERCORE_API const TCHAR* GetCubeFaceName(ECubeFace Face);

/**
 * Convert from text string to ECubeFace 
 * @param Name e.g. RandomNamePosX
 * @return CubeFace_MAX if not recognized
 */
RENDERCORE_API ECubeFace GetCubeFaceFromName(const FString& Name);

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector4();

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector3();

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector2();

RENDERCORE_API bool PlatformSupportsSimpleForwardShading(EShaderPlatform Platform);

RENDERCORE_API bool IsSimpleForwardShadingEnabled(EShaderPlatform Platform);

/** Returns if ForwardShading is enabled. Only valid for the current platform (otherwise call ITargetPlatform::UsesForwardShading()). */
inline bool IsForwardShadingEnabled(EShaderPlatform Platform)
{
	extern RENDERCORE_API uint32 GForwardShadingPlatformMask;
	return !!(GForwardShadingPlatformMask & (1u << Platform))
		// Culling uses compute shader
		&& GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM5;
}

/** Returns if ForwardShading or SimpleForwardShading is enabled. Only valid for the current platform. */
inline bool IsAnyForwardShadingEnabled(EShaderPlatform Platform)
{
	return IsForwardShadingEnabled(Platform) || IsSimpleForwardShadingEnabled(Platform);
}

/** Returns if the GBuffer is used. Only valid for the current platform. */
inline bool IsUsingGBuffers(EShaderPlatform Platform)
{
	return !IsAnyForwardShadingEnabled(Platform);
}

/** Returns whether DBuffer decals are enabled for a given shader platform */
inline bool IsUsingDBuffers(EShaderPlatform Platform)
{
	extern RENDERCORE_API uint32 GDBufferPlatformMask;
	return !!(GDBufferPlatformMask & (1u << Platform));
}

inline bool IsUsingPerPixelDBufferMask(EShaderPlatform Platform)
{
	switch (Platform)
	{
	case SP_SWITCH:
	case SP_SWITCH_FORWARD:
		// Per-pixel DBufferMask optimization is currently only tested and supported on Switch.
		return true;
	default:
		return false;
	}
}

inline bool UseGPUScene(EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel)
{
	// GPU Scene management uses compute shaders
	return FeatureLevel >= ERHIFeatureLevel::SM5 
		//@todo - support GPU Scene management compute shaders on these platforms to get dynamic instancing speedups on the Rendering Thread and RHI Thread
		&& !IsOpenGLPlatform(Platform)
		&& !IsVulkanPlatform(Platform)
		&& !IsSwitchPlatform(Platform);
}

/** Unit cube vertex buffer (VertexDeclarationFVector4) */
RENDERCORE_API FVertexBufferRHIRef& GetUnitCubeVertexBuffer();

/** Unit cube index buffer */
RENDERCORE_API FIndexBufferRHIRef& GetUnitCubeIndexBuffer();

/**
* Takes the requested buffer size and quantizes it to an appropriate size for the rest of the
* rendering pipeline. Currently ensures that sizes are multiples of 4 so that they can safely
* be halved in size several times.
*/
RENDERCORE_API void QuantizeSceneBufferSize(const FIntPoint& InBufferSize, FIntPoint& OutBufferSize);

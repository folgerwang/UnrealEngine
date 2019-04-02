// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHI.h: Render Hardware Interface definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHIDefinitions.h"
#include "Containers/StaticArray.h"

#ifndef RHI_COMMAND_LIST_DEBUG_TRACES
#define RHI_COMMAND_LIST_DEBUG_TRACES 0
#endif

class FResourceArrayInterface;
class FResourceBulkDataInterface;


/** Alignment of the shader parameters struct is required to be 16-byte boundaries. */
#define SHADER_PARAMETER_STRUCT_ALIGNMENT 16

/** The alignment in bytes between elements of array shader parameters. */
#define SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT 16

// RHICreateUniformBuffer assumes C++ constant layout matches the shader layout when extracting float constants, yet the C++ struct contains pointers.  
// Enforce a min size of 64 bits on pointer types in uniform buffer structs to guarantee layout matching between languages.
#define SHADER_PARAMETER_POINTER_ALIGNMENT sizeof(uint64)
static_assert(sizeof(void*) <= SHADER_PARAMETER_POINTER_ALIGNMENT, "The alignment of pointer needs to match the largest pointer.");


/** RHI Logging. */
RHI_API DECLARE_LOG_CATEGORY_EXTERN(LogRHI,Log,VeryVerbose);

/**
 * RHI configuration settings.
 */

namespace RHIConfig
{
	RHI_API bool ShouldSaveScreenshotAfterProfilingGPU();
	RHI_API bool ShouldShowProfilerAfterProfilingGPU();
	RHI_API float GetGPUHitchThreshold();
}

/**
 * RHI globals.
 */

/** True if the render hardware has been initialized. */
extern RHI_API bool GIsRHIInitialized;

class RHI_API FRHICommandList;

/**
 * RHI capabilities.
 */


/** The maximum number of mip-maps that a texture can contain. 	*/
extern	RHI_API int32		GMaxTextureMipCount;

/** Does the RHI implements CopyToTexture() with FRHICopyTextureInfo::NumMips > 1 */
extern RHI_API bool GRHISupportsCopyToTextureMultipleMips;

/** true if this platform has quad buffer stereo support. */
extern RHI_API bool GSupportsQuadBufferStereo;

/** true if the RHI supports textures that may be bound as both a render target and a shader resource. */
extern RHI_API bool GSupportsRenderDepthTargetableShaderResources;

/** true if the RHI supports binding depth as a texture when testing against depth */
extern RHI_API bool GSupportsDepthFetchDuringDepthTest;

// The maximum feature level and shader platform available on this system
// GRHIFeatureLevel and GRHIShaderPlatform have been deprecated. There is no longer a current featurelevel/shaderplatform that
// should be used for all rendering, rather a specific set for each view.
extern RHI_API ERHIFeatureLevel::Type GMaxRHIFeatureLevel;
extern RHI_API EShaderPlatform GMaxRHIShaderPlatform;

/** true if the RHI supports SRVs */
extern RHI_API bool GSupportsResourceView;

/** 
 * only set if RHI has the information (after init of the RHI and only if RHI has that information, never changes after that)
 * e.g. "NVIDIA GeForce GTX 670"
 */
extern RHI_API FString GRHIAdapterName;
extern RHI_API FString GRHIAdapterInternalDriverVersion;
extern RHI_API FString GRHIAdapterUserDriverVersion;
extern RHI_API FString GRHIAdapterDriverDate;
extern RHI_API uint32 GRHIDeviceId;
extern RHI_API uint32 GRHIDeviceRevision;

// 0 means not defined yet, use functions like IsRHIDeviceAMD() to access
extern RHI_API uint32 GRHIVendorId;

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceAMD();

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceIntel();

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceNVIDIA();

// helper to convert GRHIVendorId into a printable string, or "Unknown" if unknown.
RHI_API const TCHAR* RHIVendorIdToString();

// helper to return the shader language version for the given shader platform.
RHI_API uint32 RHIGetShaderLanguageVersion(const EShaderPlatform Platform);

// helper to check that the shader platform supports tessellation.
RHI_API bool RHISupportsTessellation(const EShaderPlatform Platform);

// helper to check that the shader platform supports writing to UAVs from pixel shaders.
RHI_API bool RHISupportsPixelShaderUAVs(const EShaderPlatform Platform);

// helper to check that the shader platform supports creating a UAV off an index buffer.
RHI_API bool RHISupportsIndexBufferUAVs(const EShaderPlatform Platform);

// helper to check if a preview feature level has been requested.
RHI_API bool RHIGetPreviewFeatureLevel(ERHIFeatureLevel::Type& PreviewFeatureLevelOUT);

inline bool RHISupportsInstancedStereo(const EShaderPlatform Platform)
{
	// Only D3D SM5, PS4 and Metal SM5 supports Instanced Stereo
	return (Platform == EShaderPlatform::SP_PCD3D_SM5 || Platform == EShaderPlatform::SP_PS4 || Platform == EShaderPlatform::SP_METAL_SM5 || Platform == EShaderPlatform::SP_METAL_SM5_NOTESS);
}

inline bool RHISupportsMultiView(const EShaderPlatform Platform)
{
	// Only PS4 and Metal SM5 from 10.13 onward supports Multi-View
	return (Platform == EShaderPlatform::SP_PS4) || ((Platform == EShaderPlatform::SP_METAL_SM5 || Platform == SP_METAL_SM5_NOTESS));
}

inline bool RHISupportsMSAA(EShaderPlatform Platform)
{
	return 
		//@todo-rco: Fix when iOS OpenGL supports MSAA
		Platform != SP_OPENGL_ES2_IOS
		// @todo optimise MSAA for XboxOne, currently uses significant eRAM.
		&& Platform != SP_XBOXONE_D3D12;
}

inline bool RHISupportsBufferLoadTypeConversion(EShaderPlatform Platform)
{
	return true;
}

/** Whether the platform supports reading from volume textures (does not cover rendering to volume textures). */
inline bool RHISupportsVolumeTextures(ERHIFeatureLevel::Type FeatureLevel)
{
	return FeatureLevel >= ERHIFeatureLevel::SM4;
}

inline bool RHISupportsVertexShaderLayer(const EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && IsMetalPlatform(Platform) && (IsPCPlatform(Platform) || (Platform == SP_METAL_MRT && RHIGetShaderLanguageVersion(Platform) >= 4));
}

inline bool RHISupports4ComponentUAVReadWrite(EShaderPlatform Platform)
{
	// Must match usf PLATFORM_SUPPORTS_4COMPONENT_UAV_READ_WRITE
	// D3D11 does not support multi-component loads from a UAV: "error X3676: typed UAV loads are only allowed for single-component 32-bit element types"
	return Platform == SP_XBOXONE_D3D12 || Platform == SP_PS4 || IsMetalPlatform(Platform);
}

/** Whether Manual Vertex Fetch is supported for the specified shader platform.
	Shader Platform must not use the mobile renderer, and for Metal, the shader language must be at least 2. */
inline bool RHISupportsManualVertexFetch(EShaderPlatform InShaderPlatform)
{
	return (!IsOpenGLPlatform(InShaderPlatform) || IsSwitchPlatform(InShaderPlatform)) && !IsMobilePlatform(InShaderPlatform);
}

/** 
 * Returns true if SV_VertexID contains BaseVertexIndex passed to the draw call, false if shaders must manually construct an absolute VertexID.
 */
inline bool RHISupportsAbsoluteVertexID(EShaderPlatform InShaderPlatform)
{
	return IsVulkanPlatform(InShaderPlatform) || IsVulkanMobilePlatform(InShaderPlatform);
}

/** Can this platform compile ray tracing shaders (regardless of project settings).
 *  To use at runtime, also check GRHISupportsRayTracing and r.RayTracing CVar (see IsRayTracingEnabled() helper).
 **/
inline RHI_API bool RHISupportsRayTracingShaders(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5;
}

/** Can this platform compile shaders that use shader model 6.0 wave intrinsics.
 *  To use such shaders at runtime, also check GRHISupportsWaveOperations.
 **/
inline RHI_API bool RHISupportsWaveOperations(EShaderPlatform Platform)
{
	// Currently SM6 shaders are treated as an extension of SM5.
	return Platform == SP_PCD3D_SM5;
}


// Wrapper for GRHI## global variables, allows values to be overridden for mobile preview modes.
template <typename TValueType>
class TRHIGlobal
{
public:
	explicit TRHIGlobal(const TValueType& InValue) : Value(InValue) {}

	TRHIGlobal& operator=(const TValueType& InValue) 
	{
		Value = InValue; 
		return *this;
	}

#if WITH_EDITOR
	inline void SetPreviewOverride(const TValueType& InValue)
	{
		PreviewValue = InValue;
	}

	inline operator TValueType() const
	{ 
		return PreviewValue.IsSet() ? GetPreviewValue() : Value;
	}
#else
	inline operator TValueType() const { return Value; }
#endif

private:
	TValueType Value;
#if WITH_EDITOR
	TOptional<TValueType> PreviewValue;
	TValueType GetPreviewValue() const { return PreviewValue.GetValue(); }
#endif
};

#if WITH_EDITOR
template<>
inline int32 TRHIGlobal<int32>::GetPreviewValue() const 
{
	// ensure the preview values are subsets of RHI functionality.
	return FMath::Min(PreviewValue.GetValue(), Value);
}
template<>
inline bool TRHIGlobal<bool>::GetPreviewValue() const
{
	// ensure the preview values are subsets of RHI functionality.
	return PreviewValue.GetValue() && Value;
}
#endif

/** true if the GPU is AMD's Pre-GCN architecture */
extern RHI_API bool GRHIDeviceIsAMDPreGCNArchitecture;

/** true if PF_G8 render targets are supported */
extern RHI_API TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_G8;

/** true if PF_FloatRGBA render targets are supported */
extern RHI_API TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_FloatRGBA;

/** true if mobile framebuffer fetch is supported */
extern RHI_API bool GSupportsShaderFramebufferFetch;

/** true if mobile depth & stencil fetch is supported */
extern RHI_API bool GSupportsShaderDepthStencilFetch;

/** true if RQT_AbsoluteTime is supported by RHICreateRenderQuery */
extern RHI_API bool GSupportsTimestampRenderQueries;

/** true if RQT_AbsoluteTime is supported by RHICreateRenderQuery */
extern RHI_API bool GRHISupportsGPUTimestampBubblesRemoval;

/** true if RHIGetGPUFrameCycles removes CPu generated bubbles. */
extern RHI_API bool GRHISupportsFrameCyclesBubblesRemoval;

/** true if the GPU supports hidden surface removal in hardware. */
extern RHI_API bool GHardwareHiddenSurfaceRemoval;

/** true if the RHI supports asynchronous creation of texture resources */
extern RHI_API bool GRHISupportsAsyncTextureCreation;

/** true if the RHI supports quad topology (PT_QuadList). */
extern RHI_API bool GRHISupportsQuadTopology;

/** true if the RHI supports rectangular topology (PT_RectList). */
extern RHI_API bool GRHISupportsRectTopology;

/** Temporary. When OpenGL is running in a separate thread, it cannot yet do things like initialize shaders that are first discovered in a rendering task. It is doable, it just isn't done. */
extern RHI_API bool GSupportsParallelRenderingTasksWithSeparateRHIThread;

/** If an RHI is so slow, that it is the limiting factor for the entire frame, we can kick early to try to give it as much as possible. */
extern RHI_API bool GRHIThreadNeedsKicking;

/** If an RHI cannot do an unlimited number of occlusion queries without stalling and waiting for the GPU, this can be used to tune hte occlusion culler to try not to do that. */
extern RHI_API int32 GRHIMaximumReccommendedOustandingOcclusionQueries;

/** Some RHIs can only do visible or not occlusion queries. */
extern RHI_API bool GRHISupportsExactOcclusionQueries;

/** True if and only if the GPU support rendering to volume textures (2D Array, 3D). Some OpenGL 3.3 cards support SM4, but can't render to volume textures. */
extern RHI_API bool GSupportsVolumeTextureRendering;

/** True if the RHI supports separate blend states per render target. */
extern RHI_API bool GSupportsSeparateRenderTargetBlendState;

/** True if the RHI can render to a depth-only render target with no additional color render target. */
extern RHI_API bool GSupportsDepthRenderTargetWithoutColorRenderTarget;

/** True if the RHI has artifacts with atlased CSM depths. */
extern RHI_API bool GRHINeedsUnatlasedCSMDepthsWorkaround;

/** true if the RHI supports 3D textures */
extern RHI_API bool GSupportsTexture3D;

/** true if the RHI supports mobile multi-view */
extern RHI_API bool GSupportsMobileMultiView;

/** true if the RHI supports image external */
extern RHI_API bool GSupportsImageExternal;

/** true if the RHI supports MRT */
extern RHI_API TRHIGlobal<bool> GSupportsMultipleRenderTargets;

/** true if the RHI supports 256bit MRT */
extern RHI_API bool GSupportsWideMRT;

/** True if the RHI and current hardware supports supports depth bounds testing */
extern RHI_API bool GSupportsDepthBoundsTest;

/** True if the RHI and current hardware support a render target write mask */
extern RHI_API bool GSupportsRenderTargetWriteMask;

/** True if the RHI and current hardware supports efficient AsyncCompute (by default we assume false and later we can enable this for more hardware) */
extern RHI_API bool GSupportsEfficientAsyncCompute;

/** True if the RHI supports 'GetHDR32bppEncodeModeES2' shader intrinsic. */
extern RHI_API bool GSupportsHDR32bppEncodeModeIntrinsic;

/** True if the RHI supports getting the result of occlusion queries when on a thread other than the renderthread */
extern RHI_API bool GSupportsParallelOcclusionQueries;

/** true if the RHI supports aliasing of transient resources */
extern RHI_API bool GSupportsTransientResourceAliasing;

/** true if the RHI requires a valid RT bound during UAV scatter operation inside the pixel shader */
extern RHI_API bool GRHIRequiresRenderTargetForPixelShaderUAVs;

/** The minimum Z value in clip space for the RHI. */
extern RHI_API float GMinClipZ;

/** The sign to apply to the Y axis of projection matrices. */
extern RHI_API float GProjectionSignY;

/** Does this RHI need to wait for deletion of resources due to ref counting. */
extern RHI_API bool GRHINeedsExtraDeletionLatency;

/** Allow opt-out default RHI resource deletion latency for streaming textures */
extern RHI_API bool GRHIForceNoDeletionLatencyForStreamingTextures;

/** The maximum size allowed for a computeshader dispatch. */
extern RHI_API TRHIGlobal<int32> GMaxComputeDispatchDimension;

/** If true, then avoiding loading shader code and instead force the "native" path, which sends a library and a hash instead. */
extern RHI_API bool GRHILazyShaderCodeLoading;

/** If true, then it is possible to turn on GRHILazyShaderCodeLoading. */
extern RHI_API bool GRHISupportsLazyShaderCodeLoading;

/** The maximum size to allow for the shadow depth buffer in the X dimension.  This must be larger or equal to GMaxShadowDepthBufferSizeY. */
extern RHI_API TRHIGlobal<int32> GMaxShadowDepthBufferSizeX;
/** The maximum size to allow for the shadow depth buffer in the Y dimension. */
extern RHI_API TRHIGlobal<int32> GMaxShadowDepthBufferSizeY;

/** The maximum size allowed for 2D textures in both dimensions. */
extern RHI_API TRHIGlobal<int32> GMaxTextureDimensions;

FORCEINLINE uint32 GetMax2DTextureDimension()
{
	return GMaxTextureDimensions;
}

/** The maximum size allowed for cube textures. */
extern RHI_API TRHIGlobal<int32> GMaxCubeTextureDimensions;
FORCEINLINE uint32 GetMaxCubeTextureDimension()
{
	return GMaxCubeTextureDimensions;
}

/** The Maximum number of layers in a 1D or 2D texture array. */
extern RHI_API int32 GMaxTextureArrayLayers;
FORCEINLINE uint32 GetMaxTextureArrayLayers()
{
	return GMaxTextureArrayLayers;
}

extern RHI_API int32 GMaxTextureSamplers;
FORCEINLINE uint32 GetMaxTextureSamplers()
{
	return GMaxTextureSamplers;
}

/** true if we are running with the NULL RHI */
extern RHI_API bool GUsingNullRHI;

/**
 *	The size to check against for Draw*UP call vertex counts.
 *	If greater than this value, the draw call will not occur.
 */
extern RHI_API int32 GDrawUPVertexCheckCount;
/**
 *	The size to check against for Draw*UP call index counts.
 *	If greater than this value, the draw call will not occur.
 */
extern RHI_API int32 GDrawUPIndexCheckCount;

/** true for each VET that is supported. One-to-one mapping with EVertexElementType */
extern RHI_API class FVertexElementTypeSupportInfo GVertexElementTypeSupport;

#include "MultiGPU.h"

/** Whether the next frame should profile the GPU. */
extern RHI_API bool GTriggerGPUProfile;

/** Whether we are profiling GPU hitches. */
extern RHI_API bool GTriggerGPUHitchProfile;

/** Non-empty if we are performing a gpu trace. Also says where to place trace file. */
extern RHI_API FString GGPUTraceFileName;

/** True if the RHI supports texture streaming */
extern RHI_API bool GRHISupportsTextureStreaming;
/** Amount of memory allocated by textures. In kilobytes. */
extern RHI_API volatile int32 GCurrentTextureMemorySize;
/** Amount of memory allocated by rendertargets. In kilobytes. */
extern RHI_API volatile int32 GCurrentRendertargetMemorySize;
/** Current texture streaming pool size, in bytes. 0 means unlimited. */
extern RHI_API int64 GTexturePoolSize;
/** In percent. If non-zero, the texture pool size is a percentage of GTotalGraphicsMemory. */
extern RHI_API int32 GPoolSizeVRAMPercentage;

/** Some simple runtime stats, reset on every call to RHIBeginFrame */
/** Num draw calls & primitives on previous frame (accurate on any thread)*/
extern RHI_API int32 GNumDrawCallsRHI;
extern RHI_API int32 GNumPrimitivesDrawnRHI;

/** Num draw calls and primitives this frame (only accurate on RenderThread) */
extern RHI_API int32 GCurrentNumDrawCallsRHI;
extern RHI_API int32 GCurrentNumPrimitivesDrawnRHI;


/** Whether or not the RHI can handle a non-zero BaseVertexIndex - extra SetStreamSource calls will be needed if this is false */
extern RHI_API bool GRHISupportsBaseVertexIndex;

/** True if the RHI supports hardware instancing */
extern RHI_API TRHIGlobal<bool> GRHISupportsInstancing;

/** True if the RHI supports copying cubemap faces using CopyToResolveTarget */
extern RHI_API bool GRHISupportsResolveCubemapFaces;

/** Whether or not the RHI can handle a non-zero FirstInstance to DrawIndexedPrimitive and friends - extra SetStreamSource calls will be needed if this is false */
extern RHI_API bool GRHISupportsFirstInstance;

/** Whether or not the RHI can handle dynamic resolution or not. */
extern RHI_API bool GRHISupportsDynamicResolution;

/** Whether or not the RHI supports ray tracing on current hardware (acceleration structure building and new ray tracing-specific shader types). */
extern RHI_API bool GRHISupportsRayTracing;

/** Whether or not the RHI supports shader wave operations (shader model 6.0). */
extern RHI_API bool GRHISupportsWaveOperations;

/** Whether or not the RHI supports an RHI thread.
Requirements for RHI thread
* Microresources (those in RHIStaticStates.h) need to be able to be created by any thread at any time and be able to work with a radically simplified rhi resource lifecycle. CreateSamplerState, CreateRasterizerState, CreateDepthStencilState, CreateBlendState
* CreateUniformBuffer needs to be threadsafe
* GetRenderQueryResult should be threadsafe, but this isn't required. If it isn't threadsafe, then you need to flush yourself in the RHI
* GetViewportBackBuffer and AdvanceFrameForGetViewportBackBuffer need to be threadsafe and need to support the fact that the render thread has a different concept of "current backbuffer" than the RHI thread. Without an RHIThread this is moot due to the next two items.
* AdvanceFrameForGetViewportBackBuffer needs be added as an RHI method and this needs to work with GetViewportBackBuffer to give the render thread the right back buffer even though many commands relating to the beginning and end of the frame are queued.
* BeginDrawingViewport, and 5 or so other frame advance methods are queued with an RHIThread. Without an RHIThread, these just flush internally.
***/
extern RHI_API bool GRHISupportsRHIThread;
/* as above, but we run the commands on arbitrary task threads */
extern RHI_API bool GRHISupportsRHIOnTaskThread;

/** Whether or not the RHI supports parallel RHIThread executes / translates
Requirements:
* RHICreateBoundShaderState & RHICreateGraphicsPipelineState is threadsafe and GetCachedBoundShaderState must not be used. GetCachedBoundShaderState_Threadsafe has a slightly different protocol.
***/
extern RHI_API bool GRHISupportsParallelRHIExecute;

/** Whether or not the RHI can perform MSAA sample load. */
extern RHI_API bool GRHISupportsMSAADepthSampleAccess;

/** Whether or not HDR is currently enabled */
extern RHI_API bool GRHIIsHDREnabled;

/** Whether the present adapter/display offers HDR output capabilities. */
extern RHI_API bool GRHISupportsHDROutput;

/** Format used for the backbuffer when outputting to a HDR display. */
extern RHI_API EPixelFormat GRHIHDRDisplayOutputFormat;

/** Counter incremented once on each frame present. Used to support game thread synchronization with swap chain frame flips. */
extern RHI_API uint64 GRHIPresentCounter;

/** Called once per frame only from within an RHI. */
extern RHI_API void RHIPrivateBeginFrame();


RHI_API FName LegacyShaderPlatformToShaderFormat(EShaderPlatform Platform);
RHI_API EShaderPlatform ShaderFormatToLegacyShaderPlatform(FName ShaderFormat);
RHI_API FName ShaderPlatformToPlatformName(EShaderPlatform Platform);

/**
 * Adjusts a projection matrix to output in the correct clip space for the
 * current RHI. Unreal projection matrices follow certain conventions and
 * need to be patched for some RHIs. All projection matrices should be adjusted
 * before being used for rendering!
 */
inline FMatrix AdjustProjectionMatrixForRHI(const FMatrix& InProjectionMatrix)
{
	FScaleMatrix ClipSpaceFixScale(FVector(1.0f, GProjectionSignY, 1.0f - GMinClipZ));
	FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, GMinClipZ));	
	return InProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate;
}

/** Set runtime selection of mobile feature level preview. */
RHI_API void RHISetMobilePreviewFeatureLevel(ERHIFeatureLevel::Type MobilePreviewFeatureLevel);

/** Current shader platform. */


/** Finds a corresponding ERHIFeatureLevel::Type given an FName, or returns false if one could not be found. */
extern RHI_API bool GetFeatureLevelFromName(FName Name, ERHIFeatureLevel::Type& OutFeatureLevel);

/** Creates a string for the given feature level. */
extern RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FString& OutName);

/** Creates an FName for the given feature level. */
extern RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FName& OutName);


/** Table for finding out which shader platform corresponds to a given feature level for this RHI. */
extern RHI_API EShaderPlatform GShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num];

/** Get the shader platform associated with the supplied feature level on this machine */
inline EShaderPlatform GetFeatureLevelShaderPlatform(ERHIFeatureLevel::Type InFeatureLevel)
{
	return GShaderPlatformForFeatureLevel[InFeatureLevel];
}


/** Finds a corresponding ERHIShadingPath::Type given an FName, or returns false if one could not be found. */
extern RHI_API bool GetShadingPathFromName(FName Name, ERHIShadingPath::Type& OutShadingPath);

/** Creates a string for the given shading path. */
extern RHI_API void GetShadingPathName(ERHIShadingPath::Type InShadingPath, FString& OutName);

/** Creates an FName for the given shading path. */
extern RHI_API void GetShadingPathName(ERHIShadingPath::Type InShadingPath, FName& OutName);


inline FArchive& operator <<(FArchive& Ar, EResourceLockMode& LockMode)
{
	uint32 Temp = LockMode;
	Ar << Temp;
	LockMode = (EResourceLockMode)Temp;
	return Ar;
}

/** to customize the RHIReadSurfaceData() output */
class FReadSurfaceDataFlags
{
public:
	// @param InCompressionMode defines the value input range that is mapped to output range
	// @param InCubeFace defined which cubemap side is used, only required for cubemap content, then it needs to be a valid side
	FReadSurfaceDataFlags(ERangeCompressionMode InCompressionMode = RCM_UNorm, ECubeFace InCubeFace = CubeFace_MAX) 
		:CubeFace(InCubeFace), CompressionMode(InCompressionMode), bLinearToGamma(true), MaxDepthRange(16000.0f), bOutputStencil(false), MipLevel(0)
	{
	}

	ECubeFace GetCubeFace() const
	{
		checkSlow(CubeFace <= CubeFace_NegZ);
		return CubeFace;
	}

	ERangeCompressionMode GetCompressionMode() const
	{
		return CompressionMode;
	}

	void SetLinearToGamma(bool Value)
	{
		bLinearToGamma = Value;
	}

	bool GetLinearToGamma() const
	{
		return bLinearToGamma;
	}

	void SetOutputStencil(bool Value)
	{
		bOutputStencil = Value;
	}

	bool GetOutputStencil() const
	{
		return bOutputStencil;
	}

	void SetMip(uint8 InMipLevel)
	{
		MipLevel = InMipLevel;
	}

	uint8 GetMip() const
	{
		return MipLevel;
	}	

	void SetMaxDepthRange(float Value)
	{
		MaxDepthRange = Value;
	}

	float ComputeNormalizedDepth(float DeviceZ) const
	{
		return FMath::Abs(ConvertFromDeviceZ(DeviceZ) / MaxDepthRange);
	}

private:

	// @return SceneDepth
	float ConvertFromDeviceZ(float DeviceZ) const
	{
		DeviceZ = FMath::Min(DeviceZ, 1 - Z_PRECISION);

		// for depth to linear conversion
		const FVector2D InvDeviceZToWorldZ(0.1f, 0.1f);

		return 1.0f / (DeviceZ * InvDeviceZToWorldZ.X - InvDeviceZToWorldZ.Y);
	}

	ECubeFace CubeFace;
	ERangeCompressionMode CompressionMode;
	bool bLinearToGamma;	
	float MaxDepthRange;
	bool bOutputStencil;
	uint8 MipLevel;
};

/** Info for supporting the vertex element types */
class FVertexElementTypeSupportInfo
{
public:
	FVertexElementTypeSupportInfo() { for(int32 i=0; i<VET_MAX; i++) ElementCaps[i]=true; }
	FORCEINLINE bool IsSupported(EVertexElementType ElementType) { return ElementCaps[ElementType]; }
	FORCEINLINE void SetSupported(EVertexElementType ElementType,bool bIsSupported) { ElementCaps[ElementType]=bIsSupported; }
private:
	/** cap bit set for each VET. One-to-one mapping based on EVertexElementType */
	bool ElementCaps[VET_MAX];
};

struct FVertexElement
{
	uint8 StreamIndex;
	uint8 Offset;
	TEnumAsByte<EVertexElementType> Type;
	uint8 AttributeIndex;
	uint16 Stride;
	/**
	 * Whether to use instance index or vertex index to consume the element.  
	 * eg if bUseInstanceIndex is 0, the element will be repeated for every instance.
	 */
	uint16 bUseInstanceIndex;

	FVertexElement() {}
	FVertexElement(uint8 InStreamIndex,uint8 InOffset,EVertexElementType InType,uint8 InAttributeIndex,uint16 InStride,bool bInUseInstanceIndex = false):
		StreamIndex(InStreamIndex),
		Offset(InOffset),
		Type(InType),
		AttributeIndex(InAttributeIndex),
		Stride(InStride),
		bUseInstanceIndex(bInUseInstanceIndex)
	{}
	/**
	* Suppress the compiler generated assignment operator so that padding won't be copied.
	* This is necessary to get expected results for code that zeros, assigns and then CRC's the whole struct.
	*/
	void operator=(const FVertexElement& Other)
	{
		StreamIndex = Other.StreamIndex;
		Offset = Other.Offset;
		Type = Other.Type;
		AttributeIndex = Other.AttributeIndex;
		bUseInstanceIndex = Other.bUseInstanceIndex;
	}

	friend FArchive& operator<<(FArchive& Ar,FVertexElement& Element)
	{
		Ar << Element.StreamIndex;
		Ar << Element.Offset;
		Ar << Element.Type;
		Ar << Element.AttributeIndex;
		Ar << Element.Stride;
		Ar << Element.bUseInstanceIndex;
		return Ar;
	}
	RHI_API FString ToString() const;
	RHI_API void FromString(const FString& Src);
};

typedef TArray<FVertexElement,TFixedAllocator<MaxVertexElementCount> > FVertexDeclarationElementList;

/** RHI representation of a single stream out element. */
struct FStreamOutElement
{
	/** Index of the output stream from the geometry shader. */
	uint32 Stream;

	/** Semantic name of the output element as defined in the geometry shader.  This should not contain the semantic number. */
	const ANSICHAR* SemanticName;

	/** Semantic index of the output element as defined in the geometry shader.  For example "TEXCOORD5" in the shader would give a SemanticIndex of 5. */
	uint32 SemanticIndex;

	/** Start component index of the shader output element to stream out. */
	uint8 StartComponent;

	/** Number of components of the shader output element to stream out. */
	uint8 ComponentCount;

	/** Stream output target slot, corresponding to the streams set by RHISetStreamOutTargets. */
	uint8 OutputSlot;

	FStreamOutElement() {}
	FStreamOutElement(uint32 InStream, const ANSICHAR* InSemanticName, uint32 InSemanticIndex, uint8 InComponentCount, uint8 InOutputSlot) :
		Stream(InStream),
		SemanticName(InSemanticName),
		SemanticIndex(InSemanticIndex),
		StartComponent(0),
		ComponentCount(InComponentCount),
		OutputSlot(InOutputSlot)
	{}
};

typedef TArray<FStreamOutElement,TFixedAllocator<MaxVertexElementCount> > FStreamOutElementList;

struct FSamplerStateInitializerRHI
{
	FSamplerStateInitializerRHI() {}
	FSamplerStateInitializerRHI(
		ESamplerFilter InFilter,
		ESamplerAddressMode InAddressU = AM_Wrap,
		ESamplerAddressMode InAddressV = AM_Wrap,
		ESamplerAddressMode InAddressW = AM_Wrap,
		float InMipBias = 0,
		int32 InMaxAnisotropy = 0,
		float InMinMipLevel = 0,
		float InMaxMipLevel = FLT_MAX,
		uint32 InBorderColor = 0,
		/** Only supported in D3D11 */
		ESamplerCompareFunction InSamplerComparisonFunction = SCF_Never
		)
	:	Filter(InFilter)
	,	AddressU(InAddressU)
	,	AddressV(InAddressV)
	,	AddressW(InAddressW)
	,	MipBias(InMipBias)
	,	MinMipLevel(InMinMipLevel)
	,	MaxMipLevel(InMaxMipLevel)
	,	MaxAnisotropy(InMaxAnisotropy)
	,	BorderColor(InBorderColor)
	,	SamplerComparisonFunction(InSamplerComparisonFunction)
	{
	}
	TEnumAsByte<ESamplerFilter> Filter;
	TEnumAsByte<ESamplerAddressMode> AddressU;
	TEnumAsByte<ESamplerAddressMode> AddressV;
	TEnumAsByte<ESamplerAddressMode> AddressW;
	float MipBias;
	/** Smallest mip map level that will be used, where 0 is the highest resolution mip level. */
	float MinMipLevel;
	/** Largest mip map level that will be used, where 0 is the highest resolution mip level. */
	float MaxMipLevel;
	int32 MaxAnisotropy;
	uint32 BorderColor;
	TEnumAsByte<ESamplerCompareFunction> SamplerComparisonFunction;

	friend FArchive& operator<<(FArchive& Ar,FSamplerStateInitializerRHI& SamplerStateInitializer)
	{
		Ar << SamplerStateInitializer.Filter;
		Ar << SamplerStateInitializer.AddressU;
		Ar << SamplerStateInitializer.AddressV;
		Ar << SamplerStateInitializer.AddressW;
		Ar << SamplerStateInitializer.MipBias;
		Ar << SamplerStateInitializer.MinMipLevel;
		Ar << SamplerStateInitializer.MaxMipLevel;
		Ar << SamplerStateInitializer.MaxAnisotropy;
		Ar << SamplerStateInitializer.BorderColor;
		Ar << SamplerStateInitializer.SamplerComparisonFunction;
		return Ar;
	}
};

struct FRasterizerStateInitializerRHI
{
	TEnumAsByte<ERasterizerFillMode> FillMode;
	TEnumAsByte<ERasterizerCullMode> CullMode;
	float DepthBias;
	float SlopeScaleDepthBias;
	bool bAllowMSAA;
	bool bEnableLineAA;
	
	friend FArchive& operator<<(FArchive& Ar,FRasterizerStateInitializerRHI& RasterizerStateInitializer)
	{
		Ar << RasterizerStateInitializer.FillMode;
		Ar << RasterizerStateInitializer.CullMode;
		Ar << RasterizerStateInitializer.DepthBias;
		Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
		Ar << RasterizerStateInitializer.bAllowMSAA;
		Ar << RasterizerStateInitializer.bEnableLineAA;
		return Ar;
	}
};

struct FDepthStencilStateInitializerRHI
{
	bool bEnableDepthWrite;
	TEnumAsByte<ECompareFunction> DepthTest;

	bool bEnableFrontFaceStencil;
	TEnumAsByte<ECompareFunction> FrontFaceStencilTest;
	TEnumAsByte<EStencilOp> FrontFaceStencilFailStencilOp;
	TEnumAsByte<EStencilOp> FrontFaceDepthFailStencilOp;
	TEnumAsByte<EStencilOp> FrontFacePassStencilOp;
	bool bEnableBackFaceStencil;
	TEnumAsByte<ECompareFunction> BackFaceStencilTest;
	TEnumAsByte<EStencilOp> BackFaceStencilFailStencilOp;
	TEnumAsByte<EStencilOp> BackFaceDepthFailStencilOp;
	TEnumAsByte<EStencilOp> BackFacePassStencilOp;
	uint8 StencilReadMask;
	uint8 StencilWriteMask;

	FDepthStencilStateInitializerRHI(
		bool bInEnableDepthWrite = true,
		ECompareFunction InDepthTest = CF_LessEqual,
		bool bInEnableFrontFaceStencil = false,
		ECompareFunction InFrontFaceStencilTest = CF_Always,
		EStencilOp InFrontFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InFrontFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InFrontFacePassStencilOp = SO_Keep,
		bool bInEnableBackFaceStencil = false,
		ECompareFunction InBackFaceStencilTest = CF_Always,
		EStencilOp InBackFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InBackFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InBackFacePassStencilOp = SO_Keep,
		uint8 InStencilReadMask = 0xFF,
		uint8 InStencilWriteMask = 0xFF
		)
	: bEnableDepthWrite(bInEnableDepthWrite)
	, DepthTest(InDepthTest)
	, bEnableFrontFaceStencil(bInEnableFrontFaceStencil)
	, FrontFaceStencilTest(InFrontFaceStencilTest)
	, FrontFaceStencilFailStencilOp(InFrontFaceStencilFailStencilOp)
	, FrontFaceDepthFailStencilOp(InFrontFaceDepthFailStencilOp)
	, FrontFacePassStencilOp(InFrontFacePassStencilOp)
	, bEnableBackFaceStencil(bInEnableBackFaceStencil)
	, BackFaceStencilTest(InBackFaceStencilTest)
	, BackFaceStencilFailStencilOp(InBackFaceStencilFailStencilOp)
	, BackFaceDepthFailStencilOp(InBackFaceDepthFailStencilOp)
	, BackFacePassStencilOp(InBackFacePassStencilOp)
	, StencilReadMask(InStencilReadMask)
	, StencilWriteMask(InStencilWriteMask)
	{}
	
	friend FArchive& operator<<(FArchive& Ar,FDepthStencilStateInitializerRHI& DepthStencilStateInitializer)
	{
		Ar << DepthStencilStateInitializer.bEnableDepthWrite;
		Ar << DepthStencilStateInitializer.DepthTest;
		Ar << DepthStencilStateInitializer.bEnableFrontFaceStencil;
		Ar << DepthStencilStateInitializer.FrontFaceStencilTest;
		Ar << DepthStencilStateInitializer.FrontFaceStencilFailStencilOp;
		Ar << DepthStencilStateInitializer.FrontFaceDepthFailStencilOp;
		Ar << DepthStencilStateInitializer.FrontFacePassStencilOp;
		Ar << DepthStencilStateInitializer.bEnableBackFaceStencil;
		Ar << DepthStencilStateInitializer.BackFaceStencilTest;
		Ar << DepthStencilStateInitializer.BackFaceStencilFailStencilOp;
		Ar << DepthStencilStateInitializer.BackFaceDepthFailStencilOp;
		Ar << DepthStencilStateInitializer.BackFacePassStencilOp;
		Ar << DepthStencilStateInitializer.StencilReadMask;
		Ar << DepthStencilStateInitializer.StencilWriteMask;
		return Ar;
	}
	RHI_API FString ToString() const;
	RHI_API void FromString(const FString& Src);


};

class FBlendStateInitializerRHI
{
public:

	struct FRenderTarget
	{
		enum
		{
			NUM_STRING_FIELDS = 7
		};
		TEnumAsByte<EBlendOperation> ColorBlendOp;
		TEnumAsByte<EBlendFactor> ColorSrcBlend;
		TEnumAsByte<EBlendFactor> ColorDestBlend;
		TEnumAsByte<EBlendOperation> AlphaBlendOp;
		TEnumAsByte<EBlendFactor> AlphaSrcBlend;
		TEnumAsByte<EBlendFactor> AlphaDestBlend;
		TEnumAsByte<EColorWriteMask> ColorWriteMask;
		
		FRenderTarget(
			EBlendOperation InColorBlendOp = BO_Add,
			EBlendFactor InColorSrcBlend = BF_One,
			EBlendFactor InColorDestBlend = BF_Zero,
			EBlendOperation InAlphaBlendOp = BO_Add,
			EBlendFactor InAlphaSrcBlend = BF_One,
			EBlendFactor InAlphaDestBlend = BF_Zero,
			EColorWriteMask InColorWriteMask = CW_RGBA
			)
		: ColorBlendOp(InColorBlendOp)
		, ColorSrcBlend(InColorSrcBlend)
		, ColorDestBlend(InColorDestBlend)
		, AlphaBlendOp(InAlphaBlendOp)
		, AlphaSrcBlend(InAlphaSrcBlend)
		, AlphaDestBlend(InAlphaDestBlend)
		, ColorWriteMask(InColorWriteMask)
		{}
		
		friend FArchive& operator<<(FArchive& Ar,FRenderTarget& RenderTarget)
		{
			Ar << RenderTarget.ColorBlendOp;
			Ar << RenderTarget.ColorSrcBlend;
			Ar << RenderTarget.ColorDestBlend;
			Ar << RenderTarget.AlphaBlendOp;
			Ar << RenderTarget.AlphaSrcBlend;
			Ar << RenderTarget.AlphaDestBlend;
			Ar << RenderTarget.ColorWriteMask;
			return Ar;
		}
		RHI_API FString ToString() const;
		RHI_API void FromString(const TArray<FString>& Parts, int32 Index);


	};

	FBlendStateInitializerRHI() {}

	FBlendStateInitializerRHI(const FRenderTarget& InRenderTargetBlendState)
	:	bUseIndependentRenderTargetBlendStates(false)
	{
		RenderTargets[0] = InRenderTargetBlendState;
	}

	template<uint32 NumRenderTargets>
	FBlendStateInitializerRHI(const TStaticArray<FRenderTarget,NumRenderTargets>& InRenderTargetBlendStates)
	:	bUseIndependentRenderTargetBlendStates(NumRenderTargets > 1)
	{
		static_assert(NumRenderTargets <= MaxSimultaneousRenderTargets, "Too many render target blend states.");

		for(uint32 RenderTargetIndex = 0;RenderTargetIndex < NumRenderTargets;++RenderTargetIndex)
		{
			RenderTargets[RenderTargetIndex] = InRenderTargetBlendStates[RenderTargetIndex];
		}
	}

	TStaticArray<FRenderTarget,MaxSimultaneousRenderTargets> RenderTargets;
	bool bUseIndependentRenderTargetBlendStates;
	
	friend FArchive& operator<<(FArchive& Ar,FBlendStateInitializerRHI& BlendStateInitializer)
	{
		Ar << BlendStateInitializer.RenderTargets;
		Ar << BlendStateInitializer.bUseIndependentRenderTargetBlendStates;
		return Ar;
	}
	RHI_API FString ToString() const;
	RHI_API void FromString(const FString& Src);


};

/**
 *	Screen Resolution
 */
struct FScreenResolutionRHI
{
	uint32	Width;
	uint32	Height;
	uint32	RefreshRate;
};

/**
 *	Viewport bounds structure to set multiple view ports for the geometry shader
 *  (needs to be 1:1 to the D3D11 structure)
 */
struct FViewportBounds
{
	float	TopLeftX;
	float	TopLeftY;
	float	Width;
	float	Height;
	float	MinDepth;
	float	MaxDepth;

	FViewportBounds() {}

	FViewportBounds(float InTopLeftX, float InTopLeftY, float InWidth, float InHeight, float InMinDepth = 0.0f, float InMaxDepth = 1.0f)
		:TopLeftX(InTopLeftX), TopLeftY(InTopLeftY), Width(InWidth), Height(InHeight), MinDepth(InMinDepth), MaxDepth(InMaxDepth)
	{
	}

	friend FArchive& operator<<(FArchive& Ar,FViewportBounds& ViewportBounds)
	{
		Ar << ViewportBounds.TopLeftX;
		Ar << ViewportBounds.TopLeftY;
		Ar << ViewportBounds.Width;
		Ar << ViewportBounds.Height;
		Ar << ViewportBounds.MinDepth;
		Ar << ViewportBounds.MaxDepth;
		return Ar;
	}
};


typedef TArray<FScreenResolutionRHI>	FScreenResolutionArray;

struct FVRamAllocation
{
	FVRamAllocation(uint32 InAllocationStart = 0, uint32 InAllocationSize = 0)
		: AllocationStart(InAllocationStart)
		, AllocationSize(InAllocationSize)
	{
	}

	bool IsValid() const { return AllocationSize > 0; }

	// in bytes
	uint32 AllocationStart;
	// in bytes
	uint32 AllocationSize;
};

struct FRHIResourceInfo
{
	FVRamAllocation VRamAllocation;
};

enum class EClearBinding
{
	ENoneBound, //no clear color associated with this target.  Target will not do hardware clears on most platforms
	EColorBound, //target has a clear color bound.  Clears will use the bound color, and do hardware clears.
	EDepthStencilBound, //target has a depthstencil value bound.  Clears will use the bound values and do hardware clears.
};

struct FClearValueBinding
{
	struct DSVAlue
	{
		float Depth;
		uint32 Stencil;
	};

	FClearValueBinding()
		: ColorBinding(EClearBinding::EColorBound)
	{
		Value.Color[0] = 0.0f;
		Value.Color[1] = 0.0f;
		Value.Color[2] = 0.0f;
		Value.Color[3] = 0.0f;
	}

	FClearValueBinding(EClearBinding NoBinding)
		: ColorBinding(NoBinding)
	{
		check(ColorBinding == EClearBinding::ENoneBound);
	}

	explicit FClearValueBinding(const FLinearColor& InClearColor)
		: ColorBinding(EClearBinding::EColorBound)
	{
		Value.Color[0] = InClearColor.R;
		Value.Color[1] = InClearColor.G;
		Value.Color[2] = InClearColor.B;
		Value.Color[3] = InClearColor.A;
	}

	explicit FClearValueBinding(float DepthClearValue, uint32 StencilClearValue = 0)
		: ColorBinding(EClearBinding::EDepthStencilBound)
	{
		Value.DSValue.Depth = DepthClearValue;
		Value.DSValue.Stencil = StencilClearValue;
	}

	FLinearColor GetClearColor() const
	{
		ensure(ColorBinding == EClearBinding::EColorBound);
		return FLinearColor(Value.Color[0], Value.Color[1], Value.Color[2], Value.Color[3]);
	}

	void GetDepthStencil(float& OutDepth, uint32& OutStencil) const
	{
		ensure(ColorBinding == EClearBinding::EDepthStencilBound);
		OutDepth = Value.DSValue.Depth;
		OutStencil = Value.DSValue.Stencil;
	}

	bool operator==(const FClearValueBinding& Other) const
	{
		if (ColorBinding == Other.ColorBinding)
		{
			if (ColorBinding == EClearBinding::EColorBound)
			{
				return
					Value.Color[0] == Other.Value.Color[0] &&
					Value.Color[1] == Other.Value.Color[1] &&
					Value.Color[2] == Other.Value.Color[2] &&
					Value.Color[3] == Other.Value.Color[3];

			}
			if (ColorBinding == EClearBinding::EDepthStencilBound)
			{
				return
					Value.DSValue.Depth == Other.Value.DSValue.Depth &&
					Value.DSValue.Stencil == Other.Value.DSValue.Stencil;
			}
			return true;
		}
		return false;
	}

	EClearBinding ColorBinding;

	union ClearValueType
	{
		float Color[4];
		DSVAlue DSValue;
	} Value;

	// common clear values
	static RHI_API const FClearValueBinding None;
	static RHI_API const FClearValueBinding Black;
	static RHI_API const FClearValueBinding White;
	static RHI_API const FClearValueBinding Transparent;
	static RHI_API const FClearValueBinding DepthOne;
	static RHI_API const FClearValueBinding DepthZero;
	static RHI_API const FClearValueBinding DepthNear;
	static RHI_API const FClearValueBinding DepthFar;	
	static RHI_API const FClearValueBinding Green;
	static RHI_API const FClearValueBinding DefaultNormal8Bit;
};

struct FRHIResourceCreateInfo
{
	FRHIResourceCreateInfo()
		: BulkData(nullptr)
		, ResourceArray(nullptr)
		, ClearValueBinding(FLinearColor::Transparent)
		, DebugName(nullptr)
	{}

	// for CreateTexture calls
	FRHIResourceCreateInfo(FResourceBulkDataInterface* InBulkData)
		: BulkData(InBulkData)
		, ResourceArray(nullptr)
		, ClearValueBinding(FLinearColor::Transparent)
		, DebugName(nullptr)
	{}

	// for CreateVertexBuffer/CreateStructuredBuffer calls
	FRHIResourceCreateInfo(FResourceArrayInterface* InResourceArray)
		: BulkData(nullptr)
		, ResourceArray(InResourceArray)
		, ClearValueBinding(FLinearColor::Transparent)
		, DebugName(nullptr)
	{}

	FRHIResourceCreateInfo(const FClearValueBinding& InClearValueBinding)
		: BulkData(nullptr)
		, ResourceArray(nullptr)
		, ClearValueBinding(InClearValueBinding)
		, DebugName(nullptr)
	{
	}

	FRHIResourceCreateInfo(const TCHAR* InDebugName)
		: BulkData(nullptr)
		, ResourceArray(nullptr)
		, ClearValueBinding(FLinearColor::Transparent)
		, DebugName(InDebugName)
	{
	}

	// for CreateTexture calls
	FResourceBulkDataInterface* BulkData;
	// for CreateVertexBuffer/CreateStructuredBuffer calls
	FResourceArrayInterface* ResourceArray;

	// for binding clear colors to rendertargets.
	FClearValueBinding ClearValueBinding;
	const TCHAR* DebugName;
};

// Forward-declaration.
struct FResolveParams;

struct FResolveRect
{
	int32 X1;
	int32 Y1;
	int32 X2;
	int32 Y2;
	// e.g. for a a full 256 x 256 area starting at (0, 0) it would be 
	// the values would be 0, 0, 256, 256
	FORCEINLINE FResolveRect(int32 InX1=-1, int32 InY1=-1, int32 InX2=-1, int32 InY2=-1)
	:	X1(InX1)
	,	Y1(InY1)
	,	X2(InX2)
	,	Y2(InY2)
	{}

	FORCEINLINE FResolveRect(const FResolveRect& Other)
		: X1(Other.X1)
		, Y1(Other.Y1)
		, X2(Other.X2)
		, Y2(Other.Y2)
	{}

	bool IsValid() const
	{
		return X1 >= 0 && Y1 >= 0 && X2 - X1 > 0 && Y2 - Y1 > 0;
	}

	friend FArchive& operator<<(FArchive& Ar,FResolveRect& ResolveRect)
	{
		Ar << ResolveRect.X1;
		Ar << ResolveRect.Y1;
		Ar << ResolveRect.X2;
		Ar << ResolveRect.Y2;
		return Ar;
	}
};

struct FResolveParams
{
	/** used to specify face when resolving to a cube map texture */
	ECubeFace CubeFace;
	/** resolve RECT bounded by [X1,Y1]..[X2,Y2]. Or -1 for fullscreen */
	FResolveRect Rect;
	FResolveRect DestRect;
	/** The mip index to resolve in both source and dest. */
	int32 MipIndex;
	/** Array index to resolve in the source. */
	int32 SourceArrayIndex;
	/** Array index to resolve in the dest. */
	int32 DestArrayIndex;

	/** constructor */
	FResolveParams(
		const FResolveRect& InRect = FResolveRect(), 
		ECubeFace InCubeFace = CubeFace_PosX,
		int32 InMipIndex = 0,
		int32 InSourceArrayIndex = 0,
		int32 InDestArrayIndex = 0,
		const FResolveRect& InDestRect = FResolveRect())
		:	CubeFace(InCubeFace)
		,	Rect(InRect)
		,	DestRect(InDestRect)
		,	MipIndex(InMipIndex)
		,	SourceArrayIndex(InSourceArrayIndex)
		,	DestArrayIndex(InDestArrayIndex)
	{}

	FORCEINLINE FResolveParams(const FResolveParams& Other)
		: CubeFace(Other.CubeFace)
		, Rect(Other.Rect)
		, DestRect(Other.DestRect)
		, MipIndex(Other.MipIndex)
		, SourceArrayIndex(Other.SourceArrayIndex)
		, DestArrayIndex(Other.DestArrayIndex)
	{}
};


struct FRHICopyTextureInfo
{
	// Number of texels to copy. By default it will copy the whole resource if no size is specified.
	FIntVector Size = FIntVector::ZeroValue;

	// Position of the copy from the source texture/to destination texture
	FIntVector SourcePosition = FIntVector::ZeroValue;
	FIntVector DestPosition = FIntVector::ZeroValue;

	uint32 SourceSliceIndex = 0;
	uint32 DestSliceIndex = 0;
	uint32 NumSlices = 1;

	// Mips to copy and destination mips
	uint32 SourceMipIndex = 0;
	uint32 DestMipIndex = 0;
	uint32 NumMips = 1;
};

enum class EResourceTransitionAccess
{
	EReadable, //transition from write-> read
	EWritable, //transition from read -> write	
	ERWBarrier, // Mostly for UAVs.  Transition to read/write state and always insert a resource barrier.
	ERWNoBarrier, //Mostly UAVs.  Indicates we want R/W access and do not require synchronization for the duration of the RW state.  The initial transition from writable->RWNoBarrier and readable->RWNoBarrier still requires a sync
	ERWSubResBarrier, //For special cases where read/write happens to different subresources of the same resource in the same call.  Inserts a barrier, but read validation will pass.  Temporary until we pass full subresource info to all transition calls.
	EMetaData,		  // For transitioning texture meta data, for example for making readable in shaders
	EMaxAccess,
};

enum class EResourceAliasability
{
	EAliasable, // Make the resource aliasable with other resources
	EUnaliasable, // Make the resource unaliasable with any other resources
};

class RHI_API FResourceTransitionUtility
{
public:
	static const FString ResourceTransitionAccessStrings[(int32)EResourceTransitionAccess::EMaxAccess + 1];
};

enum class EResourceTransitionPipeline
{
	EGfxToCompute,
	EComputeToGfx,
	EGfxToGfx,
	EComputeToCompute,	
};

/** specifies an update region for a texture */
struct FUpdateTextureRegion2D
{
	/** offset in texture */
	uint32 DestX;
	uint32 DestY;
	
	/** offset in source image data */
	int32 SrcX;
	int32 SrcY;
	
	/** size of region to copy */
	uint32 Width;
	uint32 Height;

	FUpdateTextureRegion2D()
	{}

	FUpdateTextureRegion2D(uint32 InDestX, uint32 InDestY, int32 InSrcX, int32 InSrcY, uint32 InWidth, uint32 InHeight)
	:	DestX(InDestX)
	,	DestY(InDestY)
	,	SrcX(InSrcX)
	,	SrcY(InSrcY)
	,	Width(InWidth)
	,	Height(InHeight)
	{}

	friend FArchive& operator<<(FArchive& Ar,FUpdateTextureRegion2D& Region)
	{
		Ar << Region.DestX;
		Ar << Region.DestY;
		Ar << Region.SrcX;
		Ar << Region.SrcY;
		Ar << Region.Width;
		Ar << Region.Height;
		return Ar;
	}
};

/** specifies an update region for a texture */
struct FUpdateTextureRegion3D
{
	/** offset in texture */
	uint32 DestX;
	uint32 DestY;
	uint32 DestZ;

	/** offset in source image data */
	int32 SrcX;
	int32 SrcY;
	int32 SrcZ;

	/** size of region to copy */
	uint32 Width;
	uint32 Height;
	uint32 Depth;

	FUpdateTextureRegion3D()
	{}

	FUpdateTextureRegion3D(uint32 InDestX, uint32 InDestY, uint32 InDestZ, int32 InSrcX, int32 InSrcY, int32 InSrcZ, uint32 InWidth, uint32 InHeight, uint32 InDepth)
	:	DestX(InDestX)
	,	DestY(InDestY)
	,	DestZ(InDestZ)
	,	SrcX(InSrcX)
	,	SrcY(InSrcY)
	,	SrcZ(InSrcZ)
	,	Width(InWidth)
	,	Height(InHeight)
	,	Depth(InDepth)
	{}

	FUpdateTextureRegion3D(FIntVector InDest, FIntVector InSource, FIntVector InSourceSize)
		: DestX(InDest.X)
		, DestY(InDest.Y)
		, DestZ(InDest.Z)
		, SrcX(InSource.X)
		, SrcY(InSource.Y)
		, SrcZ(InSource.Z)
		, Width(InSourceSize.X)
		, Height(InSourceSize.Y)
		, Depth(InSourceSize.Z)
	{}

	friend FArchive& operator<<(FArchive& Ar,FUpdateTextureRegion3D& Region)
	{
		Ar << Region.DestX;
		Ar << Region.DestY;
		Ar << Region.DestZ;
		Ar << Region.SrcX;
		Ar << Region.SrcY;
		Ar << Region.SrcZ;
		Ar << Region.Width;
		Ar << Region.Height;
		Ar << Region.Depth;
		return Ar;
	}
};

struct FRHIDispatchIndirectParameters
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
};

struct FRHIDrawIndirectParameters
{
	uint32 VertexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartVertexLocation;
	uint32 StartInstanceLocation;
};

struct FRHIDrawIndexedIndirectParameters
{
	uint32 IndexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartIndexLocation;
	int32 BaseVertexLocation;
	uint32 StartInstanceLocation;
};


struct FTextureMemoryStats
{
	// Hardware state (never change after device creation):

	// -1 if unknown, in bytes
	int64 DedicatedVideoMemory;
	// -1 if unknown, in bytes
	int64 DedicatedSystemMemory;
	// -1 if unknown, in bytes
	int64 SharedSystemMemory;
	// Total amount of "graphics memory" that we think we can use for all our graphics resources, in bytes. -1 if unknown.
	int64 TotalGraphicsMemory;

	// Size of allocated memory, in bytes
	int64 AllocatedMemorySize;
	// Size of the largest memory fragment, in bytes
	int64 LargestContiguousAllocation;
	// 0 if streaming pool size limitation is disabled, in bytes
	int64 TexturePoolSize;
	// Upcoming adjustments to allocated memory, in bytes (async reallocations)
	int32 PendingMemoryAdjustment;

	// defaults
	FTextureMemoryStats()
		: DedicatedVideoMemory(-1)
		, DedicatedSystemMemory(-1)
		, SharedSystemMemory(-1)
		, TotalGraphicsMemory(-1)
		, AllocatedMemorySize(0)
		, LargestContiguousAllocation(0)
		, TexturePoolSize(0)
		, PendingMemoryAdjustment(0)
	{
	}

	bool AreHardwareStatsValid() const
	{
#if !PLATFORM_HTML5 // TODO: should this be tested with GRHISupportsRHIThread instead? -- seems this would be better done in SynthBenchmarkPrivate.cpp
		return (DedicatedVideoMemory >= 0 && DedicatedSystemMemory >= 0 && SharedSystemMemory >= 0);
#else
		return false;
#endif
	}

	bool IsUsingLimitedPoolSize() const
	{
		return TexturePoolSize > 0;
	}

	int64 ComputeAvailableMemorySize() const
	{
		return FMath::Max(TexturePoolSize - AllocatedMemorySize, (int64)0);
	}
};

// RHI counter stats.
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawPrimitive calls"),STAT_RHIDrawPrimitiveCalls,STATGROUP_RHI,RHI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Triangles drawn"),STAT_RHITriangles,STATGROUP_RHI,RHI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lines drawn"),STAT_RHILines,STATGROUP_RHI,RHI_API);

#if STATS
	#define RHI_DRAW_CALL_INC() \
		INC_DWORD_STAT(STAT_RHIDrawPrimitiveCalls); \
		FPlatformAtomics::InterlockedIncrement(&GCurrentNumDrawCallsRHI);

	#define RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives) \
		RHI_DRAW_CALL_INC(); \
		INC_DWORD_STAT_BY(STAT_RHITriangles,(uint32)(PrimitiveType != PT_LineList ? (NumPrimitives) : 0)); \
		INC_DWORD_STAT_BY(STAT_RHILines,(uint32)(PrimitiveType == PT_LineList ? (NumPrimitives) : 0)); \
		FPlatformAtomics::InterlockedAdd(&GCurrentNumPrimitivesDrawnRHI, NumPrimitives);
#else
	#define RHI_DRAW_CALL_INC() \
		FPlatformAtomics::InterlockedIncrement(&GCurrentNumDrawCallsRHI);

	#define RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives) \
		FPlatformAtomics::InterlockedAdd(&GCurrentNumPrimitivesDrawnRHI, NumPrimitives); \
		FPlatformAtomics::InterlockedIncrement(&GCurrentNumDrawCallsRHI);
#endif

// RHI memory stats.
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory 2D"),STAT_RenderTargetMemory2D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory 3D"),STAT_RenderTargetMemory3D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory Cube"),STAT_RenderTargetMemoryCube,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory 2D"),STAT_TextureMemory2D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory 3D"),STAT_TextureMemory3D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory Cube"),STAT_TextureMemoryCube,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Uniform buffer memory"),STAT_UniformBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Index buffer memory"),STAT_IndexBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Vertex buffer memory"),STAT_VertexBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Structured buffer memory"),STAT_StructuredBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Pixel buffer memory"),STAT_PixelBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);


// RHI base resource types.
#include "RHIResources.h"
#include "DynamicRHI.h"


/** Initializes the RHI. */
extern RHI_API void RHIInit(bool bHasEditorToken);

/** Performs additional RHI initialization before the render thread starts. */
extern RHI_API void RHIPostInit(const TArray<uint32>& InPixelFormatByteWidth);

/** Shuts down the RHI. */
extern RHI_API void RHIExit();


// the following helper macros allow to safely convert shader types without much code clutter
#define GETSAFERHISHADER_PIXEL(Shader) ((Shader) ? (Shader)->GetPixelShader() : (FPixelShaderRHIParamRef)FPixelShaderRHIRef())
#define GETSAFERHISHADER_VERTEX(Shader) ((Shader) ? (Shader)->GetVertexShader() : (FVertexShaderRHIParamRef)FVertexShaderRHIRef())
#define GETSAFERHISHADER_HULL(Shader) ((Shader) ? (Shader)->GetHullShader() : (FHullShaderRHIParamRef)FHullShaderRHIRef())
#define GETSAFERHISHADER_DOMAIN(Shader) ((Shader) ? (Shader)->GetDomainShader() : (FDomainShaderRHIParamRef)FDomainShaderRHIRef())
#define GETSAFERHISHADER_GEOMETRY(Shader) ((Shader) ? (Shader)->GetGeometryShader() : (FGeometryShaderRHIParamRef)FGeometryShaderRHIRef())
#define GETSAFERHISHADER_COMPUTE(Shader) ((Shader) ? (Shader)->GetComputeShader() : (FComputeShaderRHIParamRef)FComputeShaderRHIRef())
#define GETSAFERHISHADER_RAYTRACING(Shader) ((Shader) ? (Shader)->GetRayTracingShader() : (FRayTracingShaderRHIParamRef)FRayTracingShaderRHIRef())


// Panic delegate is called when when a fatal condition is encountered within RHI function.
DECLARE_DELEGATE_OneParam(FRHIPanicEvent, const FName&);
extern RHI_API FRHIPanicEvent& RHIGetPanicDelegate();

// RHI utility functions that depend on the RHI definitions.
#include "RHIUtilities.h"

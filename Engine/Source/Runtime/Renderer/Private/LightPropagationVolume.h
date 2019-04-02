//-----------------------------------------------------------------------------
// File:		LightPropagationVolume.h
//
// Summary:		Light Propagation Volumes implementation 
//
// Created:		2013-03-01
//
// Author:		Ben Woodhouse - mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "RendererInterface.h"

#define LPV_MULTIPLE_BOUNCES  1
#define LPV_GV_SH_ORDER		  1

class FLightSceneProxy;
class FProjectedShadowInfo;
class FSceneView;
class FViewInfo;
struct FRsmInfo;

static const int32 NUM_GV_TEXTURES = LPV_GV_SH_ORDER + 1;

class FLpvWriteUniformBufferParameters;
struct FLpvBaseWriteShaderParams;
typedef TUniformBufferRef<FLpvWriteUniformBufferParameters> FLpvWriteUniformBufferRef;
typedef TUniformBuffer<FLpvWriteUniformBufferParameters> FLpvWriteUniformBuffer;
struct FRsmInfo;

static const TCHAR * LpvVolumeTextureSRVNames[7] = { 
	TEXT("gLpv3DTexture0"),
	TEXT("gLpv3DTexture1"),
	TEXT("gLpv3DTexture2"),
	TEXT("gLpv3DTexture3"),
	TEXT("gLpv3DTexture4"),
	TEXT("gLpv3DTexture5"),
	TEXT("gLpv3DTexture6") };

static const TCHAR * LpvVolumeTextureUAVNames[7] = { 
	TEXT("gLpv3DTextureRW0"),
	TEXT("gLpv3DTextureRW1"),
	TEXT("gLpv3DTextureRW2"),
	TEXT("gLpv3DTextureRW3"),
	TEXT("gLpv3DTextureRW4"),
	TEXT("gLpv3DTextureRW5"),
	TEXT("gLpv3DTextureRW6") };

static const TCHAR * LpvGvVolumeTextureSRVNames[NUM_GV_TEXTURES] = { 
	TEXT("gGv3DTexture0"),
#if ( LPV_GV_SH_ORDER >= 1 )
	TEXT("gGv3DTexture1"),
#endif
#if ( LPV_GV_SH_ORDER >= 2 )
	TEXT("gGv3DTexture2") 
#endif
};

static const TCHAR * LpvGvVolumeTextureUAVNames[NUM_GV_TEXTURES] = { 
	TEXT("gGv3DTextureRW0"),
#if ( LPV_GV_SH_ORDER >= 1 )
	TEXT("gGv3DTextureRW1"),
#endif
#if ( LPV_GV_SH_ORDER >= 2 )
	TEXT("gGv3DTextureRW2") 
#endif
};

//
// LPV Read constant buffer
//
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FLpvReadUniformBufferParameters, )
	SHADER_PARAMETER( FIntVector, mLpvGridOffset )
	SHADER_PARAMETER( float, LpvScale )
	SHADER_PARAMETER( float, OneOverLpvScale )
	SHADER_PARAMETER( float, SpecularIntensity )
	SHADER_PARAMETER( float, DiffuseIntensity )

	SHADER_PARAMETER( float, DirectionalOcclusionIntensity )
	SHADER_PARAMETER( float, DiffuseOcclusionExponent )
	SHADER_PARAMETER( float, SpecularOcclusionExponent )
	SHADER_PARAMETER( float, SpecularOcclusionIntensity )
	SHADER_PARAMETER( float, DiffuseOcclusionIntensity )
	SHADER_PARAMETER( float, PostprocessSpecularIntensityThreshold )

	SHADER_PARAMETER( FVector, LpvGridOffsetSmooth )
	SHADER_PARAMETER( FVector, DirectionalOcclusionDefaultValue )
	SHADER_PARAMETER( float, DirectionalOcclusionFadeRange )
	SHADER_PARAMETER( float, FadeRange )
END_GLOBAL_SHADER_PARAMETER_STRUCT()


/**
 * Uniform buffer parameters for LPV write shaders
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FLpvWriteUniformBufferParameters, )
	SHADER_PARAMETER( FMatrix, mRsmToWorld )
	SHADER_PARAMETER( FVector4, mLightColour )
	SHADER_PARAMETER( FVector4, GeometryVolumeCaptureLightDirection )
	SHADER_PARAMETER( FVector4, mEyePos )
	SHADER_PARAMETER( FIntVector, mOldGridOffset ) 
	SHADER_PARAMETER( FIntVector, mLpvGridOffset )
	SHADER_PARAMETER( float, ClearMultiplier )
	SHADER_PARAMETER( float, LpvScale )
	SHADER_PARAMETER( float, OneOverLpvScale )
	SHADER_PARAMETER( float, DirectionalOcclusionIntensity )
	SHADER_PARAMETER( float, DirectionalOcclusionRadius )
	SHADER_PARAMETER( float, RsmAreaIntensityMultiplier )
	SHADER_PARAMETER( float, RsmPixelToTexcoordMultiplier )
	SHADER_PARAMETER( float, SecondaryOcclusionStrength )
	SHADER_PARAMETER( float, SecondaryBounceStrength )
	SHADER_PARAMETER( float, VplInjectionBias )
	SHADER_PARAMETER( float, GeometryVolumeInjectionBias )
	SHADER_PARAMETER( float, EmissiveInjectionMultiplier )
	SHADER_PARAMETER( int,	 PropagationIndex )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

// ----------------------------------------------------------------------------
// Shader params for base LPV write shaders
// ----------------------------------------------------------------------------
struct FLpvBaseWriteShaderParams
{
	FLpvWriteUniformBufferRef		UniformBuffer;
	FTextureRHIParamRef				LpvBufferSRVs[7];
	FUnorderedAccessViewRHIParamRef LpvBufferUAVs[7];

	FShaderResourceViewRHIParamRef	VplListHeadBufferSRV;
	FUnorderedAccessViewRHIParamRef VplListHeadBufferUAV;
	FShaderResourceViewRHIParamRef	VplListBufferSRV;
	FUnorderedAccessViewRHIParamRef VplListBufferUAV;

	FTextureRHIParamRef				GvBufferSRVs[3];
	FUnorderedAccessViewRHIParamRef GvBufferUAVs[3];

	FShaderResourceViewRHIParamRef	GvListHeadBufferSRV;
	FUnorderedAccessViewRHIParamRef GvListHeadBufferUAV;
	FShaderResourceViewRHIParamRef	GvListBufferSRV;
	FUnorderedAccessViewRHIParamRef GvListBufferUAV;

	FUnorderedAccessViewRHIParamRef AOVolumeTextureUAV;
	FTextureRHIParamRef				AOVolumeTextureSRV;
};

class FLightPropagationVolume : public FRefCountedObject
{
public:
	FLightPropagationVolume();
	virtual ~FLightPropagationVolume();

	void InitSettings(FRHICommandListImmediate& RHICmdList, const FSceneView& View);

	void Clear(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	void SetVplInjectionConstants(
		const FProjectedShadowInfo&	ProjectedShadowInfo,
		const FLightSceneProxy* LightProxy );

	void InjectDirectionalLightRSM(
		FRHICommandListImmediate& RHICmdList,
		FViewInfo&					View,
		const FTexture2DRHIRef&		RsmNormalTex, 
		const FTexture2DRHIRef&		RsmDiffuseTex, 
		const FTexture2DRHIRef&		RsmDepthTex, 
		const FProjectedShadowInfo&	ProjectedShadowInfo,
		const FLinearColor&			LightColour );

	void InjectLightDirect(FRHICommandListImmediate& RHICmdList, const FLightSceneProxy& Light, const FViewInfo& View);

	void Update(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	void Visualise(FRHICommandList& RHICmdList, const FViewInfo& View) const;

	// Copy LpvWriteUniformBufferParams into RsmUniformBuffer for parallel RSM draw-call submission
	// NOTE: Should only be called before rendering RSMs and once per frame
	void SetRsmUniformBuffer();

	const FIntVector& GetGridOffset() const { return mGridOffset; }

	const FLpvReadUniformBufferParameters& GetReadUniformBufferParams()		{ return LpvReadUniformBufferParams; }
	const FLpvWriteUniformBufferParameters& GetWriteUniformBufferParams() const	{ return *LpvWriteUniformBufferParams; }

	FLpvWriteUniformBufferRef GetWriteUniformBuffer() const				{ return LpvWriteUniformBuffer.GetUniformBufferRef(); }
	FLpvWriteUniformBufferRef GetRsmUniformBuffer() const				{ return RsmRenderUniformBuffer.GetUniformBufferRef(); }

	FTextureRHIParamRef GetLpvBufferSrv( int i )						{ return LpvVolumeTextures[ 1-mWriteBufferIndex ][i]->GetRenderTargetItem().ShaderResourceTexture; }

	FUnorderedAccessViewRHIParamRef GetVplListBufferUav()				{ return mVplListBuffer->UAV; }
	FUnorderedAccessViewRHIParamRef GetVplListHeadBufferUav()			{ return mVplListHeadBuffer->UAV; }

	FUnorderedAccessViewRHIParamRef GetGvListBufferUav()				{ return GvListBuffer->UAV; }
	FUnorderedAccessViewRHIParamRef GetGvListHeadBufferUav()			{ return GvListHeadBuffer->UAV; }

	bool IsEnabled() const												{ return bEnabled;	}
	bool IsDirectionalOcclusionEnabled() const							{ return bDirectionalOcclusionEnabled; }

	const FBox&	GetBoundingBox()										{ return BoundingBox; }

	void InsertGPUWaitForAsyncUpdate(FRHICommandListImmediate& RHICmdList);							

	void GetShaderParams( FLpvBaseWriteShaderParams& OutParams) const;
public:

	void GetShadowInfo( const FProjectedShadowInfo& ProjectedShadowInfo, FRsmInfo& RsmInfoOut );

	void ComputeDirectionalOcclusion(FRHICommandListImmediate& RHICmdList, FViewInfo& View);
	FTextureRHIParamRef GetAOVolumeTextureSRV() { return AOVolumeTexture->GetRenderTargetItem().ShaderResourceTexture; }

	TRefCountPtr<IPooledRenderTarget>	LpvVolumeTextures[2][7];		// double buffered
	FRWByteAddressBuffer*				mVplListHeadBuffer;
	FRWBufferStructured*				mVplListBuffer;

	FIntVector							mGridOffset;
	FIntVector							mOldGridOffset;

	FLpvWriteUniformBufferParameters*	LpvWriteUniformBufferParams;
	FLpvReadUniformBufferParameters     LpvReadUniformBufferParams;

	uint32								mInjectedLightCount;

	// Geometry volume
	FRWByteAddressBuffer*				GvListHeadBuffer;
	FRWBufferStructured*				GvListBuffer;

	FShaderResourceParameter			LpvVolumeTextureSampler;

	TRefCountPtr<IPooledRenderTarget>	GvVolumeTextures[NUM_GV_TEXTURES];		// SH coeffs + RGB
	TRefCountPtr<IPooledRenderTarget>	AOVolumeTexture;

	float								SecondaryOcclusionStrength;
	float								SecondaryBounceStrength;

	float								CubeSize;
	float								Strength;
	bool								bEnabled;
	bool								bDirectionalOcclusionEnabled;
	bool								bGeometryVolumeNeeded;

	uint32								mWriteBufferIndex;
	bool								bNeedsBufferClear;

	FBox								BoundingBox;
	bool								GeometryVolumeGenerated; 

	FLpvWriteUniformBuffer				LpvWriteUniformBuffer;
	FLpvWriteUniformBuffer				RsmRenderUniformBuffer;

	bool								bInitialized;

	// only needed for Async Compute
	uint32								AsyncJobFenceID;

	friend class FLpvVisualisePS;
};


// use for render thread only
bool UseLightPropagationVolumeRT(ERHIFeatureLevel::Type InFeatureLevel);


static inline bool IsLPVSupported(EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (IsD3DPlatform(Platform, true) || IsConsolePlatform(Platform) || IsMetalPlatform(Platform));
}

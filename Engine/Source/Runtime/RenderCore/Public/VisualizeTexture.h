// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VisualizeTexture.h: Post processing visualize texture.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderResource.h"
#include "RenderGraph.h"


class RENDERCORE_API FVisualizeTexture : public FRenderResource
{
public:
	FVisualizeTexture();

#if WITH_ENGINE
	virtual void ReleaseDynamicRHI() override;

	/**
	* calling this allows to grab the state of the texture at this point to be queried by visualizetexture e.g. "vis LightAttenuation@2"
	* @param PooledRenderTarget 0 is silently ignored
	* Warning: this may change the active render target and other state
	*/
#if SUPPORTS_VISUALIZE_TEXTURE
	void SetCheckPoint(FRHICommandList& RHICmdList, const IPooledRenderTarget* PooledRenderTarget);
#else
	inline void SetCheckPoint(FRHICommandList& RHICmdList, const IPooledRenderTarget* PooledRenderTarget) {}
#endif

	/** Query some information from game thread. */
	// TODO: refactor
	void QueryInfo_GameThread( FQueryVisualizeTexureInfo& Out );

	/** Sets the render  */
	void SetRenderTargetNameToObserve(const FString& InObservedDebugName, uint32 InObservedDebugNameReusedGoal = 0xffffffff);
#endif

	// VisualizeTexture console command settings:
	// written on game thread, read on render thread (uses FlushRenderingCommands to avoid the threading issues)

	// 0=off, >0=texture id, changed by "VisualizeTexture" console command, useful for debugging
	int32 Mode;
	//
	float RGBMul;

	// -1=off, 0=R, 1=G, 2=B, 3=A
	int32 SingleChannel;

	// Multiplier for the single channel
	float SingleChannelMul;

	//
	float AMul;
	// 0=view in left top, 1=whole texture, 2=pixel perfect centered, 3=Picture in Picture
	int32 UVInputMapping;
	// bit 1: if 1, saturation mode, if 0, frac mode
	int32 Flags;
	//
	int32 CustomMip;
	//
	int32 ArrayIndex;
	//
	bool bSaveBitmap;
	// stencil normally displays in the alpha channel of depth buffer visualization.  This option is just for BMP writeout to get a stencil only BMP.
	bool bOutputStencil;
	//
	bool bFullList;
	// -1:by index, 0:by name, 1:by size
	int32 SortOrder;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// [DebugName of the RT] = ReuseCount this frame
	TMap<const TCHAR*, uint32> VisualizeTextureCheckpoints;
#endif

	// render target DebugName that is observed, "" if the feature is deactivated
	FString ObservedDebugName;
	// each frame this is counting up each time a RT with the same name is reused
	uint32 ObservedDebugNameReusedCurrent;
	// this is the count we want to reach, 0xffffffff if the last one
	uint32 ObservedDebugNameReusedGoal;

private:
	// Copy of the texture being visualized.
	TRefCountPtr<IPooledRenderTarget> VisualizeTextureContent;

	/** Descriptor of the texture being visualized. */
	FPooledRenderTargetDesc VisualizeTextureDesc;

	TRefCountPtr<FRHIShaderResourceView> StencilSRV;
	FTextureRHIRef StencilSRVSrc;

	// Flag to determine whether texture visualization is enabled, currently based on the feature level we are rendering with
	bool bEnabled;

	// Store feature level that we're currently using
	ERHIFeatureLevel::Type FeatureLevel;

#if WITH_ENGINE
	bool ShouldCapture(const TCHAR* DebugName);

	/** Create a pass capturing a texture. */
	void CreateContentCapturePass(FRDGBuilder& GraphBuilder, const FRDGTextureRef Texture);
#endif

	friend class FRDGBuilder;
	friend class FVisualizeTexturePresent;
};


/** The global render targets for easy shading. */
extern RENDERCORE_API TGlobalResource<FVisualizeTexture> GVisualizeTexture;

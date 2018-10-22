// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"


/** Generic graph resource. */
class RENDERCORE_API FRDGResource
{
public:
	// Name of the resource for debugging purpose.
	const TCHAR* const Name = nullptr;

	FRDGResource(const TCHAR* InDebugName)
		: Name(InDebugName)
	{ }

	FRDGResource() = delete;
	FRDGResource(const FRDGResource&) = delete;
	void operator = (const FRDGResource&) = delete;

private:
	/** Number of references in passes and deferred queries. */
	mutable int32 ReferenceCount = 0;

	// Used for tracking resource state during execution
	mutable bool bWritable = false;
	mutable bool bCompute = false;

	/** Boolean to tracked whether a ressource is actually used by the lambda of a pass or not. */
	mutable bool bIsActuallyUsedByPass = false;

	friend class FRDGBuilder;


	/** Friendship over parameter settings for bIsActuallyUsedByPass. It means only this parameter settings can be
	 * used for render graph resource, to force the useless dependency tracking to just work.
	 */
	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Render graph tracked Texture. */
class RENDERCORE_API FRDGTexture : public FRDGResource
{
public:
	//TODO(RDG): using FDesc = FPooledRenderTargetDesc;

	/** Descriptor of the graph tracked texture. */
	const FPooledRenderTargetDesc Desc;

private:
	/** This is not a TRefCountPtr<> because FRDGTexture is allocated on the FMemStack
	 * FGraphBuilder::AllocatedTextures is actually keeping the reference.
	 */
	mutable IPooledRenderTarget* PooledRenderTarget = nullptr;

	FRDGTexture(const TCHAR* DebugName, const FPooledRenderTargetDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated RHI texture. */
	inline FTextureRHIParamRef GetRHITexture() const
	{
		check(PooledRenderTarget);
		return PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
	}

	friend class FRDGBuilder;
	friend class FRDGTextureSRV;
	friend class FRDGTextureUAV;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Decsriptor for render graph tracked SRV. */
class RENDERCORE_API FRDGTextureSRVDesc
{
public:
	const FRDGTexture* Texture;
	uint8 MipLevel = 0;

	FRDGTextureSRVDesc() {}

	FRDGTextureSRVDesc(const FRDGTexture* InTexture, uint8 InMipLevel) :
		Texture(InTexture),
		MipLevel(InMipLevel)
	{}
};

/** Render graph tracked SRV. */
class RENDERCORE_API FRDGTextureSRV : public FRDGResource
{
public:
	/** Descriptor of the graph tracked SRV. */
	const FRDGTextureSRVDesc Desc;

private:
	FRDGTextureSRV(const TCHAR* DebugName, const FRDGTextureSRVDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated  shader resource view. */
	inline FShaderResourceViewRHIParamRef GetRHIShaderResourceView() const
	{
		check(Desc.Texture);
		check(Desc.Texture->PooledRenderTarget);
		return Desc.Texture->PooledRenderTarget->GetRenderTargetItem().MipSRVs[Desc.MipLevel];
	}

	friend class FRDGBuilder;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Decsriptor for render graph tracked UAV. */
class RENDERCORE_API FRDGTextureUAVDesc
{
public:
	const FRDGTexture* Texture;
	uint8 MipLevel = 0;

	FRDGTextureUAVDesc() {}

	FRDGTextureUAVDesc(const FRDGTexture* InTexture, uint8 InMipLevel = 0) :
		Texture(InTexture),
		MipLevel(InMipLevel)
	{}
};

/** Render graph tracked UAV. */
class RENDERCORE_API FRDGTextureUAV : public FRDGResource
{
public:
	/** Descriptor of the graph tracked UAV. */
	const FRDGTextureUAVDesc Desc;

private:
	FRDGTextureUAV(const TCHAR* DebugName, const FRDGTextureUAVDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated unordered access view. */
	inline FUnorderedAccessViewRHIParamRef GetRHIUnorderedAccessView() const
	{
		check(Desc.Texture);
		check(Desc.Texture->PooledRenderTarget);
		return Desc.Texture->PooledRenderTarget->GetRenderTargetItem().MipUAVs[Desc.MipLevel];
	}

	friend class FRDGBuilder;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

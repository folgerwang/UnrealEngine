// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"


/** Generic graph resource. */
class RENDERCORE_API FGraphResource
{
public:
	// Name of the resource for debugging purpose.
	const TCHAR* const Name = nullptr;

	FGraphResource(const TCHAR* InDebugName)
		: Name(InDebugName)
	{ }

	FGraphResource() = delete;
	FGraphResource(const FGraphResource&) = delete;
	void operator = (const FGraphResource&) = delete;

private:
	/** Number of references in passes and deferred queries. */
	mutable int32 ReferenceCount = 0;

	// Used for tracking resource state during execution
	mutable bool bWritable = false;
	mutable bool bCompute = false;

	/** Boolean to tracked whether a ressource is actually used by the lambda of a pass or not. */
	mutable bool bIsActuallyUsedByPass = false;

	friend class FRenderGraphBuilder;


	/** Friendship over parameter settings for bIsActuallyUsedByPass. It means only this parameter settings can be
	 * used for render graph resource, to force the useless dependency tracking to just work.
	 */
	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Render graph tracked Texture. */
class RENDERCORE_API FGraphTexture : public FGraphResource
{
public:
	//TODO(RDG): using FDesc = FPooledRenderTargetDesc;

	/** Descriptor of the graph tracked texture. */
	const FPooledRenderTargetDesc Desc;

private:
	/** This is not a TRefCountPtr<> because FGraphTexture is allocated on the FMemStack
	 * FGraphBuilder::AllocatedTextures is actually keeping the reference.
	 */
	mutable IPooledRenderTarget* PooledRenderTarget = nullptr;

	FGraphTexture(const TCHAR* DebugName, const FPooledRenderTargetDesc& InDesc)
		: FGraphResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated RHI texture. */
	inline FTextureRHIParamRef GetRHITexture() const
	{
		check(PooledRenderTarget);
		return PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
	}

	friend class FRenderGraphBuilder;
	friend class FGraphSRV;
	friend class FGraphUAV;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Decsriptor for render graph tracked SRV. */
class RENDERCORE_API FGraphSRVDesc
{
public:
	const FGraphTexture* Texture;
	uint8 MipLevel = 0;

	FGraphSRVDesc() {}

	FGraphSRVDesc(const FGraphTexture* InTexture, uint8 InMipLevel) :
		Texture(InTexture),
		MipLevel(InMipLevel)
	{}
};

/** Render graph tracked SRV. */
class RENDERCORE_API FGraphSRV : public FGraphResource
{
public:
	/** Descriptor of the graph tracked SRV. */
	const FGraphSRVDesc Desc;

private:
	FGraphSRV(const TCHAR* DebugName, const FGraphSRVDesc& InDesc)
		: FGraphResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated  shader resource view. */
	inline FShaderResourceViewRHIParamRef GetRHIShaderResourceView() const
	{
		check(Desc.Texture);
		check(Desc.Texture->PooledRenderTarget);
		return Desc.Texture->PooledRenderTarget->GetRenderTargetItem().MipSRVs[Desc.MipLevel];
	}

	friend class FRenderGraphBuilder;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Decsriptor for render graph tracked UAV. */
class RENDERCORE_API FGraphUAVDesc
{
public:
	const FGraphTexture* Texture;
	uint8 MipLevel = 0;

	FGraphUAVDesc() {}

	FGraphUAVDesc(const FGraphTexture* InTexture, uint8 InMipLevel = 0) :
		Texture(InTexture),
		MipLevel(InMipLevel)
	{}
};

/** Render graph tracked UAV. */
class RENDERCORE_API FGraphUAV : public FGraphResource
{
public:
	/** Descriptor of the graph tracked UAV. */
	const FGraphUAVDesc Desc;

private:
	FGraphUAV(const TCHAR* DebugName, const FGraphUAVDesc& InDesc)
		: FGraphResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated unordered access view. */
	inline FUnorderedAccessViewRHIParamRef GetRHIUnorderedAccessView() const
	{
		check(Desc.Texture);
		check(Desc.Texture->PooledRenderTarget);
		return Desc.Texture->PooledRenderTarget->GetRenderTargetItem().MipUAVs[Desc.MipLevel];
	}

	friend class FRenderGraphBuilder;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

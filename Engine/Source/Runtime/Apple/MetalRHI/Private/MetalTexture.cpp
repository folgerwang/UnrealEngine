// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 MetalVertexBuffer.cpp: Metal texture RHI implementation.
 =============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h" // for STAT_MetalTexturePageOffTime
#include "MetalCommandBuffer.h"
#include "RenderUtils.h"
#include "Containers/ResourceArray.h"
#include "Misc/ScopeRWLock.h"

volatile int64 FMetalSurface::ActiveUploads = 0;

int32 GMetalMaxOutstandingAsyncTexUploads = 100 * 1024 * 1024;
FAutoConsoleVariableRef CVarMetalMaxOutstandingAsyncTexUploads(
															   TEXT("rhi.Metal.MaxOutstandingAsyncTexUploads"),
															   GMetalMaxOutstandingAsyncTexUploads,
															   TEXT("The maximum number of outstanding asynchronous texture uploads allowed to be pending in Metal. After the limit is reached the next upload will wait for all outstanding operations to complete and purge the waiting free-lists in order to reduce peak memory consumption. Defaults to 0 (infinite), set to a value > 0 limit the number."),
															   ECVF_ReadOnly|ECVF_RenderThreadSafe
															   );

enum EMetalTextureCacheMode
{
	EMetalTextureCacheModeOff = 0,
	EMetalTextureCacheModeInFrame = 1,
	EMetalTextureCacheModeAlways = 2
};

int32 GMetalTextureCacheMode = 0;
FAutoConsoleVariableRef CVarMetalTextureCacheMode(
												  TEXT("rhi.Metal.TextureCacheMode"),
												  GMetalTextureCacheMode,
												  TEXT("Set the internal texture cache mode to use in Metal.\n\t0: Off.\n\t1: Mark as volatile during streaming & either reuse within the frame or delete at the end.\n\t2: Always cache the texture object but if not reused within the frame, mark the backing store as empty to clear from VRAM. Default is 1."),
												  ECVF_ReadOnly|ECVF_RenderThreadSafe
												  );


/** Texture reference class. */
class FMetalTextureReference : public FRHITextureReference
{
public:
	explicit FMetalTextureReference(FLastRenderTimeContainer* InLastRenderTime)
	: FRHITextureReference(InLastRenderTime)
	{}
	
	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
	
	void SetReferencedTexture(FRHITexture* InTexture)
	{
		FRHITextureReference::SetReferencedTexture(InTexture);
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return GetMetalSurfaceFromRHITexture(GetReferencedTexture());
	}
};

/** Given a pointer to a RHI texture that was created by the Metal RHI, returns a pointer to the FMetalTextureBase it encapsulates. */
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture)
{
	if (!Texture)
	{
		return NULL;
	}
	else
	{
		FMetalSurface* Surface = (FMetalSurface*)Texture->GetTextureBaseRHI();
		return Surface;
	}
}

static bool IsRenderTarget(uint32 Flags)
{
	return (Flags & (TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable)) != 0;
}

static mtlpp::TextureUsage ConvertFlagsToUsage(uint32 Flags)
{
	NSUInteger Usage = mtlpp::TextureUsage::Unknown;
	if(Flags & (TexCreate_ShaderResource|TexCreate_ResolveTargetable))
	{
		Usage |= mtlpp::TextureUsage::ShaderRead;
		Usage |= mtlpp::TextureUsage::PixelFormatView;
	}
	
	if (Flags & TexCreate_UAV)
	{
		Usage |= mtlpp::TextureUsage::ShaderRead;
		Usage |= mtlpp::TextureUsage::ShaderWrite;
		Usage |= mtlpp::TextureUsage::PixelFormatView;
	}
	
	// offline textures are normal shader read textures
	if (Flags & TexCreate_OfflineProcessed)
	{
		Usage |= mtlpp::TextureUsage::ShaderRead;
	}

	//if the high level is doing manual resolves then the textures specifically markes as resolve targets
	//are likely to be used in a manual shader resolve by the high level and must be bindable as rendertargets.
	const bool bSeparateResolveTargets = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
	const bool bResolveTarget = (Flags & TexCreate_ResolveTargetable);
	if ((Flags & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable)) || (bResolveTarget && bSeparateResolveTargets))
	{
		Usage |= mtlpp::TextureUsage::RenderTarget;
		Usage |= mtlpp::TextureUsage::ShaderRead;
		Usage &= ~(mtlpp::TextureUsage::PixelFormatView);
	}
	return (mtlpp::TextureUsage)Usage;
}

static bool IsPixelFormatCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_DXT1:
		case PF_DXT3:
		case PF_DXT5:
		case PF_PVRTC2:
		case PF_PVRTC4:
		case PF_BC4:
		case PF_BC5:
		case PF_ATC_RGB:
		case PF_ATC_RGBA_E:
		case PF_ATC_RGBA_I:
		case PF_ETC1:
		case PF_ETC2_RGB:
		case PF_ETC2_RGBA:
		case PF_ASTC_4x4:
		case PF_ASTC_6x6:
		case PF_ASTC_8x8:
		case PF_ASTC_10x10:
		case PF_ASTC_12x12:
		case PF_BC6H:
		case PF_BC7:
			return true;
		default:
			return false;
	}
}

static bool IsPixelFormatASTCCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_ASTC_4x4:
		case PF_ASTC_6x6:
		case PF_ASTC_8x8:
		case PF_ASTC_10x10:
		case PF_ASTC_12x12:
			return true;
		default:
			return false;
	}
}

static bool IsPixelFormatPVRTCCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_PVRTC2:
		case PF_PVRTC4:
		case PF_ATC_RGB:
		case PF_ATC_RGBA_E:
		case PF_ATC_RGBA_I:
		case PF_ETC1:
		case PF_ETC2_RGB:
		case PF_ETC2_RGBA:
			return true;
		default:
			return false;
	}
}

void SafeReleaseMetalTexture(FMetalSurface* Surface, FMetalTexture& Texture, bool bTextureView, bool bAVFoundationTexture)
{
	if(GIsMetalInitialized && GDynamicRHI)
	{
		if (!bAVFoundationTexture && !bTextureView)
		{
			GetMetalDeviceContext().ReleaseTexture(Surface, Texture);
		}
		else
		{
			SafeReleaseMetalObject([Texture.GetPtr() retain]);
		}
	}
}

void SafeReleaseMetalTexture(FMetalSurface* Surface, FMetalTexture& Texture)
{
	if(GIsMetalInitialized && GDynamicRHI)
	{
		GetMetalDeviceContext().ReleaseTexture(Surface, Texture);
	}
}

void FMetalSurface::PrepareTextureView()
{
	// Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	mtlpp::TextureUsage Usage = (mtlpp::TextureUsage)Texture.GetUsage();
	if(!(Usage & mtlpp::TextureUsage::PixelFormatView))
	{
		check(!bTextureView);
		check(ImageSurfaceRef == nullptr);
		
		if (StencilTexture && (StencilTexture != Texture))
		{
			FMetalTexture OldStencilTexture = StencilTexture;
			StencilTexture = Reallocate(StencilTexture, mtlpp::TextureUsage::PixelFormatView);
			SafeReleaseMetalTexture(this, OldStencilTexture, bTextureView, ImageSurfaceRef != nullptr);
		}
		
		check(Texture);
		const bool bMSAATextureIsTexture = MSAATexture == Texture;
		const bool bMSAAResolveTextureIsTexture = MSAAResolveTexture == Texture;
		if (MSAATexture && !bMSAATextureIsTexture)
		{
			FMetalTexture OldMSAATexture = MSAATexture;
			MSAATexture = Reallocate(MSAATexture, mtlpp::TextureUsage::PixelFormatView);
			SafeReleaseMetalTexture(this, OldMSAATexture, bTextureView, ImageSurfaceRef != nullptr);
		}
		if (MSAAResolveTexture && !bMSAAResolveTextureIsTexture)
		{
			FMetalTexture OldMSAAResolveTexture = MSAAResolveTexture;
			MSAAResolveTexture = Reallocate(MSAAResolveTexture, mtlpp::TextureUsage::PixelFormatView);
			SafeReleaseMetalTexture(this, OldMSAAResolveTexture, bTextureView, ImageSurfaceRef != nullptr);
		}
		
		FMetalTexture OldTexture = Texture;
		Texture = Reallocate(Texture, mtlpp::TextureUsage::PixelFormatView);
		SafeReleaseMetalTexture(this, OldTexture, bTextureView, ImageSurfaceRef != nullptr);
		
		if (bMSAATextureIsTexture)
		{
			MSAATexture = Texture;
		}
		if (bMSAAResolveTextureIsTexture)
		{
			MSAAResolveTexture = Texture;
		}
	}
}

FMetalTexture FMetalSurface::Reallocate(FMetalTexture InTexture, mtlpp::TextureUsage UsageModifier)
{
	mtlpp::TextureDescriptor Desc;
	Desc.SetTextureType(InTexture.GetTextureType());
	Desc.SetPixelFormat(InTexture.GetPixelFormat());
	Desc.SetWidth(InTexture.GetWidth());
	Desc.SetHeight(InTexture.GetHeight());
	Desc.SetDepth(InTexture.GetDepth());
	Desc.SetMipmapLevelCount(InTexture.GetMipmapLevelCount());
	Desc.SetSampleCount(InTexture.GetSampleCount());
	Desc.SetArrayLength(InTexture.GetArrayLength());
	
	static mtlpp::ResourceOptions GeneralResourceOption = (mtlpp::ResourceOptions)GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions(mtlpp::ResourceOptions::HazardTrackingModeUntracked);
	
	Desc.SetResourceOptions(mtlpp::ResourceOptions(((NSUInteger)InTexture.GetCpuCacheMode() << mtlpp::ResourceCpuCacheModeShift) | ((NSUInteger)Texture.GetStorageMode() << mtlpp::ResourceStorageModeShift) | GeneralResourceOption));
	Desc.SetCpuCacheMode(InTexture.GetCpuCacheMode());
	Desc.SetStorageMode(InTexture.GetStorageMode());
	Desc.SetUsage(mtlpp::TextureUsage(InTexture.GetUsage() | UsageModifier));
	
	FMetalTexture NewTex = GetMetalDeviceContext().CreateTexture(this, Desc);
	check(NewTex);
	return NewTex;
}

void FMetalSurface::ReplaceTexture(FMetalContext& Context, FMetalTexture CurrentTexture, FMetalTexture NewTexture)
{
	check(NewTexture);
	{
		// Copy texture content...
		const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
		const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
		const uint32 BlockSizeZ = GPixelFormats[PixelFormat].BlockSizeZ;
		
		mtlpp::Origin Origin(0,0,0);
		
		// DXT/BC formats on Mac actually do have mip-tails that are smaller than the block size, they end up being uncompressed.
		bool const bPixelFormatASTC = IsPixelFormatASTCCompressed(PixelFormat);
		
		for (uint32 SliceIndex = 0; SliceIndex < CurrentTexture.GetArrayLength(); SliceIndex++)
		{
			for (uint32 MipIndex = 0; MipIndex < CurrentTexture.GetMipmapLevelCount(); ++MipIndex)
			{
				const uint32 UnalignedMipSizeX = FMath::Max<uint32>(1, CurrentTexture.GetWidth() >> (MipIndex));
				const uint32 UnalignedMipSizeY = FMath::Max<uint32>(1, CurrentTexture.GetHeight() >> (MipIndex));
				const uint32 UnalignedMipSizeZ = FMath::Max<uint32>(1, CurrentTexture.GetDepth() >> (MipIndex));
				const uint32 MipSizeX = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeX, BlockSizeX) : UnalignedMipSizeX;
				const uint32 MipSizeY = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeY, BlockSizeY) : UnalignedMipSizeY;
				const uint32 MipSizeZ = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeZ, BlockSizeZ) : UnalignedMipSizeZ;
				
				Context.CopyFromTextureToTexture(CurrentTexture, SliceIndex, MipIndex, Origin, mtlpp::Size(MipSizeX, MipSizeY, MipSizeZ), NewTexture, SliceIndex, MipIndex, Origin);
			}
		}
		
	}
	
	FMetalTexture OldTexture = Texture;
	if (Texture && Texture == CurrentTexture)
	{
		SafeReleaseMetalTexture(OldTexture);
		Texture = NewTexture;
	}
	if (StencilTexture && StencilTexture == CurrentTexture)
	{
		if (StencilTexture && (StencilTexture != OldTexture))
		{
			FMetalTexture OldStencilTexture = StencilTexture;
			SafeReleaseMetalTexture(OldStencilTexture);
			StencilTexture = NewTexture;
		}
		else if(StencilTexture)
		{
			StencilTexture = NewTexture;
		}
	}
	if (MSAATexture && MSAATexture == CurrentTexture)
	{
		if (MSAATexture && (MSAATexture != OldTexture))
		{
			FMetalTexture OldMSAATexture = MSAATexture;
			SafeReleaseMetalTexture(OldMSAATexture);
			MSAATexture = NewTexture;
		}
		else if(MSAATexture)
		{
			MSAATexture = NewTexture;
		}
		
		if (MSAAResolveTexture && MSAAResolveTexture == CurrentTexture)
		{
			if (MSAAResolveTexture && (MSAAResolveTexture != OldTexture))
			{
				FMetalTexture OldMSAAResolveTexture = MSAAResolveTexture;
				SafeReleaseMetalTexture(OldMSAAResolveTexture);
				MSAAResolveTexture = NewTexture;
			}
			else if (MSAAResolveTexture)
			{
				MSAAResolveTexture = NewTexture;
			}
		}
	}
	
	for (FMetalShaderResourceView* SRV : SRVs)
	{
		if(OldTexture != SRV->TextureView->Texture)
		{
			SafeReleaseMetalTexture(nullptr, SRV->TextureView->Texture, true, false);
		}
		SRV->TextureView->Texture = nil;
		
		SRV->TextureView->StencilTexture = nil;
		
		SRV->TextureView->MSAATexture = nil;
		
		if (SRV->Format == PF_Unknown)
		{
			SRV->TextureView->Init(*this, NSMakeRange(SRV->MipLevel, SRV->NumMips));
		}
		else
		{
			SRV->TextureView->Init(*this, NSMakeRange(SRV->MipLevel, SRV->NumMips), (EPixelFormat)SRV->Format);
		}
	}
}

void FMetalSurface::MakeAliasable(void)
{
	check(!bTextureView);
	check(ImageSurfaceRef == nullptr);
	
	static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
	if (bSupportsHeaps && Texture.GetStorageMode() == mtlpp::StorageMode::Private && Texture.GetHeap())
	{
		if (StencilTexture && (StencilTexture != Texture) && !StencilTexture.IsAliasable())
		{
			StencilTexture.MakeAliasable();
		}
		if (MSAATexture && (MSAATexture != Texture) && !MSAATexture.IsAliasable())
		{
			MSAATexture.MakeAliasable();
		}
		if (!Texture.IsAliasable())
		{
			Texture.MakeAliasable();
		}
	}
}

void FMetalSurface::MakeUnAliasable(void)
{
	check(!bTextureView);
	check(ImageSurfaceRef == nullptr);
	
	static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
	if (bSupportsHeaps && Texture.GetStorageMode() == mtlpp::StorageMode::Private && Texture.GetHeap())
	{
		FMetalTexture OldTexture = Texture;
		Texture = Reallocate(Texture, mtlpp::TextureUsage::Unknown);
		SafeReleaseMetalTexture(this, OldTexture);
		if (StencilTexture && (StencilTexture != OldTexture))
		{
			FMetalTexture OldStencilTexture = StencilTexture;
			StencilTexture = Reallocate(StencilTexture, mtlpp::TextureUsage::Unknown);
			SafeReleaseMetalTexture(this, OldStencilTexture);
		}
		else if(StencilTexture)
		{
			StencilTexture = Texture;
		}
		if (MSAATexture && (MSAATexture != OldTexture))
		{
			FMetalTexture OldMSAATexture = MSAATexture;
			MSAATexture = Reallocate(MSAATexture, mtlpp::TextureUsage::Unknown);
			SafeReleaseMetalTexture(this, OldMSAATexture);
		}
		else if(MSAATexture)
		{
			MSAATexture = Texture;
		}
		
		for (FMetalShaderResourceView* SRV : SRVs)
		{
			if(OldTexture != SRV->TextureView->Texture)
			{
				SafeReleaseMetalTexture(this, SRV->TextureView->Texture, true, false);
			}
			SRV->TextureView->Texture = nil;
			
			SRV->TextureView->StencilTexture = nil;
			SRV->TextureView->MSAATexture = nil;
			
			if (SRV->Format == PF_Unknown)
			{
				SRV->TextureView->Init(*this, NSMakeRange(SRV->MipLevel, SRV->NumMips));
			}
			else
			{
				SRV->TextureView->Init(*this, NSMakeRange(SRV->MipLevel, SRV->NumMips), (EPixelFormat)SRV->Format);
			}
		}
	}
}

void FMetalSurface::Init(FMetalSurface& Source, NSRange MipRange)
{
	mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[PixelFormat].PlatformFormat;
	
	bool const bUseSourceTex = (Source.PixelFormat != PF_DepthStencil) && MipRange.location == 0 && MipRange.length == Source.Texture.GetMipmapLevelCount();
	
	// Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	mtlpp::TextureUsage Usage = Source.Texture.GetUsage();
	if(!(Usage & mtlpp::TextureUsage::PixelFormatView) && (Source.PixelFormat != PF_DepthStencil) && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions) && !bUseSourceTex)
	{
		Source.PrepareTextureView();
	}
	
	ns::Range Slices(0, Source.Texture.GetArrayLength() * (bIsCubemap ? 6 : 1));
	// Stencil requires a format conversion, so this will access depth only, consequently there are no mip levels other than 0, so this path requires no real special casing.
	if(Source.PixelFormat != PF_DepthStencil && !bUseSourceTex)
	{
		Texture = Source.Texture.NewTextureView(MetalFormat, Source.Texture.GetTextureType(), ns::Range(MipRange.location, MipRange.length), Slices);
	}
	else
	{
		Texture = Source.Texture;
	}
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	SizeX = FMath::Max(SizeX >> MipRange.location, BlockSizeX);
	SizeY = FMath::Max(SizeY >> MipRange.location, BlockSizeY);
	SizeZ = (Type != RRT_Texture3D) ? SizeZ : FMath::Max(SizeZ >> MipRange.location, 1u);
}

void FMetalSurface::Init(FMetalSurface& Source, NSRange MipRange, EPixelFormat Format)
{
	check(!Source.MSAATexture || Format == PF_X24_G8);
	
	mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[PixelFormat].PlatformFormat;
	
	bool const bUseSourceTex = (Source.PixelFormat != PF_DepthStencil) && Source.PixelFormat == Format && MipRange.location == 0 && MipRange.length == Source.Texture.GetMipmapLevelCount();
	
	// Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	mtlpp::TextureUsage Usage = Source.Texture.GetUsage();
	if(!(Usage & mtlpp::TextureUsage::PixelFormatView) && (Source.PixelFormat == PF_DepthStencil && Format == PF_X24_G8 && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesStencilView)) && !bUseSourceTex)
	{
		Source.PrepareTextureView();
	}
	
	ns::Range Slices(0, Source.Texture.GetArrayLength() * (bIsCubemap ? 6 : 1));
	// @todo Zebra Temporary workaround for absence of X24_G8 or equivalent to GL_STENCIL_INDEX so that the stencil part of a texture may be sampled
	// For now, if we find ourselves *requiring* this we lazily blit the stencil data out to a separate texture. radr://21813831
	if(Source.PixelFormat != PF_DepthStencil && !bUseSourceTex)
	{
		Texture = Source.Texture.NewTextureView(MetalFormat, Source.Texture.GetTextureType(), ns::Range(MipRange.location, MipRange.length), Slices);
	}
	else if (Source.PixelFormat == PF_DepthStencil && Format == PF_X24_G8 && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesStencilView) && (mtlpp::PixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat != mtlpp::PixelFormat::Depth32Float)
	{
		switch (Source.Texture.GetPixelFormat())
		{
#if PLATFORM_MAC // Currently only required on Mac as iOS uses separate textures
			case mtlpp::PixelFormat::Depth24Unorm_Stencil8:
				MetalFormat = mtlpp::PixelFormat::X24_Stencil8;
				break;
#endif
			case mtlpp::PixelFormat::Depth32Float_Stencil8:
				MetalFormat = mtlpp::PixelFormat::X32_Stencil8;
				break;
			default:
				check(false);
				break;
		}
		
		Texture = Source.Texture.NewTextureView(MetalFormat, Source.Texture.GetTextureType(), ns::Range(MipRange.location, MipRange.length), Slices);
	}
	else if(Source.PixelFormat == PF_DepthStencil && Format == PF_X24_G8)
	{
		check(Source.Type == RRT_Texture2D); // Only 2D Textures can be X24_G8 SRVs
		
		mtlpp::PixelFormat DepthStencilFormat = Source.Texture ? Source.Texture.GetPixelFormat() : mtlpp::PixelFormat::Invalid;
		
		switch(DepthStencilFormat)
		{
			case mtlpp::PixelFormat::Stencil8:
			{
				Texture = Source.Texture;
				break;
			}
			case mtlpp::PixelFormat::Depth32Float:
			{
				check(Source.StencilTexture);
				Texture = Source.StencilTexture;
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	else
	{
		Texture = Source.Texture;
	}
	
	if(Source.StencilTexture && !StencilTexture)
	{
		StencilTexture = Texture;
	}
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	SizeX = FMath::Max(SizeX >> MipRange.location, BlockSizeX);
	SizeY = FMath::Max(SizeY >> MipRange.location, BlockSizeY);
	SizeZ = (Type != RRT_Texture3D) ? SizeZ : FMath::Max(SizeZ >> MipRange.location, 1u);
}

FMetalSurface::FMetalSurface(FMetalSurface& Source, NSRange MipRange)
: Type(Source.Type)
, PixelFormat(Source.PixelFormat)
, Texture(nil)
, MSAATexture(nil)
, MSAAResolveTexture(nil)
, StencilTexture(nil)
, SizeX(Source.SizeX)
, SizeY(Source.SizeY)
, SizeZ(Source.SizeZ)
, bIsCubemap(Source.bIsCubemap)
, Flags(Source.Flags)
, WriteLock(0)
, TotalTextureSize(0)
, Viewport(nullptr)
, ImageSurfaceRef(nullptr)
, bTextureView(true)
{
	FPlatformAtomics::InterlockedExchange(&Written, Source.Written);
	
	Init(Source, MipRange);
}

FMetalSurface::FMetalSurface(FMetalSurface& Source, NSRange const MipRange, EPixelFormat Format)
: Type(Source.Type)
, PixelFormat(Format)
, Texture(nil)
, MSAATexture(nil)
, MSAAResolveTexture(nil)
, StencilTexture(nil)
, SizeX(Source.SizeX)
, SizeY(Source.SizeY)
, SizeZ(Source.SizeZ)
, bIsCubemap(Source.bIsCubemap)
, Flags(Source.Flags)
, WriteLock(0)
, TotalTextureSize(0)
, Viewport(nullptr)
, ImageSurfaceRef(nullptr)
, bTextureView(true)
{
	FPlatformAtomics::InterlockedExchange(&Written, Source.Written);
	
	Init(Source, MipRange, Format);
}

mtlpp::PixelFormat ToSRGBFormat(mtlpp::PixelFormat LinMTLFormat)
{
	mtlpp::PixelFormat MTLFormat = LinMTLFormat;
	
	switch (LinMTLFormat)
	{
		case mtlpp::PixelFormat::RGBA8Unorm:
			MTLFormat = mtlpp::PixelFormat::RGBA8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::BGRA8Unorm:
			MTLFormat = mtlpp::PixelFormat::BGRA8Unorm_sRGB;
			break;
#if PLATFORM_MAC
		case mtlpp::PixelFormat::BC1_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC1_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC2_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC2_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC3_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC3_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC7_RGBAUnorm:
			MTLFormat = mtlpp::PixelFormat::BC7_RGBAUnorm_sRGB;
			break;
#endif //PLATFORM_MAC
#if PLATFORM_IOS
		case mtlpp::PixelFormat::R8Unorm:
			MTLFormat = mtlpp::PixelFormat::R8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::PVRTC_RGBA_2BPP:
			MTLFormat = mtlpp::PixelFormat::PVRTC_RGBA_2BPP_sRGB;
			break;
		case mtlpp::PixelFormat::PVRTC_RGBA_4BPP:
			MTLFormat = mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_4x4_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_4x4_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_6x6_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_6x6_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_8x8_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_8x8_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_10x10_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_10x10_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_12x12_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_12x12_sRGB;
			break;
#endif //PLATFORM_IOS
		default:
			break;
	}
	
	return MTLFormat;
}

static TMap<uint64, uint8>& GetMetalPixelFormatKeyMap()
{
	static TMap<uint64, uint8> PixelFormatKeyMap;
	return PixelFormatKeyMap;
}

uint8 GetMetalPixelFormatKey(mtlpp::PixelFormat Format)
{
	static FRWLock Mutex;
	static uint8 NextKey = 1; // 0 is reserved for mtlpp::PixelFormat::Invalid
	FRWScopeLock Lock(Mutex, SLT_ReadOnly);
	uint8* Key = GetMetalPixelFormatKeyMap().Find((uint64)Format);
	if (Key == NULL)
	{
		Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
		Key = GetMetalPixelFormatKeyMap().Find((uint64)Format);
		if (Key == NULL)
		{
			Key = &GetMetalPixelFormatKeyMap().Add((uint64)Format, NextKey++);
			// only giving 5 bits to the key
			checkf(NextKey < 32, TEXT("Too many unique pixel formats to fit into the PipelineStateHash"));
		}
	}
	return *Key;
}

FMetalSurface::FMetalSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 NumSamples, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 InFlags, FResourceBulkDataInterface* BulkData)
: Type(ResourceType)
, PixelFormat(Format)
, Texture(nil)
, MSAATexture(nil)
, MSAAResolveTexture(nil)
, StencilTexture(nil)
, SizeX(InSizeX)
, SizeY(InSizeY)
, SizeZ(InSizeZ)
, bIsCubemap(false)
, Flags(InFlags)
, WriteLock(0)
, TotalTextureSize(0)
, Viewport(nullptr)
, ImageSurfaceRef(nullptr)
, bTextureView(false)
{
	// get a unique key for this surface's format
	TMap<uint64, uint8>& PixelFormatKeyMap = GetMetalPixelFormatKeyMap();
	if (PixelFormatKeyMap.Num() == 0)
	{
		// Add depth stencil formats fist, so we don't have to use 5 bits for them in the pipeline hash
		GetMetalPixelFormatKey(mtlpp::PixelFormat::Depth32Float);
		GetMetalPixelFormatKey(mtlpp::PixelFormat::Stencil8);
		GetMetalPixelFormatKey(mtlpp::PixelFormat::Depth32Float_Stencil8);
#if PLATFORM_MAC
		GetMetalPixelFormatKey(mtlpp::PixelFormat::Depth24Unorm_Stencil8);
		if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesDepth16))
		{
			GetMetalPixelFormatKey(mtlpp::PixelFormat::Depth16Unorm);
		}
#endif
	}
	
	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}
	
	FPlatformAtomics::InterlockedExchange(&Written, 0);
	mtlpp::PixelFormat MTLFormat = (mtlpp::PixelFormat)GPixelFormats[Format].PlatformFormat;
	
	if (Flags & TexCreate_SRGB)
	{
#if PLATFORM_MAC
		// For now R8 sRGB expansion is 2D only, log other usage for later.
		if (MTLFormat == mtlpp::PixelFormat::R8Unorm)
		{
			if (Type == RRT_Texture2D)
			{
				MTLFormat = mtlpp::PixelFormat::RGBA8Unorm;
			}
			else
			{
				UE_LOG(LogMetal, Error, TEXT("Attempting to use unsupported mtlpp::PixelFormat::R8Unorm_sRGB on Mac with texture type: %d, no format expansion will be provided so rendering errors may occur."), Type);
			}
		}
#endif
		MTLFormat = ToSRGBFormat(MTLFormat);
	}
	
	// set the key
	FormatKey = GetMetalPixelFormatKey(MTLFormat);
	
	
	// the special back buffer surface will be updated in GetMetalDeviceContext().BeginDrawingViewport - no need to set the texture here
	if (Flags & TexCreate_Presentable)
	{
		return;
	}
	
	bool bIsRenderTarget = IsRenderTarget(Flags);
	mtlpp::TextureDescriptor Desc;
	
	if (ResourceType == RRT_TextureCube)
	{
		Desc = mtlpp::TextureDescriptor::TextureCubeDescriptor(MTLFormat, SizeX, (NumMips > 1));
		bIsCubemap = true;
	}
	else if (ResourceType == RRT_Texture3D)
	{
		Desc.SetTextureType(mtlpp::TextureType::Texture3D);
		Desc.SetWidth(SizeX);
		Desc.SetHeight(SizeY);
		Desc.SetDepth(SizeZ);
		Desc.SetPixelFormat(MTLFormat);
		Desc.SetArrayLength(1);
		Desc.SetMipmapLevelCount(1);
		Desc.SetSampleCount(1);
	}
	else
	{
		Desc = mtlpp::TextureDescriptor::Texture2DDescriptor(MTLFormat, SizeX, SizeY, (NumMips > 1));
		Desc.SetDepth(SizeZ);
	}
	
	// flesh out the descriptor
	if (bArray)
	{
		Desc.SetArrayLength(ArraySize);
		if (bIsCubemap)
		{
			if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesCubemapArrays))
			{
				Desc.SetTextureType(mtlpp::TextureType::TextureCubeArray);
			}
			else
			{
				Desc.SetTextureType(mtlpp::TextureType::Texture2DArray);
				Desc.SetArrayLength(ArraySize * 6);
			}
		}
	}
	Desc.SetMipmapLevelCount(NumMips);
	
	if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions))
	{
		Desc.SetUsage(ConvertFlagsToUsage(Flags));
		
		if((Flags & TexCreate_CPUReadback) && !(Flags & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable|TexCreate_FastVRAM)))
		{
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
#if PLATFORM_MAC
			Desc.SetStorageMode(mtlpp::StorageMode::Managed);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache|mtlpp::ResourceOptions::StorageModeManaged));
#else
			Desc.SetStorageMode(mtlpp::StorageMode::Shared);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache|mtlpp::ResourceOptions::StorageModeShared));
#endif
		}
		else if(((Flags & (TexCreate_NoTiling)) && !(Flags & (TexCreate_FastVRAM|TexCreate_DepthStencilTargetable|TexCreate_RenderTargetable))))
		{
#if PLATFORM_MAC
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::WriteCombined);
			Desc.SetStorageMode(mtlpp::StorageMode::Managed);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeWriteCombined|mtlpp::ResourceOptions::StorageModeManaged));
#else
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
			Desc.SetStorageMode(mtlpp::StorageMode::Shared);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache|mtlpp::ResourceOptions::StorageModeShared));
#endif
		}
		else if (Flags & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable))
		{
			check(!(Flags & TexCreate_CPUReadback));
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
			Desc.SetStorageMode(mtlpp::StorageMode::Private);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache|mtlpp::ResourceOptions::StorageModePrivate));
		}
		else
		{
			check(!(Flags & TexCreate_CPUReadback));
#if PLATFORM_MAC
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::WriteCombined);
			Desc.SetStorageMode(mtlpp::StorageMode::Private);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeWriteCombined|mtlpp::ResourceOptions::StorageModePrivate));
#else
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
			// No private storage for PVRTC as it messes up the blit-encoder usage.
			// note: this is set to always be on and will be re-addressed in a future release
            if (IsPixelFormatPVRTCCompressed(Format))
            {
                Desc.SetStorageMode(mtlpp::StorageMode::Shared);
                Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache|mtlpp::ResourceOptions::StorageModeShared));
            }
            else
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Private);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache|mtlpp::ResourceOptions::StorageModePrivate));
			}
#endif
		}
		
		static mtlpp::ResourceOptions GeneralResourceOption = GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions(mtlpp::ResourceOptions::HazardTrackingModeUntracked);
		Desc.SetResourceOptions((mtlpp::ResourceOptions)(Desc.GetResourceOptions() | GeneralResourceOption));
	}
	
	if (BulkData != nullptr)
	{
		if (BulkData->GetResourceType() == FResourceBulkDataInterface::EBulkDataType::MediaTexture)
		{
			checkf(NumMips == 1 && ArraySize == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
			ImageSurfaceRef = (CFTypeRef)BulkData->GetResourceBulkData();
			CFRetain(ImageSurfaceRef);
			{
#if !COREVIDEO_SUPPORTS_METAL
				Texture = MTLPP_VALIDATE(mtlpp::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewTexture(Desc, CVPixelBufferGetIOSurface((CVPixelBufferRef)ImageSurfaceRef), 0));
#else
				Texture = CVMetalTextureGetTexture((CVMetalTextureRef)ImageSurfaceRef);
#endif
				
				if (Texture.GetPtr() == nil)
				{
					UE_LOG(LogMetal, Fatal, TEXT("Failed to create texture, desc %s"), *FString([Desc description]));
				}
			}
			
			BulkData->Discard();
		}
#if PLATFORM_MAC
		else if (BulkData->GetResourceType() == FResourceBulkDataInterface::EBulkDataType::VREyeBuffer)
		{
			ImageSurfaceRef = (CFTypeRef)BulkData->GetResourceBulkData();
			CFRetain(ImageSurfaceRef);
			
			Desc.SetStorageMode(mtlpp::StorageMode::Managed);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)((Desc.GetResourceOptions() & ~(mtlpp::ResourceStorageModeMask)) | mtlpp::ResourceOptions::StorageModeManaged));
			
			Texture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc iosurface : (IOSurfaceRef)ImageSurfaceRef plane : 0];
			
			if (Texture.GetPtr() == nil)
			{
				UE_LOG(LogMetal, Fatal, TEXT("Failed to create texture, desc %s"), *FString([Desc description]));
			}
			
			BulkData->Discard();
		}
#endif
	}
	else
	{
		Texture = GetMetalDeviceContext().CreateTexture(this, Desc);
		if (Texture.GetPtr() == nil)
		{
			UE_LOG(LogMetal, Fatal, TEXT("Failed to create texture, desc %s"), *FString([Desc description]));
		}
		
		// upload existing bulkdata
		if (BulkData)
		{
			UE_LOG(LogMetal, Display, TEXT("Got a bulk data texture, with %d mips"), NumMips);
			checkf(NumMips == 1&& ArraySize == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
			uint32 Stride;
			
			// lock, copy, unlock
			void* LockedData = Lock(0, 0, RLM_WriteOnly, Stride);
			FMemory::Memcpy(LockedData, BulkData->GetResourceBulkData(), BulkData->GetResourceBulkDataSize());
			Unlock(0, 0);
			
			// bulk data can be unloaded now
			BulkData->Discard();
		}
	}
	
	// calculate size of the texture
	TotalTextureSize = GetMemorySize();
	
	const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
	if (!FParse::Param(FCommandLine::Get(), TEXT("nomsaa")))
	{
		if (NumSamples > 1)
		{
			check(bIsRenderTarget);
			Desc.SetTextureType(mtlpp::TextureType::Texture2DMultisample);
			
			// allow commandline to override
			FParse::Value(FCommandLine::Get(), TEXT("msaa="), NumSamples);
			Desc.SetSampleCount(NumSamples);
			
			MSAATexture = GetMetalDeviceContext().CreateTexture(this, Desc);
			
			//device doesn't support HW depth resolve.  This case only valid on mobile renderer or
			//on Mac where RHISupportsSeparateMSAAAndResolveTextures is true.
			const bool bDepthButNoResolveSupported = Format == PF_DepthStencil && !bSupportsMSAADepthResolve;
			if (bDepthButNoResolveSupported)
			{
				Texture = MSAATexture;
				
				// we don't have the resolve texture, so we just update the memory size with the MSAA size
				TotalTextureSize = TotalTextureSize * NumSamples;
			}
			else
			{
				// an MSAA render target takes NumSamples more space, in addition to the resolve texture
				TotalTextureSize += TotalTextureSize * NumSamples;
			}
			
			if (MSAATexture != Texture)
			{
				check(!MSAAResolveTexture);
				
				//if bSupportsSeparateMSAAAndResolve then the high level expect to binds the MSAA when binding shader params.
				const bool bSupportsSeparateMSAAAndResolve = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
				if (bSupportsSeparateMSAAAndResolve)
				{
					MSAAResolveTexture = Texture;
					
					Texture = MSAATexture;
				}
				else
				{
					MSAAResolveTexture = Texture;
				}
			}
			
			//we always require an MSAAResolveTexture if MSAATexture is active.
			check(!MSAATexture || MSAAResolveTexture || bDepthButNoResolveSupported);
			
			NSLog(@"Creating %dx MSAA %d x %d %s surface", (int32)Desc.GetSampleCount(), SizeX, SizeY, (Flags & TexCreate_RenderTargetable) ? "Color" : "Depth");
			if (MSAATexture.GetPtr() == nil)
			{
				NSLog(@"Failed to create texture, desc  %@", Desc.GetPtr());
			}
		}
	}
	
	// create a stencil buffer if needed
	if (Format == PF_DepthStencil)
	{
		if(MTLFormat == mtlpp::PixelFormat::Depth32Float)
		{
			Desc.SetPixelFormat(mtlpp::PixelFormat::Stencil8);
			StencilTexture = GetMetalDeviceContext().CreateTexture(this, Desc);
			
			// 1 byte per texel
			TotalTextureSize += SizeX * SizeY;
		}
		else
		{
			StencilTexture = Texture;
			
			// 1 byte per texel
			TotalTextureSize += SizeX * SizeY;
		}
	}
	
	// track memory usage
	if (bIsRenderTarget)
	{
		GCurrentRendertargetMemorySize += Align(TotalTextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize += Align(TotalTextureSize, 1024) / 1024;
	}
	
#if STATS
	if (ResourceType == RRT_TextureCube)
	{
		if (bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TotalTextureSize);
		}
	}
	else if (ResourceType == RRT_Texture3D)
	{
		if (bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory3D, TotalTextureSize);
		}
	}
	else
	{
		if (bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D, TotalTextureSize);
		}
	}
#endif
}

@interface FMetalDeferredStats : FApplePlatformObject
{
@public
	uint64 TextureSize;
	ERHIResourceType TexType;
	bool bIsRenderTarget;
}
@end

@implementation FMetalDeferredStats
APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDeferredStats)
-(void)dealloc
{
#if STATS
	if (TexType == RRT_TextureCube)
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TextureSize);
		}
	}
	else if (TexType == RRT_Texture3D)
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory3D, TextureSize);
		}
	}
	else
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D, TextureSize);
		}
	}
#endif
	if (bIsRenderTarget)
	{
		GCurrentRendertargetMemorySize -= Align(TextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize -= Align(TextureSize, 1024) / 1024;
	}
	[super dealloc];
}
@end

FMetalSurface::~FMetalSurface()
{
	bool const bIsRenderTarget = IsRenderTarget(Flags);
	
	if (MSAATexture.GetPtr())
	{
		if (Texture.GetPtr() != MSAATexture.GetPtr())
		{
			SafeReleaseMetalTexture(this, MSAATexture, bTextureView, false);
		}
	}
	
	
	//do the same as above.  only do a [release] if it'st he same as texture.
	if (MSAAResolveTexture.GetPtr())
	{
		if (Texture.GetPtr() != MSAAResolveTexture.GetPtr())
		{
			SafeReleaseMetalTexture(this, MSAAResolveTexture, bTextureView, false);
		}
	}
	
	if (StencilTexture.GetPtr())
	{
		if (StencilTexture.GetPtr() != Texture.GetPtr())
		{
			SafeReleaseMetalTexture(this, StencilTexture, bTextureView, false);
		}
	}
	
	if (!(Flags & TexCreate_Presentable) && Texture.GetPtr())
	{
		SafeReleaseMetalTexture(this, Texture, bTextureView, (ImageSurfaceRef != nullptr));
	}
	
	MSAATexture = nil;
	MSAAResolveTexture = nil;
	Texture = nil;
	StencilTexture = nil;
	
	// track memory usage
	FMetalDeferredStats* Block = [FMetalDeferredStats new];
	Block->TexType = Type;
	Block->TextureSize = TotalTextureSize;
	Block->bIsRenderTarget = bIsRenderTarget;
	SafeReleaseMetalObject(Block);
	
	if(ImageSurfaceRef)
	{
		// CFArray can contain CFType objects and is toll-free bridged with NSArray
		CFArrayRef Temp = CFArrayCreate(kCFAllocatorSystemDefault, &ImageSurfaceRef, 1, &kCFTypeArrayCallBacks);
		SafeReleaseMetalObject((NSArray*)Temp);
		CFRelease(ImageSurfaceRef);
	}
	
	ImageSurfaceRef = nullptr;
	for(uint32 i = 0; i < 16; i++)
	{
		if(LockedMemory[i])
		{
			SafeReleaseMetalBuffer(LockedMemory[i]);
			LockedMemory[i] = nullptr;
		}
	}
}

FMetalBuffer FMetalSurface::AllocSurface(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	// Whether the device supports resource options, so we don't access invalid properties on older versions of iOS
	bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
	
	// get size and stride
	uint32 MipBytes = GetMipSize(MipIndex, &DestStride, false);
	
	// allocate some temporary memory
	mtlpp::ResourceOptions ResMode = GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::StorageModeShared | (bSupportsResourceOptions && !(PLATFORM_MAC && PixelFormat == PF_G8 && (Flags & TexCreate_SRGB)) ? mtlpp::ResourceOptions::CpuCacheModeWriteCombined : 0)));
	
	FMetalBuffer Buffer = GetMetalDeviceContext().GetResourceHeap().CreateBuffer(MipBytes, BufferOffsetAlignment, ResMode);
	
#if PLATFORM_MAC
	// Expand R8_sRGB into RGBA8_sRGB for Mac.
	if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB) && Type == RRT_Texture2D && LockMode == RLM_WriteOnly)
	{
		DestStride = FMath::Max<uint32>(SizeX >> MipIndex, 1);
	}
#endif
	
	check(Buffer);
	
	return Buffer;
}

void FMetalSurface::UpdateSurface(FMetalBuffer& Buffer, uint32 MipIndex, uint32 ArrayIndex)
{
#if STATS
	uint64 Start = FPlatformTime::Cycles64();
#endif
	check(Buffer);
	
	// Whether the device supports resource options, so we don't access invalid properties on older versions of iOS
	bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
	
	uint32 Stride;
	uint32 BytesPerImage = GetMipSize(MipIndex, &Stride, true);
	
	mtlpp::Region Region;
	if (SizeZ <= 1 || bIsCubemap)
	{
		// upload the texture to the texture slice
		Region = mtlpp::Region(0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1));
	}
	else
	{
		// upload the texture to the texture slice
		Region = mtlpp::Region(0, 0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1), FMath::Max<uint32>(SizeZ >> MipIndex, 1));
	}
#if PLATFORM_MAC
	// Expand R8_sRGB into RGBA8_sRGB for Mac.
	if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB) && Type == RRT_Texture2D)
	{
		TArray<uint8> Data;
		uint8* ExpandedMem = (uint8*)Buffer.GetContents();
		Data.Append(ExpandedMem, BytesPerImage);
		uint32 SrcStride = FMath::Max<uint32>(SizeX >> MipIndex, 1);
		for(uint y = 0; y < FMath::Max<uint32>(SizeY >> MipIndex, 1); y++)
		{
			uint8* RowDest = ExpandedMem;
			for(uint x = 0; x < FMath::Max<uint32>(SizeX >> MipIndex, 1); x++)
			{
				*(RowDest++) = Data[(y * SrcStride) + x];
				*(RowDest++) = Data[(y * SrcStride) + x];
				*(RowDest++) = Data[(y * SrcStride) + x];
				*(RowDest++) = Data[(y * SrcStride) + x];
			}
			ExpandedMem = (ExpandedMem + Stride);
		}
	}
#endif
	
	if(bSupportsResourceOptions && Texture.GetStorageMode() == mtlpp::StorageMode::Private)
	{
		SCOPED_AUTORELEASE_POOL;
		
		int64 Size = BytesPerImage * Region.size.depth * FMath::Max(1u, ArrayIndex);
		
		int64 Count = FPlatformAtomics::InterlockedAdd(&ActiveUploads, Size);
		
		bool const bWait = ((GetMetalDeviceContext().GetNumActiveContexts() == 1) && (GMetalMaxOutstandingAsyncTexUploads > 0) && (Count >= GMetalMaxOutstandingAsyncTexUploads));
		
		mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
		if (Texture.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Texture.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
		{
			Options = mtlpp::BlitOption::RowLinearPVRTC;
		}
#endif
		
		if(GetMetalDeviceContext().AsyncCopyFromBufferToTexture(Buffer, 0, Stride, BytesPerImage, Region.size, Texture, ArrayIndex, MipIndex, Region.origin, Options))
		{
			mtlpp::CommandBufferHandler ScheduledHandler = nil;
	#if STATS
			int64* Cycles = new int64;
			FPlatformAtomics::InterlockedExchange(Cycles, 0);
			ScheduledHandler = [Cycles](mtlpp::CommandBuffer const&)
			{
				FPlatformAtomics::InterlockedExchange(Cycles, FPlatformTime::Cycles64());
			};
			mtlpp::CommandBufferHandler CompletionHandler = [Size, Cycles](mtlpp::CommandBuffer const&)
	#else
			mtlpp::CommandBufferHandler CompletionHandler = [Size](mtlpp::CommandBuffer const&)
	#endif
			{
				FPlatformAtomics::InterlockedAdd(&ActiveUploads, -Size);
	#if STATS
				int64 Taken = FPlatformTime::Cycles64() - *Cycles;
				delete Cycles;
				FPlatformAtomics::InterlockedAdd(&GMetalTexturePageOnTime, Taken);
	#endif
			};
			GetMetalDeviceContext().SubmitAsyncCommands(ScheduledHandler, CompletionHandler, bWait);
			
		}
		else
		{
			mtlpp::CommandBufferHandler CompletionHandler = [Size](mtlpp::CommandBuffer const&) { FPlatformAtomics::InterlockedAdd(&ActiveUploads, -Size); };
			GetMetalDeviceContext().GetCurrentRenderPass().AddCompletionHandler(CompletionHandler);
		}
		
		GetMetalDeviceContext().ReleaseBuffer(Buffer);
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, Size);
		
		if (bWait)
		{
			GetMetalDeviceContext().ClearFreeList();
		}
	}
	else
	{
#if !PLATFORM_MAC
		if (Texture.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Texture.GetPixelFormat() <= mtlpp::PixelFormat::ETC2_RGB8A1_sRGB) // @todo zebra
		{
			Stride = 0;
			BytesPerImage = 0;
		}
#endif
		
		MTLPP_VALIDATE(mtlpp::Texture, Texture, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, ArrayIndex, MTLPP_VALIDATE(mtlpp::Buffer, Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents()), Stride, BytesPerImage));
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, BytesPerImage);
	}
	
	FPlatformAtomics::InterlockedExchange(&Written, 1);
	
#if STATS
	GMetalTexturePageOnTime += (FPlatformTime::Cycles64() - Start);
#endif
}

void* FMetalSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	// Whether the device supports resource options, so we don't access invalid properties on older versions of iOS
	bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
	
	// get size and stride
	uint32 MipBytes = GetMipSize(MipIndex, &DestStride, false);
	
	// allocate some temporary memory
	if(!LockedMemory[MipIndex])
	{
		LockedMemory[MipIndex] = AllocSurface(MipIndex, ArrayIndex, LockMode, DestStride);
	}
	
	switch(LockMode)
	{
		case RLM_ReadOnly:
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
			
			mtlpp::Region Region;
			if (SizeZ <= 1 || bIsCubemap)
			{
				// upload the texture to the texture slice
				Region = mtlpp::Region(0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1));
			}
			else
			{
				// upload the texture to the texture slice
				Region = mtlpp::Region(0, 0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1), FMath::Max<uint32>(SizeZ >> MipIndex, 1));
			}
			
			if (bSupportsResourceOptions && Texture.GetStorageMode() == mtlpp::StorageMode::Private)
			{
				GetMetalDeviceContext().CopyFromTextureToBuffer(Texture, ArrayIndex, MipIndex, Region.origin, Region.size, LockedMemory[MipIndex], 0, DestStride, MipBytes, mtlpp::BlitOption::None);
				
				//kick the current command buffer.
				GetMetalDeviceContext().SubmitCommandBufferAndWait();
			}
			else
			{
#if PLATFORM_MAC
				GetMetalDeviceContext().SynchronizeTexture(Texture, ArrayIndex, MipIndex);
				
				//kick the current command buffer.
				GetMetalDeviceContext().SubmitCommandBufferAndWait();
#endif
				
				// This block breaks the texture atlas system in Ocean, which depends on nonzero strides coming back from compressed textures. Turning off.
#if 0
				if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
				{
					// for compressed textures metal debug RT expects 0 for rowBytes and imageBytes.
					DestStride = 0;
					MipBytes = 0;
				}
#endif
				uint32 BytesPerRow = DestStride;
				if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
				{
					// for compressed textures metal debug RT expects 0 for rowBytes and imageBytes.
					BytesPerRow = 0;
					MipBytes = 0;
				}
				MTLPP_VALIDATE(mtlpp::Texture, Texture, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetBytes(MTLPP_VALIDATE(mtlpp::Buffer, LockedMemory[MipIndex], SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents()), BytesPerRow, MipBytes, Region, MipIndex, ArrayIndex));
			}
			
#if PLATFORM_MAC
			// Pack RGBA8_sRGB into R8_sRGB for Mac.
			if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB) && Type == RRT_Texture2D)
			{
				TArray<uint8> Data;
				uint8* ExpandedMem = (uint8*)LockedMemory[MipIndex].GetContents();
				Data.Append(ExpandedMem, MipBytes);
				uint32 SrcStride = DestStride;
				DestStride = FMath::Max<uint32>(SizeX >> MipIndex, 1);
				for(uint y = 0; y < FMath::Max<uint32>(SizeY >> MipIndex, 1); y++)
				{
					uint8* RowDest = ExpandedMem;
					for(uint x = 0; x < FMath::Max<uint32>(SizeX >> MipIndex, 1); x++)
					{
						*(RowDest++) = Data[(y * SrcStride) + (x * 4)];
					}
					ExpandedMem = (ExpandedMem + DestStride);
				}
			}
#endif
			
			break;
		}
		case RLM_WriteOnly:
		{
			WriteLock |= 1 << MipIndex;
			break;
		}
		default:
			check(false);
			break;
	}
	
	return LockedMemory[MipIndex].GetContents();
}

void FMetalSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
	if(WriteLock & (1 << MipIndex))
	{
		WriteLock &= ~(1 << MipIndex);
		UpdateSurface(LockedMemory[MipIndex], MipIndex, ArrayIndex);
		LockedMemory[MipIndex] = nullptr;
	}
}

void* FMetalSurface::AsyncLock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bNeedsDefaultRHIFlush)
{
	bool bDirectLock = (LockMode == RLM_ReadOnly || !GIsRHIInitialized);
	
	void* BufferData = nullptr;
	
	// Never flush for writing, it is unnecessary
	if (bDirectLock)
	{
		if (bNeedsDefaultRHIFlush)
		{
			// @todo Not all read locks need to flush either, but that'll require resource use tracking
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
		BufferData = Lock(MipIndex, ArrayIndex, LockMode, DestStride);
		
		GRHILockTracker.Lock(this, BufferData, MipIndex, 0, LockMode, bDirectLock);
	}
	else
	{
		FMetalBuffer* Buffer = new FMetalBuffer(AllocSurface(MipIndex, 0, LockMode, DestStride));
		check(Buffer && Buffer->GetPtr());
		
		BufferData = Buffer->GetContents();
		
		GRHILockTracker.Lock(this, Buffer, MipIndex, 0, LockMode, bDirectLock);
	}
	
	return BufferData;
}

struct FMetalRHICommandUnlockTextureUpdate final : public FRHICommand<FMetalRHICommandUnlockTextureUpdate>
{
	FMetalSurface* Surface;
	FMetalBuffer* UpdateData;
	uint32 MipIndex;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandUnlockTextureUpdate(FMetalSurface* InSurface, FMetalBuffer* InUpdateData, uint32 InMipIndex)
	: Surface(InSurface)
	, UpdateData(InUpdateData)
	, MipIndex(InMipIndex)
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		Surface->UpdateSurface(*UpdateData, MipIndex, 0);
	}
	
	virtual ~FMetalRHICommandUnlockTextureUpdate()
	{
		SafeReleaseMetalBuffer(*UpdateData);
		delete UpdateData;
	}
};

void FMetalSurface::AsyncUnlock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex)
{
	FRHILockTracker::FLockParams Params = GRHILockTracker.Unlock(this, MipIndex);
	bool bDoDirectUnlock = Params.bDirectLock;
	const bool bUnlockForCreate = Params.bCreateLock;
	
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread() || bDoDirectUnlock)
	{
		if (bDoDirectUnlock)
		{
			Unlock(MipIndex, ArrayIndex);
		}
		else
		{
			FMetalRHICommandUnlockTextureUpdate UpdateCommand(this, (FMetalBuffer*)Params.Buffer, Params.Offset);
			UpdateCommand.Execute(RHICmdList);
		}
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalRHICommandUnlockTextureUpdate>()) FMetalRHICommandUnlockTextureUpdate(this, (FMetalBuffer*)Params.Buffer, Params.Offset);
	}
}

uint32 FMetalSurface::GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer)
{
	// DXT/BC formats on Mac actually do have mip-tails that are smaller than the block size, they end up being uncompressed.
	bool const bPixelFormatASTC = IsPixelFormatASTCCompressed(PixelFormat);
	
	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 Alignment = 1u; // Apparently we always want natural row alignment (tightly-packed) even though the docs say iOS doesn't support it - this may be because we don't upload texture data from one contiguous buffer.
	const uint32 UnalignedMipSizeX = FMath::Max(SizeX >> MipIndex, BlockSizeX);
	const uint32 UnalignedMipSizeY = FMath::Max(SizeY >> MipIndex, BlockSizeY);
	const uint32 MipSizeX = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeX, BlockSizeX) : UnalignedMipSizeX;
	const uint32 MipSizeY = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeY, BlockSizeY) : UnalignedMipSizeY;
	
	const uint32 MipSizeZ = bSingleLayer ? 1 : FMath::Max(SizeZ >> MipIndex, 1u);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}
#if PLATFORM_MAC
	else if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB))
	{
		// RGBA_sRGB is the closest match - so expand the data.
		NumBlocksX *= 4;
	}
#endif
	
	const uint32 MipStride = NumBlocksX * BlockBytes;
	const uint32 AlignedStride = ((MipStride - 1) & ~(Alignment - 1)) + Alignment;
	
	const uint32 MipBytes = AlignedStride * NumBlocksY * MipSizeZ;
	
	if (Stride)
	{
		*Stride = AlignedStride;
	}
	
	return MipBytes;
}

uint32 FMetalSurface::GetMemorySize()
{
	// if already calculated, no need to do it again
	if (TotalTextureSize != 0)
	{
		return TotalTextureSize;
	}
	
	if (Texture.GetPtr() == nil)
	{
		return 0;
	}
	
	uint32 TotalSize = 0;
	for (uint32 MipIndex = 0; MipIndex < Texture.GetMipmapLevelCount(); MipIndex++)
	{
		TotalSize += GetMipSize(MipIndex, NULL, false);
	}
	
	return TotalSize;
}

uint32 FMetalSurface::GetNumFaces()
{
	switch (Type)
	{
		case RRT_Texture2DArray:
		case RRT_Texture3D:
		case RRT_TextureCube:
			return SizeZ * Texture.GetArrayLength();
			
		case RRT_Texture:
		case RRT_Texture2D:
		default:
			return 1;
	}
}

FMetalTexture FMetalSurface::GetDrawableTexture()
{
	if (!Texture && (Flags & TexCreate_Presentable))
	{
		check(Viewport);
		Texture = Viewport->GetDrawableTexture(EMetalViewportAccessRHI);
	}
	return Texture;
}


/*-----------------------------------------------------------------------------
 Texture allocator support.
 -----------------------------------------------------------------------------*/

void FMetalDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	if(MemoryStats.TotalGraphicsMemory > 0)
	{
		OutStats.DedicatedVideoMemory = MemoryStats.DedicatedVideoMemory;
		OutStats.DedicatedSystemMemory = MemoryStats.DedicatedSystemMemory;
		OutStats.SharedSystemMemory = MemoryStats.SharedSystemMemory;
		OutStats.TotalGraphicsMemory = MemoryStats.TotalGraphicsMemory;
	}
	else
	{
		OutStats.DedicatedVideoMemory = 0;
		OutStats.DedicatedSystemMemory = 0;
		OutStats.SharedSystemMemory = 0;
		OutStats.TotalGraphicsMemory = 0;
	}
	
	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

bool FMetalDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	NOT_SUPPORTED("RHIGetTextureMemoryVisualizeData");
	return false;
}

uint32 FMetalDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	@autoreleasepool {
		if(!TextureRHI)
		{
			return 0;
		}
		
		return GetMetalSurfaceFromRHITexture(TextureRHI)->GetMemorySize();
	}
}

/*-----------------------------------------------------------------------------
 2D texture support.
 -----------------------------------------------------------------------------*/

FTexture2DRHIRef FMetalDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalTexture2D((EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
	}
}

FTexture2DRHIRef FMetalDynamicRHI::RHICreateTextureExternal2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalTexture2D((EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
	}
}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogMetal, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	return FTexture2DRHIRef();
}

void FMetalDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	NOT_SUPPORTED("RHICopySharedMips");
}

FTexture2DArrayRHIRef FMetalDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalTexture2DArray((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
	}
}

FTexture3DRHIRef FMetalDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalTexture3D((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
	}
}

void FMetalDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	// @todo Needed for visualisation!!
	// NOT_SUPPORTED("RHIGetResourceInfo");
}

void FMetalDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SourceSurfaceRHI)
{
	@autoreleasepool {
		FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(SourceSurfaceRHI);
		if (Surf && Surf->Texture)
		{
			ImmediateContext.GetInternalContext().AsyncGenerateMipmapsForTexture(Surf->Texture);
		}
	}
}

// deferred unlock commands
struct FMetalRHICommandAsyncReallocateTexture2D final : public FRHICommand<FMetalRHICommandAsyncReallocateTexture2D>
{
	FMetalContext& Context;
	FMetalTexture2D* OldTexture;
	FMetalTexture2D* NewTexture;
	int32 NewMipCount;
	int32 NewSizeX;
	int32 NewSizeY;
	FThreadSafeCounter* RequestStatus;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandAsyncReallocateTexture2D(FMetalContext& InContext, FMetalTexture2D* InOldTexture, FMetalTexture2D* InNewTexture, int32 InNewMipCount, int32 InNewSizeX, int32 InNewSizeY, FThreadSafeCounter* InRequestStatus)
	: Context(InContext)
	, OldTexture(InOldTexture)
	, NewTexture(InNewTexture)
	, NewMipCount(InNewMipCount)
	, NewSizeX(InNewSizeX)
	, NewSizeY(InNewSizeY)
	, RequestStatus(InRequestStatus)
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		CopyMips(Context, OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
	
	static void CopyMips(FMetalContext& Context, FMetalTexture2D* OldTexture, FMetalTexture2D* NewTexture, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
	{
		// figure out what mips to schedule
		const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
		const uint32 SourceMipOffset = OldTexture->GetNumMips() - NumSharedMips;
		const uint32 DestMipOffset = NewTexture->GetNumMips() - NumSharedMips;
		
		const uint32 BlockSizeX = GPixelFormats[OldTexture->GetFormat()].BlockSizeX;
		const uint32 BlockSizeY = GPixelFormats[OldTexture->GetFormat()].BlockSizeY;
		
		// only handling straight 2D textures here
		uint32 SliceIndex = 0;
		mtlpp::Origin Origin(0,0,0);
		
		FMetalTexture Tex = OldTexture->Surface.Texture;
		
		// DXT/BC formats on Mac actually do have mip-tails that are smaller than the block size, they end up being uncompressed.
		bool const bPixelFormatASTC = IsPixelFormatASTCCompressed(OldTexture->GetFormat());
		
		bool bAsync = true;
		for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
		{
			const uint32 UnalignedMipSizeX = FMath::Max<uint32>(1, NewSizeX >> (MipIndex + DestMipOffset));
			const uint32 UnalignedMipSizeY = FMath::Max<uint32>(1, NewSizeY >> (MipIndex + DestMipOffset));
			const uint32 MipSizeX = FMath::Max<uint32>(1, NewSizeX >> (MipIndex + DestMipOffset));
			const uint32 MipSizeY = FMath::Max<uint32>(1, NewSizeY >> (MipIndex + DestMipOffset));
			
			bAsync &= Context.AsyncCopyFromTextureToTexture(OldTexture->Surface.Texture, SliceIndex, MipIndex + SourceMipOffset, Origin, mtlpp::Size(MipSizeX, MipSizeY, 1), NewTexture->Surface.Texture, SliceIndex, MipIndex + DestMipOffset, Origin);
		}
		
		// when done, decrement the counter to indicate it's safe
		mtlpp::CommandBufferHandler CompletionHandler = [Tex](mtlpp::CommandBuffer const&)
		{
		};
		
		if(bAsync)
		{
	    	// kck it off!
			Context.SubmitAsyncCommands(nil, CompletionHandler, false);
		}
		else
		{
			Context.GetCurrentRenderPass().AddCompletionHandler(CompletionHandler);
		}
		
		// Like D3D mark this as complete immediately.
		RequestStatus->Decrement();
	}
};

FTexture2DRHIRef FMetalDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	@autoreleasepool {
		FTexture2DRHIRef Result;
		
		if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
		{
			Result = GDynamicRHI->RHIAsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
		}
		else
		{
			FMetalTexture2D* OldTexture = ResourceCast(Texture2D);
			
			FMetalTexture2D* NewTexture = new FMetalTexture2D(OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), NULL, Texture2D->GetClearBinding());
			
			Result = NewTexture;
			
			new (RHICmdList.AllocCommand<FMetalRHICommandAsyncReallocateTexture2D>()) FMetalRHICommandAsyncReallocateTexture2D(ImmediateContext.GetInternalContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
		}
		
		return Result;
	}
}

ETextureReallocationStatus FMetalDynamicRHI::FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	// No need to flush - does nothing
	return GDynamicRHI->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

ETextureReallocationStatus FMetalDynamicRHI::CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	// No need to flush - does nothing
	return GDynamicRHI->RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}


FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	@autoreleasepool {
		FMetalTexture2D* OldTexture = ResourceCast(OldTextureRHI);
		
		FMetalTexture2D* NewTexture = new FMetalTexture2D(OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), NULL, OldTextureRHI->GetClearBinding());
		
		FMetalRHICommandAsyncReallocateTexture2D::CopyMips(ImmediateContext.GetInternalContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
		
		return NewTexture;
	}
}

ETextureReallocationStatus FMetalDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FMetalDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Failed;
}

void* FMetalDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	@autoreleasepool {
		check(IsInRenderingThread());
		
		FMetalTexture2D* TextureMTL = ResourceCast(Texture);
		
		void* BufferData = TextureMTL->Surface.AsyncLock(RHICmdList, MipIndex, 0, LockMode, DestStride, bNeedsDefaultRHIFlush);
		
		return BufferData;
	}
}

void FMetalDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	@autoreleasepool
	{
		check(IsInRenderingThread());
		
		FMetalTexture2D* TextureMTL = ResourceCast(Texture);
		TextureMTL->Surface.AsyncUnlock(RHICmdList, MipIndex, 0);
	}
}


void* FMetalDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalTexture2D* Texture = ResourceCast(TextureRHI);
		return Texture->Surface.Lock(MipIndex, 0, LockMode, DestStride);
	}
}

void FMetalDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalTexture2D* Texture = ResourceCast(TextureRHI);
		Texture->Surface.Unlock(MipIndex, 0);
	}
}

void* FMetalDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalTexture2DArray* Texture = ResourceCast(TextureRHI);
		return Texture->Surface.Lock(MipIndex, TextureIndex, LockMode, DestStride);
	}
}

void FMetalDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalTexture2DArray* Texture = ResourceCast(TextureRHI);
		Texture->Surface.Unlock(MipIndex, TextureIndex);
	}
}

struct FMetalRHICommandUpdateTexture2D final : public FRHICommand<FMetalRHICommandUpdateTexture2D>
{
	FMetalContext& Context;
	FMetalTexture2D* Texture;
	uint32 MipIndex;
	FUpdateTextureRegion2D UpdateRegion;
	uint32 SourcePitch;
	FMetalBuffer SourceData;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandUpdateTexture2D(FMetalContext& InContext, FMetalTexture2D* InTexture, uint32 InMipIndex, FUpdateTextureRegion2D InUpdateRegion, uint32 InSourcePitch, FMetalBuffer InSourceData)
	: Context(InContext)
	, Texture(InTexture)
	, MipIndex(InMipIndex)
	, UpdateRegion(InUpdateRegion)
	, SourcePitch(InSourcePitch)
	, SourceData(InSourceData)
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, UpdateRegion.Height*SourcePitch);
		
		UpdateFromBuffer(Context, Texture->Surface.Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
		
		FPlatformAtomics::InterlockedExchange(&Texture->Surface.Written, 1);
	}
	
	static void UpdateFromBuffer(FMetalContext& Context, FMetalTexture Tex, uint32 MipIndex, FUpdateTextureRegion2D const& UpdateRegion, uint32 SourcePitch, FMetalBuffer LockedMemory)
	{
		static bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
		
		mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height);
		
		if(bSupportsResourceOptions && Tex.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			SCOPED_AUTORELEASE_POOL;
			
			uint32 BytesPerImage = SourcePitch * UpdateRegion.Height;
			
			const uint32 BufferSize = UpdateRegion.Height*SourcePitch;
			
			mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
			if (Tex.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Tex.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
			{
				Options = mtlpp::BlitOption::RowLinearPVRTC;
			}
#endif
			if(Context.AsyncCopyFromBufferToTexture(LockedMemory, 0, SourcePitch, BytesPerImage, Region.size, Tex, 0, MipIndex, Region.origin, Options))
			{
				Context.SubmitAsyncCommands(nil, nil, false);
			}
		}
		else
		{
			MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, MTLPP_VALIDATE(mtlpp::Buffer, LockedMemory, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents()), SourcePitch, 0));
		}
		
		GetMetalDeviceContext().ReleaseBuffer(LockedMemory);
	}
};

void FMetalDynamicRHI::UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	@autoreleasepool {
		if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
		{
			this->RHIUpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
		}
		else
		{
			FMetalTexture2D* TextureMTL = ResourceCast(Texture);
			
			FMetalTexture Tex = TextureMTL->Surface.Texture;
			
			bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
			
#if PLATFORM_MAC
			// Expand R8_sRGB into RGBA8_sRGB for Mac.
			TArray<uint32> Data;
			if (Texture->GetFormat() == PF_G8 && (Texture->GetFlags() & TexCreate_SRGB))
			{
				Data.AddZeroed(UpdateRegion.Height * UpdateRegion.Width);
				uint32* Dest = Data.GetData();
				check(Dest);
				
				for(uint y = 0; y < UpdateRegion.Height; y++)
				{
					for(uint x = 0; x < UpdateRegion.Width; x++)
					{
						uint8 Value = SourceData[(y * SourcePitch) + x];
						*(Dest++) = (Value | (Value << 8) | (Value << 16) | (Value << 24));
					}
				}
				
				SourceData = (uint8*)Data.GetData();
				SourcePitch = UpdateRegion.Width * sizeof(uint32);
			}
#endif
			mtlpp::StorageMode ResMode = mtlpp::StorageMode::Shared;
			
			const uint32 BufferSize = UpdateRegion.Height*SourcePitch;
			
			FMetalBuffer LockedMemory = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, ResMode));
			
			FMemory::Memcpy(LockedMemory.GetContents(), SourceData, BufferSize);
			
			new (RHICmdList.AllocCommand<FMetalRHICommandUpdateTexture2D>()) FMetalRHICommandUpdateTexture2D(ImmediateContext.GetInternalContext(), TextureMTL, MipIndex, UpdateRegion, SourcePitch, LockedMemory);
		}
	}
}

void FMetalDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	@autoreleasepool {
		FMetalTexture2D* Texture = ResourceCast(TextureRHI);
		
		FMetalTexture Tex = Texture->Surface.Texture;
		
		mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height);
		
		bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
		
#if PLATFORM_MAC
		// Expand R8_sRGB into RGBA8_sRGB for Mac.
		TArray<uint32> Data;
		if (Texture->GetFormat() == PF_G8 && (Texture->GetFlags() & TexCreate_SRGB))
		{
			Data.AddZeroed(UpdateRegion.Height * UpdateRegion.Width);
			uint32* Dest = Data.GetData();
			check(Dest);
			
			for(uint y = 0; y < UpdateRegion.Height; y++)
			{
				for(uint x = 0; x < UpdateRegion.Width; x++)
				{
					uint8 Value = SourceData[(y * SourcePitch) + x];
					*(Dest++) = (Value | (Value << 8) | (Value << 16) | (Value << 24));
				}
			}
			
			SourceData = (uint8*)Data.GetData();
			SourcePitch = UpdateRegion.Width * sizeof(uint32);
		}
#endif
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, UpdateRegion.Height*SourcePitch);
		
		if(bSupportsResourceOptions && Tex.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			SCOPED_AUTORELEASE_POOL;
			
			uint32 BytesPerImage = SourcePitch * UpdateRegion.Height;
			
			mtlpp::StorageMode ResMode = mtlpp::StorageMode::Shared;
			
			const uint32 BufferSize = UpdateRegion.Height*SourcePitch;
			
			FMetalBuffer LockedMemory = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, ResMode));
			
			FMemory::Memcpy(LockedMemory.GetContents(), SourceData, BufferSize);
			
			mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
			if (Tex.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Tex.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
			{
				Options = mtlpp::BlitOption::RowLinearPVRTC;
			}
#endif
			if(ImmediateContext.GetInternalContext().AsyncCopyFromBufferToTexture(LockedMemory, 0, SourcePitch, BytesPerImage, Region.size, Tex, 0, MipIndex, Region.origin, Options))
			{
				ImmediateContext.GetInternalContext().SubmitAsyncCommands(nil, nil, false);
			}
			
			GetMetalDeviceContext().ReleaseBuffer(LockedMemory);
		}
		else
		{
			MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, SourceData, SourcePitch, 0));
		}
		
		FPlatformAtomics::InterlockedExchange(&Texture->Surface.Written, 1);
	}
}

struct FMetalDynamicRHIUpdateTexture3DCommand final : public FRHICommand<FMetalDynamicRHIUpdateTexture3DCommand>
{
	FTexture3DRHIParamRef DestinationTexture;
	uint32 MipIndex;
	FUpdateTextureRegion3D UpdateRegion;
	uint32 SourceRowPitch;
	uint32 SourceDepthPitch;
	uint8* Data;
	
	FORCEINLINE_DEBUGGABLE FMetalDynamicRHIUpdateTexture3DCommand(FTexture3DRHIParamRef Texture, uint32 InMipIndex, const struct FUpdateTextureRegion3D& InUpdateRegion, uint32 InSourceRowPitch, uint32 InSourceDepthPitch, const uint8* SourceData)
	: DestinationTexture(Texture)
	, MipIndex(InMipIndex)
	, UpdateRegion(InUpdateRegion)
	, SourceRowPitch(InSourceRowPitch)
	, SourceDepthPitch(InSourceDepthPitch)
	, Data(nullptr)
	{
		uint32 BytesPerImage = SourceRowPitch * UpdateRegion.Height;
		const uint32 BufferSize = UpdateRegion.Height*UpdateRegion.Depth*SourceRowPitch;
		Data = (uint8*)FMemory::Malloc(BufferSize);
		FMemory::Memcpy(Data, SourceData, BufferSize);
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		GDynamicRHI->RHIUpdateTexture3D(DestinationTexture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, Data);
		FMemory::Free(Data);
	}
};

void FMetalDynamicRHI::UpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		RHIUpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalDynamicRHIUpdateTexture3DCommand>()) FMetalDynamicRHIUpdateTexture3DCommand(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
}

FUpdateTexture3DData FMetalDynamicRHI::BeginUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInRenderingThread());
	
	const int32 FormatSize = PixelFormatBlockBytes[Texture->GetFormat()];
	const int32 RowPitch = UpdateRegion.Width * FormatSize;
	const int32 DepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;
	
	SIZE_T MemorySize = DepthPitch * UpdateRegion.Depth;
	uint8* Data = (uint8*)FMemory::Malloc(MemorySize);
	
	return FUpdateTexture3DData(Texture, MipIndex, UpdateRegion, RowPitch, DepthPitch, Data, MemorySize, GFrameNumberRenderThread);
}

void FMetalDynamicRHI::EndUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber);
	
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GDynamicRHI->RHIUpdateTexture3D(UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalDynamicRHIUpdateTexture3DCommand>()) FMetalDynamicRHIUpdateTexture3DCommand(UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	}
	
	FMemory::Free(UpdateData.Data);
	UpdateData.Data = nullptr;
}

void FMetalDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{
	@autoreleasepool {
		FMetalTexture3D* Texture = ResourceCast(TextureRHI);
		
		FMetalTexture Tex = Texture->Surface.Texture;
		
		mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth);
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, SourceRowPitch * UpdateRegion.Height * Region.size.depth);
		
		bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
		
#if PLATFORM_MAC
		checkf(!(Texture->GetFormat() == PF_G8 && (Texture->GetFlags() & TexCreate_SRGB)), TEXT("MetalRHI does not support PF_G8_sRGB on 3D, array or cube textures as it requires manual, CPU-side expansion to RGBA8_sRGB which is expensive!"));
#endif
		if(bSupportsResourceOptions && Tex.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			SCOPED_AUTORELEASE_POOL;
			
			uint32 BytesPerImage = SourceRowPitch * UpdateRegion.Height;
			
			mtlpp::StorageMode ResMode = mtlpp::StorageMode::Shared;
			const uint32 BufferSize = UpdateRegion.Height*UpdateRegion.Depth*SourceRowPitch;
			
			FMetalBuffer LockedMemory = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, ResMode));
			
			FMemory::Memcpy(LockedMemory.GetContents(), SourceData, BufferSize);
			
			mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
			if (Tex.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Tex.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
			{
				Options = mtlpp::BlitOption::RowLinearPVRTC;
			}
#endif
			if(ImmediateContext.GetInternalContext().AsyncCopyFromBufferToTexture(LockedMemory, 0, SourceRowPitch, BytesPerImage, Region.size, Tex, 0, MipIndex, Region.origin, Options))
			{
				ImmediateContext.GetInternalContext().SubmitAsyncCommands(nil, nil, false);
			}
			
			GetMetalDeviceContext().ReleaseBuffer(LockedMemory);
		}
		else
		{
			MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, SourceData, SourceRowPitch, SourceDepthPitch));
		}
		
		FPlatformAtomics::InterlockedExchange(&Texture->Surface.Written, 1);
	}
}


/*-----------------------------------------------------------------------------
 Cubemap texture support.
 -----------------------------------------------------------------------------*/
FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalTextureCube((EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
	}
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalTextureCube((EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
	}
}

void* FMetalDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
		uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
		return TextureCube->Surface.Lock(MipIndex, MetalFace + (6 * ArrayIndex), LockMode, DestStride);
	}
}

void FMetalDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
		uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
		TextureCube->Surface.Unlock(MipIndex, MetalFace + (ArrayIndex * 6));
	}
}


FTexture2DRHIRef FMetalDynamicRHI::RHICreateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		FRHIResourceCreateInfo Info = CreateInfo;
		bool const bIOSurfaceData = (CreateInfo.BulkData && CreateInfo.BulkData->GetResourceType() != FResourceBulkDataInterface::EBulkDataType::Default);
		Info.BulkData = bIOSurfaceData ? CreateInfo.BulkData : nullptr;
		FTexture2DRHIRef Result = GDynamicRHI->RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, Info);
		if (!bIOSurfaceData && CreateInfo.BulkData)
		{
			// upload existing bulkdata
			UE_LOG(LogMetal, Display, TEXT("Got a bulk data texture, with %d mips"), NumMips);
			checkf(NumMips == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
			uint32 Stride;
			
			// lock, copy, unlock
			void* LockedData = LockTexture2D_RenderThread(RHICmdList, Result.GetReference(), 0, RLM_WriteOnly, Stride, false);
			check(LockedData);
			FMemory::Memcpy(LockedData, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());
			UnlockTexture2D_RenderThread(RHICmdList, Result.GetReference(), 0, false);
			
			// bulk data can be unloaded now
			CreateInfo.BulkData->Discard();
		}
		return Result;
	}
}

FTexture2DArrayRHIRef FMetalDynamicRHI::RHICreateTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		FRHIResourceCreateInfo Info = CreateInfo;
		bool const bIOSurfaceData = (CreateInfo.BulkData && CreateInfo.BulkData->GetResourceType() != FResourceBulkDataInterface::EBulkDataType::Default);
		Info.BulkData = bIOSurfaceData ? CreateInfo.BulkData : nullptr;
		FTexture2DArrayRHIRef Result = GDynamicRHI->RHICreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, Info);
		if (CreateInfo.BulkData)
		{
			// upload existing bulkdata
			UE_LOG(LogMetal, Display, TEXT("Got a bulk data texture, with %d mips"), NumMips);
			checkf(NumMips == 1 && SizeZ == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
			uint32 Stride;
			
			// lock, copy, unlock
			FMetalTexture2DArray* TextureMTL = ResourceCast(Result.GetReference());
			void* LockedData = TextureMTL->Surface.AsyncLock(RHICmdList, 0, 0, RLM_WriteOnly, Stride, false);
			FMemory::Memcpy(LockedData, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());
			TextureMTL->Surface.AsyncUnlock(RHICmdList, 0, 0);
			
			// bulk data can be unloaded now
			CreateInfo.BulkData->Discard();
		}
		return Result;
	}
}

FTexture3DRHIRef FMetalDynamicRHI::RHICreateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		FRHIResourceCreateInfo Info = CreateInfo;
		bool const bIOSurfaceData = (CreateInfo.BulkData && CreateInfo.BulkData->GetResourceType() != FResourceBulkDataInterface::EBulkDataType::Default);
		Info.BulkData = bIOSurfaceData ? CreateInfo.BulkData : nullptr;
		FTexture3DRHIRef Result = GDynamicRHI->RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, Info);
		if (CreateInfo.BulkData)
		{
			// upload existing bulkdata
			UE_LOG(LogMetal, Display, TEXT("Got a bulk data texture, with %d mips"), NumMips);
			checkf(NumMips == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
			uint32 Stride;
			
			// lock, copy, unlock
			FMetalTexture3D* TextureMTL = ResourceCast(Result.GetReference());
			void* LockedData = TextureMTL->Surface.AsyncLock(RHICmdList, 0, 0, RLM_WriteOnly, Stride, false);
			FMemory::Memcpy(LockedData, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());
			TextureMTL->Surface.AsyncUnlock(RHICmdList, 0, 0);
			
			// bulk data can be unloaded now
			CreateInfo.BulkData->Discard();
		}
		return Result;
	}
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCube_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		check(!CreateInfo.BulkData);
		FTextureCubeRHIRef Result = GDynamicRHI->RHICreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
		return Result;
	}
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCubeArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		check(!CreateInfo.BulkData);
		FTextureCubeRHIRef Result = GDynamicRHI->RHICreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
		return Result;
	}
}




FTextureReferenceRHIRef FMetalDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* InLastRenderTime)
{
	@autoreleasepool {
		return new FMetalTextureReference(InLastRenderTime);
	}
}

void FMetalRHICommandContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRefRHI, FTextureRHIParamRef NewTextureRHI)
{
	@autoreleasepool {
		FMetalTextureReference* TextureRef = (FMetalTextureReference*)TextureRefRHI;
		if (TextureRef)
		{
			TextureRef->SetReferencedTexture(NewTextureRHI);
		}
	}
}


void FMetalDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
	@autoreleasepool {
		FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(TextureRHI);
		if(Surf->Texture)
		{
			Surf->Texture.SetLabel(FString(Name).GetNSString());
		}
		if(Surf->MSAATexture)
		{
			Surf->MSAATexture.SetLabel(FString(Name).GetNSString());
		}
		if(Surf->StencilTexture)
		{
			if(Surf->StencilTexture != Surf->Texture)
			{
				Surf->StencilTexture.SetLabel([NSString stringWithFormat:@"%@StencilSRV", FString(Name).GetNSString()]);
			}
			else
			{
				Surf->StencilTexture.SetLabel(FString(Name).GetNSString());
			}
		}
	}
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipInMemory");
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipVisible");
}

struct FMetalRHICommandUnaliasTextures final : public FRHICommand<FMetalRHICommandUnaliasTextures>
{
	TArray<FTextureRHIParamRef> Textures;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandUnaliasTextures(FTextureRHIParamRef* InTextures, int32 NumTextures)
	{
		check(InTextures && NumTextures);
		Textures.Append(InTextures, NumTextures);
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		for (int32 i = 0; i < Textures.Num(); ++i)
		{
			FMetalSurface* Source = GetMetalSurfaceFromRHITexture(Textures[i]);
			Source->MakeUnAliasable();
		}
	}
};

void FMetalDynamicRHI::RHISetResourceAliasability_RenderThread(class FRHICommandListImmediate& RHICmdList, EResourceAliasability AliasMode, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	check(InTextures && NumTextures);
	@autoreleasepool
	{
		switch(AliasMode)
		{
			case EResourceAliasability::EAliasable:
			{
				for (int32 i = 0; i < NumTextures; ++i)
				{
					FMetalSurface* Source = GetMetalSurfaceFromRHITexture(InTextures[i]);
					Source->MakeAliasable();
				}
				break;
			}
			case EResourceAliasability::EUnaliasable:
			{
				if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
				{
					for (int32 i = 0; i < NumTextures; ++i)
					{
						FMetalSurface* Source = GetMetalSurfaceFromRHITexture(InTextures[i]);
						Source->MakeUnAliasable();
					}
				}
				else
				{
					new (RHICmdList.AllocCommand<FMetalRHICommandUnaliasTextures>()) FMetalRHICommandUnaliasTextures(InTextures, NumTextures);
				}
				break;
			}
			default:
				break;
		}
	}
}

struct FRHICopySubTextureRegion final : public FRHICommand<FRHICopySubTextureRegion>
{
	FTexture2DRHIParamRef SourceTexture;
	FTexture2DRHIParamRef DestinationTexture;
	FBox2D SourceBox;
	FBox2D DestinationBox;
	
	FORCEINLINE_DEBUGGABLE FRHICopySubTextureRegion(FTexture2DRHIParamRef InSourceTexture, FTexture2DRHIParamRef InDestinationTexture, FBox2D InSourceBox, FBox2D InDestinationBox)
	: SourceTexture(InSourceTexture)
	, DestinationTexture(InDestinationTexture)
	, SourceBox(InSourceBox)
	, DestinationBox(InDestinationBox)
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		GDynamicRHI->RHICopySubTextureRegion(SourceTexture, DestinationTexture, SourceBox, DestinationBox);
	}
};

void FMetalDynamicRHI::RHICopySubTextureRegion_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox)
{
	@autoreleasepool
	{
		if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
		{
			GDynamicRHI->RHICopySubTextureRegion(SourceTexture, DestinationTexture, SourceBox, DestinationBox);
		}
		else
		{
			new (RHICmdList.AllocCommand<FRHICopySubTextureRegion>()) FRHICopySubTextureRegion(SourceTexture, DestinationTexture, SourceBox, DestinationBox);
		}
	}
}

void FMetalDynamicRHI::RHICopySubTextureRegion(FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox)
{
	@autoreleasepool {
		check(SourceTexture);
		check(DestinationTexture);
		
		if(SourceTexture->GetFormat() == DestinationTexture->GetFormat())
		{
			FMetalTexture2D* MetalSrcTexture = ResourceCast(SourceTexture);
			FMetalTexture2D* MetalDestTexture = ResourceCast(DestinationTexture);
			
			FVector2D SourceSizeVector = SourceBox.GetSize();
			FVector2D DestinatioSizeVector = DestinationBox.GetSize();
			
			mtlpp::Origin SourceOrigin(SourceBox.Min.X, SourceBox.Min.Y, 0);
			mtlpp::Size SourceSize(SourceSizeVector.X, SourceSizeVector.Y, 1);
			
			mtlpp::Origin DestinationOrigin(DestinationBox.Min.X, DestinationBox.Min.Y, 0);
			mtlpp::Size DestinationSize(DestinatioSizeVector.X, DestinatioSizeVector.Y, 1);
			
			check(DestinationSize.width == SourceSize.width);
			check(DestinationSize.height == SourceSize.height);
			
			// Account for create with TexCreate_SRGB flag which could make these different
			if(MetalSrcTexture->Surface.Texture.GetPixelFormat() == MetalDestTexture->Surface.Texture.GetPixelFormat())
			{
				ImmediateContext.GetInternalContext().CopyFromTextureToTexture(MetalSrcTexture->Surface.Texture, 0, 0, SourceOrigin,SourceSize,MetalDestTexture->Surface.Texture, 0, 0, DestinationOrigin);
			}
			else
			{
				// Linear and sRGB mismatch then try to go via metal buffer
				// Modified clone of logic from MetalRenderTarget.cpp
				uint32 BytesPerPixel = (MetalSrcTexture->Surface.PixelFormat != PF_DepthStencil) ? GPixelFormats[MetalSrcTexture->Surface.PixelFormat].BlockBytes : 1;
				const uint32 Stride = BytesPerPixel * SourceSize.width;
				const uint32 Alignment = PLATFORM_MAC ? 1u : 64u;
				const uint32 AlignedStride = ((Stride - 1) & ~(Alignment - 1)) + Alignment;
				const uint32 BytesPerImage = AlignedStride *  SourceSize.height;
				
				FMetalBuffer Buffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(ImmediateContext.Context->GetDevice(), BytesPerImage, mtlpp::StorageMode::Shared));
				
				check(Buffer);
				
				mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
				if (MetalSrcTexture->Surface.Texture.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && MetalSrcTexture->Surface.Texture.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
				{
					Options = mtlpp::BlitOption::RowLinearPVRTC;
				}
#endif
				ImmediateContext.GetInternalContext().CopyFromTextureToBuffer(MetalSrcTexture->Surface.Texture, 0, 0, SourceOrigin, SourceSize, Buffer, 0, AlignedStride, BytesPerImage, Options);
				ImmediateContext.GetInternalContext().CopyFromBufferToTexture(Buffer, 0, Stride, BytesPerImage, SourceSize, MetalDestTexture->Surface.Texture, 0, 0, DestinationOrigin, Options);
				
				GetMetalDeviceContext().ReleaseBuffer(Buffer);
			}
		}
		else
		{
			UE_LOG(LogMetal, Warning, TEXT("RHICopySubTextureRegion Source <-> Destination texture format mismatch"));
		}
	}
}


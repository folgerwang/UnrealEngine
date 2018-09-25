// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalStateCache.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

static mtlpp::TriangleFillMode TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch (FillMode)
	{
		case FM_Wireframe:	return mtlpp::TriangleFillMode::Lines;
		case FM_Point:		return mtlpp::TriangleFillMode::Fill;
		default:			return mtlpp::TriangleFillMode::Fill;
	};
}

static mtlpp::CullMode TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
		case CM_CCW:	return mtlpp::CullMode::Front;
		case CM_CW:		return mtlpp::CullMode::Back;
		default:		return mtlpp::CullMode::None;
	}
}

FORCEINLINE mtlpp::StoreAction GetMetalRTStoreAction(ERenderTargetStoreAction StoreAction)
{
	switch(StoreAction)
	{
		case ERenderTargetStoreAction::ENoAction: return mtlpp::StoreAction::DontCare;
		case ERenderTargetStoreAction::EStore: return mtlpp::StoreAction::Store;
		//default store action in the desktop renderers needs to be mtlpp::StoreAction::StoreAndMultisampleResolve.  Trying to express the renderer by the requested maxrhishaderplatform
        //because we may render to the same MSAA target twice in two separate passes.  BasePass, then some stuff, then translucency for example and we need to not lose the prior MSAA contents to do this properly.
		case ERenderTargetStoreAction::EMultisampleResolve:
		{
			static bool bSupportsMSAAStoreResolve = FMetalCommandQueue::SupportsFeature(EMetalFeaturesMSAAStoreAndResolve) && (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
			if (bSupportsMSAAStoreResolve)
			{
				return mtlpp::StoreAction::StoreAndMultisampleResolve;
			}
			else
			{
				return mtlpp::StoreAction::MultisampleResolve;
			}
		}
		default: return mtlpp::StoreAction::DontCare;
	}
}

FORCEINLINE mtlpp::StoreAction GetConditionalMetalRTStoreAction(bool bMSAATarget)
{
	if (bMSAATarget)
	{
		//this func should only be getting called when an encoder had to abnormally break.  In this case we 'must' do StoreAndResolve because the encoder will be restarted later
		//with the original MSAA rendertarget and the original data must still be there to continue the render properly.
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMSAAStoreAndResolve));
		return mtlpp::StoreAction::StoreAndMultisampleResolve;
	}
	else
	{
		return mtlpp::StoreAction::Store;
	}	
}

FMetalStateCache::FMetalStateCache(bool const bInImmediate)
: DepthStore(mtlpp::StoreAction::Unknown)
, StencilStore(mtlpp::StoreAction::Unknown)
, VisibilityResults(nullptr)
, VisibilityMode(mtlpp::VisibilityResultMode::Disabled)
, VisibilityOffset(0)
, VisibilityWritten(0)
, DepthStencilState(nullptr)
, RasterizerState(nullptr)
, StencilRef(0)
, BlendFactor(FLinearColor::Transparent)
, FrameBufferSize(CGSizeMake(0.0, 0.0))
, RenderTargetArraySize(1)
, RenderPassDesc(nil)
, RasterBits(0)
, PipelineBits(0)
, bIsRenderTargetActive(false)
, bHasValidRenderTarget(false)
, bHasValidColorTarget(false)
, bScissorRectEnabled(false)
, bUsingTessellation(false)
, bCanRestartRenderPass(false)
, bImmediate(bInImmediate)
, bFallbackDepthStencilBound(false)
{
	FMemory::Memzero(Viewport);
	FMemory::Memzero(Scissor);
	
	ActiveViewports = 0;
	ActiveScissors = 0;
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStore[i] = mtlpp::StoreAction::Unknown;
	}
	
	FMemory::Memzero(RenderTargetsInfo);
	FMemory::Memzero(DirtyUniformBuffers);
}

FMetalStateCache::~FMetalStateCache()
{
	RenderPassDesc = nil;
	
	for (uint32 i = 0; i < MaxVertexElementCount; i++)
	{
		VertexBuffers[i].Buffer = nil;
		VertexBuffers[i].Bytes = nil;
		VertexBuffers[i].Length = 0;
		VertexBuffers[i].Offset = 0;
	}
	for (uint32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
	{
		ShaderSamplers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxSamplers; i++)
		{
			ShaderSamplers[Frequency].Samplers[i] = nil;
		}
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			BoundUniformBuffers[Frequency][i] = nullptr;
			ShaderBuffers[Frequency].Buffers[i].Buffer = nil;
			ShaderBuffers[Frequency].Buffers[i].Bytes = nil;
			ShaderBuffers[Frequency].Buffers[i].Length = 0;
			ShaderBuffers[Frequency].Buffers[i].Offset = 0;
			ShaderBuffers[Frequency].Formats[i] = PF_Unknown;
		}
		ShaderBuffers[Frequency].Bound = 0;
		ShaderBuffers[Frequency].FormatHash = 0;
		for (uint32 i = 0; i < ML_MaxTextures; i++)
		{
			ShaderTextures[Frequency].Textures[i] = nil;
		}
		ShaderTextures[Frequency].Bound = 0;
	}
	
	VisibilityResults = nil;
}

void FMetalStateCache::Reset(void)
{
	IndexType = EMetalIndexType_None;
	SampleCount = 0;
	
	FMemory::Memzero(Viewport);
	FMemory::Memzero(Scissor);
	
	ActiveViewports = 0;
	ActiveScissors = 0;
	
	FMemory::Memzero(RenderTargetsInfo);
	bIsRenderTargetActive = false;
	bHasValidRenderTarget = false;
	bHasValidColorTarget = false;
	bScissorRectEnabled = false;
	
	FMemory::Memzero(DirtyUniformBuffers);
	FMemory::Memzero(BoundUniformBuffers);
	ActiveUniformBuffers.Empty();
	
	for (uint32 i = 0; i < MaxVertexElementCount; i++)
	{
		VertexBuffers[i].Buffer = nil;
		VertexBuffers[i].Bytes = nil;
		VertexBuffers[i].Length = 0;
		VertexBuffers[i].Offset = 0;
	}
	for (uint32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
	{
		ShaderSamplers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxSamplers; i++)
		{
			ShaderSamplers[Frequency].Samplers[i] = nil;
		}
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i].Buffer = nil;
			ShaderBuffers[Frequency].Buffers[i].Bytes = nil;
			ShaderBuffers[Frequency].Buffers[i].Length = 0;
			ShaderBuffers[Frequency].Buffers[i].Offset = 0;
			ShaderBuffers[Frequency].Formats[i] = PF_Unknown;
		}
		ShaderBuffers[Frequency].Bound = 0;
		ShaderBuffers[Frequency].FormatHash = 0;
		for (uint32 i = 0; i < ML_MaxTextures; i++)
		{
			ShaderTextures[Frequency].Textures[i] = nil;
		}
		ShaderTextures[Frequency].Bound = 0;
	}
	
	VisibilityResults = nil;
	VisibilityMode = mtlpp::VisibilityResultMode::Disabled;
	VisibilityOffset = 0;
	VisibilityWritten = 0;
	
	DepthStencilState.SafeRelease();
	RasterizerState.SafeRelease();
	GraphicsPSO.SafeRelease();
	ComputeShader.SafeRelease();
	DepthStencilSurface.SafeRelease();
	StencilRef = 0;
	
	RenderPassDesc = nil;
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStore[i] = mtlpp::StoreAction::Unknown;
	}
	DepthStore = mtlpp::StoreAction::Unknown;
	StencilStore = mtlpp::StoreAction::Unknown;
	
	BlendFactor = FLinearColor::Transparent;
	FrameBufferSize = CGSizeMake(0.0, 0.0);
	RenderTargetArraySize = 0;
    bUsingTessellation = false;
    bCanRestartRenderPass = false;
    
    RasterBits = EMetalRenderFlagMask;
    PipelineBits = EMetalPipelineFlagMask;
}

static bool MTLScissorRectEqual(mtlpp::ScissorRect const& Left, mtlpp::ScissorRect const& Right)
{
	return Left.x == Right.x && Left.y == Right.y && Left.width == Right.width && Left.height == Right.height;
}

void FMetalStateCache::SetScissorRect(bool const bEnable, mtlpp::ScissorRect const& Rect)
{
	if (bScissorRectEnabled != bEnable || !MTLScissorRectEqual(Scissor[0], Rect))
	{
		bScissorRectEnabled = bEnable;
		if (bEnable)
		{
			Scissor[0] = Rect;
		}
		else
		{
			Scissor[0].x = Viewport[0].originX;
			Scissor[0].y = Viewport[0].originY;
			Scissor[0].width = Viewport[0].width;
			Scissor[0].height = Viewport[0].height;
		}
		
		// Clamp to framebuffer size - Metal doesn't allow scissor to be larger.
		Scissor[0].x = Scissor[0].x;
		Scissor[0].y = Scissor[0].y;
		Scissor[0].width = FMath::Max((Scissor[0].x + Scissor[0].width <= FMath::RoundToInt(FrameBufferSize.width)) ? Scissor[0].width : FMath::RoundToInt(FrameBufferSize.width) - Scissor[0].x, (NSUInteger)1u);
		Scissor[0].height = FMath::Max((Scissor[0].y + Scissor[0].height <= FMath::RoundToInt(FrameBufferSize.height)) ? Scissor[0].height : FMath::RoundToInt(FrameBufferSize.height) - Scissor[0].y, (NSUInteger)1u);
		
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	ActiveScissors = 1;
}

void FMetalStateCache::SetBlendFactor(FLinearColor const& InBlendFactor)
{
	if(BlendFactor != InBlendFactor) // @todo zebra
	{
		BlendFactor = InBlendFactor;
		RasterBits |= EMetalRenderFlagBlendColor;
	}
}

void FMetalStateCache::SetStencilRef(uint32 const InStencilRef)
{
	if(StencilRef != InStencilRef) // @todo zebra
	{
		StencilRef = InStencilRef;
		RasterBits |= EMetalRenderFlagStencilReferenceValue;
	}
}

void FMetalStateCache::SetDepthStencilState(FMetalDepthStencilState* InDepthStencilState)
{
	if(DepthStencilState != InDepthStencilState) // @todo zebra
	{
		DepthStencilState = InDepthStencilState;
		RasterBits |= EMetalRenderFlagDepthStencilState;
	}
}

void FMetalStateCache::SetRasterizerState(FMetalRasterizerState* InRasterizerState)
{
	if(RasterizerState != InRasterizerState) // @todo zebra
	{
		RasterizerState = InRasterizerState;
		RasterBits |= EMetalRenderFlagFrontFacingWinding|EMetalRenderFlagCullMode|EMetalRenderFlagDepthBias|EMetalRenderFlagTriangleFillMode;
	}
}

void FMetalStateCache::SetComputeShader(FMetalComputeShader* InComputeShader)
{
	if(ComputeShader != InComputeShader) // @todo zebra
	{
		ComputeShader = InComputeShader;
		
		PipelineBits |= EMetalPipelineFlagComputeShader;
		
		bUsingTessellation = false;
		
		DirtyUniformBuffers[SF_Compute] = 0xffffffff;

		for (const auto& PackedGlobalArray : InComputeShader->Bindings.PackedGlobalArrays)
		{
			ShaderParameters[CrossCompiler::SHADER_STAGE_COMPUTE].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
		}
	}
}

bool FMetalStateCache::SetRenderTargetsInfo(FRHISetRenderTargetsInfo const& InRenderTargets, FMetalQueryBuffer* QueryBuffer, bool const bRestart)
{
	bool bNeedsSet = false;
	
	// see if our new Info matches our previous Info
	if (NeedsToSetRenderTarget(InRenderTargets))
	{
		bool bNeedsClear = false;
		
		// Deferred store actions make life a bit easier...
		static bool bSupportsDeferredStore = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
		
		//Create local store action states if we support deferred store
		mtlpp::StoreAction NewColorStore[MaxSimultaneousRenderTargets];
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			NewColorStore[i] = mtlpp::StoreAction::Unknown;
		}
		
		mtlpp::StoreAction NewDepthStore = mtlpp::StoreAction::Unknown;
		mtlpp::StoreAction NewStencilStore = mtlpp::StoreAction::Unknown;
		
		// back this up for next frame
		RenderTargetsInfo = InRenderTargets;
		
		// at this point, we need to fully set up an encoder/command buffer, so make a new one (autoreleased)
		mtlpp::RenderPassDescriptor RenderPass;
	
		// if we need to do queries, write to the supplied query buffer
		if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::ES3_1))
		{
			VisibilityResults = QueryBuffer;
			RenderPass.SetVisibilityResultBuffer(QueryBuffer ? QueryBuffer->Buffer : nil);
		}
		else
		{
			VisibilityResults = NULL;
		}
		
		if (QueryBuffer != VisibilityResults)
		{
			VisibilityOffset = 0;
			VisibilityWritten = 0;
		}
	
		// default to non-msaa
	    int32 OldCount = SampleCount;
		SampleCount = 0;
	
		bIsRenderTargetActive = false;
		bHasValidRenderTarget = false;
		bHasValidColorTarget = false;
		
		bFallbackDepthStencilBound = false;
		
		uint8 ArrayTargets = 0;
		uint8 BoundTargets = 0;
		uint32 ArrayRenderLayers = UINT_MAX;
		
		bool bFramebufferSizeSet = false;
		FrameBufferSize = CGSizeMake(0.f, 0.f);
		
		bCanRestartRenderPass = true;
		
		ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> Attachements = RenderPass.GetColorAttachments();
		
		for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxSimultaneousRenderTargets; RenderTargetIndex++)
		{
			// default to invalid
			uint8 FormatKey = 0;
			// only try to set it if it was one that was set (ie less than RenderTargetsInfo.NumColorRenderTargets)
			if (RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets && RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex].Texture != nullptr)
			{
				const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
				ColorTargets[RenderTargetIndex] = RenderTargetView.Texture;
				
				FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);
				FormatKey = Surface.FormatKey;
				
				uint32 Width = FMath::Max((uint32)(Surface.SizeX >> RenderTargetView.MipIndex), (uint32)1);
				uint32 Height = FMath::Max((uint32)(Surface.SizeY >> RenderTargetView.MipIndex), (uint32)1);
				if(!bFramebufferSizeSet)
				{
					bFramebufferSizeSet = true;
					FrameBufferSize.width = Width;
					FrameBufferSize.height = Height;
				}
				else
				{
					FrameBufferSize.width = FMath::Min(FrameBufferSize.width, (CGFloat)Width);
					FrameBufferSize.height = FMath::Min(FrameBufferSize.height, (CGFloat)Height);
				}
	
				// if this is the back buffer, make sure we have a usable drawable
				ConditionalUpdateBackBuffer(Surface);
	
				BoundTargets |= 1 << RenderTargetIndex;
            
#if !PLATFORM_MAC
                if (Surface.Texture.GetPtr() == nil)
                {
                    SampleCount = OldCount;
                    bCanRestartRenderPass &= (OldCount <= 1);
                    return true;
                }
#endif
				
				// The surface cannot be nil - we have to have a valid render-target array after this call.
				check (Surface.Texture);
	
				// user code generally passes -1 as a default, but we need 0
				uint32 ArraySliceIndex = RenderTargetView.ArraySliceIndex == 0xFFFFFFFF ? 0 : RenderTargetView.ArraySliceIndex;
				if (Surface.bIsCubemap)
				{
					ArraySliceIndex = GetMetalCubeFace((ECubeFace)ArraySliceIndex);
				}
				
				switch(Surface.Type)
				{
					case RRT_Texture2DArray:
					case RRT_Texture3D:
					case RRT_TextureCube:
						if(RenderTargetView.ArraySliceIndex == 0xFFFFFFFF)
						{
							ArrayTargets |= (1 << RenderTargetIndex);
							ArrayRenderLayers = FMath::Min(ArrayRenderLayers, Surface.GetNumFaces());
						}
						else
						{
							ArrayRenderLayers = 1;
						}
						break;
					default:
						ArrayRenderLayers = 1;
						break;
				}
	
				mtlpp::RenderPassColorAttachmentDescriptor ColorAttachment = Attachements[RenderTargetIndex];
	
				if (Surface.MSAATexture)
				{
					// set up an MSAA attachment
					ColorAttachment.SetTexture(Surface.MSAATexture);
					NewColorStore[RenderTargetIndex] = GetMetalRTStoreAction(ERenderTargetStoreAction::EMultisampleResolve);
					ColorAttachment.SetStoreAction(bSupportsDeferredStore && GRHIDeviceId > 2 ? mtlpp::StoreAction::Unknown : NewColorStore[RenderTargetIndex]);
					ColorAttachment.SetResolveTexture(Surface.MSAAResolveTexture ? Surface.MSAAResolveTexture : Surface.Texture);
					SampleCount = Surface.MSAATexture.GetSampleCount();
                    
					// only allow one MRT with msaa
					checkf(RenderTargetsInfo.NumColorRenderTargets == 1, TEXT("Only expected one MRT when using MSAA"));
				}
				else
				{
					// set up non-MSAA attachment
					ColorAttachment.SetTexture(Surface.Texture);
					NewColorStore[RenderTargetIndex] = GetMetalRTStoreAction(RenderTargetView.StoreAction);
					ColorAttachment.SetStoreAction(bSupportsDeferredStore ? mtlpp::StoreAction::Unknown : NewColorStore[RenderTargetIndex]);
                    SampleCount = 1;
				}
				
				ColorAttachment.SetLevel(RenderTargetView.MipIndex);
				if(Surface.Type == RRT_Texture3D)
				{
					ColorAttachment.SetDepthPlane(ArraySliceIndex);
				}
				else
				{
					ColorAttachment.SetSlice(ArraySliceIndex);
				}
				
				ColorAttachment.SetLoadAction((Surface.Written || !bImmediate || bRestart) ? GetMetalRTLoadAction(RenderTargetView.LoadAction) : mtlpp::LoadAction::Clear);
				FPlatformAtomics::InterlockedExchange(&Surface.Written, 1);
				
				bNeedsClear |= (ColorAttachment.GetLoadAction() == mtlpp::LoadAction::Clear);
				
				const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex].Texture->GetClearBinding();
				if (ClearValue.ColorBinding == EClearBinding::EColorBound)
				{
					const FLinearColor& ClearColor = ClearValue.GetClearColor();
					ColorAttachment.SetClearColor(mtlpp::ClearColor(ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A));
				}

				bCanRestartRenderPass &= (SampleCount <= 1) && (ColorAttachment.GetLoadAction() == mtlpp::LoadAction::Load) && (RenderTargetView.StoreAction == ERenderTargetStoreAction::EStore);
	
				bHasValidRenderTarget = true;
				bHasValidColorTarget = true;
			}
			else
			{
				ColorTargets[RenderTargetIndex].SafeRelease();
			}
		}
		
		RenderTargetArraySize = 1;
		
		if(ArrayTargets)
		{
			if (!GetMetalDeviceContext().SupportsFeature(EMetalFeaturesLayeredRendering))
			{
				if (ArrayRenderLayers != 1)
				{
					UE_LOG(LogMetal, Fatal, TEXT("Layered rendering is unsupported on this device."));
				}
			}
#if PLATFORM_MAC
			else
			{
				if (ArrayTargets == BoundTargets)
				{
					RenderTargetArraySize = ArrayRenderLayers;
					RenderPass.SetRenderTargetArrayLength(ArrayRenderLayers);
				}
				else
				{
					UE_LOG(LogMetal, Fatal, TEXT("All color render targets must be layered when performing multi-layered rendering under Metal."));
				}
			}
#endif
		}
	
		// default to invalid
		uint8 DepthFormatKey = 0;
		uint8 StencilFormatKey = 0;
		
		// setup depth and/or stencil
		if (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr)
		{
			FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
			
			switch(Surface.Type)
			{
				case RRT_Texture2DArray:
				case RRT_Texture3D:
				case RRT_TextureCube:
					ArrayRenderLayers = Surface.GetNumFaces();
					break;
				default:
					ArrayRenderLayers = 1;
					break;
			}
			if(!ArrayTargets && ArrayRenderLayers > 1)
			{
				if (!GetMetalDeviceContext().SupportsFeature(EMetalFeaturesLayeredRendering))
				{
					UE_LOG(LogMetal, Fatal, TEXT("Layered rendering is unsupported on this device."));
				}
#if PLATFORM_MAC
				else
				{
					RenderTargetArraySize = ArrayRenderLayers;
					RenderPass.SetRenderTargetArrayLength(ArrayRenderLayers);
				}
#endif
			}
			
			if(!bFramebufferSizeSet)
			{
				bFramebufferSizeSet = true;
				FrameBufferSize.width = Surface.SizeX;
				FrameBufferSize.height = Surface.SizeY;
			}
			else
			{
				FrameBufferSize.width = FMath::Min(FrameBufferSize.width, (CGFloat)Surface.SizeX);
				FrameBufferSize.height = FMath::Min(FrameBufferSize.height, (CGFloat)Surface.SizeY);
			}
			
			EPixelFormat DepthStencilPixelFormat = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetFormat();
			
			FMetalTexture DepthTexture = nil;
			FMetalTexture StencilTexture = nil;
			
            const bool bSupportSeparateMSAAResolve = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
			uint32 DepthSampleCount = (Surface.MSAATexture ? Surface.MSAATexture.GetSampleCount() : Surface.Texture.GetSampleCount());
            bool bDepthStencilSampleCountMismatchFixup = false;
            DepthTexture = Surface.MSAATexture ? Surface.MSAATexture : Surface.Texture;
			if (SampleCount == 0)
			{
				SampleCount = DepthSampleCount;
			}
			else if (SampleCount != DepthSampleCount)
            {
				static bool bLogged = false;
				if (!bSupportSeparateMSAAResolve)
				{
					//in the case of NOT support separate MSAA resolve the high level may legitimately cause a mismatch which we need to handle by binding the resolved target which we normally wouldn't do.
					DepthTexture = Surface.Texture;
					bDepthStencilSampleCountMismatchFixup = true;
					DepthSampleCount = 1;
				}
				else if (!bLogged)
				{
					UE_LOG(LogMetal, Error, TEXT("If we support separate targets the high level should always give us matching counts"));
					bLogged = true;
				}
            }

			switch (DepthStencilPixelFormat)
			{
				case PF_X24_G8:
				case PF_DepthStencil:
				case PF_D24:
				{
					mtlpp::PixelFormat DepthStencilFormat = Surface.Texture ? (mtlpp::PixelFormat)Surface.Texture.GetPixelFormat() : mtlpp::PixelFormat::Invalid;
					
					switch(DepthStencilFormat)
					{
						case mtlpp::PixelFormat::Depth32Float:
#if !PLATFORM_MAC
							StencilTexture = (DepthStencilPixelFormat == PF_DepthStencil) ? Surface.StencilTexture : nil;
#endif
							break;
						case mtlpp::PixelFormat::Stencil8:
							StencilTexture = DepthTexture;
							break;
						case mtlpp::PixelFormat::Depth32Float_Stencil8:
							StencilTexture = DepthTexture;
							break;
#if PLATFORM_MAC
						case mtlpp::PixelFormat::Depth24Unorm_Stencil8:
							StencilTexture = DepthTexture;
							break;
#endif
						default:
							break;
					}
					
					break;
				}
				case PF_ShadowDepth:
				{
					break;
				}
				default:
					break;
			}
			
			float DepthClearValue = 0.0f;
			uint32 StencilClearValue = 0;
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			if (ClearValue.ColorBinding == EClearBinding::EDepthStencilBound)
			{
				ClearValue.GetDepthStencil(DepthClearValue, StencilClearValue);
			}
			else if(!ArrayTargets && ArrayRenderLayers > 1)
			{
				DepthClearValue = 1.0f;
			}

            static bool const bUsingValidation = FMetalCommandQueue::SupportsFeature(EMetalFeaturesValidation) && !FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 14, 0}, (uint32[]){12, 0, 0}, (uint32[]){12, 0, 0});
            
            bool const bCombinedDepthStencilUsingStencil = (DepthTexture && (mtlpp::PixelFormat)DepthTexture.GetPixelFormat() != mtlpp::PixelFormat::Depth32Float && RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingStencil());			
			bool const bUsingDepth = (RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingDepth() || (bUsingValidation && bCombinedDepthStencilUsingStencil));
			if (DepthTexture && bUsingDepth)
			{
				mtlpp::RenderPassDepthAttachmentDescriptor DepthAttachment;
				
				DepthFormatKey = Surface.FormatKey;
	
				// set up the depth attachment
				DepthAttachment.SetTexture(DepthTexture);
				DepthAttachment.SetLoadAction(GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction));
				
				bNeedsClear |= (DepthAttachment.GetLoadAction() == mtlpp::LoadAction::Clear);
				
				ERenderTargetStoreAction HighLevelStoreAction = (Surface.MSAATexture && !bDepthStencilSampleCountMismatchFixup) ? ERenderTargetStoreAction::EMultisampleResolve : RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction;
				if (bUsingDepth && (HighLevelStoreAction == ERenderTargetStoreAction::ENoAction || bDepthStencilSampleCountMismatchFixup))
				{
					if (DepthSampleCount > 1)
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EMultisampleResolve;
					}
					else
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EStore;
					}
				}
				
                //needed to quiet the metal validation that runs when you end renderpass. (it requires some kind of 'resolve' for an msaa target)
				//But with deferredstore we don't set the real one until submit time.
				const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
				NewDepthStore = !Surface.MSAATexture || bSupportsMSAADepthResolve ? GetMetalRTStoreAction(HighLevelStoreAction) : mtlpp::StoreAction::DontCare;
				DepthAttachment.SetStoreAction(bSupportsDeferredStore && Surface.MSAATexture && GRHIDeviceId > 2 ? mtlpp::StoreAction::Unknown : NewDepthStore);
				DepthAttachment.SetClearDepth(DepthClearValue);
				check(SampleCount > 0);

				if (Surface.MSAATexture && bSupportsMSAADepthResolve)
				{
                    if (!bDepthStencilSampleCountMismatchFixup)
                    {
                        DepthAttachment.SetResolveTexture(Surface.MSAAResolveTexture ? Surface.MSAAResolveTexture : Surface.Texture);
                    }
#if PLATFORM_MAC
					//would like to assert and do manual custom resolve, but that is causing some kind of weird corruption.
					//checkf(false, TEXT("Depth resolves need to do 'max' for correctness.  MacOS does not expose this yet unless the spec changed."));
#else
					DepthAttachment.SetDepthResolveFilter(mtlpp::MultisampleDepthResolveFilter::Max);
#endif
				}
				
				bHasValidRenderTarget = true;
				bFallbackDepthStencilBound = (RenderTargetsInfo.DepthStencilRenderTarget.Texture == FallbackDepthStencilSurface);

				bCanRestartRenderPass &= (SampleCount <= 1) && ((RenderTargetsInfo.DepthStencilRenderTarget.Texture == FallbackDepthStencilSurface) || ((DepthAttachment.GetLoadAction() == mtlpp::LoadAction::Load) && (!RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsDepthWrite() || (RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction == ERenderTargetStoreAction::EStore))));
				
				// and assign it
				RenderPass.SetDepthAttachment(DepthAttachment);
			}
	
            //if we're dealing with a samplecount mismatch we just bail on stencil entirely as stencil
            //doesn't have an autoresolve target to use.
			
			bool const bCombinedDepthStencilUsingDepth = (StencilTexture && StencilTexture.GetPixelFormat() != mtlpp::PixelFormat::Stencil8 && RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingDepth());
			bool const bUsingStencil = RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingStencil() || (bUsingValidation && bCombinedDepthStencilUsingDepth);
			if (StencilTexture && bUsingStencil && (FMetalCommandQueue::SupportsFeature(EMetalFeaturesCombinedDepthStencil) || !bDepthStencilSampleCountMismatchFixup))
			{
                if (!FMetalCommandQueue::SupportsFeature(EMetalFeaturesCombinedDepthStencil) && bDepthStencilSampleCountMismatchFixup)
                {
                    checkf(!RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite(), TEXT("Stencil write not allowed as we don't have a proper stencil to use."));
                }
                else
                {
					mtlpp::RenderPassStencilAttachmentDescriptor StencilAttachment;
                    
                    StencilFormatKey = Surface.FormatKey;
        
                    // set up the stencil attachment
                    StencilAttachment.SetTexture(StencilTexture);
                    StencilAttachment.SetLoadAction(GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction));
                    
                    bNeedsClear |= (StencilAttachment.GetLoadAction() == mtlpp::LoadAction::Clear);
					
					ERenderTargetStoreAction HighLevelStoreAction = RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction();
					if (bUsingStencil && (HighLevelStoreAction == ERenderTargetStoreAction::ENoAction || bDepthStencilSampleCountMismatchFixup))
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EStore;
					}
					
					// For the case where Depth+Stencil is MSAA we can't Resolve depth and Store stencil - we can only Resolve + DontCare or StoreResolve + Store (on newer H/W and iOS).
					// We only allow use of StoreResolve in the Desktop renderers as the mobile renderer does not and should not assume hardware support for it.
					NewStencilStore = (StencilTexture.GetSampleCount() == 1  || GetMetalRTStoreAction(ERenderTargetStoreAction::EMultisampleResolve) == mtlpp::StoreAction::StoreAndMultisampleResolve) ? GetMetalRTStoreAction(HighLevelStoreAction) : mtlpp::StoreAction::DontCare;
					StencilAttachment.SetStoreAction(bSupportsDeferredStore && StencilTexture.GetSampleCount() > 1 && GRHIDeviceId > 2 ? mtlpp::StoreAction::Unknown : NewStencilStore);
                    StencilAttachment.SetClearStencil(StencilClearValue);

                    if (SampleCount == 0)
                    {
                        SampleCount = StencilAttachment.GetTexture().GetSampleCount();
                    }
                    
                    bHasValidRenderTarget = true;
                    
                    // @todo Stencil writes that need to persist must use ERenderTargetStoreAction::EStore on iOS.
                    // We should probably be using deferred store actions so that we can safely lazily instantiate encoders.
                    bCanRestartRenderPass &= (SampleCount <= 1) && ((RenderTargetsInfo.DepthStencilRenderTarget.Texture == FallbackDepthStencilSurface) || ((StencilAttachment.GetLoadAction() == mtlpp::LoadAction::Load) && (1 || !RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite() || (RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction() == ERenderTargetStoreAction::EStore))));
                    
                    // and assign it
                    RenderPass.SetStencilAttachment(StencilAttachment);
                }
			}
		}
		
		//Update deferred store states if required otherwise they're already set directly on the Metal Attachement Descriptors
		if (bSupportsDeferredStore)
		{
			for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
			{
				ColorStore[i] = NewColorStore[i];
			}
			DepthStore = NewDepthStore;
			StencilStore = NewStencilStore;
		}
		
		bHasValidRenderTarget |= (InRenderTargets.NumUAVs > 0);
		if (SampleCount == 0)
		{
			SampleCount = 1;
		}
		
		bIsRenderTargetActive = bHasValidRenderTarget;
		
		// Only start encoding if the render target state is valid
		if (bHasValidRenderTarget)
		{
			// Retain and/or release the depth-stencil surface in case it is a temporary surface for a draw call that writes to depth without a depth/stencil buffer bound.
			DepthStencilSurface = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		}
		else
		{
			DepthStencilSurface.SafeRelease();
		}
		
		RenderPassDesc = RenderPass;
		
		bNeedsSet = true;
	}

	return bNeedsSet;
}

void FMetalStateCache::InvalidateRenderTargets(void)
{
	bHasValidRenderTarget = false;
	bHasValidColorTarget = false;
	bIsRenderTargetActive = false;
}

void FMetalStateCache::SetRenderTargetsActive(bool const bActive)
{
	bIsRenderTargetActive = bActive;
}

static bool MTLViewportEqual(mtlpp::Viewport const& Left, mtlpp::Viewport const& Right)
{
	return FMath::IsNearlyEqual(Left.originX, Right.originX) &&
			FMath::IsNearlyEqual(Left.originY, Right.originY) &&
			FMath::IsNearlyEqual(Left.width, Right.width) &&
			FMath::IsNearlyEqual(Left.height, Right.height) &&
			FMath::IsNearlyEqual(Left.znear, Right.znear) &&
			FMath::IsNearlyEqual(Left.zfar, Right.zfar);
}

void FMetalStateCache::SetViewport(const mtlpp::Viewport& InViewport)
{
	if (!MTLViewportEqual(Viewport[0], InViewport))
	{
		Viewport[0] = InViewport;
	
		RasterBits |= EMetalRenderFlagViewport;
	}
	
	ActiveViewports = 1;
	
	if (!bScissorRectEnabled)
	{
		mtlpp::ScissorRect Rect;
		Rect.x = InViewport.originX;
		Rect.y = InViewport.originY;
		Rect.width = InViewport.width;
		Rect.height = InViewport.height;
		SetScissorRect(false, Rect);
	}
}

void FMetalStateCache::SetViewport(uint32 Index, const mtlpp::Viewport& InViewport)
{
	check(Index < ML_MaxViewports);
	
	if (!MTLViewportEqual(Viewport[Index], InViewport))
	{
		Viewport[Index] = InViewport;
		
		RasterBits |= EMetalRenderFlagViewport;
	}
	
	// There may not be gaps in the viewport array.
	ActiveViewports = Index + 1;
	
	// This always sets the scissor rect because the RHI doesn't bother to expose proper scissor states for multiple viewports.
	// This will have to change if we want to guarantee correctness in the mid to long term.
	{
		mtlpp::ScissorRect Rect;
		Rect.x = InViewport.originX;
		Rect.y = InViewport.originY;
		Rect.width = InViewport.width;
		Rect.height = InViewport.height;
		SetScissorRect(Index, false, Rect);
	}
}

void FMetalStateCache::SetScissorRect(uint32 Index, bool const bEnable, mtlpp::ScissorRect const& Rect)
{
	check(Index < ML_MaxViewports);
	if (!MTLScissorRectEqual(Scissor[Index], Rect))
	{
		// There's no way we can setup the bounds correctly - that must be done by the caller or incorrect rendering & crashes will ensue.
		Scissor[Index] = Rect;
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	ActiveScissors = Index + 1;
}

void FMetalStateCache::SetViewports(const mtlpp::Viewport InViewport[], uint32 Count)
{
	check(Count >= 1 && Count < ML_MaxViewports);
	
	// Check if the count has changed first & if so mark for a rebind
	if (ActiveViewports != Count)
	{
		RasterBits |= EMetalRenderFlagViewport;
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	for (uint32 i = 0; i < Count; i++)
	{
		SetViewport(i, InViewport[i]);
	}
	
	ActiveViewports = Count;
}

void FMetalStateCache::SetVertexStream(uint32 const Index, FMetalBuffer* Buffer, FMetalBufferData* Bytes, uint32 const Offset, uint32 const Length)
{
	check(Index < MaxVertexElementCount);
	check(UNREAL_TO_METAL_BUFFER_INDEX(Index) < MaxMetalStreams);

	if (Buffer)
	{
		VertexBuffers[Index].Buffer = *Buffer;
	}
	else
	{
		VertexBuffers[Index].Buffer = nil;
	}
	VertexBuffers[Index].Offset = 0;
	VertexBuffers[Index].Bytes = Bytes;
	VertexBuffers[Index].Length = Length;
	
	SetShaderBuffer(SF_Vertex, VertexBuffers[Index].Buffer, Bytes, Offset, Length, UNREAL_TO_METAL_BUFFER_INDEX(Index));
}

uint32 FMetalStateCache::GetVertexBufferSize(uint32 const Index)
{
	check(Index < MaxVertexElementCount);
	check(UNREAL_TO_METAL_BUFFER_INDEX(Index) < MaxMetalStreams);
	return VertexBuffers[Index].Length;
}

void FMetalStateCache::SetGraphicsPipelineState(FMetalGraphicsPipelineState* State)
{
	if (GraphicsPSO != State)
	{
		GraphicsPSO = State;
		
		bool bNewUsingTessellation = (State && State->GetPipeline(IndexType, EMetalBufferType_Dynamic, EMetalBufferType_Dynamic, EMetalBufferType_Dynamic)->TessellationPipelineDesc.DomainVertexDescriptor);
		if (bNewUsingTessellation != bUsingTessellation)
		{
			for (uint32 i = 0; i < SF_NumFrequencies; i++)
			{
				ShaderBuffers[i].Bound = UINT32_MAX;
#if PLATFORM_MAC
#ifndef UINT128_MAX
#define UINT128_MAX (((__uint128_t)1 << 127) - (__uint128_t)1 + ((__uint128_t)1 << 127))
#endif
				ShaderTextures[i].Bound = UINT128_MAX;
#else
				ShaderTextures[i].Bound = UINT32_MAX;
#endif
				ShaderSamplers[i].Bound = UINT16_MAX;
			}
		}
		// Whenever the pipeline changes & a Hull shader is bound clear the Hull shader bindings, otherwise the Hull resources from a
		// previous pipeline with different binding table will overwrite the vertex shader bindings for the current pipeline.
		if (bNewUsingTessellation)
		{
			ShaderBuffers[SF_Hull].Bound = UINT32_MAX;
#if PLATFORM_MAC
			ShaderTextures[SF_Hull].Bound = UINT128_MAX;
#else
			ShaderTextures[SF_Hull].Bound = UINT32_MAX;
#endif
			ShaderSamplers[SF_Hull].Bound = UINT16_MAX;
			ShaderBuffers[SF_Hull].FormatHash = 0;
			
			for (uint32 i = 0; i < ML_MaxBuffers; i++)
			{
				BoundUniformBuffers[SF_Hull][i] = nullptr;
				ShaderBuffers[SF_Hull].Buffers[i].Buffer = nil;
				ShaderBuffers[SF_Hull].Buffers[i].Bytes = nil;
				ShaderBuffers[SF_Hull].Buffers[i].Length = 0;
				ShaderBuffers[SF_Hull].Buffers[i].Offset = 0;
				ShaderBuffers[SF_Hull].Formats[i] = PF_Unknown;
			}
			for (uint32 i = 0; i < ML_MaxTextures; i++)
			{
				ShaderTextures[SF_Hull].Textures[i] = nil;
			}
			
			for (uint32 i = 0; i < ML_MaxSamplers; i++)
			{
				ShaderSamplers[SF_Hull].Samplers[i] = nil;
			}

			for (const auto& PackedGlobalArray : State->HullShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[CrossCompiler::SHADER_STAGE_HULL].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
			}

			for (const auto& PackedGlobalArray : State->DomainShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[CrossCompiler::SHADER_STAGE_DOMAIN].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
			}
		}
		bUsingTessellation = bNewUsingTessellation;
		
		DirtyUniformBuffers[SF_Vertex] = 0xffffffff;
		DirtyUniformBuffers[SF_Pixel] = 0xffffffff;
		DirtyUniformBuffers[SF_Hull] = 0xffffffff;
		DirtyUniformBuffers[SF_Domain] = 0xffffffff;
		DirtyUniformBuffers[SF_Geometry] = 0xffffffff;
		
		PipelineBits |= EMetalPipelineFlagPipelineState;
		
		SetDepthStencilState(State->DepthStencilState);
		SetRasterizerState(State->RasterizerState);

		for (const auto& PackedGlobalArray : State->VertexShader->Bindings.PackedGlobalArrays)
		{
			ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
		}

		if (State->PixelShader)
		{
			for (const auto& PackedGlobalArray : State->PixelShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
			}
		}
		
		static bool bSupportsUAVs = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesGraphicsUAVs);
		if (bSupportsUAVs)
		{
			for (uint32 i = 0; i < RenderTargetsInfo.NumUAVs; i++)
			{
				if (IsValidRef(RenderTargetsInfo.UnorderedAccessView[i]))
				{
					FMetalUnorderedAccessView* UAV = ResourceCast(RenderTargetsInfo.UnorderedAccessView[i].GetReference());
					SetShaderUnorderedAccessView(SF_Pixel, i, UAV);
				}
			}
		}
	}
}

void FMetalStateCache::SetIndexType(EMetalIndexType InIndexType)
{
	if (IndexType != InIndexType)
	{
		IndexType = InIndexType;
		
		PipelineBits |= EMetalPipelineFlagPipelineState;
	}
}

void FMetalStateCache::BindUniformBuffer(EShaderFrequency const Freq, uint32 const BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	check(BufferIndex < ML_MaxBuffers);
	if (BoundUniformBuffers[Freq][BufferIndex] != BufferRHI)
	{
		ActiveUniformBuffers.Add(BufferRHI);
		BoundUniformBuffers[Freq][BufferIndex] = BufferRHI;
		DirtyUniformBuffers[Freq] |= 1 << BufferIndex;
	}
}

void FMetalStateCache::SetDirtyUniformBuffers(EShaderFrequency const Freq, uint32 const Dirty)
{
	DirtyUniformBuffers[Freq] = Dirty;
}

void FMetalStateCache::SetVisibilityResultMode(mtlpp::VisibilityResultMode const Mode, NSUInteger const Offset)
{
	if (VisibilityMode != Mode || VisibilityOffset != Offset)
	{
		VisibilityMode = Mode;
		VisibilityOffset = Offset;
		
		RasterBits |= EMetalRenderFlagVisibilityResultMode;
	}
}

void FMetalStateCache::ConditionalUpdateBackBuffer(FMetalSurface& Surface)
{
	// are we setting the back buffer? if so, make sure we have the drawable
	if ((Surface.Flags & TexCreate_Presentable))
	{
		// update the back buffer texture the first time used this frame
		if (Surface.Texture.GetPtr() == nil)
		{
			// set the texture into the backbuffer
			Surface.GetDrawableTexture();
		}
#if PLATFORM_MAC
		check (Surface.Texture);
#endif
	}
}

bool FMetalStateCache::NeedsToSetRenderTarget(const FRHISetRenderTargetsInfo& InRenderTargetsInfo)
{
	// see if our new Info matches our previous Info
	
	// basic checks
	bool bAllChecksPassed = GetHasValidRenderTarget() && bIsRenderTargetActive && InRenderTargetsInfo.NumColorRenderTargets == RenderTargetsInfo.NumColorRenderTargets && InRenderTargetsInfo.NumUAVs == RenderTargetsInfo.NumUAVs &&
		(InRenderTargetsInfo.DepthStencilRenderTarget.Texture == RenderTargetsInfo.DepthStencilRenderTarget.Texture);

	// now check each color target if the basic tests passe
	if (bAllChecksPassed)
	{
		for (int32 RenderTargetIndex = 0; RenderTargetIndex < InRenderTargetsInfo.NumColorRenderTargets; RenderTargetIndex++)
		{
			const FRHIRenderTargetView& RenderTargetView = InRenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
			const FRHIRenderTargetView& PreviousRenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];

			// handle simple case of switching textures or mip/slice
			if (RenderTargetView.Texture != PreviousRenderTargetView.Texture ||
				RenderTargetView.MipIndex != PreviousRenderTargetView.MipIndex ||
				RenderTargetView.ArraySliceIndex != PreviousRenderTargetView.ArraySliceIndex)
			{
				bAllChecksPassed = false;
				break;
			}
			
			// it's non-trivial when we need to switch based on load/store action:
			// LoadAction - it only matters what we are switching to in the new one
			//    If we switch to Load, no need to switch as we can re-use what we already have
			//    If we switch to Clear, we have to always switch to a new RT to force the clear
			//    If we switch to DontCare, there's definitely no need to switch
			//    If we switch *from* Clear then we must change target as we *don't* want to clear again.
            if (RenderTargetView.LoadAction == ERenderTargetLoadAction::EClear)
            {
                bAllChecksPassed = false;
                break;
            }
            // StoreAction - this matters what the previous one was **In Spirit**
            //    If we come from Store, we need to switch to a new RT to force the store
            //    If we come from DontCare, then there's no need to switch
            //    @todo metal: However, we basically only use Store now, and don't
            //        care about intermediate results, only final, so we don't currently check the value
            //			if (PreviousRenderTargetView.StoreAction == ERenderTTargetStoreAction::EStore)
            //			{
            //				bAllChecksPassed = false;
            //				break;
            //			}
        }
        
        if (InRenderTargetsInfo.DepthStencilRenderTarget.Texture && (InRenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::EClear || InRenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction == ERenderTargetLoadAction::EClear))
        {
            bAllChecksPassed = false;
		}
		
		if (InRenderTargetsInfo.DepthStencilRenderTarget.Texture && (InRenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction > RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction || InRenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction() > RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction()))
		{
			// Don't break the encoder if we can just change the store actions.
			if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesDeferredStoreActions))
			{
				mtlpp::StoreAction NewDepthStore = DepthStore;
				mtlpp::StoreAction NewStencilStore = StencilStore;
				if (InRenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction > RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction)
				{
					if (RenderPassDesc.GetDepthAttachment().GetTexture())
					{
						FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
						
						const uint32 DepthSampleCount = (Surface.MSAATexture ? Surface.MSAATexture.GetSampleCount() : Surface.Texture.GetSampleCount());
						bool const bDepthStencilSampleCountMismatchFixup = (SampleCount != DepthSampleCount);

						ERenderTargetStoreAction HighLevelStoreAction = (Surface.MSAATexture && !bDepthStencilSampleCountMismatchFixup) ? ERenderTargetStoreAction::EMultisampleResolve : RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction;
						
						NewDepthStore = GetMetalRTStoreAction(HighLevelStoreAction);
					}
					else
					{
						bAllChecksPassed = false;
					}
				}
				
				if (InRenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction() > RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction())
				{
					if (RenderPassDesc.GetStencilAttachment().GetTexture())
					{
						NewStencilStore = GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction());
					}
					else
					{
						bAllChecksPassed = false;
					}
				}
				
				if (bAllChecksPassed)
				{
					DepthStore = NewDepthStore;
					StencilStore = NewStencilStore;
				}
			}
			else
			{
				bAllChecksPassed = false;
			}
		}
	}

	// if we are setting them to nothing, then this is probably end of frame, and we can't make a framebuffer
	// with nothng, so just abort this (only need to check on single MRT case)
	if (InRenderTargetsInfo.NumColorRenderTargets == 1 && InRenderTargetsInfo.ColorRenderTarget[0].Texture == nullptr &&
		InRenderTargetsInfo.DepthStencilRenderTarget.Texture == nullptr)
	{
		bAllChecksPassed = true;
	}

	return bAllChecksPassed == false;
}

static uint8 GMetalShaderFreqFormat[SF_NumFrequencies] =
{
	EMetalPipelineFlagVertexBuffers,
	0,
	EMetalPipelineFlagDomainBuffers,
	EMetalPipelineFlagPixelBuffers,
	0,
	EMetalPipelineFlagComputeBuffers
};

void FMetalStateCache::SetShaderBuffer(EShaderFrequency const Frequency, FMetalBuffer const& Buffer, FMetalBufferData* const Bytes, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index, EPixelFormat const Format)
{
	check(Frequency < SF_NumFrequencies);
	check(Index < ML_MaxBuffers);
	
	if (ShaderBuffers[Frequency].Buffers[Index].Buffer != Buffer ||
		ShaderBuffers[Frequency].Buffers[Index].Bytes != Bytes ||
		ShaderBuffers[Frequency].Buffers[Index].Offset != Offset ||
		ShaderBuffers[Frequency].Buffers[Index].Length != Length ||
		ShaderBuffers[Frequency].Formats[Index] != Format)
	{
		ShaderBuffers[Frequency].Buffers[Index].Buffer = Buffer;
		ShaderBuffers[Frequency].Buffers[Index].Bytes = Bytes;
		ShaderBuffers[Frequency].Buffers[Index].Offset = Offset;
		ShaderBuffers[Frequency].Buffers[Index].Length = Length;
		
		PipelineBits |= (ShaderBuffers[Frequency].Formats[Index] != Format) ? GMetalShaderFreqFormat[Frequency] : 0;
		ShaderBuffers[Frequency].Formats[Index] = Format;
		
		if (Buffer || Bytes)
		{
			ShaderBuffers[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderBuffers[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetShaderTexture(EShaderFrequency const Frequency, FMetalTexture const& Texture, NSUInteger const Index)
{
	check(Frequency < SF_NumFrequencies);
	check(Index < ML_MaxTextures);
	
	if (ShaderTextures[Frequency].Textures[Index] != Texture)
	{
		ShaderTextures[Frequency].Textures[Index] = Texture;
		
		if (Texture)
		{
			ShaderTextures[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderTextures[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetShaderSamplerState(EShaderFrequency const Frequency, FMetalSamplerState* const Sampler, NSUInteger const Index)
{
	check(Frequency < SF_NumFrequencies);
	check(Index < ML_MaxSamplers);
	
	if (ShaderSamplers[Frequency].Samplers[Index].GetPtr() != (Sampler ? Sampler->State.GetPtr() : nil))
	{
		if (Sampler)
		{
#if !PLATFORM_MAC
			ShaderSamplers[Frequency].Samplers[Index] = ((Frequency == SF_Vertex || Frequency == SF_Compute) && Sampler->NoAnisoState) ? Sampler->NoAnisoState : Sampler->State;
#else
			ShaderSamplers[Frequency].Samplers[Index] = Sampler->State;
#endif
			ShaderSamplers[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderSamplers[Frequency].Samplers[Index] = nil;
			ShaderSamplers[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI, float CurrentTime)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	ns::AutoReleased<FMetalTexture> Texture;
	if (Surface != nullptr)
	{
		TextureRHI->SetLastRenderTime(CurrentTime);
		Texture = Surface->Texture;
	}
	
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderTexture(SF_Pixel, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderTexture(SF_Vertex, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderTexture(SF_Compute, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderTexture(SF_Hull, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderTexture(SF_Domain, Texture, BindIndex);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetShaderResourceView(FMetalContext* Context, EShaderFrequency ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV)
{
	if (SRV)
	{
		FRHITexture* Texture = SRV->SourceTexture.GetReference();
		FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
		FMetalIndexBuffer* IB = SRV->SourceIndexBuffer.GetReference();
		FMetalStructuredBuffer* SB = SRV->SourceStructuredBuffer.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = SRV->TextureView;
			if (Surface != nullptr)
			{
				SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
		else if (IsLinearBuffer(ShaderStage, BindIndex) && SRV->GetLinearTexture(false))
		{
			ns::AutoReleased<FMetalTexture> Tex;
			Tex = SRV->GetLinearTexture(false);
			
			uint32 PackedLen = (Tex.GetWidth() | (Tex.GetHeight() << 16));
			SetShaderTexture(ShaderStage, Tex, BindIndex);
			if (VB)
            {
                SetShaderBuffer(ShaderStage, VB->Buffer, VB->Data, 0, PackedLen, BindIndex, (EPixelFormat)SRV->Format);
            }
            else if (IB)
            {
                SetShaderBuffer(ShaderStage, IB->Buffer, nil, 0, PackedLen, BindIndex, (EPixelFormat)SRV->Format);
            }
		}
		else if (VB)
		{
			checkf(ValidateBufferFormat(ShaderStage, BindIndex, (EPixelFormat)SRV->Format), TEXT("Invalid buffer format %d for index %d, shader %d"), SRV->Format, BindIndex, ShaderStage);
			SetShaderBuffer(ShaderStage, VB->Buffer, VB->Data, 0, VB->GetSize(), BindIndex, (EPixelFormat)SRV->Format);
		}
		else if (IB)
		{
			checkf(ValidateBufferFormat(ShaderStage, BindIndex, (EPixelFormat)SRV->Format), TEXT("Invalid buffer format %d for index %d, shader %d"), SRV->Format, BindIndex, ShaderStage);
			SetShaderBuffer(ShaderStage, IB->Buffer, nil, 0, IB->GetSize(), BindIndex, (EPixelFormat)SRV->Format);
		}
		else if (SB)
		{
			SetShaderBuffer(ShaderStage, SB->Buffer, nil, 0, SB->GetSize(), BindIndex);
		}
	}
}

bool FMetalStateCache::IsLinearBuffer(EShaderFrequency ShaderStage, uint32 BindIndex)
{
    switch (ShaderStage)
    {
        case SF_Vertex:
        {
            return (GraphicsPSO->VertexShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
            break;
        }
        case SF_Pixel:
        {
            return (GraphicsPSO->PixelShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
            break;
        }
        case SF_Hull:
        {
            return (GraphicsPSO->HullShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
            break;
        }
        case SF_Domain:
        {
            return (GraphicsPSO->DomainShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
            break;
        }
        case SF_Compute:
        {
            return (ComputeShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
        }
        default:
        {
            check(false);
            return false;
        }
    }
}

bool FMetalStateCache::ValidateBufferFormat(EShaderFrequency ShaderStage, uint32 BindIndex, EPixelFormat Format)
{
	switch (ShaderStage)
	{
		case SF_Vertex:
		{
			return (GraphicsPSO->VertexShader->Bindings.InvariantBuffers & (1 << BindIndex)) == 0 || (GMetalBufferFormats[Format].DataFormat == GraphicsPSO->VertexShader->Bindings.TypedBufferFormats[BindIndex]);
			break;
		}
		case SF_Pixel:
		{
			return (GraphicsPSO->PixelShader->Bindings.InvariantBuffers & (1 << BindIndex)) == 0 || (GMetalBufferFormats[Format].DataFormat == GraphicsPSO->PixelShader->Bindings.TypedBufferFormats[BindIndex]);
			break;
		}
		case SF_Hull:
		{
			return (GraphicsPSO->HullShader->Bindings.InvariantBuffers & (1 << BindIndex)) == 0 || (GMetalBufferFormats[Format].DataFormat == GraphicsPSO->HullShader->Bindings.TypedBufferFormats[BindIndex]);
			break;
		}
		case SF_Domain:
		{
			return (GraphicsPSO->DomainShader->Bindings.InvariantBuffers & (1 << BindIndex)) == 0 || (GMetalBufferFormats[Format].DataFormat == GraphicsPSO->DomainShader->Bindings.TypedBufferFormats[BindIndex]);
			break;
		}
		case SF_Compute:
		{
			return (ComputeShader->Bindings.InvariantBuffers & (1 << BindIndex)) == 0 || (GMetalBufferFormats[Format].DataFormat == ComputeShader->Bindings.TypedBufferFormats[BindIndex]);
		}
		default:
		{
			check(false);
			return false;
		}
	}
}

void FMetalStateCache::SetShaderUnorderedAccessView(EShaderFrequency ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV)
{
	if (UAV)
	{
		// figure out which one of the resources we need to set
		FMetalStructuredBuffer* StructuredBuffer = UAV->SourceView->SourceStructuredBuffer.GetReference();
		FMetalVertexBuffer* VertexBuffer = UAV->SourceView->SourceVertexBuffer.GetReference();
		FMetalIndexBuffer* IndexBuffer = UAV->SourceView->SourceIndexBuffer.GetReference();
		FRHITexture* Texture = UAV->SourceView->SourceTexture.GetReference();
		FMetalSurface* Surface = UAV->SourceView->TextureView;
		if (StructuredBuffer)
		{
			SetShaderBuffer(ShaderStage, StructuredBuffer->Buffer, nil, 0, StructuredBuffer->GetSize(), BindIndex);
		}
		else if (VertexBuffer)
		{
			check(!VertexBuffer->Data && VertexBuffer->Buffer);
			if (IsLinearBuffer(ShaderStage, BindIndex) && UAV->SourceView->GetLinearTexture(true))
			{
				ns::AutoReleased<FMetalTexture> Tex;
				Tex = UAV->SourceView->GetLinearTexture(true);
				SetShaderTexture(ShaderStage, Tex, BindIndex);
                
                uint32 PackedLen = (Tex.GetWidth() | (Tex.GetHeight() << 16));
                SetShaderBuffer(ShaderStage, VertexBuffer->Buffer, VertexBuffer->Data, 0, PackedLen, BindIndex, (EPixelFormat)UAV->SourceView->Format);
			}
			else
			{
				checkf(ValidateBufferFormat(ShaderStage, BindIndex, (EPixelFormat)UAV->SourceView->Format), TEXT("Invalid buffer format %d for index %d, shader %d"), UAV->SourceView->Format, BindIndex, ShaderStage);
				SetShaderBuffer(ShaderStage, VertexBuffer->Buffer, VertexBuffer->Data, 0, VertexBuffer->GetSize(), BindIndex, (EPixelFormat)UAV->SourceView->Format);
			}
		}
		else if (IndexBuffer)
		{
			check(IndexBuffer->Buffer);
			if (IsLinearBuffer(ShaderStage, BindIndex) && UAV->SourceView->GetLinearTexture(true))
			{
				ns::AutoReleased<FMetalTexture> Tex;
				Tex = UAV->SourceView->GetLinearTexture(true);
				SetShaderTexture(ShaderStage, Tex, BindIndex);
				
				uint32 PackedLen = (Tex.GetWidth() | (Tex.GetHeight() << 16));
				SetShaderBuffer(ShaderStage, IndexBuffer->Buffer, nullptr, 0, PackedLen, BindIndex, (EPixelFormat)UAV->SourceView->Format);
			}
			else
			{
				checkf(ValidateBufferFormat(ShaderStage, BindIndex, (EPixelFormat)UAV->SourceView->Format), TEXT("Invalid buffer format %d for index %d, shader %d"), UAV->SourceView->Format, BindIndex, ShaderStage);
				SetShaderBuffer(ShaderStage, IndexBuffer->Buffer, nullptr, 0, IndexBuffer->GetSize(), BindIndex, (EPixelFormat)UAV->SourceView->Format);
			}
		}
		else if (Texture)
		{
			if (!Surface)
			{
				Surface = GetMetalSurfaceFromRHITexture(Texture);
			}
			if (Surface != nullptr)
			{
				FMetalSurface* Source = GetMetalSurfaceFromRHITexture(Texture);
				
				FPlatformAtomics::InterlockedExchange(&Surface->Written, 1);
				FPlatformAtomics::InterlockedExchange(&Source->Written, 1);
				
				SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderResourceView(nullptr, SF_Pixel, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderResourceView(nullptr, SF_Vertex, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderResourceView(nullptr, SF_Compute, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderResourceView(nullptr, SF_Hull, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderResourceView(nullptr, SF_Domain, BindIndex, SRV);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState, float CurrentTime)
{
	check(SamplerState->State);
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderSamplerState(SF_Pixel, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderSamplerState(SF_Vertex, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderSamplerState(SF_Compute, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderSamplerState(SF_Hull, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderSamplerState(SF_Domain, SamplerState, BindIndex);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderUnorderedAccessView(SF_Pixel, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderUnorderedAccessView(SF_Vertex, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderUnorderedAccessView(SF_Compute, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderUnorderedAccessView(SF_Hull, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderUnorderedAccessView(SF_Domain, BindIndex, UAV);
			break;
			
		default:
			check(0);
			break;
	}
}


template <typename MetalResourceType>
inline int32 FMetalStateCache::SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex, float CurrentTime)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);
			
			MetalResourceType* ResourcePtr = (MetalResourceType*)Resources[ResourceIndex].GetReference();
			
			// todo: could coalesce adjacent bound resources.
			SetResource(ShaderStage, BindIndex, ResourcePtr, CurrentTime);
			
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <class ShaderType>
void FMetalStateCache::SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage)
{
	checkSlow(Shader);
	
	EShaderFrequency Frequency;
	switch(ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_VERTEX:
			Frequency = SF_Vertex;
			break;
		case CrossCompiler::SHADER_STAGE_HULL:
			Frequency = SF_Hull;
			break;
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			Frequency = SF_Domain;
			break;
		case CrossCompiler::SHADER_STAGE_PIXEL:
			Frequency = SF_Pixel;
			break;
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			Frequency = SF_Compute;
			break;
		default:
			Frequency = SF_NumFrequencies; //Silence a compiler warning/error
			check(false);
			break;
	}

	float CurrentTime = FPlatformTime::Seconds();

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->Bindings.ShaderResourceTable.ResourceTableBits & GetDirtyUniformBuffers(Frequency);
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FMetalUniformBuffer* Buffer = (FMetalUniformBuffer*)GetBoundUniformBuffers(Frequency)[BufferIndex];
		if (Buffer)
		{
			check(BufferIndex < Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes.Num());
			check(Buffer->GetLayout().GetHash() == Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);
			
			// todo: could make this two pass: gather then set
			SetShaderResourcesFromBuffer<FRHITexture>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.TextureMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalShaderResourceView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalSamplerState>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.SamplerMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalUnorderedAccessView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.UnorderedAccessViewMap.GetData(), BufferIndex, CurrentTime);
		}
	}
	SetDirtyUniformBuffers(Frequency, 0);
}

void FMetalStateCache::CommitRenderResources(FMetalCommandEncoder* Raster)
{
	check(IsValidRef(GraphicsPSO));
    
    SetResourcesFromTables(GraphicsPSO->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, Raster, SF_Vertex, GraphicsPSO->VertexShader->Bindings);
	
    if (IsValidRef(GraphicsPSO->PixelShader))
    {
    	SetResourcesFromTables(GraphicsPSO->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL);
        GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, Raster, SF_Pixel, GraphicsPSO->PixelShader->Bindings);
    }
}

void FMetalStateCache::CommitTessellationResources(FMetalCommandEncoder* Raster, FMetalCommandEncoder* Compute)
{
	check(IsValidRef(GraphicsPSO));
    check(IsValidRef(GraphicsPSO->HullShader) && IsValidRef(GraphicsPSO->DomainShader));
    
    SetResourcesFromTables(GraphicsPSO->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, Compute, SF_Vertex, GraphicsPSO->VertexShader->Bindings);
	
    if (IsValidRef(GraphicsPSO->PixelShader))
    {
    	SetResourcesFromTables(GraphicsPSO->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL);
        GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, Raster, SF_Pixel, GraphicsPSO->PixelShader->Bindings);
    }
    
    SetResourcesFromTables(GraphicsPSO->HullShader, CrossCompiler::SHADER_STAGE_HULL);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_HULL).CommitPackedGlobals(this, Compute, SF_Hull, GraphicsPSO->HullShader->Bindings);
	
	SetResourcesFromTables(GraphicsPSO->DomainShader, CrossCompiler::SHADER_STAGE_DOMAIN);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_DOMAIN).CommitPackedGlobals(this, Raster, SF_Domain, GraphicsPSO->DomainShader->Bindings);
}

void FMetalStateCache::CommitComputeResources(FMetalCommandEncoder* Compute)
{
	check(IsValidRef(ComputeShader));
	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);
	
	GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedGlobals(this, Compute, SF_Compute, ComputeShader->Bindings);
}

bool FMetalStateCache::PrepareToRestart(void)
{
	if(CanRestartRenderPass())
	{
		return true;
	}
	else
	{
		if (SampleCount <= 1)
		{
			// Deferred store actions make life a bit easier...
			static bool bSupportsDeferredStore = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
			
			FRHISetRenderTargetsInfo Info = GetRenderTargetsInfo();
			for (int32 RenderTargetIndex = 0; RenderTargetIndex < Info.NumColorRenderTargets; RenderTargetIndex++)
			{
				FRHIRenderTargetView& RenderTargetView = Info.ColorRenderTarget[RenderTargetIndex];
				RenderTargetView.LoadAction = ERenderTargetLoadAction::ELoad;
				check(RenderTargetView.Texture == nil || RenderTargetView.StoreAction == ERenderTargetStoreAction::EStore);
			}
			Info.bClearColor = false;
			
			if (Info.DepthStencilRenderTarget.Texture)
			{
				Info.DepthStencilRenderTarget.DepthLoadAction = ERenderTargetLoadAction::ELoad;
				check(bSupportsDeferredStore || !Info.DepthStencilRenderTarget.GetDepthStencilAccess().IsDepthWrite() || Info.DepthStencilRenderTarget.DepthStoreAction == ERenderTargetStoreAction::EStore);
				Info.bClearDepth = false;
				
				Info.DepthStencilRenderTarget.StencilLoadAction = ERenderTargetLoadAction::ELoad;
				// @todo Stencil writes that need to persist must use ERenderTargetStoreAction::EStore on iOS.
				// We should probably be using deferred store actions so that we can safely lazily instantiate encoders.
				check(bSupportsDeferredStore || !Info.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite() || Info.DepthStencilRenderTarget.GetStencilStoreAction() == ERenderTargetStoreAction::EStore);
				Info.bClearStencil = false;
			}
			
			InvalidateRenderTargets();
			return SetRenderTargetsInfo(Info, GetVisibilityResultsBuffer(), true) && CanRestartRenderPass();
		}
		else
		{
			return false;
		}
	}
}

void FMetalStateCache::SetStateDirty(void)
{	
	RasterBits = UINT32_MAX;
    PipelineBits = EMetalPipelineFlagMask;
	for (uint32 i = 0; i < SF_NumFrequencies; i++)
	{
		ShaderBuffers[i].Bound = UINT32_MAX;
#if PLATFORM_MAC
#ifndef UINT128_MAX
#define UINT128_MAX (((__uint128_t)1 << 127) - (__uint128_t)1 + ((__uint128_t)1 << 127))
#endif
		ShaderTextures[i].Bound = UINT128_MAX;
#else
		ShaderTextures[i].Bound = UINT32_MAX;
#endif
		ShaderSamplers[i].Bound = UINT16_MAX;
	}
}

void FMetalStateCache::SetRenderStoreActions(FMetalCommandEncoder& CommandEncoder, bool const bConditionalSwitch)
{
	check(CommandEncoder.IsRenderCommandEncoderActive())
	{
		// Deferred store actions make life a bit easier...
		static bool bSupportsDeferredStore = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
		if (bConditionalSwitch && bSupportsDeferredStore)
		{
			ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> ColorAttachments = RenderPassDesc.GetColorAttachments();
			for (int32 RenderTargetIndex = 0; RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets; RenderTargetIndex++)
			{
				FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
				if(RenderTargetView.Texture != nil)
				{
					const bool bMultiSampled = (ColorAttachments[RenderTargetIndex].GetTexture().GetSampleCount() > 1);
					ColorStore[RenderTargetIndex] = GetConditionalMetalRTStoreAction(bMultiSampled);
				}
			}
			
			if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
			{
				const bool bMultiSampled = RenderPassDesc.GetDepthAttachment().GetTexture() && (RenderPassDesc.GetDepthAttachment().GetTexture().GetSampleCount() > 1);
				DepthStore = GetConditionalMetalRTStoreAction(bMultiSampled);
				StencilStore = GetConditionalMetalRTStoreAction(false);
			}
		}
		CommandEncoder.SetRenderPassStoreActions(ColorStore, DepthStore, StencilStore);
	}
}

void FMetalStateCache::FlushVisibilityResults(FMetalCommandEncoder& CommandEncoder)
{
#if PLATFORM_MAC
	if(VisibilityResults && VisibilityResults->Buffer && VisibilityResults->Buffer.GetStorageMode() == mtlpp::StorageMode::Managed && VisibilityWritten && CommandEncoder.IsRenderCommandEncoderActive())
	{
		mtlpp::Fence Fence = CommandEncoder.EndEncoding();
		
		CommandEncoder.BeginBlitCommandEncoding();
		CommandEncoder.WaitForFence(Fence);
		
		mtlpp::BlitCommandEncoder& Encoder = CommandEncoder.GetBlitCommandEncoder();

		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CommandEncoder.GetCommandBufferStats(), __FUNCTION__));
		MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Synchronize(VisibilityResults->Buffer));
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, CommandEncoder.GetBlitCommandEncoderDebugging().Synchronize(VisibilityResults->Buffer));
		
		VisibilityWritten = 0;
	}
#endif
}

void FMetalStateCache::SetRenderState(FMetalCommandEncoder& CommandEncoder, FMetalCommandEncoder* PrologueEncoder)
{
	if (RasterBits)
	{
		if (RasterBits & EMetalRenderFlagViewport)
		{
			CommandEncoder.SetViewport(Viewport, ActiveViewports);
		}
		if (RasterBits & EMetalRenderFlagFrontFacingWinding)
		{
			CommandEncoder.SetFrontFacingWinding(mtlpp::Winding::CounterClockwise);
		}
		if (RasterBits & EMetalRenderFlagCullMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetCullMode(TranslateCullMode(RasterizerState->State.CullMode));
		}
		if (RasterBits & EMetalRenderFlagDepthBias)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetDepthBias(RasterizerState->State.DepthBias, RasterizerState->State.SlopeScaleDepthBias, FLT_MAX);
		}
		if ((RasterBits & EMetalRenderFlagScissorRect))
		{
			CommandEncoder.SetScissorRect(Scissor, ActiveScissors);
		}
		if (RasterBits & EMetalRenderFlagTriangleFillMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetTriangleFillMode(TranslateFillMode(RasterizerState->State.FillMode));
		}
		if (RasterBits & EMetalRenderFlagBlendColor)
		{
			CommandEncoder.SetBlendColor(BlendFactor.R, BlendFactor.G, BlendFactor.B, BlendFactor.A);
		}
		if (RasterBits & EMetalRenderFlagDepthStencilState)
		{
			check(IsValidRef(DepthStencilState));
			CommandEncoder.SetDepthStencilState(DepthStencilState ? DepthStencilState->State : nil);
		}
		if (RasterBits & EMetalRenderFlagStencilReferenceValue)
		{
			CommandEncoder.SetStencilReferenceValue(StencilRef);
		}
		if (RasterBits & EMetalRenderFlagVisibilityResultMode)
		{
			CommandEncoder.SetVisibilityResultMode(VisibilityMode, VisibilityOffset);
			if (VisibilityMode != mtlpp::VisibilityResultMode::Disabled)
			{
            	VisibilityWritten = VisibilityOffset + FMetalQueryBufferPool::EQueryResultMaxSize;
			}
        }
		RasterBits = 0;
	}
}

void FMetalStateCache::SetRenderPipelineState(FMetalCommandEncoder& CommandEncoder, FMetalCommandEncoder* PrologueEncoder)
{
    if ((PipelineBits & EMetalPipelineFlagRasterMask) != 0)
    {
    	// @todo Could optimise it so that we only re-evaluate the buffer hashes if the shader buffer binding mask changes when changing the PSO
        if (PipelineBits & EMetalPipelineFlagPipelineState)
        {
        	PipelineBits |= (EMetalPipelineFlagVertexBuffers|EMetalPipelineFlagPixelBuffers|EMetalPipelineFlagDomainBuffers);
        }
    	
    	if (PipelineBits & EMetalPipelineFlagVertexBuffers)
    	{
    		ShaderBuffers[SF_Vertex].FormatHash = GraphicsPSO->VertexShader->GetBindingHash(ShaderBuffers[SF_Vertex].Formats);
    	}
    	
    	if (PipelineBits & EMetalPipelineFlagPixelBuffers)
    	{
   			ShaderBuffers[SF_Pixel].FormatHash = (IsValidRef(GraphicsPSO->PixelShader)) ? GraphicsPSO->PixelShader->GetBindingHash(ShaderBuffers[SF_Pixel].Formats) : 0;
    	}
    	
    	if (PipelineBits & EMetalPipelineFlagDomainBuffers)
    	{
   			ShaderBuffers[SF_Domain].FormatHash = (IsValidRef(GraphicsPSO->DomainShader)) ? GraphicsPSO->DomainShader->GetBindingHash(ShaderBuffers[SF_Domain].Formats) : 0;
    	}
    
    	EPixelFormat const* const VertexFormats = ShaderBuffers[SF_Vertex].Formats;
    	EPixelFormat const* const PixelFormats = (IsValidRef(GraphicsPSO->PixelShader)) ? ShaderBuffers[SF_Pixel].Formats : nullptr;
    	EPixelFormat const* const DomainFormats = (IsValidRef(GraphicsPSO->DomainShader)) ? ShaderBuffers[SF_Domain].Formats : nullptr;
    
        // Some Intel drivers need RenderPipeline state to be set after DepthStencil state to work properly
        // As it happens, in order to use function constants to emulate Buffer<T>/RWBuffer<T> implicit typing we'll do that anyway.
    	FMetalShaderPipeline* Pipeline = GetPipelineState(ShaderBuffers[SF_Vertex].FormatHash, ShaderBuffers[SF_Pixel].FormatHash, ShaderBuffers[SF_Domain].FormatHash, VertexFormats, PixelFormats, DomainFormats);
        // FMetalShaderPipeline* Pipeline = GetPipelineState(0, 0, 0, nullptr, nullptr, nullptr);
        check(Pipeline);
        CommandEncoder.SetRenderPipelineState(Pipeline);
        if (Pipeline->ComputePipelineState)
        {
            check(PrologueEncoder);
            PrologueEncoder->SetComputePipelineState(Pipeline);
        }
        
        PipelineBits &= EMetalPipelineFlagComputeMask;
    }
}

void FMetalStateCache::SetComputePipelineState(FMetalCommandEncoder& CommandEncoder)
{
	if ((PipelineBits & EMetalPipelineFlagComputeMask) != 0)
	{
		if (PipelineBits & EMetalPipelineFlagComputeShader)
		{
			PipelineBits |= EMetalPipelineFlagComputeBuffers;
		}
	
		if (PipelineBits & EMetalPipelineFlagComputeBuffers)
		{
			ShaderBuffers[SF_Compute].FormatHash = ComputeShader->GetBindingHash(ShaderBuffers[SF_Compute].Formats);
		}
	    
	    FMetalShaderPipeline* Pipeline = ComputeShader->GetPipeline(ShaderBuffers[SF_Compute].Formats, ShaderBuffers[SF_Compute].FormatHash);
	    check(Pipeline);
	    CommandEncoder.SetComputePipelineState(Pipeline);
        
        PipelineBits &= EMetalPipelineFlagRasterMask;
    }
}

void FMetalStateCache::CommitResourceTable(EShaderFrequency const Frequency, mtlpp::FunctionType const Type, FMetalCommandEncoder& CommandEncoder)
{
	FMetalBufferBindings& BufferBindings = ShaderBuffers[Frequency];
	while(BufferBindings.Bound)
	{
		uint32 Index = __builtin_ctz(BufferBindings.Bound);
		BufferBindings.Bound &= ~(1 << Index);
		
		if (Index < ML_MaxBuffers)
		{
			FMetalBufferBinding& Binding = BufferBindings.Buffers[Index];
			if (Binding.Buffer)
			{
				CommandEncoder.SetShaderBuffer(Type, Binding.Buffer, Binding.Offset, Binding.Length, Index, BufferBindings.Formats[Index]);
				
				if (Binding.Buffer.IsSingleUse())
				{
					Binding.Buffer = nil;
				}
			}
			else if (Binding.Bytes)
			{
				CommandEncoder.SetShaderData(Type, Binding.Bytes, Binding.Offset, Index, BufferBindings.Formats[Index]);
			}
		}
	}
	
	FMetalTextureBindings& TextureBindings = ShaderTextures[Frequency];
#if PLATFORM_MAC
	uint64 LoTextures = (uint64)TextureBindings.Bound;
	while(LoTextures)
	{
		uint32 Index = __builtin_ctzll(LoTextures);
		LoTextures &= ~(uint64(1) << uint64(Index));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index);
		}
	}
	
	uint64 HiTextures = (uint64)(TextureBindings.Bound >> FMetalTextureMask(64));
	while(HiTextures)
	{
		uint32 Index = __builtin_ctzll(HiTextures);
		HiTextures &= ~(uint64(1) << uint64(Index));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index + 64);
		}
	}
	
	TextureBindings.Bound = FMetalTextureMask(LoTextures) | (FMetalTextureMask(HiTextures) << FMetalTextureMask(64));
	check(TextureBindings.Bound == 0);
#else
	while(TextureBindings.Bound)
	{
		uint32 Index = __builtin_ctz(TextureBindings.Bound);
		TextureBindings.Bound &= ~(FMetalTextureMask(FMetalTextureMask(1) << FMetalTextureMask(Index)));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index);
		}
	}
#endif
	
    FMetalSamplerBindings& SamplerBindings = ShaderSamplers[Frequency];
	while(SamplerBindings.Bound)
	{
		uint32 Index = __builtin_ctz(SamplerBindings.Bound);
		SamplerBindings.Bound &= ~(1 << Index);
		
		if (Index < ML_MaxSamplers && SamplerBindings.Samplers[Index])
		{
			CommandEncoder.SetShaderSamplerState(Type, SamplerBindings.Samplers[Index], Index);
		}
	}
}

FTexture2DRHIRef FMetalStateCache::CreateFallbackDepthStencilSurface(uint32 Width, uint32 Height)
{
#if PLATFORM_MAC
	if (!IsValidRef(FallbackDepthStencilSurface) || FallbackDepthStencilSurface->GetSizeX() < Width || FallbackDepthStencilSurface->GetSizeY() < Height)
#else
	if (!IsValidRef(FallbackDepthStencilSurface) || FallbackDepthStencilSurface->GetSizeX() != Width || FallbackDepthStencilSurface->GetSizeY() != Height)
#endif
	{
		FRHIResourceCreateInfo TexInfo;
		FallbackDepthStencilSurface = RHICreateTexture2D(Width, Height, PF_DepthStencil, 1, 1, TexCreate_DepthStencilTargetable, TexInfo);
	}
	check(IsValidRef(FallbackDepthStencilSurface));
	return FallbackDepthStencilSurface;
}

void FMetalStateCache::DiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	if (Depth)
	{
		DepthStore = mtlpp::StoreAction::DontCare;
	}

	if (Stencil)
	{
		StencilStore = mtlpp::StoreAction::DontCare;
	}

	for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		if ((ColorBitMask & (1u << Index)) != 0)
		{
			ColorStore[Index] = mtlpp::StoreAction::DontCare;
		}
	}
}

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMVideoDecoder.h"

#if WITH_WEBM_LIBS

#include "WebMMediaPrivate.h"
#include "WebMMediaFrame.h"
#include "WebMMediaTextureSample.h"
#include "MediaShaders.h"
#include "MediaSamples.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "Containers/DynamicRHIResourceArray.h"

namespace
{
	/**
	* RHI resources for rendering texture to polygon.
	*/
	class FMoviePlaybackResources
		: public FRenderResource
	{
	public:

		FVertexDeclarationRHIRef VertexDeclarationRHI;
		FVertexBufferRHIRef VertexBufferRHI;

		virtual ~FMoviePlaybackResources() { }

		virtual void InitRHI() override
		{
			FVertexDeclarationElementList Elements;
			uint16 Stride = sizeof(FMediaElementVertex);
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, Position), VET_Float4, 0, Stride));
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, TextureCoordinate), VET_Float2, 1, Stride));
			VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);

			TResourceArray<FMediaElementVertex> Vertices;
			Vertices.AddUninitialized(4);
			Vertices[0].Position.Set(-1.0f, 1.0f, 1.0f, 1.0f);
			Vertices[0].TextureCoordinate.Set(0.0f, 0.0f);
			Vertices[1].Position.Set(1.0f, 1.0f, 1.0f, 1.0f);
			Vertices[1].TextureCoordinate.Set(1.0f, 0.0f);
			Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f);
			Vertices[2].TextureCoordinate.Set(0.0f, 1.0f);
			Vertices[3].Position.Set(1.0f, -1.0f, 1.0f, 1.0f);
			Vertices[3].TextureCoordinate.Set(1.0f, 1.0f);
			FRHIResourceCreateInfo CreateInfo(&Vertices);
			VertexBufferRHI = RHICreateVertexBuffer(sizeof(FMediaElementVertex) * 4, BUF_Static, CreateInfo);
		}

		virtual void ReleaseRHI() override
		{
			VertexDeclarationRHI.SafeRelease();
			VertexBufferRHI.SafeRelease();
		}
	};

	/** Singleton instance of the RHI resources. */
	TGlobalResource<FMoviePlaybackResources> GMoviePlayerResources;
}

FWebMVideoDecoder::FWebMVideoDecoder(TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> InSamples)
	: Samples(InSamples)
	, VideoSamplePool(new FWebMMediaTextureSamplePool)
	, bTexturesCreated(false)
	, bIsInitialized(false)
{
}

FWebMVideoDecoder::~FWebMVideoDecoder()
{
	Close();
}

bool FWebMVideoDecoder::Initialize(const char* CodecName)
{
	Close();

	const int32 NumOfThreads = 1;
	const vpx_codec_dec_cfg_t CodecConfig = { NumOfThreads, 0, 0 };

	if (FCStringAnsi::Strcmp(CodecName, "V_VP8") == 0)
	{
		verify(vpx_codec_dec_init(&Context, vpx_codec_vp8_dx(), &CodecConfig, /*VPX_CODEC_USE_FRAME_THREADING*/ 0) == 0);
	}
	else if (FCStringAnsi::Strcmp(CodecName, "V_VP9") == 0)
	{
		verify(vpx_codec_dec_init(&Context, vpx_codec_vp9_dx(), &CodecConfig, /*VPX_CODEC_USE_FRAME_THREADING*/ 0) == 0);
	}
	else
	{
		UE_LOG(LogWebMMedia, Display, TEXT("Unsupported video codec: %s"), CodecName);
		return false;
	}

	bIsInitialized = true;

	return true;
}

void FWebMVideoDecoder::DecodeVideoFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames)
{
	FGraphEventRef PreviousDecodingTask = VideoDecodingTask;

	VideoDecodingTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this, PreviousDecodingTask, VideoFrames]()
	{
		if(PreviousDecodingTask && !PreviousDecodingTask->IsComplete())
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(PreviousDecodingTask);
		}

		DoDecodeVideoFrames(VideoFrames);
	}, TStatId(), nullptr, ENamedThreads::AnyThread);
}

bool FWebMVideoDecoder::IsBusy() const
{
	return VideoDecodingTask && !VideoDecodingTask->IsComplete();
}

void FWebMVideoDecoder::DoDecodeVideoFrames(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames)
{
	for (const TSharedPtr<FWebMFrame>& VideoFrame : VideoFrames)
	{
		if (vpx_codec_decode(&Context, VideoFrame->Data.GetData(), VideoFrame->Data.Num(), nullptr, 0) != 0)
		{
			UE_LOG(LogWebMMedia, Display, TEXT("Error decoding video frame"));
			return;
		}

		const void* ImageIter = nullptr;
		while (const vpx_image_t* Image = vpx_codec_get_frame(&Context, &ImageIter))
		{
			if (!bTexturesCreated)
			{
				// First creation of conversion textures

				bTexturesCreated = true;

				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(WebMMediaPlayerCreateTextures,
					FWebMVideoDecoder*, Self, this, const vpx_image_t*, Image, Image,
					{
						Self->CreateTextures(Image);
					});
			}

			TSharedRef<FWebMMediaTextureSample, ESPMode::ThreadSafe> VideoSample = VideoSamplePool->AcquireShared();

			VideoSample->Initialize(FIntPoint(Image->d_w, Image->d_h), FIntPoint(Image->d_w, Image->d_h), VideoFrame->Time);

			FConvertParams Params;
			Params.VideoSample = VideoSample;
			Params.Image = Image;

			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(WebMMediaPlayerConvertYUVToRGB,
				FWebMVideoDecoder*, Self, this, FConvertParams, Params, Params,
				{
					Self->ConvertYUVToRGBAndSubmit(Params);
				});
		}
	}
}

void FWebMVideoDecoder::CreateTextures(const vpx_image_t* Image)
{
	FRHIResourceCreateInfo CreateInfo;

	DecodedY = RHICreateTexture2D(Image->stride[0], Image->d_h, PF_G8, 1, 1, TexCreate_Dynamic, CreateInfo);
	DecodedU = RHICreateTexture2D(Image->stride[1], Image->d_h / 2, PF_G8, 1, 1, TexCreate_Dynamic, CreateInfo);
	DecodedV = RHICreateTexture2D(Image->stride[2], Image->d_h / 2, PF_G8, 1, 1, TexCreate_Dynamic, CreateInfo);
}

void FWebMVideoDecoder::Close()
{
	if (VideoDecodingTask && !VideoDecodingTask->IsComplete())
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(VideoDecodingTask);
	}

	// Make sure all compute shader decoding is done
	FlushRenderingCommands();

	if (bIsInitialized)
	{
		vpx_codec_destroy(&Context);
		bIsInitialized = false;
	}

	bTexturesCreated = false;
}

void FWebMVideoDecoder::ConvertYUVToRGBAndSubmit(const FConvertParams& Params)
{
	TSharedPtr<FWebMMediaTextureSample, ESPMode::ThreadSafe> VideoSample = Params.VideoSample;
	check(VideoSample.IsValid());
	const vpx_image_t* Image = Params.Image;

	VideoSample->CreateTexture();

	// render video frame into output texture
	FRHICommandListImmediate& CommandList = GetImmediateCommandList_ForRenderCommand();
	{
		// copy the Y plane out of the video buffer
		{
			uint32 Stride = 0;
			void * TextureMemory = GDynamicRHI->LockTexture2D_RenderThread(CommandList, DecodedY.GetReference(), 0, RLM_WriteOnly, Stride, false);

			if (TextureMemory)
			{
				check(Stride == Image->stride[0]);
				memcpy(TextureMemory, Image->planes[0], Image->stride[0] * Image->d_h);
				GDynamicRHI->UnlockTexture2D_RenderThread(CommandList, DecodedY.GetReference(), 0, false);
			}
		}

		// copy the U plane out of the video buffer
		{
			uint32 Stride = 0;
			void * TextureMemory = GDynamicRHI->LockTexture2D_RenderThread(CommandList, DecodedU.GetReference(), 0, RLM_WriteOnly, Stride, false);

			if (TextureMemory)
			{
				check(Stride == Image->stride[1]);
				memcpy(TextureMemory, Image->planes[1], Image->stride[1] * Image->d_h / 2);
				GDynamicRHI->UnlockTexture2D_RenderThread(CommandList, DecodedU.GetReference(), 0, false);
			}
		}

		// copy the V plane out of the video buffer
		{
			uint32 Stride = 0;
			void * TextureMemory = GDynamicRHI->LockTexture2D_RenderThread(CommandList, DecodedV.GetReference(), 0, RLM_WriteOnly, Stride, false);

			if (TextureMemory)
			{
				check(Stride == Image->stride[2]);
				memcpy(TextureMemory, Image->planes[2], Image->stride[2] * Image->d_h / 2);
				GDynamicRHI->UnlockTexture2D_RenderThread(CommandList, DecodedV.GetReference(), 0, false);
			}
		}

		FRHITexture* RenderTarget = VideoSample->GetTexture();
		SetRenderTargets(CommandList, 1, &RenderTarget, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);

		// configure media shaders
		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		TShaderMapRef<FYUVConvertPS> PixelShader(ShaderMap);
		TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		{
			CommandList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMoviePlayerResources.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		}

		SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
		PixelShader->SetParameters(CommandList, DecodedY->GetTexture2D(), DecodedU->GetTexture2D(), DecodedV->GetTexture2D(), FIntPoint(Image->d_w, Image->d_h), MediaShaders::YuvToSrgbDefault, true);

		// draw full-size quad
		CommandList.SetViewport(0, 0, 0.0f, Image->d_w, Image->d_h, 1.0f);
		CommandList.SetStreamSource(0, GMoviePlayerResources.VertexBufferRHI, 0);
		CommandList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);
		CommandList.CopyToResolveTarget(RenderTarget, RenderTarget, FResolveParams());

		Samples->AddVideo(VideoSample.ToSharedRef());
	}
}

#endif // WITH_WEBM_LIBS

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Channels/RemoteSessionARCameraChannel.h"
#include "RemoteSession.h"
#include "Framework/Application/SlateApplication.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "MessageHandler/Messages.h"

#include "Async/Async.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "SceneViewExtension.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MediaShaders.h"
#include "PipelineStateCache.h"
#include "RHIUtilities.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "RendererInterface.h"
#include "ScreenRendering.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcessParameters.h"
#include "EngineModule.h"

#include "GeneralProjectSettings.h"
#include "ARTextures.h"
#include "ARSessionConfig.h"
#include "ARBlueprintLibrary.h"

#include "ARBlueprintLibrary.h"
#include "IAppleImageUtilsPlugin.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "CommonRenderResources.h"

#define CAMERA_MESSAGE_ADDRESS TEXT("/ARCamera")

TAutoConsoleVariable<int32> CVarJPEGQuality(
	TEXT("remote.arcameraquality"),
	85,
	TEXT("Sets quality (1-100)"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarJPEGColor(
	TEXT("remote.arcameracolorjpeg"),
	1,
	TEXT("1 (default) sends color data, 0 sends B&W"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarJPEGGpu(
	TEXT("remote.arcameraqcgpucompressed"),
	1,
	TEXT("1 (default) compresses on the GPU, 0 on the CPU"),
	ECVF_Default);

/** Shaders to render our post process material */
class FRemoteSessionARCameraVS :
	public FMaterialShader
{
	DECLARE_SHADER_TYPE(FRemoteSessionARCameraVS, Material);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->GetMaterialDomain() == MD_PostProcess && !IsMobilePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Material->GetBlendableLocation() != BL_AfterTonemapping) ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_AR_PASSTHROUGH"), 1);
	}

	FRemoteSessionARCameraVS() { }
	FRemoteSessionARCameraVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRemoteSessionARCameraVS, TEXT("/Engine/Private/PostProcessMaterialShaders.usf"), TEXT("MainVS_VideoOverlay"), SF_Vertex);

class FRemoteSessionARCameraPS :
	public FMaterialShader
{
	DECLARE_SHADER_TYPE(FRemoteSessionARCameraPS, Material);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->GetMaterialDomain() == MD_PostProcess && !IsMobilePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);
		OutEnvironment.SetDefine(TEXT("OUTPUT_MOBILE_HDR"), IsMobileHDR() ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Material->GetBlendableLocation() != BL_AfterTonemapping) ? 1 : 0);
	}

	FRemoteSessionARCameraPS() {}
	FRemoteSessionARCameraPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMaterialShader(Initializer)
	{
		for (uint32 InputIter = 0; InputIter < ePId_Input_MAX; ++InputIter)
		{
			PostprocessInputParameter[InputIter].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("PostprocessInput%d"), InputIter));
			PostprocessInputParameterSampler[InputIter].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("PostprocessInput%dSampler"), InputIter));
		}
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* Material)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, Material, *Material->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneTextureSetupMode::None);

		for (uint32 InputIter = 0; InputIter < ePId_Input_MAX; ++InputIter)
		{
			if (PostprocessInputParameter[InputIter].IsBound())
			{
				SetTextureParameter(
						RHICmdList,
						ShaderRHI,
						PostprocessInputParameter[InputIter],
						PostprocessInputParameterSampler[InputIter],
						TStaticSamplerState<>::GetRHI(),
						GBlackTexture->TextureRHI);
			}
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter PostprocessInputParameter[ePId_Input_MAX];
	FShaderResourceParameter PostprocessInputParameterSampler[ePId_Input_MAX];
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRemoteSessionARCameraPS, TEXT("/Engine/Private/PostProcessMaterialShaders.usf"), TEXT("MainPS_VideoOverlay"), SF_Pixel);

class FARCameraSceneViewExtension :
	public FSceneViewExtensionBase
{
public:
	FARCameraSceneViewExtension(const FAutoRegister& AutoRegister, FRemoteSessionARCameraChannel& InChannel);
	virtual ~FARCameraSceneViewExtension() { }

private:
	//~ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual bool IsActiveThisFrame(FViewport* InViewport) const override;
	//~ISceneViewExtension interface

	void RenderARCamera_RenderThread(FRHICommandListImmediate& RHICmdList, const FSceneView& InView);

	/** The channel this is rendering for */
	FRemoteSessionARCameraChannel& Channel;

	/** Note the session channel is responsible for preventing GC */
	UMaterialInterface* PPMaterial;
	/** Index buffer for drawing the quad */
	FIndexBufferRHIRef IndexBufferRHI;
	/** Vertex buffer for drawing the quad */
	FVertexBufferRHIRef VertexBufferRHI;
};

FARCameraSceneViewExtension::FARCameraSceneViewExtension(const FAutoRegister& AutoRegister, FRemoteSessionARCameraChannel& InChannel) :
	FSceneViewExtensionBase(AutoRegister),
	Channel(InChannel)
{
}

void FARCameraSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{

}

void FARCameraSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{

}

void FARCameraSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{

}

void FARCameraSceneViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	if (VertexBufferRHI == nullptr || !VertexBufferRHI.IsValid())
	{
		// Setup vertex buffer
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(4);

		Vertices[0].Position = FVector4(0.f, 0.f, 0.f, 1.f);
		Vertices[0].UV = FVector2D(0.f, 0.f);

		Vertices[1].Position = FVector4(1.f, 0.f, 0.f, 1.f);
		Vertices[1].UV = FVector2D(1.f, 0.f);

		Vertices[2].Position = FVector4(0.f, 1.f, 0.f, 1.f);
		Vertices[2].UV = FVector2D(0.f, 1.f);

		Vertices[3].Position = FVector4(1.f, 1.f, 0.f, 1.f);
		Vertices[3].UV = FVector2D(1.f, 1.f);

		FRHIResourceCreateInfo CreateInfoVB(&Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfoVB);
	}

	if (IndexBufferRHI == nullptr || !IndexBufferRHI.IsValid())
	{
		// Setup index buffer
		const uint16 Indices[] = { 0, 1, 2, 2, 1, 3 };

		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		const uint32 NumIndices = ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

		FRHIResourceCreateInfo CreateInfoIB(&IndexBuffer);
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);
	}

	PPMaterial = Channel.GetPostProcessMaterial();
}

void FARCameraSceneViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{

}

void FARCameraSceneViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	if (PPMaterial == nullptr || !PPMaterial->IsValidLowLevel() ||
		VertexBufferRHI == nullptr || !VertexBufferRHI.IsValid() ||
		IndexBufferRHI == nullptr || !IndexBufferRHI.IsValid())
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < InViewFamily.Views.Num(); ++ViewIndex)
	{
		RenderARCamera_RenderThread(RHICmdList, *InViewFamily.Views[ViewIndex]);
	}
}

void FARCameraSceneViewExtension::RenderARCamera_RenderThread(FRHICommandListImmediate& RHICmdList, const FSceneView& InView)
{
#if PLATFORM_DESKTOP
	const auto FeatureLevel = InView.GetFeatureLevel();

	IRendererModule& RendererModule = GetRendererModule();

	const FMaterial* const CameraMaterial = PPMaterial->GetRenderProxy()->GetMaterial(FeatureLevel);
	const FMaterialShaderMap* const MaterialShaderMap = CameraMaterial->GetRenderingThreadShaderMap();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;

	FRemoteSessionARCameraVS* VertexShader = MaterialShaderMap->GetShader<FRemoteSessionARCameraVS>();
	FRemoteSessionARCameraPS* PixelShader = MaterialShaderMap->GetShader<FRemoteSessionARCameraPS>();
	check(PixelShader != nullptr && VertexShader != nullptr);

	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	const FIntPoint ViewSize = InView.UnconstrainedViewRect.Size();
	FDrawRectangleParameters Parameters;
	Parameters.PosScaleBias = FVector4(ViewSize.X, ViewSize.Y, 0, 0);
	Parameters.UVScaleBias = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
	Parameters.InvTargetSizeAndTextureSize = FVector4(
			1.0f / ViewSize.X, 1.0f / ViewSize.Y,
			1.0f, 1.0f);

	SetUniformBufferParameterImmediate(RHICmdList, VertexShader->GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);
	VertexShader->SetParameters(RHICmdList, InView);
	PixelShader->SetParameters(RHICmdList, InView, PPMaterial->GetRenderProxy());

	if (VertexBufferRHI && IndexBufferRHI.IsValid())
	{
		RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
		RHICmdList.DrawIndexedPrimitive(
				IndexBufferRHI,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 4,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 2,
				/*NumInstances=*/ 1
			);
	}
#endif
}

bool FARCameraSceneViewExtension::IsActiveThisFrame(FViewport* InViewport) const
{
	return PLATFORM_DESKTOP && Channel.GetPostProcessMaterial() != nullptr;
}

static FName CameraImageParamName(TEXT("CameraImage"));

FRemoteSessionARCameraChannel::FRemoteSessionARCameraChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection)
	: IRemoteSessionChannel(InRole, InConnection)
	, LastQueuedTimestamp(0.f)
	, RenderingTextureIndex(0)
	, Connection(InConnection)
	, Role(InRole)
{
	if (InRole == ERemoteSessionChannelMode::Write)
	{
		if (UARBlueprintLibrary::GetARSessionStatus().Status != EARSessionStatus::Running)
		{
			UARSessionConfig* Config = NewObject<UARSessionConfig>();
			UARBlueprintLibrary::StartARSession(Config);
		}
	}
	
	RenderingTextures[0] = nullptr;
	RenderingTextures[1] = nullptr;
	PPMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/RemoteSession/ARCameraPostProcess.ARCameraPostProcess"));
	MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(PPMaterial, GetTransientPackage());
	if (MaterialInstanceDynamic != nullptr)
	{
		UTexture2D* DefaultTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		// Set the initial texture so we render something until data comes in
		MaterialInstanceDynamic->SetTextureParameterValue(CameraImageParamName, DefaultTexture);
	}

	if (Role == ERemoteSessionChannelMode::Read)
	{
		// Create our image renderer
		SceneViewExtension = FSceneViewExtensions::NewExtension<FARCameraSceneViewExtension>(*this);

		auto Delegate = FBackChannelDispatchDelegate::FDelegate::CreateRaw(this, &FRemoteSessionARCameraChannel::ReceiveARCameraImage);
		MessageCallbackHandle = Connection->AddMessageHandler(CAMERA_MESSAGE_ADDRESS, Delegate);
		Connection->SetMessageOptions(CAMERA_MESSAGE_ADDRESS, 1);
	}
}

FRemoteSessionARCameraChannel::~FRemoteSessionARCameraChannel()
{
	if (Role == ERemoteSessionChannelMode::Read)
	{
		// Remove the callback so it doesn't call back on an invalid this
		Connection->RemoveMessageHandler(CAMERA_MESSAGE_ADDRESS, MessageCallbackHandle);
	}
}

void FRemoteSessionARCameraChannel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RenderingTextures[0]);
	Collector.AddReferencedObject(RenderingTextures[1]);
	Collector.AddReferencedObject(PPMaterial);
	Collector.AddReferencedObject(MaterialInstanceDynamic);
}

void FRemoteSessionARCameraChannel::Tick(const float InDeltaTime)
{
	// Camera capture only works on iOS right now
#if PLATFORM_IOS
	if (Role == ERemoteSessionChannelMode::Write)
	{
		QueueARCameraImage();
		SendARCameraImage();
	}
#endif
	if (Role == ERemoteSessionChannelMode::Read)
	{
		UpdateRenderingTexture();
		if (MaterialInstanceDynamic != nullptr)
		{
			if (UTexture2D* NextTexture = RenderingTextures[RenderingTextureIndex.GetValue()])
			{
				// Update the texture to the current one
				MaterialInstanceDynamic->SetTextureParameterValue(CameraImageParamName, NextTexture);
			}
		}
	}
}

void FRemoteSessionARCameraChannel::QueueARCameraImage()
{
	check(IsInGameThread());

	if (!Connection.IsValid())
	{
		return;
	}

	UARTextureCameraImage* CameraImage = UARBlueprintLibrary::GetCameraImage();
	if (CameraImage != nullptr)
    {
        if (CameraImage->Timestamp > LastQueuedTimestamp)
        {
            TSharedPtr<FCompressionTask, ESPMode::ThreadSafe> CompressionTask = MakeShareable(new FCompressionTask());
            CompressionTask->Width = CameraImage->Size.X;
            CompressionTask->Height = CameraImage->Size.Y;
            CompressionTask->AsyncTask = IAppleImageUtilsPlugin::Get().ConvertToJPEG(CameraImage, CVarJPEGQuality.GetValueOnGameThread(), !!CVarJPEGColor.GetValueOnGameThread(), !!CVarJPEGGpu.GetValueOnGameThread());
            if (CompressionTask->AsyncTask.IsValid())
            {
                LastQueuedTimestamp = CameraImage->Timestamp;
                CompressionQueue.Add(CompressionTask);
            }
        }
    }
    else
    {
        UE_LOG(LogRemoteSession, Warning, TEXT("No AR Camera Image to send!"));
    }
}

void FRemoteSessionARCameraChannel::SendARCameraImage()
{
	check(IsInGameThread());

	if (!Connection.IsValid())
	{
		return;
	}

	TSharedPtr<FCompressionTask, ESPMode::ThreadSafe> CompressionTask;
	{
		int32 CompleteIndex = -1;
		bool bFound = false;
		for (int32 Index = 0; Index < CompressionQueue.Num() && bFound; Index++)
		{
			// Find the latest task that has completed
			if (CompressionQueue[Index]->AsyncTask->IsDone())
			{
				CompleteIndex = Index;
			}
			// If this task isn't done, but the one before it is, then we're done
			else if (CompleteIndex > -1 || Index == 0)
			{
				bFound = true;
			}
		}
		if (CompleteIndex > -1)
		{
			// Grab the latest completed one
			CompressionTask = CompressionQueue[CompleteIndex];
			// And clear out all of the tasks between 0 and CompleteIndex
			CompressionQueue.RemoveAt(0, CompleteIndex + 1);
		}
	}

	if (CompressionTask.IsValid() && !CompressionTask->AsyncTask->HadError())
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, CompressionTask]()
		{
			FBackChannelOSCMessage Msg(CAMERA_MESSAGE_ADDRESS);
			Msg.Write(CompressionTask->Width);
			Msg.Write(CompressionTask->Height);
			Msg.Write(CompressionTask->AsyncTask->GetData());

			Connection->SendPacket(Msg);
		});
	}
}

UMaterialInterface* FRemoteSessionARCameraChannel::GetPostProcessMaterial() const
{
	return PPMaterial;
}

void FRemoteSessionARCameraChannel::ReceiveARCameraImage(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	IImageWrapperModule* ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>(FName("ImageWrapper"));
	if (ImageWrapperModule == nullptr)
	{
		return;
	}
	if (DecompressionTaskCount.GetValue() > 0)
	{
		// Skip if decoding is in flight so we don't have to deal with queue ordering issues
		// This will make the last one in the DecompressionQueue always be the latest image
		return;
	}
	DecompressionTaskCount.Increment();

	TSharedPtr<FDecompressedImage, ESPMode::ThreadSafe> DecompressedImage = MakeShareable(new FDecompressedImage());
	Message << DecompressedImage->Width;
	Message << DecompressedImage->Height;
	Message << DecompressedImage->ImageData;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ImageWrapperModule, DecompressedImage]
	{
		// @todo joeg -- Make FRemoteSessionFrameBufferChannel and this share code where it makes sense
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);

		ImageWrapper->SetCompressed(DecompressedImage->ImageData.GetData(), DecompressedImage->ImageData.Num());

		const TArray<uint8>* RawData = nullptr;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
		{
			check(RawData != nullptr);
			DecompressedImage->ImageData = MoveTemp(*(TArray<uint8>*)RawData);
			{
				FScopeLock sl(&DecompressionQueueLock);
				DecompressionQueue.Add(DecompressedImage);
			}
		}
		DecompressionTaskCount.Decrement();
	});
}

void FRemoteSessionARCameraChannel::UpdateRenderingTexture()
{
	TSharedPtr<FDecompressedImage, ESPMode::ThreadSafe> DecompressedImage;
	{
		FScopeLock sl(&DecompressionQueueLock);
		if (DecompressionQueue.Num())
		{
			DecompressedImage = DecompressionQueue.Last();
			DecompressionQueue.Empty();
		}
	}

	if (DecompressedImage.IsValid())
	{
		int32 NextImage = RenderingTextureIndex.GetValue() == 0 ? 1 : 0;
		// The next texture is still being updated on the rendering thread
		if (RenderingTexturesUpdateCount[NextImage].GetValue() > 0)
		{
			return;
		}
		RenderingTexturesUpdateCount[NextImage].Increment();

		// Create a texture if the sizes changed or the texture hasn't been created yet
		if (RenderingTextures[NextImage] == nullptr || DecompressedImage->Width != RenderingTextures[NextImage]->GetSizeX() || DecompressedImage->Height != RenderingTextures[NextImage]->GetSizeY())
		{
			RenderingTextures[NextImage] = UTexture2D::CreateTransient(DecompressedImage->Width, DecompressedImage->Height);
			RenderingTextures[NextImage]->UpdateResource();
		}

		// Update it on the render thread. There shouldn't (...) be any harm in GT code using it from this point
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, DecompressedImage->Width, DecompressedImage->Height);
		TArray<uint8>* TextureData = new TArray<uint8>(MoveTemp(DecompressedImage->ImageData));

		RenderingTextures[NextImage]->UpdateTextureRegions(0, 1, Region, 4 * DecompressedImage->Width, 8, TextureData->GetData(), [this, NextImage](auto InTextureData, auto InRegions)
		{
			RenderingTextureIndex.Set(NextImage);
			RenderingTexturesUpdateCount[NextImage].Decrement();
			delete InTextureData;
			delete InRegions;
		});
	} //-V773
}

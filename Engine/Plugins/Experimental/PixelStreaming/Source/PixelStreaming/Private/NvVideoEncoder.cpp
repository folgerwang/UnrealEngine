// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NvVideoEncoder.h"

#include "Utils.h"
#include "ScreenRendering.h"
#include "PixelStreamingCommon.h"
#include "ShaderCore.h"
#include "RendererInterface.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "CommonRenderResources.h"

#if defined PLATFORM_WINDOWS
// Disable macro redefinition warning for compatibility with Windows SDK 8+
#	pragma warning(push)
#		pragma warning(disable : 4005)	// macro redefinition
#		include "Windows/AllowWindowsPlatformTypes.h"
#			include "NvEncoder/nvEncodeAPI.h"
#			include <d3d11.h>
#		include "Windows/HideWindowsPlatformTypes.h"
#		include "D3D11RHIPrivate.h"
#	pragma warning(pop)
#endif

DECLARE_CYCLE_STAT(TEXT("CopyBackBuffer"), STAT_NvEnc_CopyBackBuffer, STATGROUP_NvEnc);
DECLARE_CYCLE_STAT(TEXT("SendBackBufferToEncoder"), STAT_NvEnc_SendBackBufferToEncoder, STATGROUP_NvEnc);
DECLARE_CYCLE_STAT(TEXT("WaitForEncodeEvent"), STAT_NvEnc_WaitForEncodeEvent, STATGROUP_NvEnc);
DECLARE_CYCLE_STAT(TEXT("RetrieveEncodedFrame"), STAT_NvEnc_RetrieveEncodedFrame, STATGROUP_NvEnc);
DECLARE_CYCLE_STAT(TEXT("StreamEncodedFrame"), STAT_NvEnc_StreamEncodedFrame, STATGROUP_NvEnc);
DECLARE_DWORD_COUNTER_STAT(TEXT("AsyncMode"), STAT_NvEnc_AsyncMode, STATGROUP_NvEnc);

#define BITSTREAM_SIZE 1024 * 1024 * 2
#define NV_RESULT(NvFunction) NvFunction == NV_ENC_SUCCESS

#if defined PLATFORM_WINDOWS
#define CLOSE_EVENT_HANDLE(EventHandle) CloseHandle(EventHandle);
#else
#define CLOSE_EVENT_HANDLE(EventHandle) fclose((FILE*)EventHandle);
#endif

class FNvVideoEncoder::FNvVideoEncoderImpl
{
private:
	struct FInputFrame
	{
		void*					RegisteredResource;
		NV_ENC_INPUT_PTR		MappedResource;
		NV_ENC_BUFFER_FORMAT	BufferFormat;
	};

	struct FOutputFrame
	{
		NV_ENC_OUTPUT_PTR		BitstreamBuffer;
		HANDLE					EventHandle;
	};

	struct FFrame
	{
		FTexture2DRHIRef		ResolvedBackBuffer;
		FInputFrame				InputFrame;
		FOutputFrame			OutputFrame;
		TArray<uint8>			EncodedFrame;
		bool					bIdrFrame = false;
		uint64					FrameIdx = 0;

		// timestamps to measure encoding latency
		uint64					CaptureTimeStamp = 0;
		uint64					EncodeStartTimeStamp = 0;
		uint64					EncodeEndTimeStamp = 0;

		FThreadSafeBool bEncoding = false;
	};

	struct FRHITransferRenderTargetToNvEnc final : public FRHICommand<FRHITransferRenderTargetToNvEnc>
	{
		FNvVideoEncoder::FNvVideoEncoderImpl* Encoder;
		FFrame* Frame;

		FORCEINLINE_DEBUGGABLE FRHITransferRenderTargetToNvEnc(FNvVideoEncoder::FNvVideoEncoderImpl* InEncoder, FFrame* InFrame)
			: Encoder(InEncoder), Frame(InFrame)
		{}

		void Execute(FRHICommandListBase& CmdList)
		{
			Encoder->TransferRenderTargetToHWEncoder(*Frame);
		}
	};

public:
	FNvVideoEncoderImpl(void* DllHandle, const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, bool bEnableAsyncMode, const FEncodedFrameReadyCallback& InEncodedFrameReadyCallback);
	~FNvVideoEncoderImpl();

	void UpdateSettings(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer);
	void EncodeFrame(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, uint64 CaptureMs);
	void TransferRenderTargetToHWEncoder(FFrame& Frame);

	void PostRenderingThreadCreated()				{ bWaitForRenderThreadToResume = false; }
	void PreRenderingThreadDestroyed()				{ bWaitForRenderThreadToResume = true; }
	bool IsSupported() const						{ return bIsSupported; }
	bool IsAsyncEnabled() const						{ return NvEncInitializeParams.enableEncodeAsync > 0; }
	const TArray<uint8>& GetSpsPpsHeader() const	{ return SpsPpsHeader; }
	void ForceIdrFrame()							{ bForceIdrFrame = true; }

private:
	void InitFrameInputBuffer(const FTexture2DRHIRef& BackBuffer, FFrame& Frame);
	void InitializeResources(const FTexture2DRHIRef& BackBuffer);
	void ReleaseFrameInputBuffer(FFrame& Frame);
	void ReleaseResources();
	void RegisterAsyncEvent(void** OutEvent);
	void UnregisterAsyncEvent(void* Event);
	void EncoderCheckLoop();
	void ProcessFrame(FFrame& Frame);
	void CopyBackBuffer(const FTexture2DRHIRef& BackBuffer, const FTexture2DRHIRef& ResolvedBackBuffer);
	void UpdateSpsPpsHeader();

	TUniquePtr<NV_ENCODE_API_FUNCTION_LIST> NvEncodeAPI;
	void*									EncoderInterface;
	NV_ENC_INITIALIZE_PARAMS				NvEncInitializeParams;
	NV_ENC_CONFIG							NvEncConfig;
	bool									bIsSupported;
	TArray<uint8>							SpsPpsHeader;
	FThreadSafeBool							bWaitForRenderThreadToResume;
	FThreadSafeBool							bForceIdrFrame;
	// Used to make sure we don't have a race condition trying to access a deleted "this" captured
	// in the render command lambda sent to the render thread from EncoderCheckLoop
	static FThreadSafeCounter				ImplCounter;
	uint64									FrameCount;
	static const uint32						NumBufferedFrames = 3;
	FFrame									BufferedFrames[NumBufferedFrames];
	TUniquePtr<FThread>						EncoderThread;
	FThreadSafeBool							bExitEncoderThread;
	FEncodedFrameReadyCallback				EncodedFrameReadyCallback;
};

FThreadSafeCounter FNvVideoEncoder::FNvVideoEncoderImpl::ImplCounter(0);

/**
* Implementation class of NvEnc.
* Note bEnableAsyncMode flag is for debugging purpose, it should be set to true normally unless user wants to test in synchronous mode.
*/
FNvVideoEncoder::FNvVideoEncoderImpl::FNvVideoEncoderImpl(void* DllHandle, const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, bool bEnableAsyncMode, const FEncodedFrameReadyCallback& InEncodedFrameReadyCallback)
	: EncoderInterface(nullptr)
	, bIsSupported(false)
	, bWaitForRenderThreadToResume(false)
	, bForceIdrFrame(false)
	, FrameCount(0)
	, bExitEncoderThread(false)
	, EncodedFrameReadyCallback(InEncodedFrameReadyCallback)
{
	// Bind to the delegates that are triggered when render thread is created or destroyed, so the encoder thread can act accordingly.
	FCoreDelegates::PostRenderingThreadCreated.AddRaw(this, &FNvVideoEncoderImpl::PostRenderingThreadCreated);
	FCoreDelegates::PreRenderingThreadDestroyed.AddRaw(this, &FNvVideoEncoderImpl::PreRenderingThreadDestroyed);

	uint32 Width = Settings.Width;
	uint32 Height = Settings.Height;

	ID3D11Device* Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	checkf(Device != nullptr, TEXT("Cannot initialize NvEnc with invalid device"));
	checkf(Width > 0 && Height > 0, TEXT("Cannot initialize NvEnc with invalid width/height"));
	_NVENCSTATUS Result;
	bool bWebSocketStreaming = FParse::Param(FCommandLine::Get(), TEXT("WebSocketStreaming"));

	// Load NvEnc dll and create an NvEncode API instance
	{		
		// define a function pointer for creating an instance of nvEncodeAPI
		typedef NVENCSTATUS(NVENCAPI *NVENCAPIPROC)(NV_ENCODE_API_FUNCTION_LIST*);
		NVENCAPIPROC NvEncodeAPICreateInstanceFunc;

#if defined PLATFORM_WINDOWS
#	pragma warning(push)
#		pragma warning(disable: 4191) // https://stackoverflow.com/a/4215425/453271
		NvEncodeAPICreateInstanceFunc = (NVENCAPIPROC)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("NvEncodeAPICreateInstance"));
#	pragma warning(pop)
#else
		NvEncodeAPICreateInstanceFunc = (NVENCAPIPROC)dlsym(DllHandle, "NvEncodeAPICreateInstance");
#endif
		checkf(NvEncodeAPICreateInstanceFunc != nullptr, TEXT("NvEncodeAPICreateInstance failed"));
		NvEncodeAPI.Reset(new NV_ENCODE_API_FUNCTION_LIST);
		FMemory::Memzero(NvEncodeAPI.Get(), sizeof(NV_ENCODE_API_FUNCTION_LIST));
		NvEncodeAPI->version = NV_ENCODE_API_FUNCTION_LIST_VER;
		Result = NvEncodeAPICreateInstanceFunc(NvEncodeAPI.Get());
		checkf(NV_RESULT(Result), TEXT("Unable to create NvEnc API function list (status: %d)"), Result);
	}
	// Open an encoding session
	{
		NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS OpenEncodeSessionExParams;
		FMemory::Memzero(OpenEncodeSessionExParams);
		OpenEncodeSessionExParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
		OpenEncodeSessionExParams.device = Device;
		OpenEncodeSessionExParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;	// Currently only DX11 is supported
		OpenEncodeSessionExParams.apiVersion = NVENCAPI_VERSION;
		Result = NvEncodeAPI->nvEncOpenEncodeSessionEx(&OpenEncodeSessionExParams, &EncoderInterface);
		checkf(NV_RESULT(Result), TEXT("Unable to open NvEnc encoding session (status: %d)"), Result);
	}	
	// Set initialization parameters
	{
		FMemory::Memzero(NvEncInitializeParams);
		NvEncInitializeParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
		NvEncInitializeParams.encodeWidth = Width;
		NvEncInitializeParams.encodeHeight = Height;
		NvEncInitializeParams.darWidth = Width;
		NvEncInitializeParams.darHeight = Height;
		NvEncInitializeParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
		NvEncInitializeParams.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
		NvEncInitializeParams.frameRateNum = Settings.FrameRate;
		FParse::Value(FCommandLine::Get(), TEXT("NvEncFrameRateNum="), NvEncInitializeParams.frameRateNum);
		UE_LOG(PixelStreaming, Log, TEXT("NvEnc configured to %d FPS"), NvEncInitializeParams.frameRateNum);
		NvEncInitializeParams.frameRateDen = 1;
		NvEncInitializeParams.enablePTD = 1;
		NvEncInitializeParams.reportSliceOffsets = 0;
		NvEncInitializeParams.enableSubFrameWrite = 0;
		NvEncInitializeParams.encodeConfig = &NvEncConfig;
		NvEncInitializeParams.maxEncodeWidth = 3840;
		NvEncInitializeParams.maxEncodeHeight = 2160;
		FParse::Value(FCommandLine::Get(), TEXT("NvEncMaxEncodeWidth="), NvEncInitializeParams.maxEncodeWidth);
		FParse::Value(FCommandLine::Get(), TEXT("NvEncMaxEncodeHeight="), NvEncInitializeParams.maxEncodeHeight);
	}
	// Get preset config and tweak it accordingly
	{
		NV_ENC_PRESET_CONFIG PresetConfig;
		FMemory::Memzero(PresetConfig);
		PresetConfig.version = NV_ENC_PRESET_CONFIG_VER;
		PresetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
		Result = NvEncodeAPI->nvEncGetEncodePresetConfig(EncoderInterface, NvEncInitializeParams.encodeGUID, NvEncInitializeParams.presetGUID, &PresetConfig);
		checkf(NV_RESULT(Result), TEXT("Failed to select NVEncoder preset config (status: %d)"), Result);
		FMemory::Memcpy(&NvEncConfig, &PresetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
		
		NvEncConfig.profileGUID = NV_ENC_H264_PROFILE_BASELINE_GUID;
		//NvEncConfig.profileGUID = NV_ENC_H264_PROFILE_HIGH_GUID;
		//NvEncConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
		NvEncConfig.gopLength = NvEncInitializeParams.frameRateNum; // once a sec
		//NvEncConfig.frameIntervalP = 1;
		//NvEncConfig.frameFieldMode = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
		//NvEncConfig.mvPrecision = NV_ENC_MV_PRECISION_QUARTER_PEL;
		//NvEncConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
		//FString RateControlMode;
		//FParse::Value(FCommandLine::Get(), TEXT("NvEncRateControlMode="), RateControlMode);
		//if (RateControlMode == TEXT("NV_ENC_PARAMS_RC_VBR_HQ"))
		//{
		//	NvEncConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR_HQ;
		//}
		NvEncConfig.rcParams.averageBitRate = Settings.AverageBitRate;
		FParse::Value(FCommandLine::Get(), TEXT("NvEncAverageBitRate="), NvEncConfig.rcParams.averageBitRate);
		//NvEncConfig.encodeCodecConfig.h264Config.chromaFormatIDC = 1;
		NvEncConfig.encodeCodecConfig.h264Config.idrPeriod = NvEncConfig.gopLength;

		if (bWebSocketStreaming)
		{
			NvEncConfig.encodeCodecConfig.h264Config.sliceMode = 0;
			NvEncConfig.encodeCodecConfig.h264Config.sliceModeData = 0;
		}
		else
		{
			// configure "entire frame as a single slice"
			// seems WebRTC implementation doesn't work well with slicing, default mode 
			// (Mode=3/ModeData=4 - 4 slices per frame) produces (rarely) grey full screen or just top half of it. 
			// it also can be related with our handling of slices in proxy's FakeVideoEncoder
			NvEncConfig.encodeCodecConfig.h264Config.sliceMode = 0;
			NvEncConfig.encodeCodecConfig.h264Config.sliceModeData = 0;

			// let encoder slice encoded frame so they can fit into RTP packets
			// commented out because at some point it started to produce immediately visible visual artefacts
			// on clients
			//NvEncConfig.encodeCodecConfig.h264Config.sliceMode = 1;
			//NvEncConfig.encodeCodecConfig.h264Config.sliceModeData = 1100; // max bytes per slice

			// repeat SPS/PPS with each key-frame for a case when the first frame (with mandatory SPS/PPS) 
			// was dropped by WebRTC
			NvEncConfig.encodeCodecConfig.h264Config.repeatSPSPPS = 1;
		}

		// maybe doesn't have an effect, high level is chosen because we aim at high bitrate
		NvEncConfig.encodeCodecConfig.h264Config.level = NV_ENC_LEVEL_H264_51;			
		FString NvEncH264ConfigLevel;
		FParse::Value(FCommandLine::Get(), TEXT("NvEncH264ConfigLevel="), NvEncH264ConfigLevel);
		if (NvEncH264ConfigLevel == TEXT("NV_ENC_LEVEL_H264_52"))
		{
			NvEncConfig.encodeCodecConfig.h264Config.level = NV_ENC_LEVEL_H264_52;
		}
	}		
	// Get encoder capability
	{
		NV_ENC_CAPS_PARAM CapsParam;
		FMemory::Memzero(CapsParam);
		CapsParam.version = NV_ENC_CAPS_PARAM_VER;
		CapsParam.capsToQuery = NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT;
		int32 AsyncMode = 0;
		Result = NvEncodeAPI->nvEncGetEncodeCaps(EncoderInterface, NvEncInitializeParams.encodeGUID, &CapsParam, &AsyncMode);
		checkf(NV_RESULT(Result), TEXT("Failed to get NVEncoder capability params (status: %d)"), Result);
		NvEncInitializeParams.enableEncodeAsync = bEnableAsyncMode ? AsyncMode : 0;
	}
	
	Result = NvEncodeAPI->nvEncInitializeEncoder(EncoderInterface, &NvEncInitializeParams);
	checkf(NV_RESULT(Result), TEXT("Failed to initialize NVEncoder (status: %d)"), Result);

	UpdateSpsPpsHeader();

	InitializeResources(BackBuffer);

	if (NvEncInitializeParams.enableEncodeAsync)
	{
		EncoderThread.Reset(new FThread(TEXT("PixelStreaming Video Send"), [this]() { EncoderCheckLoop(); }));
	}

	bIsSupported = true;
}

FNvVideoEncoder::FNvVideoEncoderImpl::~FNvVideoEncoderImpl()
{
	FCoreDelegates::PostRenderingThreadCreated.RemoveAll(this);
	FCoreDelegates::PreRenderingThreadDestroyed.RemoveAll(this);

	if (EncoderThread)
	{
		// Reset bWaitForRenderThreadToResume so encoder thread can quit
		bWaitForRenderThreadToResume = false;

		bExitEncoderThread = true;
		// Trigger all frame events to release encoder thread waiting on them
		// (we don't know here which frame it's waiting for)
		for (FFrame& Frame : BufferedFrames)
		{
			SetEvent(Frame.OutputFrame.EventHandle);
		}
		// Exit encoder runnable thread before shutting down NvEnc interface
		EncoderThread->Join();
		// Increment the counter, so that if any pending render commands sent from EncoderCheckLoop 
		// to the Render Thread still reference "this", they will be ignored because the counter is different
		ImplCounter.Increment();
	}

	ReleaseResources();

	if (EncoderInterface)
	{
		_NVENCSTATUS Result = NvEncodeAPI->nvEncDestroyEncoder(EncoderInterface);
		checkf(NV_RESULT(Result), TEXT("Failed to destroy NvEnc interface (status: %d)"), Result);
		EncoderInterface = nullptr;
	}

	bIsSupported = false;
}

void FNvVideoEncoder::FNvVideoEncoderImpl::UpdateSpsPpsHeader()
{
	uint8 SpsPpsBuffer[NV_MAX_SEQ_HDR_LEN];
	uint32 PayloadSize = 0;

	NV_ENC_SEQUENCE_PARAM_PAYLOAD SequenceParamPayload;
	FMemory::Memzero(SequenceParamPayload);
	SequenceParamPayload.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;
	SequenceParamPayload.inBufferSize = NV_MAX_SEQ_HDR_LEN;
	SequenceParamPayload.spsppsBuffer = &SpsPpsBuffer;
	SequenceParamPayload.outSPSPPSPayloadSize = &PayloadSize;

	_NVENCSTATUS Result = NvEncodeAPI->nvEncGetSequenceParams(EncoderInterface, &SequenceParamPayload);
	checkf(NV_RESULT(Result), TEXT("Unable to get NvEnc sequence params (status: %d)"), Result);

	SpsPpsHeader.SetNum(PayloadSize);
	FMemory::Memcpy(SpsPpsHeader.GetData(), SpsPpsBuffer, PayloadSize);
}

void FNvVideoEncoder::FNvVideoEncoderImpl::UpdateSettings(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer)
{
	bool bSettingsChanged = false;
	bool bResolutionChanged = false;
	if (NvEncConfig.rcParams.averageBitRate != Settings.AverageBitRate)
	{
		NvEncConfig.rcParams.averageBitRate = Settings.AverageBitRate;
		bSettingsChanged = true;
	}
	if (NvEncInitializeParams.frameRateNum != Settings.FrameRate)
	{
		NvEncInitializeParams.frameRateNum = Settings.FrameRate;
		bSettingsChanged = true;
		UE_LOG(PixelStreaming, Log, TEXT("NvEnc reconfigured to %d FPS"), NvEncInitializeParams.frameRateNum);
	}
	if (NvEncInitializeParams.encodeWidth != Settings.Width)
	{
		NvEncInitializeParams.encodeWidth = Settings.Width;
		NvEncInitializeParams.darWidth = Settings.Width;
		bResolutionChanged = true;
		bSettingsChanged = true;
	}
	if (NvEncInitializeParams.encodeHeight != Settings.Height)
	{
		NvEncInitializeParams.encodeHeight = Settings.Height;
		NvEncInitializeParams.darHeight = Settings.Height;
		bResolutionChanged = true;
		bSettingsChanged = true;
	}

	if (bSettingsChanged)
	{
		NV_ENC_RECONFIGURE_PARAMS NvEncReconfigureParams;
		FMemory::Memzero(NvEncReconfigureParams);
		FMemory::Memcpy(&NvEncReconfigureParams.reInitEncodeParams, &NvEncInitializeParams, sizeof(NvEncInitializeParams));
		NvEncReconfigureParams.version = NV_ENC_RECONFIGURE_PARAMS_VER;
		NvEncReconfigureParams.forceIDR = bResolutionChanged;

		_NVENCSTATUS Result = NvEncodeAPI->nvEncReconfigureEncoder(EncoderInterface, &NvEncReconfigureParams);
		checkf(NV_RESULT(Result), TEXT("Failed to reconfigure encoder (status: %d)"), Result);
	}

	if (bResolutionChanged)
	{
		UpdateSpsPpsHeader();
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::CopyBackBuffer(const FTexture2DRHIRef& BackBuffer, const FTexture2DRHIRef& ResolvedBackBuffer)
{
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	if (BackBuffer->GetFormat() == ResolvedBackBuffer->GetFormat() &&
		BackBuffer->GetSizeXY() == ResolvedBackBuffer->GetSizeXY())
	{
		RHICmdList.CopyToResolveTarget(BackBuffer, ResolvedBackBuffer, FResolveParams());
	}
	else // Texture format mismatch, use a shader to do the copy.
	{
		// #todo-renderpasses there's no explicit resolve here? Do we need one?
		FRHIRenderPassInfo RPInfo(ResolvedBackBuffer, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyBackbuffer"));
		{
			RHICmdList.SetViewport(0, 0, 0.0f, ResolvedBackBuffer->GetSizeX(), ResolvedBackBuffer->GetSizeY(), 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
			TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			if (ResolvedBackBuffer->GetSizeX() != BackBuffer->GetSizeX() || ResolvedBackBuffer->GetSizeY() != BackBuffer->GetSizeY())
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), BackBuffer);
			else
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), BackBuffer);

			RendererModule->DrawRectangle(
				RHICmdList,
				0, 0,									// Dest X, Y
				ResolvedBackBuffer->GetSizeX(),			// Dest Width
				ResolvedBackBuffer->GetSizeY(),			// Dest Height
				0, 0,									// Source U, V
				1, 1,									// Source USize, VSize
				ResolvedBackBuffer->GetSizeXY(),		// Target buffer size
				FIntPoint(1, 1),						// Source texture size
				*VertexShader,
				EDRF_Default);
		}
		RHICmdList.EndRenderPass();
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::EncoderCheckLoop()
{
	int CurrentIndex = 0;
	while (!bExitEncoderThread)
	{
		FFrame& Frame = BufferedFrames[CurrentIndex];

		{
			SCOPE_CYCLE_COUNTER(STAT_NvEnc_WaitForEncodeEvent);
			DWORD Result = WaitForSingleObject(Frame.OutputFrame.EventHandle, INFINITE);
			checkf(Result == WAIT_OBJECT_0, TEXT("Error waiting for frame event: %d"), Result);
			if (bExitEncoderThread)
			{
				return;
			}
		}

		Frame.EncodeEndTimeStamp = NowMs();

		ResetEvent(Frame.OutputFrame.EventHandle);
		int32 CurrImplCounter = ImplCounter.GetValue();
		// When resolution changes, render thread is stopped and later restarted from game thread.
		// We can't enqueue render commands when render thread is stopped, so pause until render thread is restarted.
		while (bWaitForRenderThreadToResume) {}
		FNvVideoEncoderImpl* This = this;
		FFrame* InFrame = &Frame;
		ENQUEUE_RENDER_COMMAND(NvEncProcessFrame)(
			[This, InFrame, CurrImplCounter](FRHICommandListImmediate& RHICmdList)
			{
				if (CurrImplCounter != ImplCounter.GetValue()) // Check if the "this" we captured is still valid
				{
					return;
				}
	
				This->ProcessFrame(*InFrame);
			}
		);

		CurrentIndex = (CurrentIndex + 1) % NumBufferedFrames;
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::EncodeFrame(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, uint64 CaptureMs)
{
	SET_DWORD_STAT(STAT_NvEnc_AsyncMode, NvEncInitializeParams.enableEncodeAsync ? 1 : 0);

	UpdateSettings(Settings, BackBuffer);

	uint32 BufferIndexToWrite = FrameCount % NumBufferedFrames;
	FFrame& Frame = BufferedFrames[BufferIndexToWrite];

	// If we don't have any free buffers, then we skip this rendered frame
	if (Frame.bEncoding)
	{
		return;
	}

	// When resolution changes, buffers need to be recreated
	if (Frame.ResolvedBackBuffer->GetSizeX() != Settings.Width || Frame.ResolvedBackBuffer->GetSizeY() != Settings.Height)
	{
		ReleaseFrameInputBuffer(Frame);
		InitFrameInputBuffer(BackBuffer, Frame);
	}

	Frame.bEncoding = true;
	Frame.FrameIdx = FrameCount;
	Frame.CaptureTimeStamp = CaptureMs;

	// Copy BackBuffer to ResolvedBackBuffer
	{
		SCOPE_CYCLE_COUNTER(STAT_NvEnc_CopyBackBuffer);
		CopyBackBuffer(BackBuffer, Frame.ResolvedBackBuffer);
	}

	// Encode frame
	{
		FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (RHICmdList.Bypass())
		{
			FRHITransferRenderTargetToNvEnc Command(this, &Frame);
			Command.Execute(RHICmdList);
		}
		else
		{
			ALLOC_COMMAND_CL(RHICmdList, FRHITransferRenderTargetToNvEnc)(this, &Frame);
		}
	}

	FrameCount++;
}

void FNvVideoEncoder::FNvVideoEncoderImpl::TransferRenderTargetToHWEncoder(FFrame& Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_NvEnc_SendBackBufferToEncoder);

	NV_ENC_PIC_PARAMS PicParams;
	FMemory::Memzero(PicParams);
	PicParams.version = NV_ENC_PIC_PARAMS_VER;
	PicParams.inputBuffer = Frame.InputFrame.MappedResource;
	PicParams.bufferFmt = Frame.InputFrame.BufferFormat;
	PicParams.inputWidth = NvEncInitializeParams.encodeWidth;
	PicParams.inputHeight = NvEncInitializeParams.encodeHeight;
	PicParams.outputBitstream = Frame.OutputFrame.BitstreamBuffer;
	PicParams.completionEvent = Frame.OutputFrame.EventHandle;
	PicParams.inputTimeStamp = Frame.FrameIdx;
	PicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

	if (bForceIdrFrame)
	{
		PicParams.encodePicFlags |= NV_ENC_PIC_FLAG_FORCEIDR;
	}
	bForceIdrFrame = false;

	Frame.EncodeStartTimeStamp = NowMs();
	_NVENCSTATUS Result = NvEncodeAPI->nvEncEncodePicture(EncoderInterface, &PicParams);
	checkf(NV_RESULT(Result), TEXT("Failed to encode frame (status: %d)"), Result);

	if (!NvEncInitializeParams.enableEncodeAsync)
	{
		// In synchronous mode, simply process the frame immediately.
		ProcessFrame(Frame);
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::ProcessFrame(FFrame& Frame)
{
	// If the expected frame hasn't been doing encoding, then nothing to do
	checkf(Frame.bEncoding, TEXT("This should not happen"));
	if (!Frame.bEncoding)
	{
		return;
	}

	// log encoding latency for every 1000th frame
	if (Frame.FrameIdx % 1000 == 0)
	{
		uint64 ms = NowMs();
		UE_LOG(PixelStreaming, Log, TEXT("#%d %d %d %d"), Frame.FrameIdx, Frame.EncodeStartTimeStamp - Frame.CaptureTimeStamp, Frame.EncodeEndTimeStamp - Frame.EncodeStartTimeStamp, ms - Frame.EncodeEndTimeStamp);
	}

	Frame.bEncoding = false;

	// Retrieve encoded frame from output buffer
	{
		SCOPE_CYCLE_COUNTER(STAT_NvEnc_RetrieveEncodedFrame);

		NV_ENC_LOCK_BITSTREAM LockBitstream;
		FMemory::Memzero(LockBitstream);
		LockBitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
		LockBitstream.outputBitstream = Frame.OutputFrame.BitstreamBuffer;
		LockBitstream.doNotWait = NvEncInitializeParams.enableEncodeAsync;

		_NVENCSTATUS Result = NvEncodeAPI->nvEncLockBitstream(EncoderInterface, &LockBitstream);
		checkf(NV_RESULT(Result), TEXT("Failed to lock bitstream (status: %d)"), Result);

		Frame.EncodedFrame.SetNum(LockBitstream.bitstreamSizeInBytes);
		FMemory::Memcpy(Frame.EncodedFrame.GetData(), LockBitstream.bitstreamBufferPtr, LockBitstream.bitstreamSizeInBytes);

		Result = NvEncodeAPI->nvEncUnlockBitstream(EncoderInterface, Frame.OutputFrame.BitstreamBuffer);
		checkf(NV_RESULT(Result), TEXT("Failed to unlock bitstream (status: %d)"), Result);
		Frame.bIdrFrame = LockBitstream.pictureType == NV_ENC_PIC_TYPE_IDR;
	}

	// Stream the encoded frame
	{
		SCOPE_CYCLE_COUNTER(STAT_NvEnc_StreamEncodedFrame);
		EncodedFrameReadyCallback(Frame.CaptureTimeStamp, Frame.bIdrFrame, Frame.EncodedFrame.GetData(), Frame.EncodedFrame.Num());
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::InitFrameInputBuffer(const FTexture2DRHIRef& BackBuffer, FFrame& Frame)
{
	// Create resolved back buffer texture
	{
		// Make sure format used here is compatible with NV_ENC_BUFFER_FORMAT specified later in NV_ENC_REGISTER_RESOURCE bufferFormat
		FRHIResourceCreateInfo CreateInfo;
		Frame.ResolvedBackBuffer = RHICreateTexture2D(NvEncInitializeParams.encodeWidth, NvEncInitializeParams.encodeHeight, EPixelFormat::PF_A2B10G10R10, 1, 1, TexCreate_RenderTargetable, CreateInfo);
	}

	FMemory::Memzero(Frame.InputFrame);
	// Register input back buffer
	{
		ID3D11Texture2D* ResolvedBackBufferDX11 = (ID3D11Texture2D*)(GetD3D11TextureFromRHITexture(Frame.ResolvedBackBuffer)->GetResource());
		EPixelFormat PixelFormat = Frame.ResolvedBackBuffer->GetFormat();

		NV_ENC_REGISTER_RESOURCE RegisterResource;
		FMemory::Memzero(RegisterResource);
		RegisterResource.version = NV_ENC_REGISTER_RESOURCE_VER;
		RegisterResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
		RegisterResource.resourceToRegister = (void*)ResolvedBackBufferDX11;
		RegisterResource.width = NvEncInitializeParams.encodeWidth;
		RegisterResource.height = NvEncInitializeParams.encodeHeight;
		RegisterResource.bufferFormat = NV_ENC_BUFFER_FORMAT_ABGR10;	// Make sure ResolvedBackBuffer is created with a compatible format
		_NVENCSTATUS Result = NvEncodeAPI->nvEncRegisterResource(EncoderInterface, &RegisterResource);
		checkf(NV_RESULT(Result), TEXT("Failed to register input back buffer (status: %d)"), Result);

		Frame.InputFrame.RegisteredResource = RegisterResource.registeredResource;
		Frame.InputFrame.BufferFormat = RegisterResource.bufferFormat;
	}
	// Map input buffer resource
	{
		NV_ENC_MAP_INPUT_RESOURCE MapInputResource;
		FMemory::Memzero(MapInputResource);
		MapInputResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
		MapInputResource.registeredResource = Frame.InputFrame.RegisteredResource;
		_NVENCSTATUS Result = NvEncodeAPI->nvEncMapInputResource(EncoderInterface, &MapInputResource);
		checkf(NV_RESULT(Result), TEXT("Failed to map NvEnc input resource (status: %d)"), Result);
		Frame.InputFrame.MappedResource = MapInputResource.mappedResource;
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::InitializeResources(const FTexture2DRHIRef& BackBuffer)
{
	for (uint32 i = 0; i < NumBufferedFrames; ++i)
	{
		FFrame& Frame = BufferedFrames[i];
		
		InitFrameInputBuffer(BackBuffer, Frame);

		FMemory::Memzero(Frame.OutputFrame);
		// Create output bitstream buffer
		{
			NV_ENC_CREATE_BITSTREAM_BUFFER CreateBitstreamBuffer;
			FMemory::Memzero(CreateBitstreamBuffer);
			CreateBitstreamBuffer.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
			CreateBitstreamBuffer.size = BITSTREAM_SIZE;
			CreateBitstreamBuffer.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
			_NVENCSTATUS Result = NvEncodeAPI->nvEncCreateBitstreamBuffer(EncoderInterface, &CreateBitstreamBuffer);
			checkf(NV_RESULT(Result), TEXT("Failed to create NvEnc bitstream buffer (status: %d)"), Result);
			Frame.OutputFrame.BitstreamBuffer = CreateBitstreamBuffer.bitstreamBuffer;
		}
		// Register event handles
		if (NvEncInitializeParams.enableEncodeAsync)
		{
			RegisterAsyncEvent(&Frame.OutputFrame.EventHandle);
		}
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::ReleaseFrameInputBuffer(FFrame& Frame)
{
	_NVENCSTATUS Result = NvEncodeAPI->nvEncUnmapInputResource(EncoderInterface, Frame.InputFrame.MappedResource);
	checkf(NV_RESULT(Result), TEXT("Failed to unmap input resource (status: %d)"), Result);
	Frame.InputFrame.MappedResource = nullptr;

	Result = NvEncodeAPI->nvEncUnregisterResource(EncoderInterface, Frame.InputFrame.RegisteredResource);
	checkf(NV_RESULT(Result), TEXT("Failed to unregister input buffer resource (status: %d)"), Result);
	Frame.InputFrame.RegisteredResource = nullptr;

	Frame.ResolvedBackBuffer.SafeRelease();
}

void FNvVideoEncoder::FNvVideoEncoderImpl::ReleaseResources()
{
	for (uint32 i = 0; i < NumBufferedFrames; ++i)
	{
		FFrame& Frame = BufferedFrames[i];

		ReleaseFrameInputBuffer(Frame);

		_NVENCSTATUS Result = NvEncodeAPI->nvEncDestroyBitstreamBuffer(EncoderInterface, Frame.OutputFrame.BitstreamBuffer);
		checkf(NV_RESULT(Result), TEXT("Failed to destroy output buffer bitstream (status: %d)"), Result);
		Frame.OutputFrame.BitstreamBuffer = nullptr;

		if (Frame.OutputFrame.EventHandle)
		{
			UnregisterAsyncEvent(Frame.OutputFrame.EventHandle);
			CLOSE_EVENT_HANDLE(Frame.OutputFrame.EventHandle);
			Frame.OutputFrame.EventHandle = nullptr;
		}
	}
}

void FNvVideoEncoder::FNvVideoEncoderImpl::RegisterAsyncEvent(void** OutEvent)
{
	NV_ENC_EVENT_PARAMS EventParams;
	FMemory::Memzero(EventParams);
	EventParams.version = NV_ENC_EVENT_PARAMS_VER;
#if defined PLATFORM_WINDOWS
	EventParams.completionEvent = CreateEvent(nullptr, false, false, nullptr);
#endif
	_NVENCSTATUS Result = NvEncodeAPI->nvEncRegisterAsyncEvent(EncoderInterface, &EventParams);
	checkf(NV_RESULT(Result), TEXT("Failed to register async event (status: %d)"), Result);
	*OutEvent = EventParams.completionEvent;
}

void FNvVideoEncoder::FNvVideoEncoderImpl::UnregisterAsyncEvent(void* Event)
{
	if (Event)
	{
		NV_ENC_EVENT_PARAMS EventParams;
		FMemory::Memzero(EventParams);
		EventParams.version = NV_ENC_EVENT_PARAMS_VER;
		EventParams.completionEvent = Event;
		_NVENCSTATUS Result = NvEncodeAPI->nvEncUnregisterAsyncEvent(EncoderInterface, &EventParams);
		checkf(NV_RESULT(Result), TEXT("Failed to unregister async event (status: %d)"), Result);
	}
}


FNvVideoEncoder::FNvVideoEncoder(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, const FEncodedFrameReadyCallback& InEncodedFrameReadyCallback)
	: NvVideoEncoderImpl(nullptr), DllHandle(nullptr)
{
#if defined PLATFORM_WINDOWS
#if defined _WIN64
	DllHandle = FPlatformProcess::GetDllHandle(TEXT("nvEncodeAPI64.dll"));
#else
	DllHandle = FPlatformProcess::GetDllHandle(TEXT("nvEncodeAPI.dll"));
#endif
#else
	DllHandle = FPlatformProcess::GetDllHandle(TEXT("libnvidia-encode.so.1"));
#endif
	checkf(DllHandle != nullptr, TEXT("Failed to load NvEncode dll"));

	if (DllHandle)
	{
		NvVideoEncoderImpl = new FNvVideoEncoderImpl(DllHandle, Settings, BackBuffer, true, InEncodedFrameReadyCallback);
	}
}

FNvVideoEncoder::~FNvVideoEncoder()
{
	if (DllHandle)
	{
		delete NvVideoEncoderImpl;

#if defined PLATFORM_WINDOWS
		FPlatformProcess::FreeDllHandle(DllHandle);
#else
		dlclose(DllHandle);
#endif
		DllHandle = nullptr;
	}
}

bool FNvVideoEncoder::IsSupported() const
{
	return DllHandle && NvVideoEncoderImpl->IsSupported();
}

bool FNvVideoEncoder::IsAsyncEnabled() const
{
	return NvVideoEncoderImpl->IsAsyncEnabled();
}

void FNvVideoEncoder::EncodeFrame(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, uint64 CaptureMs)
{
	NvVideoEncoderImpl->EncodeFrame(Settings, BackBuffer, CaptureMs);
}

const TArray<uint8>& FNvVideoEncoder::GetSpsPpsHeader() const
{
	return NvVideoEncoderImpl->GetSpsPpsHeader();
}

void FNvVideoEncoder::ForceIdrFrame()
{
	NvVideoEncoderImpl->ForceIdrFrame();
}

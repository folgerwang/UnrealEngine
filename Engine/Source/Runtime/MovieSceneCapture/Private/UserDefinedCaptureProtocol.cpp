// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Protocols/UserDefinedCaptureProtocol.h"
#include "ImageWriteQueue.h"
#include "ImageWriteBlueprintLibrary.h"
#include "Modules/ModuleManager.h"
#include "Async/Async.h"
#include "Engine/Texture.h"
#include "UnrealClient.h"
#include "MovieSceneCaptureSettings.h"
#include "Slate/SceneViewport.h"
#include "ImagePixelData.h"

/** Callable utility struct that calls a handler with the specified parameters on the game thread */
struct FCallPixelHandler_GameThread
{
	static void Dispatch(FName InStreamName, const FFrameMetrics& InFrameMetrics, const FCapturedPixels& InPixels, TWeakObjectPtr<UUserDefinedCaptureProtocol> InWeakProtocol, const FOnReceiveCapturedPixels& InHandler)
	{
		FCallPixelHandler_GameThread Functor;
		Functor.Pixels           = InPixels;
		Functor.Handler          = InHandler;
		Functor.FrameMetrics     = InFrameMetrics;
		Functor.StreamName       = InStreamName;
		Functor.WeakProtocol     = InWeakProtocol;

		if (!IsInGameThread())
		{
			AsyncTask(ENamedThreads::GameThread, MoveTemp(Functor));
		}
		else
		{
			Functor();
		}
	}

	void operator()()
	{
		check(IsInGameThread());

		UUserDefinedCaptureProtocol* Protocol = WeakProtocol.Get();
		if (Protocol)
		{
			Protocol->OnBufferReady();
		}

		if (Pixels.ImageData->IsDataWellFormed() && Handler.IsBound())
		{
			Handler.Execute(MoveTemp(Pixels), StreamName, FrameMetrics);
		}
	}

private:

	/** The captured pixels themselves */
	FCapturedPixels Pixels;
	/** The name of the stream that these pixels represent */
	FName StreamName;
	/** Metrics for the frame from which the pixel data is derived */
	FFrameMetrics FrameMetrics;
	/** A handler to call with the pixels */
	FOnReceiveCapturedPixels Handler;
	/** Weak pointer back to the protocol. Only used to invoke OnBufferReady. */
	TWeakObjectPtr<UUserDefinedCaptureProtocol> WeakProtocol;
};

struct FCaptureProtocolBufferEndpoint : FImageStreamEndpoint
{
	FCaptureProtocolBufferEndpoint(UUserDefinedCaptureProtocol* CaptureProtocol, FName InStreamName, const FOnReceiveCapturedPixels& InHandler)
		: WeakProtocol(CaptureProtocol)
		, Handler(InHandler)
		, StreamName(InStreamName)
	{}

	void PushMetrics(const FFrameMetrics& InMetrics)
	{
		FScopeLock Lock(&IncomingMetricsMutex);
		IncomingMetrics.Add(InMetrics);
	}

private:

	// Called on the thread in which the data is created (possibly the render thread)
	virtual void OnImageReceived(TUniquePtr<FImagePixelData>&& InOwnedImage) override
	{
		FFrameMetrics   ThisFrameMetrics = PopMetrics();
		FCapturedPixels PixelData        = { MakeShareable(InOwnedImage.Release()) };

		FCallPixelHandler_GameThread::Dispatch(StreamName, ThisFrameMetrics, PixelData, WeakProtocol, Handler);
	}

	FFrameMetrics PopMetrics()
	{
		FFrameMetrics Popped;

		FScopeLock Lock(&IncomingMetricsMutex);
		if (ensure(IncomingMetrics.Num()))
		{
			Popped = IncomingMetrics[0];
			IncomingMetrics.RemoveAt(0, 1, false);
		}

		return Popped;
	}

private:

	TWeakObjectPtr<UUserDefinedCaptureProtocol> WeakProtocol;
	FOnReceiveCapturedPixels Handler;
	FName StreamName;

	FCriticalSection IncomingMetricsMutex;
	TArray<FFrameMetrics> IncomingMetrics;
};

UUserDefinedCaptureProtocol::UUserDefinedCaptureProtocol(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, World(nullptr)
	, NumOutstandingOperations(0)
{
}

void UUserDefinedCaptureProtocol::PreTickImpl()
{
	OnPreTick();
}

void UUserDefinedCaptureProtocol::TickImpl()
{
	OnTick();

	TSharedPtr<FImagePixelPipe, ESPMode::ThreadSafe> FinalPixelsPipe = StreamPipes.FindRef(FinalPixelsStreamName);

	// Process any frames that have been captured from the frame grabber
	if (FinalPixelsPipe.IsValid() && FinalPixelsFrameGrabber.IsValid())
	{
		TArray<FCapturedFrameData> CapturedFrames = FinalPixelsFrameGrabber->GetCapturedFrames();

		for (FCapturedFrameData& Frame : CapturedFrames)
		{
			FinalPixelsPipe->Push(MakeUnique<TImagePixelData<FColor>>(Frame.BufferSize, MoveTemp(Frame.ColorBuffer)));
		}
	}
}

bool UUserDefinedCaptureProtocol::SetupImpl()
{
	if (FViewportClient* Client = InitSettings->SceneViewport->GetClient())
	{
		World = Client->GetWorld();
	}
	else
	{
		World = nullptr;
	}

	// Preemptively create the frame grabber for final pixels, but do not start capturing final pixels until instructed
	FinalPixelsFrameGrabber.Reset(new FFrameGrabber(InitSettings->SceneViewport.ToSharedRef(), InitSettings->DesiredSize, PF_B8G8R8A8, 3));
	return OnSetup();
}

void UUserDefinedCaptureProtocol::WarmUpImpl()
{
	OnWarmUp();
}

bool UUserDefinedCaptureProtocol::StartCaptureImpl()
{
	OnStartCapture();
	return true;
}

void UUserDefinedCaptureProtocol::BindToStream(FName StreamName, FOnReceiveCapturedPixels Handler)
{
	if (Handler.IsBound())
	{
		TSharedPtr<FImagePixelPipe, ESPMode::ThreadSafe> Pipe = StreamPipes.FindRef(StreamName);
		if (!Pipe.IsValid())
		{
			Pipe = MakeShared<FImagePixelPipe, ESPMode::ThreadSafe>();
			StreamPipes.Add(StreamName, Pipe);
		}

		Pipe->AddEndpoint(MakeUnique<FCaptureProtocolBufferEndpoint>(this, StreamName, Handler));
	}
}

void UUserDefinedCaptureProtocol::StartCapturingFinalPixels(FName StreamName)
{
	if (FinalPixelsFrameGrabber.IsValid() && GetState() == EMovieSceneCaptureProtocolState::Capturing && !FinalPixelsFrameGrabber->IsCapturingFrames())
	{
		FinalPixelsStreamName = StreamName;
		FinalPixelsFrameGrabber->StartCapturingFrames();
	}
}

void UUserDefinedCaptureProtocol::StopCapturingFinalPixels()
{
	if (FinalPixelsFrameGrabber.IsValid() && GetState() == EMovieSceneCaptureProtocolState::Capturing && FinalPixelsFrameGrabber->IsCapturingFrames())
	{
		FinalPixelsFrameGrabber->StopCapturingFrames();
	}
}

void UUserDefinedCaptureProtocol::BeginFinalizeImpl()
{
	StopCapturingFinalPixels();

	OnBeginFinalize();
}

bool UUserDefinedCaptureProtocol::HasFinishedProcessingImpl() const
{
	if (NumOutstandingOperations != 0)
	{
		return false;
	}

	// If the frame grabber is still processing, we still have work to do
	if (FinalPixelsFrameGrabber.IsValid() && FinalPixelsFrameGrabber->HasOutstandingFrames())
	{
		return false;
	}

	return OnCanFinalize();
}

void UUserDefinedCaptureProtocol::FinalizeImpl()
{
	if (FinalPixelsFrameGrabber.IsValid())
	{
		FinalPixelsFrameGrabber->Shutdown();
		FinalPixelsFrameGrabber.Reset();
	}

	OnFinalize();
}

void UUserDefinedCaptureProtocol::CaptureFrameImpl(const FFrameMetrics& InFrameMetrics)
{
	CachedFrameMetrics = InFrameMetrics;

	if (FinalPixelsFrameGrabber.IsValid() && FinalPixelsFrameGrabber->IsCapturingFrames())
	{
		TSharedPtr<FImagePixelPipe, ESPMode::ThreadSafe> Pipe = StreamPipes.FindRef(FinalPixelsStreamName);
		if (Pipe.IsValid())
		{
			// Add the incoming metrics for this buffer to all the pipes that are streaming it
			for (const TUniquePtr<FImageStreamEndpoint>& EndPoint : Pipe->GetEndPoints())
			{
				static_cast<FCaptureProtocolBufferEndpoint*>(EndPoint.Get())->PushMetrics(CachedFrameMetrics);
			}

			++NumOutstandingOperations;
			FinalPixelsFrameGrabber->CaptureThisFrame(nullptr);
		}
	}

	OnCaptureFrame();
}

void UUserDefinedCaptureProtocol::PushBufferToStream(UTexture* Buffer, FName StreamName)
{
	if (!IsCapturing())
	{
		FFrame::KismetExecutionMessage(TEXT("Capture protocol is not currently capturing frames."), ELogVerbosity::Error);
		return;
	}

	// Find the pipe that we want to push this buffer onto
	TSharedPtr<FImagePixelPipe, ESPMode::ThreadSafe> Pipe = StreamPipes.FindRef(StreamName);
	if (Pipe.IsValid())
	{
		// Add the incoming metrics for this buffer to all the pipes that are streaming it
		for (const TUniquePtr<FImageStreamEndpoint>& EndPoint : Pipe->GetEndPoints())
		{
			static_cast<FCaptureProtocolBufferEndpoint*>(EndPoint.Get())->PushMetrics(CachedFrameMetrics);
		}

		auto OnPixelsReady = [Pipe](TUniquePtr<FImagePixelData>&& PixelData)
		{
			Pipe->Push(MoveTemp(PixelData));
		};

		// Resolve the texture data
		if (UImageWriteBlueprintLibrary::ResolvePixelData(Buffer, OnPixelsReady))
		{
			++NumOutstandingOperations;
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Format(TEXT("No handlers have been added for stream '{0}' - PushBufferToStream without first calling BindToStream is ineffectual."), TArray<FStringFormatArg>{ StreamName.ToString() }), ELogVerbosity::Warning);
	}
}

void UUserDefinedCaptureProtocol::ResolveBuffer(UTexture* Buffer, FName StreamName, FOnReceiveCapturedPixels Handler)
{
	if (!IsCapturing())
	{
		FFrame::KismetExecutionMessage(TEXT("Capture protocol is not currently capturing frames."), ELogVerbosity::Error);
		return;
	}

	if (!Handler.IsBound())
	{
		FFrame::KismetExecutionMessage(*FString::Format(TEXT("The specified handler for stream '{0}' is not bound. ResolveBuffer BindToStream is ineffectual."), TArray<FStringFormatArg>{ StreamName.ToString() }), ELogVerbosity::Warning);
		return;
	}

	TWeakObjectPtr<UUserDefinedCaptureProtocol> WeakProtocol = MakeWeakObjectPtr(this);
	FFrameMetrics FrameMetrics = CachedFrameMetrics;

	// Capture the current state by-value into the lambda so it can be correctly processed by the thread that resolves the pixels
	auto OnPixelsReady = [StreamName, FrameMetrics, Handler, WeakProtocol](TUniquePtr<FImagePixelData>&& PixelData)
	{
		FCapturedPixels CapturedPixels = { MakeShareable(PixelData.Release()) };
		FCallPixelHandler_GameThread::Dispatch(StreamName, FrameMetrics, CapturedPixels, WeakProtocol, Handler);
	};

	// Resolve the texture data
	if (UImageWriteBlueprintLibrary::ResolvePixelData(Buffer, OnPixelsReady))
	{
		++NumOutstandingOperations;
	}
}


void UUserDefinedCaptureProtocol::OnBufferReady()
{
	--NumOutstandingOperations;
}

FString UUserDefinedCaptureProtocol::GenerateFilename(const FFrameMetrics& InFrameMetrics) const
{
	if (!CaptureHost)
	{
		FFrame::KismetExecutionMessage(TEXT("Capture protocol is not currently set up to generate filenames."), ELogVerbosity::Error);
		return FString();
	}

	FString Filename = Super::GenerateFilenameImpl(InFrameMetrics, TEXT(""));
	EnsureFileWritableImpl(Filename);
	return Filename;
}

void UUserDefinedCaptureProtocol::AddFormatMappingsImpl(TMap<FString, FStringFormatArg>& FormatMappings) const
{
	FormatMappings.Add(TEXT("BufferName"), CurrentStreamName.ToString());
	FormatMappings.Add(TEXT("StreamName"), CurrentStreamName.ToString());
}

UUserDefinedImageCaptureProtocol::UUserDefinedImageCaptureProtocol(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, Format(EDesiredImageFormat::EXR)
	, bEnableCompression(false)
	, CompressionQuality(100)
{}

void UUserDefinedImageCaptureProtocol::OnReleaseConfigImpl(FMovieSceneCaptureSettings& InSettings)
{
	// Remove _{BufferName} if it exists
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT("_{BufferName}"), TEXT(""));

	// Remove .{frame} if it exists
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT(".{frame}"), TEXT(""));
}

void UUserDefinedImageCaptureProtocol::OnLoadConfigImpl(FMovieSceneCaptureSettings& InSettings)
{
	FString OutputFormat = InSettings.OutputFormat;

	if (!OutputFormat.Contains(TEXT("{BufferName}")))
	{
		OutputFormat.Append(TEXT("_{BufferName}"));

		InSettings.OutputFormat = OutputFormat;
	}

	if (!OutputFormat.Contains(TEXT("{frame}")))
	{
		OutputFormat.Append(TEXT(".{frame}"));

		InSettings.OutputFormat = OutputFormat;
	}
}

FString UUserDefinedImageCaptureProtocol::GenerateFilename(const FFrameMetrics& InFrameMetrics) const
{
	if (!CaptureHost)
	{
		FFrame::KismetExecutionMessage(TEXT("Capture protocol is not currently set up to generate filenames."), ELogVerbosity::Error);
		return FString();
	}

	const TCHAR* Extension = TEXT("");
	switch (Format)
	{
	case EDesiredImageFormat::BMP: Extension = TEXT(".bmp"); break;
	case EDesiredImageFormat::PNG: Extension = TEXT(".png"); break;
	case EDesiredImageFormat::JPG: Extension = TEXT(".jpg"); break;

	case EDesiredImageFormat::EXR:
	default:
		Extension = TEXT(".exr");
		break;
	}

	FString Filename = GenerateFilenameImpl(InFrameMetrics, Extension);
	EnsureFileWritableImpl(Filename);
	return Filename;
}

FString UUserDefinedImageCaptureProtocol::GenerateFilenameForCurrentFrame()
{
	return GenerateFilename(CachedFrameMetrics);
}

FString UUserDefinedImageCaptureProtocol::GenerateFilenameForBuffer(UTexture* Buffer, FName StreamName)
{
	if (!CaptureHost)
	{
		FFrame::KismetExecutionMessage(TEXT("Capture protocol is not currently set up to generate filenames."), ELogVerbosity::Error);
		return FString();
	}

	const TCHAR* Extension = TEXT(".ext");
	switch (Format)
	{
	case EDesiredImageFormat::EXR: Extension = TEXT(".exr"); break;
	case EDesiredImageFormat::BMP: Extension = TEXT(".bmp"); break;
	case EDesiredImageFormat::PNG: Extension = TEXT(".png"); break;
	case EDesiredImageFormat::JPG: Extension = TEXT(".jpg"); break;
	default: break;
	}

	CurrentStreamName = StreamName;

	FString Filename = GenerateFilenameImpl(CachedFrameMetrics, Extension);
	EnsureFileWritableImpl(Filename);

	CurrentStreamName = NAME_None;

	return Filename;
}

void UUserDefinedImageCaptureProtocol::WriteImageToDisk(const FCapturedPixels& PixelData, FName StreamName, const FFrameMetrics& FrameMetrics, bool bCopyImageData)
{
	if (!PixelData.ImageData.IsValid())
	{
		return;
	}

	TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();

	// If the pixels are FColors, and this is the final pixels buffer, and we're writing PNG, always write out full alpha
	if (PixelData.ImageData->GetType() == EImagePixelType::Color && StreamName == FinalPixelsStreamName && ImageTask->Format == EImageFormat::PNG)
	{
		ImageTask->PixelPreProcessors.Add(TAsyncAlphaWrite<FColor>(255));
	}

	// Cache the buffer name so we generate the correct filename
	CurrentStreamName = StreamName;

	ImageTask->PixelData = bCopyImageData ? PixelData.ImageData->CopyImageData() : PixelData.ImageData->MoveImageDataToNew();
	ImageTask->Filename = GenerateFilename(FrameMetrics);
	ImageTask->Format = ImageFormatFromDesired(Format);
	ImageTask->bOverwriteFile = false;

	CurrentStreamName = NAME_None;

	if (Format == EDesiredImageFormat::EXR)
	{
		ImageTask->CompressionQuality = bEnableCompression ? (int32)EImageCompressionQuality::Default : (int32)EImageCompressionQuality::Uncompressed;
	}
	else
	{
		ImageTask->CompressionQuality = bEnableCompression ? CompressionQuality : 100;
	}

	{
		// Set a callback that is called on the main thread when this file has been written
		TWeakObjectPtr<UUserDefinedImageCaptureProtocol> WeakThis = this;
		ImageTask->OnCompleted = [WeakThis](bool)
		{
			UUserDefinedImageCaptureProtocol* This = WeakThis.Get();
			if (This)
			{
				This->OnFileWritten();
			}
		};
	}

	IImageWriteQueue& ImageWriteQueue = FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();
	TFuture<bool> DispatchedTask = ImageWriteQueue.Enqueue(MoveTemp(ImageTask));
	if (DispatchedTask.IsValid())
	{
		// If we actually dispatched the task, increment the number of outstanding operations
		++NumOutstandingOperations;
	}
}

void UUserDefinedImageCaptureProtocol::OnFileWritten()
{
	--NumOutstandingOperations;
}
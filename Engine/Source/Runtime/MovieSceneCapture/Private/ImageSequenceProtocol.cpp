// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Protocols/ImageSequenceProtocol.h"

#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Templates/Casts.h"
#include "MovieSceneCaptureSettings.h"
#include "ImageWriteQueue.h"

struct FImageFrameData : IFramePayload
{
	FString Filename;
};

UImageSequenceProtocol::UImageSequenceProtocol(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Format = EImageFormat::BMP;
	ImageWriteQueue = nullptr;
}

void UImageSequenceProtocol::OnLoadConfigImpl(FMovieSceneCaptureSettings& InSettings)
{
	// Add .{frame} if it doesn't already exist
	FString OutputFormat = InSettings.OutputFormat;

	if (!OutputFormat.Contains(TEXT("{frame}")))
	{
		OutputFormat.Append(TEXT(".{frame}"));

		InSettings.OutputFormat = OutputFormat;
	}

	Super::OnLoadConfigImpl(InSettings);
}

void UImageSequenceProtocol::OnReleaseConfigImpl(FMovieSceneCaptureSettings& InSettings)
{
	// Remove .{frame} if it exists. The "." before the {frame} is intentional because some media players denote frame numbers separated by "."
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT(".{frame}"), TEXT(""));

	Super::OnReleaseConfigImpl(InSettings);
}

bool UImageSequenceProtocol::SetupImpl()
{
	ImageWriteQueue = &FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();
	FinalizeFence = TFuture<void>();

	return Super::SetupImpl();
}

bool UImageSequenceProtocol::HasFinishedProcessingImpl() const
{
	return Super::HasFinishedProcessingImpl() && (!FinalizeFence.IsValid() || FinalizeFence.WaitFor(0));
}

void UImageSequenceProtocol::BeginFinalizeImpl()
{
	FinalizeFence = ImageWriteQueue->CreateFence();
}

void UImageSequenceProtocol::FinalizeImpl()
{
	if (FinalizeFence.IsValid())
	{
		double StartTime = FPlatformTime::Seconds();

		FScopedSlowTask SlowTask(0, NSLOCTEXT("ImageSequenceProtocol", "Finalizing", "Finalizing write operations..."));
		SlowTask.MakeDialogDelayed(.1f, true, true);

		FTimespan HalfSecond = FTimespan::FromSeconds(0.5);
		while ( !GWarn->ReceivedUserCancel() && !FinalizeFence.WaitFor(HalfSecond) )
		{
			// Tick the slow task
			SlowTask.EnterProgressFrame(0);
		}
	}

	Super::FinalizeImpl();
}

FFramePayloadPtr UImageSequenceProtocol::GetFramePayload(const FFrameMetrics& FrameMetrics)
{
	TSharedRef<FImageFrameData, ESPMode::ThreadSafe> FrameData = MakeShareable(new FImageFrameData);

	const TCHAR* Extension = TEXT("");
	switch(Format)
	{
	case EImageFormat::BMP:		Extension = TEXT(".bmp"); break;
	case EImageFormat::PNG:		Extension = TEXT(".png"); break;
	case EImageFormat::JPEG:	Extension = TEXT(".jpg"); break;
	case EImageFormat::EXR:		Extension = TEXT(".exr"); break;
	}

	FrameData->Filename = GenerateFilenameImpl(FrameMetrics, Extension);
	EnsureFileWritableImpl(FrameData->Filename);

	// Add our custom formatting rules as well
	// @todo: document these on the tooltip?
	FrameData->Filename = FString::Format(*FrameData->Filename, StringFormatMap);

	return FrameData;
}

void UImageSequenceProtocol::ProcessFrame(FCapturedFrameData Frame)
{
	check(Frame.ColorBuffer.Num() >= Frame.BufferSize.X * Frame.BufferSize.Y);

	TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();

	// Move the color buffer into a raw image data container that we can pass to the write queue
	ImageTask->PixelData = MakeUnique<TImagePixelData<FColor>>(Frame.BufferSize, MoveTemp(Frame.ColorBuffer));
	if (Format == EImageFormat::PNG)
	{
		// Always write full alpha for PNGs
		ImageTask->PixelPreProcessors.Add(TAsyncAlphaWrite<FColor>(255));
	}

	switch (Format)
	{
	case EImageFormat::EXR:
	case EImageFormat::PNG:
	case EImageFormat::BMP:
	case EImageFormat::JPEG:
		ImageTask->Format = Format;
		break;

	default:
		check(false);
		break;
	}

	ImageTask->CompressionQuality = GetCompressionQuality();
	ImageTask->Filename = Frame.GetPayload<FImageFrameData>()->Filename;

	ImageWriteQueue->Enqueue(MoveTemp(ImageTask));
}

void UImageSequenceProtocol::AddFormatMappingsImpl(TMap<FString, FStringFormatArg>& FormatMappings) const
{
	FormatMappings.Add(TEXT("quality"), TEXT(""));
}

bool UCompressedImageSequenceProtocol::SetupImpl()
{
	FParse::Value( FCommandLine::Get(), TEXT( "-MovieQuality=" ), CompressionQuality );
	CompressionQuality = FMath::Clamp<int32>(CompressionQuality, 1, 100);

	return Super::SetupImpl();
}

void UCompressedImageSequenceProtocol::AddFormatMappingsImpl(TMap<FString, FStringFormatArg>& FormatMappings) const
{
	FormatMappings.Add(TEXT("quality"), CompressionQuality);
}

UImageSequenceProtocol_EXR::UImageSequenceProtocol_EXR(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Format = EImageFormat::EXR;
	bCompressed = false;
	CaptureGamut = HCGM_Rec709;
}

bool UImageSequenceProtocol_EXR::SetupImpl()
{
	{
		int32 OverrideCaptureGamut = (int32)CaptureGamut;
		FParse::Value(FCommandLine::Get(), TEXT("-CaptureGamut="), OverrideCaptureGamut);
		CaptureGamut = (EHDRCaptureGamut)OverrideCaptureGamut;
	}

	int32 HDRCompressionQuality = 0;
	if ( FParse::Value( FCommandLine::Get(), TEXT( "-HDRCompressionQuality=" ), HDRCompressionQuality ) )
	{
		bCompressed = HDRCompressionQuality != (int32)EImageCompressionQuality::Uncompressed;
	}

	IConsoleVariable* CVarDumpGamut = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.ColorGamut"));
	IConsoleVariable* CVarDumpDevice = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.OutputDevice"));

	RestoreColorGamut = CVarDumpGamut->GetInt();
	RestoreOutputDevice = CVarDumpDevice->GetInt();

	if (CaptureGamut == HCGM_Linear)
	{
		CVarDumpGamut->Set(1);
		CVarDumpDevice->Set(7);
	}
	else
	{
		CVarDumpGamut->Set(CaptureGamut);
	}

	return Super::SetupImpl();
}

void UImageSequenceProtocol_EXR::FinalizeImpl()
{
	Super::FinalizeImpl();

	IConsoleVariable* CVarDumpGamut = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.ColorGamut"));
	IConsoleVariable* CVarDumpDevice = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.OutputDevice"));

	CVarDumpGamut->Set(RestoreColorGamut);
	CVarDumpDevice->Set(RestoreOutputDevice);
}

void UImageSequenceProtocol_EXR::AddFormatMappingsImpl(TMap<FString, FStringFormatArg>& FormatMappings) const
{
	FormatMappings.Add(TEXT("quality"), bCompressed ? TEXT("Compressed") : TEXT("Uncompressed"));

	const TCHAR* GamutString = TEXT("");
	switch (CaptureGamut)
	{
		case HCGM_Rec709:  GamutString = TEXT("sRGB"); break;
		case HCGM_P3DCI:   GamutString = TEXT("P3D65"); break;
		case HCGM_Rec2020: GamutString = TEXT("Rec2020"); break;
		case HCGM_ACES:    GamutString = TEXT("ACES"); break;
		case HCGM_ACEScg:  GamutString = TEXT("ACEScg"); break;
		case HCGM_Linear:  GamutString = TEXT("Linear"); break;
		default: check(false); break;
	}
	FormatMappings.Add(TEXT("gamut"), GamutString);
}

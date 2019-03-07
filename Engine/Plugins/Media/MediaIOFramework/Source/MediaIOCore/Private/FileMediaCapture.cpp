// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FileMediaCapture.h"

#include "FileMediaOutput.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"


void UFileMediaCapture::OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData> InUserData, void* InBuffer, int32 Width, int32 Height)
{
	IImageWriteQueueModule* ImageWriteQueueModule = FModuleManager::Get().GetModulePtr<IImageWriteQueueModule>("ImageWriteQueue");
	if (ImageWriteQueueModule == nullptr)
	{
		SetState(EMediaCaptureState::Error);
		return;
	}

	TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
	ImageTask->Format = ImageFormat;
	ImageTask->Filename = FString::Printf(TEXT("%s%5d"), *BaseFilePathName, InBaseData.SourceFrameNumberRenderThread);
	ImageTask->bOverwriteFile = bOverwriteFile;
	ImageTask->CompressionQuality = CompressionQuality;
	ImageTask->OnCompleted = OnCompleteWrapper;

	EPixelFormat PixelFormat = GetDesiredPixelFormat();
	if (PixelFormat == PF_B8G8R8A8)
	{
		TUniquePtr<TImagePixelData<FColor>> PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint(Width, Height));
		PixelData->Pixels = TArray<FColor>(reinterpret_cast<FColor*>(InBuffer), Width * Height);
		ImageTask->PixelData = MoveTemp(PixelData);
	}
	else if (PixelFormat == PF_FloatRGBA)
	{
		TUniquePtr<TImagePixelData<FFloat16Color>> PixelData = MakeUnique<TImagePixelData<FFloat16Color>>(FIntPoint(Width, Height));
		PixelData->Pixels = TArray<FFloat16Color>(reinterpret_cast<FFloat16Color*>(InBuffer), Width * Height);
		ImageTask->PixelData = MoveTemp(PixelData);
	}
	else
	{
		check(false);
	}

	TFuture<bool> DispatchedTask = ImageWriteQueueModule->GetWriteQueue().Enqueue(MoveTemp(ImageTask));

	if (!bAsync)
	{
		// If not async, wait for the dispatched task to complete.
		if (DispatchedTask.IsValid())
		{
			DispatchedTask.Wait();
		}
	}
}


bool UFileMediaCapture::CaptureSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue");
	CacheMediaOutputValues();

	SetState(EMediaCaptureState::Capturing);
	return true;
}


bool UFileMediaCapture::CaptureRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue");
	CacheMediaOutputValues();

	SetState(EMediaCaptureState::Capturing);
	return true;
}


void UFileMediaCapture::CacheMediaOutputValues()
{
	UFileMediaOutput* FileMediaOutput = CastChecked<UFileMediaOutput>(MediaOutput);
	BaseFilePathName = FPaths::Combine(FileMediaOutput->FilePath.Path, FileMediaOutput->BaseFileName);
	ImageFormat = ImageFormatFromDesired(FileMediaOutput->WriteOptions.Format);
	CompressionQuality = FileMediaOutput->WriteOptions.CompressionQuality;
	bOverwriteFile = FileMediaOutput->WriteOptions.bOverwriteFile;
	bAsync = FileMediaOutput->WriteOptions.bAsync;

	OnCompleteWrapper = [NativeCB = FileMediaOutput->WriteOptions.NativeOnComplete, DynamicCB = FileMediaOutput->WriteOptions.OnComplete](bool bSuccess)
	{
		if (NativeCB)
		{
			NativeCB(bSuccess);
		}
		DynamicCB.ExecuteIfBound(bSuccess);
	};
}

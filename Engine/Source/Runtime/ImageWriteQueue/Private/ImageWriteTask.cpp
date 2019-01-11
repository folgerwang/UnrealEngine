// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ImageWriteTask.h"
#include "ImageWriteQueue.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"

struct FGlobalImageWrappers
{
	IImageWrapper* FindOrCreateImageWrapper(EImageFormat InFormat)
	{
		FScopeLock ScopeLock(&ImageWrappersCriticalSection);

		// Try and find an available image wrapper of the correct format first
		for (int32 Index = 0; Index < AvailableImageWrappers.Num(); ++Index)
		{
			if (AvailableImageWrappers[Index].Get<0>() == InFormat)
			{
				IImageWrapper* Wrapper = AvailableImageWrappers[Index].Get<1>();
				AvailableImageWrappers.RemoveAtSwap(Index, 1, false);
				return Wrapper;
			}
		}

		// Create a new one if none other could be used
		IImageWrapperModule* ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>("ImageWrapper");
		if (!ensure(ImageWrapperModule))
		{
			return nullptr;
		}

		TSharedPtr<IImageWrapper> NewImageWrapper = ImageWrapperModule->CreateImageWrapper(InFormat);
		if (!ensureMsgf(NewImageWrapper.IsValid(), TEXT("Unable to create an image wrapper for the desired format.")))
		{
			return nullptr;
		}

		AllImageWrappers.Add(MakeTuple(InFormat, NewImageWrapper.ToSharedRef()));
		return NewImageWrapper.Get();
	}

	void ReturnImageWrapper(IImageWrapper* InWrapper)
	{
		FScopeLock ScopeLock(&ImageWrappersCriticalSection);

		// Try and find an available image wrapper of the correct format first
		for (const TTuple<EImageFormat, TSharedRef<IImageWrapper>>& Pair : AllImageWrappers)
		{
			if (&Pair.Get<1>().Get() == InWrapper)
			{
				AvailableImageWrappers.Add(MakeTuple(Pair.Get<0>(), InWrapper));
				return;
			}
		}

		checkf(false, TEXT("Unable to find image wrapper in list of owned wrappers - this is invalid"));
	}

private:
	FCriticalSection ImageWrappersCriticalSection;
	TArray< TTuple<EImageFormat, IImageWrapper*> > AvailableImageWrappers;
	TArray< TTuple<EImageFormat, TSharedRef<IImageWrapper>> > AllImageWrappers;
} GImageWrappers;



static const TCHAR* GetFormatExtension(EImageFormat InImageFormat)
{
	switch (InImageFormat)
	{
	case EImageFormat::PNG:           return TEXT(".png");
	case EImageFormat::JPEG:          return TEXT(".jpg");
	case EImageFormat::GrayscaleJPEG: return TEXT(".jpg");
	case EImageFormat::BMP:           return TEXT(".bmp");
	case EImageFormat::ICO:           return TEXT(".ico");
	case EImageFormat::EXR:           return TEXT(".exr");
	case EImageFormat::ICNS:          return TEXT(".icns");
	}
	return nullptr;
}

bool FImageWriteTask::RunTask()
{
	bool bSuccess = WriteToDisk();

	if (OnCompleted)
	{
		AsyncTask(ENamedThreads::GameThread, [bSuccess, LocalOnCompleted = MoveTemp(OnCompleted)] { LocalOnCompleted(bSuccess); });
	}

	return bSuccess;
}

void FImageWriteTask::OnAbandoned()
{
	if (OnCompleted)
	{
		AsyncTask(ENamedThreads::GameThread, [LocalOnCompleted = MoveTemp(OnCompleted)] { LocalOnCompleted(false); });
	}
}

bool FImageWriteTask::InitializeWrapper(IImageWrapper* InWrapper, EImageFormat WrapperFormat)
{
	const void* RawPtr = nullptr;
	int32 SizeBytes = 0;

	if (PixelData->GetRawData(RawPtr, SizeBytes))
	{
		uint8      BitDepth    = PixelData->GetBitDepth();
		FIntPoint  Size        = PixelData->GetSize();
		ERGBFormat PixelLayout = PixelData->GetPixelLayout();

		return InWrapper->SetRaw(RawPtr, SizeBytes, Size.X, Size.Y, PixelLayout, BitDepth);
	}

	return false;
}

bool FImageWriteTask::WriteBitmap()
{
	uint8      NumChannels = PixelData->GetNumChannels();
	uint8      BitDepth    = PixelData->GetBitDepth();
	FIntPoint  Size        = PixelData->GetSize();

	if (BitDepth != 8 || NumChannels != 4)
	{
		return false;
	}

	const void* RawPtr = nullptr;
	int32 SizeBytes = 0;

	if (PixelData->GetRawData(RawPtr, SizeBytes))
	{
		return FFileHelper::CreateBitmap(*Filename, Size.X, Size.Y, static_cast<const FColor*>(RawPtr));
	}
	return false;
}

void FImageWriteTask::PreProcess()
{
	FImagePixelData* Data = PixelData.Get();
	for (const FPixelPreProcessor& PreProcessor : PixelPreProcessors)
	{
		// PreProcessors are assumed to be valid.
		PreProcessor(Data);
	}
}

bool FImageWriteTask::WriteToDisk()
{
	// Ensure that the payload filename has the correct extension for the format (have to special case jpeg since they can be both *.jpg and *.jpeg)
	const TCHAR* FormatExtension = GetFormatExtension(Format);
	if (FormatExtension && !Filename.EndsWith(FormatExtension) && (Format != EImageFormat::JPEG || !Filename.EndsWith(TEXT(".jpeg"))))
	{
		Filename = FPaths::GetBaseFilename(Filename, false) + FormatExtension;
	}

	bool bSuccess = EnsureWritableFile();

	if (bSuccess)
	{
		PreProcess();

		// bitmap support with IImageWrapper is flaky so it needs its own codepath for now
		if (Format == EImageFormat::BMP)
		{
			bSuccess = WriteBitmap();
		}
		else
		{
			IImageWrapper* ImageWrapper = GImageWrappers.FindOrCreateImageWrapper(Format);
			if (ImageWrapper)
			{
				if (InitializeWrapper(ImageWrapper, Format))
				{
					bSuccess = FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(CompressionQuality), *Filename);
				}

				GImageWrappers.ReturnImageWrapper(ImageWrapper);
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogImageWriteQueue, Error, TEXT("Failed to write image to '%s'. The pixel format may not be compatible with this image type, or there was an error writing to that filename."), *Filename);
	}

	return bSuccess;
}

bool FImageWriteTask::EnsureWritableFile()
{
	FString Directory = FPaths::GetPath(Filename);

	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		IFileManager::Get().MakeDirectory(*Directory);
	}

	// If the file doesn't exist, we're ok to continue
	if (IFileManager::Get().FileSize(*Filename) == -1)
	{
		return true;
	}
	// If we're allowed to overwrite the file, and we deleted it ok, we can continue
	else if (bOverwriteFile && FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
	{
		return true;
	}
	// We can't write to the file
	else
	{
		UE_LOG(LogImageWriteQueue, Error, TEXT("Failed to write image to '%s'. Should Overwrite: %d - If we should have overwritten the file, we failed to delete the file. If we shouldn't have overwritten the file the file already exists so we can't replace it."), *Filename, bOverwriteFile);
		return false;
	}
}


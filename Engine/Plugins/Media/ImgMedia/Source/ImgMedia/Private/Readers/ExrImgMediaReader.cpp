// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExrImgMediaReader.h"

#if IMGMEDIA_EXR_SUPPORTED_PLATFORM

#include "Async/Async.h"
#include "Misc/Paths.h"
#include "OpenExrWrapper.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"


/* FExrImgMediaReader structors
 *****************************************************************************/

FExrImgMediaReader::FExrImgMediaReader()
{
	auto Settings = GetDefault<UImgMediaSettings>();
	
	FOpenExr::SetGlobalThreadCount(Settings->ExrDecoderThreads == 0
		? FPlatformMisc::NumberOfCoresIncludingHyperthreads()
		: Settings->ExrDecoderThreads);
}


/* FExrImgMediaReader interface
 *****************************************************************************/

bool FExrImgMediaReader::GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo)
{
	FRgbaInputFile InputFile(ImagePath, 2);
	
	return GetInfo(InputFile, OutInfo);
}


bool FExrImgMediaReader::ReadFrame(const FString& ImagePath, FImgMediaFrame& OutFrame)
{
	FRgbaInputFile InputFile(ImagePath, 2);
	
	if (!GetInfo(InputFile, OutFrame.Info))
	{
		return false;
	}

	const FIntPoint& Dim = OutFrame.Info.Dim;

	if (Dim.GetMin() <= 0)
	{
		return false;
	}

	// allocate frame buffer
	const SIZE_T BufferSize = Dim.X * Dim.Y * sizeof(uint16) * 4;
	void* Buffer = FMemory::Malloc(BufferSize, PLATFORM_CACHE_LINE_SIZE);

	auto BufferDeleter = [BufferSize](void* ObjectToDelete) {
#if USE_IMGMEDIA_DEALLOC_POOL
		if (FQueuedThreadPool* ImgMediaThreadPoolSlow = GetImgMediaThreadPoolSlow())
		{
			// free buffers on the thread pool, because memory allocators may perform
			// expensive operations, such as filling the memory with debug values
			TFunction<void()> FreeBufferTask = [ObjectToDelete]()
			{
				FMemory::Free(ObjectToDelete);
			};
			AsyncPool(*ImgMediaThreadPoolSlow, FreeBufferTask);
		}
		else
		{
			FMemory::Free(ObjectToDelete);
		}
#else
		FMemory::Free(ObjectToDelete);
#endif
	};

	// read frame data
	InputFile.SetFrameBuffer(Buffer, Dim);
	InputFile.ReadPixels(0, Dim.Y - 1);

	OutFrame.Data = MakeShareable(Buffer, MoveTemp(BufferDeleter));
	OutFrame.Format = EMediaTextureSampleFormat::FloatRGBA;
	OutFrame.Stride = Dim.X * sizeof(unsigned short) * 4;

	return true;
}


/* FExrImgMediaReader implementation
 *****************************************************************************/

bool FExrImgMediaReader::GetInfo(FRgbaInputFile& InputFile, FImgMediaFrameInfo& OutInfo) const
{
	OutInfo.CompressionName = InputFile.GetCompressionName();
	OutInfo.Dim = InputFile.GetDataWindow();
	OutInfo.FormatName = TEXT("EXR");
	OutInfo.FrameRate = InputFile.GetFrameRate(ImgMedia::DefaultFrameRate);
	OutInfo.Srgb = false;
	OutInfo.UncompressedSize = InputFile.GetUncompressedSize();

	return (OutInfo.UncompressedSize > 0) && (OutInfo.Dim.GetMin() > 0);
}


#endif //IMGMEDIA_EXR_SUPPORTED_PLATFORM

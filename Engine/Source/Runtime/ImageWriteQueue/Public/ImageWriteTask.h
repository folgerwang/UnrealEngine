// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDefines.h"
#include "Math/Color.h"
#include "Math/Float16Color.h"
#include "Math/IntPoint.h"
#include "Templates/UniquePtr.h"
#include "IImageWrapper.h"
#include "ImagePixelData.h"

template<typename> struct TImageDataTraits;

typedef TFunction<void(FImagePixelData*)> FPixelPreProcessor;

/**
 * Interface that is implemented in order to be able to asynchronously write images to disk
 */
class IImageWriteTaskBase
{
public:

	/**
	 * Virtual destruction
	 */
	virtual ~IImageWriteTaskBase() {}

	/**
	 * 
	 */
	virtual bool RunTask() = 0;

	/**
	 * 
	 */
	virtual void OnAbandoned() = 0;
};

class FImageWriteTask
	: public IImageWriteTaskBase
{
public:

	/** The filename to write to */
	FString Filename;

	/** The desired image format to write out */
	EImageFormat Format;

	/** A compression quality setting specific to the desired image format */
	int32 CompressionQuality;

	/** True if this task is allowed to overwrite an existing file, false otherwise. */
	bool bOverwriteFile;

	/** A function to invoke on the game thread when the task has completed */
	TFunction<void(bool)> OnCompleted;

	/** The actual write operation. */
	TUniquePtr<FImagePixelData> PixelData;

	/** Array of preprocessors to apply serially to the pixel data when this task is executed. */
	TArray<FPixelPreProcessor> PixelPreProcessors;

	FImageWriteTask()
		: Format(EImageFormat::BMP)
		, CompressionQuality((int32)EImageCompressionQuality::Default)
		, bOverwriteFile(true)
	{}

public:

	IMAGEWRITEQUEUE_API virtual bool RunTask() override final;
	IMAGEWRITEQUEUE_API virtual void OnAbandoned() override final;

private:

	/**
	 * Run the task, attempting to write out the raw data using the currently specified parameters
	 *
	 * @return true on success, false on any failure
	 */
	bool WriteToDisk();

	/**
	 * Ensures that the desired output filename is writable, deleting an existing file if bOverwriteFile is true
	 *
	 * @return True if the file is writable and the task can proceed, false otherwise
	 */
	bool EnsureWritableFile();

	/**
	 * Initialize the specified image wrapper with our raw data, ready for writing
	 *
	 * @param InWrapper      The wrapper to initialize with our data
	 * @param WrapperFormat  The desired image format to write out
	 * @return true on success, false on any failure
	 */
	bool InitializeWrapper(IImageWrapper* InWrapper, EImageFormat WrapperFormat);

	/**
	 * Special case implementation for writing bitmap data due to deficiencies in the IImageWriter API (it can't set raw pixel data without trying to compress it, which asserts)
	 *
	 * @return true a bitmap was written out at the specified filename, false otherwise
	 */
	bool WriteBitmap();


	/**
	 * Run over all the processors for the pixel data
	 */
	void PreProcess();
};


/**
 * A pixel preprocessor for use with FImageWriteTask::PixelPreProcessor that does gamma correction as part of the threaded work
 */
template<typename PixelType> struct TAsyncGammaCorrect;


template<>
struct TAsyncGammaCorrect<FColor>
{
	float Gamma;
	TAsyncGammaCorrect(float InGamma) : Gamma(InGamma) {}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Color);

		TImagePixelData<FColor>* ColorData = static_cast<TImagePixelData<FColor>*>(PixelData);
		for (FColor& Pixel : ColorData->Pixels)
		{
			Pixel.A = (uint8)FMath::RoundToFloat(FMath::Pow(Pixel.A / 255.f, Gamma) * 255.f);
			Pixel.R = (uint8)FMath::RoundToFloat(FMath::Pow(Pixel.R / 255.f, Gamma) * 255.f);
			Pixel.G = (uint8)FMath::RoundToFloat(FMath::Pow(Pixel.G / 255.f, Gamma) * 255.f);
			Pixel.B = (uint8)FMath::RoundToFloat(FMath::Pow(Pixel.B / 255.f, Gamma) * 255.f);
		}
	}
};

template<>
struct TAsyncGammaCorrect<FFloat16Color>
{
	float Gamma;
	TAsyncGammaCorrect(float InGamma) : Gamma(InGamma) {}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Float16);

		TImagePixelData<FFloat16Color>* Float16ColorData = static_cast<TImagePixelData<FFloat16Color>*>(PixelData);
		for (FFloat16Color& Pixel : Float16ColorData->Pixels)
		{
			Pixel.A = FMath::Pow(Pixel.A.GetFloat(), Gamma);
			Pixel.R = FMath::Pow(Pixel.R.GetFloat(), Gamma);
			Pixel.G = FMath::Pow(Pixel.G.GetFloat(), Gamma);
			Pixel.B = FMath::Pow(Pixel.B.GetFloat(), Gamma);
		}
	}
};

template<>
struct TAsyncGammaCorrect<FLinearColor>
{
	float Gamma;
	TAsyncGammaCorrect(float InGamma) : Gamma(InGamma) {}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Float32);

		TImagePixelData<FLinearColor>* LinearColorData = static_cast<TImagePixelData<FLinearColor>*>(PixelData);
		for (FLinearColor& Pixel : LinearColorData->Pixels)
		{
			Pixel.A = FMath::Pow(Pixel.A, Gamma);
			Pixel.R = FMath::Pow(Pixel.R, Gamma);
			Pixel.G = FMath::Pow(Pixel.G, Gamma);
			Pixel.B = FMath::Pow(Pixel.B, Gamma);
		}
	}
};


/**
 * A pixel preprocessor for use with FImageWriteTask::PixelPreProcessor that overwrites the alpha channel with a fixed value as part of the threaded work
 */
template<typename PixelType> struct TAsyncAlphaWrite;

template<>
struct TAsyncAlphaWrite<FColor>
{
	uint8 Alpha;
	TAsyncAlphaWrite(uint8 InAlpha) : Alpha(InAlpha) {}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Color);

		TImagePixelData<FColor>* ColorData = static_cast<TImagePixelData<FColor>*>(PixelData);
		for (FColor& Pixel : static_cast<TImagePixelData<FColor>*>(PixelData)->Pixels)
		{
			Pixel.A = Alpha;
		}
	}
};

template<>
struct TAsyncAlphaWrite<FFloat16Color>
{
	FFloat16 Alpha;
	TAsyncAlphaWrite(float InAlpha) : Alpha(InAlpha) {}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Float16);

		TImagePixelData<FFloat16Color>* Float16ColorData = static_cast<TImagePixelData<FFloat16Color>*>(PixelData);
		for (FFloat16Color& Pixel : Float16ColorData->Pixels)
		{
			Pixel.A = Alpha;
		}
	}
};

template<>
struct TAsyncAlphaWrite<FLinearColor>
{
	float Alpha;
	TAsyncAlphaWrite(float InAlpha) : Alpha(InAlpha) {}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Float32);

		TImagePixelData<FLinearColor>* LinearColorData = static_cast<TImagePixelData<FLinearColor>*>(PixelData);
		for (FLinearColor& Pixel : LinearColorData->Pixels)
		{
			Pixel.A = Alpha;
		}
	}
};
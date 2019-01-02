// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDefines.h"
#include "Math/IntPoint.h"
#include "IImageWrapper.h"
#include "Templates/UniquePtr.h"

class FFloat16Color;
template<typename PixelType> struct TImagePixelDataTraits;


enum class EImagePixelType
{
	Color,
	Float16,
	Float32,
};

struct FImagePixelData
{
	virtual ~FImagePixelData() {}

	/**
	 * Retrieve the type of this data
	 */
	EImagePixelType GetType() const
	{
		return Type;
	}

	/**
	 * Retrieve the size of this data
	 */
	FIntPoint GetSize() const
	{
		return Size;
	}

	/**
	 * Retrieve the pixel layout of this data
	 */
	ERGBFormat GetPixelLayout() const
	{
		return PixelLayout;
	}

	/**
	 * Retrieve the number of bits per each channel of color in the data
	 */
	uint8 GetBitDepth() const
	{
		return BitDepth;
	}

	/**
	 * Retrieve the number of channels in the data
	 */
	uint8 GetNumChannels() const
	{
		return NumChannels;
	}

	/**
	 * Check that this data is the size it should be
	 */
	bool IsDataWellFormed() const
	{
		const void* RawPtr    = nullptr;
		int32       SizeBytes = 0;

		return GetRawData(RawPtr, SizeBytes);
	}

	/**
	 * Get the data and its size only if it is well formed
	 */
	bool GetRawData(const void*& OutRawData, int32& OutSizeBytes) const
	{
		const void* RawPtr    = nullptr;
		int32       SizeBytes = 0;

		RetrieveData(RawPtr, SizeBytes);

		if (RawPtr && SizeBytes == Size.X*Size.Y*BitDepth/8*NumChannels)
		{
			OutRawData = RawPtr;
			OutSizeBytes = SizeBytes;
			return true;
		}
		return false;
	}

	/**
	 * Copy this whole image buffer. This can be very costly for large images.
	 */
	TUniquePtr<FImagePixelData> CopyImageData() const
	{
		return Copy();
	}

	/**
	 * Move this whole image buffer to a new allocation.
	 */
	TUniquePtr<FImagePixelData> MoveImageDataToNew()
	{
		return Move();
	}

protected:

	FImagePixelData(const FIntPoint& InSize, EImagePixelType InPixelType, ERGBFormat InPixelLayout, uint8 InBitDepth, uint8 InNumChannels)
		: Size(InSize)
		, Type(InPixelType)
		, PixelLayout(InPixelLayout)
		, BitDepth(InBitDepth)
		, NumChannels(InNumChannels)
	{}

private:

	/**
	 * Retrieve the raw pixel data
	 */
	virtual void RetrieveData(const void*& OutDataPtr, int32& OutSizeBytes) const = 0;
	virtual TUniquePtr<FImagePixelData> Copy() const = 0;
	virtual TUniquePtr<FImagePixelData> Move() = 0;

	/** The size of the pixel data */
	FIntPoint Size;

	/** The type of the derived data */
	EImagePixelType Type;

	/** The pixel layout of this data */
	ERGBFormat PixelLayout;

	/** The number of bits per each channel of color in the data */
	uint8 BitDepth;

	/** Number of channels in the data */
	uint8 NumChannels;
};

/**
 * Templated pixel data - currently supports FColor, FFloat16Color and FLinearColor
 */
template<typename PixelType>
struct TImagePixelData : FImagePixelData
{
	TArray<PixelType> Pixels;

	TImagePixelData(const FIntPoint& InSize)
		: FImagePixelData(InSize, TImagePixelDataTraits<PixelType>::PixelType, TImagePixelDataTraits<PixelType>::PixelLayout, TImagePixelDataTraits<PixelType>::BitDepth, TImagePixelDataTraits<PixelType>::NumChannels)
	{}

	TImagePixelData(const FIntPoint& InSize, TArray<PixelType>&& InPixels)
		: FImagePixelData(InSize, TImagePixelDataTraits<PixelType>::PixelType, TImagePixelDataTraits<PixelType>::PixelLayout, TImagePixelDataTraits<PixelType>::BitDepth, TImagePixelDataTraits<PixelType>::NumChannels)
		, Pixels(MoveTemp(InPixels))
	{}

	virtual TUniquePtr<FImagePixelData> Move() override
	{
		return MakeUnique<TImagePixelData<PixelType>>(MoveTemp(*this));
	}

	virtual TUniquePtr<FImagePixelData> Copy() const override
	{
		return MakeUnique<TImagePixelData<PixelType>>(*this);
	}

	virtual void RetrieveData(const void*& OutDataPtr, int32& OutSizeBytes) const override
	{
		OutDataPtr = static_cast<const void*>(&Pixels[0]);
		OutSizeBytes = Pixels.Num() * sizeof(PixelType);
	}
};

template<> struct TImagePixelDataTraits<FColor>
{
	static const ERGBFormat PixelLayout = ERGBFormat::BGRA;
	static const EImagePixelType PixelType = EImagePixelType::Color;

	enum { BitDepth = 8, NumChannels = 4 };
};

template<> struct TImagePixelDataTraits<FFloat16Color>
{
	static const ERGBFormat PixelLayout = ERGBFormat::RGBA;
	static const EImagePixelType PixelType = EImagePixelType::Float16;

	enum { BitDepth = 16, NumChannels = 4 };
};

template<> struct TImagePixelDataTraits<FLinearColor>
{
	static const ERGBFormat PixelLayout = ERGBFormat::RGBA;
	static const EImagePixelType PixelType = EImagePixelType::Float32;

	enum { BitDepth = 32, NumChannels = 4 };
};


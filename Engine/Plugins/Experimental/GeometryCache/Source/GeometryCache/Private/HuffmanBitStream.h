// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"


/**
A bit stream writer class for use with the Huffman coding.
This class allows coding arbitrary sized integers up to 32 bits in size. The bits in the int are logically written
into the stream with their most significant bit first.
*/
class FHuffmanBitStreamWriter
{
public:

	/**
	Create a writer. The class will automatically grow it's internal byte buffer used to store the bits.
	*/
	FHuffmanBitStreamWriter()
	{
		Clear();
	}

	/**
	Reset the writer as if it was freshly created. All written data will be discarded.
	*/
	void Clear()
	{
		Bytes.Empty();
		Pos = -1;
		BitsLeftInLastByte = 0;
		bFlushed = false;
		NumBits = 0;
	}

	/**
	Return a new integer which only the lowest NumBits of the input integer K.
	*/
	FORCEINLINE static uint32 LeastNBits(uint32 K, uint32 NumBits)
	{
		return K & ((1 << NumBits) - 1);
	}

	/**
	Return a new integer with NumBits bits from K starting at FirstBit. The
	returned value is right aligned so FirstBit will be in position 0 of
	the returned number.
	*/
	FORCEINLINE static uint32 ExtractBits(uint32 K, int32 FirstBit, uint32 InNumBits)
	{
		return LeastNBits(K >> FirstBit, InNumBits);
	}

	/**
	Write an NumBits integer value to the stream. The most significant bit
	will be logically first in the stream.
	*/
	void Write(uint32 Bits, uint32 InNumBits)
	{
		checkf(bFlushed == false, TEXT("You cannot write to a stream that has already been closed"));

		uint32 BitsLeft = InNumBits;
		while (BitsLeft > 0)
		{
			// Alloc another byte if we're out of space
			if (BitsLeftInLastByte == 0)
			{
				Bytes.Add(0);
				Pos++;
				BitsLeftInLastByte = 8;
			}

			const uint32 NumBitsToWrite = FMath::Min(BitsLeftInLastByte, BitsLeft);
			Bytes[Pos] = Bytes[Pos] << NumBitsToWrite;
			Bytes[Pos] |= ExtractBits(Bits, BitsLeft - NumBitsToWrite, NumBitsToWrite);
			BitsLeft -= NumBitsToWrite;
			BitsLeftInLastByte -= NumBitsToWrite;
		}
		NumBits += InNumBits;
	}

	/**
	Close the stream. This ensures the stream's underlying byte buffer is correctly
	flushed with all bits written to the stream.
	Once closed you cannot longer Write additional data on the stream.
	*/
	void Close()
	{
		// Write zeros to the last byte this ensures the MSB is correctly shifted left
		// in the last byte.
		Write(0, BitsLeftInLastByte);
		bFlushed = true;
	}

	/**
	Get the bytes corresponding to this stream. The destination will be overwritten with the
	bytes for this stream.
	This can only be called on a flushed stream.
	*/
	const TArray<uint8> &GetBytes()
	{
		checkf(bFlushed == true, TEXT("You can only get bytes on an stream that has been closed"));
		return Bytes;
	}

	/**
	Get the number of bytes written into the stream so far.
	*/
	uint32 GetNumBytes() const
	{
		return Bytes.Num();
	}

	/**
	Get the number of bits written into the stream so far.
	*/
	uint32 GetNumBits() const
	{
		return NumBits;
	}

private:
	TArray<uint8> Bytes;
	uint32 Pos;
	uint32 BitsLeftInLastByte;
	uint32 NumBits;
	bool bFlushed;
};

/**
Helper class to keep track of the amount of bits written by the BitstreamWriter
*/
class FBitstreamWriterByteCounter
{
public:	
	FBitstreamWriterByteCounter(const FHuffmanBitStreamWriter* SetWriter) : StartNumBits(0), StartNumBytes(0), Writer(SetWriter)
	{
		StartNumBits = Writer->GetNumBits();
		StartNumBytes = Writer->GetNumBytes();
	}

	/**
	Returns the amount of bytes written since this object was constructed
	*/
	uint32 Read()
	{
		const bool bHasExtraBits = Writer->GetNumBits() != StartNumBits;
		return Writer->GetNumBytes() - StartNumBytes + (bHasExtraBits ? 1 : 0);
	}
private:
	uint32 StartNumBits;
	uint32 StartNumBytes;
	const FHuffmanBitStreamWriter* Writer;
};

class FHuffmanBitStreamReader
{
public:

	/**
	Initialize the stream for reading with the specified data bytes. Bytes will be read assuming the most significant bit in the byte comes logically
	earlier in the stream.
	*/
	FHuffmanBitStreamReader(uint8 *Data, uint32 NumBytes)
	{
		Bytes.SetNumUninitialized(NumBytes);
		FMemory::Memcpy(&Bytes[0], Data, NumBytes);
		Reset();
	}

	/**
	Initialize the stream for reading with the specified data bytes. Bytes will be read assuming the most significant bit in the byte comes logically
	earlier in the stream.
	*/
	FHuffmanBitStreamReader(const TArray<uint8> &SetBytes)
	{
		Bytes = SetBytes;
		Reset();
	}

	/**
	Restart reading from the stream. Reading will begin anew from the start of the stream.
	*/
	void Reset()
	{
		ReadPos = -1;
		BitsLeftInByte = 0;
	}

	/**
	Read a the next bit from the stream.
	*/
	FORCEINLINE uint32 Read()
	{
		// Advance to the next byte
		if (BitsLeftInByte == 0)
		{
			ReadPos++;
			BitsLeftInByte = 8;
			checkf(ReadPos < Bytes.Num(), TEXT("Read past the end of the bitstream"));
		}

		uint32 Result = FHuffmanBitStreamWriter::ExtractBits(Bytes[ReadPos], BitsLeftInByte - 1, 1);
		BitsLeftInByte--;
		return Result;
	}

	/**
	Read the next NumBits from the stream.
	*/
	FORCEINLINE uint32 Read(uint32 NumBits)
	{
		uint32 BitsLeft = NumBits;
		uint32 Result = 0;
		while (BitsLeft > 0)
		{
			// Advance to the next byte
			if (BitsLeftInByte == 0)
			{
				ReadPos++;
				BitsLeftInByte = 8;
				checkf(ReadPos < Bytes.Num(), TEXT("Read past the end of the bitstream"));
			}

			const uint32 NumBitsToRead = FMath::Min(BitsLeftInByte, BitsLeft);
			Result = Result << NumBitsToRead;
			Result |= FHuffmanBitStreamWriter::ExtractBits(Bytes[ReadPos], BitsLeftInByte - NumBitsToRead, NumBitsToRead);
			BitsLeft -= NumBitsToRead;
			BitsLeftInByte -= NumBitsToRead;
		}
		return Result;
	}

	/**
	Skip the next NumBits in the stream.
	This "may" be slightly faster than reading the bits and discarding the result.
	*/
	void Advance(uint32 NumBits)
	{
		uint32 BitsLeft = NumBits;
		while (BitsLeft > 0)
		{
			// Advance to the next byte
			if (BitsLeftInByte == 0)
			{
				ReadPos++;
				BitsLeftInByte = 8;
				checkf(ReadPos < Bytes.Num(), TEXT("Read past the end of the bitstream"));
			}

			const uint32 NumBitsToRead = FMath::Min(BitsLeftInByte, BitsLeft);
			BitsLeft -= NumBitsToRead;
			BitsLeftInByte -= NumBitsToRead;
		}
	}

	/**
	Return the next NumBits in the stream but do not advance the read position.
	If this reads past the end zero bits will be returned for the bits beyond the end of the stream.
	*/
	FORCEINLINE int32 PeekWithZeros(uint32 NumBits)
	{
		uint32 BitsLeft = NumBits;
		uint32 Result = 0;
		int32 PeekPos = ReadPos;
		uint32 PeekBitsLeftInByte = BitsLeftInByte;
		while (BitsLeft > 0)
		{
			// Advance to the next byte
			if (PeekBitsLeftInByte == 0)
			{
				PeekPos++;
				PeekBitsLeftInByte = 8;
			}

			const uint32 NumBitsToRead = FMath::Min(PeekBitsLeftInByte, BitsLeft);
			Result = Result << NumBitsToRead;
			Result |= FHuffmanBitStreamWriter::ExtractBits((PeekPos >= Bytes.Num()) ? 0 : Bytes[PeekPos], PeekBitsLeftInByte - NumBitsToRead, NumBitsToRead);
			BitsLeft -= NumBitsToRead;
			PeekBitsLeftInByte -= NumBitsToRead;
		}
		return Result;
	}

	/**
	Get the total number of bytes in the stream.
	*/
	FORCEINLINE int32 GetNumBytes()
	{
		return Bytes.Num();
	}

	/**
	Note GetNumBits intentionally does not exist. This class doesn't know exactly how much bits there are
	it only knows this up to a multiple of 8 the bytes. If data was correctly written using
	the FHuffmanBitstreamWiter class these extra bits are however guaranteed to be zero.
	*/
	//uint32 GetNumBits()

private:
	TArray<uint8> Bytes;
	int32 ReadPos;
	uint32 BitsLeftInByte;
};


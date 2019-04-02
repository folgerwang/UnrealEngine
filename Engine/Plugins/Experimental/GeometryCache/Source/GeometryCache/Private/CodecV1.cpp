// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "CodecV1.h"
#include "GeometryCacheMeshData.h"
#include "Serialization/MemoryWriter.h"
#include "GeometryCacheStreamingManager.h"
#include "GeometryCacheTrackStreamable.h"
#include "GeometryCacheModule.h"
#include "Misc/FileHelper.h"
#include "HuffmanBitStream.h"
#include "CodecV1Test.h"
#include "Stats/StatsMisc.h"

/*
//#define DEBUG_DUMP_FRAMES // Dump raw frames to file during encoding for debugging
//#define DEBUG_CODECTEST_DUMPED_FRAMES // Test codec using dumped raw files

#ifdef DEBUG_CODECTEST_DUMPED_FRAMES
#ifndef DEBUG_DUMP_FRAMES
// Standalone codec for the test, reads raw dumps of frames, encodes and decodes them.
static CodecV1Test CodecV1Test(TEXT("F:\\"));		// Test run in constructor
#endif
#endif
*/

static TAutoConsoleVariable<int32> CVarCodecDebug(
	TEXT("GeometryCache.Codec.Debug"),
	0,
	TEXT("Enables debug logging for the codec."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

DEFINE_LOG_CATEGORY(LogGeoCaStreamingCodecV1);

// At start of frame
struct FCodedFrameHeader
{
	uint32 Magic;
	uint32 PayloadSize;
	uint32 IndexCount;
	uint32 VertexCount;
};

// At start of vertex position stream
struct FVertexStreamHeader
{	
	float QuantizationPrecision;
	FIntVector Translation;
};

// At start of UV stream
struct FUVStreamHeader
{
	uint32 QuantizationBits;
	FVector2D Range;
};

// At start of UV stream
struct FMotionVectorStreamHeader
{
	float QuantizationPrecision;
};

/** Timer returning milliseconds, for fast iteration development */
class FExperimentTimer
{
public:
	FExperimentTimer()
	{
		StartTime = FPlatformTime::Seconds();
	}

	double Get()
	{
		return (FPlatformTime::Seconds() - StartTime) * 1000.0;
	}

private:
	double StartTime;
};

/** Quantizer, discretizes a continuous range of values into bins */
class FQuantizer
{
public:	
	/** Initialize with a fixed precision (which is the bin size) */
	FQuantizer(float Precision)
	{
		BinSize = Precision;
		HalfBinSize = BinSize / 2.0f;
		OneOverBinSize = 1.0f / BinSize;
	}

	/** Initialize with a range and a number of bits. NumBits determines the number of bins we have, 
	    and Range determines the sizes of the bins. */
	FQuantizer(float Range, int32 NumBits)
	{		
		int32 BinCount = (int)FMath::Pow(2.0f, NumBits);
		BinSize = Range / BinCount;
		HalfBinSize = Range / BinCount / 2.0f;
		OneOverBinSize = BinCount / Range;
	}
#if WITH_EDITOR
	/** Quantize a value */
	FORCEINLINE int32 QuantizeFloat(float Value)
	{
		// We compensate for energy loss around zero, e.g., Given a bin size 1, we map [-0.5,0.5[ -> 0, [-1.5,-0.5[ -> -1, [0.5,1.5[ -> 1, 
		int32 Negative = (int)(Value >= 0.0f) * 2 - 1; // Positive: 1, negative: -1
		int32 IntValue = (FMath::Abs(Value) + HalfBinSize) * OneOverBinSize;
		return IntValue * Negative;
	}
#endif // WITH_EDITOR

	/** Dequantize a quantized value */
	FORCEINLINE float DequantizeFloat(int32 Value)
	{
		return (float)Value * BinSize;
	}

private:
	float BinSize;
	float HalfBinSize;
	float OneOverBinSize;
};

/** Quantizer for FVector2Ds, discretizes a continuous 2D range of values into bins */
class FQuantizerVector2
{
public:
	/** Initialize with a fixed precision (which is the bin size) */
	FQuantizerVector2(float Precision) : QuantizerX(Precision), QuantizerY(Precision)
	{
	}

	/** Initialize with a range and a number of bits. NumBits determines the number of bins we have,
		and Range determines the sizes of the bins. */
	FQuantizerVector2(const FVector2D& Range, int32 Bits) : QuantizerX(Range.GetMax(), Bits), QuantizerY(Range.GetMax(), Bits)
	{
	}

#if WITH_EDITOR
	/** Quantize a value */
	FORCEINLINE FIntVector Quantize(const FVector2D& Value)
	{
		return FIntVector(QuantizerX.QuantizeFloat(Value.X), QuantizerY.QuantizeFloat(Value.Y), 0);
	}
#endif // WITH_EDITOR

	/** Dequantize a quantized value */
	FORCEINLINE FVector2D Dequantize(const FIntVector& Value)
	{
		return FVector2D(QuantizerX.DequantizeFloat(Value.X), QuantizerY.DequantizeFloat(Value.Y));
	}

private:
	FQuantizer QuantizerX;
	FQuantizer QuantizerY;
};

/** Quantizer for FVectors, discretizes a continuous 3D range of values into bins */
class FQuantizerVector3
{
public:
	/** Initialize with a fixed precision (which is the bin size) */
	FQuantizerVector3(float Precision) : Quantizer(Precision)
	{
	}

	/** Initialize with a range and a number of bits. NumBits determines the number of bins we have,
		and Range determines the sizes of the bins. */
	FQuantizerVector3(const FVector& Range, int32 Bits) : Quantizer(Range.GetMax(), Bits)
	{
	}

#if WITH_EDITOR
	/** Quantize a value */
	FORCEINLINE FIntVector Quantize(const FVector& Value)
	{
		return FIntVector(Quantizer.QuantizeFloat(Value.X), Quantizer.QuantizeFloat(Value.Y), Quantizer.QuantizeFloat(Value.Z));
	}
#endif // WITH_EDITOR

	/** Dequantize a quantized value */
	FORCEINLINE FVector Dequantize(const FIntVector& Value)
	{
		return FVector(Quantizer.DequantizeFloat(Value.X), Quantizer.DequantizeFloat(Value.Y), Quantizer.DequantizeFloat(Value.Z));
	}

private:
	FQuantizer Quantizer;	
};

// Custom serialization for our const TArray because of the const
FArchive& operator<<(FArchive &Ar, const TArray<FGeometryCacheMeshBatchInfo>& BatchesInfo)
{
	check(Ar.IsSaving());
	int32 Num = BatchesInfo.Num();
	Ar << Num;

	for (int32 Index = 0; Index < Num; ++Index)
	{
		FGeometryCacheMeshBatchInfo NonConstCopy = BatchesInfo[Index];
		Ar << NonConstCopy;
	}

	return Ar;
}

// Custom serialization for our const FBox because of the const
FArchive& operator<<(FArchive &Ar, const FBox& Box)
{
	check(Ar.IsSaving());
	FBox NonConstBox = Box; // copy
	Ar << NonConstBox;
	return Ar;
}

#if WITH_EDITOR
FCodecV1Encoder::FCodecV1Encoder() : Config(FCodecV1EncoderConfig::DefaultConfig())
{
	EncodingContext = { 0 };

	SetupTables();	// Create our huffman tables
}

FCodecV1Encoder::FCodecV1Encoder(const FCodecV1EncoderConfig& EncoderConfig) : Config(EncoderConfig)
{
	EncodingContext = { 0 };

	SetupTables();	// Create our huffman tables
}

void FCodecV1Encoder::EncodeIndexStream(const uint32* Stream, uint64 ElementOffsetBytes, uint32 ElementCount, FStreamEncodingStatistics& Stats)
{	
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer); // Count the number of bytes we are writing

	FRingBuffer<uint32, IndexStreamCodingHistorySize> LastReconstructed(IndexStreamCodingHistorySize); // History holding previously seen indices

	uint8* RawElementData = (uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffsetBytes)
	{
		// Load data
		uint32 Value = *(uint32*)RawElementData;

		uint32 Prediction = LastReconstructed[0]; // Delta coding, best effort
		int32 Residual = Value - Prediction;

		// Write residual	
		WriteInt32(EncodingContext.ResidualIndicesTable, Residual);		

		// Store previous encountered values
		uint32 Reconstructed = Prediction + Residual;
		LastReconstructed.Push(Reconstructed);
	}

	// Gather rate and quality statistics
	float Quality = 0.0f;
	Stats = FStreamEncodingStatistics(ByteCounter.Read(), ElementCount * sizeof(uint32), Quality);
}

void FCodecV1Encoder::EncodePositionStream(const FVector* VertexStream, uint64 VertexElementOffset, uint32 VertexElementCount, const uint32* IndexStream, uint64 IndexElementOffset, uint32 IndexElementCount, FStreamEncodingStatistics& Stats)
{
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer);

	// Quantizer	
	const float QuantizationPrecision = Config.VertexQuantizationPrecision;
	FQuantizerVector3 Quantizer(QuantizationPrecision);
	
	// Bounding box and translation
	const FBox& BoundingBox = EncodingContext.MeshData->BoundingBox;
	FIntVector QuantizedBoxMin = Quantizer.Quantize(BoundingBox.Min); // Quantize the bounds of the bounding box
	FIntVector QuantizedBoxMax = Quantizer.Quantize(BoundingBox.Max);
	FIntVector QuantizedBoxCenter = (QuantizedBoxMax + QuantizedBoxMin) / 2; // Calculate the center of our new quantized bounding box
	FIntVector QuantizedTranslationToCenter = QuantizedBoxCenter; // Translation vector to move the mesh to the center of the quantized bounding box

	// Write header
	FVertexStreamHeader Header;	
	Header.QuantizationPrecision = QuantizationPrecision;
	Header.Translation = QuantizedTranslationToCenter;
	WriteBytes((void*)&Header, sizeof(Header));

	uint32 EncodedVertexCount = 0;
	
	FIntVector Prediction(0, 0, 0);	// Previously seen position
	int64 MaxEncounteredIndex = -1;
		
	FQualityMetric QualityMetric;

	const uint8* RawElementDataIndices = (const uint8*)IndexStream;
	const uint8* RawElementDataVertices = (const uint8*)VertexStream;

	// Walk over indices/triangles
	for (uint32 IndexIdx = 0; IndexIdx < IndexElementCount; ++IndexIdx)
	{
		const uint32 IndexValue = *(const uint32*)RawElementDataIndices;
		RawElementDataIndices += IndexElementOffset;

		if ((int64)IndexValue > MaxEncounteredIndex)
		{
			MaxEncounteredIndex = (int64)IndexValue;

			// Code a newly encountered vertex
			FVector VertexValue = *(FVector*)RawElementDataVertices;
			RawElementDataVertices += VertexElementOffset;

			// Quantize
			FIntVector Encoded = Quantizer.Quantize(VertexValue);

			// Translate to center
			FIntVector EncodedCentered = Encoded - QuantizedTranslationToCenter;

			// Residual to code
			FIntVector Residual = EncodedCentered - Prediction;
				
			// Write residual
			WriteInt32(EncodingContext.ResidualVertexPosTable, Residual.X);
			WriteInt32(EncodingContext.ResidualVertexPosTable, Residual.Y);
			WriteInt32(EncodingContext.ResidualVertexPosTable, Residual.Z);

			EncodedVertexCount++;
				
			// Store previous encountered values
			FIntVector Reconstructed = Prediction + Residual;

			// Calculate error
			FVector DequantReconstructed = Quantizer.Dequantize(Reconstructed);
			QualityMetric.Register(VertexValue, DequantReconstructed);
			Prediction = Reconstructed;
		}
	}

	// Gather rate and quality statistics
	Stats = FStreamEncodingStatistics(ByteCounter.Read(), VertexElementCount * sizeof(FVector), QualityMetric.ReadMSE());
}

void FCodecV1Encoder::EncodeColorStream(const FColor* Stream, uint64 ElementOffsetBytes, uint32 ElementCount, FStreamEncodingStatistics& Stats)
{	
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer);

	FRingBuffer<FIntVector4, ColorStreamCodingHistorySize> ReconstructedHistory(ColorStreamCodingHistorySize, FIntVector4(128, 128, 128, 255)); // Previously seen colors

	const uint8* RawElementData = (const uint8*)Stream;

	// Walk over colors
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffsetBytes)
	{
		// Load data
		FColor& ColorValue = *(FColor*)RawElementData;
		FIntVector4 Value(ColorValue.R, ColorValue.G, ColorValue.B, ColorValue.A);

		FIntVector4 Prediction = ReconstructedHistory[0]; 
		FIntVector4 Residual = FCodecV1SharedTools::SubtractVector4(Value, Prediction); // Residual = Value - Prediction

		// We signal a perfect prediction with a skip bit
		bool bEqual = (Residual == FIntVector4(0, 0, 0, 0));
		int32 SkipBit = bEqual ? 1 : 0;
		WriteBits(SkipBit, 1);

		if (!bEqual)
		{
			// No perfect prediction so write the residuals
			WriteInt32(EncodingContext.ResidualColorTable, Residual.X);
			WriteInt32(EncodingContext.ResidualColorTable, Residual.Y);
			WriteInt32(EncodingContext.ResidualColorTable, Residual.Z);
			WriteInt32(EncodingContext.ResidualColorTable, Residual.W);
		}

		// Decode as the decoder would and keep the result for future prediction
		FIntVector4 Reconstructed = FCodecV1SharedTools::SumVector4(Prediction, Residual); // Decode as the decoder will do
		ReconstructedHistory.Push(Reconstructed);
	}

	// Gather rate and quality statistics
	float Quality = 0.0f; // Lossless
	Stats = FStreamEncodingStatistics(ByteCounter.Read(), ElementCount * sizeof(FColor), Quality);
}

void FCodecV1Encoder::EncodeNormalStream(const FPackedNormal* Stream, uint64 ElementOffsetBytes, uint32 ElementCount, FHuffmanEncodeTable& Table, FStreamEncodingStatistics& Stats)
{
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer);

	uint8 x = 128, y = 128, z = 128, w = 128;
	
	const uint8* RawElementData = (const uint8*)Stream;

	// Walk over colors
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffsetBytes)
	{
		// Load data
		FPackedNormal& NormalValue = *(FPackedNormal*)RawElementData;		
		
		int8 dx = NormalValue.Vector.X - x;
		int8 dy = NormalValue.Vector.Y - y;
		int8 dz = NormalValue.Vector.Z - z;
		int8 dw = NormalValue.Vector.W - w;

		// Write residual	
		WriteSymbol(Table, (uint8)dx);
		WriteSymbol(Table, (uint8)dy);
		WriteSymbol(Table, (uint8)dz);
		WriteSymbol(Table, (uint8)dw);

		x = NormalValue.Vector.X;
		y = NormalValue.Vector.Y;
		z = NormalValue.Vector.Z;
		w = NormalValue.Vector.W;
	}

	// Gather rate and quality statistics
	float Quality = 0.0f; // Lossless
	Stats = FStreamEncodingStatistics(ByteCounter.Read(), ElementCount * sizeof(FPackedNormal), Quality);
}

void FCodecV1Encoder::EncodeUVStream(const FVector2D* Stream, uint64 ElementOffsetBytes, uint32 ElementCount, FStreamEncodingStatistics& Stats)
{
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer);
	
	// Setup quantizer. We set the range to a static [0-1] even though we can get coordinates out of this range: a static range
	// to avoid jittering of coordinates over frames. Note that out of range values (e.g., [0-6]) will quantize fine, but will take 
	// 'UVQuantizationBitRange' bits for their fraction part
	const int32 BitRange = Config.UVQuantizationBitRange;
	FVector2D Range(1.0f, 1.0f);	
	FQuantizerVector2 Quantizer(Range, BitRange);

	// Write header
	FUVStreamHeader Header;
	Header.QuantizationBits = BitRange;	
	Header.Range = Range;
	WriteBytes((void*)&Header, sizeof(Header));
		
	FRingBuffer<FIntVector, UVStreamCodingHistorySize> ReconstructedHistory(UVStreamCodingHistorySize, FIntVector(0, 0, 0)); // Previously seen UVs
	FQualityMetric2D QualityMetric;

	const uint8* RawElementData = (const uint8*)Stream;

	// Walk over UVs, note, we can get better results if we walk the indices and use knowledge on the triangles to predict the UVs
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffsetBytes)
	{
		// Load data
		FVector2D& UVValue = *(FVector2D*)RawElementData;

		FIntVector Encoded = Quantizer.Quantize(UVValue);

		FIntVector Prediction = ReconstructedHistory[0]; // Delta coding
		FIntVector Residual = Encoded - Prediction;

		// Write residual	
		WriteInt32(EncodingContext.ResidualUVTable, Residual.X);
		WriteInt32(EncodingContext.ResidualUVTable, Residual.Y);

		// Store previous encountered values
		FIntVector Reconstructed = Prediction + Residual;
		ReconstructedHistory.Push(Reconstructed);

		// Calculate error
		FVector2D DequantReconstructed = Quantizer.Dequantize(Reconstructed);
		QualityMetric.Register(UVValue, DequantReconstructed);
	}

	// Gather rate and quality statistics
	Stats = FStreamEncodingStatistics(ByteCounter.Read(), ElementCount * sizeof(FVector2D), QualityMetric.ReadMSE());
}

void FCodecV1Encoder::EncodeMotionVectorStream(const FVector* Stream, uint64 ElementOffsetBytes, uint32 ElementCount, FStreamEncodingStatistics& Stats)
{
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer);

	float QuantizationPrecision = Config.VertexQuantizationPrecision; // We use the same precision as the one used for the positions
	FQuantizerVector3 Quantizer(Config.VertexQuantizationPrecision);

	// Write header
	FMotionVectorStreamHeader Header;
	Header.QuantizationPrecision = QuantizationPrecision;
	WriteBytes((void*)&Header, sizeof(Header));

	FRingBuffer<FIntVector, MotionVectorStreamCodingHistorySize> ReconstructedHistory(MotionVectorStreamCodingHistorySize, FIntVector(0, 0, 0)); // Previously seen UVs
	FQualityMetric QualityMetric;

	const uint8* RawElementData = (const uint8*)Stream;

	// Walk over UVs, note, we can get better results if we walk the indices and use knowledge on the triangles to predict the UVs
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffsetBytes)
	{
		// Load data
		FVector& MVValue = *(FVector*)RawElementData;

		FIntVector Encoded = Quantizer.Quantize(MVValue);

		FIntVector Prediction = ReconstructedHistory[0]; // Delta coding
		FIntVector Residual = Encoded - Prediction;

		// Write residual	
		WriteInt32(EncodingContext.ResidualMotionVectorTable, Residual.X);
		WriteInt32(EncodingContext.ResidualMotionVectorTable, Residual.Y);
		WriteInt32(EncodingContext.ResidualMotionVectorTable, Residual.Z);

		// Store previous encountered values
		FIntVector Reconstructed = Prediction + Residual;
		ReconstructedHistory.Push(Reconstructed);

		// Calculate error
		FVector DequantReconstructed = Quantizer.Dequantize(Reconstructed);
		QualityMetric.Register(MVValue, DequantReconstructed);
	}

	// Gather rate and quality statistics
	Stats = FStreamEncodingStatistics(ByteCounter.Read(), ElementCount * sizeof(FVector2D), QualityMetric.ReadMSE());
}
void FCodecV1Encoder::WriteCodedStreamDescription()
{
	const FGeometryCacheVertexInfo& VertexInfo = EncodingContext.MeshData->VertexInfo;

	WriteBits(VertexInfo.bHasTangentX ? 1 : 0, 1);
	WriteBits(VertexInfo.bHasTangentZ ? 1 : 0, 1);
	WriteBits(VertexInfo.bHasUV0 ? 1 : 0, 1);
	WriteBits(VertexInfo.bHasColor0 ? 1 : 0, 1);
	WriteBits(VertexInfo.bHasMotionVectors ? 1 : 0, 1);

	WriteBits(VertexInfo.bConstantUV0 ? 1 : 0, 1);
	WriteBits(VertexInfo.bConstantColor0 ? 1 : 0, 1);
	WriteBits(VertexInfo.bConstantIndices ? 1 : 0, 1);
}

bool FCodecV1Encoder::EncodeFrameData(FMemoryWriter& Writer, const FGeometryCacheCodecEncodeArguments &Args)
{
/*
#ifdef DEBUG_DUMP_FRAMES
	{
		// Dump a frame to disk for codec development purposes
		static int32 FrameIndex = 0;
		FString Path = TEXT("E:\\");
		FString FileName = Path + TEXT("frame_") + FString::FormatAsNumber(FrameIndex) + TEXT("_raw.dump"); // F:\\frame_%i_raw.dump
		CodecV1Test::WriteRawMeshDataToFile(Args.MeshData, FileName);
		FrameIndex++;		
	}
#endif
*/
	const FGeometryCacheMeshData& MeshData = Args.MeshData;

	FExperimentTimer CodingTime;

	// Two-pass encoding: first we collect statistics and don't write any bits, second, we use the collected statistics and write our bitstream
	bool bPerformPrepass = true; // For now we always perform a prepass. In the future, we can e.g., do a prepass only at the start of a group-of-frames.
	if (bPerformPrepass)
	{
		// First pass, collect statistics
		bool bSuccess = EncodeFrameData(Writer, MeshData, /*bPrePass=*/true);
		if (!bSuccess)
		{
			return false;
		}
	}

	// Second pass, use statistics and actually write the bitstream
	bool bSuccess = EncodeFrameData(Writer, MeshData, /*bPrePass=*/false);
	if (!bSuccess)
	{
		return false;
	}

	// Additional stats
	Statistics.DurationMs = CodingTime.Get();
	Statistics.All.Quality = 0.0f;	
	Statistics.NumVertices = MeshData.Positions.Num();
	UE_LOG(LogGeoCaStreamingCodecV1, Log, TEXT("Compressed %u vertices, %u bytes to %u bytes in %.2f milliseconds (%.2f ratio), quantizer precision: %.2f units."), 
		Statistics.NumVertices, Statistics.All.RawNumBytes, Statistics.All.CodedNumBytes, Statistics.DurationMs, Statistics.All.CompressionRatio, Config.VertexQuantizationPrecision);

	return true;
}

bool FCodecV1Encoder::EncodeFrameData(FMemoryWriter& Writer, const FGeometryCacheMeshData& MeshData, bool bPrepass)
{	
	FHuffmanBitStreamWriter BitWriter;

	EncodingContext.MeshData = &MeshData;
	EncodingContext.Writer = &BitWriter;
	EncodingContext.bPrepass = bPrepass;

	SetPrepass(bPrepass); // Tell our tables we are collecting or using statistics

	if (!bPrepass)
	{
		// Write in bitstream which streams are embedded
		WriteCodedStreamDescription();

		// Write tables on the second pass, when we are writing the bitstream
		WriteTables();
	}

	const TArray<FVector>& Positions = MeshData.Positions;
	const TArray<FVector2D>& TextureCoordinates = MeshData.TextureCoordinates;
	const TArray<FPackedNormal>& TangentsX = MeshData.TangentsX;
	const TArray<FPackedNormal>& TangentsZ = MeshData.TangentsZ;
	const TArray<FColor>& Colors = MeshData.Colors;

	const TArray<uint32>& Indices = MeshData.Indices;
	const TArray<FVector>& MotionVectors = MeshData.MotionVectors;	

	{
		// Check if indices are referenced in order, i.e. if a previously unreferenced vertex is referenced by the index 
		// list it's id will always be the next unreferenced id instead of some random unused id. E.g., ok: 1, 2, 3, 2, 4, not ok: 1, 2, 4
		// This is a requirement of the encoder and should be enforced by the preprocessor. 
		uint32 MaxIndex = 0;
		for (int32 i = 0; i < Indices.Num(); ++i)
		{
			bool bIsInOrder = Indices[i] <= MaxIndex + 1;
			checkf(bIsInOrder, TEXT("Vertices are not referenced in index buffer in order. Please make sure the preprocessor has processed the mesh such that vertexes are referenced in-order, i.e. if a previously unreferenced vertex is referenced by the index list it's id will always be the next unreferenced id instead of some random unused id."));
			MaxIndex = FMath::Max(Indices[i], MaxIndex);
		}
	}
	FBitstreamWriterByteCounter TotalByteCounter(EncodingContext.Writer);
	const FGeometryCacheVertexInfo& VertexInfo = MeshData.VertexInfo;

	// Encode streams	
	if (!VertexInfo.bConstantIndices)
	{
		EncodeIndexStream(&Indices[0], Indices.GetTypeSize(), Indices.Num(), Statistics.Indices);
	}
	EncodePositionStream(&Positions[0], Positions.GetTypeSize(), Positions.Num(), &Indices[0], Indices.GetTypeSize(), Indices.Num(), Statistics.Vertices);
	if (VertexInfo.bHasColor0)
	{
		EncodeColorStream(&Colors[0], Colors.GetTypeSize(), Colors.Num(), Statistics.Colors);
	}
	if (VertexInfo.bHasTangentX)
	{
		EncodeNormalStream(&TangentsX[0], TangentsX.GetTypeSize(), TangentsX.Num(), EncodingContext.ResidualNormalTangentXTable, Statistics.TangentX);
	}
	if (VertexInfo.bHasTangentZ)
	{
		EncodeNormalStream(&TangentsZ[0], TangentsZ.GetTypeSize(), TangentsZ.Num(), EncodingContext.ResidualNormalTangentZTable, Statistics.TangentY);
	}
	if (VertexInfo.bHasUV0)
	{
		EncodeUVStream(&TextureCoordinates[0], TextureCoordinates.GetTypeSize(), TextureCoordinates.Num(), Statistics.TexCoords);
	}
	if (VertexInfo.bHasMotionVectors)
	{
		checkf(MotionVectors.Num() > 0, TEXT("No motion vectors while VertexInfo states otherwise"));
		EncodeMotionVectorStream(&MotionVectors[0], MotionVectors.GetTypeSize(), MotionVectors.Num(), Statistics.MotionVectors);		
	}
	
	BitWriter.Close();
	
	if (!bPrepass)
	{
		// Write out bitstream
		FCodedFrameHeader Header = { 0 };
		Header.Magic = 123;
		Header.VertexCount = (uint32)Positions.Num();
		Header.IndexCount = (uint32)Indices.Num();
		uint32 PayloadSize = BitWriter.GetNumBytes();
		Header.PayloadSize = PayloadSize;
		Writer.Serialize(&Header, sizeof(Header));	// Write header				
		Writer << MeshData.BatchesInfo;	// Uncompressed data: bounding box & material list
		Writer << MeshData.BoundingBox;
		Writer.Serialize((void*)BitWriter.GetBytes().GetData(), PayloadSize);	// Write payload
	}	

	// Gather stats for all streams
	const uint32 TotalRawSize =
		sizeof(uint32) * Indices.Num()	// Indices
		+ sizeof(FVector) * Positions.Num() // Vertices
		+ sizeof(FColor) * Colors.Num() // Colors
		+ sizeof(FPackedNormal) * TangentsX.Num() // TangentX
		+ sizeof(FPackedNormal) * TangentsZ.Num() // TangentY
		+ sizeof(FVector2D) * TextureCoordinates.Num(); // UVs
	Statistics.All = FStreamEncodingStatistics(TotalByteCounter.Read() + sizeof(FCodedFrameHeader), TotalRawSize, 0.0f);
		
	return true;
}


void FCodecV1Encoder::WriteBytes(const void* Data, int64 NumBytes)
{
	if (EncodingContext.bPrepass)
	{
		return; // Nothing gets actually written in the prepass phase
	}

	const uint8* ByteData = (const uint8*)Data;
	
	for (int64 ByteIndex = 0; ByteIndex < NumBytes; ++ByteIndex)
	{
		uint32 ByteValue = *ByteData++;
		EncodingContext.Writer->Write(ByteValue, 8);
	}
}


void FCodecV1Encoder::WriteInt32(FHuffmanEncodeTable& ValueTable, int32 Value)
{
	// It is impractical to entropy code an entire integer, so we split it into an entropy coded magnitude followed by a number of raw bits.
	// The reasoning is that usually most of the redundancy is in the magnitude of the number, not the exact value.
	
	// Positive values are encoded as the index k of the first 1-bit (at most 30) followed by the remaining k bits encoded as raw bits.
	// Negative values are handled symmetrically, but using the index of the first 0-bit.
	// With one symbol for every bit length and sign, the set of reachable number is 2 * (2^0 + 2^1 + ... + 2^30) = 2 * (2^31 - 1) = 2^32 - 2
	// To cover all 2^32 possible integer values, we have use separate codes for the remaining two symbols (with no raw bits).
	// The total number of symbols is 2 * 31 + 2 = 64
	
	if (Value >= -2 && Value <= 1)
	{
		// 4 center values have no raw bits. One more negative values than positive,
		// so we have an equal number of positive and negative values remaining.
		WriteSymbol(ValueTable, Value + 2);	// [-2, 1] -> [0, 3]
	}
	else
	{
		// At least one raw bit.
		if (Value >= 0)
		{
			// Value >= 2
			int32 NumRawBits = HighestSetBit(Value);	// Find first 1-bit. 1 <= NumRawBits <= 30.
			int32 Packed = 2 + NumRawBits * 2;			// First positive code is 4
			WriteSymbol(ValueTable, Packed);
			int32 RawBits = Value - (1 << NumRawBits);
			WriteBits(RawBits, NumRawBits);
		}
		else
		{
			// Value <= -3
			int32 NumRawBits = HighestSetBit(~Value);	// Find first 0-bit. 1 <= NumRawBits <= 30.
			int32 Packed = 3 + NumRawBits * 2;			// First negative code is 5
			WriteSymbol(ValueTable, Packed);
			int32 RawBits = Value & ~(0xFFFFFFFFu << NumRawBits);
			WriteBits(RawBits, NumRawBits);
		}
	}
	
}

void FCodecV1Encoder::SetupTables()
{
	// Initialize Huffman tables.
	// Most tables store 32-bit integers stored with a bit-length;raw value scheme. Some store specific symbols.
	EncodingContext.ResidualIndicesTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualVertexPosTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualColorTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualNormalTangentXTable.Initialize(HuffmanTableInt8SymbolCount);
	EncodingContext.ResidualNormalTangentZTable.Initialize(HuffmanTableInt8SymbolCount);
	EncodingContext.ResidualUVTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualMotionVectorTable.Initialize(HuffmanTableInt32SymbolCount);
	// Add additional tables here
}

void FCodecV1Encoder::SetPrepass(bool bPrepass)
{
	const FGeometryCacheVertexInfo& VertexInfo = EncodingContext.MeshData->VertexInfo;

	// When bPrepass is set to true, the tables gather statistics about the data they encounter and do not write 
	// any output bits.When set to false, they build the internal symbol representations and will write bits.
	if ( !VertexInfo.bConstantIndices)
		EncodingContext.ResidualIndicesTable.SetPrepass(bPrepass);	
	EncodingContext.ResidualVertexPosTable.SetPrepass(bPrepass);
	if (VertexInfo.bHasColor0)
	{
		EncodingContext.ResidualColorTable.SetPrepass(bPrepass);
	}
	if (VertexInfo.bHasTangentX)
	{
		EncodingContext.ResidualNormalTangentXTable.SetPrepass(bPrepass);
	}
	if (VertexInfo.bHasTangentZ)
	{
		EncodingContext.ResidualNormalTangentZTable.SetPrepass(bPrepass);
	}
	if (VertexInfo.bHasUV0)
	{
		EncodingContext.ResidualUVTable.SetPrepass(bPrepass);
	}
	if (VertexInfo.bHasMotionVectors)
	{
		EncodingContext.ResidualMotionVectorTable.SetPrepass(bPrepass);
	}
	
	// Add additional tables here
}

void FCodecV1Encoder::WriteTables()
{
	// Write all our Huffman tables to the bitstream. This gets typically done after a SetPrepass(false) call sets
	// up the tables for their first use, and before symbols are written.
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer); // Count the bytes we are going to write
	FHuffmanBitStreamWriter& Writer = *EncodingContext.Writer;	
	const FGeometryCacheVertexInfo& VertexInfo = EncodingContext.MeshData->VertexInfo;

	if ( !VertexInfo.bConstantIndices)
		EncodingContext.ResidualIndicesTable.Serialize(Writer);	
	EncodingContext.ResidualVertexPosTable.Serialize(Writer);
	if (VertexInfo.bHasColor0)
	{
		EncodingContext.ResidualColorTable.Serialize(Writer);
	}
	if (VertexInfo.bHasTangentX)
	{
		EncodingContext.ResidualNormalTangentXTable.Serialize(Writer);
	}
	if (VertexInfo.bHasTangentZ)
	{
		EncodingContext.ResidualNormalTangentZTable.Serialize(Writer);
	}
	if (VertexInfo.bHasUV0)
	{
		EncodingContext.ResidualUVTable.Serialize(Writer);
	}	
	if (VertexInfo.bHasMotionVectors)
	{
		EncodingContext.ResidualMotionVectorTable.Serialize(Writer);
	}
	// Add additional tables here

	Statistics.HuffmanTablesNumBytes = ByteCounter.Read();
}

#endif // WITH_EDITOR

FCodecV1Decoder::FCodecV1Decoder()
{
	// Precalculate table mapping symbol index to non-raw bits. ((Sign ? -2 : 1) << NumRawBits)
	for (int32 NumRawBits = 1; NumRawBits <= 30; NumRawBits++)
	{
		for (int32 Sign = 0; Sign <= 1; Sign++)
		{
			HighBitsLUT[2 + Sign + NumRawBits * 2] = (Sign ? -2 : 1) << NumRawBits;
		}
	}	
}

void FCodecV1Decoder::DecodeIndexStream(uint32* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	const uint8* RawElementData = (const uint8*)Stream;

	uint32 Value = 0;
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset)
	{
		// Read coded residual
		int32 DecodedResidual = ReadInt32(DecodingContext.ResidualIndicesTable);
		Value += DecodedResidual;

		// Save result to our list
		*(uint32*)RawElementData = Value;
	}
}

void FCodecV1Decoder::DecodeMotionVectorStream(FVector* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	// Read header
	FMotionVectorStreamHeader Header;
	ReadBytes((void*)&Header, sizeof(Header));

	FQuantizerVector3 Quantizer(Header.QuantizationPrecision); // We quantize MVs to a certain precision just like the positions

	FIntVector QuantizedValue(0, 0, 0);

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset) // Walk MVs
	{
		// Read coded residual
		FIntVector DecodedResidual;
		DecodedResidual.X = ReadInt32(DecodingContext.ResidualMotionVectorTable);
		DecodedResidual.Y = ReadInt32(DecodingContext.ResidualMotionVectorTable);
		DecodedResidual.Z = ReadInt32(DecodingContext.ResidualMotionVectorTable);

		QuantizedValue += DecodedResidual;
		*(FVector*)RawElementData = Quantizer.Dequantize(QuantizedValue);
	}
}

void FCodecV1Decoder::DecodeUVStream(FVector2D* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	// Read header
	FUVStreamHeader Header;
	ReadBytes((void*)&Header, sizeof(Header));

	FQuantizerVector2 Quantizer(Header.Range, Header.QuantizationBits); // We quantize UVs to a number of bits, set in the bitstream header
	
	FIntVector QuantizedValue(0, 0, 0);

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset) // Walk UVs
	{
		// Read coded residual
		FIntVector DecodedResidual;
		DecodedResidual.X = ReadInt32(DecodingContext.ResidualUVTable);
		DecodedResidual.Y = ReadInt32(DecodingContext.ResidualUVTable);

		QuantizedValue += DecodedResidual;
		*(FVector2D*)RawElementData = Quantizer.Dequantize(QuantizedValue);
	}
}

void FCodecV1Decoder::DecodeNormalStream(FPackedNormal* Stream, uint64 ElementOffset, uint32 ElementCount, FHuffmanDecodeTable& Table)
{
	uint8 x = 128, y = 128, z = 128, w = 128;

	const uint8* RawElementData = (const uint8*)Stream;

	check(HUFFMAN_MAX_CODE_LENGTH * 4 <= MINIMUM_BITS_AFTER_REFILL);	// Make sure we can safely decode all 4 symbols with a single refill

	FHuffmanBitStreamReader& Reader = *DecodingContext.Reader;
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset) // Walk normals
	{
		// Read coded residual
		Reader.Refill();
		x += Table.DecodeNoRefill(Reader);	
		y += Table.DecodeNoRefill(Reader);
		z += Table.DecodeNoRefill(Reader);
		w += Table.DecodeNoRefill(Reader);
		
		FPackedNormal* Value = (FPackedNormal*)RawElementData;
		Value->Vector.X = x;
		Value->Vector.Y = y;
		Value->Vector.Z = z;
		Value->Vector.W = w;
	}
}

void FCodecV1Decoder::DecodeColorStream(FColor* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	FIntVector4 QuantizedValue(128, 128, 128, 255);

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset)
	{
		FIntVector4 DecodedResidual(0, 0, 0, 0);

		int32 SkipBit = ReadBits(1); // 1: Perfect prediction, nothing coded, 0: we have coded residuals

		if (SkipBit != 1) 
		{
			// Prediction not perfect, residual were coded
			int32 DecodedResidualR = ReadInt32(DecodingContext.ResidualColorTable);
			int32 DecodedResidualG = ReadInt32(DecodingContext.ResidualColorTable);
			int32 DecodedResidualB = ReadInt32(DecodingContext.ResidualColorTable);
			int32 DecodedResidualA = ReadInt32(DecodingContext.ResidualColorTable);

			DecodedResidual = FIntVector4(DecodedResidualR, DecodedResidualG, DecodedResidualB, DecodedResidualA);
			QuantizedValue = FCodecV1SharedTools::SumVector4(QuantizedValue, DecodedResidual);
		}
																			
		FColor* Value = (FColor*)RawElementData; // Save result to our list
		Value->R = QuantizedValue.X;
		Value->G = QuantizedValue.Y;
		Value->B = QuantizedValue.Z;
		Value->A = QuantizedValue.W;
	}
}

void FCodecV1Decoder::DecodePositionStream(const uint32* IndexStream, uint64 IndexElementOffset, uint32 IndexElementCount, FVector* VertexStream, uint64 VertexElementOffset, uint32 MaxVertexElementCount)
{
	checkf(IndexElementCount > 0, TEXT("You cannot decode vertex stream before the index stream was decoded"));

	// Read header
	FVertexStreamHeader Header;
	ReadBytes((void*)&Header, sizeof(Header));

	FQuantizerVector3 Quantizer(Header.QuantizationPrecision);
	
	int64 MaxEncounteredIndex = -1; // We rely on indices being references in order, a requirement of the encoder and enforced by the preprocessor
	uint32 DecodedVertexCount = 0;

	const uint8* RawElementDataIndices = (const uint8*)IndexStream;
	const uint8* RawElementDataVertices = (const uint8*)VertexStream;

	FIntVector QuantizedValue(0, 0, 0);

	// Walk over indices/triangles
	for (uint32 IndexIdx = 0; IndexIdx < IndexElementCount; ++IndexIdx)
	{
		const uint32 IndexValue = *(const uint32*)RawElementDataIndices;
		RawElementDataIndices += IndexElementOffset;

		if ((int64)IndexValue > MaxEncounteredIndex)
		{
			MaxEncounteredIndex = (int64)IndexValue;
			checkf(DecodedVertexCount < MaxVertexElementCount, TEXT("Encountering more vertices than we have encoded. Encoding and decoding algorithms don't seem to match. Please make sure the preprocessor has processed the mesh such that vertexes are referenced in-order, i.e. if a previously unreferenced vertex is referenced by the index list it's id will always be the next unreferenced id instead of some random unused id."));

			// Read coded residual
			const FIntVector DecodedResidual = { ReadInt32(DecodingContext.ResidualVertexPosTable), ReadInt32(DecodingContext.ResidualVertexPosTable), ReadInt32(DecodingContext.ResidualVertexPosTable) };
			DecodedVertexCount++;

			QuantizedValue += DecodedResidual;

			// Save result to our list
			FVector* Value = (FVector*)RawElementDataVertices;
			*Value = Quantizer.Dequantize(QuantizedValue + Header.Translation);
			RawElementDataVertices += VertexElementOffset;		
		}
	}
}

void FCodecV1Decoder::SetupAndReadTables()
{
	// Initialize and read Huffman tables from the bitstream
	FHuffmanBitStreamReader& Reader = *DecodingContext.Reader;	
	const FGeometryCacheVertexInfo& VertexInfo = DecodingContext.MeshData->VertexInfo;
		
	if (!VertexInfo.bConstantIndices)
		DecodingContext.ResidualIndicesTable.Initialize(Reader);	
	DecodingContext.ResidualVertexPosTable.Initialize(Reader);
	
	if (VertexInfo.bHasColor0)
	{
		DecodingContext.ResidualColorTable.Initialize(Reader);		
	}
	if (VertexInfo.bHasTangentX)
	{
		DecodingContext.ResidualNormalTangentXTable.Initialize(Reader);
	}
	if (VertexInfo.bHasTangentZ)
	{
		DecodingContext.ResidualNormalTangentZTable.Initialize(Reader);
	}
	if (VertexInfo.bHasUV0)
	{
		DecodingContext.ResidualUVTable.Initialize(Reader);
	}
	if (VertexInfo.bHasMotionVectors)
	{
		DecodingContext.ResidualMotionVectorTable.Initialize(Reader);
	}
	
	// Add additional tables here
}

void FCodecV1Decoder::ReadCodedStreamDescription()
{	
	FGeometryCacheVertexInfo& VertexInfo = DecodingContext.MeshData->VertexInfo;
	
	VertexInfo = FGeometryCacheVertexInfo();
	VertexInfo.bHasTangentX = (ReadBits(1) == 1);
	VertexInfo.bHasTangentZ = (ReadBits(1) == 1);
	VertexInfo.bHasUV0 = (ReadBits(1) == 1);
	VertexInfo.bHasColor0 = (ReadBits(1) == 1);
	VertexInfo.bHasMotionVectors = (ReadBits(1) == 1);

	VertexInfo.bConstantUV0 = (ReadBits(1) == 1);
	VertexInfo.bConstantColor0 = (ReadBits(1) == 1);
	VertexInfo.bConstantIndices = (ReadBits(1) == 1);
}

DECLARE_CYCLE_STAT(TEXT("SetupAndReadTables"), STAT_SetupAndReadTables, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("DecodeIndexStream"), STAT_DecodeIndexStream, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("DecodePositionStream"), STAT_DecodePositionStream, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("DecodeColorStream"), STAT_DecodeColorStream, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("DecodeTangentXStream"), STAT_DecodeTangentXStream, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("DecodeTangentZStream"), STAT_DecodeTangentZStream, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("DecodeUVStream"), STAT_DecodeUVStream, STATGROUP_GeometryCache);


bool FCodecV1Decoder::DecodeFrameData(FBufferReader& Reader, FGeometryCacheMeshData &OutMeshData)
{
	FExperimentTimer DecodingTime;

	// Read stream header
	FCodedFrameHeader Header;
	Reader.Serialize(&Header, sizeof(Header));

	if (Header.Magic != 123)
	{
		UE_LOG(LogGeoCaStreamingCodecV1, Error, TEXT("Incompatible bitstream found"));
		return false;
	}
		
	// Read uncompressed data: bounding box & material list
	Reader << OutMeshData.BatchesInfo; 
	Reader << OutMeshData.BoundingBox;

	DecodingContext = { 0 };
	DecodingContext.MeshData = &OutMeshData;	
	
	// Read the payload in memory to pass to the bit reader
	TArray<uint8> Bytes;
	Bytes.AddUninitialized(Header.PayloadSize + 16);	// Overallocate by 16 bytes to ensure BitReader can safely perform uint64 reads.
	Reader.Serialize(Bytes.GetData(), Header.PayloadSize);
	FHuffmanBitStreamReader BitReader(Bytes.GetData(), Bytes.Num());
	DecodingContext.Reader = &BitReader;

	// Read which vertex attributes are in the bit stream
	ReadCodedStreamDescription();

	// Restore entropy coding contexts
	{
		SCOPE_CYCLE_COUNTER(STAT_SetupAndReadTables);
		SetupAndReadTables();
	}
	
	
	{
		// Decode streams
		
		TArray<uint32>& Indices = OutMeshData.Indices;
		if (!OutMeshData.VertexInfo.bConstantIndices)
		{
			OutMeshData.Indices.Empty(Header.IndexCount);
			OutMeshData.Indices.AddUninitialized(Header.IndexCount);
			SCOPE_CYCLE_COUNTER(STAT_DecodeIndexStream);
			DecodeIndexStream(&Indices[0], Indices.GetTypeSize(), Header.IndexCount);
		}
		
		TArray<FVector>& Positions = OutMeshData.Positions;
		OutMeshData.Positions.Empty(Header.VertexCount);
		OutMeshData.Positions.AddZeroed(Header.VertexCount);
		{
			SCOPE_CYCLE_COUNTER(STAT_DecodePositionStream);
			DecodePositionStream(&Indices[0], Indices.GetTypeSize(), Indices.Num(), &Positions[0], Positions.GetTypeSize(), Header.VertexCount);
		}

		OutMeshData.Colors.Empty(Header.VertexCount);
		OutMeshData.Colors.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasColor0)
		{
			TArray<FColor>& Colors = OutMeshData.Colors;
			SCOPE_CYCLE_COUNTER(STAT_DecodeColorStream);
			DecodeColorStream(&Colors[0], Colors.GetTypeSize(), Header.VertexCount);
		}

		OutMeshData.TangentsX.Empty(Header.VertexCount);
		OutMeshData.TangentsX.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasTangentX)
		{
			TArray<FPackedNormal>& TangentsX = OutMeshData.TangentsX;
			SCOPE_CYCLE_COUNTER(STAT_DecodeTangentXStream);
			DecodeNormalStream(&TangentsX[0], TangentsX.GetTypeSize(), Header.VertexCount, DecodingContext.ResidualNormalTangentXTable);
		}

		OutMeshData.TangentsZ.Empty(Header.VertexCount);
		OutMeshData.TangentsZ.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasTangentZ)
		{
			TArray<FPackedNormal>& TangentsZ = OutMeshData.TangentsZ;
			SCOPE_CYCLE_COUNTER(STAT_DecodeTangentZStream);
			DecodeNormalStream(&TangentsZ[0], TangentsZ.GetTypeSize(), Header.VertexCount, DecodingContext.ResidualNormalTangentZTable);
		}

		OutMeshData.TextureCoordinates.Empty(Header.VertexCount);
		OutMeshData.TextureCoordinates.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasUV0)
		{
			TArray<FVector2D>& TextureCoordinates = OutMeshData.TextureCoordinates;
			SCOPE_CYCLE_COUNTER(STAT_DecodeUVStream);
			DecodeUVStream(&TextureCoordinates[0], TextureCoordinates.GetTypeSize(), Header.VertexCount);
		}

		TArray<FVector>& MotionVectors = OutMeshData.MotionVectors;
		OutMeshData.MotionVectors.Empty(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasMotionVectors)
		{
			OutMeshData.MotionVectors.AddUninitialized(Header.VertexCount);
			DecodeMotionVectorStream(&MotionVectors[0], MotionVectors.GetTypeSize(), Header.VertexCount);
		}

		if (CVarCodecDebug.GetValueOnAnyThread() == 1)
		{
			const float TimeFloat = DecodingTime.Get();
			UE_LOG(LogGeoCaStreamingCodecV1, Log, TEXT("Decoded frame with %i vertices in %.2f milliseconds."), Positions.Num(), TimeFloat);
		}
	}
	DecodingContext.Reader = NULL;

	return true;
}

void FCodecV1Decoder::ReadBytes(void* Data, uint32 NumBytes)
{
	uint8* ByteData = (uint8*)Data;

	for (int64 ByteIndex = 0; ByteIndex < NumBytes; ++ByteIndex)
	{
		const uint32 ByteValue = DecodingContext.Reader->Read(8);
		*ByteData++ = ByteValue & 255;
	}
}

int32 FCodecV1Decoder::ReadInt32(FHuffmanDecodeTable& ValueTable)
{
	// See write WriteInt32 for encoding details.
	const int32 Packed = ReadSymbol(ValueTable);
	if (Packed < 4)
	{
		// [-2, 1] coded directly with no additional raw bits
		return Packed - 2;
	}
	else
	{
		// At least one raw bit.
		int32 NumRawBits = (Packed - 2) >> 1;
		return ReadBitsNoRefill(NumRawBits) + HighBitsLUT[Packed];
	}
}
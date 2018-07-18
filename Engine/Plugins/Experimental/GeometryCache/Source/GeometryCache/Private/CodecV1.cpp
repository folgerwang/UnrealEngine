// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
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

int32 FCodecV1SharedTools::EstimateCodingCost(const FIntVector& Value)
{
	// Just use the absolute sizes of the components as a representative for a relative coding cost of this vector
	return FMath::Abs(Value.X) + FMath::Abs(Value.Y) + FMath::Abs(Value.Z);
}

FIntVector FCodecV1SharedTools::PredictVertex(uint32 CornerIndex, uint32 PreviousTriangleRotationOffsets[3], FRingBuffer<FIntVector, VertexStreamCodingVertexHistorySize>& ReconstructedVertexHistory, uint32 Mode)
{
	FIntVector Prediction;
	
	switch (Mode)
	{
	case 0:
		{
			// Mode 0: delta coding, last visited vertex
			Prediction = ReconstructedVertexHistory[0];
		}
		break;
	case 1:
		{
			// Mode 1: delta coding, second to last visited vertex
			Prediction = ReconstructedVertexHistory[1]; 		
		}
		break;
	case 2:
		{
			// Mode 2: delta coding, third to last visited vertex
			Prediction = ReconstructedVertexHistory[2];			
		}
		break;
	case 3:
		{
			// Mode 3: Parallelogram prediction: P = V + U - W				
			// Calculate offsets of the previous triangle in our vertex history buffer and compensate for possible rotation of the triangle		
			const uint32 OffsetV = CornerIndex + 2 - PreviousTriangleRotationOffsets[0]; // Shared edge vertex 1
			const uint32 OffsetU = CornerIndex + 2 - PreviousTriangleRotationOffsets[1]; // Shared edge vertex 2
			const uint32 OffsetW = CornerIndex + 2 - PreviousTriangleRotationOffsets[2]; // non-shared vertex
			
			const FIntVector PredV = ReconstructedVertexHistory[OffsetV];
			const FIntVector PredU = ReconstructedVertexHistory[OffsetU];
			const FIntVector PredW = ReconstructedVertexHistory[OffsetW];
			Prediction = PredV + PredU - PredW; 			
		}		
		break;
	default:
		{
			checkf(false, TEXT("Unknown prediction mode, corrupt bitstream or encoder and decoder are out of sync."));
		}
	}
	
	return Prediction;
}

bool FCodecV1SharedTools::FindTrianglesSharedEdge(const uint32 Triangle1[3], const uint32 Triangle2[3], uint32 Triangle1RotationOffsets[3])
{
	// e.g. Triangle1 = {3, 4, 5}, Triangle2 = {5, 6, 3}
	//      Returns: true, orderedTriangle1 = {2, 0, 1}
	//      The shared edge is (orderedTriangle[0], orderedTriangle1[1]), the non-edge vertex is orderedTriangle1[2]

	const bool bIsShared[3] =
	{
		Triangle1[0] == Triangle2[0] || Triangle1[0] == Triangle2[1] || Triangle1[0] == Triangle2[2],
		Triangle1[1] == Triangle2[0] || Triangle1[1] == Triangle2[1] || Triangle1[1] == Triangle2[2],
		Triangle1[2] == Triangle2[0] || Triangle1[2] == Triangle2[1] || Triangle1[2] == Triangle2[2]
	};
	
	if (bIsShared[0] && bIsShared[1])
	{
		Triangle1RotationOffsets[0] = 0;
		Triangle1RotationOffsets[1] = 1;
		Triangle1RotationOffsets[2] = 2;
		return true;
	}

	if (bIsShared[1] && bIsShared[2])
	{
		Triangle1RotationOffsets[0] = 1;
		Triangle1RotationOffsets[1] = 2;
		Triangle1RotationOffsets[2] = 0;
		return true;
	}

	if (bIsShared[2] && bIsShared[0])
	{
		Triangle1RotationOffsets[0] = 2;
		Triangle1RotationOffsets[1] = 0;
		Triangle1RotationOffsets[2] = 1;
		return true;
	}

	return false;
}
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

FIntVector FCodecV1Encoder::FindModeAndPredictVertex(const FIntVector& ValueToCode, uint32 CornerIndex, uint32 PreviousTriangleRotationOffsets[3], FRingBuffer<FIntVector, VertexStreamCodingVertexHistorySize>& ReconstructedVertexHistory, uint32& ChosenMode)
{
	// We perform multiple prediction methods and return prediction and method. 
	// Methods comprise of: delta coding (previous or n-th last value), parallelogram prediction on last seen triangle.
	// Whether the parallelogram prediction perform a good prediction depends on the mesh optimization the importer/preprocessor did and how much 
	// coherency there is between successive triangles. No matter the input, the algorithm will never fail. Worst case scenario, prediction is bad 
	// and we code more bits.	
	uint32 BestMode = 0;
	uint32 MinimumCost = UINT32_MAX;
	FIntVector BestPrediction;
		
	for (uint32 Mode = 0; Mode < VertexPredictionModeCount; ++Mode)
	{
		// Predict according to this mode
		FIntVector Prediction = FCodecV1SharedTools::PredictVertex(CornerIndex, PreviousTriangleRotationOffsets, ReconstructedVertexHistory, Mode);

		// Estimate the cost of resulting delta vector
		FIntVector Delta = ValueToCode - Prediction;		
		uint32 Cost = FCodecV1SharedTools::EstimateCodingCost(Delta);
		
		if (Cost < MinimumCost)
		{			
			MinimumCost = Cost;
			BestMode = Mode;
			BestPrediction = Prediction;
		}
	}
	
	ChosenMode = BestMode;
	return BestPrediction;
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
	FRingBuffer<uint32, VertexStreamCodingIndexHistorySize> IndexHistory(VertexStreamCodingIndexHistorySize); // Previously seen indices
	FRingBuffer<FIntVector, VertexStreamCodingVertexHistorySize> ReconstructedVertexHistory(VertexStreamCodingVertexHistorySize, FIntVector(0, 0, 0)); // Previously seen positions
	int64 MaxEncounteredIndex = -1;
		
	FQualityMetric QualityMetric;

	const uint8* RawElementDataIndices = (const uint8*)IndexStream;
	const uint8* RawElementDataVertices = (const uint8*)VertexStream;

	uint32 PreviousTriangleCornerIndices[3] = { 0 };
	uint32 FaceCount = IndexElementCount / 3;

	// Walk over indices/triangles
	for (uint32 FaceIdx = 0; FaceIdx < FaceCount; ++FaceIdx, RawElementDataIndices += 3 * IndexElementOffset)
	{
		uint32 CornerIndices[3];
		CornerIndices[0] = *(uint32*)(RawElementDataIndices);
		CornerIndices[1] = *(uint32*)(RawElementDataIndices + IndexElementOffset);
		CornerIndices[2] = *(uint32*)(RawElementDataIndices + 2 * IndexElementOffset);

		// Find if this triangle shares an edge with the previous one, if so, get the rotation offsets to rotate the triangle's indices 
		// We want a well-defined order of indices so we can look up the right vertices to apply the parallelogram predictor
		uint32 PreviousTriangleRotationOffsets[3] = { 0, 0, 0 };
		bool bTrianglesShareEdge = FCodecV1SharedTools::FindTrianglesSharedEdge(PreviousTriangleCornerIndices, CornerIndices, PreviousTriangleRotationOffsets);

		// Walk over triangle corners
		for (uint32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
		{
			uint32 IndexValue = CornerIndices[CornerIndex];
			bool NewlyEncounteredVertex = (int64)IndexValue > MaxEncounteredIndex; // We rely on indices being references in order
			MaxEncounteredIndex = FMath::Max((int64)IndexValue, MaxEncounteredIndex);

			if (NewlyEncounteredVertex)
			{
				// Code a newly encountered vertex
				FVector VertexValue = *(FVector*)RawElementDataVertices;
				RawElementDataVertices += VertexElementOffset;

				// Quantize
				FIntVector Encoded = Quantizer.Quantize(VertexValue);

				// Translate to center
				FIntVector EncodedCentered = Encoded - QuantizedTranslationToCenter;

				// Find best prediction mode and prediction using our triangle history
				uint32 PredictionMode;
				FIntVector Prediction = FindModeAndPredictVertex(EncodedCentered, CornerIndex, PreviousTriangleRotationOffsets, ReconstructedVertexHistory, PredictionMode);

				// Residual to code
				FIntVector Residual = EncodedCentered - Prediction;
				
				// Write mode
				WriteSymbol(EncodingContext.VertexPredictionModeTable, PredictionMode);
								
				// Write residual
				WriteInt32(EncodingContext.ResidualVertexPosTable, Residual.X);
				WriteInt32(EncodingContext.ResidualVertexPosTable, Residual.Y);
				WriteInt32(EncodingContext.ResidualVertexPosTable, Residual.Z);				

				EncodedVertexCount++;
				
				// Store previous encountered values
				FIntVector Reconstructed = Prediction + Residual;
				ReconstructedVertexHistory.Push(Reconstructed);

				// Calculate error
				FVector DequantReconstructed = Quantizer.Dequantize(Reconstructed);
				QualityMetric.Register(VertexValue, DequantReconstructed);
			}
			else // NewlyEncounteredVertex == false
			{
				// Keep our vertex history consistent with the indices we have seen.
				// We fetch from our previously reconstructed vertex history. Our history is limited in size, so
				// we do a explicit search and if the value is not found, we use the last seen vector as a safe prediction
				FIntVector EncounteredVertex = ReconstructedVertexHistory[0];

				for (uint32 Index = 0; Index < IndexHistory.Num(); ++Index)
				{
					if (IndexHistory[Index] == IndexValue)
					{
						// We have it in our history buffer
						EncounteredVertex = ReconstructedVertexHistory[Index];
						break;
					}
				}

				ReconstructedVertexHistory.Push(EncounteredVertex);				
			}

			IndexHistory.Push(IndexValue);
		}

		// Save our triangle
		PreviousTriangleCornerIndices[0] = CornerIndices[0];
		PreviousTriangleCornerIndices[1] = CornerIndices[1];
		PreviousTriangleCornerIndices[2] = CornerIndices[2];
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

void FCodecV1Encoder::EncodeNormalStream(const FPackedNormal* Stream, uint64 ElementOffsetBytes, uint32 ElementCount, FHuffmanTable& Table, FStreamEncodingStatistics& Stats)
{
	FBitstreamWriterByteCounter ByteCounter(EncodingContext.Writer);

	FRingBuffer<FIntVector4, NormalStreamCodingHistorySize> ReconstructedHistory(NormalStreamCodingHistorySize, FIntVector4(128, 128, 128, 128));

	const uint8* RawElementData = (const uint8*)Stream;

	// Walk over colors
	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffsetBytes)
	{
		// Load data
		FPackedNormal& NormalValue = *(FPackedNormal*)RawElementData;		
		FIntVector4 Value(NormalValue.Vector.X, NormalValue.Vector.Y, NormalValue.Vector.Z, NormalValue.Vector.W);

		FIntVector4 Prediction = ReconstructedHistory[0]; // Delta coding
		FIntVector4 Residual = FCodecV1SharedTools::SubtractVector4(Value, Prediction); // Residual = Value - Prediction

		// Write residual	
		WriteInt32(Table, Residual.X);
		WriteInt32(Table, Residual.Y);
		WriteInt32(Table, Residual.Z);
		WriteInt32(Table, Residual.W);

		// Decode as the decoder would and keep the result for future prediction
		FIntVector4 Reconstructed = FCodecV1SharedTools::SumVector4(Prediction, Residual); // Reconstructed = Prediction + Residual
		ReconstructedHistory.Push(Reconstructed);
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


void FCodecV1Encoder::WriteInt32(FHuffmanTable& ValueTable, int32 Value)
{
	// We code the length of the value (huffman table), followed by the raw bits. Small values take up very small amount of bits.
	const int32 ZeroValue32BitInt = 32;	// Zero value used to shift interval [32,31] to [0,63]

	int32 AbsValue = FMath::Abs(Value);

	if (Value == 0)
	{
		WriteSymbol(ValueTable, ZeroValue32BitInt);
	}
	else if (Value > 0)
	{
		int32 HiBit = HighestSetBit(AbsValue);	// Find most significant bit k, i.e., number of bits
		WriteSymbol(ValueTable, ZeroValue32BitInt + (HiBit + 1));	// Code number of bits, recenter around new zero, e.g., 32 + 10 bits
		WriteBits(AbsValue - (1 << HiBit), HiBit);	// Code remaining k bits verbatim
	}
	else /*if (Value < 0)*/
	{
		int32 HiBit = HighestSetBit(AbsValue);	// Find most significant bit k, i.e., number of bits
		WriteSymbol(ValueTable, ZeroValue32BitInt - (HiBit + 1));	// Code number of bits, recenter around new zero, e.g., 32 - 10 bits
		WriteBits(AbsValue - (1 << HiBit), HiBit);	// code remaining k bits verbatim
	}	
}

void FCodecV1Encoder::SetupTables()
{
	// Initialize Huffman tables.
	// Most tables store 32-bit integers stored with a bit-length;raw value scheme. Some store specific symbols.
	EncodingContext.ResidualIndicesTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualVertexPosTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.VertexPredictionModeTable.Initialize(VertexPredictionModeCount);
	EncodingContext.ResidualColorTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualNormalTangentXTable.Initialize(HuffmanTableInt32SymbolCount);
	EncodingContext.ResidualNormalTangentZTable.Initialize(HuffmanTableInt32SymbolCount);
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
	EncodingContext.VertexPredictionModeTable.SetPrepass(bPrepass);
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
	EncodingContext.VertexPredictionModeTable.Serialize(Writer);
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
}

void FCodecV1Decoder::DecodeIndexStream(uint32* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	FRingBuffer<uint32, IndexStreamCodingHistorySize> LastReconstructed(IndexStreamCodingHistorySize); // History holding previously seen indices

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset)
	{
		// Read coded residual
		int32 DecodedResidual = ReadInt32(DecodingContext.ResidualIndicesTable);

		uint32 Prediction = LastReconstructed[0]; // Delta coding, best effort
		uint32 Reconstructed = DecodedResidual + Prediction; // Reconstruct

		// Save result to our list
		uint32* Value = (uint32*)RawElementData;
		*Value = Reconstructed;

		// Store previous encountered values
		LastReconstructed.Push(Reconstructed);
	}
}

void FCodecV1Decoder::DecodeMotionVectorStream(FVector* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	// Read header
	FMotionVectorStreamHeader Header;
	ReadBytes((void*)&Header, sizeof(Header));

	FQuantizerVector3 Quantizer(Header.QuantizationPrecision); // We quantize MVs to a certain precision just like the positions

	FRingBuffer<FIntVector, MotionVectorStreamCodingHistorySize> ReconstructedHistory(MotionVectorStreamCodingHistorySize, FIntVector(0, 0, 0)); // Previously seen texture coordinates

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset) // Walk MVs
	{
		// Read coded residual
		FIntVector DecodedResidual;
		DecodedResidual.X = ReadInt32(DecodingContext.ResidualMotionVectorTable);
		DecodedResidual.Y = ReadInt32(DecodingContext.ResidualMotionVectorTable);
		DecodedResidual.Z = ReadInt32(DecodingContext.ResidualMotionVectorTable);

		FIntVector Prediction = ReconstructedHistory[0]; // Delta coding													  
		FIntVector Reconstructed = DecodedResidual + Prediction; // Reconstruct
		FVector Decoded = Quantizer.Dequantize(Reconstructed); // Dequantize

		FVector* Value = (FVector*)RawElementData;	// Save result to our list
		*Value = Decoded;

		// Store previous encountered values
		ReconstructedHistory.Push(Reconstructed);
	}
}

void FCodecV1Decoder::DecodeUVStream(FVector2D* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	// Read header
	FUVStreamHeader Header;
	ReadBytes((void*)&Header, sizeof(Header));

	FQuantizerVector2 Quantizer(Header.Range, Header.QuantizationBits); // We quantize UVs to a number of bits, set in the bitstream header
	
	FRingBuffer<FIntVector, UVStreamCodingHistorySize> ReconstructedHistory(UVStreamCodingHistorySize, FIntVector(0, 0, 0)); // Previously seen texture coordinates

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset) // Walk UVs
	{
		// Read coded residual
		FIntVector DecodedResidual;
		DecodedResidual.X = ReadInt32(DecodingContext.ResidualUVTable);
		DecodedResidual.Y = ReadInt32(DecodingContext.ResidualUVTable);

		FIntVector Prediction = ReconstructedHistory[0]; // Delta coding													  
		FIntVector Reconstructed = DecodedResidual + Prediction; // Reconstruct
		FVector2D Decoded = Quantizer.Dequantize(Reconstructed); // Dequantize

		FVector2D* Value = (FVector2D*)RawElementData;	// Save result to our list
		*Value = Decoded;

		// Store previous encountered values
		ReconstructedHistory.Push(Reconstructed);
	}
}

void FCodecV1Decoder::DecodeNormalStream(FPackedNormal* Stream, uint64 ElementOffset, uint32 ElementCount, FHuffmanTable& Table)
{
	FRingBuffer<FIntVector4, NormalStreamCodingHistorySize> ReconstructedHistory(NormalStreamCodingHistorySize, FIntVector4(128, 128, 128, 128)); // Previously seen normals

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset) // Walk normals
	{
		// Read coded residual
		FIntVector4 DecodedResidual;
		DecodedResidual.X = ReadInt32(Table);
		DecodedResidual.Y = ReadInt32(Table);
		DecodedResidual.Z = ReadInt32(Table);
		DecodedResidual.W = ReadInt32(Table);

		FIntVector4 Prediction = ReconstructedHistory[0]; // Delta coding
		FIntVector4 Reconstructed = FCodecV1SharedTools::SumVector4(DecodedResidual, Prediction);

		FPackedNormal* Value = (FPackedNormal*)RawElementData;	// Save result to our list
		Value->Vector.X = Reconstructed.X;
		Value->Vector.Y = Reconstructed.Y;
		Value->Vector.Z = Reconstructed.Z;
		Value->Vector.W = Reconstructed.W;

		// Store previous encountered values
		ReconstructedHistory.Push(Reconstructed);
	}
}

void FCodecV1Decoder::DecodeColorStream(FColor* Stream, uint64 ElementOffset, uint32 ElementCount)
{
	FRingBuffer<FIntVector4, ColorStreamCodingHistorySize> ReconstructedHistory(ColorStreamCodingHistorySize, FIntVector4(128, 128, 128, 255)); // Previously seen colors

	const uint8* RawElementData = (const uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < ElementCount; ++ElementIdx, RawElementData += ElementOffset)
	{
		FIntVector4 Prediction = ReconstructedHistory[0]; // Delta coding
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
		}	

		FIntVector4 Reconstructed = FCodecV1SharedTools::SumVector4(DecodedResidual, Prediction);
																			
		FColor* Value = (FColor*)RawElementData; // Save result to our list
		Value->R = Reconstructed.X;
		Value->G = Reconstructed.Y;
		Value->B = Reconstructed.Z;
		Value->A = Reconstructed.W;

		// Store previous encountered values
		ReconstructedHistory.Push(Reconstructed);
	}
}

void FCodecV1Decoder::DecodePositionStream(const uint32* IndexStream, uint64 IndexElementOffset, uint32 IndexElementCount, FVector* VertexStream, uint64 VertexElementOffset, uint32 MaxVertexElementCount)
{
	checkf(IndexElementCount > 0, TEXT("You cannot decode vertex stream before the index stream was decoded"));

	// Read header
	FVertexStreamHeader Header;
	ReadBytes((void*)&Header, sizeof(Header));

	FQuantizerVector3 Quantizer(Header.QuantizationPrecision);
	
	FRingBuffer<uint32, VertexStreamCodingIndexHistorySize> IndexHistory(VertexStreamCodingIndexHistorySize); // Previously seen indices
	FRingBuffer<FIntVector, VertexStreamCodingVertexHistorySize> ReconstructedVertexHistory(VertexStreamCodingVertexHistorySize, FIntVector(0, 0, 0)); // Previously seen positions
	int64 MaxEncounteredIndex = -1; // We rely on indices being references in order, a requirement of the encoder and enforced by the preprocessor
	uint32 DecodedVertexCount = 0;

	const uint8* RawElementDataIndices = (const uint8*)IndexStream;
	const uint8* RawElementDataVertices = (const uint8*)VertexStream;

	uint32 PreviousTriangleCornerIndices[3] = { 0, 0, 0 };
	const uint32 FaceCount = IndexElementCount / 3;

	// Walk over indices/triangles
	for (uint32 FaceIdx = 0; FaceIdx < FaceCount; ++FaceIdx, RawElementDataIndices += 3 * IndexElementOffset)
	{
		const uint32 CornerIndices[3]
		{
			*(uint32*)(RawElementDataIndices),
			*(uint32*)(RawElementDataIndices + IndexElementOffset),
			*(uint32*)(RawElementDataIndices + 2 * IndexElementOffset)
		};

		// Find if this triangle shares an edge with the previous one, if so, get the rotation offsets to rotate the triangle's indices 
		// We want a well-defined order of indices so we can look up the right vertices to apply the parallelogram predictor
		uint32 PreviousTriangleRotationOffsets[3] = { 0, 0, 0 }; 
		bool bTrianglesShareEdge = FCodecV1SharedTools::FindTrianglesSharedEdge(PreviousTriangleCornerIndices, CornerIndices, PreviousTriangleRotationOffsets);

		// Walk over triangle corners
		for (uint32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
		{
			const uint32 IndexValue = CornerIndices[CornerIndex];
			const bool NewlyEncounteredVertex = (int64)IndexValue > MaxEncounteredIndex; // We rely on indices being references in order
			MaxEncounteredIndex = FMath::Max((int64)IndexValue, MaxEncounteredIndex);

			if (NewlyEncounteredVertex)
			{
				checkf(DecodedVertexCount < MaxVertexElementCount, TEXT("Encountering more vertices than we have encoded. Encoding and decoding algorithms don't seem to match. Please make sure the preprocessor has processed the mesh such that vertexes are referenced in-order, i.e. if a previously unreferenced vertex is referenced by the index list it's id will always be the next unreferenced id instead of some random unused id."));

				// Read prediction mode
				const uint32 PredictionMode = ReadSymbol(DecodingContext.VertexPredictionModeTable);

				// Read coded residual
				const FIntVector DecodedResidual = { ReadInt32(DecodingContext.ResidualVertexPosTable), ReadInt32(DecodingContext.ResidualVertexPosTable), ReadInt32(DecodingContext.ResidualVertexPosTable) };
				DecodedVertexCount++;

				// Predict using the history according to the prediction mode 
				const FIntVector Prediction = FCodecV1SharedTools::PredictVertex(CornerIndex, PreviousTriangleRotationOffsets, ReconstructedVertexHistory, PredictionMode);

				// Reconstruct, dequantize
				const FIntVector Reconstructed = DecodedResidual + Prediction;
				// Store previous encountered values
				ReconstructedVertexHistory.Push(Reconstructed);

				// Save result to our list
				FVector* Value = (FVector*)RawElementDataVertices;
				*Value = Quantizer.Dequantize(Reconstructed + Header.Translation);
				RawElementDataVertices += VertexElementOffset;		
			}
			else
			{
				// Keep our vertex history consistent with the indices we have seen.
				// We fetch from our previously reconstructed vertex history. Our history is limited in size, so
				// we do a explicit search and if the value is not found, we use the last seen vector as a safe prediction
				FIntVector EncounteredVertex = ReconstructedVertexHistory[0];
				for (uint32 Index = 0; Index < IndexHistory.Num(); ++Index)
				{
					if (IndexHistory[Index] == IndexValue)
					{
						// We have it in our history buffer
						EncounteredVertex = ReconstructedVertexHistory[Index];
						break;
					}
				}

				ReconstructedVertexHistory.Push(EncounteredVertex);
			}

			IndexHistory.Push(IndexValue);
		}

		// Save our triangle
		PreviousTriangleCornerIndices[0] = CornerIndices[0];
		PreviousTriangleCornerIndices[1] = CornerIndices[1];
		PreviousTriangleCornerIndices[2] = CornerIndices[2];
	}
}

void FCodecV1Decoder::ZeroStream(uint8* Stream, uint64 ElementOffset, uint64 ElementSize, uint32 NumElements)
{
	uint8* RawElementData = (uint8*)Stream;

	FMemory::Memzero(RawElementData, ElementSize * NumElements);

	for (uint32 ElementIdx = 0; ElementIdx < NumElements; ++ElementIdx, RawElementData += ElementOffset)
	{
		FMemory::Memzero(RawElementData, ElementSize);
	}
}

void FCodecV1Decoder::ClearStream(FVector* Stream, uint64 ElementOffset, uint32 NumElements)
{
	ZeroStream((uint8*)Stream, ElementOffset, sizeof(FVector), NumElements);
}

void FCodecV1Decoder::ClearStream(FColor* Stream, uint64 ElementOffset, uint32 NumElements)
{
	ZeroStream((uint8*)Stream, ElementOffset, sizeof(FColor), NumElements);
}

void FCodecV1Decoder::ClearStream(FPackedNormal* Stream, uint64 ElementOffset, uint32 NumElements)
{
	uint8* RawElementData = (uint8*)Stream;

	for (uint32 ElementIdx = 0; ElementIdx < NumElements; ++ElementIdx, RawElementData += ElementOffset)
	{
		new (RawElementData) FPackedNormal(); //Zero initialize
	}
}

void FCodecV1Decoder::ClearStream(FVector2D* Stream, uint64 ElementOffset, uint32 NumElements)
{
	ZeroStream((uint8*)Stream, ElementOffset, sizeof(FVector2D), NumElements);
}

void FCodecV1Decoder::SetupAndReadTables()
{
	// Initialize and read Huffman tables from the bitstream
	FHuffmanBitStreamReader& Reader = *DecodingContext.Reader;	
	const FGeometryCacheVertexInfo& VertexInfo = DecodingContext.MeshData->VertexInfo;
		
	if (!VertexInfo.bConstantIndices)
		DecodingContext.ResidualIndicesTable.Initialize(Reader);	
	DecodingContext.ResidualVertexPosTable.Initialize(Reader);
	DecodingContext.VertexPredictionModeTable.Initialize(Reader);
	
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
	Bytes.AddUninitialized(Header.PayloadSize);
	Reader.Serialize(Bytes.GetData(), Bytes.Num()); 	
	FHuffmanBitStreamReader BitReader(Bytes); // This copies the data again. We should optimize this out.
	Bytes.Empty(); // No need anymore
	DecodingContext.Reader = &BitReader;

	// Read which vertex attributes are in the bit stream
	ReadCodedStreamDescription();

	// Restore entropy coding contexts
	SetupAndReadTables();
	
	{
		// Decode streams
		
		TArray<uint32>& Indices = OutMeshData.Indices;
		if (!OutMeshData.VertexInfo.bConstantIndices)
		{
			OutMeshData.Indices.Empty(Header.IndexCount);
			OutMeshData.Indices.AddUninitialized(Header.IndexCount);
			DecodeIndexStream(&Indices[0], Indices.GetTypeSize(), Header.IndexCount);
		}
		
		TArray<FVector>& Positions = OutMeshData.Positions;
		OutMeshData.Positions.Empty(Header.VertexCount);
		OutMeshData.Positions.AddZeroed(Header.VertexCount);
		DecodePositionStream(&Indices[0], Indices.GetTypeSize(), Indices.Num(), &Positions[0], Positions.GetTypeSize(), Header.VertexCount);

		OutMeshData.Colors.Empty(Header.VertexCount);
		OutMeshData.Colors.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasColor0)
		{
			TArray<FColor>& Colors = OutMeshData.Colors;
			DecodeColorStream(&Colors[0], Colors.GetTypeSize(), Header.VertexCount);
		}

		OutMeshData.TangentsX.Empty(Header.VertexCount);
		OutMeshData.TangentsX.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasTangentX)
		{
			TArray<FPackedNormal>& TangentsX = OutMeshData.TangentsX;
			DecodeNormalStream(&TangentsX[0], TangentsX.GetTypeSize(), Header.VertexCount, DecodingContext.ResidualNormalTangentXTable);
		}

		OutMeshData.TangentsZ.Empty(Header.VertexCount);
		OutMeshData.TangentsZ.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasTangentZ)
		{
			TArray<FPackedNormal>& TangentsZ = OutMeshData.TangentsZ;
			DecodeNormalStream(&TangentsZ[0], TangentsZ.GetTypeSize(), Header.VertexCount, DecodingContext.ResidualNormalTangentZTable);
		}

		OutMeshData.TextureCoordinates.Empty(Header.VertexCount);
		OutMeshData.TextureCoordinates.AddZeroed(Header.VertexCount);
		if (OutMeshData.VertexInfo.bHasUV0)
		{
			TArray<FVector2D>& TextureCoordinates = OutMeshData.TextureCoordinates;
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

int32 FCodecV1Decoder::ReadInt32(FHuffmanTable& ValueTable)
{	
	// We code the length of the value (huffman table), followed by the raw bits. Small values take up very small amount of bits.
	const int32 ZeroValue32BitInt = 32;	// Zero value used to shift interval [32,31] to [0,63]

	const int32 PackedLength = ReadSymbol(ValueTable); // Read number of bits, i.e., most significant bit + 1
	if (PackedLength == ZeroValue32BitInt)
	{
		return 0;
	}
	else if (PackedLength > ZeroValue32BitInt)
	{	
		int32 NumBits = PackedLength - 1 - ZeroValue32BitInt;  // Positive number, undo re centering around new zero, extract length
		return ReadBits(NumBits) + (1 << NumBits);	// Read remaining bits
	}
	else /*if (PackedLength < ZeroValue32BitInt)*/
	{		
		int32 NumBits = ZeroValue32BitInt - PackedLength - 1;	 // Negative number, undo re centering around new zero, extract length
		return -(ReadBits(NumBits) + (1 << NumBits)); // Read remaining bits
	}	
}
// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_PerTrackCompression.cpp
=============================================================================*/ 

#include "Animation/AnimCompress_PerTrackCompression.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"
#include "AnimEncoding_PerTrackCompression.h"

struct FAnimSetMeshLinkup;

struct FPerTrackCachedInfo
{
	/** Used as a sanity check to validate the cache */
	const FAnimSetMeshLinkup* AnimLinkup;

	/** Contains the maximum end effector errors from probe perturbations throughout the skeleton */
	TArray<FAnimPerturbationError> PerTrackErrors;

	/** Contains the height of each track within the skeleton */
	TArray<int32> TrackHeights;
};


/**
 * Structure that carries compression settings used in FPerTrackCompressor
 */
struct FPerTrackParams
{
	float MaxZeroingThreshold;

	const UAnimSequence* AnimSeq;

	bool bIncludeKeyTable;
};

/**
 * This class compresses a single rotation or translation track into an internal buffer, keeping error metrics as it goes.
 */
class FPerTrackCompressor
{
public:
	// Used during compression
	float MaxError;
	double SumError;

	// Results of compression
	TArray<uint8> CompressedBytes;
	AnimationCompressionFormat ActualCompressionMode;
	int32 ActualKeyFlags;

	/** Does the compression scheme need a key->frame table (needed if the keys are spaced non-uniformly in time) */
	bool bReallyNeedsFrameTable;

protected:
	/** Resets the compression buffer to defaults (no data) */
	void Reset()
	{
		MaxError = 0.0f;
		SumError = 0.0;
		bReallyNeedsFrameTable = false;
		ActualCompressionMode = ACF_None;
		ActualKeyFlags = 0;
		CompressedBytes.Empty();
	}

	/**
	 * Creates a header integer with four fields:
	 *   NumKeys can be no more than 24 bits (positions 0..23)
	 *   KeyFlags can be no more than 3 bits (positions 24..27)
	 *   bReallyNeedsFrameTable is a single bit (position 27)
	 *   KeyFormat can be no more than 4 bits (positions 31..28)
	 *
	 *   Also updates the ActualCompressionMode field
	 */
	int32 MakeHeader(const int32 NumKeys, const AnimationCompressionFormat KeyFormat, const int32 KeyFlags)
	{
		ActualCompressionMode = KeyFormat;
		ActualKeyFlags = KeyFlags;
		return FAnimationCompression_PerTrackUtils::MakeHeader(NumKeys, (int32)KeyFormat, KeyFlags, bReallyNeedsFrameTable);
	}

	/** Ensures that the CompressedBytes output stream is a multiple of 4 bytes long */
	void PadOutputStream()
	{
		const uint8 PadSentinel = 85; //(1<<1)+(1<<3)+(1<<5)+(1<<7)

		const int32 PadLength = Align(CompressedBytes.Num(), 4) - CompressedBytes.Num();
		for (int32 i = 0; i < PadLength; ++i)
		{
			CompressedBytes.Add(PadSentinel);
		}
	}

	/** Writes Length bytes from Data to the output stream */
	void AppendBytes(const void* Data, int32 Length)
	{
		const int32 Offset = CompressedBytes.AddUninitialized(Length);
		FMemory::Memcpy(CompressedBytes.GetData() + Offset, Data, Length);
	}

	void CompressTranslation_Identity(const FTranslationTrack& TranslationData)
	{
		// Compute the error when using this compression type (how far off from (0,0,0) are they?)
		const int32 NumKeys = TranslationData.PosKeys.Num();
		for (int32 i = 0; i < NumKeys; ++i)
		{
			float Error = TranslationData.PosKeys[i].Size();
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
		ActualCompressionMode = ACF_Identity;

		// Add nothing to compressed bytes; this type gets flagged extra-special, back at the offset table
	}

	void CompressTranslation_16_16_16(const FTranslationTrack& TranslationData, float ZeroingThreshold)
	{
		const int32 NumKeys = TranslationData.PosKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(TranslationData.PosKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressTranslation_Identity(TranslationData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_Fixed48NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys for the non-zero components
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FVector& V = TranslationData.PosKeys[i];
			
			uint16 X = 0;
			uint16 Y = 0;
			uint16 Z = 0;

			if (bHasX)
			{
				X = FAnimationCompression_PerTrackUtils::CompressFixed16(V.X, LogScale);
				AppendBytes(&X, sizeof(X));
			}
			if (bHasY)
			{
				Y = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Y, LogScale);
				AppendBytes(&Y, sizeof(Y));
			}
			if (bHasZ)
			{
				Z = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Z, LogScale);
				AppendBytes(&Z, sizeof(Z));
			}

			const FVector DecompressedV(
				bHasX ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(X) : 0.0f,
				bHasY ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(Y) : 0.0f,
				bHasZ ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(Z) : 0.0f);

			const float Error = (V - DecompressedV).Size();
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressTranslation_Uncompressed(const FTranslationTrack& TranslationData, float ZeroingThreshold)
	{
		const int32 NumKeys = TranslationData.PosKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(TranslationData.PosKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if( !bHasX && !bHasY && !bHasZ )
		{
			// No point in using this over the identity encoding
			CompressTranslation_Identity(TranslationData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_Float96NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FVector& V = TranslationData.PosKeys[i];
			if( bHasX )
			{
				AppendBytes(&(V.X), sizeof(float));
			}
			if( bHasY )
			{
				AppendBytes(&(V.Y), sizeof(float));
			}
			if( bHasZ )
			{
				AppendBytes(&(V.Z), sizeof(float));
			}
		}

		// No error, it's a perfect encoding
		MaxError = 0.0f;
		SumError = 0.0;
	}

	// Encode a 0..1 interval in 10:11:11 (X and Z swizzled in the 11:11:10 source because Z is more important in most animations)
	// and store an uncompressed bounding box at the start of the track to scale that 0..1 back up
	void CompressTranslation_10_11_11(const FTranslationTrack& TranslationData, float ZeroingThreshold)
	{
		const int32 NumKeys = TranslationData.PosKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(TranslationData.PosKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressTranslation_Identity(TranslationData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_IntervalFixed32NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the bounds out
		float Mins[3];
		float Ranges[3];
		FVector Range(KeyBounds.Max - KeyBounds.Min);
		Mins[0] = KeyBounds.Min.X;
		Mins[1] = KeyBounds.Min.Y;
		Mins[2] = KeyBounds.Min.Z;
		Ranges[0] = Range.X;
		Ranges[1] = Range.Y;
		Ranges[2] = Range.Z;
		if (bHasX)
		{
			AppendBytes(Mins + 0, sizeof(float));
			AppendBytes(Ranges + 0, sizeof(float));
		}
		else
		{
			Ranges[0] = Mins[0] = 0.0f;
		}

		if (bHasY)
		{
			AppendBytes(Mins + 1, sizeof(float));
			AppendBytes(Ranges + 1, sizeof(float));
		}
		else
		{
			Ranges[1] = Mins[1] = 0.0f;
		}

		if (bHasZ)
		{
			AppendBytes(Mins + 2, sizeof(float));
			AppendBytes(Ranges + 2, sizeof(float));
		}
		else
		{
			Ranges[2] = Mins[2] = 0.0f;
		}

		// Write the keys out
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FVector& V = TranslationData.PosKeys[i];
			const FVectorIntervalFixed32NoW Compressor(V, Mins, Ranges);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and update the error stats
			FVector DecompressedV;
			Compressor.ToVector(DecompressedV, Mins, Ranges);

			const float Error = (DecompressedV - V).Size();
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	static FBox CalculateQuatACF96Bounds(const FQuat* Points, int32 NumPoints)
	{
		FBox Results(ForceInitToZero);

		for (int32 i = 0; i < NumPoints; ++i)
		{
			const FQuatFloat96NoW Converter(Points[i]);

			Results += FVector(Converter.X, Converter.Y, Converter.Z);
		}


		return Results;
	}

	void CompressRotation_Identity(const FRotationTrack& RotationData)
	{
		// Compute the error when using this compression type (how far off from identity are they?)
		const int32 NumKeys = RotationData.RotKeys.Num();
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const float Error = FQuat::ErrorAutoNormalize(RotationData.RotKeys[i], FQuat::Identity);
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
		ActualCompressionMode = ACF_Identity;

		// Add nothing to compressed bytes; this type gets flagged extra-special, back at the offset table
	}

	template <typename CompressorType>
	void InnerCompressRotation(const FRotationTrack& RotationData)
	{
		// Write the keys out
		const int32 NumKeys = RotationData.RotKeys.Num();
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FQuat& Q = RotationData.RotKeys[i];
			check(Q.IsNormalized());

			// Compress and write out the quaternion
			const CompressorType Compressor(Q);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and check the error caused by the compression
			FQuat DecompressedQ;
			Compressor.ToQuat(DecompressedQ);

			check(DecompressedQ.IsNormalized());
			const float Error = FQuat::ErrorAutoNormalize(Q, DecompressedQ);
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	// Uncompressed packing still drops the W component, storing a rotation in 3 floats (ACF_Float96NoW)
	void CompressRotation_Uncompressed(const FRotationTrack& RotationData)
	{
		const int32 NumKeys = RotationData.RotKeys.Num();

		// Write the header out
		int32 Header = MakeHeader(NumKeys, ACF_Float96NoW, 7);
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		InnerCompressRotation<FQuatFloat96NoW>(RotationData);
	}

	void CompressRotation_16_16_16(const FRotationTrack& RotationData, float ZeroingThreshold)
	{
		const int32 NumKeys = RotationData.RotKeys.Num();

		// Determine the bounds
		const FBox KeyBounds = CalculateQuatACF96Bounds(RotationData.RotKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressRotation_Identity(RotationData);
			return;
		}


		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_Fixed48NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys for the non-zero components
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FQuat& Q = RotationData.RotKeys[i];

			FQuat QRenorm(Q);
			if (!bHasX)
			{
				QRenorm.X = 0;
			}
			if (!bHasY)
			{
				QRenorm.Y = 0;
			}
			if (!bHasZ)
			{
				QRenorm.Z = 0;
			}
			QRenorm.Normalize();

			const FQuatFloat96NoW V(QRenorm);

			uint16 X = 0;
			uint16 Y = 0;
			uint16 Z = 0;

			if (bHasX)
			{
				X = FAnimationCompression_PerTrackUtils::CompressFixed16(V.X);
				AppendBytes(&X, sizeof(X));
			}
			if (bHasY)
			{
				Y = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Y);
				AppendBytes(&Y, sizeof(Y));
			}
			if (bHasZ)
			{
				Z = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Z);
				AppendBytes(&Z, sizeof(Z));
			}

			FQuatFloat96NoW Decompressor;
			Decompressor.X = bHasX ? FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(X) : 0.0f;
			Decompressor.Y = bHasY ? FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(Y) : 0.0f;
			Decompressor.Z = bHasZ ? FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(Z) : 0.0f;

			FQuat DecompressedQ;
			Decompressor.ToQuat(DecompressedQ);

			if (!DecompressedQ.IsNormalized())
			{
				UE_LOG(LogAnimationCompression, Log, TEXT("Error: Loss of normalization!"));
				UE_LOG(LogAnimationCompression, Log, TEXT("  Track: %i, Key: %i"), 0, i);
				UE_LOG(LogAnimationCompression, Log, TEXT("  Q : %s"), *Q.ToString());
				UE_LOG(LogAnimationCompression, Log, TEXT("  Q': %s"), *DecompressedQ.ToString());
				UE_LOG(LogAnimationCompression, Log, TEXT(" XYZ: %i, %i, %i"), X, Y, Z);
			}

			check(DecompressedQ.IsNormalized());
			const float Error = FQuat::ErrorAutoNormalize(Q, DecompressedQ);
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressRotation_11_11_10(const FRotationTrack& RotationData, float ZeroingThreshold)
	{
		const int32 NumKeys = RotationData.RotKeys.Num();

		// Determine the bounds
		const FBox KeyBounds = CalculateQuatACF96Bounds(RotationData.RotKeys.GetData(), NumKeys);
		FVector Range(KeyBounds.Max - KeyBounds.Min);

		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if ((!bHasX && !bHasY && !bHasZ) || (Range.SizeSquared() > 16.0f))
		{
			// If there are no components, then there is no point in using this over the identity encoding
			// If the range is insane, error out early (error metric will be high)
			CompressRotation_Identity(RotationData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_IntervalFixed32NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the bounds out
		float Mins[3];
		float Ranges[3];
		Mins[0] = KeyBounds.Min.X;
		Mins[1] = KeyBounds.Min.Y;
		Mins[2] = KeyBounds.Min.Z;
		Ranges[0] = Range.X;
		Ranges[1] = Range.Y;
		Ranges[2] = Range.Z;
		if (bHasX)
		{
			AppendBytes(Mins + 0, sizeof(float));
			AppendBytes(Ranges + 0, sizeof(float));
		}
		else
		{
			Ranges[0] = Mins[0] = 0.0f;
		}

		if (bHasY)
		{
			AppendBytes(Mins + 1, sizeof(float));
			AppendBytes(Ranges + 1, sizeof(float));
		}
		else
		{
			Ranges[1] = Mins[1] = 0.0f;
		}

		if (bHasZ)
		{
			AppendBytes(Mins + 2, sizeof(float));
			AppendBytes(Ranges + 2, sizeof(float));
		}
		else
		{
			Ranges[2] = Mins[2] = 0.0f;
		}

		// Write the keys out
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FQuat& Q = RotationData.RotKeys[i];

			FQuat QRenorm(Q);
			if (!bHasX)
			{
				QRenorm.X = 0;
			}
			if (!bHasY)
			{
				QRenorm.Y = 0;
			}
			if (!bHasZ)
			{
				QRenorm.Z = 0;
			}
			QRenorm.Normalize();


			// Compress and write out the quaternion
			const FQuatIntervalFixed32NoW Compressor(QRenorm, Mins, Ranges);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and check the error caused by the compression
			FQuat DecompressedQ;
			Compressor.ToQuat(DecompressedQ, Mins, Ranges);

			if (!DecompressedQ.IsNormalized())
			{
				UE_LOG(LogAnimationCompression, Log, TEXT("Error: Loss of normalization!"));
				UE_LOG(LogAnimationCompression, Log, TEXT("  Track: %i, Key: %i"), 0, i);
				UE_LOG(LogAnimationCompression, Log, TEXT("  Q : %s"), *Q.ToString());
				UE_LOG(LogAnimationCompression, Log, TEXT("  Q': %s"), *DecompressedQ.ToString());
				UE_LOG(LogAnimationCompression, Log, TEXT(" XYZ: %f, %f, %f, %f"), QRenorm.X, QRenorm.Y, QRenorm.Z, QRenorm.W);
				UE_LOG(LogAnimationCompression, Log, TEXT(" Mins(%f, %f, %f)   Maxs(%f, %f,%f)"), KeyBounds.Min.X, KeyBounds.Min.Y, KeyBounds.Min.Z, KeyBounds.Max.X, KeyBounds.Max.Y, KeyBounds.Max.Z);
			}
			check(DecompressedQ.IsNormalized());
			const float Error = FQuat::ErrorAutoNormalize(Q, DecompressedQ);
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressRotation_Fixed32(const FRotationTrack& RotationData)
	{
		// Write the header out
		const int32 NumKeys = RotationData.RotKeys.Num();
		const int32 Header = MakeHeader(NumKeys, ACF_Fixed32NoW, 7);
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		InnerCompressRotation<FQuatFixed32NoW>(RotationData);
	}

	void CompressRotation_Float32(const FRotationTrack& RotationData)
	{
		// Write the header out
		const int32 NumKeys = RotationData.RotKeys.Num();
		const int32 Header = MakeHeader(NumKeys, ACF_Float32NoW, 7);
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		InnerCompressRotation<FQuatFloat32NoW>(RotationData);
	}

	void CompressScale_Identity(const FScaleTrack& ScaleData)
	{
		// Compute the error when using this compression type (how far off from (0,0,0) are they?)
		const int32 NumKeys = ScaleData.ScaleKeys.Num();
		for (int32 i = 0; i < NumKeys; ++i)
		{
			float Error = ScaleData.ScaleKeys[i].Size();
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
		ActualCompressionMode = ACF_Identity;

		// Add nothing to compressed bytes; this type gets flagged extra-special, back at the offset table
	}

	void CompressScale_16_16_16(const FScaleTrack& ScaleData, float ZeroingThreshold)
	{
		const int32 NumKeys = ScaleData.ScaleKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(ScaleData.ScaleKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressScale_Identity(ScaleData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_Fixed48NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys for the non-zero components
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FVector& V = ScaleData.ScaleKeys[i];

			uint16 X = 0;
			uint16 Y = 0;
			uint16 Z = 0;

			if (bHasX)
			{
				X = FAnimationCompression_PerTrackUtils::CompressFixed16(V.X, LogScale);
				AppendBytes(&X, sizeof(X));
			}
			if (bHasY)
			{
				Y = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Y, LogScale);
				AppendBytes(&Y, sizeof(Y));
			}
			if (bHasZ)
			{
				Z = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Z, LogScale);
				AppendBytes(&Z, sizeof(Z));
			}

			const FVector DecompressedV(
				bHasX ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(X) : 0.0f,
				bHasY ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(Y) : 0.0f,
				bHasZ ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(Z) : 0.0f);

			const float Error = (V - DecompressedV).Size();
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressScale_Uncompressed(const FScaleTrack& ScaleData, float ZeroingThreshold)
	{
		const int32 NumKeys = ScaleData.ScaleKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(ScaleData.ScaleKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if( !bHasX && !bHasY && !bHasZ )
		{
			// No point in using this over the identity encoding
			CompressScale_Identity(ScaleData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_Float96NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FVector& V = ScaleData.ScaleKeys[i];
			if( bHasX )
			{
				AppendBytes(&(V.X), sizeof(float));
			}
			if( bHasY )
			{
				AppendBytes(&(V.Y), sizeof(float));
			}
			if( bHasZ )
			{
				AppendBytes(&(V.Z), sizeof(float));
			}
		}

		// No error, it's a perfect encoding
		MaxError = 0.0f;
		SumError = 0.0;
	}

	// Encode a 0..1 interval in 10:11:11 (X and Z swizzled in the 11:11:10 source because Z is more important in most animations)
	// and store an uncompressed bounding box at the start of the track to scale that 0..1 back up
	void CompressScale_10_11_11(const FScaleTrack& ScaleData, float ZeroingThreshold)
	{
		const int32 NumKeys = ScaleData.ScaleKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(ScaleData.ScaleKeys.GetData(), NumKeys);
		const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (FMath::Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressScale_Identity(ScaleData);
			return;
		}

		// Write the header out
		const int32 Header = MakeHeader(NumKeys, ACF_IntervalFixed32NoW, (bHasX ? 1 : 0) | ((bHasY ? 1 : 0)<<1) | ((bHasZ ? 1 : 0)<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the bounds out
		float Mins[3];
		float Ranges[3];
		FVector Range(KeyBounds.Max - KeyBounds.Min);
		Mins[0] = KeyBounds.Min.X;
		Mins[1] = KeyBounds.Min.Y;
		Mins[2] = KeyBounds.Min.Z;
		Ranges[0] = Range.X;
		Ranges[1] = Range.Y;
		Ranges[2] = Range.Z;
		if (bHasX)
		{
			AppendBytes(Mins + 0, sizeof(float));
			AppendBytes(Ranges + 0, sizeof(float));
		}
		else
		{
			Ranges[0] = Mins[0] = 0.0f;
		}

		if (bHasY)
		{
			AppendBytes(Mins + 1, sizeof(float));
			AppendBytes(Ranges + 1, sizeof(float));
		}
		else
		{
			Ranges[1] = Mins[1] = 0.0f;
		}

		if (bHasZ)
		{
			AppendBytes(Mins + 2, sizeof(float));
			AppendBytes(Ranges + 2, sizeof(float));
		}
		else
		{
			Ranges[2] = Mins[2] = 0.0f;
		}

		// Write the keys out
		for (int32 i = 0; i < NumKeys; ++i)
		{
			const FVector& V = ScaleData.ScaleKeys[i];
			const FVectorIntervalFixed32NoW Compressor(V, Mins, Ranges);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and update the error stats
			FVector DecompressedV;
			Compressor.ToVector(DecompressedV, Mins, Ranges);

			const float Error = (DecompressedV - V).Size();
			MaxError = FMath::Max(MaxError, Error);
			SumError += Error;
		}
	}

	/** Helper method for writing out the key->frame mapping table with a given index type */
	template <typename FrameIndexType>
	void EmitKeyToFrameTable(int32 NumFrames, float FramesPerSecond, const TArray<float>& Times)
	{
		PadOutputStream();

		// write the key table
		const int32 NumKeys = Times.Num();
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			// Convert the frame time into a frame index and write it out
			FrameIndexType FrameIndex = (FrameIndexType)FMath::Clamp(FMath::TruncToInt((Times[KeyIndex] * FramesPerSecond) + 0.5f), 0, NumFrames - 1);
			AppendBytes(&FrameIndex, sizeof(FrameIndexType));
		}

		PadOutputStream();
	}

	/** Writes out the key->frame mapping table if it is needed for the current compression type */
	void ProcessKeyToFrameTable(const FPerTrackParams& Params, const TArray<float>& FrameTimes)
	{
		if (bReallyNeedsFrameTable && (CompressedBytes.Num() > 0))
		{
			const int32 NumFrames = Params.AnimSeq->GetRawNumberOfFrames();
			const float SequenceLength = Params.AnimSeq->SequenceLength;
			const float FramesPerSecond = (NumFrames - 1) / SequenceLength;

			if (NumFrames <= 0xFF)
			{
				EmitKeyToFrameTable<uint8>(NumFrames, FramesPerSecond, FrameTimes);
			}
			else
			{
				EmitKeyToFrameTable<uint16>(NumFrames, FramesPerSecond, FrameTimes);
			}
		}
	}

public:
	/** Constructs a compressed track of translation data */
	FPerTrackCompressor(int32 InCompressionType, const FTranslationTrack& TranslationData, const FPerTrackParams& Params)
	{
		Reset();
		bReallyNeedsFrameTable = Params.bIncludeKeyTable && (TranslationData.PosKeys.Num() > 1) && (TranslationData.PosKeys.Num() < Params.AnimSeq->GetRawNumberOfFrames());

		switch (InCompressionType)
		{
		case ACF_Identity:
			CompressTranslation_Identity(TranslationData);
			break;
		case ACF_None:
		case ACF_Float96NoW:
			CompressTranslation_Uncompressed(TranslationData, Params.MaxZeroingThreshold);
			break;
		case ACF_Fixed48NoW:
			CompressTranslation_16_16_16(TranslationData, Params.MaxZeroingThreshold);
			break;
		case ACF_IntervalFixed32NoW:
			CompressTranslation_10_11_11(TranslationData, Params.MaxZeroingThreshold);
			break;
			// The following two formats don't work well for translation (fixed range & low precision)
			//case ACF_Fixed32NoW:
			//case ACF_Float32NoW:
		default:
			UE_LOG(LogAnimationCompression, Fatal,TEXT("Unsupported translation compression format"));
			break;
		}

		PadOutputStream();

		ProcessKeyToFrameTable(Params, TranslationData.Times);
	}

	/** Constructs a compressed track of rotation data */
	FPerTrackCompressor(int32 InCompressionType, const FRotationTrack& RotationData, const FPerTrackParams& Params)
	{
		Reset();
		bReallyNeedsFrameTable = Params.bIncludeKeyTable && (RotationData.RotKeys.Num() > 1) && (RotationData.RotKeys.Num() < Params.AnimSeq->GetRawNumberOfFrames());

		switch (InCompressionType)
		{
		case ACF_Identity:
			CompressRotation_Identity(RotationData);
			break;
		case ACF_None:
		case ACF_Float96NoW:
			CompressRotation_Uncompressed(RotationData);
			break;
		case ACF_Fixed48NoW:
			CompressRotation_16_16_16(RotationData, Params.MaxZeroingThreshold);
			break;
		case ACF_IntervalFixed32NoW:
			CompressRotation_11_11_10(RotationData, Params.MaxZeroingThreshold);
			break;
		case ACF_Fixed32NoW:
			CompressRotation_Fixed32(RotationData);
			break;
		case ACF_Float32NoW:
			CompressRotation_Float32(RotationData);
			break;
		default:
			UE_LOG(LogAnimationCompression, Fatal,TEXT("Unsupported rotation compression format"));
			break;
		}

		PadOutputStream();

		ProcessKeyToFrameTable(Params, RotationData.Times);
	}

	/** Constructs a compressed track of Scale data */
	FPerTrackCompressor(int32 InCompressionType, const FScaleTrack& ScaleData, const FPerTrackParams& Params)
	{
		Reset();
		bReallyNeedsFrameTable = Params.bIncludeKeyTable && (ScaleData.ScaleKeys.Num() > 1) && (ScaleData.ScaleKeys.Num() < Params.AnimSeq->GetRawNumberOfFrames());

		switch (InCompressionType)
		{
		case ACF_Identity:
			CompressScale_Identity(ScaleData);
			break;
		case ACF_None:
		case ACF_Float96NoW:
			CompressScale_Uncompressed(ScaleData, Params.MaxZeroingThreshold);
			break;
		case ACF_Fixed48NoW:
			CompressScale_16_16_16(ScaleData, Params.MaxZeroingThreshold);
			break;
		case ACF_IntervalFixed32NoW:
			CompressScale_10_11_11(ScaleData, Params.MaxZeroingThreshold);
			break;
			// The following two formats don't work well for Scale (fixed range & low precision)
			//case ACF_Fixed32NoW:
			//case ACF_Float32NoW:
		default:
			UE_LOG(LogAnimationCompression, Fatal,TEXT("Unsupported Scale compression format"));
			break;
		}

		PadOutputStream();

		ProcessKeyToFrameTable(Params, ScaleData.Times);
	}
};

UAnimCompress_PerTrackCompression::UAnimCompress_PerTrackCompression(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Compress each track independently");
	MaxPosDiffBitwise = 0.007f;
	MaxAngleDiffBitwise = 0.002f;
	MaxScaleDiffBitwise	= 0.0007f;
	MaxZeroingThreshold = 0.0002f;
	ResampledFramerate = 15.0f;
	bResampleAnimation = false;
	MinKeysForResampling = 10;
	bRetarget = false;
	bActuallyFilterLinearKeys = false;
	bUseAdaptiveError = false;
	ParentingDivisor = 1.0f;
	ParentingDivisorExponent = 1.0f;
	TrackHeightBias = 1;
	bUseOverrideForEndEffectors = false;
	bUseAdaptiveError2 = false;
	RotationErrorSourceRatio = 0.8f;
	TranslationErrorSourceRatio = 0.8f;
	ScaleErrorSourceRatio = 0.001f;
	MaxErrorPerTrackRatio = 0.3f;
	PerturbationProbeSize = 0.001f;

	AllowedRotationFormats.Add(ACF_Identity);
	AllowedRotationFormats.Add(ACF_Fixed48NoW);

	AllowedTranslationFormats.Add(ACF_Identity);
	AllowedTranslationFormats.Add(ACF_IntervalFixed32NoW);
	AllowedTranslationFormats.Add(ACF_Fixed48NoW);

	AllowedScaleFormats.Add(ACF_Identity);
	AllowedScaleFormats.Add(ACF_IntervalFixed32NoW);
	AllowedScaleFormats.Add(ACF_Fixed48NoW);
}

#if WITH_EDITOR
void UAnimCompress_PerTrackCompression::CompressUsingUnderlyingCompressor(
	UAnimSequence* AnimSeq, 
	const TArray<FBoneData>& BoneData, 
	const TArray<FTranslationTrack>& TranslationData,
	const TArray<FRotationTrack>& RotationData,
	const TArray<FScaleTrack>& ScaleData,
	const bool bFinalPass)
{
	// If not doing final pass, then do the RemoveLinearKey version that is less destructive.
	// We're potentially removing whole tracks here, and that doesn't work well with LinearKeyRemoval algorithm.
	if( !bFinalPass )
	{
		UAnimCompress_RemoveLinearKeys::CompressUsingUnderlyingCompressor(
			AnimSeq,
			BoneData,
			TranslationData,
			RotationData,
			ScaleData,
			bFinalPass);
		return;
	}

	// Grab the cache
	check(PerReductionCachedData != NULL);
	FPerTrackCachedInfo* Cache = (FPerTrackCachedInfo*)PerReductionCachedData;

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_PerTrackCompression;
	AnimSeq->RotationCompressionFormat = ACF_Identity;
	AnimSeq->TranslationCompressionFormat = ACF_Identity;
	AnimSeq->ScaleCompressionFormat = ACF_Identity;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);

	// Prime the compression buffers
	check(TranslationData.Num() == RotationData.Num());
	const int32 NumTracks = TranslationData.Num();
	const bool bHasScale = ScaleData.Num() > 0;

	AnimSeq->CompressedTrackOffsets.Empty(NumTracks*2);
	AnimSeq->CompressedTrackOffsets.AddUninitialized(NumTracks*2);
	AnimSeq->CompressedScaleOffsets.Empty(0);

	if ( bHasScale )
	{
		AnimSeq->CompressedScaleOffsets.SetStripSize(1);
		AnimSeq->CompressedScaleOffsets.AddUninitialized(NumTracks);
	}

	AnimSeq->CompressedByteStream.Empty();

	// Compress each track independently
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		// Compression parameters / thresholds
		FPerTrackParams Params;
		Params.AnimSeq = AnimSeq;
		Params.MaxZeroingThreshold = MaxZeroingThreshold;

		// Determine the local-space error cutoffs
		float MaxPositionErrorCutoff = MaxPosDiffBitwise;
		float MaxAngleErrorCutoff = MaxAngleDiffBitwise;
		float MaxScaleErrorCutoff = MaxScaleDiffBitwise;

		if (bUseAdaptiveError)
		{
			// The height of the track is the distance from an end effector.  It's used to reduce the acceptable error the
			// higher in the skeleton we get, since a higher bone will cause cascading errors everywhere.
			const int32 PureTrackHeight = Cache->TrackHeights[TrackIndex];
			const int32 EffectiveTrackHeight = FMath::Max(0, PureTrackHeight + TrackHeightBias);

			const float Scaler = 1.0f / FMath::Pow(FMath::Max(ParentingDivisor, 1.0f), EffectiveTrackHeight * FMath::Max(0.0f, ParentingDivisorExponent));

			MaxPositionErrorCutoff = FMath::Max<float>(MaxZeroingThreshold, MaxPosDiff * Scaler);
			MaxAngleErrorCutoff = FMath::Max<float>(MaxZeroingThreshold, MaxAngleDiff * Scaler);
			MaxScaleErrorCutoff = FMath::Max<float>(MaxZeroingThreshold, MaxScaleDiff * Scaler);

			if (bUseOverrideForEndEffectors && (PureTrackHeight == 0))
			{
				MaxPositionErrorCutoff = MinEffectorDiff;
			}
		}
		else if (bUseAdaptiveError2)
		{
			const FAnimPerturbationError& TrackError = Cache->PerTrackErrors[TrackIndex];

			float ThresholdT_DueR = (TrackError.MaxErrorInTransDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToRot) : 1.0f;
			float ThresholdT_DueT = (TrackError.MaxErrorInTransDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToTrans) : 1.0f;
			float ThresholdT_DueS = (TrackError.MaxErrorInTransDueToScale > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToScale) : 1.0f;

			//@TODO: Mixing spaces (target angle error is in radians, perturbation is in quaternion component units)
			float ThresholdR_DueR = (TrackError.MaxErrorInRotDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToRot) : 1.0f;
			float ThresholdR_DueT = (TrackError.MaxErrorInRotDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToTrans) : 1.0f;
			float ThresholdR_DueS = (TrackError.MaxErrorInRotDueToScale > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToScale) : 1.0f;

			// these values are not used, so I don't think we should calculate?
// 			float ThresholdS_DueR = (TrackError.MaxErrorInScaleDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInScaleDueToRot) : 1.0f;
// 			float ThresholdS_DueT = (TrackError.MaxErrorInScaleDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInScaleDueToTrans) : 1.0f;
// 			float ThresholdS_DueS = (TrackError.MaxErrorInScaleDueToScale > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInScaleDueToScale) : 1.0f;

			// @Todo fix the error - this doesn't make sense
			MaxAngleErrorCutoff = FMath::Min(MaxAngleDiffBitwise, MaxErrorPerTrackRatio * MaxAngleDiff * FMath::Lerp(ThresholdR_DueR, ThresholdT_DueR, RotationErrorSourceRatio));
			MaxPositionErrorCutoff = FMath::Min(MaxPosDiffBitwise, MaxErrorPerTrackRatio * MaxPosDiff * FMath::Lerp(ThresholdR_DueT, ThresholdT_DueT, TranslationErrorSourceRatio));
			MaxScaleErrorCutoff = FMath::Min(MaxScaleDiffBitwise, MaxErrorPerTrackRatio * MaxScaleDiff * FMath::Lerp(ThresholdR_DueS, ThresholdT_DueS, ScaleErrorSourceRatio));
		}

		// Start compressing translation using a totally lossless float32x3
		const FTranslationTrack& TranslationTrack = TranslationData[TrackIndex];

		Params.bIncludeKeyTable = bActuallyFilterLinearKeys && !FAnimationUtils::HasUniformKeySpacing(AnimSeq, TranslationTrack.Times);
		FPerTrackCompressor BestTranslation(ACF_Float96NoW, TranslationTrack, Params);

		// Try the other translation formats
		for (int32 FormatIndex = 0; FormatIndex < AllowedTranslationFormats.Num(); ++FormatIndex)
		{
			FPerTrackCompressor TrialCompression(AllowedTranslationFormats[FormatIndex], TranslationTrack, Params);

			if (TrialCompression.MaxError <= MaxPositionErrorCutoff)
			{
				// Swap if it's smaller or equal-sized but lower-max-error
				const int32 BytesSaved = BestTranslation.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
				const bool bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestTranslation.MaxError));

				if (bIsImprovement)
				{
					BestTranslation = TrialCompression;
				}
			}
		}

		// Start compressing rotation, first using lossless float32x3
		const FRotationTrack& RotationTrack = RotationData[TrackIndex];

		Params.bIncludeKeyTable = bActuallyFilterLinearKeys && !FAnimationUtils::HasUniformKeySpacing(AnimSeq, RotationTrack.Times);
		FPerTrackCompressor BestRotation(ACF_Float96NoW, RotationTrack, Params);

		//bool bLeaveRotationUncompressed = (RotationTrack.Times.Num() <= 1) && (GHighQualityEmptyTracks != 0);
		// Try the other rotation formats
		//if (!bLeaveRotationUncompressed)
		{
			for (int32 FormatIndex = 0; FormatIndex < AllowedRotationFormats.Num(); ++FormatIndex)
			{
				FPerTrackCompressor TrialCompression(AllowedRotationFormats[FormatIndex], RotationTrack, Params);

				if (TrialCompression.MaxError <= MaxAngleErrorCutoff)
				{
					// Swap if it's smaller or equal-sized but lower-max-error
					const int32 BytesSaved = BestRotation.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
					const bool bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestRotation.MaxError));

					if (bIsImprovement)
					{
						BestRotation = TrialCompression;
					}
				}
			}
		}

		// Start compressing Scale, first using lossless float32x3
		if (bHasScale)
		{
			const FScaleTrack& ScaleTrack = ScaleData[TrackIndex];

			Params.bIncludeKeyTable = bActuallyFilterLinearKeys && !FAnimationUtils::HasUniformKeySpacing(AnimSeq, ScaleTrack.Times);
			FPerTrackCompressor BestScale(ACF_Float96NoW, ScaleTrack, Params);

			//bool bLeaveScaleUncompressed = (ScaleTrack.Times.Num() <= 1) && (GHighQualityEmptyTracks != 0);
			// Try the other Scale formats
			//if (!bLeaveScaleUncompressed)
			{
				for (int32 FormatIndex = 0; FormatIndex < AllowedScaleFormats.Num(); ++FormatIndex)
				{
					FPerTrackCompressor TrialCompression(AllowedScaleFormats[FormatIndex], ScaleTrack, Params);

					if (TrialCompression.MaxError <= MaxAngleErrorCutoff)
					{
						// Swap if it's smaller or equal-sized but lower-max-error
						const int32 BytesSaved = BestScale.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
						const bool bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestScale.MaxError));

						if (bIsImprovement)
						{
							BestScale = TrialCompression;
						}
					}
				}
			}

			int32 ScaleOffset = INDEX_NONE;
			if (BestScale.CompressedBytes.Num() > 0)
			{
				check(BestScale.ActualCompressionMode < ACF_MAX);
				ScaleOffset = AnimSeq->CompressedByteStream.Num();
				AnimSeq->CompressedByteStream.Append(BestScale.CompressedBytes);
			}
			AnimSeq->CompressedScaleOffsets.SetOffsetData(TrackIndex, 0, ScaleOffset);
		}

		// Now write out compression and translation frames into the stream
		int32 TranslationOffset = INDEX_NONE;
		if (BestTranslation.CompressedBytes.Num() > 0 )
		{
			check(BestTranslation.ActualCompressionMode < ACF_MAX);
			TranslationOffset = AnimSeq->CompressedByteStream.Num();
			AnimSeq->CompressedByteStream.Append(BestTranslation.CompressedBytes);
		}
		AnimSeq->CompressedTrackOffsets[TrackIndex*2 + 0] = TranslationOffset;

		int32 RotationOffset = INDEX_NONE;
		if (BestRotation.CompressedBytes.Num() > 0)
		{
			check(BestRotation.ActualCompressionMode < ACF_MAX);
			RotationOffset = AnimSeq->CompressedByteStream.Num();
			AnimSeq->CompressedByteStream.Append(BestRotation.CompressedBytes);
		}
		AnimSeq->CompressedTrackOffsets[TrackIndex*2 + 1] = RotationOffset;
	
#if 0
		// This block outputs information about each individual track during compression, which is useful for debugging the compressors
		UE_LOG(LogAnimationCompression, Warning, TEXT("   Compressed track %i, Trans=%s_%i (#keys=%i, err=%f), Rot=%s_%i (#keys=%i, err=%f)  (height=%i max pos=%f, angle=%f)"), 
			TrackIndex,
			*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(BestTranslation.ActualCompressionMode)),
			BestTranslation.ActualCompressionMode != ACF_Identity ? ( ( *( (const int32*)BestTranslation.CompressedBytes.GetTypedData() ) ) >> 24) & 0xF : 0,
			TranslationTrack.PosKeys.Num(),
			BestTranslation.MaxError,
			*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(BestRotation.ActualCompressionMode)),
			BestRotation.ActualCompressionMode != ACF_Identity ? ( ( *( (const int32*)BestRotation.CompressedBytes.GetTypedData() ) ) >> 24) & 0xF : 0,
			RotationTrack.RotKeys.Num(),
			BestRotation.MaxError,
			(bUseAdaptiveError)? (Cache->TrackHeights(TrackIndex)): -1,
			MaxPositionErrorCutoff,
			MaxAngleErrorCutoff
			);
#endif
	}
}

void UAnimCompress_PerTrackCompression::PackTranslationKey(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, const FPerTrackFormat& TrackFormat)
{
	const bool bHasX = TrackFormat.TranslationKeyFlags.IsComponentNeededX();
	const bool bHasY = TrackFormat.TranslationKeyFlags.IsComponentNeededY();
	const bool bHasZ = TrackFormat.TranslationKeyFlags.IsComponentNeededZ();

	if (!bHasX && !bHasY && !bHasZ)
	{
		// No point in using this over the identity encoding
		return;
	}

	switch (Format)
	{
	case ACF_Identity:
		// Nothing to pack
		break;
	case ACF_None:
	case ACF_Float96NoW:
		if (bHasX)
		{
			UnalignedWriteToStream(ByteStream, &Key.X, sizeof(float));
		}
		if (bHasY)
		{
			UnalignedWriteToStream(ByteStream, &Key.Y, sizeof(float));
		}
		if (bHasZ)
		{
			UnalignedWriteToStream(ByteStream, &Key.Z, sizeof(float));
		}
		break;
	case ACF_Fixed48NoW:
		if (bHasX)
		{
			const uint16 X = FAnimationCompression_PerTrackUtils::CompressFixed16(Key.X, LogScale);
			UnalignedWriteToStream(ByteStream, &X, sizeof(uint16));
		}
		if (bHasY)
		{
			const uint16 Y = FAnimationCompression_PerTrackUtils::CompressFixed16(Key.Y, LogScale);
			UnalignedWriteToStream(ByteStream, &Y, sizeof(uint16));
		}
		if (bHasZ)
		{
			const uint16 Z = FAnimationCompression_PerTrackUtils::CompressFixed16(Key.Z, LogScale);
			UnalignedWriteToStream(ByteStream, &Z, sizeof(uint16));
		}
		break;
	case ACF_IntervalFixed32NoW:
		{
			const float MaskedMins[3] = { bHasX ? Mins[0] : 0.0f, bHasY ? Mins[1] : 0.0f, bHasZ ? Mins[2] : 0.0f };
			const float MaskedRanges[3] = { bHasX ? Ranges[0] : 0.0f, bHasY ? Ranges[1] : 0.0f, bHasZ ? Ranges[2] : 0.0f };
			const FVectorIntervalFixed32NoW Compressor(Key, MaskedMins, MaskedRanges);
			UnalignedWriteToStream(ByteStream, &Compressor, sizeof(FVectorIntervalFixed32NoW));
		}
		break;
		// The following two formats don't work well for translation (fixed range & low precision)
		//case ACF_Fixed32NoW:
		//case ACF_Float32NoW:
	default:
		UE_LOG(LogAnimationCompression, Fatal, TEXT("Unsupported translation compression format"));
		break;
	}
}

void UAnimCompress_PerTrackCompression::PackRotationKey(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, const FPerTrackFormat& TrackFormat)
{
	const bool bHasX = TrackFormat.RotationKeyFlags.IsComponentNeededX();
	const bool bHasY = TrackFormat.RotationKeyFlags.IsComponentNeededY();
	const bool bHasZ = TrackFormat.RotationKeyFlags.IsComponentNeededZ();

	if (!bHasX && !bHasY && !bHasZ && Format != ACF_Float96NoW && Format != ACF_Fixed32NoW && Format != ACF_Float32NoW)
	{
		// No point in using this over the identity encoding
		return;
	}

	switch (Format)
	{
	case ACF_Identity:
		// Nothing to pack
		break;
	case ACF_None:
	case ACF_Float96NoW:
		{
			const FQuatFloat96NoW Compressor(Key);
			UnalignedWriteToStream(ByteStream, &Compressor, sizeof(FQuatFloat96NoW));
		}
		break;
	case ACF_Fixed48NoW:
		{
			FQuat MaskedKey(bHasX ? Key.X : 0.0f, bHasY ? Key.Y : 0.0f, bHasZ ? Key.Z : 0.0f, Key.W);
			MaskedKey.Normalize();

			const FQuatFloat96NoW Compressor(MaskedKey);

			if (bHasX)
			{
				const uint16 X = FAnimationCompression_PerTrackUtils::CompressFixed16(Compressor.X);
				UnalignedWriteToStream(ByteStream, &X, sizeof(uint16));
			}
			if (bHasY)
			{
				const uint16 Y = FAnimationCompression_PerTrackUtils::CompressFixed16(Compressor.Y);
				UnalignedWriteToStream(ByteStream, &Y, sizeof(uint16));
			}
			if (bHasZ)
			{
				const uint16 Z = FAnimationCompression_PerTrackUtils::CompressFixed16(Compressor.Z);
				UnalignedWriteToStream(ByteStream, &Z, sizeof(uint16));
			}
		}
		break;
	case ACF_IntervalFixed32NoW:
		{
			const float MaskedMins[3] = { bHasX ? Mins[0] : 0.0f, bHasY ? Mins[1] : 0.0f, bHasZ ? Mins[2] : 0.0f };
			const float MaskedRanges[3] = { bHasX ? Ranges[0] : 0.0f, bHasY ? Ranges[1] : 0.0f, bHasZ ? Ranges[2] : 0.0f };
			FQuat MaskedKey(bHasX ? Key.X : 0.0f, bHasY ? Key.Y : 0.0f, bHasZ ? Key.Z : 0.0f, Key.W);
			MaskedKey.Normalize();

			const FQuatIntervalFixed32NoW Compressor(MaskedKey, MaskedMins, MaskedRanges);
			UnalignedWriteToStream(ByteStream, &Compressor, sizeof(Compressor));
		}
		break;
	case ACF_Fixed32NoW:
		{
			const FQuatFixed32NoW Compressor(Key);
			UnalignedWriteToStream(ByteStream, &Compressor, sizeof(FQuatFixed32NoW));
		}
		break;
	case ACF_Float32NoW:
		{
			const FQuatFloat32NoW Compressor(Key);
			UnalignedWriteToStream(ByteStream, &Compressor, sizeof(FQuatFloat32NoW));
		}
		break;
	default:
		UE_LOG(LogAnimationCompression, Fatal, TEXT("Unsupported rotation compression format"));
		break;
	}
}

void UAnimCompress_PerTrackCompression::PackScaleKey(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, const FPerTrackFormat& TrackFormat)
{
	const bool bHasX = TrackFormat.ScaleKeyFlags.IsComponentNeededX();
	const bool bHasY = TrackFormat.ScaleKeyFlags.IsComponentNeededY();
	const bool bHasZ = TrackFormat.ScaleKeyFlags.IsComponentNeededZ();

	if (!bHasX && !bHasY && !bHasZ)
	{
		// No point in using this over the identity encoding
		return;
	}

	switch (Format)
	{
	case ACF_Identity:
		// Nothing to pack
		break;
	case ACF_None:
	case ACF_Float96NoW:
		if (bHasX)
		{
			UnalignedWriteToStream(ByteStream, &Key.X, sizeof(float));
		}
		if (bHasY)
		{
			UnalignedWriteToStream(ByteStream, &Key.Y, sizeof(float));
		}
		if (bHasZ)
		{
			UnalignedWriteToStream(ByteStream, &Key.Z, sizeof(float));
		}
		break;
	case ACF_Fixed48NoW:
		if (bHasX)
		{
			const uint16 X = FAnimationCompression_PerTrackUtils::CompressFixed16(Key.X, LogScale);
			UnalignedWriteToStream(ByteStream, &X, sizeof(uint16));
		}
		if (bHasY)
		{
			const uint16 Y = FAnimationCompression_PerTrackUtils::CompressFixed16(Key.Y, LogScale);
			UnalignedWriteToStream(ByteStream, &Y, sizeof(uint16));
		}
		if (bHasZ)
		{
			const uint16 Z = FAnimationCompression_PerTrackUtils::CompressFixed16(Key.Z, LogScale);
			UnalignedWriteToStream(ByteStream, &Z, sizeof(uint16));
		}
		break;
	case ACF_IntervalFixed32NoW:
		{
			const float MaskedMins[3] = { bHasX ? Mins[0] : 0.0f, bHasY ? Mins[1] : 0.0f, bHasZ ? Mins[2] : 0.0f };
			const float MaskedRanges[3] = { bHasX ? Ranges[0] : 0.0f, bHasY ? Ranges[1] : 0.0f, bHasZ ? Ranges[2] : 0.0f };
			const FVectorIntervalFixed32NoW Compressor(Key, MaskedMins, MaskedRanges);
			UnalignedWriteToStream(ByteStream, &Compressor, sizeof(FVectorIntervalFixed32NoW));
		}
	break;
	// The following two formats don't work well for scale (fixed range & low precision)
	//case ACF_Fixed32NoW:
	//case ACF_Float32NoW:
	default:
		UE_LOG(LogAnimationCompression, Fatal, TEXT("Unsupported scale compression format"));
		break;
	}
}

/**
 * Structure that holds the necessary information for performing the per track compression.
 * Each segment will have its own instance. Each instance is independent.
 * This makes it safe to compress multiple segments in parallel.
 */
struct FOptimizeSegmentTracksContext
{
	const UAnimSequence& AnimSeq;
	FAnimSegmentContext& Segment;

	FOptimizeSegmentTracksContext(
		const UAnimSequence& AnimSeq_,
		FAnimSegmentContext& Segment_)
		: AnimSeq(AnimSeq_)
		, Segment(Segment_)
	{}
};

/**
 * Struct that holds the relevant information to optimize segment tracks in parallel.
 * Instances of this structure are live as long as parallel task instances are live.
 */
struct FAsyncOptimizeSegmentTracksTaskGroupContext
{
	TArray<FOptimizeSegmentTracksContext*> TaskContexes;
	volatile int32 AtomicTaskIndexCounter;
	volatile int32 AtomicNumExecutedTasks;

	FAsyncOptimizeSegmentTracksTaskGroupContext()
		: AtomicTaskIndexCounter(0)
		, AtomicNumExecutedTasks(0)
	{}

	~FAsyncOptimizeSegmentTracksTaskGroupContext()
	{
		for (FOptimizeSegmentTracksContext* Context : TaskContexes)
		{
			delete Context;
		}
	}

	void ExecuteTasks(UAnimCompress_PerTrackCompression* Compressor)
	{
		while (true)
		{
			const int32 TaskIndex = FPlatformAtomics::InterlockedIncrement(&AtomicTaskIndexCounter) - 1;
			if (TaskIndex >= TaskContexes.Num())
			{
				break;
			}

			FOptimizeSegmentTracksContext* JobContext = TaskContexes[TaskIndex];
			Compressor->OptimizeSegmentTracks(*JobContext);

			FPlatformAtomics::InterlockedIncrement(&AtomicNumExecutedTasks);
		}
	}

	void WaitForAllTasks()
	{
		// We just spin wait until everything is done
		// This is a decent option because segments are already sorted largest to smallest and so
		// they should all take about the same amount of time. We should never end up waiting here for too long.
		while (AtomicNumExecutedTasks != TaskContexes.Num());
	}
};

/**
 * Class that represents a parallel task to perform per track compression.
 */
class FAsyncOptimizeSegmentTracksTask
{
public:
	FAsyncOptimizeSegmentTracksTask(UAnimCompress_PerTrackCompression* Compressor_, FAsyncOptimizeSegmentTracksTaskGroupContext* TaskGroupContext_)
		: Compressor(Compressor_)
		, TaskGroupContext(TaskGroupContext_)
	{}

	/** return the name of the task **/
	static const TCHAR* GetTaskName() { return TEXT("FAsyncOptimizeSegmentTracksTask"); }
	FORCEINLINE static TStatId GetStatId() { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncOptimizeSegmentTracksTask, STATGROUP_TaskGraphTasks); }

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskGroupContext->ExecuteTasks(Compressor);
	}

	UAnimCompress_PerTrackCompression* Compressor;
	FAsyncOptimizeSegmentTracksTaskGroupContext* TaskGroupContext;
};

/**
 * Class that represents a clean up task once every parallel task has executed.
 * This is executed last.
 */
class FAsyncCleanUpOptimizeSegmentTracksTask
{
public:
	FAsyncCleanUpOptimizeSegmentTracksTask(FAsyncOptimizeSegmentTracksTaskGroupContext* TaskGroupContext_)
		: TaskGroupContext(TaskGroupContext_)
	{}

	/** return the name of the task **/
	static const TCHAR* GetTaskName() { return TEXT("FAsyncCleanUpOptimizeSegmentTracksTask"); }
	FORCEINLINE static TStatId GetStatId() { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCleanUpOptimizeSegmentTracksTask, STATGROUP_TaskGraphTasks); }

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		delete TaskGroupContext;
	}

	FAsyncOptimizeSegmentTracksTaskGroupContext* TaskGroupContext;
};

void UAnimCompress_PerTrackCompression::OptimizeSegmentTracks(FOptimizeSegmentTracksContext& Context) const
{
	// Prime the compression buffers
	check(Context.Segment.TranslationData.Num() == Context.Segment.RotationData.Num());
	const int32 NumTracks = Context.Segment.TranslationData.Num();
	const bool bHasScale = Context.Segment.ScaleData.Num() > 0;

	TArray<FPerTrackFormat> BestTrackFormats;
	BestTrackFormats.Empty(NumTracks);
	BestTrackFormats.AddUninitialized(NumTracks);

	check(PerReductionCachedData != nullptr);
	const FPerTrackCachedInfo* Cache = PerReductionCachedData;

	// Compress each track independently
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		// Compression parameters / thresholds
		FPerTrackParams Params;
		Params.AnimSeq = &Context.AnimSeq;
		Params.MaxZeroingThreshold = MaxZeroingThreshold;

		FPerTrackFormat& TrackFormats = BestTrackFormats[TrackIndex];

		// Determine the local-space error cutoffs
		float MaxPositionErrorCutoff = MaxPosDiffBitwise;
		float MaxAngleErrorCutoff = MaxAngleDiffBitwise;
		float MaxScaleErrorCutoff = MaxScaleDiffBitwise;

		if (bUseAdaptiveError)
		{
			// The height of the track is the distance from an end effector.  It's used to reduce the acceptable error the
			// higher in the skeleton we get, since a higher bone will cause cascading errors everywhere.
			const int32 PureTrackHeight = Cache->TrackHeights[TrackIndex];
			const int32 EffectiveTrackHeight = FMath::Max(0, PureTrackHeight + TrackHeightBias);

			const float Scaler = 1.0f / FMath::Pow(FMath::Max(ParentingDivisor, 1.0f), EffectiveTrackHeight * FMath::Max(0.0f, ParentingDivisorExponent));

			MaxPositionErrorCutoff = FMath::Max<float>(MaxZeroingThreshold, MaxPosDiff * Scaler);
			MaxAngleErrorCutoff = FMath::Max<float>(MaxZeroingThreshold, MaxAngleDiff * Scaler);
			MaxScaleErrorCutoff = FMath::Max<float>(MaxZeroingThreshold, MaxScaleDiff * Scaler);

			if (bUseOverrideForEndEffectors && (PureTrackHeight == 0))
			{
				MaxPositionErrorCutoff = MinEffectorDiff;
			}
		}
		else if (bUseAdaptiveError2)
		{
			const FAnimPerturbationError& TrackError = Cache->PerTrackErrors[TrackIndex];

			float ThresholdT_DueR = (TrackError.MaxErrorInTransDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToRot) : 1.0f;
			float ThresholdT_DueT = (TrackError.MaxErrorInTransDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToTrans) : 1.0f;
			float ThresholdT_DueS = (TrackError.MaxErrorInTransDueToScale > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToScale) : 1.0f;

			//@TODO: Mixing spaces (target angle error is in radians, perturbation is in quaternion component units)
			float ThresholdR_DueR = (TrackError.MaxErrorInRotDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToRot) : 1.0f;
			float ThresholdR_DueT = (TrackError.MaxErrorInRotDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToTrans) : 1.0f;
			float ThresholdR_DueS = (TrackError.MaxErrorInRotDueToScale > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToScale) : 1.0f;

			// these values are not used, so I don't think we should calculate?
			// 			float ThresholdS_DueR = (TrackError.MaxErrorInScaleDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInScaleDueToRot) : 1.0f;
			// 			float ThresholdS_DueT = (TrackError.MaxErrorInScaleDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInScaleDueToTrans) : 1.0f;
			// 			float ThresholdS_DueS = (TrackError.MaxErrorInScaleDueToScale > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInScaleDueToScale) : 1.0f;

			// @Todo fix the error - this doesn't make sense
			MaxAngleErrorCutoff = FMath::Min(MaxAngleDiffBitwise, MaxErrorPerTrackRatio * MaxAngleDiff * FMath::Lerp(ThresholdR_DueR, ThresholdT_DueR, RotationErrorSourceRatio));
			MaxPositionErrorCutoff = FMath::Min(MaxPosDiffBitwise, MaxErrorPerTrackRatio * MaxPosDiff * FMath::Lerp(ThresholdR_DueT, ThresholdT_DueT, TranslationErrorSourceRatio));
			MaxScaleErrorCutoff = FMath::Min(MaxScaleDiffBitwise, MaxErrorPerTrackRatio * MaxScaleDiff * FMath::Lerp(ThresholdR_DueS, ThresholdT_DueS, ScaleErrorSourceRatio));
		}

		// Start compressing translation using a totally lossless float32x3
		const FTranslationTrack& TranslationTrack = Context.Segment.TranslationData[TrackIndex];

		Params.bIncludeKeyTable = bActuallyFilterLinearKeys && TranslationTrack.PosKeys.Num() != Context.Segment.NumFrames;
		FPerTrackCompressor BestTranslation(ACF_Float96NoW, TranslationTrack, Params);

		// Try the other translation formats
		for (int32 FormatIndex = 0; FormatIndex < AllowedTranslationFormats.Num(); ++FormatIndex)
		{
			FPerTrackCompressor TrialCompression(AllowedTranslationFormats[FormatIndex], TranslationTrack, Params);

			if (TrialCompression.MaxError <= MaxPositionErrorCutoff)
			{
				// Swap if it's smaller or equal-sized but lower-max-error
				const int32 BytesSaved = BestTranslation.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
				const bool bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestTranslation.MaxError));

				if (bIsImprovement)
				{
					BestTranslation = TrialCompression;
				}
			}
		}
		TrackFormats.TranslationFormat = BestTranslation.ActualCompressionMode;
		TrackFormats.bHasTranslationTimeMarkers = BestTranslation.bReallyNeedsFrameTable;
		TrackFormats.TranslationKeyFlags = FTrackKeyFlags(BestTranslation.ActualKeyFlags);

		// Start compressing rotation, first using lossless float32x3
		const FRotationTrack& RotationTrack = Context.Segment.RotationData[TrackIndex];

		Params.bIncludeKeyTable = bActuallyFilterLinearKeys && RotationTrack.RotKeys.Num() != Context.Segment.NumFrames;
		FPerTrackCompressor BestRotation(ACF_Float96NoW, RotationTrack, Params);

		//bool bLeaveRotationUncompressed = (RotationTrack.Times.Num() <= 1) && (GHighQualityEmptyTracks != 0);
		// Try the other rotation formats
		//if (!bLeaveRotationUncompressed)
		{
			for (int32 FormatIndex = 0; FormatIndex < AllowedRotationFormats.Num(); ++FormatIndex)
			{
				FPerTrackCompressor TrialCompression(AllowedRotationFormats[FormatIndex], RotationTrack, Params);

				if (TrialCompression.MaxError <= MaxAngleErrorCutoff)
				{
					// Swap if it's smaller or equal-sized but lower-max-error
					const int32 BytesSaved = BestRotation.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
					const bool bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestRotation.MaxError));

					if (bIsImprovement)
					{
						BestRotation = TrialCompression;
					}
				}
			}
		}
		TrackFormats.RotationFormat = BestRotation.ActualCompressionMode;
		TrackFormats.bHasRotationTimeMarkers = BestRotation.bReallyNeedsFrameTable;
		TrackFormats.RotationKeyFlags = FTrackKeyFlags(BestRotation.ActualKeyFlags);

		// Start compressing Scale, first using lossless float32x3
		TrackFormats.ScaleFormat = ACF_None;
		TrackFormats.bHasScaleTimeMarkers = false;
		TrackFormats.ScaleKeyFlags = FTrackKeyFlags();
		if (bHasScale)
		{
			const FScaleTrack& ScaleTrack = Context.Segment.ScaleData[TrackIndex];

			Params.bIncludeKeyTable = bActuallyFilterLinearKeys && ScaleTrack.ScaleKeys.Num() != Context.Segment.NumFrames;
			FPerTrackCompressor BestScale(ACF_Float96NoW, ScaleTrack, Params);

			//bool bLeaveScaleUncompressed = (ScaleTrack.Times.Num() <= 1) && (GHighQualityEmptyTracks != 0);
			// Try the other Scale formats
			//if (!bLeaveScaleUncompressed)
			{
				for (int32 FormatIndex = 0; FormatIndex < AllowedScaleFormats.Num(); ++FormatIndex)
				{
					FPerTrackCompressor TrialCompression(AllowedScaleFormats[FormatIndex], ScaleTrack, Params);

					if (TrialCompression.MaxError <= MaxAngleErrorCutoff)
					{
						// Swap if it's smaller or equal-sized but lower-max-error
						const int32 BytesSaved = BestScale.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
						const bool bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestScale.MaxError));

						if (bIsImprovement)
						{
							BestScale = TrialCompression;
						}
					}
				}
			}

			TrackFormats.ScaleFormat = BestScale.ActualCompressionMode;
			TrackFormats.bHasScaleTimeMarkers = BestScale.bReallyNeedsFrameTable;
			TrackFormats.ScaleKeyFlags = FTrackKeyFlags(BestScale.ActualKeyFlags);
		}

#if 0
		// This block outputs information about each individual track during compression, which is useful for debugging the compressors
		UE_LOG(LogAnimationCompression, Warning, TEXT("   Compressed track %i, Trans=%s_%i (#keys=%i, err=%f), Rot=%s_%i (#keys=%i, err=%f)  (height=%i max pos=%f, angle=%f)"),
			TrackIndex,
			*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(BestTranslation.ActualCompressionMode)),
			BestTranslation.ActualCompressionMode != ACF_Identity ? ((*((const int32*)BestTranslation.CompressedBytes.GetTypedData())) >> 24) & 0xF : 0,
			TranslationTrack.PosKeys.Num(),
			BestTranslation.MaxError,
			*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(BestRotation.ActualCompressionMode)),
			BestRotation.ActualCompressionMode != ACF_Identity ? ((*((const int32*)BestRotation.CompressedBytes.GetTypedData())) >> 24) & 0xF : 0,
			RotationTrack.RotKeys.Num(),
			BestRotation.MaxError,
			(bUseAdaptiveError) ? (Cache->TrackHeights(TrackIndex)) : -1,
			MaxPositionErrorCutoff,
			MaxAngleErrorCutoff
		);
#endif
	}

	SanityCheckTrackData(Context.AnimSeq, Context.Segment);

	Context.Segment.CompressedByteStream.Empty(64 * 1024);

	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		const FPerTrackFormat& TrackFormats = BestTrackFormats[TrackIndex];

		const FPerTrackFlags TranslationFlags(TrackFormats.bHasTranslationTimeMarkers, TrackFormats.TranslationFormat, TrackFormats.TranslationKeyFlags.Flags);
		UnalignedWriteToStream(Context.Segment.CompressedByteStream, &TranslationFlags, sizeof(FPerTrackFlags));

		const FPerTrackFlags RotationFlags(TrackFormats.bHasRotationTimeMarkers, TrackFormats.RotationFormat, TrackFormats.RotationKeyFlags.Flags);
		UnalignedWriteToStream(Context.Segment.CompressedByteStream, &RotationFlags, sizeof(FPerTrackFlags));

		if (bHasScale)
		{
			const FPerTrackFlags ScaleFlags(TrackFormats.bHasScaleTimeMarkers, TrackFormats.ScaleFormat, TrackFormats.ScaleKeyFlags.Flags);
			UnalignedWriteToStream(Context.Segment.CompressedByteStream, &ScaleFlags, sizeof(FPerTrackFlags));
		}
	}

	PadByteStream(Context.Segment.CompressedByteStream, 4, AnimationPadSentinel);

	TArray<FAnimTrackRange> TrackRanges;
	CalculateTrackRanges(ACF_IntervalFixed32NoW, ACF_IntervalFixed32NoW, ACF_IntervalFixed32NoW, Context.Segment, TrackRanges);

	checkf((Context.Segment.CompressedByteStream.Num() % 4) == 0, TEXT("CompressedByteStream not aligned to four bytes"));

	// Write track ranges
	WriteTrackRanges(
		Context.Segment.CompressedByteStream,
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].TranslationFormat; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].RotationFormat; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].ScaleFormat; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].TranslationKeyFlags; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].RotationKeyFlags; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].ScaleKeyFlags; },
		Context.Segment, TrackRanges, true);

	checkf((Context.Segment.CompressedByteStream.Num() % 4) == 0, TEXT("CompressedByteStream not aligned to four bytes"));

	WriteUniformTrackData(
		Context.Segment.CompressedByteStream,
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].TranslationFormat; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].RotationFormat; },
		[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].ScaleFormat; },
		[&](int32 TrackIndex) { return !BestTrackFormats[TrackIndex].bHasTranslationTimeMarkers; },
		[&](int32 TrackIndex) { return !BestTrackFormats[TrackIndex].bHasRotationTimeMarkers; },
		[&](int32 TrackIndex) { return !BestTrackFormats[TrackIndex].bHasScaleTimeMarkers; },
		[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackTranslationKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
		[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackRotationKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
		[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackScaleKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
		Context.Segment, TrackRanges);

	PadByteStream(Context.Segment.CompressedByteStream, 4, AnimationPadSentinel);

	if (bOptimizeForForwardPlayback)
	{
		WriteSortedVariableTrackData(
			Context.Segment.CompressedByteStream,
			Context.AnimSeq,
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].TranslationFormat; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].RotationFormat; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].ScaleFormat; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].bHasTranslationTimeMarkers; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].bHasRotationTimeMarkers; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].bHasScaleTimeMarkers; },
			[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackTranslationKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
			[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackRotationKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
			[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackScaleKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
			Context.Segment, TrackRanges);
	}
	else
	{
		WriteLinearVariableTrackData(
			Context.Segment.CompressedByteStream,
			Context.AnimSeq,
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].TranslationFormat; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].RotationFormat; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].ScaleFormat; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].bHasTranslationTimeMarkers; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].bHasRotationTimeMarkers; },
			[&](int32 TrackIndex) { return BestTrackFormats[TrackIndex].bHasScaleTimeMarkers; },
			[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackTranslationKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
			[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackRotationKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
			[&](TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex) { PackScaleKey(ByteStream, Format, Key, Mins, Ranges, BestTrackFormats[TrackIndex]); },
			Context.Segment, TrackRanges);
	}

	// Make sure we have a safe alignment
	PadByteStream(Context.Segment.CompressedByteStream, 4, AnimationPadSentinel);

	// Trim unused memory.
	Context.Segment.CompressedByteStream.Shrink();
}

void UAnimCompress_PerTrackCompression::CompressUsingUnderlyingCompressor(
	UAnimSequence& AnimSeq,
	const TArray<FBoneData>& BoneData,
	TArray<FAnimSegmentContext>& RawSegments,
	const bool bFinalPass)
{
	// If not doing final pass, then do the RemoveLinearKey version that is less destructive.
	// We're potentially removing whole tracks here, and that doesn't work well with LinearKeyRemoval algorithm.
	if (!bFinalPass)
	{
		UAnimCompress_RemoveLinearKeys::CompressUsingUnderlyingCompressor(
			AnimSeq,
			BoneData,
			RawSegments,
			bFinalPass);
		return;
	}

	// record the proper runtime decompressor to use
	AnimSeq.KeyEncodingFormat = AKF_PerTrackCompression;
	AnimSeq.RotationCompressionFormat = ACF_Identity;
	AnimSeq.TranslationCompressionFormat = ACF_Identity;
	AnimSeq.ScaleCompressionFormat = ACF_Identity;
	AnimationFormat_SetInterfaceLinks(AnimSeq);

	if (bUseDecompression || !bUseMultithreading || RawSegments.Num() <= 1)
	{
		for (FAnimSegmentContext& Segment : RawSegments)
		{
			FOptimizeSegmentTracksContext Context(AnimSeq, Segment);
			OptimizeSegmentTracks(Context);
		}
	}
	else
	{
		// Created the context objects.
		FAsyncOptimizeSegmentTracksTaskGroupContext* TaskGroupContext = new FAsyncOptimizeSegmentTracksTaskGroupContext();
		for (FAnimSegmentContext& Segment : RawSegments)
		{
			FOptimizeSegmentTracksContext* Context = new FOptimizeSegmentTracksContext(AnimSeq, Segment);
			TaskGroupContext->TaskContexes.Add(Context);
		}

		// Dispatch 1 task per job thread.
		FGraphEventArray AsyncTaskCompletionEvents;
		const int32 NumTaskThreads = FTaskGraphInterface::Get().GetNumWorkerThreads();
		for (int32 TaskIndex = 0; TaskIndex < NumTaskThreads; ++TaskIndex)
		{
			AsyncTaskCompletionEvents.Add(TGraphTask<FAsyncOptimizeSegmentTracksTask>::CreateTask(NULL, ENamedThreads::AnyThread).ConstructAndDispatchWhenReady(this, TaskGroupContext));
		}

		// Execute the contexts concurrently.
		TaskGroupContext->ExecuteTasks(this);

		// Wait for all concurrent tasks to be done, we only wait for ones that were executing.
		TaskGroupContext->WaitForAllTasks();

		// Dispatch a cleanup job that will execute once all the tasks are done.
		// The current thread/task no longer needs the TaskGroupContext beyond this point nor anything it references.
		// All tasks that were executing actual compression work are done or queued up and pending
		// and will do nothing since no work remains (they only touch the TaskGroupContext to find more work).
		// Once all tasks are actually done, the clean up task will execute, freeing memory.
		TGraphTask<FAsyncCleanUpOptimizeSegmentTracksTask>::CreateTask(&AsyncTaskCompletionEvents, ENamedThreads::AnyThread).ConstructAndDispatchWhenReady(TaskGroupContext);
	}

	// Ensure we compress the trivial tracks into our first segment
	BitwiseCompressTrivialAnimationTracks(AnimSeq, RawSegments[0]);

	CoalesceCompressedSegments(AnimSeq, RawSegments, bOptimizeForForwardPlayback);
}

/** Resamples a track of position keys */
void ResamplePositionKeys(
	FTranslationTrack& Track, 
	float StartTime,
	float IntervalTime)
{
	const int32 KeyCount = Track.Times.Num();

	// Oddness about the original data: 30 keys will have times from 0..1 *inclusive*, and 30 Hz isn't
	// This means the key spacing needs a boost
	if (KeyCount > 1)
	{
		IntervalTime = IntervalTime * (KeyCount / (float)(KeyCount - 1));
	}

	check(Track.Times.Num() == Track.PosKeys.Num());

	TArray<FVector> NewPosKeys;
	TArray<float> NewTimes;

	NewTimes.Empty(KeyCount);
	NewPosKeys.Empty(KeyCount);

	float FinalTime = Track.Times[KeyCount - 1];

	// step through and retain the desired interval
	int32 CachedIndex = 0;

	float Time = StartTime;
	while (Time <= FinalTime)
	{
		// Find the bracketing current keys
		if (CachedIndex < KeyCount - 1)
		{
			while ((CachedIndex < KeyCount - 1) && (Track.Times[CachedIndex+1] < Time))
			{
				CachedIndex++;
			}
		}

		FVector Value;

		check(Track.Times[CachedIndex] <= Time);
		if (CachedIndex + 1 < KeyCount)
		{
			check(Track.Times[CachedIndex+1] >= Time);

			FVector A = Track.PosKeys[CachedIndex];
			FVector B = Track.PosKeys[CachedIndex + 1];

			float Alpha = (Time - Track.Times[CachedIndex]) / (Track.Times[CachedIndex+1] - Track.Times[CachedIndex]);
			Value = FMath::Lerp(A, B, Alpha);
		}
		else
		{
			Value = Track.PosKeys[CachedIndex];
		}

		NewPosKeys.Add(Value);
		NewTimes.Add(Time);

		Time += IntervalTime;
	}

	NewTimes.Shrink();
	NewPosKeys.Shrink();

	Track.Times = NewTimes;
	Track.PosKeys = NewPosKeys;
}

/** Resamples a track of Scale keys */
void ResampleScaleKeys(
	FScaleTrack& Track, 
	float StartTime,
	float IntervalTime)
{
	const int32 KeyCount = Track.Times.Num();

	// Oddness about the original data: 30 keys will have times from 0..1 *inclusive*, and 30 Hz isn't
	// This means the key spacing needs a boost
	if (KeyCount > 1)
	{
		IntervalTime = IntervalTime * (KeyCount / (float)(KeyCount - 1));
	}

	check(Track.Times.Num() == Track.ScaleKeys.Num());

	TArray<FVector> NewScaleKeys;
	TArray<float> NewTimes;

	NewTimes.Empty(KeyCount);
	NewScaleKeys.Empty(KeyCount);

	float FinalTime = Track.Times[KeyCount - 1];

	// step through and retain the desired interval
	int32 CachedIndex = 0;

	float Time = StartTime;
	while (Time <= FinalTime)
	{
		// Find the bracketing current keys
		if (CachedIndex < KeyCount - 1)
		{
			while ((CachedIndex < KeyCount - 1) && (Track.Times[CachedIndex+1] < Time))
			{
				CachedIndex++;
			}
		}

		FVector Value;

		check(Track.Times[CachedIndex] <= Time);
		if (CachedIndex + 1 < KeyCount)
		{
			check(Track.Times[CachedIndex+1] >= Time);

			FVector A = Track.ScaleKeys[CachedIndex];
			FVector B = Track.ScaleKeys[CachedIndex + 1];

			float Alpha = (Time - Track.Times[CachedIndex]) / (Track.Times[CachedIndex+1] - Track.Times[CachedIndex]);
			Value = FMath::Lerp(A, B, Alpha);
		}
		else
		{
			Value = Track.ScaleKeys[CachedIndex];
		}

		NewScaleKeys.Add(Value);
		NewTimes.Add(Time);

		Time += IntervalTime;
	}

	NewTimes.Shrink();
	NewScaleKeys.Shrink();

	Track.Times = NewTimes;
	Track.ScaleKeys = NewScaleKeys;
}
/**
 * Resamples a track of rotation keys
 */
void ResampleRotationKeys(
	FRotationTrack& Track,
	float StartTime,
	float IntervalTime)
{
	const int32 KeyCount = Track.Times.Num();
	check(Track.Times.Num() == Track.RotKeys.Num());

	// Oddness about the original data: 30 keys will have times from 0..1 *inclusive*, and 30 Hz isn't
	// This means the key spacing needs a boost
	if (KeyCount > 1)
	{
		IntervalTime = IntervalTime * (KeyCount / (float)(KeyCount - 1));
	}

	TArray<FQuat> NewRotKeys;
	TArray<float> NewTimes;

	NewTimes.Empty(KeyCount);
	NewRotKeys.Empty(KeyCount);

	float FinalTime = Track.Times[KeyCount - 1];

	// step through and retain the desired interval
	int32 CachedIndex = 0;

	float Time = StartTime;
	while (Time <= FinalTime)
	{
		// Find the bracketing current keys
		if (CachedIndex < KeyCount - 1)
		{
			while ((CachedIndex < KeyCount - 1) && (Track.Times[CachedIndex+1] < Time))
			{
				CachedIndex++;
			}
		}

		FQuat Value;

		check(Track.Times[CachedIndex] <= Time);
		if (CachedIndex + 1 < KeyCount)
		{
			check(Track.Times[CachedIndex+1] >= Time);

			FQuat A = Track.RotKeys[CachedIndex];
			FQuat B = Track.RotKeys[CachedIndex + 1];

			float Alpha = (Time - Track.Times[CachedIndex]) / (Track.Times[CachedIndex+1] - Track.Times[CachedIndex]);
			Value = FMath::Lerp(A, B, Alpha);
			Value.Normalize();
		}
		else
		{
			Value = Track.RotKeys[CachedIndex];
		}

		NewRotKeys.Add(Value);
		NewTimes.Add(Time);

		Time += IntervalTime;
	}

	NewTimes.Shrink();
	NewRotKeys.Shrink();

	Track.Times = NewTimes;
	Track.RotKeys = NewRotKeys;
}




void ResampleKeys(
	TArray<FTranslationTrack>& PositionTracks, 
	TArray<FRotationTrack>& RotationTracks,
	TArray<FScaleTrack>& ScaleTracks,
	float Interval,
	float Time0 = 0.0f)
{
	check(PositionTracks.Num() == RotationTracks.Num());
	check((Time0 >= 0.0f) && (Interval > 0.0f));
	const bool bHasScaleTracks = ScaleTracks.Num() > 0;

	for (int32 TrackIndex = 0; TrackIndex < PositionTracks.Num(); ++TrackIndex)
	{
		ResamplePositionKeys(PositionTracks[TrackIndex], Time0, Interval);
		ResampleRotationKeys(RotationTracks[TrackIndex], Time0, Interval);
		if (bHasScaleTracks)
		{
			ResampleScaleKeys(ScaleTracks[TrackIndex], Time0, Interval);
		}
	}
}




void UAnimCompress_PerTrackCompression::FilterBeforeMainKeyRemoval(
	UAnimSequence* AnimSeq, 
	const TArray<FBoneData>& BoneData, 
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData, 
	TArray<FScaleTrack>& ScaleData)
{
	const int32 NumTracks = TranslationData.Num();

	// Downsample the keys if enabled
	if ((AnimSeq->GetRawNumberOfFrames() >= MinKeysForResampling) && bResampleAnimation)
	{
		if(AnimSeq->SequenceLength > 0)
		{
			//Make sure we aren't going to oversample the original animation
			const float CurrentFramerate = (AnimSeq->GetRawNumberOfFrames() - 1) / AnimSeq->SequenceLength;
			if (CurrentFramerate > ResampledFramerate)
			{
				ResampleKeys(TranslationData, RotationData, ScaleData, 1.0f / ResampledFramerate, 0.0f);
			}
		}
	}

	// Create the cache
	check(PerReductionCachedData == NULL);
	FPerTrackCachedInfo* Cache = new FPerTrackCachedInfo();
	PerReductionCachedData = Cache;
	
	// Calculate how far each track is from controlling an end effector
	if (bUseAdaptiveError)
	{
		FAnimationUtils::CalculateTrackHeights(AnimSeq, BoneData, NumTracks, /*OUT*/ Cache->TrackHeights);
	}

	// Find out how a small change affects the maximum error in the end effectors
	if (bUseAdaptiveError2)
	{
		FVector TranslationProbe(PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize);
		FQuat RotationProbe(PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize);
		FVector ScaleProbe(PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize);

		FAnimationUtils::TallyErrorsFromPerturbation(
			AnimSeq,
			NumTracks,
			BoneData,
			TranslationProbe,
			RotationProbe,
			ScaleProbe,
			/*OUT*/ Cache->PerTrackErrors);
	}

	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, ScaleData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD, SCALE_ZEROING_THRESHOLD);
}

void UAnimCompress_PerTrackCompression::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();


		// It is an error to set both bUseAdaptiveError and bUseAdaptiveError2 to true at the same time so make sure if 
		// we are enabling one the other is not enabled.
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimCompress_PerTrackCompression, bUseAdaptiveError))
		{
			// We have changed bUseAdaptiveError, bUseAdaptiveError2 can only be true if it was already true
			// and bUseAdaptiveError is false
			bUseAdaptiveError2 = (!bUseAdaptiveError) && bUseAdaptiveError2;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimCompress_PerTrackCompression, bUseAdaptiveError2))
		{
			// We have changed bUseAdaptiveError2, bUseAdaptiveError can only be true if it was already true
			// and bUseAdaptiveError2 is not true
			bUseAdaptiveError = (!bUseAdaptiveError2) && bUseAdaptiveError;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimCompress_PerTrackCompression, AllowedScaleFormats))
		{
			for (TEnumAsByte<enum AnimationCompressionFormat>& ScaleFormat : AllowedScaleFormats)
			{
				if (ScaleFormat == ACF_Fixed32NoW || ScaleFormat == ACF_Float32NoW)
				{
					ScaleFormat = ACF_None;
				}
			}
		}
	}
}

void UAnimCompress_PerTrackCompression::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
	if (FPlatformProperties::HasEditorOnlyData())
	{
		ensure((MaxPosDiffBitwise > 0.0f) && (MaxAngleDiffBitwise > 0.0f) && (MaxScaleDiffBitwise > 0.0f) && (MaxZeroingThreshold >= 0.0f));
		ensure(MaxZeroingThreshold <= MaxPosDiffBitwise);
		ensure(!(bUseAdaptiveError2 && bUseAdaptiveError));

		// Compress
		UAnimCompress_RemoveLinearKeys::DoReduction(AnimSeq, BoneData);

		// Delete the cache
		if (PerReductionCachedData != NULL)
		{
			delete PerReductionCachedData;
		}
		PerReductionCachedData = NULL;
	}
}

void WriteEnumArrayToKey(FArchive& Ar, TArray<TEnumAsByte<enum AnimationCompressionFormat> >& EnumArray)
{
	for (TEnumAsByte<enum AnimationCompressionFormat>& EnumVal : EnumArray)
	{
		uint8 Val = EnumVal.GetValue();
		Ar << Val;
	}
}
void UAnimCompress_PerTrackCompression::PopulateDDCKey(FArchive& Ar)
{
	Super::PopulateDDCKey(Ar);

	Ar << MaxZeroingThreshold;
	Ar << MaxPosDiffBitwise;
	Ar << MaxAngleDiffBitwise;
	Ar << MaxScaleDiffBitwise;
	
	WriteEnumArrayToKey(Ar, AllowedRotationFormats);
	WriteEnumArrayToKey(Ar, AllowedTranslationFormats);
	WriteEnumArrayToKey(Ar, AllowedScaleFormats);
	
	Ar << ResampledFramerate;
	Ar << MinKeysForResampling;
	Ar << TrackHeightBias;
	Ar << ParentingDivisor;
	Ar << ParentingDivisorExponent;
	Ar << RotationErrorSourceRatio;

	Ar << TranslationErrorSourceRatio;
	Ar << ScaleErrorSourceRatio;
	Ar << MaxErrorPerTrackRatio;
	Ar << PerturbationProbeSize;


	uint8 Flags =	MakeBitForFlag(bResampleAnimation, 0) +
					MakeBitForFlag(bUseAdaptiveError, 1) +
					MakeBitForFlag(bUseOverrideForEndEffectors, 2) +
					MakeBitForFlag(bUseAdaptiveError2, 3);
	Ar << Flags;
}
#endif // WITH_EDITOR

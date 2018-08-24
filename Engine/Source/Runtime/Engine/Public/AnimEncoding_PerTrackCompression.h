// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_PerTrackCompression.h: Per-track decompressor.
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "AnimEncoding.h"

class FMemoryArchive;

/**
 * Decompression codec for the per-track compressor.
 */
class AEFPerTrackCompressionCodec : public AnimEncoding
{
public:
	/**
	 * Handles Byte-swapping incoming animation data from a MemoryReader
	 *
	 * @param	Seq					An Animation Sequence to contain the read data.
	 * @param	MemoryReader		The MemoryReader object to read from.
	 * @param	SourceArVersion		The version of the archive that the data is coming from.
	 */
	virtual void ByteSwapIn(UAnimSequence& Seq, FMemoryReader& MemoryReader) override;

	/**
	 * Handles Byte-swapping outgoing animation data to an array of BYTEs
	 *
	 * @param	Seq					An Animation Sequence to write.
	 * @param	SerializedData		The output buffer.
	 * @param	ForceByteSwapping	true is byte swapping is not optional.
	 */
	virtual void ByteSwapOut(
		UAnimSequence& Seq,
		TArray<uint8>& SerializedData, 
		bool ForceByteSwapping) override;

	/**
	 * Extracts a single BoneAtom from an Animation Sequence.
	 *
	 * @param	OutAtom			The BoneAtom to fill with the extracted result.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	virtual void GetBoneAtom(
		FTransform& OutAtom,
		FAnimSequenceDecompressionContext& DecompContext,
		int32 TrackIndex) override;

#if USE_ANIMATION_CODEC_BATCH_SOLVER

	/**
	 * Decompress all requested rotation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	virtual void GetPoseRotations(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext) override;

	/**
	 * Decompress all requested translation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	virtual void GetPoseTranslations(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext) override;

	/**
	 * Decompress all requested Scale components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	virtual void GetPoseScales(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext) override;
#endif

#if USE_SEGMENTING_CONTEXT
	virtual void CreateEncodingContext(FAnimSequenceDecompressionContext& DecompContext) override;
	virtual void ReleaseEncodingContext(FAnimSequenceDecompressionContext& DecompContext) override;
#endif

protected:
	/**
	 * Handles Byte-swapping a single track of animation data from a MemoryReader or to a MemoryWriter
	 *
	 * @param	Seq					The Animation Sequence being operated on.
	 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
	 * @param	Offset				The starting offset into the compressed byte stream for this track (can be INDEX_NONE to indicate an identity track)
	 */
	template<class TArchive>
	static void ByteSwapOneTrack(UAnimSequence& Seq, TArchive& MemoryStream, int32 Offset);

	/**
	 * Preserves 4 byte alignment within a stream
	 *
	 * @param	TrackData [inout]	The current data offset (will be returned four byte aligned from the start of the compressed byte stream)
	 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
	 */
	static void PreservePadding(uint8*& TrackData, FMemoryArchive& MemoryStream);

	/**
	 * Decompress the Rotation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	static void GetBoneAtomRotation(
		FTransform& OutAtom,
		FAnimSequenceDecompressionContext& DecompContext,
		int32 TrackIndex);

	/**
	 * Decompress the Translation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	static void GetBoneAtomTranslation(
		FTransform& OutAtom,
		FAnimSequenceDecompressionContext& DecompContext,
		int32 TrackIndex);

	/**
	 * Decompress the Scale component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	static void GetBoneAtomScale(
		FTransform& OutAtom,
		FAnimSequenceDecompressionContext& DecompContext,
		int32 TrackIndex);
};

/**
 * Structure to wrap per track flags.
 */
struct FPerTrackFlags
{
	constexpr explicit FPerTrackFlags(uint8 InFlags) : Flags(InFlags) {}
	FPerTrackFlags(bool bHasTimeMarkers, AnimationCompressionFormat Format, uint8 FormatFlags)
		: Flags((bHasTimeMarkers ? 0x80 : 0) | (FormatFlags << 4) | (uint8)Format)
	{
		check((FormatFlags & ~0x7) == 0);
		check(((uint8)Format & ~0xF) == 0);
	}

	constexpr bool IsUniform() const { return (Flags & 0x80) == 0; }
	constexpr uint8 GetFormatFlags() const { return (Flags >> 4) & 0x7; }
	constexpr uint8 GetFormat() const { return Flags & 0xF; }

	uint8 Flags;
};

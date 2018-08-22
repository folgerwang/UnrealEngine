// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_RemoveLinearKeys.cpp: Keyframe reduction algorithm that simply removes every second key.
=============================================================================*/ 

#include "Animation/AnimCompress_RemoveLinearKeys.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"
#include "Misc/FeedbackContext.h"

// Define to 1 to enable timing of the meat of linear key removal done in DoReduction
// The times are non-trivial, but the extra log spam isn't useful if one isn't optimizing DoReduction runtime
#define TIME_LINEAR_KEY_REMOVAL 0

/**
 * Helper function to enforce that the delta between two Quaternions represents
 * the shortest possible rotation angle
 */
static FQuat EnforceShortestArc(const FQuat& A, const FQuat& B)
{
	const float DotResult = (A | B);
	const float Bias = FMath::FloatSelect(DotResult, 1.0f, -1.0f);
	return B*Bias;
}

/**
 * Helper template function to interpolate between two data types.
 * Used in the FilterLinearKeysTemplate function below
 */
template <typename T>
T Interpolate(const T& A, const T& B, float Alpha)
{
	// only the custom instantiations below are valid
	check(0);
	return 0;
}

/** custom instantiation of Interpolate for FVectors */
template <> FVector Interpolate<FVector>(const FVector& A, const FVector& B, float Alpha)
{
	return FMath::Lerp(A,B,Alpha);
}

/** custom instantiation of Interpolate for FQuats */
template <> FQuat Interpolate<FQuat>(const FQuat& A, const FQuat& B, float Alpha)
{
	FQuat result = FQuat::FastLerp(A,B,Alpha);
	result.Normalize();

	return result;
}

/**
 * Helper template function to calculate the delta between two data types.
 * Used in the FilterLinearKeysTemplate function below
 */
template <typename T>
float CalcDelta(const T& A, const T& B)
{
	// only the custom instantiations below are valid
	check(0);
	return 0;
}

/** custom instantiation of CalcDelta for FVectors */
template <> float CalcDelta<FVector>(const FVector& A, const FVector& B)
{
	return (A - B).Size();
}

/** custom instantiation of CalcDelta for FQuat */
template <> float CalcDelta<FQuat>(const FQuat& A, const FQuat& B)
{
	return FQuat::Error(A, B);
}

UAnimCompress_RemoveLinearKeys::UAnimCompress_RemoveLinearKeys(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bNeedsSkeleton = true;
	Description = TEXT("Remove Linear Keys");
	MaxPosDiff = 0.001f;
	MaxAngleDiff = 0.00075f;
	MaxScaleDiff = 0.000001;
	MaxEffectorDiff = 0.001f;
	MinEffectorDiff = 0.001f;
	EffectorDiffSocket = 0.001f;
	ParentKeyScale = 2.0f;
	bRetarget = true;
	bActuallyFilterLinearKeys = true;
	bOptimizeForForwardPlayback = false;
	bUseDecompression = false;
	bUseMultithreading = true;
}

#if WITH_EDITOR
/**
 * Structure that holds the necessary information for performing linear key reduction.
 * Each segment will have its own instance. Each instance is independent.
 * This makes it safe to compress multiple segments in parallel.
 */
struct FProcessAnimationTracksContext
{
	UAnimSequence& AnimSeq;
	const TArray<FBoneData>& BoneData;
	const TArray<FTransform>& RefPose;
	const TArray<int32>& EndEffectors;
	const TArray<int32>& BoneIndexToTrackIndex;
	const TArray<FTransform>& TrivialTrackValues;

	FAnimSegmentContext& Segment;
	TArray<FAnimSegmentContext>& SegmentList;

	TArray<FTransform> RawWorldBones;
	TArray<FTransform> NewWorldBones;

	FProcessAnimationTracksContext(
		UAnimSequence& AnimSeq_,
		const TArray<FBoneData>& BoneData_,
		const TArray<int32>& EndEffectors_,
		const TArray<int32>& BoneIndexToTrackIndex_,
		const TArray<FTransform>& TrivialTrackValues_,
		FAnimSegmentContext& Segment_,
		TArray<FAnimSegmentContext>& SegmentList_)
		: AnimSeq(AnimSeq_)
		, BoneData(BoneData_)
		, RefPose(AnimSeq_.GetSkeleton()->GetRefLocalPoses())
		, EndEffectors(EndEffectors_)
		, BoneIndexToTrackIndex(BoneIndexToTrackIndex_)
		, TrivialTrackValues(TrivialTrackValues_)
		, Segment(Segment_)
		, SegmentList(SegmentList_)
	{}
};

struct RotationAdapter
{
	typedef FQuat KeyType;

	static FTransform UpdateBoneAtom(const FTransform& Atom, const FQuat& Component) { return FTransform(Component, Atom.GetTranslation(), Atom.GetScale3D()); }
};

struct TranslationAdapter
{
	typedef FVector KeyType;

	static FTransform UpdateBoneAtom(const FTransform& Atom, const FVector& Component) { return FTransform(Atom.GetRotation(), Component, Atom.GetScale3D()); }
};

struct ScaleAdapter
{
	typedef FVector KeyType;

	static FTransform UpdateBoneAtom(const FTransform& Atom, const FVector& Component) { return FTransform(Atom.GetRotation(), Atom.GetTranslation(), Component); }
};

/**
 * Template function to reduce the keys of a given data type.
 * Used to reduce both Translation, Rotation, and Scale keys using the corresponding
 * data types FVector and FQuat
 */
template <typename AdapterType>
void FilterLinearKeysTemplate(
	TArray<typename AdapterType::KeyType>& Keys,
	TArray<float>& Times,
	TArray<FTransform>& BoneAtoms,
	const TArray<float>* ParentTimes,
	const TArray<FTransform>& RawWorldBones,
	const TArray<FTransform>& NewWorldBones,
	const TArray<int32>& TargetBoneIndices,
	int32 NumFrames,
	int32 BoneIndex,
	int32 ParentBoneIndex,
	float ParentScale,
	float MaxDelta,
	float MaxTargetDelta,
	float EffectorDiffSocket,
	const TArray<FBoneData>& BoneData
	)
{
	typedef typename AdapterType::KeyType KeyType;

	const int32 KeyCount = Keys.Num();
	check( Keys.Num() == Times.Num() );
	check( KeyCount >= 1 );
	
	// generate new arrays we will fill with the final keys
	TArray<KeyType> NewKeys;
	TArray<float> NewTimes;
	NewKeys.Empty(KeyCount);
	NewTimes.Empty(KeyCount);

	// Only bother doing anything if we have some keys!
	if(KeyCount > 0)
	{
		int32 LowKey = 0;
		int32 HighKey = KeyCount-1;

		TArray<uint32> KnownParentTimes;
		KnownParentTimes.Empty(KeyCount);
		const int32 ParentKeyCount = ParentTimes ? ParentTimes->Num() : 0;
		for (int32 TimeIndex = 0, ParentTimeIndex = 0; TimeIndex < KeyCount; TimeIndex++)
		{
			while ((ParentTimeIndex < ParentKeyCount) && (Times[TimeIndex] > (*ParentTimes)[ParentTimeIndex]))
			{
				ParentTimeIndex++;
			}

			KnownParentTimes.Add((ParentTimeIndex < ParentKeyCount) && (Times[TimeIndex] == (*ParentTimes)[ParentTimeIndex]));
		}

		TArray<FTransform> CachedInvRawBases;
		CachedInvRawBases.Empty(KeyCount);
		for (int32 FrameIndex = 0; FrameIndex < KeyCount; ++FrameIndex)
		{
			const FTransform& RawBase = RawWorldBones[(BoneIndex*NumFrames) + FrameIndex];
			CachedInvRawBases.Add(RawBase.Inverse());
		}
		
		// copy the low key (this one is a given)
		NewTimes.Add(Times[0]);
		NewKeys.Add(Keys[0]);

		const FTransform EndEffectorDummyBoneSocket(FQuat::Identity, FVector(END_EFFECTOR_DUMMY_BONE_LENGTH_SOCKET));
		const FTransform EndEffectorDummyBone(FQuat::Identity, FVector(END_EFFECTOR_DUMMY_BONE_LENGTH));

		const float DeltaThreshold = (BoneData[BoneIndex].IsEndEffector() && (BoneData[BoneIndex].bHasSocket || BoneData[BoneIndex].bKeyEndEffector)) ? EffectorDiffSocket : MaxTargetDelta;

		// We will test within a sliding window between LowKey and HighKey.
		// Therefore, we are done when the LowKey exceeds the range
		while (LowKey + 1 < KeyCount)
		{
			int32 GoodHighKey = LowKey + 1;
			int32 BadHighKey = KeyCount;
			
			// bisect until we find the lowest acceptable high key
			while (BadHighKey - GoodHighKey >= 2)
			{
				HighKey = GoodHighKey + (BadHighKey - GoodHighKey) / 2;

				// get the parameters of the window we are testing
				const float LowTime = Times[LowKey];
				const float HighTime = Times[HighKey];
				const KeyType& LowValue = Keys[LowKey];
				const KeyType& HighValue = Keys[HighKey];
				const float Range = HighTime - LowTime;
				const float InvRange = 1.0f/Range;

				// iterate through all interpolated members of the window to
				// compute the error when compared to the original raw values
				float MaxLerpError = 0.0f;
				float MaxTargetError = 0.0f;
				for (int32 TestKey = LowKey+1; TestKey< HighKey; ++TestKey)
				{
					// get the parameters of the member being tested
					float TestTime = Times[TestKey];
					const KeyType& TestValue = Keys[TestKey];

					// compute the proposed, interpolated value for the key
					const float Alpha = (TestTime - LowTime) * InvRange;
					const KeyType LerpValue = Interpolate(LowValue, HighValue, Alpha);

					// compute the error between our interpolated value and the desired value
					float LerpError = CalcDelta(TestValue, LerpValue);

					// if the local-space lerp error is within our tolerances, we will also check the
					// effect this interpolated key will have on our target end effectors
					float TargetError = -1.0f;
					if (LerpError <= MaxDelta)
					{
						// get the raw world transform for this bone (the original world-space position)
						const int32 FrameIndex = TestKey;
						const FTransform& InvRawBase = CachedInvRawBases[FrameIndex];
						
						// generate the proposed local bone atom and transform (local space)
						FTransform ProposedTM = AdapterType::UpdateBoneAtom(BoneAtoms[FrameIndex], LerpValue);

						// convert the proposed local transform to world space using this bone's parent transform
						const FTransform& CurrentParent = ParentBoneIndex != INDEX_NONE ? NewWorldBones[(ParentBoneIndex*NumFrames) + FrameIndex] : FTransform::Identity;
						FTransform ProposedBase = ProposedTM * CurrentParent;
						
						// for each target end effector, compute the error we would introduce with our proposed key
						for (int32 TargetIndex=0; TargetIndex<TargetBoneIndices.Num(); ++TargetIndex)
						{
							// find the offset transform from the raw base to the end effector
							const int32 TargetBoneIndex = TargetBoneIndices[TargetIndex];
							FTransform RawTarget = RawWorldBones[(TargetBoneIndex*NumFrames) + FrameIndex];
							const FTransform RelTM = RawTarget * InvRawBase;

							// forecast where the new end effector would be using our proposed key
							FTransform ProposedTarget = RelTM * ProposedBase;

							// If this is an EndEffector, add a dummy bone to measure the effect of compressing the rotation.
							// Sockets and Key EndEffectors have a longer dummy bone to maintain higher precision.
							if (BoneData[TargetIndex].bHasSocket || BoneData[TargetIndex].bKeyEndEffector)
							{
								ProposedTarget = EndEffectorDummyBoneSocket * ProposedTarget;
								RawTarget = EndEffectorDummyBoneSocket * RawTarget;
							}
							else
							{
								ProposedTarget = EndEffectorDummyBone * ProposedTarget;
								RawTarget = EndEffectorDummyBone * RawTarget;
							}

							// determine the extend of error at the target end effector
							const float ThisError = (ProposedTarget.GetTranslation() - RawTarget.GetTranslation()).Size();
							TargetError = FMath::Max(TargetError, ThisError); 

							// exit early when we encounter a large delta
							const float TargetDeltaThreshold = BoneData[TargetIndex].bHasSocket ? EffectorDiffSocket : DeltaThreshold;
							if( TargetError > TargetDeltaThreshold )
							{ 
								break;
							}
						}
					}

					// If the parent has a key at this time, we'll scale our error values as requested.
					// This increases the odds that we will choose keys on the same frames as our parent bone,
					// making the skeleton more uniform in key distribution.
					if (ParentTimes)
					{
						if (KnownParentTimes[TestKey])
						{
							// our parent has a key at this time, 
							// inflate our perceived error to increase our sensitivity
							// for also retaining a key at this time
							LerpError *= ParentScale;
							TargetError *= ParentScale;
						}
					}
					
					// keep track of the worst errors encountered for both 
					// the local-space 'lerp' error and the end effector drift we will cause
					MaxLerpError = FMath::Max(MaxLerpError, LerpError);
					MaxTargetError = FMath::Max(MaxTargetError, TargetError);

					// exit early if we have failed in this span
					if (MaxLerpError > MaxDelta ||
						MaxTargetError > DeltaThreshold)
					{
						break;
					}
				}

				// determine if the span succeeded. That is, the worst errors found are within tolerances
				if ((MaxLerpError <= MaxDelta) && (MaxTargetError <= DeltaThreshold))
				{
					GoodHighKey = HighKey;
				}
				else
				{
					BadHighKey = HighKey;
				}
			}

			NewTimes.Add(Times[GoodHighKey]);
			NewKeys.Add(Keys[GoodHighKey]);

			LowKey = GoodHighKey;
		}

		// return the new key set to the caller
		Times= NewTimes;
		Keys= NewKeys;
	}
}

template <typename AdapterType>
void FilterLinearKeysTemplate(
	TArray<typename AdapterType::KeyType>& Keys,
	TArray<float>& Times,
	TArray<FTransform>& BoneAtoms,
	const TArray<float>* ParentTimes,
	const FProcessAnimationTracksContext& Context,
	const TArray<int32>& TargetBoneIndices,
	int32 BoneIndex,
	int32 ParentBoneIndex,
	float ParentScale,
	float MaxDelta,
	float MaxTargetDelta,
	float EffectorDiffSocket)
{
	typedef typename AdapterType::KeyType KeyType;

	const int32 KeyCount = Keys.Num();
	check(Keys.Num() == Times.Num());
	check(KeyCount >= 1);

	// generate new arrays we will fill with the final keys
	TArray<KeyType> NewKeys;
	TArray<float> NewTimes;
	NewKeys.Empty(KeyCount);
	NewTimes.Empty(KeyCount);

	// Only bother doing anything if we have some keys!
	if (KeyCount > 0)
	{
		int32 LowKey = 0;
		int32 HighKey = KeyCount - 1;

		TArray<uint32> KnownParentTimes;
		KnownParentTimes.Empty(KeyCount);
		const int32 ParentKeyCount = ParentTimes ? ParentTimes->Num() : 0;
		for (int32 TimeIndex = 0, ParentTimeIndex = 0; TimeIndex < KeyCount; TimeIndex++)
		{
			while ((ParentTimeIndex < ParentKeyCount) && (Times[TimeIndex] > (*ParentTimes)[ParentTimeIndex]))
			{
				ParentTimeIndex++;
			}

			KnownParentTimes.Add((ParentTimeIndex < ParentKeyCount) && (Times[TimeIndex] == (*ParentTimes)[ParentTimeIndex]));
		}

		TArray<FTransform> CachedInvRawBases;
		CachedInvRawBases.Empty(KeyCount);
		for (int32 FrameIndex = 0; FrameIndex < KeyCount; ++FrameIndex)
		{
			const FTransform& RawBase = Context.RawWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex];
			CachedInvRawBases.Add(RawBase.Inverse());
		}

		// copy the low key (this one is a given)
		NewTimes.Add(Times[0]);
		NewKeys.Add(Keys[0]);

		const FTransform EndEffectorDummyBoneSocket(FQuat::Identity, FVector(END_EFFECTOR_DUMMY_BONE_LENGTH_SOCKET));
		const FTransform EndEffectorDummyBone(FQuat::Identity, FVector(END_EFFECTOR_DUMMY_BONE_LENGTH));

		const float DeltaThreshold = (Context.BoneData[BoneIndex].IsEndEffector() && (Context.BoneData[BoneIndex].bHasSocket || Context.BoneData[BoneIndex].bKeyEndEffector)) ? EffectorDiffSocket : MaxTargetDelta;

		// We will test within a sliding window between LowKey and HighKey.
		// Therefore, we are done when the LowKey exceeds the range
		while (LowKey + 1 < KeyCount)
		{
			int32 GoodHighKey = LowKey + 1;
			int32 BadHighKey = KeyCount;

			// bisect until we find the lowest acceptable high key
			while (BadHighKey - GoodHighKey >= 2)
			{
				HighKey = GoodHighKey + (BadHighKey - GoodHighKey) / 2;

				// get the parameters of the window we are testing
				const float LowTime = Times[LowKey];
				const float HighTime = Times[HighKey];
				const KeyType& LowValue = Keys[LowKey];
				const KeyType& HighValue = Keys[HighKey];
				const float Range = HighTime - LowTime;
				const float InvRange = 1.0f / Range;

				// iterate through all interpolated members of the window to
				// compute the error when compared to the original raw values
				float MaxLerpError = 0.0f;
				float MaxTargetError = 0.0f;
				for (int32 TestKey = LowKey + 1; TestKey < HighKey; ++TestKey)
				{
					// get the parameters of the member being tested
					float TestTime = Times[TestKey];
					const KeyType& TestValue = Keys[TestKey];

					// compute the proposed, interpolated value for the key
					const float Alpha = (TestTime - LowTime) * InvRange;
					const KeyType LerpValue = Interpolate(LowValue, HighValue, Alpha);

					// compute the error between our interpolated value and the desired value
					float LerpError = CalcDelta(TestValue, LerpValue);

					// if the local-space lerp error is within our tolerances, we will also check the
					// effect this interpolated key will have on our target end effectors
					float TargetError = -1.0f;
					if (LerpError <= MaxDelta)
					{
						// get the raw world transform for this bone (the original world-space position)
						const int32 FrameIndex = TestKey;
						const FTransform& InvRawBase = CachedInvRawBases[FrameIndex];

						// generate the proposed local bone atom and transform (local space)
						FTransform ProposedTM = AdapterType::UpdateBoneAtom(BoneAtoms[FrameIndex], LerpValue);

						// convert the proposed local transform to world space using this bone's parent transform
						const FTransform& CurrentParent = ParentBoneIndex != INDEX_NONE ? Context.NewWorldBones[(ParentBoneIndex * Context.Segment.NumFrames) + FrameIndex] : FTransform::Identity;
						FTransform ProposedBase = ProposedTM * CurrentParent;

						// for each target end effector, compute the error we would introduce with our proposed key
						for (int32 TargetIndex = 0; TargetIndex < TargetBoneIndices.Num(); ++TargetIndex)
						{
							// find the offset transform from the raw base to the end effector
							const int32 TargetBoneIndex = TargetBoneIndices[TargetIndex];
							FTransform RawTarget = Context.RawWorldBones[(TargetBoneIndex * Context.Segment.NumFrames) + FrameIndex];
							const FTransform RelTM = RawTarget * InvRawBase;

							// forecast where the new end effector would be using our proposed key
							FTransform ProposedTarget = RelTM * ProposedBase;

							// If this is an EndEffector, add a dummy bone to measure the effect of compressing the rotation.
							// Sockets and Key EndEffectors have a longer dummy bone to maintain higher precision.
							if (Context.BoneData[TargetIndex].bHasSocket || Context.BoneData[TargetIndex].bKeyEndEffector)
							{
								ProposedTarget = EndEffectorDummyBoneSocket * ProposedTarget;
								RawTarget = EndEffectorDummyBoneSocket * RawTarget;
							}
							else
							{
								ProposedTarget = EndEffectorDummyBone * ProposedTarget;
								RawTarget = EndEffectorDummyBone * RawTarget;
							}

							// determine the extend of error at the target end effector
							const float ThisError = (ProposedTarget.GetTranslation() - RawTarget.GetTranslation()).Size();
							TargetError = FMath::Max(TargetError, ThisError);

							// exit early when we encounter a large delta
							const float TargetDeltaThreshold = Context.BoneData[TargetIndex].bHasSocket ? EffectorDiffSocket : DeltaThreshold;
							if (TargetError > TargetDeltaThreshold)
							{
								break;
							}
						}
					}

					// If the parent has a key at this time, we'll scale our error values as requested.
					// This increases the odds that we will choose keys on the same frames as our parent bone,
					// making the skeleton more uniform in key distribution.
					if (ParentTimes)
					{
						if (KnownParentTimes[TestKey])
						{
							// our parent has a key at this time, 
							// inflate our perceived error to increase our sensitivity
							// for also retaining a key at this time
							LerpError *= ParentScale;
							TargetError *= ParentScale;
						}
					}

					// keep track of the worst errors encountered for both 
					// the local-space 'lerp' error and the end effector drift we will cause
					MaxLerpError = FMath::Max(MaxLerpError, LerpError);
					MaxTargetError = FMath::Max(MaxTargetError, TargetError);

					// exit early if we have failed in this span
					if (MaxLerpError > MaxDelta ||
						MaxTargetError > DeltaThreshold)
					{
						break;
					}
				}

				// determine if the span succeeded. That is, the worst errors found are within tolerances
				if ((MaxLerpError <= MaxDelta) && (MaxTargetError <= DeltaThreshold))
				{
					GoodHighKey = HighKey;
				}
				else
				{
					BadHighKey = HighKey;
				}
			}

			//UE_LOG(LogAnimation, Warning, TEXT("%u: Keeping key %u"), BoneIndex, GoodHighKey);

			NewTimes.Add(Times[GoodHighKey]);
			NewKeys.Add(Keys[GoodHighKey]);

			LowKey = GoodHighKey;
		}

		// return the new key set to the caller
		Times = NewTimes;
		Keys = NewKeys;
	}
}

void UAnimCompress_RemoveLinearKeys::UpdateWorldBoneTransformTable(
	UAnimSequence* AnimSeq, 
	const TArray<FBoneData>& BoneData, 
	const TArray<FTransform>& RefPose,
	int32 BoneIndex, // this bone index should be of skeleton, not mesh
	bool UseRaw,
	TArray<FTransform>& OutputWorldBones)
{
	const FBoneData& Bone		= BoneData[BoneIndex];
	const int32 NumFrames			= AnimSeq->NumFrames;
	const float SequenceLength	= AnimSeq->SequenceLength;
	const int32 FrameStart		= (BoneIndex*NumFrames);
	const int32 TrackIndex = AnimSeq->GetSkeleton()->GetAnimationTrackIndex(BoneIndex, AnimSeq, UseRaw);
	
	check(OutputWorldBones.Num() >= (FrameStart+NumFrames));

	const float TimePerFrame = SequenceLength / (float)(NumFrames-1);

	if( TrackIndex != INDEX_NONE )
	{
		// get the local-space bone transforms using the animation solver
		for ( int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
		{
			float Time = (float)FrameIndex * TimePerFrame;
			FTransform LocalAtom;

			AnimSeq->GetBoneTransform(LocalAtom, TrackIndex, Time, UseRaw);

			FQuat Rot = LocalAtom.GetRotation();
			LocalAtom.SetRotation(EnforceShortestArc(FQuat::Identity, Rot));
			// Saw some crashes happening with it, so normalize here. 
			LocalAtom.NormalizeRotation();

			OutputWorldBones[(BoneIndex*NumFrames) + FrameIndex] = LocalAtom;
		}
	}
	else
	{
		// get the default rotation and translation from the reference skeleton
		FTransform DefaultTransform;
		FTransform LocalAtom = RefPose[BoneIndex];
		LocalAtom.SetRotation(EnforceShortestArc(FQuat::Identity, LocalAtom.GetRotation()));
		DefaultTransform = LocalAtom;

		// copy the default transformation into the world bone table
		for ( int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
		{
			OutputWorldBones[(BoneIndex*NumFrames) + FrameIndex] = DefaultTransform;
		}
	}

	// apply parent transforms to bake into world space. We assume the parent transforms were previously set using this function
	const int32 ParentIndex = Bone.GetParent();
	if (ParentIndex != INDEX_NONE)
	{
		check (ParentIndex < BoneIndex);
		for ( int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
		{
			OutputWorldBones[(BoneIndex*NumFrames) + FrameIndex] = OutputWorldBones[(BoneIndex*NumFrames) + FrameIndex] * OutputWorldBones[(ParentIndex*NumFrames) + FrameIndex];
		}
	}
}

static void PackVector(
	const FVector& Key,
	AnimationCompressionFormat Format,
	const float* Mins,
	const float* Ranges,
	uint8* Buffer)
{
	if (Format == ACF_None || Format == ACF_Float96NoW)
	{
		memcpy(Buffer, &Key, sizeof(FVector));
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		const FVectorIntervalFixed32NoW CompressedKey(Key, Mins, Ranges);
		memcpy(Buffer, &CompressedKey, sizeof(FVectorIntervalFixed32NoW));
	}
}

static FVector UnpackVector(
	const uint8* Buffer,
	AnimationCompressionFormat Format,
	const float* Mins,
	const float* Ranges)
{
	FVector Result;

	if (Format == ACF_None || Format == ACF_Float96NoW)
	{
		memcpy(&Result, Buffer, sizeof(FVector));
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		FVectorIntervalFixed32NoW CompressedKey;
		memcpy(&CompressedKey, Buffer, sizeof(FVectorIntervalFixed32NoW));
		CompressedKey.ToVector(Result, Mins, Ranges);
	}

	return Result;
}

static void PackQuat(
	const FQuat& Key,
	AnimationCompressionFormat Format,
	const float* Mins,
	const float* Ranges,
	uint8* Buffer)
{
	if (Format == ACF_None)
	{
		memcpy(Buffer, &Key, sizeof(FQuat));
	}
	else if (Format == ACF_Float96NoW)
	{
		const FQuatFloat96NoW QuatFloat96NoW(Key);
		memcpy(Buffer, &QuatFloat96NoW, sizeof(FQuatFloat96NoW));
	}
	else if (Format == ACF_Fixed32NoW)
	{
		const FQuatFixed32NoW QuatFixed32NoW(Key);
		memcpy(Buffer, &QuatFixed32NoW, sizeof(FQuatFixed32NoW));
	}
	else if (Format == ACF_Fixed48NoW)
	{
		const FQuatFixed48NoW QuatFixed48NoW(Key);
		memcpy(Buffer, &QuatFixed48NoW, sizeof(FQuatFixed48NoW));
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		const FQuatIntervalFixed32NoW QuatIntervalFixed32NoW(Key, Mins, Ranges);
		memcpy(Buffer, &QuatIntervalFixed32NoW, sizeof(FQuatIntervalFixed32NoW));
	}
	else if (Format == ACF_Float32NoW)
	{
		const FQuatFloat32NoW QuatFloat32NoW(Key);
		memcpy(Buffer, &QuatFloat32NoW, sizeof(FQuatFloat32NoW));
	}
}

static FQuat UnpackQuat(
	const uint8* Buffer,
	AnimationCompressionFormat Format,
	const float* Mins,
	const float* Ranges)
{
	FQuat Result;

	if (Format == ACF_None)
	{
		memcpy(&Result, Buffer, sizeof(FQuat));
	}
	else if (Format == ACF_Float96NoW)
	{
		FQuatFloat96NoW QuatFloat96NoW;
		memcpy(&QuatFloat96NoW, Buffer, sizeof(FQuatFloat96NoW));
		QuatFloat96NoW.ToQuat(Result);
	}
	else if (Format == ACF_Fixed32NoW)
	{
		FQuatFixed32NoW QuatFixed32NoW;
		memcpy(&QuatFixed32NoW, Buffer, sizeof(FQuatFixed32NoW));
		QuatFixed32NoW.ToQuat(Result);
	}
	else if (Format == ACF_Fixed48NoW)
	{
		FQuatFixed48NoW QuatFixed48NoW;
		memcpy(&QuatFixed48NoW, Buffer, sizeof(FQuatFixed48NoW));
		QuatFixed48NoW.ToQuat(Result);
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		FQuatIntervalFixed32NoW QuatIntervalFixed32NoW;
		memcpy(&QuatIntervalFixed32NoW, Buffer, sizeof(FQuatIntervalFixed32NoW));
		QuatIntervalFixed32NoW.ToQuat(Result, Mins, Ranges);
	}
	else if (Format == ACF_Float32NoW)
	{
		FQuatFloat32NoW QuatFloat32NoW;
		memcpy(&QuatFloat32NoW, Buffer, sizeof(FQuatFloat32NoW));
		QuatFloat32NoW.ToQuat(Result);
	}

	return Result;
}

static float GetFrameRate(const UAnimSequence& AnimSeq)
{
	// We cannot round since some sequences have a non integral frame rate for legacy reasons
	return float(AnimSeq.NumFrames - 1) / AnimSeq.SequenceLength;
}

static float FindKeyInterpolationData(const UAnimSequence& AnimSeq, const TArray<float>& KeyTimes, float Time, int32& OutKeyIndex0, int32& OutKeyIndex1)
{
	// Use the same logic as the decompression in order to ensure we match 100%
	const int32 LastFrame = AnimSeq.NumFrames - 1;
	const float FrameRate = GetFrameRate(AnimSeq);
	const float RelativePos = Time / AnimSeq.SequenceLength;
	const float FramePos = RelativePos * float(LastFrame);
	const int32 FramePosFloor = FMath::Clamp(FMath::TruncToInt(FramePos), 0, LastFrame);

	const int32 NumKeys = KeyTimes.Num();
	int32 KeyIndex0 = NumKeys - 1;	// Default
	for (int32 KeyIndex = 1; KeyIndex < NumKeys; ++KeyIndex)
	{
		const float KeyTime = KeyTimes[KeyIndex];
		const float FrameTime = KeyTime * FrameRate;
		const int32 FrameIndex = FMath::Clamp(FMath::TruncToInt(FrameTime + 0.5f), 0, LastFrame);
		if (FrameIndex > FramePosFloor)
		{
			KeyIndex0 = KeyIndex - 1;
			break;
		}
	}

	const int32 KeyIndex1 = FMath::Min(KeyIndex0 + 1, NumKeys - 1);
	const float KeyTime0 = KeyTimes[KeyIndex0];
	const float KeyTime1 = KeyTimes[KeyIndex1];

	const float FrameTime0 = KeyTime0 * FrameRate;
	const float FrameTime1 = KeyTime1 * FrameRate;
	const int32 FrameIndex0 = FMath::Clamp(FMath::TruncToInt(FrameTime0 + 0.5f), 0, LastFrame);
	const int32 FrameIndex1 = FMath::Clamp(FMath::TruncToInt(FrameTime1 + 0.5f), 0, LastFrame);

	const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
	const float Remainder = FramePos - (float)FrameIndex0;
	const float InterpolationAlpha = AnimSeq.Interpolation == EAnimInterpolationType::Step ? 0.0f : FMath::Min(Remainder / (float)Delta, 1.0f);

	OutKeyIndex0 = KeyIndex0;
	OutKeyIndex1 = KeyIndex1;
	return InterpolationAlpha;
}

FTransform UAnimCompress_RemoveLinearKeys::SampleSegment(const FProcessAnimationTracksContext& Context, int32 TrackIndex, float Time)
{
	FTransform Result;

	{
		const FTranslationTrack& TranslationData = Context.Segment.TranslationData[TrackIndex];
		const int32 NumTransKeys = TranslationData.Times.Num();
		if (NumTransKeys > 1)
		{
			int32 KeyIndex0;
			int32 KeyIndex1;
			const float InterpolationAlpha = FindKeyInterpolationData(Context.AnimSeq, TranslationData.Times, Time, KeyIndex0, KeyIndex1);

			FVector RangeMin;
			FVector RangeExtent;
			CalculateTrackRange(TranslationData, Context.Segment.TranslationCompressionFormat, RangeMin, RangeExtent);

			const FVector& Translation0 = TranslationData.PosKeys[KeyIndex0];
			uint8 Buffer0[32];
			PackVector(Translation0, Context.Segment.TranslationCompressionFormat, &RangeMin.X, &RangeExtent.X, Buffer0);
			FVector LossyTranslation = UnpackVector(Buffer0, Context.Segment.TranslationCompressionFormat, &RangeMin.X, &RangeExtent.X);

			if (KeyIndex0 != KeyIndex1)
			{
				const FVector& Translation1 = TranslationData.PosKeys[KeyIndex1];
				uint8 Buffer1[32];
				PackVector(Translation1, Context.Segment.TranslationCompressionFormat, &RangeMin.X, &RangeExtent.X, Buffer1);
				const FVector LossyTranslation1 = UnpackVector(Buffer1, Context.Segment.TranslationCompressionFormat, &RangeMin.X, &RangeExtent.X);

				LossyTranslation = FMath::Lerp(LossyTranslation, LossyTranslation1, InterpolationAlpha);
			}

			Result.SetTranslation(LossyTranslation);
		}
		else
		{
			Result.SetTranslation(Context.TrivialTrackValues[TrackIndex].GetTranslation());
		}
	}

	{
		const FRotationTrack& RotationData = Context.Segment.RotationData[TrackIndex];
		const int32 NumRotKeys = RotationData.Times.Num();
		if (NumRotKeys > 1)
		{
			int32 KeyIndex0;
			int32 KeyIndex1;
			const float InterpolationAlpha = FindKeyInterpolationData(Context.AnimSeq, RotationData.Times, Time, KeyIndex0, KeyIndex1);

			FVector RangeMin;
			FVector RangeExtent;
			CalculateTrackRange(RotationData, Context.Segment.RotationCompressionFormat, RangeMin, RangeExtent);

			const FQuat& Rotation0 = RotationData.RotKeys[KeyIndex0];
			uint8 Buffer0[32];
			PackQuat(Rotation0, Context.Segment.RotationCompressionFormat, &RangeMin.X, &RangeExtent.X, Buffer0);
			FQuat LossyRotation = UnpackQuat(Buffer0, Context.Segment.RotationCompressionFormat, &RangeMin.X, &RangeExtent.X);

			if (KeyIndex0 != KeyIndex1)
			{
				const FQuat& Rotation1 = RotationData.RotKeys[KeyIndex1];
				uint8 Buffer1[32];
				PackQuat(Rotation1, Context.Segment.RotationCompressionFormat, &RangeMin.X, &RangeExtent.X, Buffer1);
				const FQuat LossyRotation1 = UnpackQuat(Buffer1, Context.Segment.RotationCompressionFormat, &RangeMin.X, &RangeExtent.X);

				LossyRotation = FQuat::FastLerp(LossyRotation, LossyRotation1, InterpolationAlpha);
				LossyRotation.Normalize();
			}

			Result.SetRotation(LossyRotation);
		}
		else
		{
			const FQuat Rotation = Context.TrivialTrackValues[TrackIndex].GetRotation();
			uint8 Buffer[32];
			PackQuat(Rotation, ACF_Float96NoW, nullptr, nullptr, Buffer);
			const FQuat LossyRotation = UnpackQuat(Buffer, ACF_Float96NoW, nullptr, nullptr);

			Result.SetRotation(LossyRotation);
		}
	}

	if (Context.Segment.ScaleData.Num() != 0)
	{
		const FScaleTrack& ScaleData = Context.Segment.ScaleData[TrackIndex];
		const int32 NumScaleKeys = ScaleData.Times.Num();
		if (NumScaleKeys > 1)
		{
			int32 KeyIndex0;
			int32 KeyIndex1;
			const float InterpolationAlpha = FindKeyInterpolationData(Context.AnimSeq, ScaleData.Times, Time, KeyIndex0, KeyIndex1);

			FVector RangeMin;
			FVector RangeExtent;
			CalculateTrackRange(ScaleData, Context.Segment.ScaleCompressionFormat, RangeMin, RangeExtent);

			const FVector& Scale0 = ScaleData.ScaleKeys[KeyIndex0];
			uint8 Buffer0[32];
			PackVector(Scale0, Context.Segment.ScaleCompressionFormat, &RangeMin.X, &RangeExtent.X, Buffer0);
			FVector LossyScale = UnpackVector(Buffer0, Context.Segment.ScaleCompressionFormat, &RangeMin.X, &RangeExtent.X);

			if (KeyIndex0 != KeyIndex1)
			{
				const FVector& Scale1 = ScaleData.ScaleKeys[KeyIndex1];
				uint8 Buffer1[32];
				PackVector(Scale1, Context.Segment.ScaleCompressionFormat, &RangeMin.X, &RangeExtent.X, Buffer1);
				const FVector LossyScale1 = UnpackVector(Buffer1, Context.Segment.ScaleCompressionFormat, &RangeMin.X, &RangeExtent.X);

				LossyScale = FMath::Lerp(LossyScale, LossyScale1, InterpolationAlpha);
			}

			Result.SetScale3D(LossyScale);
		}
		else
		{
			Result.SetScale3D(Context.TrivialTrackValues[TrackIndex].GetScale3D());
		}
	}

	return Result;
}

void UAnimCompress_RemoveLinearKeys::UpdateWorldBoneTransformTable(
	const FProcessAnimationTracksContext& Context,
	int32 BoneIndex,
	bool UseRaw,
	TArray<FTransform>& OutputWorldBones)
{
	const int32 TrackIndex = Context.BoneIndexToTrackIndex[BoneIndex];

	check(OutputWorldBones.Num() >= ((BoneIndex * Context.Segment.NumFrames) + Context.Segment.NumFrames));

	if (TrackIndex != INDEX_NONE)
	{
		const float FrameRate = GetFrameRate(Context.AnimSeq);
		FAnimSequenceDecompressionContext DecompContext(&Context.AnimSeq);

		// get the local-space bone transforms using the animation solver
		for (int32 FrameIndex = 0; FrameIndex < Context.Segment.NumFrames; ++FrameIndex)
		{
			const float Time = (float)(Context.Segment.StartFrame + FrameIndex) / FrameRate;

			FTransform LocalAtom;
			if (UseRaw)
			{
				DecompContext.Seek(Time);
				Context.AnimSeq.GetBoneTransform(LocalAtom, TrackIndex, DecompContext, UseRaw);
			}
			else
			{
				if (bUseDecompression)
				{
					DecompContext.Seek(Time);
					Context.AnimSeq.GetBoneTransform(LocalAtom, TrackIndex, DecompContext, UseRaw);
				}
				else
				{
					LocalAtom = SampleSegment(Context, TrackIndex, Time);
				}
			}

			FQuat Rot = LocalAtom.GetRotation();
			LocalAtom.SetRotation(EnforceShortestArc(FQuat::Identity, Rot));
			// Saw some crashes happening with it, so normalize here.
			LocalAtom.NormalizeRotation();

			OutputWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex] = LocalAtom;
		}
	}
	else
	{
		// get the default rotation and translation from the reference skeleton
		FTransform DefaultTransform = Context.RefPose[BoneIndex];
		DefaultTransform.SetRotation(EnforceShortestArc(FQuat::Identity, DefaultTransform.GetRotation()));

		// copy the default transformation into the world bone table
		for (int32 FrameIndex = 0; FrameIndex < Context.Segment.NumFrames; ++FrameIndex)
		{
			OutputWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex] = DefaultTransform;
		}
	}

	// apply parent transforms to bake into world space. We assume the parent transforms were previously set using this function
	const FBoneData& Bone = Context.BoneData[BoneIndex];
	const int32 ParentIndex = Bone.GetParent();
	if (ParentIndex != INDEX_NONE)
	{
		check(ParentIndex < BoneIndex);
		for (int32 FrameIndex = 0; FrameIndex < Context.Segment.NumFrames; ++FrameIndex)
		{
			OutputWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex] = OutputWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex] * OutputWorldBones[(ParentIndex * Context.Segment.NumFrames) + FrameIndex];
		}
	}
}

void UAnimCompress_RemoveLinearKeys::FilterBeforeMainKeyRemoval(
	UAnimSequence* AnimSeq, 
	const TArray<FBoneData>& BoneData, 
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData, 
	TArray<FScaleTrack>& ScaleData)
{
	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, ScaleData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD, SCALE_ZEROING_THRESHOLD);
}



void UAnimCompress_RemoveLinearKeys::UpdateWorldBoneTransformRange(
	UAnimSequence* AnimSeq, 
	const TArray<FBoneData>& BoneData, 
	const TArray<FTransform>& RefPose,
	const TArray<FTranslationTrack>& PositionTracks,
	const TArray<FRotationTrack>& RotationTracks,
	const TArray<FScaleTrack>& ScaleTracks,
	int32 StartingBoneIndex, // this bone index should be of skeleton, not mesh
	int32 EndingBoneIndex,// this bone index should be of skeleton, not mesh
	bool UseRaw,
	TArray<FTransform>& OutputWorldBones)
{
	// bitwise compress the tracks into the anim sequence buffers
	// to make sure the data we've compressed so far is ready for solving
	CompressUsingUnderlyingCompressor(
		AnimSeq,
		BoneData, 
		PositionTracks,
		RotationTracks,
		ScaleTracks,
		false);

	// build all world-space transforms from this bone to the target end effector we are monitoring
	// all parent transforms have been built already
	for ( int32 Index = StartingBoneIndex; Index <= EndingBoneIndex; ++Index )
	{
		UpdateWorldBoneTransformTable(
			AnimSeq, 
			BoneData, 
			RefPose,
			Index,
			UseRaw,
			OutputWorldBones);
	}
}


void UAnimCompress_RemoveLinearKeys::UpdateWorldBoneTransformRange(
	FProcessAnimationTracksContext& Context,
	int32 StartingBoneIndex,	// this bone index should be of skeleton, not mesh
	int32 EndingBoneIndex)		// this bone index should be of skeleton, not mesh
{
	if (bUseDecompression)
	{
		// bitwise compress the tracks into the anim sequence buffers
		// to make sure the data we've compressed so far is ready for solving
		CompressUsingUnderlyingCompressor(Context.AnimSeq, Context.BoneData, Context.SegmentList, false);
	}

	// build all world-space transforms from this bone to the target end effector we are monitoring
	// all parent transforms have been built already
	for (int32 Index = StartingBoneIndex; Index <= EndingBoneIndex; ++Index)
	{
		UpdateWorldBoneTransformTable(Context, Index, false, Context.NewWorldBones);
	}
}


void UAnimCompress_RemoveLinearKeys::UpdateBoneAtomList(
	UAnimSequence* AnimSeq, 
	int32 BoneIndex,
	int32 TrackIndex,
	int32 NumFrames,
	float TimePerFrame,
	TArray<FTransform>& BoneAtoms)
{
	BoneAtoms.Reset(NumFrames);
	for ( int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
	{
		float Time = (float)FrameIndex * TimePerFrame;
		FTransform LocalAtom;
		AnimSeq->GetBoneTransform(LocalAtom, TrackIndex, Time, false);

		FQuat Rot = LocalAtom.GetRotation();
		LocalAtom.SetRotation( EnforceShortestArc(FQuat::Identity, Rot) );
		BoneAtoms.Add(LocalAtom);
	}
}


void UAnimCompress_RemoveLinearKeys::UpdateBoneAtomList(
	const FProcessAnimationTracksContext& Context,
	int32 TrackIndex,
	TArray<FTransform>& BoneAtoms) const
{
	const float FrameRate = GetFrameRate(Context.AnimSeq);
	FAnimSequenceDecompressionContext DecompContext(&Context.AnimSeq);

	BoneAtoms.Reset(Context.Segment.NumFrames);
	for (int32 FrameIndex = 0; FrameIndex < Context.Segment.NumFrames; ++FrameIndex)
	{
		const float Time = (float)(FrameIndex + Context.Segment.StartFrame) / FrameRate;

		FTransform LocalAtom;
		if (bUseDecompression)
		{
			DecompContext.Seek(Time);
			Context.AnimSeq.GetBoneTransform(LocalAtom, TrackIndex, DecompContext, false);
		}
		else
		{
			LocalAtom = SampleSegment(Context, TrackIndex, Time);
		}

		FQuat Rot = LocalAtom.GetRotation();
		LocalAtom.SetRotation(EnforceShortestArc(FQuat::Identity, Rot));
		BoneAtoms.Add(LocalAtom);
	}
}


bool UAnimCompress_RemoveLinearKeys::ConvertFromRelativeSpace(UAnimSequence* AnimSeq)
{
	// if this is an additive animation, temporarily convert it out of relative-space
	const bool bAdditiveAnimation = AnimSeq->IsValidAdditive();
	if (bAdditiveAnimation)
	{
		// convert the raw tracks out of additive-space
		const int32 NumTracks = AnimSeq->GetRawAnimationData().Num();
		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			// bone index of skeleton
			int32 const BoneIndex = AnimSeq->GetRawTrackToSkeletonMapTable()[TrackIndex].BoneTreeIndex;
			bool const bIsRootBone = (BoneIndex == 0);

			const FRawAnimSequenceTrack& BasePoseTrack = AnimSeq->GetAdditiveBaseAnimationData()[TrackIndex];
			FRawAnimSequenceTrack& RawTrack	= AnimSeq->GetRawAnimationTrack(TrackIndex);

			// @note: we only extract the first frame, as we don't want to induce motion from the base pose
			// only the motion from the additive data should matter.
			const FVector& RefBonePos = BasePoseTrack.PosKeys[0];
			const FQuat& RefBoneRotation = BasePoseTrack.RotKeys[0];

			// Transform position keys.
			for (int32 PosIndex = 0; PosIndex < RawTrack.PosKeys.Num(); ++PosIndex)
			{
				RawTrack.PosKeys[PosIndex] += RefBonePos;
			}

			// Transform rotation keys.
			for (int32 RotIndex = 0; RotIndex < RawTrack.RotKeys.Num(); ++RotIndex)
			{
				RawTrack.RotKeys[RotIndex] = RawTrack.RotKeys[RotIndex] * RefBoneRotation;
				RawTrack.RotKeys[RotIndex].Normalize();
			}

			// make sure scale key exists
			if (RawTrack.ScaleKeys.Num() > 0)
			{
				const FVector DefaultScale(1.f);
				const FVector& RefBoneScale = (BasePoseTrack.ScaleKeys.Num() > 0)? BasePoseTrack.ScaleKeys[0] : DefaultScale;
				for (int32 ScaleIndex = 0; ScaleIndex < RawTrack.ScaleKeys.Num(); ++ScaleIndex)
				{
					RawTrack.ScaleKeys[ScaleIndex] = RefBoneScale * (DefaultScale + RawTrack.ScaleKeys[ScaleIndex]);
				}
			}
		}
	}

	return bAdditiveAnimation;
}

void UAnimCompress_RemoveLinearKeys::ConvertToRelativeSpace(
	UAnimSequence* AnimSeq,
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData, 
	TArray<FScaleTrack>& ScaleData)
{
	ConvertToRelativeSpace(*AnimSeq);
	ConvertToRelativeSpace(*AnimSeq, TranslationData, RotationData, ScaleData);
}

void UAnimCompress_RemoveLinearKeys::ConvertToRelativeSpace(UAnimSequence& AnimSeq) const
{
	// convert the raw tracks back to additive-space
	const int32 NumTracks = AnimSeq.GetRawAnimationData().Num();
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		const FRawAnimSequenceTrack& BasePoseTrack = AnimSeq.GetAdditiveBaseAnimationData()[TrackIndex];
		FRawAnimSequenceTrack& RawTrack = AnimSeq.GetRawAnimationTrack(TrackIndex);

		// @note: we only extract the first frame, as we don't want to induce motion from the base pose
		// only the motion from the additive data should matter.
		const FQuat InvRefBoneRotation = BasePoseTrack.RotKeys[0].Inverse();
		const FVector InvRefBoneTranslation = -BasePoseTrack.PosKeys[0];

		// transform position keys.
		for (int32 PosIndex = 0; PosIndex < RawTrack.PosKeys.Num(); ++PosIndex)
		{
			RawTrack.PosKeys[PosIndex] += InvRefBoneTranslation;
		}

		// transform rotation keys.
		for (int32 RotIndex = 0; RotIndex < RawTrack.RotKeys.Num(); ++RotIndex)
		{
			RawTrack.RotKeys[RotIndex] = RawTrack.RotKeys[RotIndex] * InvRefBoneRotation;
			RawTrack.RotKeys[RotIndex].Normalize();
		}

		// scale key
		if (RawTrack.ScaleKeys.Num() > 0)
		{
			const FVector InvRefBoneScale = FTransform::GetSafeScaleReciprocal(BasePoseTrack.ScaleKeys[0]);

			// transform scale keys.
			for (int32 ScaleIndex = 0; ScaleIndex < RawTrack.ScaleKeys.Num(); ++ScaleIndex)
			{
				// to revert scale correctly, you have to - 1.f
				// check AccumulateWithAdditiveScale
				RawTrack.ScaleKeys[ScaleIndex] = (RawTrack.ScaleKeys[ScaleIndex] * InvRefBoneScale) - 1.f;
			}
		}
	}
}

void UAnimCompress_RemoveLinearKeys::ConvertToRelativeSpace(
	const UAnimSequence& AnimSeq,
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData,
	TArray<FScaleTrack>& ScaleData) const
{
	// convert the raw tracks back to additive-space
	const int32 NumTracks = AnimSeq.GetRawAnimationData().Num();
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		const FRawAnimSequenceTrack& BasePoseTrack = AnimSeq.GetAdditiveBaseAnimationData()[TrackIndex];

		// @note: we only extract the first frame, as we don't want to induce motion from the base pose
		// only the motion from the additive data should matter.
		const FQuat InvRefBoneRotation = BasePoseTrack.RotKeys[0].Inverse();
		const FVector InvRefBoneTranslation = -BasePoseTrack.PosKeys[0];

		// convert the new translation tracks to additive space
		FTranslationTrack& TranslationTrack = TranslationData[TrackIndex];
		for (int32 KeyIndex = 0; KeyIndex < TranslationTrack.PosKeys.Num(); ++KeyIndex)
		{
			TranslationTrack.PosKeys[KeyIndex] += InvRefBoneTranslation;
		}

		// convert the new rotation tracks to additive space
		FRotationTrack& RotationTrack = RotationData[TrackIndex];
		for (int32 KeyIndex = 0; KeyIndex < RotationTrack.RotKeys.Num(); ++KeyIndex)
		{
			RotationTrack.RotKeys[KeyIndex] = RotationTrack.RotKeys[KeyIndex] * InvRefBoneRotation;
			RotationTrack.RotKeys[KeyIndex].Normalize();
		}

		// scale key
		if (ScaleData.Num() > 0)
		{
			const FVector InvRefBoneScale = FTransform::GetSafeScaleReciprocal(BasePoseTrack.ScaleKeys[0]);

			// convert the new scale tracks to additive space
			FScaleTrack& ScaleTrack = ScaleData[TrackIndex];
			for (int32 KeyIndex = 0; KeyIndex < ScaleTrack.ScaleKeys.Num(); ++KeyIndex)
			{
				ScaleTrack.ScaleKeys[KeyIndex] = (ScaleTrack.ScaleKeys[KeyIndex] * InvRefBoneScale) - 1.f;
			}
		}
	}
}

void UAnimCompress_RemoveLinearKeys::ProcessAnimationTracks(
	UAnimSequence* AnimSeq, 
	const TArray<FBoneData>& BoneData, 
	TArray<FTranslationTrack>& PositionTracks,
	TArray<FRotationTrack>& RotationTracks, 
	TArray<FScaleTrack>& ScaleTracks)
{
	// extract all the data we'll need about the skeleton and animation sequence
	const int32 NumBones			= BoneData.Num();
	const int32 NumFrames			= AnimSeq->NumFrames;
	const float SequenceLength	= AnimSeq->SequenceLength;
	const int32 LastFrame = NumFrames-1;
	const float FrameRate = (float)(LastFrame) / SequenceLength;
	const float TimePerFrame = SequenceLength / (float)(LastFrame);

	const TArray<FTransform>& RefPose = AnimSeq->GetSkeleton()->GetRefLocalPoses();
	const bool bHasScale =  (ScaleTracks.Num() > 0);

	// make sure the parent key scale is properly bound to 1.0 or more
	ParentKeyScale = FMath::Max(ParentKeyScale, 1.0f);

	// generate the raw and compressed skeleton in world-space
	TArray<FTransform> RawWorldBones;
	TArray<FTransform> NewWorldBones;
	RawWorldBones.Empty(NumBones * NumFrames);
	NewWorldBones.Empty(NumBones * NumFrames);
	RawWorldBones.AddZeroed(NumBones * NumFrames);
	NewWorldBones.AddZeroed(NumBones * NumFrames);

	// generate an array to hold the indices of our end effectors
	TArray<int32> EndEffectors;
	EndEffectors.Empty(NumBones);

	// Create an array of FTransform to use as a workspace
	TArray<FTransform> BoneAtoms;

	// setup the raw bone transformation and find all end effectors
	for ( int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
	{
		// get the raw world-atoms for this bone
		UpdateWorldBoneTransformTable(
			AnimSeq, 
			BoneData, 
			RefPose,
			BoneIndex,
			true,
			RawWorldBones);

		// also record all end-effectors we find
		const FBoneData& Bone = BoneData[BoneIndex];
		if (Bone.IsEndEffector())
		{
			EndEffectors.Add(BoneIndex);
		}
	}

	TArray<int32> TargetBoneIndices;
	// for each bone...
	for ( int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
	{
		const FBoneData& Bone = BoneData[BoneIndex];
		const int32 ParentBoneIndex = Bone.GetParent();

		const int32 TrackIndex = AnimSeq->GetSkeleton()->GetAnimationTrackIndex(BoneIndex, AnimSeq, true);

		if (TrackIndex != INDEX_NONE)
		{
			// get the tracks we will be editing for this bone
			FRotationTrack& RotTrack = RotationTracks[TrackIndex];
			FTranslationTrack& TransTrack = PositionTracks[TrackIndex];
			const int32 NumRotKeys = RotTrack.RotKeys.Num();
			const int32 NumPosKeys = TransTrack.PosKeys.Num();
			const int32 NumScaleKeys = (bHasScale)? ScaleTracks[TrackIndex].ScaleKeys.Num() : 0;

			check( (NumPosKeys == 1) || (NumRotKeys == 1) || (NumPosKeys == NumRotKeys) );

			// build an array of end effectors we need to monitor
			TargetBoneIndices.Reset(NumBones);

			int32 HighestTargetBoneIndex = BoneIndex;
			int32 FurthestTargetBoneIndex = BoneIndex;
			int32 ShortestChain = 0;
			float OffsetLength= -1.0f;
			for (int32 EffectorIndex=0; EffectorIndex < EndEffectors.Num(); ++EffectorIndex)
			{
				const int32 EffectorBoneIndex = EndEffectors[EffectorIndex];
				const FBoneData& EffectorBoneData = BoneData[EffectorBoneIndex];

				int32 RootIndex = EffectorBoneData.BonesToRoot.Find(BoneIndex);
				if (RootIndex != INDEX_NONE)
				{
					if (ShortestChain == 0 || (RootIndex+1) < ShortestChain)
					{
						ShortestChain = (RootIndex+1);
					}
					TargetBoneIndices.Add(EffectorBoneIndex);
					HighestTargetBoneIndex = FMath::Max(HighestTargetBoneIndex, EffectorBoneIndex);
					float ChainLength= 0.0f;
					for (long FamilyIndex=0; FamilyIndex < RootIndex; ++FamilyIndex)
					{
						const int32 NextParentBoneIndex= EffectorBoneData.BonesToRoot[FamilyIndex];
						ChainLength += RefPose[NextParentBoneIndex].GetTranslation().Size();
					}

					if (ChainLength > OffsetLength)
					{
						FurthestTargetBoneIndex = EffectorBoneIndex;
						OffsetLength = ChainLength;
					}

				}
			}

			// if requested, retarget the FBoneAtoms towards the target end effectors
			if (bRetarget)
			{
				if (NumScaleKeys > 0 && ParentBoneIndex != INDEX_NONE)
				{
					// update our bone table from the current bone through the last end effector we need to test
					UpdateWorldBoneTransformRange(
						AnimSeq, 
						BoneData, 
						RefPose,
						PositionTracks,
						RotationTracks,
						ScaleTracks,
						BoneIndex,
						HighestTargetBoneIndex,
						false,
						NewWorldBones);

					FScaleTrack& ScaleTrack = ScaleTracks[TrackIndex];

					// adjust all translation keys to align better with the destination
					for ( int32 KeyIndex = 0; KeyIndex < NumScaleKeys; ++KeyIndex )
					{
						FVector& Key= ScaleTrack.ScaleKeys[KeyIndex];

						const int32 FrameIndex= FMath::Clamp(KeyIndex, 0, LastFrame);
						const FTransform& NewWorldParent = NewWorldBones[(ParentBoneIndex*NumFrames) + FrameIndex];
						const FTransform& RawWorldChild = RawWorldBones[(BoneIndex*NumFrames) + FrameIndex];
						const FTransform& RelTM = (RawWorldChild.GetRelativeTransform(NewWorldParent));
						const FTransform Delta = FTransform(RelTM);

						Key = Delta.GetScale3D();
					}
				}
							
				if (NumRotKeys > 0 && ParentBoneIndex != INDEX_NONE)
				{
					if (HighestTargetBoneIndex == BoneIndex)
					{
						for ( int32 KeyIndex = 0; KeyIndex < NumRotKeys; ++KeyIndex )
						{
							FQuat& Key = RotTrack.RotKeys[KeyIndex];

							check(ParentBoneIndex != INDEX_NONE);
							const int32 FrameIndex = FMath::Clamp(KeyIndex, 0, LastFrame);
							FTransform NewWorldParent = NewWorldBones[(ParentBoneIndex*NumFrames) + FrameIndex];
							FTransform RawWorldChild = RawWorldBones[(BoneIndex*NumFrames) + FrameIndex];
							const FTransform& RelTM = (RawWorldChild.GetRelativeTransform(NewWorldParent)); 
							FQuat Rot = FTransform(RelTM).GetRotation();

							const FQuat& AlignedKey = EnforceShortestArc(Key, Rot);
							Key = AlignedKey;
						}
					}
					else
					{
						// update our bone table from the current bone through the last end effector we need to test
						UpdateWorldBoneTransformRange(
							AnimSeq, 
							BoneData, 
							RefPose,
							PositionTracks,
							RotationTracks,
							ScaleTracks,
							BoneIndex,
							HighestTargetBoneIndex,
							false,
							NewWorldBones);
						
						// adjust all rotation keys towards the end effector target
						for ( int32 KeyIndex = 0; KeyIndex < NumRotKeys; ++KeyIndex )
						{
							FQuat& Key = RotTrack.RotKeys[KeyIndex];

							const int32 FrameIndex = FMath::Clamp(KeyIndex, 0, LastFrame);

							const FTransform& NewWorldTransform = NewWorldBones[(BoneIndex*NumFrames) + FrameIndex];

							const FTransform& DesiredChildTransform = RawWorldBones[(FurthestTargetBoneIndex*NumFrames) + FrameIndex].GetRelativeTransform(NewWorldTransform);
							const FTransform& CurrentChildTransform = NewWorldBones[(FurthestTargetBoneIndex*NumFrames) + FrameIndex].GetRelativeTransform(NewWorldTransform);

							// find the two vectors which represent the angular error we are trying to correct
							const FVector& CurrentHeading = CurrentChildTransform.GetTranslation();
							const FVector& DesiredHeading = DesiredChildTransform.GetTranslation();

							// if these are valid, we can continue
							if (!CurrentHeading.IsNearlyZero() && !DesiredHeading.IsNearlyZero())
							{
								const float DotResult = CurrentHeading.GetSafeNormal() | DesiredHeading.GetSafeNormal();

								// limit the range we will retarget to something reasonable (~60 degrees)
								if (DotResult < 1.0f && DotResult > 0.5f)
								{
									FQuat Adjustment= FQuat::FindBetweenVectors(CurrentHeading, DesiredHeading);
									Adjustment= EnforceShortestArc(FQuat::Identity, Adjustment);

									const FVector Test = Adjustment.RotateVector(CurrentHeading);
									const float DeltaSqr = (Test - DesiredHeading).SizeSquared();
									if (DeltaSqr < FMath::Square(0.001f))
									{
										FQuat NewKey = Adjustment * Key;
										NewKey.Normalize();

										const FQuat& AlignedKey = EnforceShortestArc(Key, NewKey);
										Key = AlignedKey;
									}
								}
							}
						}
					}
				}

				if (NumPosKeys > 0 && ParentBoneIndex != INDEX_NONE)
				{
					// update our bone table from the current bone through the last end effector we need to test
					UpdateWorldBoneTransformRange(
						AnimSeq, 
						BoneData, 
						RefPose,
						PositionTracks,
						RotationTracks,
						ScaleTracks,
						BoneIndex,
						HighestTargetBoneIndex,
						false,
						NewWorldBones);

					// adjust all translation keys to align better with the destination
					for ( int32 KeyIndex = 0; KeyIndex < NumPosKeys; ++KeyIndex )
					{
						FVector& Key= TransTrack.PosKeys[KeyIndex];

						const int32 FrameIndex= FMath::Clamp(KeyIndex, 0, LastFrame);
						FTransform NewWorldParent = NewWorldBones[(ParentBoneIndex*NumFrames) + FrameIndex];
						FTransform RawWorldChild = RawWorldBones[(BoneIndex*NumFrames) + FrameIndex];
						const FTransform& RelTM = RawWorldChild.GetRelativeTransform(NewWorldParent);
						const FTransform Delta = FTransform(RelTM);
						ensure (!Delta.ContainsNaN());

						Key = Delta.GetTranslation();
					}
				}

			}

			// look for a parent track to reference as a guide
			int32 GuideTrackIndex = INDEX_NONE;
			if (ParentKeyScale > 1.0f)
			{
				for (long FamilyIndex=0; (FamilyIndex < Bone.BonesToRoot.Num()) && (GuideTrackIndex == INDEX_NONE); ++FamilyIndex)
				{
					const int32 NextParentBoneIndex= Bone.BonesToRoot[FamilyIndex];

					GuideTrackIndex = AnimSeq->GetSkeleton()->GetAnimationTrackIndex(NextParentBoneIndex, AnimSeq, true);
				}
			}

			// update our bone table from the current bone through the last end effector we need to test
			UpdateWorldBoneTransformRange(
				AnimSeq, 
				BoneData, 
				RefPose,
				PositionTracks,
				RotationTracks,
				ScaleTracks,
				BoneIndex,
				HighestTargetBoneIndex,
				false,
				NewWorldBones);

			// rebuild the BoneAtoms table using the current set of keys
			UpdateBoneAtomList(AnimSeq, BoneIndex, TrackIndex, NumFrames, TimePerFrame, BoneAtoms); 

			// determine the EndEffectorTolerance. 
			// We use the Maximum value by default, and the Minimum value
			// as we approach the end effectors
			float EndEffectorTolerance = MaxEffectorDiff;
			if (ShortestChain <= 1)
			{
				EndEffectorTolerance = MinEffectorDiff;
			}

			// Determine if a guidance track should be used to aid in choosing keys to retain
			TArray<float>* GuidanceTrack = NULL;
			float GuidanceScale = 1.0f;
			if (GuideTrackIndex != INDEX_NONE)
			{
				FTranslationTrack& GuideTransTrack = PositionTracks[GuideTrackIndex];
				GuidanceTrack = &GuideTransTrack.Times;
				GuidanceScale = ParentKeyScale;
			}
			
			// if the TargetBoneIndices array is empty, then this bone is an end effector.
			// so we add it to the list to maintain our tolerance checks
			if (TargetBoneIndices.Num() == 0)
			{
				TargetBoneIndices.Add(BoneIndex);
			}

			if (bActuallyFilterLinearKeys)
			{
				if (bHasScale)
				{
					FScaleTrack& ScaleTrack = ScaleTracks[TrackIndex];
					// filter out translations we can approximate through interpolation
					FilterLinearKeysTemplate<ScaleAdapter>(
						ScaleTrack.ScaleKeys, 
						ScaleTrack.Times, 
						BoneAtoms,
						GuidanceTrack, 
						RawWorldBones,
						NewWorldBones,
						TargetBoneIndices,
						NumFrames,
						BoneIndex,
						ParentBoneIndex,
						GuidanceScale, 
						MaxScaleDiff, 
						EndEffectorTolerance,
						EffectorDiffSocket,
						BoneData);

					// update our bone table from the current bone through the last end effector we need to test
					UpdateWorldBoneTransformRange(
						AnimSeq, 
						BoneData, 
						RefPose,
						PositionTracks,
						RotationTracks,
						ScaleTracks,
						BoneIndex,
						HighestTargetBoneIndex,
						false,
						NewWorldBones);

					// rebuild the BoneAtoms table using the current set of keys
					UpdateBoneAtomList(AnimSeq, BoneIndex, TrackIndex, NumFrames, TimePerFrame, BoneAtoms); 
				}

				// filter out translations we can approximate through interpolation
				FilterLinearKeysTemplate<TranslationAdapter>(
					TransTrack.PosKeys, 
					TransTrack.Times, 
					BoneAtoms,
					GuidanceTrack, 
					RawWorldBones,
					NewWorldBones,
					TargetBoneIndices,
					NumFrames,
					BoneIndex,
					ParentBoneIndex,
					GuidanceScale, 
					MaxPosDiff, 
					EndEffectorTolerance,
					EffectorDiffSocket,
					BoneData);

				// update our bone table from the current bone through the last end effector we need to test
				UpdateWorldBoneTransformRange(
					AnimSeq, 
					BoneData, 
					RefPose,
					PositionTracks,
					RotationTracks,
					ScaleTracks,
					BoneIndex,
					HighestTargetBoneIndex,
					false,
					NewWorldBones);

				// rebuild the BoneAtoms table using the current set of keys
				UpdateBoneAtomList(AnimSeq, BoneIndex, TrackIndex, NumFrames, TimePerFrame, BoneAtoms); 

				// filter out rotations we can approximate through interpolation
				FilterLinearKeysTemplate<RotationAdapter>(
					RotTrack.RotKeys, 
					RotTrack.Times, 
					BoneAtoms,
					GuidanceTrack, 
					RawWorldBones,
					NewWorldBones,
					TargetBoneIndices,
					NumFrames,
					BoneIndex,
					ParentBoneIndex,
					GuidanceScale, 
					MaxAngleDiff, 
					EndEffectorTolerance,
					EffectorDiffSocket,
					BoneData);
			}
		}

		// make sure the final compressed keys are repesented in our NewWorldBones table
		UpdateWorldBoneTransformRange(
			AnimSeq, 
			BoneData, 
			RefPose,
			PositionTracks,
			RotationTracks,
			ScaleTracks,
			BoneIndex,
			BoneIndex,
			false,
			NewWorldBones);
	}
};


static void CalculateBoneChainInformation(const FProcessAnimationTracksContext& Context, int32 BoneIndex, int32& OutHighestTargetBoneIndex, int32& OutFurthestTargetBoneIndex, int32& OutShortestChain, TArray<int32>& OutTargetBoneIndices)
{
	const int32 NumBones = Context.BoneData.Num();

	OutTargetBoneIndices.Reset(NumBones);

	int32 HighestTargetBoneIndex = BoneIndex;
	int32 FurthestTargetBoneIndex = BoneIndex;
	int32 ShortestChain = 0;
	float OffsetLength = -1.0f;
	for (int32 EffectorIndex = 0; EffectorIndex < Context.EndEffectors.Num(); ++EffectorIndex)
	{
		const int32 EffectorBoneIndex = Context.EndEffectors[EffectorIndex];
		const FBoneData& EffectorBoneData = Context.BoneData[EffectorBoneIndex];

		int32 RootIndex = EffectorBoneData.BonesToRoot.Find(BoneIndex);
		if (RootIndex != INDEX_NONE)
		{
			if (ShortestChain == 0 || (RootIndex + 1) < ShortestChain)
			{
				ShortestChain = (RootIndex + 1);
			}

			OutTargetBoneIndices.Add(EffectorBoneIndex);
			HighestTargetBoneIndex = FMath::Max(HighestTargetBoneIndex, EffectorBoneIndex);
			float ChainLength = 0.0f;
			for (long FamilyIndex = 0; FamilyIndex < RootIndex; ++FamilyIndex)
			{
				const int32 NextParentBoneIndex = EffectorBoneData.BonesToRoot[FamilyIndex];
				ChainLength += Context.RefPose[NextParentBoneIndex].GetTranslation().Size();
			}

			if (ChainLength > OffsetLength)
			{
				FurthestTargetBoneIndex = EffectorBoneIndex;
				OffsetLength = ChainLength;
			}
		}
	}

	OutHighestTargetBoneIndex = HighestTargetBoneIndex;
	OutFurthestTargetBoneIndex = FurthestTargetBoneIndex;
	OutShortestChain = ShortestChain;
}

void UAnimCompress_RemoveLinearKeys::PerformRetargeting(
	FProcessAnimationTracksContext& Context,
	int32 BoneIndex,
	int32 HighestTargetBoneIndex,
	int32 FurthestTargetBoneIndex)
{
	const int32 TrackIndex = Context.BoneIndexToTrackIndex[BoneIndex];
	const FBoneData& Bone = Context.BoneData[BoneIndex];
	const int32 ParentBoneIndex = Bone.GetParent();

	const int32 NumScaleKeys = Context.Segment.ScaleData.Num() > 0 ? Context.Segment.ScaleData[TrackIndex].ScaleKeys.Num() : 0;
	if (NumScaleKeys > 1 && ParentBoneIndex != INDEX_NONE)
	{
		// update our bone table from the current bone through the last end effector we need to test
		UpdateWorldBoneTransformRange(Context, BoneIndex, HighestTargetBoneIndex);

		FScaleTrack& ScaleTrack = Context.Segment.ScaleData[TrackIndex];

		// adjust all translation keys to align better with the destination
		for (int32 KeyIndex = 0; KeyIndex < NumScaleKeys; ++KeyIndex)
		{
			FVector& Key = ScaleTrack.ScaleKeys[KeyIndex];

			const int32 FrameIndex = FMath::Clamp(KeyIndex, 0, Context.Segment.NumFrames);
			const FTransform& NewWorldParent = Context.NewWorldBones[(ParentBoneIndex * Context.Segment.NumFrames) + FrameIndex];
			const FTransform& RawWorldChild = Context.RawWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex];
			const FTransform RelTM = RawWorldChild.GetRelativeTransform(NewWorldParent);

			Key = RelTM.GetScale3D();
		}
	}

	FRotationTrack& RotTrack = Context.Segment.RotationData[TrackIndex];
	const int32 NumRotKeys = RotTrack.RotKeys.Num();
	if (NumRotKeys > 1 && ParentBoneIndex != INDEX_NONE)
	{
		// update our bone table from the current bone through the last end effector we need to test
		UpdateWorldBoneTransformRange(Context, BoneIndex, HighestTargetBoneIndex);

		if (HighestTargetBoneIndex == BoneIndex)
		{
			for (int32 KeyIndex = 0; KeyIndex < NumRotKeys; ++KeyIndex)
			{
				FQuat& Key = RotTrack.RotKeys[KeyIndex];

				const int32 FrameIndex = FMath::Clamp(KeyIndex, 0, Context.Segment.NumFrames);
				FTransform NewWorldParent = Context.NewWorldBones[(ParentBoneIndex * Context.Segment.NumFrames) + FrameIndex];
				FTransform RawWorldChild = Context.RawWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex];
				const FTransform RelTM = RawWorldChild.GetRelativeTransform(NewWorldParent);
				FQuat Rot = RelTM.GetRotation();

				const FQuat AlignedKey = EnforceShortestArc(Key, Rot);
				Key = AlignedKey;
			}
		}
		else
		{
			// adjust all rotation keys towards the end effector target
			for (int32 KeyIndex = 0; KeyIndex < NumRotKeys; ++KeyIndex)
			{
				FQuat& Key = RotTrack.RotKeys[KeyIndex];

				const int32 FrameIndex = FMath::Clamp(KeyIndex, 0, Context.Segment.NumFrames);

				const FTransform& NewWorldTransform = Context.NewWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex];

				const FTransform DesiredChildTransform = Context.RawWorldBones[(FurthestTargetBoneIndex * Context.Segment.NumFrames) + FrameIndex].GetRelativeTransform(NewWorldTransform);
				const FTransform CurrentChildTransform = Context.NewWorldBones[(FurthestTargetBoneIndex * Context.Segment.NumFrames) + FrameIndex].GetRelativeTransform(NewWorldTransform);

				// find the two vectors which represent the angular error we are trying to correct
				const FVector CurrentHeading = CurrentChildTransform.GetTranslation();
				const FVector DesiredHeading = DesiredChildTransform.GetTranslation();

				// if these are valid, we can continue
				if (!CurrentHeading.IsNearlyZero() && !DesiredHeading.IsNearlyZero())
				{
					const float DotResult = CurrentHeading.GetSafeNormal() | DesiredHeading.GetSafeNormal();

					// limit the range we will retarget to something reasonable (~60 degrees)
					if (DotResult < 1.0f && DotResult > 0.5f)
					{
						FQuat Adjustment = FQuat::FindBetweenVectors(CurrentHeading, DesiredHeading);
						Adjustment = EnforceShortestArc(FQuat::Identity, Adjustment);

						const FVector Test = Adjustment.RotateVector(CurrentHeading);
						const float DeltaSqr = (Test - DesiredHeading).SizeSquared();
						if (DeltaSqr < FMath::Square(0.001f))
						{
							FQuat NewKey = Adjustment * Key;
							NewKey.Normalize();

							const FQuat AlignedKey = EnforceShortestArc(Key, NewKey);
							Key = AlignedKey;
						}
					}
				}
			}
		}
	}

	FTranslationTrack& TransTrack = Context.Segment.TranslationData[TrackIndex];
	const int32 NumPosKeys = TransTrack.PosKeys.Num();
	if (NumPosKeys > 1 && ParentBoneIndex != INDEX_NONE)
	{
		// update our bone table from the current bone through the last end effector we need to test
		UpdateWorldBoneTransformRange(Context, BoneIndex, HighestTargetBoneIndex);

		// adjust all translation keys to align better with the destination
		for (int32 KeyIndex = 0; KeyIndex < NumPosKeys; ++KeyIndex)
		{
			FVector& Key = TransTrack.PosKeys[KeyIndex];

			const int32 FrameIndex = FMath::Clamp(KeyIndex, 0, Context.Segment.NumFrames);
			const FTransform& NewWorldParent = Context.NewWorldBones[(ParentBoneIndex * Context.Segment.NumFrames) + FrameIndex];
			const FTransform& RawWorldChild = Context.RawWorldBones[(BoneIndex * Context.Segment.NumFrames) + FrameIndex];
			const FTransform RelTM = RawWorldChild.GetRelativeTransform(NewWorldParent);

			Key = RelTM.GetTranslation();
		}
	}
}

static int32 FindGuideTrackIndex(const FBoneData& Bone, const TArray<int32>& BoneIndexToTrackIndex, float ParentKeyScale)
{
	int32 GuideTrackIndex = INDEX_NONE;
	if (ParentKeyScale > 1.0f)
	{
		for (int32 FamilyIndex = 0; FamilyIndex < Bone.BonesToRoot.Num() && GuideTrackIndex == INDEX_NONE; ++FamilyIndex)
		{
			const int32 NextParentBoneIndex = Bone.BonesToRoot[FamilyIndex];

			GuideTrackIndex = BoneIndexToTrackIndex[NextParentBoneIndex];
		}
	}

	return GuideTrackIndex;
}

void UAnimCompress_RemoveLinearKeys::ProcessAnimationTracks(FProcessAnimationTracksContext& Context)
{
	const int32 NumBones = Context.BoneData.Num();

	Context.RawWorldBones.Empty(NumBones * Context.Segment.NumFrames);
	Context.NewWorldBones.Empty(NumBones * Context.Segment.NumFrames);
	Context.RawWorldBones.AddZeroed(NumBones * Context.Segment.NumFrames);
	Context.NewWorldBones.AddZeroed(NumBones * Context.Segment.NumFrames);

	TArray<int32> TargetBoneIndices;
	TargetBoneIndices.Empty(NumBones);

	// Create an array of FTransform to use as a workspace
	TArray<FTransform> BoneAtoms;

	// setup the raw bone transformation and find all end effectors
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		// get the raw world-atoms for this bone
		UpdateWorldBoneTransformTable(Context, BoneIndex, true, Context.RawWorldBones);
	}

	const bool bHasScale = Context.Segment.ScaleData.Num() > 0;

	// for each bone...
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FBoneData& Bone = Context.BoneData[BoneIndex];
		const int32 ParentBoneIndex = Bone.GetParent();

		const int32 TrackIndex = Context.BoneIndexToTrackIndex[BoneIndex];

		if (TrackIndex != INDEX_NONE)
		{
			// get the tracks we will be editing for this bone
			FRotationTrack& RotTrack = Context.Segment.RotationData[TrackIndex];
			FTranslationTrack& TransTrack = Context.Segment.TranslationData[TrackIndex];
			const int32 NumRotKeys = RotTrack.RotKeys.Num();
			const int32 NumPosKeys = TransTrack.PosKeys.Num();
			const int32 NumScaleKeys = bHasScale ? Context.Segment.ScaleData[TrackIndex].ScaleKeys.Num() : 0;

			check((NumPosKeys == 1) || (NumRotKeys == 1) || (NumPosKeys == NumRotKeys));

			// build an array of end effectors we need to monitor
			int32 HighestTargetBoneIndex;
			int32 FurthestTargetBoneIndex;
			int32 ShortestChain;
			CalculateBoneChainInformation(Context, BoneIndex, HighestTargetBoneIndex, FurthestTargetBoneIndex, ShortestChain, TargetBoneIndices);

			// if requested, retarget the FBoneAtoms towards the target end effectors
			if (bRetarget)
			{
				PerformRetargeting(Context, BoneIndex, HighestTargetBoneIndex, FurthestTargetBoneIndex);
			}

			// look for a parent track to reference as a guide
			const int32 GuideTrackIndex = FindGuideTrackIndex(Bone, Context.BoneIndexToTrackIndex, ParentKeyScale);

			// update our bone table from the current bone through the last end effector we need to test
			UpdateWorldBoneTransformRange(Context, BoneIndex, HighestTargetBoneIndex);

			// rebuild the BoneAtoms table using the current set of keys
			UpdateBoneAtomList(Context, TrackIndex, BoneAtoms);

			// determine the EndEffectorTolerance. 
			// We use the Maximum value by default, and the Minimum value
			// as we approach the end effectors
			const float EndEffectorTolerance = ShortestChain <= 1 ? MinEffectorDiff : MaxEffectorDiff;

			// Determine if a guidance track should be used to aid in choosing keys to retain
			const TArray<float>* GuidanceTrack = nullptr;
			float GuidanceScale = 1.0f;
			if (GuideTrackIndex != INDEX_NONE)
			{
				const FTranslationTrack& GuideTransTrack = Context.Segment.TranslationData[GuideTrackIndex];
				GuidanceTrack = &GuideTransTrack.Times;
				GuidanceScale = ParentKeyScale;
			}

			// if the TargetBoneIndices array is empty, then this bone is an end effector.
			// so we add it to the list to maintain our tolerance checks
			if (TargetBoneIndices.Num() == 0)
			{
				TargetBoneIndices.Add(BoneIndex);
			}

			if (bActuallyFilterLinearKeys)
			{
				if (bHasScale)
				{
					FScaleTrack& ScaleTrack = Context.Segment.ScaleData[TrackIndex];
					// filter out translations we can approximate through interpolation
					FilterLinearKeysTemplate<ScaleAdapter>(
						ScaleTrack.ScaleKeys,
						ScaleTrack.Times,
						BoneAtoms,
						GuidanceTrack,
						Context,
						TargetBoneIndices,
						BoneIndex,
						ParentBoneIndex,
						GuidanceScale,
						MaxScaleDiff,
						EndEffectorTolerance,
						EffectorDiffSocket);

					// update our bone table from the current bone through the last end effector we need to test
					UpdateWorldBoneTransformRange(Context, BoneIndex, HighestTargetBoneIndex);

					// rebuild the BoneAtoms table using the current set of keys
					UpdateBoneAtomList(Context, TrackIndex, BoneAtoms);
				}

				// filter out translations we can approximate through interpolation
				FilterLinearKeysTemplate<TranslationAdapter>(
					TransTrack.PosKeys,
					TransTrack.Times,
					BoneAtoms,
					GuidanceTrack,
					Context,
					TargetBoneIndices,
					BoneIndex,
					ParentBoneIndex,
					GuidanceScale,
					MaxPosDiff,
					EndEffectorTolerance,
					EffectorDiffSocket);

				// update our bone table from the current bone through the last end effector we need to test
				UpdateWorldBoneTransformRange(Context, BoneIndex, HighestTargetBoneIndex);

				// rebuild the BoneAtoms table using the current set of keys
				UpdateBoneAtomList(Context, TrackIndex, BoneAtoms);

				// filter out rotations we can approximate through interpolation
				FilterLinearKeysTemplate<RotationAdapter>(
					RotTrack.RotKeys,
					RotTrack.Times,
					BoneAtoms,
					GuidanceTrack,
					Context,
					TargetBoneIndices,
					BoneIndex,
					ParentBoneIndex,
					GuidanceScale,
					MaxAngleDiff,
					EndEffectorTolerance,
					EffectorDiffSocket);
			}
		}

		// make sure the final compressed keys are represented in our NewWorldBones table
		UpdateWorldBoneTransformRange(Context, BoneIndex, BoneIndex);
	}
}

/**
 * Struct that holds the relevant information to process animation tracks in parallel.
 * Instances of this structure are live as long as parallel task instances are live.
 */
struct FAsyncProcessAnimationTracksTaskGroupContext
{
	TArray<FProcessAnimationTracksContext*> TaskContexes;
	volatile int32 AtomicTaskIndexCounter;
	volatile int32 AtomicNumExecutedTasks;

	FAsyncProcessAnimationTracksTaskGroupContext()
		: AtomicTaskIndexCounter(0)
		, AtomicNumExecutedTasks(0)
	{}

	~FAsyncProcessAnimationTracksTaskGroupContext()
	{
		for (FProcessAnimationTracksContext* Context : TaskContexes)
		{
			delete Context;
		}
	}

	void ExecuteTasks(UAnimCompress_RemoveLinearKeys* Compressor)
	{
		while (true)
		{
			const int32 TaskIndex = FPlatformAtomics::InterlockedIncrement(&AtomicTaskIndexCounter) - 1;
			if (TaskIndex >= TaskContexes.Num())
			{
				break;
			}

			FProcessAnimationTracksContext* JobContext = TaskContexes[TaskIndex];
			Compressor->ProcessAnimationTracks(*JobContext);

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
 * Class that represents a parallel task to perform linear key reduction.
 */
class FAsyncProcessAnimationTracksTask
{
public:
	FAsyncProcessAnimationTracksTask(UAnimCompress_RemoveLinearKeys* Compressor_, FAsyncProcessAnimationTracksTaskGroupContext* TaskGroupContext_)
		: Compressor(Compressor_)
		, TaskGroupContext(TaskGroupContext_)
	{}

	/** return the name of the task **/
	static const TCHAR* GetTaskName() { return TEXT("FAsyncProcessAnimationTracksTask"); }
	FORCEINLINE static TStatId GetStatId() { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncProcessAnimationTracksTask, STATGROUP_TaskGraphTasks); }

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskGroupContext->ExecuteTasks(Compressor);
	}

	UAnimCompress_RemoveLinearKeys* Compressor;
	FAsyncProcessAnimationTracksTaskGroupContext* TaskGroupContext;
};

/**
 * Class that represents a clean up task once every parallel task has executed.
 * This is executed last.
 */
class FAsyncCleanUpProcessAnimationTracksTask
{
public:
	FAsyncCleanUpProcessAnimationTracksTask(FAsyncProcessAnimationTracksTaskGroupContext* TaskGroupContext_)
		: TaskGroupContext(TaskGroupContext_)
	{}

	/** return the name of the task **/
	static const TCHAR* GetTaskName() { return TEXT("FAsyncCleanUpProcessAnimationTracksTask"); }
	FORCEINLINE static TStatId GetStatId() { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCleanUpProcessAnimationTracksTask, STATGROUP_TaskGraphTasks); }

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		delete TaskGroupContext;
	}

	FAsyncProcessAnimationTracksTaskGroupContext* TaskGroupContext;
};

void UAnimCompress_RemoveLinearKeys::ProcessAnimationTracks(
	UAnimSequence& AnimSeq,
	const TArray<FBoneData>& BoneData,
	TArray<FAnimSegmentContext>& RawSegments)
{
	// extract all the data we'll need about the skeleton and animation sequence
	const int32 NumBones = BoneData.Num();

	// make sure the parent key scale is properly bound to 1.0 or more
	ParentKeyScale = FMath::Max(ParentKeyScale, 1.0f);

	TArray<int32> BoneIndexToTrackIndex;
	BoneIndexToTrackIndex.Empty(NumBones);
	BoneIndexToTrackIndex.AddUninitialized(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		BoneIndexToTrackIndex[BoneIndex] = AnimSeq.GetSkeleton()->GetAnimationTrackIndex(BoneIndex, &AnimSeq, true);
	}

	// generate an array to hold the indices of our end effectors
	TArray<int32> EndEffectors;
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FBoneData& Bone = BoneData[BoneIndex];
		if (Bone.IsEndEffector())
		{
			EndEffectors.Add(BoneIndex);
		}
	}

	const int32 NumTracks = RawSegments[0].RotationData.Num();
	TArray<FTransform> TrivialTrackValues;
	TrivialTrackValues.Empty(NumTracks);
	TrivialTrackValues.AddUninitialized(NumTracks);
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		const FQuat& Rotation = RawSegments[0].RotationData[TrackIndex].RotKeys[0];
		const FVector& Translation = RawSegments[0].TranslationData[TrackIndex].PosKeys[0];
		FVector Scale = FVector::OneVector;
		if (RawSegments[0].ScaleData.Num() > 0)
		{
			Scale = RawSegments[0].ScaleData[TrackIndex].ScaleKeys[0];
		}

		TrivialTrackValues[TrackIndex] = FTransform(Rotation, Translation, Scale);
	}

	if (bUseDecompression || !bUseMultithreading || RawSegments.Num() <= 1)
	{
		for (FAnimSegmentContext& Segment : RawSegments)
		{
			FProcessAnimationTracksContext Context(AnimSeq, BoneData, EndEffectors, BoneIndexToTrackIndex, TrivialTrackValues, Segment, RawSegments);
			ProcessAnimationTracks(Context);
		}
	}
	else
	{
		// Created the context objects.
		FAsyncProcessAnimationTracksTaskGroupContext* TaskGroupContext = new FAsyncProcessAnimationTracksTaskGroupContext();
		for (FAnimSegmentContext& Segment : RawSegments)
		{
			FProcessAnimationTracksContext* Context = new FProcessAnimationTracksContext(AnimSeq, BoneData, EndEffectors, BoneIndexToTrackIndex, TrivialTrackValues, Segment, RawSegments);
			TaskGroupContext->TaskContexes.Add(Context);
		}

		// Dispatch 1 task per job thread.
		FGraphEventArray AsyncTaskCompletionEvents;
		const int32 NumTaskThreads = FTaskGraphInterface::Get().GetNumWorkerThreads();
		for (int32 TaskIndex = 0; TaskIndex < NumTaskThreads; ++TaskIndex)
		{
			AsyncTaskCompletionEvents.Add(TGraphTask<FAsyncProcessAnimationTracksTask>::CreateTask(NULL, ENamedThreads::AnyThread).ConstructAndDispatchWhenReady(this, TaskGroupContext));
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
		TGraphTask<FAsyncCleanUpProcessAnimationTracksTask>::CreateTask(&AsyncTaskCompletionEvents, ENamedThreads::AnyThread).ConstructAndDispatchWhenReady(TaskGroupContext);
	}
}


void UAnimCompress_RemoveLinearKeys::CompressUsingUnderlyingCompressor(
	UAnimSequence* AnimSeq,
	const TArray<FBoneData>& BoneData,
	const TArray<FTranslationTrack>& TranslationData,
	const TArray<FRotationTrack>& RotationData,
	const TArray<FScaleTrack>& ScaleData,
	const bool bFinalPass)
{
	BitwiseCompressAnimationTracks(
		AnimSeq,
		static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
		static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
		static_cast<AnimationCompressionFormat>(ScaleCompressionFormat),
		TranslationData,
		RotationData,
		ScaleData,
		true);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_VariableKeyLerp;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);
}

void UAnimCompress_RemoveLinearKeys::CompressUsingUnderlyingCompressor(
	UAnimSequence& AnimSeq,
	const TArray<FBoneData>& BoneData,
	TArray<FAnimSegmentContext>& RawSegments,
	const bool bFinalPass)
{
	// record the proper runtime decompressor to use
	AnimSeq.KeyEncodingFormat = AKF_VariableKeyLerp;
	AnimSeq.TranslationCompressionFormat = TranslationCompressionFormat;
	AnimSeq.RotationCompressionFormat = RotationCompressionFormat;
	AnimSeq.ScaleCompressionFormat = ScaleCompressionFormat;
	AnimationFormat_SetInterfaceLinks(AnimSeq);

	const bool bIsSorted = bOptimizeForForwardPlayback;

	BitwiseCompressAnimationTracks(
		AnimSeq,
		static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
		static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
		static_cast<AnimationCompressionFormat>(ScaleCompressionFormat),
		RawSegments,
		bIsSorted);

	CoalesceCompressedSegments(AnimSeq, RawSegments, bIsSorted);

	// We could be invalid, set the links again
	AnimationFormat_SetInterfaceLinks(AnimSeq);
}

void UAnimCompress_RemoveLinearKeys::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	// Only need to do the heavy lifting if it will have some impact
	// One of these will always be true for the base class, but derived classes may choose to turn both off (e.g., in PerTrackCompression)
	const bool bRunningProcessor = bRetarget || bActuallyFilterLinearKeys;

	// If the processor is to be run, then additive animations need to be converted from relative to absolute
	const bool bNeedToConvertBackToAdditive = bRunningProcessor ? ConvertFromRelativeSpace(AnimSeq) : false;

	// Separate the raw data into tracks and remove trivial tracks (all the same value)
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	TArray<FScaleTrack> ScaleData;
	SeparateRawDataIntoTracks(AnimSeq->GetRawAnimationData(), AnimSeq->SequenceLength, TranslationData, RotationData, ScaleData);
	FilterBeforeMainKeyRemoval(AnimSeq, BoneData, TranslationData, RotationData, ScaleData);

#if USE_SEGMENTING_CONTEXT
	if (bEnableSegmenting)
	{
#if TIME_LINEAR_KEY_REMOVAL
		double TimeStart = FPlatformTime::Seconds();
#endif

		TArray<FAnimSegmentContext> RawSegments;
		SeparateRawDataIntoTracks(*AnimSeq, TranslationData, RotationData, ScaleData, IdealNumFramesPerSegment, MaxNumFramesPerSegment, RawSegments);

		if (bRunningProcessor)
		{
			if (bUseDecompression)
			{
				// compress this animation without any key-reduction to prime the codec
				CompressUsingUnderlyingCompressor(
					*AnimSeq,
					BoneData,
					RawSegments,
					false);
			}
			else
			{
				// record the proper runtime decompressor to use
				AnimSeq->KeyEncodingFormat = AKF_VariableKeyLerp;
				AnimSeq->TranslationCompressionFormat = TranslationCompressionFormat;
				AnimSeq->RotationCompressionFormat = RotationCompressionFormat;
				AnimSeq->ScaleCompressionFormat = ScaleCompressionFormat;
				AnimationFormat_SetInterfaceLinks(*AnimSeq);

				AnimSeq->CompressedByteStream.Empty();
				AnimSeq->CompressedScaleOffsets.Empty();
				AnimSeq->CompressedTrackOffsets.Empty();
				AnimSeq->CompressedSegments.Empty();

				for (FAnimSegmentContext& Segment : RawSegments)
				{
					Segment.TranslationCompressionFormat = TranslationCompressionFormat;
					Segment.RotationCompressionFormat = RotationCompressionFormat;
					Segment.ScaleCompressionFormat = ScaleCompressionFormat;
				}
			}

			// now remove the keys which can be approximated with linear interpolation
			ProcessAnimationTracks(
				*AnimSeq,
				BoneData,
				RawSegments);

#if TIME_LINEAR_KEY_REMOVAL
			double ElapsedTime = FPlatformTime::Seconds() - TimeStart;
			UE_LOG(LogAnimationCompression, Log, TEXT("ProcessAnimationTracks time is (%f) seconds"), ElapsedTime);
#endif

			// if previously additive, convert back to relative-space
			if (bNeedToConvertBackToAdditive)
			{
				ConvertToRelativeSpace(*AnimSeq);

				for (FAnimSegmentContext& Segment : RawSegments)
				{
					ConvertToRelativeSpace(*AnimSeq, Segment.TranslationData, Segment.RotationData, Segment.ScaleData);
				}
			}
		}

		// compress the final (possibly key-reduced) tracks into the anim sequence buffers
		CompressUsingUnderlyingCompressor(
			*AnimSeq,
			BoneData,
			RawSegments,
			true);

		return;
	}
#endif
	if (bRunningProcessor)
	{
#if TIME_LINEAR_KEY_REMOVAL
		double TimeStart = FPlatformTime::Seconds();
#endif
		// compress this animation without any key-reduction to prime the codec
		CompressUsingUnderlyingCompressor(
			AnimSeq,
			BoneData, 
			TranslationData,
			RotationData,
			ScaleData,
			false);

		// now remove the keys which can be approximated with linear interpolation
		ProcessAnimationTracks(
			AnimSeq,
			BoneData,
			TranslationData,
			RotationData, 
			ScaleData);
#if TIME_LINEAR_KEY_REMOVAL
		double ElapsedTime = FPlatformTime::Seconds() - TimeStart;
		UE_LOG(LogAnimationCompression, Log, TEXT("ProcessAnimationTracks time is (%f) seconds"),ElapsedTime);
#endif

		// if previously additive, convert back to relative-space
		if( bNeedToConvertBackToAdditive )
		{
			ConvertToRelativeSpace(AnimSeq, TranslationData, RotationData, ScaleData);
		}
	}

	// compress the final (possibly key-reduced) tracks into the anim sequence buffers
	CompressUsingUnderlyingCompressor(
		AnimSeq,
		BoneData,
		TranslationData,
		RotationData,
		ScaleData,
		true);

#endif // WITH_EDITORONLY_DATA
}

void UAnimCompress_RemoveLinearKeys::PopulateDDCKey(FArchive& Ar)
{
	Super::PopulateDDCKey(Ar);
	Ar << MaxPosDiff;
	Ar << MaxAngleDiff;
	Ar << MaxScaleDiff;
	Ar << MaxEffectorDiff;
	Ar << MinEffectorDiff;
	Ar << EffectorDiffSocket;
	Ar << ParentKeyScale;
	uint8 Flags =	MakeBitForFlag(bRetarget, 0) +
					MakeBitForFlag(bActuallyFilterLinearKeys, 1) +
					MakeBitForFlag(bOptimizeForForwardPlayback, 2);
	Ar << Flags;
}

#endif // WITH_EIDOT

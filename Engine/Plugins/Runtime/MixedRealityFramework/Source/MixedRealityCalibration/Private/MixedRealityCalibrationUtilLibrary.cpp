// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityCalibrationUtilLibrary.h"
#include "Math/NumericLimits.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "OpenCVHelper.h" // for OPENCV_INCLUDES_START/END

#if WITH_OPENCV
OPENCV_INCLUDES_START
#undef check
#include "opencv2/core.hpp" // for cv::eigen()
#include "opencv2/core/mat.hpp"
OPENCV_INCLUDES_END
#endif

/* FMRSampleSegment 
 *****************************************************************************/

struct FMRSampleSegment
{
	/** Reorder PointA & B, so that B is further than A (so that [PointA - PointB] points towards the ViewOrigin) */
	FVector Orient(const FVector& ViewOrigin, const FRotator& ViewOrientation);

	FVector   PointA;
	FVector   PointB;
	FIntPoint PlanarId;
};

FVector FMRSampleSegment::Orient(const FVector& ViewOrigin, const FRotator& ViewOrientation)
{
	const FVector LookAtVec = ViewOrientation.Vector();

	auto GetViewDepth = [ViewOrigin, LookAtVec](const FVector& WorldPt)->float
	{
		const FVector ToPt = (WorldPt - ViewOrigin);
		return FVector::DotProduct(ToPt, LookAtVec);
	};

	const float ViewDepthA = GetViewDepth(PointA);
	const float ViewDepthB = GetViewDepth(PointB);

	float MaxDepth = ViewDepthB;
	if (ViewDepthA > ViewDepthB)
	{
		const FVector Temp = PointA;
		PointA = PointB;
		PointB = Temp;

		MaxDepth = ViewDepthA;
	}

	return PointB - PointA;
}

/* MixedRealityUtilLibrary_Impl 
 *****************************************************************************/

namespace MRCalibrationUtilLibrary_Impl
{
	static FVector FindAvgVector(const TArray<FVector>& VectorSet);

	static void ComputeDivergenceField(const TArray<FVector>& VectorSet, TArray<float>& Out);

	template<typename T> 
	static void FindOutliers(const TArray<T>& DataSet, TArray<int32>& OutlierIndices);

	static void RemoveOutliers(TArray<FVector>& VectorSet, bool bRecursive = true);

	static void CollectSampledViewSegments(const TArray<FMRAlignmentSample>& AlignmentPoints, TArray<FMRSampleSegment>& DepthSegmentsOut);
}

static FVector MRCalibrationUtilLibrary_Impl::FindAvgVector(const TArray<FVector>& VectorSet)
{
	FVector AvgVec = FVector::ZeroVector;
	for (const FVector& Vec : VectorSet)
	{
		AvgVec += Vec;
	}
	if (VectorSet.Num() > 0)
	{
		AvgVec /= VectorSet.Num();
	}
	return AvgVec;
}

static void MRCalibrationUtilLibrary_Impl::ComputeDivergenceField(const TArray<FVector>& VectorSet, TArray<float>& Out)
{
	Out.Empty(VectorSet.Num());
	FVector AvgVec = FindAvgVector(VectorSet);

	for (const FVector& Vec : VectorSet)
	{
		Out.Add(FVector::DistSquared(Vec, AvgVec));
	}
}

template<typename T>
static void MRCalibrationUtilLibrary_Impl::FindOutliers(const TArray<T>& DataSet, TArray<int32>& OutlierIndices)
{
	if (DataSet.Num() <= 1)
	{
		return;
	}

	TArray<int32> IndexBuffer;
	IndexBuffer.Reserve(DataSet.Num());

	for (int32 SampleIndex = 0; SampleIndex < DataSet.Num(); ++SampleIndex)
	{
		IndexBuffer.Add(SampleIndex);
	}

	IndexBuffer.Sort([&DataSet](const int32& A, const int32& B) {
		return DataSet[A] < DataSet[B]; // sort by divergence (smallest to largest)
	});

	auto GetValue = [&DataSet, &IndexBuffer](const int32 IndiceIndex)->T
	{
		return DataSet[IndexBuffer[IndiceIndex]];
	};

	const int32 IsEven = 1 - (IndexBuffer.Num() % 2);
	const int32 FirstHalfEnd = (IndexBuffer.Num() / 2) - IsEven;
	const int32 SecondHalfStart = FirstHalfEnd + IsEven;
	const int32 SecondHalfEnd = IndexBuffer.Num() - 1;

	auto ComputeMedian = [&GetValue](const int32 StartIndx, const int32 LastIndx)->T
	{
		const int32 ValCount = (LastIndx - StartIndx) + 1;
		const int32 MedianIndex = StartIndx + (ValCount / 2);

		T MedianVal = GetValue(MedianIndex);
		if (ValCount % 2 == 0)
		{
			MedianVal = (MedianVal + GetValue(MedianIndex - 1)) / 2.f;
		}
		return MedianVal;
	};

	// compute the 1st and 3rd quartile
	const T Q1 = ComputeMedian(0, FirstHalfEnd);
	const T Q3 = ComputeMedian(SecondHalfStart, SecondHalfEnd);
	// compute the interquartile range
	const T IQR = Q3 - Q1;

	const T UpperLimit = Q3 + 1.5 * IQR;
	const T LowerLimit = Q1 - 1.5 * IQR;

	// ensures outlier indices are in ascending order (expected)
	for (int32 SampleIndex = 0; SampleIndex < DataSet.Num(); ++SampleIndex)
	{
		const T& SampleValue = DataSet[SampleIndex];
		if (SampleValue < LowerLimit)
		{
			OutlierIndices.AddUnique(SampleIndex);
		}
		else if (SampleValue > UpperLimit)
		{
			OutlierIndices.AddUnique(SampleIndex);
		}
	}
}

static void MRCalibrationUtilLibrary_Impl::RemoveOutliers(TArray<FVector>& VectorSet, bool bRecursive)
{
	TArray<float> Divergences;
	MRCalibrationUtilLibrary_Impl::ComputeDivergenceField(VectorSet, Divergences);

	TArray<int32> OutlierIndices;
	MRCalibrationUtilLibrary_Impl::FindOutliers(Divergences, OutlierIndices);

	if (OutlierIndices.Num() > 0)
	{
		for (int32 IndiceIndex = OutlierIndices.Num() - 1; IndiceIndex >= 0; --IndiceIndex)
		{
			const int32 OutlierIndex = OutlierIndices[IndiceIndex];
			VectorSet.RemoveAtSwap(OutlierIndex);
		}

		if (bRecursive)
		{
			RemoveOutliers(VectorSet, /*bRecursive =*/true);
		}
	}
}

static void MRCalibrationUtilLibrary_Impl::CollectSampledViewSegments(const TArray<FMRAlignmentSample>& AlignmentPoints, TArray<FMRSampleSegment>& DepthSegmentsOut)
{
	for (int32 SampleIndex = 0; SampleIndex < AlignmentPoints.Num(); ++SampleIndex)
	{
		const FVector SamplePoint = AlignmentPoints[SampleIndex].GetAdjustedSamplePoint();
		const FIntVector& SamplePlanarId = AlignmentPoints[SampleIndex].PlanarId;

		for (int32 SubSampleIndex = SampleIndex + 1; SubSampleIndex < AlignmentPoints.Num(); ++SubSampleIndex)
		{
			const FIntVector& SubSamplePlanarId = AlignmentPoints[SubSampleIndex].PlanarId;
			if (SamplePlanarId[0] == SubSamplePlanarId[0])
			{
				// on the same depth plane, cannot form a frustum ray
				continue;
			}

			// if these two points would align in screen space
			if (SamplePlanarId[1] == SubSamplePlanarId[1] && SamplePlanarId[2] == SubSamplePlanarId[2])
			{
				FMRSampleSegment SampleSegment;
				SampleSegment.PointA   = SamplePoint;
				SampleSegment.PointB   = AlignmentPoints[SubSampleIndex].GetAdjustedSamplePoint();
				SampleSegment.PlanarId = FIntPoint(SamplePlanarId[1], SamplePlanarId[2]);

				DepthSegmentsOut.Add(SampleSegment);
			}
		}
	}
}

/* FMRAlignmentSample 
 *****************************************************************************/

FMRAlignmentSample::FMRAlignmentSample() 
	: PlanarId(INDEX_NONE) 
{}

FVector FMRAlignmentSample::GetAdjustedSamplePoint() const
{
	return SampledWorldPosition + SampleAdjustmentOffset;
}

FVector FMRAlignmentSample::GetTargetPositionInWorldSpace(const FVector& ViewOrigin, const FRotator& ViewOrientation) const
{
	return ViewOrigin + ViewOrientation.RotateVector(RelativeTargetPosition);
}

FVector FMRAlignmentSample::GetAdjustedTargetPositionInWorldSpace(const FVector& ViewOrigin, const FRotator& ViewOrientation) const
{
	return ViewOrigin + ViewOrientation.RotateVector(RelativeTargetPosition + TargetAdjustmentOffset);
}

/* UMRCalibrationUtilLibrary 
 *****************************************************************************/

UMRCalibrationUtilLibrary::UMRCalibrationUtilLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMRCalibrationUtilLibrary::FindOutliers(const TArray<float>& DataSet, TArray<int32>& OutlierIndices)
{
	if (DataSet.Num() > 1)
	{
		MRCalibrationUtilLibrary_Impl::FindOutliers(DataSet, OutlierIndices);
	}
}

int32 UMRCalibrationUtilLibrary::FindBestAnchorPoint(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, const FRotator& ViewOrientation)
{
	int32 BestAlignmentPoint = INDEX_NONE;
	if (AlignmentPoints.Num() <= 2)
	{
		// if there's only two points, the divergence at both will be the same...
		// arbitrarily, go with the newest
		BestAlignmentPoint = AlignmentPoints.Num() - 1;
	}
	else
	{
		float BestAvgDivergence = MAX_flt;
		for (int32 OriginIndex = 0; OriginIndex < AlignmentPoints.Num(); ++OriginIndex)
		{
			const FMRAlignmentSample& PerspectiveAncor = AlignmentPoints[OriginIndex];
			const FVector AlignmentOffset = PerspectiveAncor.GetAdjustedSamplePoint() - PerspectiveAncor.GetTargetPositionInWorldSpace(ViewOrigin, ViewOrientation);
			const FVector OffsetOrigin = ViewOrigin + AlignmentOffset;

			float AvgDivergence = 0.f;
			for (int32 PointIndex = 0; PointIndex < AlignmentPoints.Num(); ++PointIndex)
			{
				if (PointIndex != OriginIndex)
				{
					const FMRAlignmentSample& OtherPoint = AlignmentPoints[PointIndex];

					const FVector ToTarget = OtherPoint.GetTargetPositionInWorldSpace(ViewOrigin, ViewOrientation) - OffsetOrigin;
					const FVector ToSample = OtherPoint.GetAdjustedSamplePoint() - OffsetOrigin;

					AvgDivergence += FVector::Distance(ToSample, ToTarget);
				}
			}
			AvgDivergence /= AlignmentPoints.Num() - 1;

			if (AvgDivergence < BestAvgDivergence)
			{
				BestAlignmentPoint = OriginIndex;
				BestAvgDivergence = AvgDivergence;
			}
		}
	}

	return BestAlignmentPoint;
}

void UMRCalibrationUtilLibrary::FindBalancingAlignmentOffset(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, const FRotator& ViewOrientation, bool bOmitOutliers, FVector& BalancingOffset)
{
	BalancingOffset = FVector::ZeroVector;

	if (AlignmentPoints.Num() == 1)
	{
		const FVector SamplePt = AlignmentPoints[0].GetAdjustedSamplePoint();
		const FVector TargetPt = AlignmentPoints[0].GetTargetPositionInWorldSpace(ViewOrigin, ViewOrientation);

		// the offset it'll take to align the TargetPt with SamplePt
		BalancingOffset = SamplePt - TargetPt;
	}
	else if (AlignmentPoints.Num() > 0)
	{
		FVector MaxDivergences(-MAX_flt);
		FVector MinDivergences( MAX_flt);

		if (bOmitOutliers)
		{
			const int32 AnchorPtIndex = FindBestAnchorPoint(AlignmentPoints, ViewOrigin, ViewOrientation);
			const FMRAlignmentSample& AnchorPt = AlignmentPoints[AnchorPtIndex];
			const FVector AnchorOffset = AnchorPt.GetAdjustedSamplePoint() - AnchorPt.GetTargetPositionInWorldSpace(ViewOrigin, ViewOrientation);
			// want to align with an anchor first, to minimize divergences (else some points might get inadvertently rejected as outliers)
			BalancingOffset = AnchorOffset;

			const FVector AnchoredOrigin = ViewOrigin + AnchorOffset;

			TArray<float> XYZDivergences[3];
			for (const FMRAlignmentSample& AlignmentPt : AlignmentPoints)
			{
				const FVector DivergenceVec = AlignmentPt.GetAdjustedSamplePoint() - AlignmentPt.GetTargetPositionInWorldSpace(AnchoredOrigin, ViewOrientation);
				for (int32 CoordIndex = 0; CoordIndex < 3; ++CoordIndex)
				{
					XYZDivergences[CoordIndex].Add(DivergenceVec[CoordIndex]);
				}
			}

			for (int32 CoordIndex = 0; CoordIndex < 3; ++CoordIndex)
			{
				TArray<float>& CoordDivergences = XYZDivergences[CoordIndex];
				CoordDivergences.Sort();

				TArray<int32> OutlierIndices;
				MRCalibrationUtilLibrary_Impl::FindOutliers(CoordDivergences, OutlierIndices);

				for (int32 IndiceIndex = OutlierIndices.Num() - 1; IndiceIndex >= 0; --IndiceIndex)
				{
					CoordDivergences.RemoveAtSwap(OutlierIndices[IndiceIndex]);
				}
				if (ensure(CoordDivergences.Num() > 0))
				{
					if (CoordDivergences.Last() > MaxDivergences[CoordIndex])
					{
						MaxDivergences[CoordIndex] = CoordDivergences.Last();
					}
					if (CoordDivergences[0] < MinDivergences[CoordIndex])
					{
						MinDivergences[CoordIndex] = CoordDivergences[0];
					}
				}
			}
		}
		else // if !bOmitOutliers
		{
			for (const FMRAlignmentSample& AlignmentPt : AlignmentPoints)
			{
				const FVector DivergenceVec = AlignmentPt.GetAdjustedSamplePoint() - AlignmentPt.GetTargetPositionInWorldSpace(ViewOrigin, ViewOrientation);

				for (int32 CoordIndex = 0; CoordIndex < 3; ++CoordIndex)
				{
					if (DivergenceVec[CoordIndex] > MaxDivergences[CoordIndex])
					{
						MaxDivergences[CoordIndex] = DivergenceVec[CoordIndex];
					}
					if (DivergenceVec[CoordIndex] < MinDivergences[CoordIndex])
					{
						MinDivergences[CoordIndex] = DivergenceVec[CoordIndex];
					}
				}
			}
		}

		BalancingOffset += (MaxDivergences + MinDivergences) / 2.0f;
	}
}

void UMRCalibrationUtilLibrary::CalculateAlignmentNormals(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, TArray<FVector>& PlanarNormals, const bool bOmitOutliers)
{
	PlanarNormals.Empty(AlignmentPoints.Num());

	for (int32 PointIndex = 0; PointIndex < AlignmentPoints.Num(); ++PointIndex)
	{
		const FMRAlignmentSample& RootPt = AlignmentPoints[PointIndex];
		const FVector PlanarOrigin = RootPt.GetAdjustedSamplePoint();

		// NOTE: this assumes the ViewOrigin is already in the correct half-space
		const FVector ToOrigin = ViewOrigin - PlanarOrigin;

		for (int32 PtIndexA = PointIndex + 1; PtIndexA < AlignmentPoints.Num(); ++PtIndexA)
		{
			const FMRAlignmentSample& PtA = AlignmentPoints[PtIndexA];
			if (PtA.PlanarId[0] != RootPt.PlanarId[0])
			{
				continue;
			}
			const FVector ToPtA = PtA.GetAdjustedSamplePoint() - PlanarOrigin;

			for (int32 PtIndexB = PtIndexA + 1; PtIndexB < AlignmentPoints.Num(); ++PtIndexB)
			{
				const FMRAlignmentSample& PtB = AlignmentPoints[PtIndexB];
				if (PtB.PlanarId[0] != RootPt.PlanarId[0])
				{
					continue;
				}
				const FVector ToPtB = PtB.GetAdjustedSamplePoint() - PlanarOrigin;

				FVector PlaneNormal = FVector::CrossProduct(ToPtA, ToPtB);
				PlaneNormal.Normalize();

				// half-space test => if the normal we computed is facing back to the origin...
				if (FVector::DotProduct(PlaneNormal, ToOrigin) >= 0)
				{
					// flip it (equivalent to crossing the other way)
					PlaneNormal = -PlaneNormal;
				}

				PlanarNormals.Add(PlaneNormal);
			}
		}
	}

	if (bOmitOutliers && PlanarNormals.Num() > 0)
	{
		TArray<float> Divergences;
		MRCalibrationUtilLibrary_Impl::ComputeDivergenceField(PlanarNormals, Divergences);

		TArray<int32> OutlierIndices;
		MRCalibrationUtilLibrary_Impl::FindOutliers(Divergences, OutlierIndices);

		OutlierIndices.Sort([](const int32& A, const int32& B) {
			return A > B; // reverse sort (largest index to smallest) so we can RemoveAtSwap()
		});

		for (int32& NormalIndex : OutlierIndices)
		{
			PlanarNormals.RemoveAtSwap(NormalIndex);
		}
	}
}

bool UMRCalibrationUtilLibrary::FindAverageLookAtDirection(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, FVector& LookAtDir, const bool bOmitOutliers)
{
	TArray<FVector> ProspectiveNormals;
	CalculateAlignmentNormals(AlignmentPoints, ViewOrigin, ProspectiveNormals, bOmitOutliers);

	const bool bComputedNormals = ProspectiveNormals.Num() > 0;
	if (bComputedNormals)
	{
		LookAtDir = MRCalibrationUtilLibrary_Impl::FindAvgVector(ProspectiveNormals);
	}
	return bComputedNormals;
}

bool UMRCalibrationUtilLibrary::FindAverageUpDirection(const TArray<FMRAlignmentSample>& AlignmentPoints, FVector& AvgUpDir, const bool bOmitOutliers)
{
	TArray<FVector> PlanarUpVecs;

	for (int32 PtIndexA = 0; PtIndexA < AlignmentPoints.Num(); ++PtIndexA)
	{
		const FMRAlignmentSample& PtA = AlignmentPoints[PtIndexA];
		for (int32 PtIndexB = PtIndexA + 1; PtIndexB < AlignmentPoints.Num(); ++PtIndexB)
		{
			const FMRAlignmentSample& PtB = AlignmentPoints[PtIndexB];
			// x & y (depth & left/right) planes have to be the same; vertical (z) planes have to be different
			if (PtB.PlanarId[0] != PtA.PlanarId[0] || PtB.PlanarId[1] != PtA.PlanarId[1] || PtB.PlanarId[2] == PtA.PlanarId[2])
			{
				continue;
			}

			const FVector BottomPt = (PtB.PlanarId[2] < PtA.PlanarId[2]) ? PtB.GetAdjustedSamplePoint() : PtA.GetAdjustedSamplePoint();
			const FVector TopPt    = (PtB.PlanarId[2] > PtA.PlanarId[2]) ? PtB.GetAdjustedSamplePoint() : PtA.GetAdjustedSamplePoint();
			
			FVector ApproxUpVec = TopPt - BottomPt;
			ApproxUpVec.Normalize();

			PlanarUpVecs.Add(ApproxUpVec);
		}
	}

	if (bOmitOutliers && PlanarUpVecs.Num() > 0)
	{
		TArray<float> Divergences;
		MRCalibrationUtilLibrary_Impl::ComputeDivergenceField(PlanarUpVecs, Divergences);

		TArray<int32> OutlierIndices;
		MRCalibrationUtilLibrary_Impl::FindOutliers(Divergences, OutlierIndices);

		for (int32 IndiceIndex = OutlierIndices.Num() - 1; IndiceIndex >= 0; --IndiceIndex)
		{
			PlanarUpVecs.RemoveAtSwap(OutlierIndices[IndiceIndex]);
		}
	}

	AvgUpDir = MRCalibrationUtilLibrary_Impl::FindAvgVector(PlanarUpVecs);
	return PlanarUpVecs.Num() > 0;
}

void UMRCalibrationUtilLibrary::GetCommandKeyStates(UObject* WorldContextObj, bool& bIsCtlDown, bool& bIsAltDown, bool& bIsShiftDown)
{
	bIsCtlDown = bIsAltDown = bIsShiftDown = false;

	UWorld* World = WorldContextObj->GetWorld();
	if (World && World->IsGameWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			if (FSceneViewport* GameViewport = ViewportClient->GetGameViewport())
			{
				bIsCtlDown = GameViewport->KeyState(EKeys::LeftControl) || GameViewport->KeyState(EKeys::RightControl);
				bIsAltDown = GameViewport->KeyState(EKeys::LeftAlt) || GameViewport->KeyState(EKeys::RightAlt);
				bIsShiftDown = GameViewport->KeyState(EKeys::LeftShift) || GameViewport->KeyState(EKeys::RightShift);
			}
		}
	}
}

UActorComponent* UMRCalibrationUtilLibrary::AddComponentFromClass(AActor* Owner, TSubclassOf<UActorComponent> ComponentClass, FName ComponentName, bool bManualAttachment)
{
	UActorComponent* NewComponent = nullptr;
	if (Owner && ComponentClass)
	{
		const FName UniqueComponentName = MakeUniqueObjectName(Owner, ComponentClass, ComponentName);
		NewComponent = NewObject<UActorComponent>(Owner, ComponentClass, UniqueComponentName, RF_Transient | RF_TextExportTransient);
		Owner->AddOwnedComponent(NewComponent);

		NewComponent->OnComponentCreated();

		if (!bManualAttachment)
		{
			if (USceneComponent* AsSceneComponent = Cast<USceneComponent>(NewComponent))
			{
				if (USceneComponent* Root = Owner->GetRootComponent())
				{
					AsSceneComponent->SetupAttachment(Root);
				}
				else
				{
					Owner->SetRootComponent(AsSceneComponent);
				}
			}
		}

		if (NewComponent->bAutoRegister)
		{
			NewComponent->RegisterComponent();
		}
	}
	return NewComponent;
}

bool UMRCalibrationUtilLibrary::ClassImplementsInterface(UClass* ObjectClass, TSubclassOf<UInterface> InterfaceClass)
{
	return ObjectClass && InterfaceClass && ObjectClass->ImplementsInterface(InterfaceClass);
}

FRotator UMRCalibrationUtilLibrary::FindAverageViewOrientation(const TArray<FMRAlignmentSample>& AlignmentPoints, bool bOmitOutliers)
{
// 	TArray<FQuat> SampleOrientations;
// 	SampleOrientations.Reserve(AlignmentPoints.Num());

	TArray<FVector> LookAtVectors, UpVectors;
	LookAtVectors.Reserve(AlignmentPoints.Num());
	UpVectors.Reserve(AlignmentPoints.Num());

	for (const FMRAlignmentSample& Sample : AlignmentPoints)
	{
// 		const FQuat AsQuat = Sample.AdjustedViewOrientation.Quaternion();
// 		float Bias = 1.0f;
// 		if (SampleOrientations.Num() > 0)
// 		{
// 			const float DotResult = (AsQuat | SampleOrientations[0]);
// 			Bias = FMath::FloatSelect(DotResult, 1.0f, -1.0f);
// 		}
// 		SampleOrientations.Add(AsQuat * Bias);

		LookAtVectors.AddZeroed();
		UpVectors.AddZeroed();

		FVector YAxis;
		FRotationMatrix(Sample.AdjustedViewOrientation).GetScaledAxes(LookAtVectors.Last(), YAxis, UpVectors.Last());
	}

	if (bOmitOutliers)
	{
		auto RemoveAxisOuliers = [&LookAtVectors, &UpVectors](const TArray<FVector>& VectorSet)
		{
			TArray<float> Divergences;
			MRCalibrationUtilLibrary_Impl::ComputeDivergenceField(VectorSet, Divergences);

			ensure(VectorSet.Num() == Divergences.Num());

			TArray<int32> OutlierIndices;
			MRCalibrationUtilLibrary_Impl::FindOutliers(Divergences, OutlierIndices);

			for (int32 IndiceIndex = OutlierIndices.Num() - 1; IndiceIndex >= 0; --IndiceIndex)
			{
				const int32 OutlierIndex = OutlierIndices[IndiceIndex];
				UpVectors.RemoveAtSwap(OutlierIndex);
				LookAtVectors.RemoveAtSwap(OutlierIndex);
			}
		};

		RemoveAxisOuliers(LookAtVectors);
		RemoveAxisOuliers(UpVectors);
	}

	const FVector AvgXAxis = MRCalibrationUtilLibrary_Impl::FindAvgVector(LookAtVectors);
	const FVector AvgZAxis = MRCalibrationUtilLibrary_Impl::FindAvgVector(UpVectors);

	return FRotationMatrix::MakeFromXZ(AvgXAxis, AvgZAxis).Rotator();

// 	FRotator AverageRotator(0.f);
// 	if (SampleOrientations.Num() == 1)
// 	{
// 		AverageRotator = SampleOrientations[0].Rotator();
// 	}
// 	else if (SampleOrientations.Num() == 2)
// 	{
// 		const FQuat& RotA = SampleOrientations[0];
// 		const FQuat& RotB = SampleOrientations[1];
// 		const FQuat AverageQuat = FQuat::Slerp(RotA, RotB, 0.5f);
// 
// 		AverageRotator = AverageQuat.Rotator();
// 	}
// 	else if (SampleOrientations.Num() > 0)
// 	{
// 		bool bAverageFound = false;
// 		if (!bFastPath)
// 		{
// #if WITH_OPENCV
// 			cv::Mat CvQuatMatrix(4, SampleOrientations.Num(), CV_32FC1);
// 
// 			const float SampleWeight = 1.f / SampleOrientations.Num();
// 			for (int32 SampleIndex = 0; SampleIndex < SampleOrientations.Num(); ++SampleIndex)
// 			{
// 				const FQuat WeightedRot = SampleOrientations[SampleIndex] * SampleWeight;
// 				CvQuatMatrix.at<float>(/*row =*/0, /*col =*/SampleIndex) = WeightedRot.X;
// 				CvQuatMatrix.at<float>(/*row =*/1, /*col =*/SampleIndex) = WeightedRot.Y;
// 				CvQuatMatrix.at<float>(/*row =*/2, /*col =*/SampleIndex) = WeightedRot.Z;
// 				CvQuatMatrix.at<float>(/*row =*/3, /*col =*/SampleIndex) = WeightedRot.W;
// 			}
// 			cv::Mat CvQuatMat_Tranposed = CvQuatMatrix.t();
// 			cv::Mat SymetricQuatMatrix = CvQuatMatrix * CvQuatMat_Tranposed;
// 
// 			cv::Mat EigenVals, EigenVectors;
// 			if (cv::eigen(SymetricQuatMatrix, EigenVals, EigenVectors))
// 			{
// 				FQuat AvgQuat(
// 					EigenVectors.at<float>(0, 0),
// 					EigenVectors.at<float>(1, 0),
// 					EigenVectors.at<float>(2, 0),
// 					EigenVectors.at<float>(3, 0)
// 				);
// 				AvgQuat.Normalize();
// 				
// 				bAverageFound  = true;
// 				AverageRotator = AvgQuat.Rotator();
// 			}
// #endif
// 		}
// 
// 		if (!bAverageFound)
// 		{
// 			FQuat AverageOrientation = SampleOrientations[0];
// 			for (int32 SampleIndex = 1; SampleIndex < SampleOrientations.Num(); ++SampleIndex)
// 			{
// 				AverageOrientation += SampleOrientations[SampleIndex];
// 			}
// 			// NOTE: this method of averaging only works if the quaternions are "close"
// 			AverageOrientation /= SampleOrientations.Num();
// 
// 			AverageRotator = AverageOrientation.Rotator();
// 		}
// 	}
//
//	return AverageRotator;
}

bool UMRCalibrationUtilLibrary::CalcConvergingViewSegments(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, const FRotator& ViewOrientation, TArray<FVector>& SegmentPoints)
{
	TArray<FMRSampleSegment> FrustumSegments;
	MRCalibrationUtilLibrary_Impl::CollectSampledViewSegments(AlignmentPoints, FrustumSegments);

	SegmentPoints.Empty(FrustumSegments.Num() * 2.f);

	for (FMRSampleSegment& Segment : FrustumSegments)
	{
		Segment.Orient(ViewOrigin, ViewOrientation);

		const FVector RayOrigin = Segment.PointB;
		FVector SegmentEnd = Segment.PointA;
		const FVector RayDirection = (SegmentEnd - RayOrigin).GetSafeNormal();
		const FVector ViewYAxis = FRotationMatrix(ViewOrientation).GetUnitAxis(EAxis::Type::Y);

		const float IntersectDenom = (ViewYAxis | RayDirection);
		if (FMath::Abs(IntersectDenom) > SMALL_NUMBER)
		{
			const float IntersectT = ((ViewOrigin - RayOrigin) | ViewYAxis) / IntersectDenom;
			SegmentEnd = RayOrigin + (2.0f * IntersectT * RayDirection);
		}

		SegmentPoints.Add(RayOrigin);
		SegmentPoints.Add(SegmentEnd);
	}

	return SegmentPoints.Num() > 0;
}

bool UMRCalibrationUtilLibrary::FindLookAtSamples(const TArray<FMRAlignmentSample>& AlignmentPoints, TArray<FVector>& SampledLookAtVectors)
{
	auto IsCenterPoint = [](const FVector& ViewPt)
	{
		return FMath::IsNearlyZero(ViewPt.Y) && FMath::IsNearlyZero(ViewPt.Z);
	};

	for (int32 SampleIndex = 0; SampleIndex < AlignmentPoints.Num(); ++SampleIndex)
	{
		const FMRAlignmentSample& Sample = AlignmentPoints[SampleIndex];
		if (IsCenterPoint(Sample.RelativeTargetPosition))
		{
			const FVector SamplePt = Sample.GetAdjustedSamplePoint();

			for (int32 SubSampleIndex = SampleIndex + 1; SubSampleIndex < AlignmentPoints.Num(); ++SubSampleIndex)
			{
				const FMRAlignmentSample& SubSample = AlignmentPoints[SubSampleIndex];
				if (!IsCenterPoint(SubSample.RelativeTargetPosition))
				{
					continue;
				}
				const FVector SubSamplePt = SubSample.GetAdjustedSamplePoint();

				const FVector& FarPt  = (Sample.RelativeTargetPosition.X > SubSample.RelativeTargetPosition.X) ? SamplePt : SubSamplePt;
				const FVector& NearPt = (Sample.RelativeTargetPosition.X > SubSample.RelativeTargetPosition.X) ? SubSamplePt : SamplePt;

				SampledLookAtVectors.Add( (FarPt - NearPt).GetSafeNormal() );
			}
		}
	}

	return SampledLookAtVectors.Num() > 0;
}

bool UMRCalibrationUtilLibrary::BisectConvergingViewSegmants(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, const FRotator& ViewOrientation, TArray<FVector>& Bisections, const bool bOmitOutliers)
{
	TArray<FMRSampleSegment> FrustumSegments;
	MRCalibrationUtilLibrary_Impl::CollectSampledViewSegments(AlignmentPoints, FrustumSegments);

	for (int32 SegIndex = 0; SegIndex < FrustumSegments.Num(); ++SegIndex)
	{
		FMRSampleSegment& SegmentA = FrustumSegments[SegIndex];
		const int32 YPlanarId = SegmentA.PlanarId(0);
		const int32 ZPlanarId = SegmentA.PlanarId(1);

		SegmentA.Orient(ViewOrigin, ViewOrientation);
		const FVector SegAVec = (SegmentA.PointB - SegmentA.PointA).GetSafeNormal();

		for (int32 SubSegIndex = SegIndex + 1; SubSegIndex < FrustumSegments.Num(); ++SubSegIndex)
		{
			FMRSampleSegment& SegmentB = FrustumSegments[SubSegIndex];
			if (YPlanarId == -SegmentB.PlanarId(0) && ZPlanarId == -SegmentB.PlanarId(1))
			{
				SegmentB.Orient(ViewOrigin, ViewOrientation);
				const FVector SegBVec = (SegmentB.PointB - SegmentB.PointA).GetSafeNormal();

				const FVector BisectingVec = (SegAVec + SegBVec) / 2.0f;
				Bisections.Add(BisectingVec);
			}
		}
	}

	FindLookAtSamples(AlignmentPoints, Bisections);

	if (bOmitOutliers)
	{
		MRCalibrationUtilLibrary_Impl::RemoveOutliers(Bisections);
	}
	return Bisections.Num() > 0;
}

bool UMRCalibrationUtilLibrary::FindAverageLookAtBisection(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& ViewOrigin, const FRotator& ViewOrientation, FVector& AvgBisection, const bool bOmitOutliers)
{
	TArray<FVector> Bisections;
	BisectConvergingViewSegmants(AlignmentPoints, ViewOrigin, ViewOrientation, Bisections, bOmitOutliers);

	AvgBisection = MRCalibrationUtilLibrary_Impl::FindAvgVector(Bisections);
	return Bisections.Num() > 0;
}

bool UMRCalibrationUtilLibrary::EstimatedViewOriginPointCloud(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& CurrentViewOrigin, const FRotator& CurrentViewOrientation, TArray<FVector>& OriginPointCloud)
{
	TArray<FVector> SegmentPoints;
	CalcConvergingViewSegments(AlignmentPoints, CurrentViewOrigin, CurrentViewOrientation, SegmentPoints);

	OriginPointCloud.Empty();

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentPoints.Num(); SegmentIndex += 2)
	{
		const FVector& SegStartA = SegmentPoints[SegmentIndex];
		const FVector& SegEndA   = SegmentPoints[SegmentIndex + 1];

		for (int32 SubSegIndex = SegmentIndex + 2; SubSegIndex < SegmentPoints.Num(); SubSegIndex += 2)
		{
			const FVector& SegStartB = SegmentPoints[SubSegIndex];
			const FVector& SegEndB   = SegmentPoints[SubSegIndex + 1];

			FVector NearestPtA, NearestPtB;
			FMath::SegmentDistToSegmentSafe(SegStartA, SegEndA, SegStartB, SegEndB, NearestPtA, NearestPtB);

			OriginPointCloud.Add(NearestPtA);
			OriginPointCloud.Add(NearestPtB);
		}
	}

	return OriginPointCloud.Num() > 0;
}

FVector UMRCalibrationUtilLibrary::EstimateNewViewOrigin(const TArray<FMRAlignmentSample>& AlignmentPoints, const FVector& CurrentViewOrigin, const FRotator& CurrentViewOrientation)
{
	FVector NewViewOrigin = CurrentViewOrigin;

	TArray<FVector> OriginPointCloud;
	if (EstimatedViewOriginPointCloud(AlignmentPoints, CurrentViewOrigin, CurrentViewOrientation, OriginPointCloud))
	{
		MRCalibrationUtilLibrary_Impl::RemoveOutliers(OriginPointCloud);
		NewViewOrigin = MRCalibrationUtilLibrary_Impl::FindAvgVector(OriginPointCloud);
	}
	return NewViewOrigin;
}

bool UMRCalibrationUtilLibrary::GetConfigValueString(const FString& FieldName, FString& StringValue)
{
	return GConfig->GetString(TEXT("MRCalibration"), *FieldName, StringValue, GEngineIni);
}

FString UMRCalibrationUtilLibrary::GetTempSaveDirectory()
{
	return FPaths::ProjectSavedDir() + TEXT("Tmp/MRCalibration/");
}

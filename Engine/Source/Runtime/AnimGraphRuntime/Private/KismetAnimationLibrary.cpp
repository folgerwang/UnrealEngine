// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "KismetAnimationLibrary.h"
#include "AnimationCoreLibrary.h"
#include "AnimationCoreLibrary.h"
#include "Blueprint/BlueprintSupport.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

#define LOCTEXT_NAMESPACE "UKismetAnimationLibrary"

//////////////////////////////////////////////////////////////////////////
// UKismetAnimationLibrary

const FName AnimationLibraryWarning = FName("Animation Library");

UKismetAnimationLibrary::UKismetAnimationLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration(
			AnimationLibraryWarning,
			LOCTEXT("AnimationLibraryWarning", "Animation Library Warning")
		)
	);
}

void UKismetAnimationLibrary::K2_TwoBoneIK(const FVector& RootPos, const FVector& JointPos, const FVector& EndPos, const FVector& JointTarget, const FVector& Effector, FVector& OutJointPos, FVector& OutEndPos, bool bAllowStretching, float StartStretchRatio, float MaxStretchScale)
{
	AnimationCore::SolveTwoBoneIK(RootPos, JointPos, EndPos, JointTarget, Effector, OutJointPos, OutEndPos, bAllowStretching, StartStretchRatio, MaxStretchScale);
}

FTransform UKismetAnimationLibrary::K2_LookAt(const FTransform& CurrentTransform, const FVector& TargetPosition, FVector AimVector, bool bUseUpVector, FVector UpVector, float ClampConeInDegree)
{
	if (AimVector.IsNearlyZero())
	{
		// aim vector should be normalized
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("AimVector should not be zero. Please specify which direction.")), ELogVerbosity::Warning, AnimationLibraryWarning);
		return FTransform::Identity;
	}

	if (bUseUpVector && UpVector.IsNearlyZero())
	{
		// upvector has to be normalized
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("LookUpVector should not be zero. Please specify which direction.")), ELogVerbosity::Warning, AnimationLibraryWarning);
		bUseUpVector = false;
	}

	if (ClampConeInDegree < 0.f || ClampConeInDegree > 180.f)
	{
		// ClampCone is out of range, it will be clamped to (0.f, 180.f)
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("ClampConeInDegree should range from (0, 180). ")), ELogVerbosity::Warning, AnimationLibraryWarning);
	}

	FQuat DiffRotation = AnimationCore::SolveAim(CurrentTransform, TargetPosition, AimVector.GetSafeNormal(), bUseUpVector, UpVector.GetSafeNormal(), ClampConeInDegree);
	FTransform NewTransform = CurrentTransform;
	NewTransform.SetRotation(DiffRotation);
	return NewTransform;
}

float UKismetAnimationLibrary::K2_DistanceBetweenTwoSocketsAndMapRange(const USkeletalMeshComponent* Component, const FName SocketOrBoneNameA, ERelativeTransformSpace SocketSpaceA, const FName SocketOrBoneNameB, ERelativeTransformSpace SocketSpaceB, bool bRemapRange, float InRangeMin, float InRangeMax, float OutRangeMin, float OutRangeMax)
{
	if (Component && SocketOrBoneNameA != NAME_None && SocketOrBoneNameB != NAME_None)
	{
		FTransform SocketTransformA = Component->GetSocketTransform(SocketOrBoneNameA, SocketSpaceA);
		FTransform SocketTransformB = Component->GetSocketTransform(SocketOrBoneNameB, SocketSpaceB);

		float Distance = (SocketTransformB.GetLocation() - SocketTransformA.GetLocation()).Size();

		if (bRemapRange)
		{
			return FMath::GetMappedRangeValueClamped(FVector2D(InRangeMin, InRangeMax), FVector2D(OutRangeMin, OutRangeMax), Distance);
		}
		else
		{
			return Distance;
		}
	}

	return 0.f;
}

FVector UKismetAnimationLibrary::K2_DirectionBetweenSockets(const USkeletalMeshComponent* Component, const FName SocketOrBoneNameFrom, const FName SocketOrBoneNameTo)
{
	if (Component && SocketOrBoneNameFrom != NAME_None && SocketOrBoneNameTo != NAME_None)
	{
		FTransform SocketTransformFrom = Component->GetSocketTransform(SocketOrBoneNameFrom, RTS_World);
		FTransform SocketTransformTo = Component->GetSocketTransform(SocketOrBoneNameTo, RTS_World);

		return (SocketTransformTo.GetLocation() - SocketTransformFrom.GetLocation());
	}

	return FVector(0.f);
}

FVector UKismetAnimationLibrary::K2_MakePerlinNoiseVectorAndRemap(float X, float Y, float Z, float RangeOutMinX, float RangeOutMaxX, float RangeOutMinY, float RangeOutMaxY, float RangeOutMinZ, float RangeOutMaxZ)
{
	FVector OutVector;
	OutVector.X = K2_MakePerlinNoiseAndRemap(X, RangeOutMinX, RangeOutMaxX);
	OutVector.Y = K2_MakePerlinNoiseAndRemap(Y, RangeOutMinY, RangeOutMaxY);
	OutVector.Z = K2_MakePerlinNoiseAndRemap(Z, RangeOutMinZ, RangeOutMaxZ);
	return OutVector;
}

float UKismetAnimationLibrary::K2_MakePerlinNoiseAndRemap(float Value, float RangeOutMin, float RangeOutMax)
{
	// perlin noise output is always from [-1, 1]
	return FMath::GetMappedRangeValueClamped(FVector2D(-1.f, 1.f), FVector2D(RangeOutMin, RangeOutMax), FMath::PerlinNoise1D(Value));
}
#undef LOCTEXT_NAMESPACE


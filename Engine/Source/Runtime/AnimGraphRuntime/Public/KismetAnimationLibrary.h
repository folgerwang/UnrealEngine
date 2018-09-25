// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SceneComponent.h"
#include "KismetAnimationLibrary.generated.h"

class USkeletalMeshComponent;

UCLASS(meta=(ScriptName="AnimGraphLibrary"))
class ANIMGRAPHRUNTIME_API UKismetAnimationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Two Bone IK Function", ScriptName = "TwoBoneIK", bAllowStretching = "false", StartStretchRatio = "1.0", MaxStretchScale = "1.2"))
	static void K2_TwoBoneIK(const FVector& RootPos, const FVector& JointPos, const FVector& EndPos, const FVector& JointTarget, const FVector& Effector, FVector& OutJointPos, FVector& OutEndPos, bool bAllowStretching, float StartStretchRatio, float MaxStretchScale);

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Look At Function", ScriptName = "LookAt", bUseUpVector = "false"))
	static FTransform K2_LookAt(const FTransform& CurrentTransform, const FVector& TargetPosition, FVector LookAtVector, bool bUseUpVector, FVector UpVector, float ClampConeInDegree);

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Get Distance Between Two Sockets", ScriptName = "DistanceBetweenSockets", bRemapRange= "false"))
	static float K2_DistanceBetweenTwoSocketsAndMapRange(const USkeletalMeshComponent* Component, const FName SocketOrBoneNameA, ERelativeTransformSpace SocketSpaceA, const FName SocketOrBoneNameB, ERelativeTransformSpace SocketSpaceB, bool bRemapRange, float InRangeMin, float InRangeMax, float OutRangeMin, float OutRangeMax);

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Get Direction Between Sockets", ScriptName = "DirectionBetweenSockets"))
	static FVector K2_DirectionBetweenSockets(const USkeletalMeshComponent* Component, const FName SocketOrBoneNameFrom, const FName SocketOrBoneNameTo);

	/* This function creates perlin noise from input X, Y, Z, and then range map to RangeOut, and out put to OutX, OutY, OutZ */
	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Make Perlin Noise Vector and Remap", ScriptName = "MakeVectorFromPerlinNoise", RangeOutMinX = "-1.f", RangeOutMaxX = "1.f", RangeOutMinY = "-1.f", RangeOutMaxY = "1.f", RangeOutMinZ = "-1.f", RangeOutMaxZ = "1.f"))
	static FVector K2_MakePerlinNoiseVectorAndRemap(float X, float Y, float Z, float RangeOutMinX, float RangeOutMaxX, float RangeOutMinY, float RangeOutMaxY, float RangeOutMinZ, float RangeOutMaxZ);

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Make Perlin Noise and Remap", ScriptName = "MakeFloatFromPerlinNoise", RangeOutMinX = "-1.f", RangeOutMaxX = "1.f", RangeOutMinY = "-1.f", RangeOutMaxY = "1.f", RangeOutMinZ = "-1.f", RangeOutMaxZ = "1.f"))
	static float K2_MakePerlinNoiseAndRemap(float Value, float RangeOutMin, float RangeOutMax);
};


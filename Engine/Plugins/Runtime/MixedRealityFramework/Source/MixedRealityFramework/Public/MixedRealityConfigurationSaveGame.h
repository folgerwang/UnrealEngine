// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/SaveGame.h"
#include "Math/Color.h" // for FLinearColor
#include "MixedRealityCaptureDevice.h" // for FMRCaptureDeviceIndex
#include "MixedRealityLensDistortion.h"
#include "MixedRealityConfigurationSaveGame.generated.h"

class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FMRLensCalibrationData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	float FOV = 90.f;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FMRLensDistortion DistortionParameters;
};

USTRUCT(BlueprintType)
struct FMRAlignmentSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FVector CameraOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FRotator Orientation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FName TrackingAttachmentId;
};

USTRUCT(BlueprintType)
struct FGarbageMatteSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FTransform Transform;
};

USTRUCT(BlueprintType)
struct FChromaKeyParams
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	FLinearColor ChromaColor = FLinearColor(0.122f, 0.765f, 0.261, 1.f);

	/*
	 * Colors matching the chroma color up to this tolerance level will be completely
	 * cut out. The higher the value the more that is cut out. A value of zero
	 * means that the chroma color has to be an exact match for the pixel to be
	 * completely transparent.
	 */
	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	float ChromaClipThreshold = 0.26f;

	/*
	 * Colors that differ from the chroma color beyond this tolerance level will
	 * be fully opaque. The higher the number, the more transparency gradient there
	 * will be along edges. This is expected to be greater than the 'Chroma Clip
	 * Threshold' param. If this matches the 'Chroma Clip Threshold' then there will
	 * be no transparency gradient (what isn't clipped will be fully opaque).
	 */
	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	float ChromaToleranceCap = 0.53f;

	/*
	 * An exponent param that governs how soft/hard the semi-translucent edges are.
	 * Larger numbers will cause the translucency to fall off faster, shrinking
	 * the silhouette and smoothing it out. Larger numbers can also be used to hide
	 * splotchy artifacts. Values under 1 will cause the transparent edges to
	 * increase in harshness (approaching on opaque).
	 */
	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	float EdgeSoftness = 10.f;

public:
	void ApplyToMaterial(UMaterialInstanceDynamic* Material) const;
};

USTRUCT(BlueprintType)
struct FMRCompositingSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FChromaKeyParams ChromaKeySettings;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FMRCaptureDeviceIndex CaptureDeviceURL;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	float DepthOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	int32 TrackingLatency = 0;
};

UCLASS(BlueprintType, Blueprintable, config = Engine)
class MIXEDREALITYFRAMEWORK_API UMixedRealityCalibrationData : public USaveGame
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	FMRLensCalibrationData LensData;

	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	FMRAlignmentSaveData AlignmentData;

	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	TArray<FGarbageMatteSaveData> GarbageMatteSaveDatas;

	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	FMRCompositingSaveData CompositingData;
};

/**
 * 
 */
UCLASS(BlueprintType, config = Engine)
class MIXEDREALITYFRAMEWORK_API UMixedRealityConfigurationSaveGame : public UMixedRealityCalibrationData
{
	GENERATED_UCLASS_BODY()

public:
	// Metadata about the save file

	UPROPERTY(BlueprintReadWrite, config, Category = SaveMetadata)
	FString SaveSlotName;

	UPROPERTY(BlueprintReadWrite, config, Category = SaveMetadata)
	int32 UserIndex;
	
	UPROPERTY(BlueprintReadOnly, Category = SaveMetadata)
	int32 ConfigurationSaveVersion;
};
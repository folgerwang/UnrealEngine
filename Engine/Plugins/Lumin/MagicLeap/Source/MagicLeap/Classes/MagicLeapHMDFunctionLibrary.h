// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapHMDFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EHeadTrackingError : uint8
{
	None,
	NotEnoughFeatures,
	LowLight,
	Unknown
};

UENUM(BlueprintType)
enum class EHeadTrackingMode : uint8
{
	PositionAndOrientation,
	OrientationOnly,
	Unknown
};

USTRUCT(BlueprintType)
struct FHeadTrackingState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicLeap")
	EHeadTrackingMode Mode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicLeap")
	EHeadTrackingError Error;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicLeap")
	float Confidence;
};

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAP_API UMagicLeapHMDFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Input|MagicLeap")
	static void SetBasePosition(const FVector& InBasePosition);

	UFUNCTION(BlueprintCallable, Category = "Input|MagicLeap")
	static void SetBaseOrientation(const FQuat& InBaseOrientation);

	UFUNCTION(BlueprintCallable, Category = "Input|MagicLeap")
	static void SetBaseRotation(const FRotator& InBaseRotation);

	/** Set the actor whose location is used as the focus point. The focus distance is the distance from the HMD to the focus point. */
	UFUNCTION(BlueprintCallable, Category = "Input|MagicLeap")
	static void SetFocusActor(const AActor* InFocusActor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MagicLeap")
	static int32 GetMLSDKVersionMajor();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MagicLeap")
	static int32 GetMLSDKVersionMinor();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MagicLeap")
	static int32 GetMLSDKVersionRevision();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MagicLeap")
	static FString GetMLSDKVersion();

	/** Returns true if this code is executing on the ML HMD, false otherwise (e.g. it's executing on PC) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MagicLeap")
	static bool IsRunningOnMagicLeapHMD();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MagicLeap")
	static bool GetHeadTrackingState(FHeadTrackingState& State);
};

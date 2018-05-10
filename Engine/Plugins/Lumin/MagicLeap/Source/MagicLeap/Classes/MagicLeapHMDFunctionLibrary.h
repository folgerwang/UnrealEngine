// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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

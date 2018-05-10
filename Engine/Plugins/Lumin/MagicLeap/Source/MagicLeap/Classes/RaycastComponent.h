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

#include "Components/ActorComponent.h"
#include "RaycastComponent.generated.h"

/** Parameters for a raycast request. */
USTRUCT(BlueprintType)
struct FRaycastQueryParams
{
	GENERATED_BODY()

public:
	/** Where the ray is cast from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap")
	FVector Position;

	/** Direction of the ray to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap")
	FVector Direction = FVector(1, 0, 0);

	/** Up vector of the ray to fire. This is used to orient the area the rays are cast over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap")
	FVector UpVector = FVector(0, 0, 1);

	/** The number of horizontal rays. For single point raycast, set this to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap", meta = (ClampMin = 1))
	int32 Width = 1;

	/** The number of vertical rays. For single point raycast, set this to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap", meta = (ClampMin = 1))
	int32 Height = 1;

	/** The angular width, in degrees, over which the horizonal rays are evenly distributed to create a raycast area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap")
	float HorizontalFovDegrees;

	/** If true, a ray will terminate when encountering an unobserved area and return a surface or
	  the ray will continue until it ends or hits an observed surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap")
	bool CollideWithUnobserved;

	/** User data for this request. The same data will be included in the result for query identification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raycast|MagicLeap")
	int32 UserData;
};

/** The states of a raycast result. */
UENUM(BlueprintType)
enum class ERaycastResultState : uint8
{
	/** The raycast request failed. */
	RequestFailed,
	/** The ray passed beyond maximum raycast distance and it doesn't hit any surface. */
	NoCollission,
	/** The ray hit an unobserved area. This can occur only when CollideWithUnobserved is set to true. */
	HitUnobserved,
	/** The ray hit an observed area. */
	HitObserved
};

/** Result of a raycast. */
USTRUCT(BlueprintType)
struct FRaycastHitResult
{
	GENERATED_BODY()

public:
	/** The raycast result. If this field is either RequestFailed or NoCollision,
	  most of the fields in this structure are invalid. */
	UPROPERTY(BlueprintReadOnly, Category = "Raycast|MagicLeap")
	ERaycastResultState HitState;

	/** Where in the world the collision happened. This field is only valid if the state
	  is either HitUnobserved or HitObserved. */
	UPROPERTY(BlueprintReadOnly, Category = "Raycast|MagicLeap")
	FVector HitPoint;

	/** Normal to the surface where the ray collided. This field is only valid if the state
	  is either HitUnobserved or HitObserved */
	UPROPERTY(BlueprintReadOnly, Category = "Raycast|MagicLeap")
	FVector Normal;

	/** Confidence of the raycast result.

	  Confidence is a non-negative value from 0 to 1 where closer to 1 indicates a higher quality.
	  It will be an indication of how much error there is in the averaging.
	  For example, a flat plane will have a high confidence, while if the surface was very noisy the confidence
	  would be very low. This field is only valid if the state is either HitUnobserved or HitObserved.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Raycast|MagicLeap")
	float Confidence;

	/** The data set in the RaycastQueryParams. This can be used for query identification. */
	UPROPERTY(BlueprintReadOnly, Category = "Raycast|MagicLeap")
	int32 UserData;
};

/** Creates raycast requests and delegates their result. */
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API URaycastComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	URaycastComponent();
	virtual ~URaycastComponent();

	/**
	  Delegate used to convey the result of a raycast.
	  @param HitResult structure containing the result of the raycast hit.
	*/
	DECLARE_DYNAMIC_DELEGATE_OneParam(FRaycastResultDelegate, FRaycastHitResult, HitResult);

	/**
	  Requests a raycast with the given query parameters.
	  @param RequestParams Parameters for the raycast query.
	  @param ResultDelegate Delegate which will be called when the raycast result is ready.
	  @returns True if the raycast request was successfully placed, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Raycast|MagicLeap")
	bool RequestRaycast(const FRaycastQueryParams& RequestParams, const FRaycastResultDelegate& ResultDelegate);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void FinishDestroy() override;

private:
	struct FRaycastRequestMetaData
	{
	public:
		FRaycastResultDelegate ResultDelegate;
		int32 UserData;
	};

	TMap<uint64, FRaycastRequestMetaData> PendingRequests;
	TArray<uint64> CompletedRequests;
	class FRaycastTrackerImpl *Impl;

#if WITH_EDITOR
private:
	void PrePIEEnded(bool bWasSimulatingInEditor);
#endif
};

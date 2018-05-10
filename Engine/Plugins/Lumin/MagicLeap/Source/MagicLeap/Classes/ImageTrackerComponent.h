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

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeBool.h"
#include "ImageTrackerComponent.generated.h"

/**
  The ImageTrackerComponent will keep track of whether the image that it has been provided is currently
  visible to the headset camera.
  @note Currently only R8G8B8A8 and B8G8R8A8 textures are supported.
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UImageTrackerComponent
	: public USceneComponent
{
	GENERATED_BODY()

public:
	/** Creates the FImageTrackerImpl instance.*/
	UImageTrackerComponent();

	/** Destroys the FImageTrackerImpl instance.*/
	virtual ~UImageTrackerComponent();

	/** Polls for and handles incoming messages from the asynchronous image tracking system. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Delegate used to notify the instigating blueprint that the target image is currently visible to the camera */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FImageTargetFound);

	/** Delegate used to notify the instigating blueprint that the target image just became invisible to the camera */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FImageTargetLost);

	/** 
	  Delegate used to notify the instigating blueprint that the target image's location has become unrealiable
	  @param LastTrackedLocation The last reliable location of the target image.
	  @param LastTrackedRotation The last reliable rotation of the target image.
	  @param NewUnreliableLocation The new location of the target image (which may or may not be accurate).
	  @param NewUnreliableRotation The new rotation of the target image (which may or may not be accurate).
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FImageTargetUnreliableTracking, const FVector&, LastTrackedLocation, const FRotator&, LastTrackedRotation, const FVector&, NewUnreliableLocation, const FRotator&, NewUnreliableRotation);

	/** Activated when the target image becomes visible to the camera */
	UPROPERTY(BlueprintAssignable)
	FImageTargetFound OnImageTargetFound;

	/** Activated the target image becomes invisible to the camera */
	UPROPERTY(BlueprintAssignable)
	FImageTargetLost OnImageTargetLost;

	/**
	  Activated when the target image is tracked with low confidence.

	  The Image Tracker system will still provide a 6 DOF pose. But this
	  pose might be inaccurate and might have jitter. When the tracking is
	  unreliable one of the folling two events will happen quickly : Either
	  the tracking will recover to Tracked or tracking will be lost and the
	  status will change to NotTracked.
	*/
	UPROPERTY(BlueprintAssignable)
	FImageTargetUnreliableTracking OnImageTargetUnreliableTracking;

	/** The texture that will be tracked by this image tracker instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageTracking|MagicLeap")
	UTexture2D* TargetImageTexture;

	/**
	  The name of the target.
	  This name has to be unique across all instances of the ImageTrackerComponent class.
	  If empty, the name of the component will be used.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageTracking|MagicLeap")
	FString Name;

	/** LongerDimension refers to the size of the longer dimension (width or height) of the physical image target in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageTracking|MagicLeap")
	float LongerDimension;

	/** Set this to true to improve detection for stationary targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageTracking|MagicLeap")
	bool bIsStationary;

	/** If false, the pose will not be updated when tracking is unreliable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageTracking|MagicLeap")
	bool bUseUnreliablePose;

private:
	class FImageTrackerImpl *Impl;
	bool bTick;

#if WITH_EDITOR
public:
	void PreEditChange(UProperty* PropertyAboutToChange) override;
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

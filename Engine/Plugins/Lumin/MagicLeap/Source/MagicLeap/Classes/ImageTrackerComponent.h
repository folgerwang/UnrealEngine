// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeBool.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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

	/**
		Attempts to change the currently tracked target.  Initiates an asynchronous call on a worker thread.
		When the task completes, the instigating blueprint will be notified by either a FSetImageTargetSucceeded
		or FSetImageTargetFailed event.
		@param ImageTarget The new texture to be tracked.
		@return True if the initiation of the target change was successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "ImageTracking|MagicLeap")
	bool SetTargetAsync(UTexture2D* ImageTarget);

	/** Delegate used to notify the instigating blueprint that the target image was successfully set. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSetImageTargetSucceeded);

	/** Delegate used to notify the instigating blueprint that the target image failed to be set. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSetImageTargetFailed);

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

	/** Activated when the target image is successfully set. */
	UPROPERTY(BlueprintAssignable)
	FSetImageTargetSucceeded OnSetImageTargetSucceeded;

	/** Activated when the target image fails to be set. */
	UPROPERTY(BlueprintAssignable)
	FSetImageTargetFailed OnSetImageTargetFailed;

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

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAP_API UImageTrackerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	  Set maximum number of Image Targets that can be tracked at any given time.

	  If the tracker is already tracking the maximum number of targets
	  possible then it will stop searching for new targets which helps
	  in reducing the load on the CPU. For example, if you are interested in
	  tracking a maximum of x targets from a list y (x < y) targets then set this
	  parameter to x.

	  The valid range for this parameter is from 1 through 25.
	*/
	UFUNCTION(BlueprintCallable, Category = "ImageTracking|MagicLeap")
	static void SetMaxSimultaneousTargets(int32 MaxSimultaneousTargets);

	/**
		Gets the maximum number of Image Targets that can be tracked at any given time.
		@return The maximum number of Image Targets that can be tracked at any given time.
	*/
	UFUNCTION(BlueprintCallable, Category = "ImageTracking|MagicLeap")
	static int32 GetMaxSimultaneousTargets();

	/**
	  If true, image tracker will detect and track targets.

	  When enabled Image Tracker will gain access to the camera and start
	  trackingimages. Enabling image tracker is expensive, takes about 1500ms
	  on the average.

	  When disabled Image Tracker will release the camera and stop tracking
	  images. Internal state of the tracker will be maintained (i.e. list of
	  active/inactive argets and their target_handles).

	  This is done automatically on application pause / resume.
	*/
	UFUNCTION(BlueprintCallable, Category = "ImageTracking|MagicLeap")
	static void EnableImageTracking(bool bEnable);

	/**
		Gets the active state of the image tracking system.
		@return True if image tracking is enabled, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "ImageTracking|MagicLeap")
	static bool IsImageTrackingEnabled();
};
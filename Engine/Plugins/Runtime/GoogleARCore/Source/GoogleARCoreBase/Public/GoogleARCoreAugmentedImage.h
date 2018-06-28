// Copyright 2017 Google Inc.

#pragma once

#include "GoogleARCoreAugmentedImageDatabase.h"

#include "ARTrackable.h"

#include "GoogleARCoreAugmentedImage.generated.h"

/**
 * An object representing an augmented image currently in the scene.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCoreAugmentedImage : public UARTrackedGeometry
{
	GENERATED_BODY()

public:

	/**
	 * Get the center in local space of the augmented image.
	 *
	 * @return The center in the augmented image's local coordinate
	 *         space.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	FVector GetCenter() const;

	/**
	 * Get the size in local space of the augmented image.
	 *
	 * @return The size in the augmented image's local coordinate
	 *         space.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	FVector GetExtent() const;

	/**
	 * Get the index of an augmented image in the augmented image
	 * database.
	 *
	 * @return An integer indicating the index into the Entries array
	 *         in the UGoogleARCoreAugmentedImageDatabase.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	int32 GetImageIndex() const;

	/**
	 * Get the name of an augmented image in the augmented image
	 * database.
	 *
	 * @return The name of the image in the image database.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	FString GetImageName() const;

	virtual void DebugDraw(
		UWorld* World, const FLinearColor& OutlineColor,
		float OutlineThickness, float PersistForSeconds = 0.0f) const override;

	void UpdateTrackedGeometry(
		const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem,
		uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform,
		const FTransform& InAlignmentTransform,
		const FVector &InCenter, const FVector &InExtent,
		int32 InImageIndex, const FString& ImageName);

private:

	UPROPERTY()
	FVector Center;

	UPROPERTY()
	FVector Extent;

	UPROPERTY()
	int32 ImageIndex;

	UPROPERTY()
	FString ImageName;

};


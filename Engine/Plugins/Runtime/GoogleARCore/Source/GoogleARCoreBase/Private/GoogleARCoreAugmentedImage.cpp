// Copyright 2018 Google Inc.

#include "GoogleARCoreAugmentedImage.h"

#include "GoogleARCoreAugmentedImageDatabase.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreAPI.h"

#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"

#if PLATFORM_ANDROID
#include "arcore_c_api.h"
#endif

void UGoogleARCoreAugmentedImage::DebugDraw(
	UWorld* World, const FLinearColor& OutlineColor,
	float OutlineThickness, float PersistForSeconds) const
{
#if PLATFORM_ANDROID

	FTransform CenterTransform =
		GetLocalToTrackingTransform();

	FVector HalfExtent(Extent / 2.0f);
	FVector Corner0(-HalfExtent.X, -HalfExtent.Y, 0.0f);
	FVector Corner1( HalfExtent.X, -HalfExtent.Y, 0.0f);
	FVector Corner2( HalfExtent.X,  HalfExtent.Y, 0.0f);
	FVector Corner3(-HalfExtent.X,  HalfExtent.Y, 0.0f);

	DrawDebugLine(
		World,
		CenterTransform.TransformPosition(Corner0),
		CenterTransform.TransformPosition(Corner1),
		OutlineColor.ToFColor(false),
		false,
		PersistForSeconds,
		0, OutlineThickness);

	DrawDebugLine(
		World,
		CenterTransform.TransformPosition(Corner1),
		CenterTransform.TransformPosition(Corner2),
		OutlineColor.ToFColor(false),
		false,
		PersistForSeconds,
		0, OutlineThickness);

	DrawDebugLine(
		World,
		CenterTransform.TransformPosition(Corner2),
		CenterTransform.TransformPosition(Corner3),
		OutlineColor.ToFColor(false),
		false,
		PersistForSeconds,
		0, OutlineThickness);

	DrawDebugLine(
		World,
		CenterTransform.TransformPosition(Corner3),
		CenterTransform.TransformPosition(Corner0),
		OutlineColor.ToFColor(false),
		false,
		PersistForSeconds,
		0, OutlineThickness);

#endif
}

void UGoogleARCoreAugmentedImage::UpdateTrackedGeometry(
	const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem,
	uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform,
	const FTransform& InAlignmentTransform, const FVector &InCenter, const FVector &InExtent,
	int32 InImageIndex, const FString& InImageName)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);

	Center = InCenter;
	Extent = InExtent;
	ImageIndex = InImageIndex;
	ImageName = InImageName;
}

FVector UGoogleARCoreAugmentedImage::GetCenter() const
{
	return Center;
}

FVector UGoogleARCoreAugmentedImage::GetExtent() const
{
	return Extent;
}

int32 UGoogleARCoreAugmentedImage::GetImageIndex() const
{
	return ImageIndex;
}

FString UGoogleARCoreAugmentedImage::GetImageName() const
{
	return ImageName;
}

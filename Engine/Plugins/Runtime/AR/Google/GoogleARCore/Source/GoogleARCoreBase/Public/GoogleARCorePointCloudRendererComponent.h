// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "GoogleARCorePointCloudRendererComponent.generated.h"

/** A helper component that renders the latest point cloud from the ARCore tracking session. */
UCLASS(Experimental, ClassGroup = (GoogleARCore), meta = (BlueprintSpawnableComponent))
class GOOGLEARCOREBASE_API UGoogleARCorePointCloudRendererComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	/** The color of the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|PointCloudRenderer")
	FColor PointColor;

	/** The size of the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|PointCloudRenderer")
	float PointSize;

	UGoogleARCorePointCloudRendererComponent();

	/** Function called on every frame on this Component. */
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction) override;

private:
	TArray<FVector> PointCloudInWorldSpace;
	double PreviousPointCloudTimestamp;

	void DrawPointCloud();
};

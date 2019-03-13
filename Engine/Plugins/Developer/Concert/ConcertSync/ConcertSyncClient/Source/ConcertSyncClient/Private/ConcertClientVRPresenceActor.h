// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ConcertClientPresenceActor.h"
#include "Engine/Scene.h"
#include "ConcertClientVRPresenceActor.generated.h"

/**
  * A ConcertClientVRPresenceActor is a child of AConcertClientPresenceActor that is used to represent users in VR 
  */
UCLASS(Abstract, Transient, NotPlaceable, Blueprintable)
class AConcertClientVRPresenceActor : public AConcertClientPresenceActor
{
	GENERATED_UCLASS_BODY()

public:
	virtual void HandleEvent(const FStructOnScope& InEvent) override;

	virtual void InitPresence(const class UConcertAssetContainer& InAssetContainer, FName DeviceType) override;

	virtual void SetPresenceColor(const FLinearColor& InColor) override;

	virtual void Tick(float DeltaSeconds) override;

	/** The left controller mesh */
	UPROPERTY()
	class UStaticMeshComponent* LeftControllerMeshComponent;

	/** The right controller mesh */
	UPROPERTY()
	class UStaticMeshComponent* RightControllerMeshComponent;

	/** Dynamic material for the laser */
	UPROPERTY()
	class UMaterialInstanceDynamic* LaserMid;

	/** Dynamic material for the laser */
	UPROPERTY()
	class UMaterialInstanceDynamic* LaserCoreMid;

private:
	/** Updates all the segments of the curved laser */
	void UpdateSplineLaser(const FVector& InStartLocation, const FVector& InEndLocation);

	void HideLeftController();
	void ShowLeftController();

	void HideRightController();
	void ShowRightController();

	void HideLaser();
	void ShowLaser();

	/** Spline for this hand's laser pointer */
	UPROPERTY()
	class USplineComponent* LaserSplineComponent;

	/** Spline meshes for curved laser */
	UPROPERTY()
	TArray<class USplineMeshComponent*> LaserSplineMeshComponents;

	UPROPERTY()
	bool bIsLeftControllerVisible;

	UPROPERTY()
	bool bIsRightControllerVisible;

	UPROPERTY()
	bool bIsLaserVisible;

	/** Movement smoothing for left controller */
	TOptional<FConcertClientMovement> LeftControllerMovement;

	/** Movement smoothing for right controller */
	TOptional<FConcertClientMovement> RightControllerMovement;

	/** Movement smoothing for laser start */
	TOptional<FConcertClientMovement> LaserStartMovement;

	/** Movement smoothing for laser start */
	TOptional<FConcertClientMovement> LaserEndMovement;
};


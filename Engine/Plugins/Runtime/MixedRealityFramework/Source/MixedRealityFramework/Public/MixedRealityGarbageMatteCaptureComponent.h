// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Actor.h"
#include "MixedRealityConfigurationSaveGame.h" // for FGarbageMatteSaveData
#include "MixedRealityGarbageMatteCaptureComponent.generated.h"

class AMixedRealityGarbageMatteActor;
class UMixedRealityCalibrationData;

/**
 *	
 */
UCLASS(ClassGroup = Rendering, editinlinenew, config = Engine)
class MIXEDREALITYFRAMEWORK_API UMixedRealityGarbageMatteCaptureComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

public:
	//~ UActorComponent interface
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	//~ USceneCaptureComponent interface
	virtual const AActor* GetViewOwner() const override;

public:
	UFUNCTION()
	void SetTrackingOrigin(USceneComponent* TrackingOrigin);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MixedReality|GarbageMatting")
	void ApplyCalibrationData(const UMixedRealityCalibrationData* ConfigData);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|GarbageMatting")
	void SetGarbageMatteActor(AMixedRealityGarbageMatteActor* NewActor);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|GarbageMatting")
	void GetGarbageMatteData(TArray<FGarbageMatteSaveData>& GarbageMatteDataOut);

	UFUNCTION(BlueprintNativeEvent, Category = "MixedReality|GarbageMatting")
	AMixedRealityGarbageMatteActor* SpawnNewGarbageMatteActor(USceneComponent* TrackingOrigin);

private:
	UPROPERTY(Transient, Config)
	TSubclassOf<AMixedRealityGarbageMatteActor> GarbageMatteActorClass;

	UPROPERTY(Transient)
	AMixedRealityGarbageMatteActor* GarbageMatteActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> TrackingOriginPtr;
};

/* AMixedRealityGarbageMatteActor
 *****************************************************************************/

class UMaterial;
class UStaticMesh;

UCLASS(notplaceable, Blueprintable)
class AMixedRealityGarbageMatteActor : public AActor
{
	GENERATED_UCLASS_BODY()


public: 
	UFUNCTION(BlueprintCallable, Category = "MixedReality|GarbageMatting")
	void ApplyCalibrationData(const TArray<FGarbageMatteSaveData>& GarbageMatteData);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|GarbageMatting")
	UPrimitiveComponent* AddNewGabageMatte(const FGarbageMatteSaveData& GarbageMatteData);

	UFUNCTION(BlueprintNativeEvent, Category = "MixedReality|GarbageMatting")
	UPrimitiveComponent* CreateGarbageMatte(const FGarbageMatteSaveData& GarbageMatteData);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|GarbageMatting")
	void GetGarbageMatteData(TArray<FGarbageMatteSaveData>& GarbageMatteDataOut);

private:
	UPROPERTY(Transient)
	UStaticMesh* GarbageMatteMesh;

	UPROPERTY(Transient)
	UMaterial* GarbageMatteMaterial;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "MixedReality|GarbageMatting", meta=(AllowPrivateAccess="true"))
	TArray<UPrimitiveComponent*> GarbageMattes;
};

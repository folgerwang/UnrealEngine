// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Tickable.h"

class UWorld;
class UEmulatorCameraModifier;
class UTextureRenderTarget2D;
class UMagicLeapEmulatorSceneCaptureComponent;
class UTexture2D;
class AGameModeBase;
class APlayerController;

/**
  * 
  */
class MAGICLEAPEMULATOR_API FMagicLeapEmulator : public FTickableGameObject
{
public:
 	FMagicLeapEmulator();
 	virtual ~FMagicLeapEmulator();

	/** Begin emulation in the given world. */
	void StartEmulating(UWorld* World);
	
	/** End emulation in the given world. */
	void StopEmulating(UWorld* World);

	void Update(float DeltaTime);
	void UpdateSceneCaptureTransform(UMagicLeapEmulatorSceneCaptureComponent* Comp);

	/** FTickableGameObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMagicLeapEmulator, STATGROUP_Tickables);
	}

private:
 
	uint32 bEmulatorInitialized : 1;

	float LastProjectionFOVDegrees;

	FTimerHandle InitCameraTimerHandle;
	TWeakObjectPtr<UWorld> MyWorld;
	TWeakObjectPtr<APlayerController> MyPlayerController;
	TWeakObjectPtr<class UEmulatorCameraModifier> EmulatorCameraModifier;

	TWeakObjectPtr<UTextureRenderTarget2D> BackgroundRenderTarget_LeftOrFull;
	TWeakObjectPtr<UMagicLeapEmulatorSceneCaptureComponent> BackgroundSceneCaptureComponent_LeftOrFull;

	TWeakObjectPtr<UTextureRenderTarget2D> BackgroundRenderTarget_Right;
	TWeakObjectPtr<UMagicLeapEmulatorSceneCaptureComponent> BackgroundSceneCaptureComponent_Right;

	FDelegateHandle PostLoginDelegateHandle;

	void InitEmulatorCamera(AGameModeBase* GameMode, APlayerController* NewPlayer);

	void ReallyInitCamera();

	void HandleOnActorSpawned(AActor* A);
	FDelegateHandle OnActorSpawnedHandle;
};


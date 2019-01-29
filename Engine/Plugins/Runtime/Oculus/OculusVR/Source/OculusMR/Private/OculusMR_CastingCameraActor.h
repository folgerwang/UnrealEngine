// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/SceneCapture2D.h"
#include "OVR_Plugin_Types.h"

#include "OculusMR_CastingCameraActor.generated.h"

class UOculusMR_PlaneMeshComponent;
class UMaterial;
class AOculusMR_BoundaryActor;
class UTextureRenderTarget2D;
class UOculusMR_Settings;
class UOculusMR_State;

/**
* The camera actor in the level that tracks the binded physical camera in game
*/
UCLASS(ClassGroup = OculusMR, NotPlaceable, NotBlueprintable)
class AOculusMR_CastingCameraActor : public ASceneCapture2D
{
	GENERATED_BODY()

public:
	AOculusMR_CastingCameraActor(const FObjectInitializer& ObjectInitializer);

	/** Initialize the MRC settings and states */
	void InitializeStates(UOculusMR_Settings* MRSettingsIn, UOculusMR_State* MRStateIn);

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;

	virtual void BeginDestroy() override;

	UPROPERTY()
	class UVRNotificationsComponent* VRNotificationComponent;

	UPROPERTY()
	UTexture2D* CameraColorTexture;

	UPROPERTY()
	UTexture2D* CameraDepthTexture;

	UPROPERTY()
	UOculusMR_PlaneMeshComponent* PlaneMeshComponent;

	UPROPERTY()
	UMaterial* ChromaKeyMaterial;

	UPROPERTY()
	UMaterial* ChromaKeyLitMaterial;

	UPROPERTY()
	UMaterial* OpaqueColoredMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* ChromaKeyMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* ChromaKeyLitMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* CameraFrameMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* BackdropMaterialInstance;

	UPROPERTY()
	AOculusMR_BoundaryActor* BoundaryActor;

	UPROPERTY()
	ASceneCapture2D* BoundarySceneCaptureActor;

	UPROPERTY()
	class UTexture2D* DefaultTexture_White;

	bool TrackedCameraCalibrationRequired;
	bool HasTrackedCameraCalibrationCalibrated;
	FQuat InitialCameraAbsoluteOrientation;
	FVector InitialCameraAbsolutePosition;
	FQuat InitialCameraRelativeOrientation;
	FVector InitialCameraRelativePosition;

	int32 RefreshBoundaryMeshCounter;

private:

	/** Move the casting camera to follow the tracking reference (i.e. player) */
	void RequestTrackedCameraCalibration();

	bool RefreshExternalCamera();
	void UpdateCameraColorTexture(const ovrpSizei &colorFrameSize, const ovrpByte* frameData, int rowPitch);
	void UpdateCameraDepthTexture(const ovrpSizei &depthFrameSize, const float* frameData, int rowPitch);

	void CalibrateTrackedCameraPose();
	void SetTrackedCameraUserPoseWithCameraTransform();
	void SetTrackedCameraInitialPoseWithPlayerTransform();
	void UpdateTrackedCameraPosition();
	void UpdateBoundaryCapture();

	/** Initialize the tracked physical camera */
	void SetupTrackedCamera();

	/** Close the tracked physical camera */
	void CloseTrackedCamera();

	void OnHMDRecentered();

	const FLinearColor& GetForegroundLayerBackgroundColor() const { return ForegroundLayerBackgroundColor; }

	void SetupCameraFrameMaterialInstance();
	void SetupBackdropMaterialInstance();
	void RepositionPlaneMesh();
	void RefreshBoundaryMesh();
	void UpdateRenderTargetSize();
	void SetupSpectatorScreen();
	void CloseSpectatorScreen();

	void Execute_BindToTrackedCameraIndexIfAvailable();

	FLinearColor ForegroundLayerBackgroundColor;
	float ForegroundMaxDistance;

	UPROPERTY()
	UTextureRenderTarget2D* BackgroundRenderTarget;

	UPROPERTY()
	ASceneCapture2D* ForegroundCaptureActor;

	UPROPERTY()
	UTextureRenderTarget2D* ForegroundRenderTarget;

	UPROPERTY()
	UOculusMR_Settings* MRSettings;

	UPROPERTY()
	UOculusMR_State* MRState;
};

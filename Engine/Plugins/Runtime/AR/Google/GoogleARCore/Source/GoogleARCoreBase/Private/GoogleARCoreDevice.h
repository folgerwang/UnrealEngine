// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDelegates.h"
#include "Engine/EngineBaseTypes.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Containers/Map.h"

#include "GoogleARCoreTypes.h"
#include "GoogleARCoreAPI.h"
#include "GoogleARCorePassthroughCameraRenderer.h"
#include "GoogleARCoreAugmentedImageDatabase.h"

class FGoogleARCoreDevice
{
public:
	static FGoogleARCoreDevice* GetInstance();

	FGoogleARCoreDevice();

	EGoogleARCoreAvailability CheckARCoreAPKAvailability();

	EGoogleARCoreAPIStatus RequestInstall(bool bUserRequestedInstall, EGoogleARCoreInstallStatus& OutInstallStatus);

	bool GetIsTrackingTypeSupported(EARSessionType SessionType);

	bool GetIsARCoreSessionRunning();

	FARSessionStatus GetSessionStatus();

	// Get Unreal Units per meter, based off of the current map's VR World to Meters setting.
	float GetWorldToMetersScale();

	// Start ARSession with custom session config.
	void StartARCoreSessionRequest(UARSessionConfig* SessionConfig);

	bool SetARCameraConfig(FGoogleARCoreCameraConfig CameraConfig);

	bool GetARCameraConfig(FGoogleARCoreCameraConfig& OutCurrentCameraConfig);

	// Add image to TargetImageDatabase and return the image index.
	// Return -1 if the image cannot be processed.
	int AddRuntimeAugmentedImage(UGoogleARCoreAugmentedImageDatabase* TargetImageDatabase, const TArray<uint8>& ImageGrayscalePixels,
		int ImageWidth, int ImageHeight, FString ImageName, float ImageWidthInMeter);

	bool AddRuntimeCandidateImage(UARSessionConfig* SessionConfig, const TArray<uint8>& ImageGrayscalePixels, int ImageWidth, int ImageHeight,
		FString FriendlyName, float PhysicalWidth);

	bool GetStartSessionRequestFinished();

	void PauseARCoreSession();

	void ResetARCoreSession();

	void AllocatePassthroughCameraTexture_RenderThread();
	FTextureRHIRef GetPassthroughCameraTexture();

	// Passthrough Camera
	FMatrix GetPassthroughCameraProjectionMatrix(FIntPoint ViewRectSize) const;
	void GetPassthroughCameraImageUVs(const TArray<float>& InUvs, TArray<float>& OutUVs) const;

	// Frame
	EGoogleARCoreTrackingState GetTrackingState() const;
	FTransform GetLatestPose() const;
	FGoogleARCoreLightEstimate GetLatestLightEstimate() const;
	EGoogleARCoreFunctionStatus GetLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const;
	EGoogleARCoreFunctionStatus AcquireLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const;
#if PLATFORM_ANDROID
	EGoogleARCoreFunctionStatus GetLatestCameraMetadata(const ACameraMetadata*& OutCameraMetadata) const;
#endif
	UTexture* GetCameraTexture();
	EGoogleARCoreFunctionStatus AcquireCameraImage(UGoogleARCoreCameraImage *&OutLatestCameraImage);

	// Hit test
	void ARLineTrace(const FVector2D& ScreenPosition, EGoogleARCoreLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults);
	void ARLineTrace(const FVector& Start, const FVector& End, EGoogleARCoreLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults);

	// Anchor, Planes
	EGoogleARCoreFunctionStatus CreateARPin(const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, USceneComponent* ComponentToPin, const FName DebugName, UARPin*& OutARAnchorObject);
	void RemoveARPin(UARPin* ARAnchorObject);

	void GetAllARPins(TArray<UARPin*>& ARCoreAnchorList);
	void GetUpdatedARPins(TArray<UARPin*>& ARCoreAnchorList);

	template< class T >
	void GetUpdatedTrackables(TArray<T*>& OutARCoreTrackableList)
	{
		if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
		{
			return;
		}
		ARCoreSession->GetLatestFrame()->GetUpdatedTrackables<T>(OutARCoreTrackableList);
	}

	template< class T >
	void GetAllTrackables(TArray<T*>& OutARCoreTrackableList)
	{
		if (!ARCoreSession.IsValid())
		{
			return;
		}
		ARCoreSession->GetAllTrackables<T>(OutARCoreTrackableList);
	}

	// Camera Intrinsics
	EGoogleARCoreFunctionStatus GetCameraImageIntrinsics(UGoogleARCoreCameraIntrinsics *&OutCameraIntrinsics);
	EGoogleARCoreFunctionStatus GetCameraTextureIntrinsics(UGoogleARCoreCameraIntrinsics *&OutCameraIntrinsics);

	void RunOnGameThread(TFunction<void()> Func)
	{
		RunOnGameThreadQueue.Enqueue(Func);
	}

	void GetRequiredRuntimePermissionsForConfiguration(const UARSessionConfig& Config, TArray<FString>& RuntimePermissions)
	{
		RuntimePermissions.Reset();
		// TODO: check for depth camera when it is supported here.
		RuntimePermissions.Add("android.permission.CAMERA");
	}
	void HandleRuntimePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& Granted);

	// Function that is used to call from the Android UI thread:
	void StartSessionWithRequestedConfig();

	TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> GetARSystem();
	void SetARSystem(TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> InARSystem);

	void* GetARSessionRawPointer();
	void* GetGameThreadARFrameRawPointer();

private:
	// Android lifecycle events.
	void OnApplicationCreated();
	void OnApplicationDestroyed();
	void OnApplicationPause();
	void OnApplicationResume();
	void OnApplicationStart();
	void OnApplicationStop();
	void OnDisplayOrientationChanged();

	// Unreal plugin events.
	void OnModuleLoaded();
	void OnModuleUnloaded();

	void OnWorldTickStart(ELevelTick TickType, float DeltaTime);

	void CheckAndRequrestPermission(const UARSessionConfig& ConfigurationData);

	void StartSession();

	friend class FGoogleARCoreAndroidHelper;
	friend class FGoogleARCoreBaseModule;

	UARSessionConfig* AccessSessionConfig() const;

private:
	TSharedPtr<FGoogleARCoreSession> ARCoreSession;
	FTextureRHIRef PassthroughCameraTexture;
	uint32 PassthroughCameraTextureId;
	bool bIsARCoreSessionRunning;
	bool bForceLateUpdateEnabled; // A debug flag to force use late update.
	bool bSessionConfigChanged;
	bool bAndroidRuntimePermissionsRequested;
	bool bAndroidRuntimePermissionsGranted;
	bool bPermissionDeniedByUser;
	bool bStartSessionRequested; // User called StartSession
	bool bShouldSessionRestart; // Start tracking on activity start
	bool bARCoreInstallRequested;
	bool bARCoreInstalled;
	float WorldToMeterScale;
	class UARCoreAndroidPermissionHandler* PermissionHandler;
	FThreadSafeBool bDisplayOrientationChanged;

	FARSessionStatus CurrentSessionStatus;

	FGoogleARCoreCameraConfig SessionCameraConfig;
	FGoogleARCoreDeviceCameraBlitter CameraBlitter;

	TQueue<TFunction<void()>> RunOnGameThreadQueue;

	TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARSystem;
};

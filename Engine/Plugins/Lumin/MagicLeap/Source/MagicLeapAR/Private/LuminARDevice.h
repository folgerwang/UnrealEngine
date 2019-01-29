// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDelegates.h"
#include "Engine/EngineBaseTypes.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Containers/Map.h"

#include "LuminARTypes.h"
#include "LuminARAPI.h"

class FLuminARImplementation;

class FLuminARDevice
{
public:
	static FLuminARDevice* GetInstance();

	FLuminARDevice();

	bool GetIsTrackingTypeSupported(EARSessionType SessionType);

	bool GetIsLuminARSessionRunning();

	EARSessionStatus GetSessionStatus();

	// Get Unreal Units per meter, based off of the current map's VR World to Meters setting.
	float GetWorldToMetersScale();

	// Start ARSession with custom session config.
	void StartLuminARSessionRequest(UARSessionConfig* SessionConfig);

	bool GetStartSessionRequestFinished();

	void PauseLuminARSession();

	void ResetLuminARSession();

	// Passthrough Camera
	FMatrix GetPassthroughCameraProjectionMatrix(FIntPoint ViewRectSize) const;
	void GetPassthroughCameraImageUVs(const TArray<float>& InUvs, TArray<float>& OutUVs) const;

	// Frame
	ELuminARTrackingState GetTrackingState() const;
	//FTransform GetLatestPose() const;
	FLuminARLightEstimate GetLatestLightEstimate() const;
	// Hit test
	void ARLineTrace(const FVector2D& ScreenPosition, ELuminARLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults);
	void ARLineTrace(const FVector& Start, const FVector& End, ELuminARLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults);

	// Anchor, Planes
	ELuminARFunctionStatus CreateARPin(const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, USceneComponent* ComponentToPin, const FName DebugName, UARPin*& OutARAnchorObject);
	void RemoveARPin(UARPin* ARAnchorObject);

	void GetAllARPins(TArray<UARPin*>& LuminARAnchorList);

	template< class T >
	void GetAllTrackables(TArray<T*>& OutLuminARTrackableList)
	{
		if (!bIsLuminARSessionRunning)
		{
			return;
		}
		LuminARSession->GetAllTrackables<T>(OutLuminARTrackableList);
	}

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

	TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> GetARSystem();
	void SetARSystem(TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> InARSystem);

	TSharedPtr<FLuminARImplementation, ESPMode::ThreadSafe> GetLuminARImplementation();
	void SetLuminARImplementation(TSharedPtr<FLuminARImplementation, ESPMode::ThreadSafe> InARImplementation);

	ELuminARFunctionStatus AcquireCameraImage(ULuminARCameraImage *&OutLatestCameraImage);

	void* GetARSessionRawPointer();
	void* GetGameThreadARFrameRawPointer();

private:
	// Unreal plugin events.
	void OnModuleLoaded();
	void OnModuleUnloaded();

	void OnWorldTickStart(ELevelTick TickType, float DeltaTime);

	void StartSession();

	friend class FLuminARAndroidHelper;
	friend class FLuminARModule;

	UARSessionConfig* AccessSessionConfig() const;

private:
	TSharedPtr<FLuminARSession> LuminARSession;
	bool bIsLuminARSessionRunning;
	bool bForceLateUpdateEnabled; // A debug flag to force use late update.
	bool bSessionConfigChanged;
	bool bStartSessionRequested; // User called StartSession
	bool bShouldSessionRestart; // Start tracking on activity start
	float WorldToMeterScale;

	EARSessionStatus CurrentSessionStatus;

	TQueue<TFunction<void()>> RunOnGameThreadQueue;

	TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> ARSystem;
	TSharedPtr<FLuminARImplementation, ESPMode::ThreadSafe> LuminARImplementation;
};

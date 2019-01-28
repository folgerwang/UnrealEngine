// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "XRTrackingSystemBase.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "SceneViewExtension.h"
#include "Slate/SceneViewport.h"
#include "SceneView.h"
#include "LuminARDevice.h"
#include "ARSystem.h"
#include "ARLightEstimate.h"
#include "Engine/Engine.h" // for FWorldContext

DEFINE_LOG_CATEGORY_STATIC(LogLuminAR, Log, All);

class FLuminARImplementation : public IARSystemSupport, public FGCObject, public TSharedFromThis<IARSystemSupport, ESPMode::ThreadSafe>
{
	friend class FLuminARCamera;

public:
	FLuminARImplementation();
	~FLuminARImplementation();

	// IXRTrackingSystem

	void* GetARSessionRawPointer() override;
	void* GetGameThreadARFrameRawPointer() override;

protected:
	// IARSystemSupport
	virtual void OnARSystemInitialized() override;
	virtual bool OnStartARGameFrame(FWorldContext& WorldContext) override;

	virtual EARTrackingQuality OnGetTrackingQuality() const override;
	virtual void OnStartARSession(UARSessionConfig* SessionConfig) override;
	virtual void OnPauseARSession() override;
	virtual void OnStopARSession() override;
	virtual FARSessionStatus OnGetARSessionStatus() const override;
	virtual void OnSetAlignmentTransform(const FTransform& InAlignmentTransform) override;
	virtual TArray<FARTraceResult> OnLineTraceTrackedObjects(const FVector2D ScreenCoord, EARLineTraceChannels TraceChannels) override;
	virtual TArray<FARTraceResult> OnLineTraceTrackedObjects(const FVector Start, const FVector End, EARLineTraceChannels TraceChannels) override;
	virtual TArray<UARTrackedGeometry*> OnGetAllTrackedGeometries() const override;
	virtual TArray<UARPin*> OnGetAllPins() const override;
	virtual bool OnIsTrackingTypeSupported(EARSessionType SessionType) const override;
	virtual UARLightEstimate* OnGetCurrentLightEstimate() const override;

	virtual UARPin* OnPinComponent(USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry = nullptr, const FName DebugName = NAME_None) override;
	virtual void OnRemovePin(UARPin* PinToRemove) override;
	virtual UARTextureCameraImage* OnGetCameraImage() override { return nullptr; }
	virtual UARTextureCameraDepth* OnGetCameraDepth() override { return nullptr; }
	virtual bool OnAddManualEnvironmentCaptureProbe(FVector Location, FVector Extent) { return false; }
	virtual TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> OnGetCandidateObject(FVector Location, FVector Extent) const { return TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe>(); }
	virtual TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> OnSaveWorld() const { return TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe>(); }
	virtual EARWorldMappingState OnGetWorldMappingStatus() const { return EARWorldMappingState::StillMappingNotRelocalizable; }
	virtual TArray<FARVideoFormat> OnGetSupportedVideoFormats(EARSessionType SessionType) const override { return TArray<FARVideoFormat>(); }
	virtual TArray<FVector> OnGetPointCloud() const;
	virtual bool OnAddRuntimeCandidateImage(UARSessionConfig* SessionConfig, UTexture2D* CandidateTexture, FString FriendlyName, float PhysicalWidth) { return false; }
	//~IARSystemSupport

private:
	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ FGCObject

private:
	FLuminARDevice* LuminARDeviceInstance;

	bool bHasValidPose;

	FRotator DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat DeltaControlOrientation; // same as DeltaControlRotation but as quat

	TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> ViewExtension;

	UARBasicLightEstimate* LightEstimate;
};

DEFINE_LOG_CATEGORY_STATIC(LogLuminARImplementation, Log, All);

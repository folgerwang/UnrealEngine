// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"

#include "AppleARKitAvailability.h"

#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif

class UARSessionConfig;
class UARTrackedGeometry;
class FAppleARKitConfiguration;
class FARSystemBase;


class APPLEARKIT_API IAppleARKitFaceSupportCallback
{
public:
	/** So that face processing can get access to the face geometry objects by their guid */
	virtual UARTrackedGeometry* GetTrackedGeometry(const FGuid& GeoGuid) = 0;
	/** So that face processing can add new geometry as updates come in */
	virtual void AddTrackedGeometry(const FGuid& Guid, UARTrackedGeometry* TrackedGeo) = 0;
};

class APPLEARKIT_API IAppleARKitFaceSupport
{
public:
#if SUPPORTS_ARKIT_1_0
	/**
	 * Forwards the anchor add for face processing
	 *
	 * @param NewAnchors the anchor list that the face support should process
	 * @param AlignmentTransform the transform to apply for user alignment
	 * @param FrameNumber the frame number to publish with LiveLink
	 * @param Timestamp the timestamp to publish with LiveLink
	 */
	virtual void ProcessAnchorAdd(NSArray<ARAnchor*>* NewAnchors, const FTransform& AlignmentTransform, uint32 FrameNumber, double Timestamp) = 0;

	/**
	 * Forwards the anchor add for face processing
	 *
	 * @param UpdatedAnchors the anchor list that the face support should process
	 * @param AlignmentTransform the transform to apply for user alignment
	 * @param FrameNumber the frame number to publish with LiveLink
	 * @param Timestamp the timestamp to publish with LiveLink
	 */
	virtual void ProcessAnchorUpdate(NSArray<ARAnchor*>* UpdatedAnchors, const FTransform& AlignmentTransform, uint32 FrameNumber, double Timestamp) = 0;

	/**
	 * Creates a face ar specific configuration object if that is requested
	 *
	 * @param SessionConfig the UE4 configuration object that needs processing
	 * @param InConfiguration the legacy configuration object that needs to go away
	 */
	virtual ARConfiguration* ToARConfiguration(UARSessionConfig* SessionConfig, FAppleARKitConfiguration& InConfiguration) = 0;
#endif

	IAppleARKitFaceSupport() { }
	IAppleARKitFaceSupport(const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem, IAppleARKitFaceSupportCallback* Callback) { }
	virtual ~IAppleARKitFaceSupport() { }
};

class APPLEARKIT_API FAppleARKitFaceSupportBase :
	public IAppleARKitFaceSupport,
	public TSharedFromThis<FAppleARKitFaceSupportBase, ESPMode::ThreadSafe>
{
public:
#if SUPPORTS_ARKIT_1_0
	/**
	 * Forwards the anchor add for face processing
	 *
	 * @param NewAnchors the anchor list that the face support should process
	 * @param AlignmentTransform the transform to apply for user alignment
	 * @param FrameNumber the frame number to publish with LiveLink
	 * @param Timestamp the timestamp to publish with LiveLink
	 */
	virtual void ProcessAnchorAdd(NSArray<ARAnchor*>* NewAnchors, const FTransform& AlignmentTransform, uint32 FrameNumber, double Timestamp) override
	{

	}

	/**
	 * Forwards the anchor add for face processing
	 *
	 * @param UpdatedAnchors the anchor list that the face support should process
	 * @param AlignmentTransform the transform to apply for user alignment
	 * @param FrameNumber the frame number to publish with LiveLink
	 * @param Timestamp the timestamp to publish with LiveLink
	 */
	virtual void ProcessAnchorUpdate(NSArray<ARAnchor*>* UpdatedAnchors, const FTransform& AlignmentTransform, uint32 FrameNumber, double Timestamp) override
	{

	}

	/**
	 * Creates a face ar specific configuration object if that is requested
	 *
	 * @param SessionConfig the UE4 configuration object that needs processing
	 * @param InConfiguration the legacy configuration object that needs to go away
	 */
	virtual ARConfiguration* ToARConfiguration(UARSessionConfig* SessionConfig, FAppleARKitConfiguration& InConfiguration) override
	{
		return nullptr;
	}
#endif

	FAppleARKitFaceSupportBase() { }
	FAppleARKitFaceSupportBase(const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem, IAppleARKitFaceSupportCallback* Callback) { }
	virtual ~FAppleARKitFaceSupportBase() { }
};

class APPLEARKIT_API IAppleARKitFaceSupportFactory :
	public IModularFeature
{
public:
	/** Factory method that returns the object to use to handle face ar requests */
	virtual TSharedPtr<FAppleARKitFaceSupportBase, ESPMode::ThreadSafe> CreateFaceSupport(const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem, IAppleARKitFaceSupportCallback* Callback) = 0;

	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("AppleARKitFaceSupportFactory"));
		return FeatureName;
	}
};

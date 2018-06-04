// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"

#include "AppleARKitAvailability.h"

#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif

class UARSessionConfig;
struct FAppleARKitAnchorData;


class APPLEARKIT_API IAppleARKitFaceSupport
{
public:
#if SUPPORTS_ARKIT_1_0
	/**
	 * Converts a set of generic ARAnchors into their face anchor equivalents without exposing the main code to the face APIs
	 *
	 * @param NewAnchors the list of anchors to convert to our intermediate format
	 * @param Timestamp the timestamp of this update
	 * @param FrameNumber the frame number for this update
	 *
	 * @return the set of face anchors to dispatch
	 */
	virtual TArray<TSharedPtr<FAppleARKitAnchorData>> MakeAnchorData(NSArray<ARAnchor*>* NewAnchors, double Timestamp, uint32 FrameNumber) = 0;

	/**
	 * Publishes any face AR data that needs to be sent to LiveLink. Done as a separate step because MakeAnchorData is called
	 * on an arbitrary thread and we can't access UObjects there safely
	 *
	 * @param AnchorList the list of anchors to publish to LiveLink
	 * @param Timestamp the timestamp of this update
	 * @param FrameNumber the frame number for this update
	 *
	 * @return the set of face anchors to dispatch
	 */
	virtual void PublishLiveLinkData(TSharedPtr<FAppleARKitAnchorData> Anchor, double Timestamp, uint32 FrameNumber) = 0;

	/**
	 * Creates a face ar specific configuration object if that is requested without exposing the main code to the face APIs
	 *
	 * @param SessionConfig the UE4 configuration object that needs processing
	 */
	virtual ARConfiguration* ToARConfiguration(UARSessionConfig* SessionConfig) = 0;
#endif
};

class APPLEARKIT_API IAppleARKitFaceSupportFactory :
	public IModularFeature
{
public:
	/** Factory method that returns the object to use to handle face ar requests */
	virtual IAppleARKitFaceSupport* CreateFaceSupport() = 0;

	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("AppleARKitFaceSupportFactory"));
		return FeatureName;
	}
};

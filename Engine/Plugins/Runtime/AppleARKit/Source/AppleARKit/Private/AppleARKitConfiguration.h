// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "AppleARKitAvailability.h"

#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif

#include "ARSystem.h"
#include "AppleARKitConversion.h"


#if SUPPORTS_ARKIT_1_0
ARConfiguration* ToARConfiguration( UARSessionConfig* SessionConfig, class FAppleARKitConfiguration& InConfiguration, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages );
#endif

/**
 * An object to describe and configure the Augmented Reality techniques to be used in a
 * UAppleARKitSession.
 */
class APPLEARKIT_API FAppleARKitConfiguration
{
public:
	

	/**
	 * Enable or disable light estimation.
	 * @discussion Enabled by default.
	 */
	bool bLightEstimationEnabled = true;
	
	/**
	 * Enables audio capture during the AR session
	 */
	bool bProvidesAudioData = false;
	
	/**
	 * The alignment that transforms will be with respect to.
	 * @discussion The default is ARWorldAlignmentGravity.
	 */
	EAppleARKitWorldAlignment Alignment = EAppleARKitWorldAlignment::Gravity;
};


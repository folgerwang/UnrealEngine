// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitConfiguration.h"
#include "AppleARKitModule.h"

#include "IAppleImageUtilsPlugin.h"

#if SUPPORTS_ARKIT_1_0

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

ARWorldAlignment ToARWorldAlignment( const EAppleARKitWorldAlignment& InWorldAlignment )
{
	switch ( InWorldAlignment )
	{
		case EAppleARKitWorldAlignment::Gravity:
			return ARWorldAlignmentGravity;
			
		case EAppleARKitWorldAlignment::GravityAndHeading:
			return ARWorldAlignmentGravityAndHeading;

		case EAppleARKitWorldAlignment::Camera:
			return ARWorldAlignmentCamera;
	};
};

#if SUPPORTS_ARKIT_1_5

void InitImageDetection(UARSessionConfig* SessionConfig, ARWorldTrackingConfiguration* WorldConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
{
	const TArray<UARCandidateImage*>& ConfigCandidateImages = SessionConfig->GetCandidateImageList();
	if (!ConfigCandidateImages.Num())
	{
		return;
	}

	NSMutableSet* ConvertedImageSet = [[NSMutableSet new] autorelease];
	for (UARCandidateImage* Candidate : ConfigCandidateImages)
	{
		if (Candidate != nullptr && Candidate->GetCandidateTexture() != nullptr)
		{
			// Store off so the session object can quickly match the anchor to our representation
			// This stores it even if we weren't able to convert to apple's type for GC reasons
			CandidateImages.Add(Candidate->GetFriendlyName(), Candidate);
			// Convert our texture to an Apple compatible image type
			CGImageRef ConvertedImage = nullptr;
			// Avoid doing the expensive conversion work if it's in the cache already
			CGImageRef* FoundImage = ConvertedCandidateImages.Find(Candidate->GetFriendlyName());
			if (FoundImage != nullptr)
			{
				ConvertedImage = *FoundImage;
			}
			else
			{
				ConvertedImage = IAppleImageUtilsPlugin::Get().UTexture2DToCGImage(Candidate->GetCandidateTexture());
				// If it didn't convert this time, it never will, so always store it off
				ConvertedCandidateImages.Add(Candidate->GetFriendlyName(), ConvertedImage);
			}
			if (ConvertedImage != nullptr)
			{
                float ImageWidth = (float)Candidate->GetPhysicalWidth() / 100.f;
				CGImagePropertyOrientation Orientation = Candidate->GetOrientation() == EARCandidateImageOrientation::Landscape ? kCGImagePropertyOrientationRight : kCGImagePropertyOrientationUp;

				ARReferenceImage* ReferenceImage = [[[ARReferenceImage alloc] initWithCGImage: ConvertedImage orientation: Orientation physicalWidth: ImageWidth] autorelease];
                ReferenceImage.name = Candidate->GetFriendlyName().GetNSString();
				[ConvertedImageSet addObject: ReferenceImage];
			}
			else
			{
				UE_LOG(LogAppleARKit, Log, TEXT("Failed to convert the texture to an Apple compatible image for UARCandidateImage (%s)"), *Candidate->GetFriendlyName());
			}
		}
		else
		{
			UE_LOG(LogAppleARKit, Log, TEXT("Missing texture for ARCandidateImage (%s)"), Candidate != nullptr ? *Candidate->GetFriendlyName() : TEXT("null"));
		}
	}
	WorldConfig.detectionImages = ConvertedImageSet;
}

#endif

ARConfiguration* ToARConfiguration( UARSessionConfig* SessionConfig, FAppleARKitConfiguration& InConfiguration, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages )
{
	EARSessionType SessionType = SessionConfig->GetSessionType();
	ARConfiguration* SessionConfiguration = nullptr;
	switch (SessionType)
	{
		case EARSessionType::Orientation:
		{
			if (AROrientationTrackingConfiguration.isSupported == FALSE)
			{
				return nullptr;
			}
			SessionConfiguration = [AROrientationTrackingConfiguration new];
			break;
		}
		case EARSessionType::World:
		{
			if (ARWorldTrackingConfiguration.isSupported == FALSE)
			{
				return nullptr;
			}
			ARWorldTrackingConfiguration* WorldTrackingConfiguration = [ARWorldTrackingConfiguration new];
			WorldTrackingConfiguration.planeDetection = ARPlaneDetectionNone;
			if ( EnumHasAnyFlags(EARPlaneDetectionMode::HorizontalPlaneDetection, SessionConfig->GetPlaneDetectionMode()))
			{
				WorldTrackingConfiguration.planeDetection |= ARPlaneDetectionHorizontal;
			}
#if SUPPORTS_ARKIT_1_5
			if (FAppleARKitAvailability::SupportsARKit15())
			{
				if (EnumHasAnyFlags(EARPlaneDetectionMode::VerticalPlaneDetection, SessionConfig->GetPlaneDetectionMode()) )
				{
					WorldTrackingConfiguration.planeDetection |= ARPlaneDetectionVertical;
				}
				WorldTrackingConfiguration.autoFocusEnabled = SessionConfig->ShouldEnableAutoFocus();
				// Add any images that wish to be detected
				InitImageDetection(SessionConfig, WorldTrackingConfiguration, CandidateImages, ConvertedCandidateImages);
			}
#endif
			SessionConfiguration = WorldTrackingConfiguration;
			break;
		}
		case EARSessionType::Face:
		{
			if (ARFaceTrackingConfiguration.isSupported == FALSE)
			{
				return nullptr;
			}
			SessionConfiguration = [ARFaceTrackingConfiguration new];
			break;
		}
		default:
			return nullptr;
	}
	check(SessionConfiguration != nullptr);

	// Copy / convert properties
	SessionConfiguration.lightEstimationEnabled = InConfiguration.bLightEstimationEnabled;
	SessionConfiguration.providesAudioData = InConfiguration.bProvidesAudioData;
	SessionConfiguration.worldAlignment = ToARWorldAlignment(InConfiguration.Alignment);

    return SessionConfiguration;
}

#pragma clang diagnostic pop

#endif

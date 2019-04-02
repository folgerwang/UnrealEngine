// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitConversion.h"
#include "AppleARKitModule.h"

#if SUPPORTS_ARKIT_1_0
ARWorldAlignment FAppleARKitConversion::ToARWorldAlignment( const EARWorldAlignment& InWorldAlignment )
{
	switch ( InWorldAlignment )
	{
		case EARWorldAlignment::Gravity:
			return ARWorldAlignmentGravity;

		case EARWorldAlignment::GravityAndHeading:
			return ARWorldAlignmentGravityAndHeading;

		case EARWorldAlignment::Camera:
			return ARWorldAlignmentCamera;
	};
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

#if SUPPORTS_ARKIT_1_5

ARVideoFormat* FAppleARKitConversion::ToARVideoFormat(const FARVideoFormat& DesiredFormat, NSArray<ARVideoFormat*>* Formats)
{
	if (Formats != nullptr)
	{
		for (ARVideoFormat* Format in Formats)
		{
			if (Format != nullptr &&
				DesiredFormat.FPS == Format.framesPerSecond &&
				DesiredFormat.Width == Format.imageResolution.width &&
				DesiredFormat.Height == Format.imageResolution.height)
			{
				return Format;
			}
		}
	}
	return nullptr;
}

FARVideoFormat FAppleARKitConversion::FromARVideoFormat(ARVideoFormat* Format)
{
	FARVideoFormat ConvertedFormat;
	if (Format != nullptr)
	{
		ConvertedFormat.FPS = Format.framesPerSecond;
		ConvertedFormat.Width = Format.imageResolution.width;
		ConvertedFormat.Height = Format.imageResolution.height;
	}
	return ConvertedFormat;
}

TArray<FARVideoFormat> FAppleARKitConversion::FromARVideoFormatArray(NSArray<ARVideoFormat*>* Formats)
{
	TArray<FARVideoFormat> ConvertedArray;
	if (Formats != nullptr)
	{
		for (ARVideoFormat* Format in Formats)
		{
			if (Format != nullptr)
			{
				ConvertedArray.Add(FromARVideoFormat(Format));
			}
		}
	}
	return ConvertedArray;
}

NSSet* FAppleARKitConversion::InitImageDetection(UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
{
	const TArray<UARCandidateImage*>& ConfigCandidateImages = SessionConfig->GetCandidateImageList();
	if (!ConfigCandidateImages.Num())
	{
		return nullptr;
	}

	NSMutableSet* ConvertedImageSet = [[NSMutableSet new] autorelease];
	for (UARCandidateImage* Candidate : ConfigCandidateImages)
	{
		if (Candidate != nullptr && Candidate->GetCandidateTexture() != nullptr)
		{
			// Don't crash if the physical size is invalid
			if (Candidate->GetPhysicalWidth() <= 0.f || Candidate->GetPhysicalHeight() <= 0.f)
			{
				UE_LOG(LogAppleARKit, Error, TEXT("Unable to process candidate image (%s - %s) due to an invalid physical size (%f,%f)"),
				   *Candidate->GetFriendlyName(), *Candidate->GetName(), Candidate->GetPhysicalWidth(), Candidate->GetPhysicalHeight());
				continue;
			}
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
				ARReferenceImage* ReferenceImage = [[[ARReferenceImage alloc] initWithCGImage: ConvertedImage orientation: kCGImagePropertyOrientationUp physicalWidth: ImageWidth] autorelease];
				ReferenceImage.name = Candidate->GetFriendlyName().GetNSString();
				[ConvertedImageSet addObject: ReferenceImage];
			}
		}
	}
	return ConvertedImageSet;
}

void FAppleARKitConversion::InitImageDetection(UARSessionConfig* SessionConfig, ARWorldTrackingConfiguration* WorldConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
{
	if (FAppleARKitAvailability::SupportsARKit15())
	{
		WorldConfig.detectionImages = InitImageDetection(SessionConfig, CandidateImages, ConvertedCandidateImages);
	}
#if SUPPORTS_ARKIT_2_0
	if (FAppleARKitAvailability::SupportsARKit20())
	{
		WorldConfig.maximumNumberOfTrackedImages = SessionConfig->GetMaxNumSimultaneousImagesTracked();
	}
#endif
}
#endif

#if SUPPORTS_ARKIT_2_0
void FAppleARKitConversion::InitImageDetection(UARSessionConfig* SessionConfig, ARImageTrackingConfiguration* ImageConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
{
	ImageConfig.trackingImages = InitImageDetection(SessionConfig, CandidateImages, ConvertedCandidateImages);
	ImageConfig.maximumNumberOfTrackedImages = SessionConfig->GetMaxNumSimultaneousImagesTracked();
	ImageConfig.autoFocusEnabled = SessionConfig->ShouldEnableAutoFocus();
}

AREnvironmentTexturing FAppleARKitConversion::ToAREnvironmentTexturing(EAREnvironmentCaptureProbeType CaptureType)
{
	switch (CaptureType)
	{
		case EAREnvironmentCaptureProbeType::Manual:
		{
			return AREnvironmentTexturingManual;
		}
		case EAREnvironmentCaptureProbeType::Automatic:
		{
			return AREnvironmentTexturingAutomatic;
		}
	}
	return AREnvironmentTexturingNone;
}

ARWorldMap* FAppleARKitConversion::ToARWorldMap(const TArray<uint8>& WorldMapData)
{
	uint8* Buffer = (uint8*)WorldMapData.GetData();
	FARWorldSaveHeader InHeader(Buffer);
	// Check for our format and reject if invalid
	if (InHeader.Magic != AR_SAVE_WORLD_KEY || InHeader.Version != AR_SAVE_WORLD_VER)
	{
		UE_LOG(LogAppleARKit, Log, TEXT("Failed to load the world map data from the session object due to incompatible versions: magic (0x%x), ver(%d)"), InHeader.Magic, (uint32)InHeader.Version);
		return nullptr;
	}

	// Decompress the data
	uint8* CompressedData = Buffer + AR_SAVE_WORLD_HEADER_SIZE;
	uint32 CompressedSize = WorldMapData.Num() - AR_SAVE_WORLD_HEADER_SIZE;
	uint32 UncompressedSize = InHeader.UncompressedSize;
	TArray<uint8> UncompressedData;
	UncompressedData.AddUninitialized(UncompressedSize);
	if (!FCompression::UncompressMemory(NAME_Zlib, UncompressedData.GetData(), UncompressedSize, CompressedData, CompressedSize))
	{
		UE_LOG(LogAppleARKit, Log, TEXT("Failed to load the world map data from the session object due to a decompression error"));
		return nullptr;
	}
	
	// Serialize into the World map data
	NSData* WorldNSData = [NSData dataWithBytesNoCopy: UncompressedData.GetData() length: UncompressedData.Num() freeWhenDone: NO];
	NSError* ErrorObj = nullptr;
	ARWorldMap* WorldMap = [NSKeyedUnarchiver unarchivedObjectOfClass: ARWorldMap.class fromData: WorldNSData error: &ErrorObj];
	if (ErrorObj != nullptr)
	{
		FString Error = [ErrorObj localizedDescription];
		UE_LOG(LogAppleARKit, Log, TEXT("Failed to load the world map data from the session object with error string (%s)"), *Error);
	}
	return WorldMap;
}

NSSet* FAppleARKitConversion::ToARReferenceObjectSet(const TArray<UARCandidateObject*>& CandidateObjects, TMap< FString, UARCandidateObject* >& CandidateObjectMap)
{
	CandidateObjectMap.Empty();

	if (CandidateObjects.Num() == 0)
	{
		return nullptr;
	}

	NSMutableSet* ConvertedObjectSet = [[NSMutableSet new] autorelease];
	for (UARCandidateObject* Candidate : CandidateObjects)
	{
		if (Candidate != nullptr && Candidate->GetCandidateObjectData().Num() > 0)
		{
			NSData* CandidateData = [NSData dataWithBytesNoCopy: (uint8*)Candidate->GetCandidateObjectData().GetData() length: Candidate->GetCandidateObjectData().Num() freeWhenDone: NO];
			NSError* ErrorObj = nullptr;
			ARReferenceObject* RefObject = [NSKeyedUnarchiver unarchivedObjectOfClass: ARReferenceObject.class fromData: CandidateData error: &ErrorObj];
			if (RefObject != nullptr)
			{
				// Store off so the session object can quickly match the anchor to our representation
				// This stores it even if we weren't able to convert to apple's type for GC reasons
				CandidateObjectMap.Add(Candidate->GetFriendlyName(), Candidate);
				RefObject.name = Candidate->GetFriendlyName().GetNSString();
				[ConvertedObjectSet addObject: RefObject];
			}
			else
			{
				UE_LOG(LogAppleARKit, Log, TEXT("Failed to convert to ARReferenceObject (%s)"), *Candidate->GetFriendlyName());
			}
		}
		else
		{
			UE_LOG(LogAppleARKit, Log, TEXT("Missing candidate object data for ARCandidateObject (%s)"), Candidate != nullptr ? *Candidate->GetFriendlyName() : TEXT("null"));
		}
	}
	return ConvertedObjectSet;
}
#endif

ARConfiguration* FAppleARKitConversion::ToARConfiguration( UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages, TMap< FString, UARCandidateObject* >& CandidateObjects )
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
				FAppleARKitConversion::InitImageDetection(SessionConfig, WorldTrackingConfiguration, CandidateImages, ConvertedCandidateImages);
				ARVideoFormat* Format = FAppleARKitConversion::ToARVideoFormat(SessionConfig->GetDesiredVideoFormat(), ARWorldTrackingConfiguration.supportedVideoFormats);
				if (Format != nullptr)
				{
					WorldTrackingConfiguration.videoFormat = Format;
				}
			}
#endif
#if SUPPORTS_ARKIT_2_0
			if (FAppleARKitAvailability::SupportsARKit20())
			{
				// Check for environment capture probe types
				WorldTrackingConfiguration.environmentTexturing = ToAREnvironmentTexturing(SessionConfig->GetEnvironmentCaptureProbeType());
				// Load the world if requested
				if (SessionConfig->GetWorldMapData().Num() > 0)
				{
					ARWorldMap* WorldMap = ToARWorldMap(SessionConfig->GetWorldMapData());
					WorldTrackingConfiguration.initialWorldMap = WorldMap;
					[WorldMap release];
				}
				// Convert any candidate objects that are to be detected
				WorldTrackingConfiguration.detectionObjects = ToARReferenceObjectSet(SessionConfig->GetCandidateObjectList(), CandidateObjects);
			}
#endif
			SessionConfiguration = WorldTrackingConfiguration;
			break;
		}
		case EARSessionType::Image:
		{
#if SUPPORTS_ARKIT_2_0
			if (FAppleARKitAvailability::SupportsARKit20())
			{
				if (ARImageTrackingConfiguration.isSupported == FALSE)
				{
					return nullptr;
				}
				ARImageTrackingConfiguration* ImageTrackingConfiguration = [ARImageTrackingConfiguration new];
				// Add any images that wish to be detected
				InitImageDetection(SessionConfig, ImageTrackingConfiguration, CandidateImages, ConvertedCandidateImages);
				SessionConfiguration = ImageTrackingConfiguration;
			}
#endif
			break;
		}
		case EARSessionType::ObjectScanning:
		{
#if SUPPORTS_ARKIT_2_0
			if (FAppleARKitAvailability::SupportsARKit20())
			{
				if (ARObjectScanningConfiguration.isSupported == FALSE)
				{
					return nullptr;
				}
				ARObjectScanningConfiguration* ObjectScanningConfiguration = [ARObjectScanningConfiguration new];
				if (EnumHasAnyFlags(EARPlaneDetectionMode::HorizontalPlaneDetection, SessionConfig->GetPlaneDetectionMode()))
				{
					ObjectScanningConfiguration.planeDetection |= ARPlaneDetectionHorizontal;
				}
				if (EnumHasAnyFlags(EARPlaneDetectionMode::VerticalPlaneDetection, SessionConfig->GetPlaneDetectionMode()))
				{
					ObjectScanningConfiguration.planeDetection |= ARPlaneDetectionVertical;
				}
				ObjectScanningConfiguration.autoFocusEnabled = SessionConfig->ShouldEnableAutoFocus();
				SessionConfiguration = ObjectScanningConfiguration;
			}
#endif
			break;
		}
		default:
			return nullptr;
	}
    if (SessionConfiguration != nullptr)
    {
        // Copy / convert properties
        SessionConfiguration.lightEstimationEnabled = SessionConfig->GetLightEstimationMode() != EARLightEstimationMode::None;
        SessionConfiguration.providesAudioData = NO;
        SessionConfiguration.worldAlignment = FAppleARKitConversion::ToARWorldAlignment(SessionConfig->GetWorldAlignment());
    }
    
    return SessionConfiguration;
}

#pragma clang diagnostic pop
#endif

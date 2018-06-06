// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Transform.h"
#include "ARPin.h"

#include "IAppleImageUtilsPlugin.h"

struct FAppleARKitConversion
{
	static FORCEINLINE float ToUE4Scale()
	{
		return 100.f;
	}

	static FORCEINLINE float ToARKitScale()
	{
		return 0.01f;
	}

#if SUPPORTS_ARKIT_1_0
	/**
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system transform to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 *
	 * Ignores scale.
	 */
	static FORCEINLINE FTransform ToFTransform(const matrix_float4x4& RawYUpMatrix)
	{
		// Conversion here is as per SteamVRHMD::ToFMatrix
		FMatrix RawYUpFMatrix(
			FPlane(RawYUpMatrix.columns[0][0], RawYUpMatrix.columns[0][1], RawYUpMatrix.columns[0][2], RawYUpMatrix.columns[0][3]),
			FPlane(RawYUpMatrix.columns[1][0], RawYUpMatrix.columns[1][1], RawYUpMatrix.columns[1][2], RawYUpMatrix.columns[1][3]),
			FPlane(RawYUpMatrix.columns[2][0], RawYUpMatrix.columns[2][1], RawYUpMatrix.columns[2][2], RawYUpMatrix.columns[2][3]),
			FPlane(RawYUpMatrix.columns[3][0], RawYUpMatrix.columns[3][1], RawYUpMatrix.columns[3][2], RawYUpMatrix.columns[3][3]));

		// Extract & convert translation
		FVector Translation = FVector( -RawYUpFMatrix.M[3][2], RawYUpFMatrix.M[3][0], RawYUpFMatrix.M[3][1] ) * ToUE4Scale();

		// Extract & convert rotation 
		FQuat RawRotation( RawYUpFMatrix );
		FQuat Rotation( -RawRotation.Z, RawRotation.X, RawRotation.Y, -RawRotation.W );

		return FTransform( Rotation, Translation );
	}

    /**
     * Convert's an Unreal 'Z up' transform to ARKit's 'Y up' 'right handed' coordinate system
     * 'left handed' coordinate system.
     *
     * Ignores scale.
     */
    static FORCEINLINE matrix_float4x4 ToARKitMatrix( const FTransform& InTransform, float WorldToMetersScale = 100.0f )
    {
		if(!ensure(WorldToMetersScale != 0.f))
		{
			WorldToMetersScale = 100.f;
		}

        matrix_float4x4 RetVal;

        const FVector   Translation = InTransform.GetLocation() / WorldToMetersScale;
        const FQuat     UnrealRotation = InTransform.GetRotation();
        const FQuat     ARKitRotation = FQuat(UnrealRotation.Y, UnrealRotation.Z, -UnrealRotation.X, UnrealRotation.W);

        const FMatrix   UnrealRotationMatrix = FRotationMatrix::Make(ARKitRotation);

        RetVal.columns[0][0] = UnrealRotationMatrix.M[0][0]; RetVal.columns[0][1] = UnrealRotationMatrix.M[0][1]; RetVal.columns[0][2] = -UnrealRotationMatrix.M[0][2]; RetVal.columns[0][3] = UnrealRotationMatrix.M[0][3];
        RetVal.columns[1][0] = UnrealRotationMatrix.M[1][0]; RetVal.columns[1][1] = UnrealRotationMatrix.M[1][1]; RetVal.columns[1][2] = UnrealRotationMatrix.M[1][2]; RetVal.columns[1][3] = UnrealRotationMatrix.M[1][3];
        RetVal.columns[2][0] = -UnrealRotationMatrix.M[2][0]; RetVal.columns[2][1] = UnrealRotationMatrix.M[2][1]; RetVal.columns[2][2] = UnrealRotationMatrix.M[2][2]; RetVal.columns[2][3] = UnrealRotationMatrix.M[2][3];
        RetVal.columns[3][0] = UnrealRotationMatrix.M[3][0]; RetVal.columns[3][1] = UnrealRotationMatrix.M[3][1]; RetVal.columns[3][2] = UnrealRotationMatrix.M[3][2]; RetVal.columns[3][3] = UnrealRotationMatrix.M[3][3];

        // Set the translation element
        RetVal.columns[3][2] = -Translation.X;
        RetVal.columns[3][0] = Translation.Y;
        RetVal.columns[3][1] = Translation.Z;

        return RetVal;
    }

	/**
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system vector to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 */
	static FORCEINLINE FVector ToFVector(const vector_float3& RawYUpVector)
	{
		return FVector( -RawYUpVector.z, RawYUpVector.x, RawYUpVector.y ) * ToUE4Scale();
	}

    /**
     * Convert's an Unreal's 'Z up' to ARKit's 'Y up' vector
     * 'left handed' coordinate system.
     */
    static FORCEINLINE vector_float3 ToARKitVector( const FVector& InFVector, float WorldToMetersScale = 100.0f )
    {
		if(!ensure(WorldToMetersScale != 0.f))
		{
			WorldToMetersScale = 100.f;
		}

        vector_float3 RetVal;
        RetVal.x = InFVector.Y;
        RetVal.y = InFVector.Z;
        RetVal.z = -InFVector.X;

        return RetVal / WorldToMetersScale;
    }

	static FORCEINLINE FGuid ToFGuid( uuid_t UUID )
	{
		FGuid AsGUID(
			*(uint32*)UUID,
			*((uint32*)UUID)+1,
			*((uint32*)UUID)+2,
			*((uint32*)UUID)+3);
		return AsGUID;
	}

	static FORCEINLINE FGuid ToFGuid( NSUUID* Identifier )
	{
		// Get bytes
		uuid_t UUID;
		[Identifier getUUIDBytes:UUID];

		// Set FGuid parts
		return ToFGuid( UUID );
	}

	static FORCEINLINE ARWorldAlignment ToARWorldAlignment( const EARWorldAlignment& InWorldAlignment )
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

	static NSSet* InitImageDetection(UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
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
			}
		}
		return ConvertedImageSet;
	}

	static void InitImageDetection(UARSessionConfig* SessionConfig, ARWorldTrackingConfiguration* WorldConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
	{
		if (FAppleARKitAvailability::SupportsARKit15())
		{
			WorldConfig.detectionImages = InitImageDetection(SessionConfig, CandidateImages, ConvertedCandidateImages);
		}
	//@joeg -- Added image tracking support
	#if SUPPORTS_ARKIT_2_0
		if (FAppleARKitAvailability::SupportsARKit20())
		{
			WorldConfig.maximumNumberOfTrackedImages = SessionConfig->GetMaxNumSimultaneousImagesTracked();
		}
	#endif
	}
	#endif

	//@joeg -- Added image tracking support
	#if SUPPORTS_ARKIT_2_0
	static void InitImageDetection(UARSessionConfig* SessionConfig, ARImageTrackingConfiguration* ImageConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
	{
		ImageConfig.trackingImages = InitImageDetection(SessionConfig, CandidateImages, ConvertedCandidateImages);
		ImageConfig.maximumNumberOfTrackedImages = SessionConfig->GetMaxNumSimultaneousImagesTracked();
		ImageConfig.autoFocusEnabled = SessionConfig->ShouldEnableAutoFocus();
	}

	//@joeg -- Added environmental texture probe support
	static AREnvironmentTexturing ToAREnvironmentTexturing(EAREnvironmentCaptureProbeType CaptureType)
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

	static ARWorldMap* ToARWorldMap(const TArray<uint8>& WorldMapData)
	{
		NSData* WorldNSData = [NSData dataWithBytesNoCopy: (uint8*)WorldMapData.GetData() length: WorldMapData.Num() freeWhenDone: NO];
		ARWorldMap* WorldMap = [NSKeyedUnarchiver unarchiveObjectWithData: WorldNSData];
		if (WorldMap == nullptr)
		{
//@todo joeg -- Make a way to log from here
//			UE_LOG(LogAppleARKit, Log, TEXT("Failed to load the world map data from the session object"));
		}
		return WorldMap;
	}

	static NSSet* ToARReferenceObjectSet(const TArray<UARCandidateObject*>& CandidateObjects, TMap< FString, UARCandidateObject* >& CandidateObjectMap)
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
				ARReferenceObject* RefObject = [NSKeyedUnarchiver unarchiveObjectWithData: CandidateData];
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
//@todo joeg -- Make a way to log from here
//					UE_LOG(LogAppleARKit, Log, TEXT("Failed to convert to ARReferenceObject (%s)"), *Candidate->GetFriendlyName());
				}
			}
			else
			{
//@todo joeg -- Make a way to log from here
//				UE_LOG(LogAppleARKit, Log, TEXT("Missing candidate object data for ARCandidateObject (%s)"), Candidate != nullptr ? *Candidate->GetFriendlyName() : TEXT("null"));
			}
		}
		return ConvertedObjectSet;
	}
	#endif

//@joeg -- Object detection addition
	static ARConfiguration* ToARConfiguration( UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages, TMap< FString, UARCandidateObject* >& CandidateObjects )
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
				}
#endif
//@joeg -- Added environmental texture probe support
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
//@joeg -- Added image tracking support
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
//@joeg -- Added object scanning support
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
		check(SessionConfiguration != nullptr);

		// Copy / convert properties
		SessionConfiguration.lightEstimationEnabled = SessionConfig->GetLightEstimationMode() != EARLightEstimationMode::None;
		SessionConfiguration.providesAudioData = NO;
		SessionConfiguration.worldAlignment = FAppleARKitConversion::ToARWorldAlignment(SessionConfig->GetWorldAlignment());

		return SessionConfiguration;
	}

	#pragma clang diagnostic pop
#endif
};

#if SUPPORTS_ARKIT_1_0
enum class EAppleAnchorType : uint8
{
	Anchor,
	PlaneAnchor,
	FaceAnchor,
	ImageAnchor,
//@joeg -- Added environmental texture probe support
	EnvironmentProbeAnchor,
//@joeg -- Object detection
	ObjectAnchor,
	MAX
};

struct FAppleARKitAnchorData
{
	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::Anchor )
		, AnchorGUID( InAnchorGuid )
	{
	}

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FVector InCenter, FVector InExtent)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::PlaneAnchor )
		, AnchorGUID( InAnchorGuid )
		, Center(InCenter)
		, Extent(InExtent)
	{
	}

//@joeg -- Eye tracking support
	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FARBlendShapeMap InBlendShapes, TArray<FVector> InFaceVerts, FTransform InLeftEyeTransform, FTransform InRightEyeTransform, FVector InLookAtTarget)
	: Transform( InTransform )
	, AnchorType( EAppleAnchorType::FaceAnchor )
	, AnchorGUID( InAnchorGuid )
	, BlendShapes( MoveTemp(InBlendShapes) )
	, FaceVerts( MoveTemp(InFaceVerts) )
	, LeftEyeTransform( InLeftEyeTransform )
	, RightEyeTransform( InRightEyeTransform )
	, LookAtTarget( InLookAtTarget )
	{
	}

//@joeg -- Changed so object and image detection share
	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, EAppleAnchorType InAnchorType, FString InDetectedAnchorName)
    : Transform( InTransform )
    , AnchorType( InAnchorType )
    , AnchorGUID( InAnchorGuid )
    , DetectedAnchorName( MoveTemp(InDetectedAnchorName) )
    {
    }

//@joeg -- Added environmental texture probe support
	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FVector InExtent, id<MTLTexture> InProbeTexture)
	: Transform( InTransform )
	, AnchorType( EAppleAnchorType::EnvironmentProbeAnchor )
	, AnchorGUID( InAnchorGuid )
	, Extent(InExtent)
	, ProbeTexture(InProbeTexture)
	{
	}

	FTransform Transform;
	EAppleAnchorType AnchorType;
	FGuid AnchorGUID;
	FVector Center;
	FVector Extent;
	TArray<FVector> BoundaryVerts;

	FARBlendShapeMap BlendShapes;
	TArray<FVector> FaceVerts;
	// Note: the index buffer never changes so can be safely read once
	static TArray<int32> FaceIndices;

//@joeg -- Changed this so both Image and Object anchors can share
	FString DetectedAnchorName;

//@joeg -- Environment texturing support
	id<MTLTexture> ProbeTexture;

//@joeg -- Eye tracking support
	FTransform LeftEyeTransform;
	FTransform RightEyeTransform;
	FVector LookAtTarget;
};
#endif

namespace ARKitUtil
{
	static UARPin* PinFromComponent( const USceneComponent* Component, const TArray<UARPin*>& InPins )
	{
		for (UARPin* Pin : InPins)
		{
			if (Pin->GetPinnedComponent() == Component)
			{
				return Pin;
			}
		}

		return nullptr;
	}

	static TArray<UARPin*> PinsFromGeometry( const UARTrackedGeometry* Geometry, const TArray<UARPin*>& InPins )
	{
		TArray<UARPin*> OutPins;
		for (UARPin* Pin : InPins)
		{
			if (Pin->GetTrackedGeometry() == Geometry)
			{
				OutPins.Add(Pin);
			}
		}

		return OutPins;
	}
}

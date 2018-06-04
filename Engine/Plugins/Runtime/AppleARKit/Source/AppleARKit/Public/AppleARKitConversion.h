// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// UE4
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
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system vector to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 */
	static FORCEINLINE FVector ToFVector(const vector_float3& RawYUpVector)
	{
		return FVector( -RawYUpVector.z, RawYUpVector.x, RawYUpVector.y ) * ToUE4Scale();
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

	static void InitImageDetection(UARSessionConfig* SessionConfig, ARWorldTrackingConfiguration* WorldConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages)
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
			}
		}
		WorldConfig.detectionImages = ConvertedImageSet;
	}

	#endif

	static ARConfiguration* ToARConfiguration( UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages )
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
				SessionConfiguration = WorldTrackingConfiguration;
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

enum class EAppleAnchorType : uint8
{
	Anchor,
	PlaneAnchor,
	FaceAnchor,
	ImageAnchor,
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

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FString InDetectedImageName)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::ImageAnchor )
		, AnchorGUID( InAnchorGuid )
		, DetectedImageName( MoveTemp(InDetectedImageName) )
	{
	}

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FARBlendShapeMap InBlendShapes, TArray<FVector> InFaceVerts)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::FaceAnchor )
		, AnchorGUID( InAnchorGuid )
		, BlendShapes( MoveTemp(InBlendShapes) )
		, FaceVerts( MoveTemp(InFaceVerts) )
	{
	}

	FTransform Transform;
	EAppleAnchorType AnchorType;
	FGuid AnchorGUID;
	FVector Center;
	FVector Extent;
	TArray<FVector> BoundaryVerts;

	FString DetectedImageName;

	FARBlendShapeMap BlendShapes;
	TArray<FVector> FaceVerts;
	// Note: the index buffer never changes so can be safely read once
	static TArray<int32> FaceIndices;
};

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

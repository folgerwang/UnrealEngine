// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AppleARKitAvailability.h"
#include "Math/Transform.h"
#include "ARPin.h"
#include "Misc/Compression.h"
#include "ARTypes.h"
#include "ARTrackable.h"
#include "ARSessionConfig.h"
#include "Misc/Timecode.h"

#include "IAppleImageUtilsPlugin.h"

#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif

#define AR_SAVE_WORLD_KEY 0x505A474A
#define AR_SAVE_WORLD_VER 1

struct FARWorldSaveHeader
{
	uint32 Magic;
	uint32 UncompressedSize;
	uint8 Version;
	
	FARWorldSaveHeader() :
		Magic(AR_SAVE_WORLD_KEY),
		UncompressedSize(0),
		Version(AR_SAVE_WORLD_VER)
	{
		
	}

	FARWorldSaveHeader(uint8* Header)
	{
		const FARWorldSaveHeader& Other = *(FARWorldSaveHeader*)Header;
		Magic = Other.Magic;
		UncompressedSize = Other.UncompressedSize;
		Version = Other.Version;
	}
};

#define AR_SAVE_WORLD_HEADER_SIZE (sizeof(FARWorldSaveHeader))

struct APPLEARKIT_API FAppleARKitConversion
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
	static FORCEINLINE FTransform ToFTransform(const matrix_float4x4& RawYUpMatrix, const FRotator& AdjustBy = FRotator::ZeroRotator)
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
		if (!AdjustBy.IsNearlyZero())
		{
			Rotation = FQuat(AdjustBy) * Rotation;
		}

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

	static ARWorldAlignment ToARWorldAlignment( const EARWorldAlignment& InWorldAlignment );

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

#if SUPPORTS_ARKIT_1_5

	static ARVideoFormat* ToARVideoFormat(const FARVideoFormat& DesiredFormat, NSArray<ARVideoFormat*>* Formats);
	
	static FARVideoFormat FromARVideoFormat(ARVideoFormat* Format);
	
	static TArray<FARVideoFormat> FromARVideoFormatArray(NSArray<ARVideoFormat*>* Formats);

	static NSSet* InitImageDetection(UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages);

	static void InitImageDetection(UARSessionConfig* SessionConfig, ARWorldTrackingConfiguration* WorldConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages);
#endif

#if SUPPORTS_ARKIT_2_0
	static void InitImageDetection(UARSessionConfig* SessionConfig, ARImageTrackingConfiguration* ImageConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages);

	static AREnvironmentTexturing ToAREnvironmentTexturing(EAREnvironmentCaptureProbeType CaptureType);

	static ARWorldMap* ToARWorldMap(const TArray<uint8>& WorldMapData);

	static NSSet* ToARReferenceObjectSet(const TArray<UARCandidateObject*>& CandidateObjects, TMap< FString, UARCandidateObject* >& CandidateObjectMap);
#endif

	static ARConfiguration* ToARConfiguration( UARSessionConfig* SessionConfig, TMap< FString, UARCandidateImage* >& CandidateImages, TMap< FString, CGImageRef >& ConvertedCandidateImages, TMap< FString, UARCandidateObject* >& CandidateObjects );

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
	EnvironmentProbeAnchor,
	ObjectAnchor,
	MAX
};

struct FAppleARKitAnchorData
{
	FAppleARKitAnchorData()
		: AnchorType()
		, AnchorGUID(FGuid())
		, ProbeTexture(nullptr)
		, Timestamp(0.0)
		, FrameNumber(0)
		, bIsTracked(false)
	{
	}

	FAppleARKitAnchorData(const FAppleARKitAnchorData& Other)
	{
		Copy(Other);
	}

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::Anchor )
		, AnchorGUID( InAnchorGuid )
		, bIsTracked(false)
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

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FARBlendShapeMap InBlendShapes, TArray<FVector> InFaceVerts, FTransform InLeftEyeTransform, FTransform InRightEyeTransform, FVector InLookAtTarget, const FTimecode& InTimecode, uint32 InFrameRate)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::FaceAnchor )
		, AnchorGUID( InAnchorGuid )
		, BlendShapes( MoveTemp(InBlendShapes) )
		, FaceVerts( MoveTemp(InFaceVerts) )
		, LeftEyeTransform( InLeftEyeTransform )
		, RightEyeTransform( InRightEyeTransform )
		, LookAtTarget( InLookAtTarget )
		, Timecode( InTimecode )
		, FrameRate( InFrameRate )
	{
	}

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, EAppleAnchorType InAnchorType, FString InDetectedAnchorName)
		: Transform( InTransform )
		, AnchorType( InAnchorType )
		, AnchorGUID( InAnchorGuid )
		, DetectedAnchorName( MoveTemp(InDetectedAnchorName) )
    {
    }

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FVector InExtent, id<MTLTexture> InProbeTexture)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::EnvironmentProbeAnchor )
		, AnchorGUID( InAnchorGuid )
		, Extent(InExtent)
		, ProbeTexture(InProbeTexture)
	{
	}

	FAppleARKitAnchorData& operator=(const FAppleARKitAnchorData& Other)
	{
		if (this != &Other)
		{
			Clear();
			Copy(Other);
		}

		return *this;
	}

	void Copy(const FAppleARKitAnchorData& Other)
	{
		Transform = Other.Transform;
		AnchorType = Other.AnchorType;
		AnchorGUID = Other.AnchorGUID;
		Center = Other.Center;
		Extent = Other.Center;
		BoundaryVerts = Other.BoundaryVerts;

		BlendShapes = Other.BlendShapes;
		FaceVerts = Other.FaceVerts;

		DetectedAnchorName = Other.DetectedAnchorName;

		ProbeTexture = Other.ProbeTexture;

		LeftEyeTransform = Other.LeftEyeTransform;
		RightEyeTransform = Other.RightEyeTransform;
		LookAtTarget = Other.LookAtTarget;
		Timestamp = Other.Timestamp;
		FrameNumber = Other.FrameNumber;
		Timecode = Other.Timecode;
		FrameRate = Other.FrameRate;

		bIsTracked = Other.bIsTracked;
	}

	void Clear()
	{
		BoundaryVerts.Empty();
		BlendShapes.Empty();
		FaceVerts.Empty();
		ProbeTexture = nullptr;
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

	FString DetectedAnchorName;

	id<MTLTexture> ProbeTexture;

	FTransform LeftEyeTransform;
	FTransform RightEyeTransform;
	FVector LookAtTarget;
	double Timestamp;
	uint32 FrameNumber;
	FTimecode Timecode;
	uint32 FrameRate;

	/** Only valid for tracked real world objects (face, images) */
	bool bIsTracked;
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

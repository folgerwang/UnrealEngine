// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// UE4
#include "Math/Transform.h"
#include "ARPin.h"

//@todo -- Move this to UARSessionConfiguration
/**
 * Enum constants for indicating the world alignment.
 */
enum class EAppleARKitWorldAlignment : uint8
{
	/** Aligns the world with gravity that is defined by vector (0, -1, 0). */
	Gravity,

	/**
	 * Aligns the world with gravity that is defined by the vector (0, -1, 0)
	 * and heading (w.r.t. True North) that is given by the vector (0, 0, -1).
	 */
	GravityAndHeading,

	/** Aligns the world with the camera's orientation. */
	Camera
};
ENUM_CLASS_FLAGS(EAppleARKitWorldAlignment)

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

	static FORCEINLINE ARWorldAlignment ToARWorldAlignment( const EAppleARKitWorldAlignment& InWorldAlignment )
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

	FTransform Transform;
	EAppleAnchorType AnchorType;
	FGuid AnchorGUID;
	FVector Center;
	FVector Extent;
	TArray<FVector> BoundaryVerts;

	FString DetectedImageName;
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

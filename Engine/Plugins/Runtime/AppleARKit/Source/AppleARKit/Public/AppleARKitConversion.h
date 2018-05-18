// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// UE4
#include "Math/Transform.h"

struct FAppleARKitTransform
{

#if SUPPORTS_ARKIT_1_0

	/** 
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system transform to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 *
	 * Ignores scale.
	 */
	static FORCEINLINE FTransform ToFTransform( const matrix_float4x4& RawYUpMatrix, float WorldToMetersScale = 100.0f )
	{
		// Conversion here is as per SteamVRHMD::ToFMatrix
		FMatrix RawYUpFMatrix(
			FPlane(RawYUpMatrix.columns[0][0], RawYUpMatrix.columns[0][1], RawYUpMatrix.columns[0][2], RawYUpMatrix.columns[0][3]),
			FPlane(RawYUpMatrix.columns[1][0], RawYUpMatrix.columns[1][1], RawYUpMatrix.columns[1][2], RawYUpMatrix.columns[1][3]),
			FPlane(RawYUpMatrix.columns[2][0], RawYUpMatrix.columns[2][1], RawYUpMatrix.columns[2][2], RawYUpMatrix.columns[2][3]),
			FPlane(RawYUpMatrix.columns[3][0], RawYUpMatrix.columns[3][1], RawYUpMatrix.columns[3][2], RawYUpMatrix.columns[3][3]));

		// Extract & convert translation
		FVector Translation = FVector( -RawYUpFMatrix.M[3][2], RawYUpFMatrix.M[3][0], RawYUpFMatrix.M[3][1] ) * WorldToMetersScale;

		// Extract & convert rotation 
		FQuat RawRotation( RawYUpFMatrix );
		FQuat Rotation( -RawRotation.Z, RawRotation.X, RawRotation.Y, -RawRotation.W );

		return FTransform( Rotation, Translation );
	}

	/** 
	 * Convert's an ARKit 'Y up' 'right handed' coordinate system vector to Unreal's 'Z up' 
	 * 'left handed' coordinate system.
	 */
	static FORCEINLINE FVector ToFVector( const vector_float3& RawYUpVector, float WorldToMetersScale = 100.0f )
	{
		return FVector( -RawYUpVector.z, RawYUpVector.x, RawYUpVector.y ) * WorldToMetersScale;
	}

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

	FAppleARKitAnchorData(FGuid InAnchorGuid, FTransform InTransform, FARBlendShapeMap InBlendShapes, TArray<FVector> InFaceVerts)
		: Transform( InTransform )
		, AnchorType( EAppleAnchorType::FaceAnchor )
		, AnchorGUID( InAnchorGuid )
		, BlendShapes( MoveTemp(InBlendShapes) )
		, FaceVerts( MoveTemp(InFaceVerts) )
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
	FARBlendShapeMap BlendShapes;
	TArray<FVector> FaceVerts;
	// Temp non-static while code is rearranged
	TArray<int32> FaceIndices;
	// Note: the index buffer never changes so can be safely read once
//	static TArray<int32> FaceIndices;
//	TArray<int32> FAppleARKitAnchorData::FaceIndices;

	FString DetectedImageName;
};

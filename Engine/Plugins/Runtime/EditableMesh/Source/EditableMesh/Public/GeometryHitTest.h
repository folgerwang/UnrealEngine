// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "EditableMesh.h"
#include "MeshElement.h"
#include "MeshEditorInteractorData.h"

class UEditableMesh;
class UPrimitiveComponent;

struct FHitParamsIn
{
	FHitParamsIn(UPrimitiveComponent* HitComponentIn,
	const FTransform& CameraToWorldIn,
	bool bIsPerspectiveViewIn,
	float ComponentSpaceFuzzyDistanceScaleFactorIn,
	const FMatrix &ComponentToWorldMatrixIn,
	FMeshEditorInteractorData &MeshEditorInteractorDataIn,
	UEditableMesh* EditableMeshIn,
	const EInteractorShape InteractorShapeIn,
	const float ComponentSpaceGrabberSphereFuzzyDistanceIn,
	const float ComponentSpaceRayFuzzyDistanceIn,
	EEditableMeshElementType OnlyElementTypeIn
	) : HitComponent(HitComponentIn),
		CameraToWorld(CameraToWorldIn),
		bIsPerspectiveView(bIsPerspectiveViewIn),
		ComponentSpaceFuzzyDistanceScaleFactor(ComponentSpaceFuzzyDistanceScaleFactorIn),
		ComponentToWorldMatrix(ComponentToWorldMatrixIn),
		MeshEditorInteractorData(MeshEditorInteractorDataIn),
		EditableMesh(EditableMeshIn),
		InteractorShape(InteractorShapeIn),
		ComponentSpaceGrabberSphereFuzzyDistance(ComponentSpaceGrabberSphereFuzzyDistanceIn),
		ComponentSpaceRayFuzzyDistance(ComponentSpaceRayFuzzyDistanceIn),
		OnlyElementType(OnlyElementTypeIn)
	{}

	UPrimitiveComponent* HitComponent;
	const FTransform& CameraToWorld;
	bool bIsPerspectiveView;
	float ComponentSpaceFuzzyDistanceScaleFactor;
	const FMatrix &ComponentToWorldMatrix;
	FMeshEditorInteractorData &MeshEditorInteractorData;
	UEditableMesh* EditableMesh;
	const EInteractorShape InteractorShape;
	const float ComponentSpaceGrabberSphereFuzzyDistance;
	const float ComponentSpaceRayFuzzyDistance;
	EEditableMeshElementType OnlyElementType;
};

struct FHitParamsOut
{
	FHitParamsOut(FVector& ClosestHoverLocationIn,
	UPrimitiveComponent* ClosestComponentIn,
	FEditableMeshElementAddress& ClosestElementAddressIn,
	EInteractorShape& ClosestInteractorShapeIn)
		: ClosestHoverLocation(ClosestHoverLocationIn)
		, ClosestComponent(ClosestComponentIn)
		, ClosestElementAddress(ClosestElementAddressIn)
		, ClosestInteractorShape(ClosestInteractorShapeIn) {}

	FVector& ClosestHoverLocation;
	UPrimitiveComponent* ClosestComponent;
	FEditableMeshElementAddress& ClosestElementAddress;
	EInteractorShape& ClosestInteractorShape;
};

class FGeometryTests
{
public:
	///** Geometry tests */
	static FEditableMeshElementAddress QueryElement( const UEditableMesh& EditableMesh, const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float RayFuzzyDistance, const EEditableMeshElementType OnlyElementType, const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& OutInteractorShape, FVector& OutHitLocation, int32 DesiredPolygonGroup = -1);

	static bool CheckVertex( const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float FuzzyDistance, const FVector& VertexPosition, const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitVertex );

	static bool CheckEdge( const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float FuzzyDistance, const FVector EdgeVertexPositions[ 2 ], const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyEdge );

	static bool CheckTriangle( const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float FuzzyDistance, const FVector TriangleVertexPositions[ 3 ], const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitTriangle );

};

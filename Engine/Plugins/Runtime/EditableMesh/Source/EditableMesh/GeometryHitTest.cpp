// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryHitTest.h"
#include "EditableMesh.h"
#include "MeshAttributes.h"
#include "MeshElement.h"

namespace GeometryTest
{
	static FAutoConsoleVariable OverlayPerspectiveDistanceBias(TEXT("MeshEd.OverlayPerspectiveDistanceBias"), 0.05f, TEXT("How much to bias distance scale by in perspective views, regardless of distance to the viewer"));
	static FAutoConsoleVariable OverlayOrthographicDistanceBias(TEXT("MeshEd.OverlayOrthographicDistanceBias"), 1.0f, TEXT("How much to bias distance scale by in orthograph views, regardless of distance to the viewer"));
}



FEditableMeshElementAddress FGeometryTests::QueryElement(const UEditableMesh& EditableMesh, const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float RayFuzzyDistance, const EEditableMeshElementType OnlyElementType, const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& OutInteractorShape, FVector& OutHitLocation, int32 DesiredPolygonGroup)
{
	OutHitLocation = FVector::ZeroVector;

	FEditableMeshElementAddress HitElementAddress;
	HitElementAddress.SubMeshAddress = EditableMesh.GetSubMeshAddress();


	// Figure out our candidate set of polygons by performing a spatial query on the mesh
	static TArray<FPolygonID> CandidatePolygons;
	CandidatePolygons.Reset();

	if (InteractorShape == EInteractorShape::Laser)
	{
		// @todo mesheditor spatial: Do we need to use a "fat ray" to account for fuzzy testing (or expanded octree boxes)?  We don't currently have a 'segment distance to AABB' function.

		check(EditableMesh.IsSpatialDatabaseAllowed());	// We need a spatial database to do this query fast!
		EditableMesh.SearchSpatialDatabaseForPolygonsPotentiallyIntersectingLineSegment(RayStart, RayEnd, /* Out */ CandidatePolygons);

		// @todo mesheditor debug
		// 	if( EditableMesh.GetPolygonCount() > 0 )
		// 	{
		// 		UE_LOG( LogEditableMesh, Display, TEXT( "%i  (%0.1f%%)" ), CandidatePolygons.Num(), ( (float)CandidatePolygons.Num() / (float)EditableMesh.GetPolygonCount() ) * 100.0f );
		// 	}
	}
	else
	{
		// @todo mesheditor spatial: Need GrabberSphere support for spatial queries.  Currently we're just testing all polygons (slow!)
		for (const FPolygonID PolygonID : EditableMesh.GetMeshDescription()->Polygons().GetElementIDs())
		{
			CandidatePolygons.Add(PolygonID);
		}
	}

	static TSet<FVertexID> FrontFacingVertices;
	FrontFacingVertices.Reset();

	static TSet<FEdgeID> FrontFacingEdges;
	FrontFacingEdges.Reset();

	static TSet<FPolygonID> FrontFacingPolygons;
	FrontFacingPolygons.Reset();

	TPolygonAttributesConstRef<FVector> PolygonCenters = EditableMesh.GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Center);

	// Look for all the front-facing elements
	for (const FPolygonID PolygonID : CandidatePolygons)
	{
		if (DesiredPolygonGroup >= 0 && EditableMesh.GetMeshDescription()->GetPolygonPolygonGroup(PolygonID).GetValue() != DesiredPolygonGroup)
			continue;

		const FVector PolygonNormal = EditableMesh.ComputePolygonNormal(PolygonID);
		const FVector PolygonCenter = PolygonCenters[PolygonID];
		if ((InteractorShape == EInteractorShape::GrabberSphere) ||	// Sphere tests never eliminate back-facing geometry
			!bIsPerspectiveView ||			// @todo mesheditor: Add support for backface culling in orthographic views
			FVector::DotProduct(CameraLocation - PolygonCenter, PolygonNormal) > 0.0f)
		{
			FrontFacingPolygons.Add(PolygonID);

			const int32 PolygonVertexCount = EditableMesh.GetPolygonPerimeterVertexCount(PolygonID);
			for (int32 Index = 0; Index < PolygonVertexCount; ++Index)
			{
				FrontFacingVertices.Add(EditableMesh.GetPolygonPerimeterVertex(PolygonID, Index));
				bool bOutEdgeWindingIsReversedForPolygons;
				FrontFacingEdges.Add(EditableMesh.GetPolygonPerimeterEdge(PolygonID, Index, bOutEdgeWindingIsReversedForPolygons));
			}
		}
	}

	EInteractorShape ClosestInteractorShape = EInteractorShape::Invalid;
	FVector ClosestHitLocation = FVector::ZeroVector;
	float ClosestDistanceOnRay = TNumericLimits<float>::Max();
	float ClosestDistanceToRay = TNumericLimits<float>::Max();
	FVector CurrentRayEnd = RayEnd;

	const FMeshDescription* MeshDescription = EditableMesh.GetMeshDescription();
	TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	// Check polygons first; this is so we always impose a closest hit location at the poly before checking other elements, so anything behind is occluded
	for (const FPolygonID PolygonID : FrontFacingPolygons)
	{
		static TArray<FVertexID> MeshVertexIDs;
		MeshVertexIDs.Reset();
		EditableMesh.GetPolygonPerimeterVertices(PolygonID, /* Out */ MeshVertexIDs);

		const uint32 PolygonTriangleCount = EditableMesh.GetPolygonTriangulatedTriangleCount(PolygonID);
		for (uint32 PolygonTriangleNumber = 0; PolygonTriangleNumber < PolygonTriangleCount; ++PolygonTriangleNumber)
		{
			FVector TriangleVertexPositions[3];
			for (uint32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber)
			{
				const FVertexInstanceID VertexInstanceID = EditableMesh.GetPolygonTriangulatedTriangle(PolygonID, PolygonTriangleNumber).GetVertexInstanceID(TriangleVertexNumber);
				const FVertexID VertexID = EditableMesh.GetVertexInstanceVertex(VertexInstanceID);
				TriangleVertexPositions[TriangleVertexNumber] = VertexPositions[VertexID];
			}

			const bool bAlreadyHitTriangle = (HitElementAddress.ElementType == EEditableMeshElementType::Polygon);
			const bool bHit = CheckTriangle(InteractorShape, Sphere, SphereFuzzyDistance, RayStart, CurrentRayEnd, RayFuzzyDistance, TriangleVertexPositions, CameraLocation, bIsPerspectiveView, FuzzyDistanceScaleFactor, ClosestInteractorShape, ClosestDistanceToRay, ClosestDistanceOnRay, ClosestHitLocation, bAlreadyHitTriangle);
			if (bHit)
			{
				HitElementAddress.ElementType = EEditableMeshElementType::Polygon;
				HitElementAddress.ElementID = PolygonID;

				// aside to the element selection, we always fill in the polygon group ID (AKA Bone Index) of the selection
				// this allows us to make selections of individual bones from within a Geometry Collection
				HitElementAddress.BoneID = MeshDescription->GetPolygonPolygonGroup(PolygonID);
				if (DesiredPolygonGroup != -1)
				{
					check(DesiredPolygonGroup == HitElementAddress.BoneID.GetValue());
				}
			}
		}
	}

	// Reset the closest distance to ray (which will have been set to 0 by the polygon check) so other elements can be found within the fuzzy distance
	ClosestDistanceToRay = TNumericLimits<float>::Max();

	// Check edges
	if (OnlyElementType == EEditableMeshElementType::Invalid || OnlyElementType == EEditableMeshElementType::Edge)
	{
		for (const FEdgeID EdgeID : FrontFacingEdges)
		{
			FVector EdgeVertexPositions[2];
			EdgeVertexPositions[0] = VertexPositions[EditableMesh.GetEdgeVertex(EdgeID, 0)];
			EdgeVertexPositions[1] = VertexPositions[EditableMesh.GetEdgeVertex(EdgeID, 1)];

			const bool bAlreadyHitEdge = (HitElementAddress.ElementType == EEditableMeshElementType::Edge);
			const bool bHit = CheckEdge(InteractorShape, Sphere, SphereFuzzyDistance, RayStart, CurrentRayEnd, RayFuzzyDistance, EdgeVertexPositions, CameraLocation, bIsPerspectiveView, FuzzyDistanceScaleFactor, ClosestInteractorShape, ClosestDistanceToRay, ClosestDistanceOnRay, ClosestHitLocation, bAlreadyHitEdge);
			if (bHit)
			{
				HitElementAddress.ElementType = EEditableMeshElementType::Edge;
				HitElementAddress.ElementID = EdgeID;
			}
		}
	}

	ClosestDistanceToRay = TNumericLimits<float>::Max();

	// Check vertices
	if (OnlyElementType == EEditableMeshElementType::Invalid || OnlyElementType == EEditableMeshElementType::Vertex)
	{
		for (const FVertexID VertexID : FrontFacingVertices)
		{
			const FVector VertexPosition = VertexPositions[VertexID];
			const bool bAlreadyHitVertex = (HitElementAddress.ElementType == EEditableMeshElementType::Vertex);
			const bool bHit = CheckVertex(InteractorShape, Sphere, SphereFuzzyDistance, RayStart, CurrentRayEnd, RayFuzzyDistance, VertexPosition, CameraLocation, bIsPerspectiveView, FuzzyDistanceScaleFactor, ClosestInteractorShape, ClosestDistanceToRay, ClosestDistanceOnRay, ClosestHitLocation, bAlreadyHitVertex);
			if (bHit)
			{
				HitElementAddress.ElementType = EEditableMeshElementType::Vertex;
				HitElementAddress.ElementID = VertexID;
			}
		}
	}

	if (HitElementAddress.ElementType != EEditableMeshElementType::Invalid)
	{
		OutInteractorShape = ClosestInteractorShape;
		OutHitLocation = ClosestHitLocation;
	}

	return HitElementAddress;
}


bool FGeometryTests::CheckVertex(const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float RayFuzzyDistance, const FVector& VertexPosition, const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitVertex)
{
	bool bHit = false;

	const float DistanceBias = bIsPerspectiveView ? GeometryTest::OverlayPerspectiveDistanceBias->GetFloat() : GeometryTest::OverlayOrthographicDistanceBias->GetFloat();
	const float DistanceToCamera = bIsPerspectiveView ? (CameraLocation - VertexPosition).Size() : 0.0f;
	const float DistanceBasedScaling = DistanceBias + DistanceToCamera * FuzzyDistanceScaleFactor;
	check(DistanceBasedScaling > 0.0f);

	if (InteractorShape == EInteractorShape::GrabberSphere)
	{
		const float DistanceToSphere = (VertexPosition - Sphere.Center).Size();
		if (DistanceToSphere <= Sphere.W)
		{
			// Inside sphere
			if (DistanceToSphere < ClosestDistanceToRay ||
				(!bAlreadyHitVertex && FMath::Abs(DistanceToSphere - ClosestDistanceToRay) < SphereFuzzyDistance * DistanceBasedScaling))
			{
				ClosestDistanceToRay = DistanceToSphere;
				ClosestDistanceOnRay = 0.0f;
				ClosestHitLocation = VertexPosition;
				ClosestInteractorShape = EInteractorShape::GrabberSphere;

				bHit = true;
			}
		}
	}

	if (InteractorShape == EInteractorShape::Laser)
	{
		const FVector ClosestPointOnRay = FMath::ClosestPointOnSegment(VertexPosition, RayStart, RayEnd);
		const float DistanceToRay = (ClosestPointOnRay - VertexPosition).Size();
		const float DistanceOnRay = (ClosestPointOnRay - RayStart).Size();

		const FVector RayDirection = (RayEnd - RayStart).GetSafeNormal();
		const FVector DirectionTowardClosestPointOnRay = (ClosestPointOnRay - RayStart).GetSafeNormal();
		const bool bIsBehindRay = FVector::DotProduct(RayDirection, DirectionTowardClosestPointOnRay) < 0.0f;
		if (!bIsBehindRay)
		{
			// Are we within the minimum distance for hitting a vertex?
			if (DistanceToRay < RayFuzzyDistance * DistanceBasedScaling)
			{
				const bool bWithinFuzzyRadius = FMath::Abs(DistanceOnRay - ClosestDistanceOnRay) < RayFuzzyDistance * DistanceBasedScaling;

				if ((bWithinFuzzyRadius && DistanceToRay < ClosestDistanceToRay) || (!bWithinFuzzyRadius && DistanceOnRay < ClosestDistanceOnRay))
				{
					ClosestDistanceToRay = DistanceToRay;
					ClosestDistanceOnRay = DistanceOnRay;
					ClosestHitLocation = ClosestPointOnRay;
					ClosestInteractorShape = EInteractorShape::Laser;
					bHit = true;
				}

				// @todo mesheditor debug
				// const float Radius = GHackComponentToWorld.InverseTransformVector( FVector( RayFuzzyDistance * DistanceBasedScaling, 0.0f, 0.0f ) ).X;
				// DrawDebugSphere( GHackVWI->GetWorld(), GHackComponentToWorld.TransformPosition( VertexPosition ), Radius, 8, bHit ? FColor::Green : FColor::Yellow, false, 0.0f );
			}
		}
	}

	return bHit;
}


bool FGeometryTests::CheckEdge(const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float RayFuzzyDistance, const FVector EdgeVertexPositions[2], const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitEdge)
{
	bool bHit = false;

	if (InteractorShape == EInteractorShape::GrabberSphere)
	{
		const float DistanceToSphere = FMath::PointDistToSegment(Sphere.Center, EdgeVertexPositions[0], EdgeVertexPositions[1]);
		if (DistanceToSphere <= Sphere.W)
		{
			const FVector ClosestPointOnEdge = FMath::ClosestPointOnSegment(Sphere.Center, EdgeVertexPositions[0], EdgeVertexPositions[1]);
			const float DistanceToCamera = bIsPerspectiveView ? (CameraLocation - ClosestPointOnEdge).Size() : 0.0f;
			const float DistanceBias = bIsPerspectiveView ? GeometryTest::OverlayPerspectiveDistanceBias->GetFloat() : GeometryTest::OverlayOrthographicDistanceBias->GetFloat();
			const float DistanceBasedScaling = DistanceBias + DistanceToCamera * FuzzyDistanceScaleFactor;

			// Inside sphere
			if (DistanceToSphere < ClosestDistanceToRay ||
				(!bAlreadyHitEdge && FMath::Abs(DistanceToSphere - ClosestDistanceToRay) < SphereFuzzyDistance * DistanceBasedScaling))
			{
				ClosestDistanceToRay = DistanceToSphere;
				ClosestDistanceOnRay = 0.0f;
				ClosestHitLocation = ClosestPointOnEdge;
				ClosestInteractorShape = EInteractorShape::GrabberSphere;

				bHit = true;
			}
		}
	}


	if (InteractorShape == EInteractorShape::Laser)
	{
		FVector ClosestPointOnEdge, ClosestPointOnRay;
		FMath::SegmentDistToSegmentSafe(
			EdgeVertexPositions[0], EdgeVertexPositions[1],
			RayStart, RayEnd,
			/* Out */ ClosestPointOnEdge,
			/* Out */ ClosestPointOnRay);
		const float DistanceToRay = (ClosestPointOnEdge - ClosestPointOnRay).Size();
		const float DistanceOnRay = (ClosestPointOnRay - RayStart).Size();

		const FVector RayDirection = (RayEnd - RayStart).GetSafeNormal();
		const FVector DirectionTowardClosestPointOnRay = (ClosestPointOnRay - RayStart).GetSafeNormal();
		const bool bIsBehindRay = FVector::DotProduct(RayDirection, DirectionTowardClosestPointOnRay) < 0.0f;
		if (!bIsBehindRay)
		{
			const float DistanceToCamera = bIsPerspectiveView ? (CameraLocation - ClosestPointOnEdge).Size() : 0.0f;
			const float DistanceBias = bIsPerspectiveView ? GeometryTest::OverlayPerspectiveDistanceBias->GetFloat() : GeometryTest::OverlayOrthographicDistanceBias->GetFloat();
			const float DistanceBasedScaling = DistanceBias + DistanceToCamera * FuzzyDistanceScaleFactor;
			check(DistanceBasedScaling > 0.0f);

			// Are we within the minimum distance for hitting an edge?
			if (DistanceToRay < RayFuzzyDistance * DistanceBasedScaling)
			{
				const bool bWithinFuzzyRadius = FMath::Abs(DistanceOnRay - ClosestDistanceOnRay) < RayFuzzyDistance * DistanceBasedScaling;

				if ((bWithinFuzzyRadius && DistanceToRay < ClosestDistanceToRay) || (!bWithinFuzzyRadius && DistanceOnRay < ClosestDistanceOnRay))
				{
					ClosestDistanceToRay = DistanceToRay;
					ClosestDistanceOnRay = DistanceOnRay;
					ClosestHitLocation = ClosestPointOnRay;
					ClosestInteractorShape = EInteractorShape::Laser;
					bHit = true;
				}

				// @todo mesheditor debug
				// const float Radius = GHackComponentToWorld.InverseTransformVector( FVector( RayFuzzyDistance * DistanceBasedScaling, 0.0f, 0.0f ) ).X;
				// DrawDebugSphere( GHackVWI->GetWorld(), GHackComponentToWorld.InverseTransformPosition( ClosestPointOnEdge ), Radius, 12, bHit ? FColor::Green : FColor::Yellow, false, 0.0f );
			}
		}
	}

	return bHit;
}


bool FGeometryTests::CheckTriangle(const EInteractorShape InteractorShape, const FSphere& Sphere, const float SphereFuzzyDistance, const FVector& RayStart, const FVector& RayEnd, const float RayFuzzyDistance, const FVector TriangleVertexPositions[3], const FVector& CameraLocation, const bool bIsPerspectiveView, const float FuzzyDistanceScaleFactor, EInteractorShape& ClosestInteractorShape, float& ClosestDistanceToRay, float& ClosestDistanceOnRay, FVector& ClosestHitLocation, const bool bAlreadyHitTriangle)
{
	bool bHit = false;

	if (InteractorShape == EInteractorShape::GrabberSphere)
	{
		// @todo grabber: FMath::ClosestPointOnTriangleToPoint doesn't work with degenerates (always returns ray start point?)
		const FVector ClosestPointOnTriangleToSphere = FMath::ClosestPointOnTriangleToPoint(Sphere.Center, TriangleVertexPositions[0], TriangleVertexPositions[1], TriangleVertexPositions[2]);
		const float DistanceToSphere = (ClosestPointOnTriangleToSphere - Sphere.Center).Size();
		if (DistanceToSphere <= Sphere.W)
		{
			const float DistanceToCamera = bIsPerspectiveView ? (CameraLocation - ClosestPointOnTriangleToSphere).Size() : 0.0f;
			const float DistanceBias = bIsPerspectiveView ? GeometryTest::OverlayPerspectiveDistanceBias->GetFloat() : GeometryTest::OverlayOrthographicDistanceBias->GetFloat();
			const float DistanceBasedScaling = DistanceBias + DistanceToCamera * FuzzyDistanceScaleFactor;

			// Inside sphere
			if (DistanceToSphere < ClosestDistanceToRay ||
				(!bAlreadyHitTriangle && FMath::Abs(DistanceToSphere - ClosestDistanceToRay) < SphereFuzzyDistance * DistanceBasedScaling))
			{
				ClosestHitLocation = ClosestPointOnTriangleToSphere;
				ClosestDistanceToRay = DistanceToSphere;
				ClosestDistanceOnRay = 0.0f;
				ClosestInteractorShape = EInteractorShape::GrabberSphere;

				bHit = true;
			}
		}
	}


	if (InteractorShape == EInteractorShape::Laser)
	{
		// @todo mesheditor: We have like 5 different versions of this in the engine, but nothing generic in a nice place
		struct Local
		{
			static bool RayIntersectTriangle(const FVector& Start, const FVector& End, const FVector& A, const FVector& B, const FVector& C, FVector& IntersectPoint)
			{
				const FVector TriNormal = (B - A) ^ (C - A);

				bool bCollide = FMath::SegmentPlaneIntersection(Start, End, FPlane(A, TriNormal), IntersectPoint);
				if (!bCollide)
				{
					return false;
				}

				// Make sure points are not colinear.  ComputeBaryCentric2D() doesn't like that.
				if (TriNormal.SizeSquared() > SMALL_NUMBER)
				{
					FVector BaryCentric = FMath::ComputeBaryCentric2D(IntersectPoint, A, B, C);
					if (BaryCentric.X > 0.0f && BaryCentric.Y > 0.0f && BaryCentric.Z > 0.0f)
					{
						return true;
					}
				}
				return false;
			}
		};


		// @todo mesheditor: Possibly we shouldn't always check for faces when in wire frame view mode?

		// Note: Polygon is assumed to be front facing
		FVector IntersectionPoint;

		// @todo mesheditor hole: Needs support for polygon hole contours
		// @todo mesheditor perf: We also have a SIMD version of this that does four triangles at once
		if (Local::RayIntersectTriangle(RayStart, RayEnd, TriangleVertexPositions[0], TriangleVertexPositions[1], TriangleVertexPositions[2], /* Out */ IntersectionPoint))
		{
			const float DistanceToCamera = bIsPerspectiveView ? (CameraLocation - IntersectionPoint).Size() : 0.0f;
			const float DistanceBias = bIsPerspectiveView ? GeometryTest::OverlayPerspectiveDistanceBias->GetFloat() : GeometryTest::OverlayOrthographicDistanceBias->GetFloat();
			const float DistanceBasedScaling = DistanceBias + DistanceToCamera * FuzzyDistanceScaleFactor;

			const float DistanceToRay = 0.0f;  // We intersected the triangle, otherwise we wouldn't even be in here
			const float DistanceOnRay = (IntersectionPoint - RayStart).Size();
			if (DistanceOnRay < ClosestDistanceOnRay ||
				(!bAlreadyHitTriangle && FMath::Abs(DistanceOnRay - ClosestDistanceOnRay) < RayFuzzyDistance * DistanceBasedScaling))
			{
				ClosestHitLocation = IntersectionPoint;
				ClosestDistanceToRay = DistanceToRay;
				ClosestDistanceOnRay = DistanceOnRay;
				ClosestInteractorShape = EInteractorShape::Laser;

				bHit = true;
			}

			// @todo mesheditor debug
			// const float Radius = GHackComponentToWorld.InverseTransformVector( FVector( RayFuzzyDistance * DistanceBasedScaling, 0.0f, 0.0f ) ).X;
			// DrawDebugSphere( GHackVWI->GetWorld(), GHackComponentToWorld.TransformPosition( IntersectionPoint ), Radius, 16, bHit ? FColor::Green : FColor::Yellow, false, 0.0f );
		}
	}

	return bHit;
}


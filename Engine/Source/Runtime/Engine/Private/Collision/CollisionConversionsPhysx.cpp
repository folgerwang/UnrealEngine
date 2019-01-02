// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if PHYSICS_INTERFACE_PHYSX

#include "Collision/CollisionConversionsPhysx.h"
#include "Collision/CollisionConversions.h"
#include "Collision/CollisionDebugDrawing.h"
#include "Components/LineBatchComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PxQueryFilterCallback.h"
#include "Physics/PhysicsInterfaceUtils.h"

#include "PhysXPublic.h"
#include "CustomPhysXPayload.h"
#include "PhysXInterfaceWrapper.h"

static FORCEINLINE bool PxQuatIsIdentity(PxQuat const& Q)
{
	return
		Q.x == 0.f &&
		Q.y == 0.f &&
		Q.z == 0.f &&
		Q.w == 1.f;
}


/** Helper to transform a normal when non-uniform scale is present. */
static PxVec3 TransformNormalToShapeSpace(const PxMeshScale& meshScale, const PxVec3& nIn)
{
	// Uniform scale makes this unnecessary
	if (meshScale.scale.x == meshScale.scale.y &&
		meshScale.scale.x == meshScale.scale.z)
	{
		return nIn;
	}

	if (PxQuatIsIdentity(meshScale.rotation))
	{
		// Inverse transpose: inverse is 1/scale, transpose = original when rotation is identity.
		const PxVec3 tmp = PxVec3(nIn.x / meshScale.scale.x, nIn.y / meshScale.scale.y, nIn.z / meshScale.scale.z);
		const PxReal denom = 1.0f / tmp.magnitude();
		return tmp * denom;
	}
	else
	{
		const PxMat33 rot(meshScale.rotation);
		const PxMat33 diagonal = PxMat33::createDiagonal(meshScale.scale);
		const PxMat33 vertex2Shape = (rot.getTranspose() * diagonal) * rot;

		const PxMat33 shape2Vertex = vertex2Shape.getInverse();
		const PxVec3 tmp = shape2Vertex.transformTranspose(nIn);
		const PxReal denom = 1.0f / tmp.magnitude();
		return tmp * denom;
	}
}

FVector FindBoxOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	// We require normal info for our algorithm.
	const bool bNormalData = (PHit.flags & PxHitFlag::eNORMAL);
	if (!bNormalData)
	{
		return InNormal;
	}

	PxBoxGeometry PxBoxGeom;
	const bool bReadGeomSuccess = PHit.shape->getBoxGeometry(PxBoxGeom);
	check(bReadGeomSuccess); // This function should only be used for box geometry

	const PxTransform LocalToWorld = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);

	// Find which faces were included in the contact normal, and for multiple faces, use the one most opposing the sweep direction.
	const PxVec3 ContactNormalLocal = LocalToWorld.rotateInv(PHit.normal);
	const float* ContactNormalLocalPtr = &ContactNormalLocal.x;
	const PxVec3 TraceDirDenormWorld = U2PVector(TraceDirectionDenorm);
	const float* TraceDirDenormWorldPtr = &TraceDirDenormWorld.x;
	const PxVec3 TraceDirDenormLocal = LocalToWorld.rotateInv(TraceDirDenormWorld);
	const float* TraceDirDenormLocalPtr = &TraceDirDenormLocal.x;

	PxVec3 BestLocalNormal(ContactNormalLocal);
	float* BestLocalNormalPtr = &BestLocalNormal.x;
	float BestOpposingDot = FLT_MAX;

	for (int32 i = 0; i < 3; i++)
	{
		// Select axis of face to compare to, based on normal.
		if (ContactNormalLocalPtr[i] > KINDA_SMALL_NUMBER)
		{
			const float TraceDotFaceNormal = TraceDirDenormLocalPtr[i]; // TraceDirDenormLocal.dot(BoxFaceNormal)
			if (TraceDotFaceNormal < BestOpposingDot)
			{
				BestOpposingDot = TraceDotFaceNormal;
				BestLocalNormal = PxVec3(0.f);
				BestLocalNormalPtr[i] = 1.f;
			}
		}
		else if (ContactNormalLocalPtr[i] < -KINDA_SMALL_NUMBER)
		{
			const float TraceDotFaceNormal = -TraceDirDenormLocalPtr[i]; // TraceDirDenormLocal.dot(BoxFaceNormal)
			if (TraceDotFaceNormal < BestOpposingDot)
			{
				BestOpposingDot = TraceDotFaceNormal;
				BestLocalNormal = PxVec3(0.f);
				BestLocalNormalPtr[i] = -1.f;
			}
		}
	}

	// Fill in result
	const PxVec3 WorldNormal = LocalToWorld.rotate(BestLocalNormal);
	return P2UVector(WorldNormal);
}

FVector FindHeightFieldOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	if (IsInvalidFaceIndex(PHit.faceIndex))
	{
		return InNormal;
	}

	PxHeightFieldGeometry PHeightFieldGeom;
	const bool bReadGeomSuccess = PHit.shape->getHeightFieldGeometry(PHeightFieldGeom);
	check(bReadGeomSuccess);	//we should only call this function when we have a heightfield
	if (PHeightFieldGeom.heightField)
	{
		const PxU32 TriIndex = PHit.faceIndex;
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);

		PxTriangle Tri;
		PxMeshQuery::getTriangle(PHeightFieldGeom, PShapeWorldPose, TriIndex, Tri);

		PxVec3 TriNormal;
		Tri.normal(TriNormal);
		return P2UVector(TriNormal);
	}

	return InNormal;
}

FVector FindConvexMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	if (IsInvalidFaceIndex(PHit.faceIndex))
	{
		return InNormal;
	}

	PxConvexMeshGeometry PConvexMeshGeom;
	bool bSuccess = PHit.shape->getConvexMeshGeometry(PConvexMeshGeom);
	check(bSuccess);	//should only call this function when we have a convex mesh

	if (PConvexMeshGeom.convexMesh)
	{
		check(PHit.faceIndex < PConvexMeshGeom.convexMesh->getNbPolygons());

		const PxU32 PolyIndex = PHit.faceIndex;
		PxHullPolygon PPoly;
		bool bSuccessData = PConvexMeshGeom.convexMesh->getPolygonData(PolyIndex, PPoly);
		if (bSuccessData)
		{
			// Account for non-uniform scale in local space normal.
			const PxVec3 PPlaneNormal(PPoly.mPlane[0], PPoly.mPlane[1], PPoly.mPlane[2]);
			const PxVec3 PLocalPolyNormal = TransformNormalToShapeSpace(PConvexMeshGeom.scale, PPlaneNormal.getNormalized());

			// Convert to world space
			const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
			const PxVec3 PWorldPolyNormal = PShapeWorldPose.rotate(PLocalPolyNormal);
			const FVector OutNormal = P2UVector(PWorldPolyNormal);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!OutNormal.IsNormalized())
			{
				UE_LOG(LogPhysics, Warning, TEXT("Non-normalized Normal (Hit shape is ConvexMesh): %s (LocalPolyNormal:%s)"), *OutNormal.ToString(), *P2UVector(PLocalPolyNormal).ToString());
				UE_LOG(LogPhysics, Warning, TEXT("WorldTransform \n: %s"), *P2UTransform(PShapeWorldPose).ToString());
			}
#endif
			return OutNormal;
		}
	}

	return InNormal;
}

FVector FindTriMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	if (IsInvalidFaceIndex(PHit.faceIndex))
	{
		return InNormal;
	}

	PxTriangleMeshGeometry PTriMeshGeom;
	bool bSuccess = PHit.shape->getTriangleMeshGeometry(PTriMeshGeom);
	check(bSuccess);	//this function should only be called when we have a trimesh

	if (PTriMeshGeom.triangleMesh)
	{
		check(PHit.faceIndex < PTriMeshGeom.triangleMesh->getNbTriangles());

		const PxU32 TriIndex = PHit.faceIndex;
		const void* Triangles = PTriMeshGeom.triangleMesh->getTriangles();

		// Grab triangle indices that we hit
		int32 I0, I1, I2;

		if (PTriMeshGeom.triangleMesh->getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES)
		{
			PxU16* P16BitIndices = (PxU16*)Triangles;
			I0 = P16BitIndices[(TriIndex * 3) + 0];
			I1 = P16BitIndices[(TriIndex * 3) + 1];
			I2 = P16BitIndices[(TriIndex * 3) + 2];
		}
		else
		{
			PxU32* P32BitIndices = (PxU32*)Triangles;
			I0 = P32BitIndices[(TriIndex * 3) + 0];
			I1 = P32BitIndices[(TriIndex * 3) + 1];
			I2 = P32BitIndices[(TriIndex * 3) + 2];
		}

		// Get verts we hit (local space)
		const PxVec3* PVerts = PTriMeshGeom.triangleMesh->getVertices();
		const PxVec3 V0 = PVerts[I0];
		const PxVec3 V1 = PVerts[I1];
		const PxVec3 V2 = PVerts[I2];

		// Find normal of triangle (local space), and account for non-uniform scale
		const PxVec3 PTempNormal = ((V1 - V0).cross(V2 - V0)).getNormalized();
		const PxVec3 PLocalTriNormal = TransformNormalToShapeSpace(PTriMeshGeom.scale, PTempNormal);

		// Convert to world space
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
		const PxVec3 PWorldTriNormal = PShapeWorldPose.rotate(PLocalTriNormal);
		FVector OutNormal = P2UVector(PWorldTriNormal);

		if (PTriMeshGeom.meshFlags & PxMeshGeometryFlag::eDOUBLE_SIDED)
		{
			//double sided mesh so we need to consider direction of query
			const float sign = FVector::DotProduct(OutNormal, TraceDirectionDenorm) > 0.f ? -1.f : 1.f;
			OutNormal *= sign;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!OutNormal.IsNormalized())
		{
			UE_LOG(LogPhysics, Warning, TEXT("Non-normalized Normal (Hit shape is TriangleMesh): %s (V0:%s, V1:%s, V2:%s)"), *OutNormal.ToString(), *P2UVector(V0).ToString(), *P2UVector(V1).ToString(), *P2UVector(V2).ToString());
			UE_LOG(LogPhysics, Warning, TEXT("WorldTransform \n: %s"), *P2UTransform(PShapeWorldPose).ToString());
		}
#endif
		return OutNormal;
	}

	return InNormal;
}



static bool ComputeInflatedMTD_Internal(const float MtdInflation, const PxLocationHit& PHit, FHitResult& OutResult, const PxTransform& QueryTM, const PxGeometry& Geom, const PxTransform& PShapeWorldPose)
{
	PxGeometry* InflatedGeom = NULL;

	PxVec3 PxMtdNormal(0.f);
	PxF32 PxMtdDepth = 0.f;
	PxGeometryHolder Holder = PHit.shape->getGeometry();
	const PxGeometry& POtherGeom = Holder.any();
	const bool bMtdResult = PxGeometryQuery::computePenetration(PxMtdNormal, PxMtdDepth, Geom, QueryTM, POtherGeom, PShapeWorldPose);
	if (bMtdResult)
	{
		if (PxMtdNormal.isFinite())
		{
			OutResult.ImpactNormal = P2UVector(PxMtdNormal);
			OutResult.PenetrationDepth = FMath::Max(FMath::Abs(PxMtdDepth) - MtdInflation, 0.f) + KINDA_SMALL_NUMBER;
			return true;
		}
		else
		{
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ComputeInflatedMTD_Internal: MTD returned NaN :( normal: (X:%f, Y:%f, Z:%f)"), PxMtdNormal.x, PxMtdNormal.y, PxMtdNormal.z);
		}
	}

	return false;
}


// Compute depenetration vector and distance if possible with a slightly larger geometry
static bool ComputeInflatedMTD(const float MtdInflation, const PxLocationHit& PHit, FHitResult& OutResult, const PxTransform& QueryTM, const PxGeometry& Geom, const PxTransform& PShapeWorldPose)
{
	switch (Geom.getType())
	{
	case PxGeometryType::eCAPSULE:
	{
		const PxCapsuleGeometry* InCapsule = static_cast<const PxCapsuleGeometry*>(&Geom);
		PxCapsuleGeometry InflatedCapsule(InCapsule->radius + MtdInflation, InCapsule->halfHeight); // don't inflate halfHeight, radius is added all around.
		return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedCapsule, PShapeWorldPose);
	}

	case PxGeometryType::eBOX:
	{
		const PxBoxGeometry* InBox = static_cast<const PxBoxGeometry*>(&Geom);
		PxBoxGeometry InflatedBox(InBox->halfExtents + PxVec3(MtdInflation));
		return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedBox, PShapeWorldPose);
	}

	case PxGeometryType::eSPHERE:
	{
		const PxSphereGeometry* InSphere = static_cast<const PxSphereGeometry*>(&Geom);
		PxSphereGeometry InflatedSphere(InSphere->radius + MtdInflation);
		return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedSphere, PShapeWorldPose);
	}

	case PxGeometryType::eCONVEXMESH:
	{
		// We can't exactly inflate the mesh (not easily), so try jittering it a bit to get an MTD result.
		PxVec3 TraceDir = U2PVector(OutResult.TraceEnd - OutResult.TraceStart);
		TraceDir.normalizeSafe();

		// Try forward (in trace direction)
		PxTransform JitteredTM = PxTransform(QueryTM.p + (TraceDir * MtdInflation), QueryTM.q);
		if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
		{
			return true;
		}

		// Try backward (opposite trace direction)
		JitteredTM = PxTransform(QueryTM.p - (TraceDir * MtdInflation), QueryTM.q);
		if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
		{
			return true;
		}

		// Try axial directions.
		// Start with -Z because this is the most common case (objects on the floor).
		for (int32 i = 2; i >= 0; i--)
		{
			PxVec3 Jitter(0.f);
			float* JitterPtr = &Jitter.x;
			JitterPtr[i] = MtdInflation;

			JitteredTM = PxTransform(QueryTM.p - Jitter, QueryTM.q);
			if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
			{
				return true;
			}

			JitteredTM = PxTransform(QueryTM.p + Jitter, QueryTM.q);
			if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
			{
				return true;
			}
		}

		return false;
	}

	default:
	{
		return false;
	}
	}
}

static bool CanFindOverlappedTriangle(const PxShape* PShape)
{
	return (PShape && (PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH || PShape->getGeometryType() == PxGeometryType::eHEIGHTFIELD));
}

/* Function to find the best normal from the list of triangles that are overlapping our geom. */
template<typename GeomType>
FVector FindBestOverlappingNormal(const UWorld* World, const PxGeometry& Geom, const PxTransform& QueryTM, const GeomType& ShapeGeom, const PxTransform& PShapeWorldPose, PxU32* HitTris, int32 NumTrisHit, bool bCanDrawOverlaps = false)
{
#if DRAW_OVERLAPPING_TRIS
	const float Lifetime = 5.f;
	bCanDrawOverlaps &= World && World->IsGameWorld() && World->PersistentLineBatcher && (World->PersistentLineBatcher->BatchedLines.Num() < 2048);
	if (bCanDrawOverlaps)
	{
		TArray<FOverlapResult> Overlaps;
		DrawGeomOverlaps(World, Geom, QueryTM, Overlaps, Lifetime);
	}
	const FLinearColor LineColor = FLinearColor::Green;
	const FLinearColor NormalColor = FLinearColor::Red;
	const FLinearColor PointColor = FLinearColor::Yellow;
#endif // DRAW_OVERLAPPING_TRIS

	// Track the best triangle plane distance
	float BestPlaneDist = -BIG_NUMBER;
	FVector BestPlaneNormal(0, 0, 1);
	// Iterate over triangles
	for (int32 TriIdx = 0; TriIdx < NumTrisHit; TriIdx++)
	{
		PxTriangle Tri;
		PxMeshQuery::getTriangle(ShapeGeom, PShapeWorldPose, HitTris[TriIdx], Tri);

		const FVector A = P2UVector(Tri.verts[0]);
		const FVector B = P2UVector(Tri.verts[1]);
		const FVector C = P2UVector(Tri.verts[2]);

		FVector TriNormal = ((B - A) ^ (C - A));
		TriNormal = TriNormal.GetSafeNormal();

		const FPlane TriPlane(A, TriNormal);

		const FVector QueryCenter = P2UVector(QueryTM.p);
		const float DistToPlane = TriPlane.PlaneDot(QueryCenter);

		if (DistToPlane > BestPlaneDist)
		{
			BestPlaneDist = DistToPlane;
			BestPlaneNormal = TriNormal;
		}

#if DRAW_OVERLAPPING_TRIS
		if (bCanDrawOverlaps && World && World->PersistentLineBatcher && World->PersistentLineBatcher->BatchedLines.Num() < 2048)
		{
			static const float LineThickness = 0.9f;
			static const float NormalThickness = 0.75f;
			static const float PointThickness = 5.0f;
			World->PersistentLineBatcher->DrawLine(A, B, LineColor, SDPG_Foreground, LineThickness, Lifetime);
			World->PersistentLineBatcher->DrawLine(B, C, LineColor, SDPG_Foreground, LineThickness, Lifetime);
			World->PersistentLineBatcher->DrawLine(C, A, LineColor, SDPG_Foreground, LineThickness, Lifetime);
			const FVector Centroid((A + B + C) / 3.f);
			World->PersistentLineBatcher->DrawLine(Centroid, Centroid + (35.0f*TriNormal), NormalColor, SDPG_Foreground, NormalThickness, Lifetime);
			World->PersistentLineBatcher->DrawPoint(Centroid + (35.0f*TriNormal), NormalColor, PointThickness, SDPG_Foreground, Lifetime);
			World->PersistentLineBatcher->DrawPoint(A, PointColor, PointThickness, SDPG_Foreground, Lifetime);
			World->PersistentLineBatcher->DrawPoint(B, PointColor, PointThickness, SDPG_Foreground, Lifetime);
			World->PersistentLineBatcher->DrawPoint(C, PointColor, PointThickness, SDPG_Foreground, Lifetime);
		}
#endif // DRAW_OVERLAPPING_TRIS
	}

	return BestPlaneNormal;
}


static bool FindOverlappedTriangleNormal_Internal(const UWorld* World, const PxGeometry& Geom, const PxTransform& QueryTM, const PxShape* PShape, const PxTransform& PShapeWorldPose, FVector& OutNormal, bool bCanDrawOverlaps = false)
{
	if (CanFindOverlappedTriangle(PShape))
	{
		PxTriangleMeshGeometry PTriMeshGeom;
		PxHeightFieldGeometry PHeightfieldGeom;

		if (PShape->getTriangleMeshGeometry(PTriMeshGeom) || PShape->getHeightFieldGeometry(PHeightfieldGeom))
		{
			PxGeometryType::Enum GeometryType = PShape->getGeometryType();
			const bool bIsTriMesh = (GeometryType == PxGeometryType::eTRIANGLEMESH);
			PxU32 HitTris[64];
			bool bOverflow = false;

			const int32 NumTrisHit = bIsTriMesh ?
				PxMeshQuery::findOverlapTriangleMesh(Geom, QueryTM, PTriMeshGeom, PShapeWorldPose, HitTris, ARRAY_COUNT(HitTris), 0, bOverflow) :
				PxMeshQuery::findOverlapHeightField(Geom, QueryTM, PHeightfieldGeom, PShapeWorldPose, HitTris, ARRAY_COUNT(HitTris), 0, bOverflow);

			if (NumTrisHit > 0)
			{
				if (bIsTriMesh)
				{
					OutNormal = FindBestOverlappingNormal(World, Geom, QueryTM, PTriMeshGeom, PShapeWorldPose, HitTris, NumTrisHit, bCanDrawOverlaps);
				}
				else
				{
					OutNormal = FindBestOverlappingNormal(World, Geom, QueryTM, PHeightfieldGeom, PShapeWorldPose, HitTris, NumTrisHit, bCanDrawOverlaps);
				}

				return true;
			}
		}
	}

	return false;
}

static bool FindOverlappedTriangleNormal(const UWorld* World, const PxGeometry& Geom, const PxTransform& QueryTM, const PxShape* PShape, const PxTransform& PShapeWorldPose, FVector& OutNormal, float Inflation, bool bCanDrawOverlaps = false)
{
	bool bSuccess = false;

	if (CanFindOverlappedTriangle(PShape))
	{
		if (Inflation <= 0.f)
		{
			bSuccess = FindOverlappedTriangleNormal_Internal(World, Geom, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
		}
		else
		{
			// Try a slightly inflated test if possible.
			switch (Geom.getType())
			{
			case PxGeometryType::eCAPSULE:
			{
				const PxCapsuleGeometry* InCapsule = static_cast<const PxCapsuleGeometry*>(&Geom);
				PxCapsuleGeometry InflatedCapsule(InCapsule->radius + Inflation, InCapsule->halfHeight); // don't inflate halfHeight, radius is added all around.
				bSuccess = FindOverlappedTriangleNormal_Internal(World, InflatedCapsule, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
				break;
			}

			case PxGeometryType::eBOX:
			{
				const PxBoxGeometry* InBox = static_cast<const PxBoxGeometry*>(&Geom);
				PxBoxGeometry InflatedBox(InBox->halfExtents + PxVec3(Inflation));
				bSuccess = FindOverlappedTriangleNormal_Internal(World, InflatedBox, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
				break;
			}

			case PxGeometryType::eSPHERE:
			{
				const PxSphereGeometry* InSphere = static_cast<const PxSphereGeometry*>(&Geom);
				PxSphereGeometry InflatedSphere(InSphere->radius + Inflation);
				bSuccess = FindOverlappedTriangleNormal_Internal(World, InflatedSphere, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
				break;
			}

			default:
			{
				// No inflation possible
				break;
			}
			}
		}
	}

	return bSuccess;
}

void DrawOverlappingTris(const UWorld* World, const PxLocationHit& Hit, const PxGeometry& Geom, const FTransform& QueryTM)
{
	FVector DummyNormal(0.f);
	const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*Hit.shape, *Hit.actor);
	FindOverlappedTriangleNormal(World, Geom, U2PTransform(QueryTM), Hit.shape, PShapeWorldPose, DummyNormal, 0.f, true);
}

void ComputeZeroDistanceImpactNormalAndPenetration(const UWorld* World, const FHitLocation& Hit, const PxGeometry& Geom, const FTransform& QueryTM, FHitResult& OutResult)
{
	const PxTransform PQueryTM = U2PTransform(QueryTM);
	const PxShape* PShape = GetShape(Hit);
	const PxRigidActor* PActor = GetActor(Hit);
	const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PShape, *PActor);

	// Try MTD with a small inflation for better accuracy, then a larger one in case the first one fails due to precision issues.
	static const float SmallMtdInflation = 0.250f;
	static const float LargeMtdInflation = 1.750f;

	if (ComputeInflatedMTD(SmallMtdInflation, Hit, OutResult, PQueryTM, Geom, PShapeWorldPose) ||
		ComputeInflatedMTD(LargeMtdInflation, Hit, OutResult, PQueryTM, Geom, PShapeWorldPose))
	{
		// Success
	}
	else
	{
		static const float SmallOverlapInflation = 0.250f;
		if (FindOverlappedTriangleNormal(World, Geom, PQueryTM, PShape, PShapeWorldPose, OutResult.ImpactNormal, 0.f, false) ||
			FindOverlappedTriangleNormal(World, Geom, PQueryTM, PShape, PShapeWorldPose, OutResult.ImpactNormal, SmallOverlapInflation, false))
		{
			// Success
		}
		else
		{
			// MTD failed, use point distance. This is not ideal.
			// Note: faceIndex seems to be unreliable for convex meshes in these cases, so not using FindGeomOpposingNormal() for them here.
			PxGeometryHolder Holder = PShape->getGeometry();
			PxGeometry& PGeom = Holder.any();
			PxVec3 PClosestPoint;
			const float Distance = PxGeometryQuery::pointDistance(PQueryTM.p, PGeom, PShapeWorldPose, &PClosestPoint);

			if (Distance < KINDA_SMALL_NUMBER)
			{
				UE_LOG(LogCollision, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: Query origin inside shape, giving poor MTD."));
				PClosestPoint = PxShapeExt::getWorldBounds(*PShape, *PActor).getCenter();
			}

			OutResult.ImpactNormal = (OutResult.Location - P2UVector(PClosestPoint)).GetSafeNormal();
		}
	}
}


#endif // WITH_PHYSX

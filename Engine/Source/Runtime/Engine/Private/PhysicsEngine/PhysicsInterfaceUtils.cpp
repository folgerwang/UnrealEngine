// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Physics/PhysicsInterfaceUtils.h"
#include "PhysXPublic.h"
#include "WorldCollision.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceTypes.h"

#if WITH_PHYSX

PxShapeFlags BuildPhysXShapeFlags(FBodyCollisionFlags BodyCollisionFlags, bool bPhysicsStatic, bool bIsSync, bool bIsTriangleMesh)
{
	PxShapeFlags ShapeFlags;

	// Only perform scene queries if enabled and the shape is non-static or the shape is sync.
	ModifyShapeFlag<PxShapeFlag::eSCENE_QUERY_SHAPE>(ShapeFlags, BodyCollisionFlags.bEnableQueryCollision && (!bPhysicsStatic || bIsSync));

	if (bIsTriangleMesh)
	{
		ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeFlags, BodyCollisionFlags.bEnableSimCollisionComplex);
	}
	else
	{
		ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeFlags, BodyCollisionFlags.bEnableSimCollisionSimple);
	}

	ModifyShapeFlag<PxShapeFlag::eVISUALIZATION>(ShapeFlags, true);

	return ShapeFlags;
}

PxFilterData U2PFilterData(const FCollisionFilterData& FilterData)
{
	return PxFilterData(FilterData.Word0, FilterData.Word1, FilterData.Word2, FilterData.Word3);
}

FCollisionFilterData P2UFilterData(const PxFilterData& PFilterData)
{
	FCollisionFilterData FilterData;
	FilterData.Word0 = PFilterData.word0;
	FilterData.Word1 = PFilterData.word1;
	FilterData.Word2 = PFilterData.word2;
	FilterData.Word3 = PFilterData.word3;
	return FilterData;
}

PxGeometryType::Enum U2PCollisionShapeType(ECollisionShapeType InUType)
{
	switch(InUType)
	{
		case ECollisionShapeType::Sphere:				return PxGeometryType::eSPHERE;
		case ECollisionShapeType::Box:					return PxGeometryType::eBOX;
		case ECollisionShapeType::Capsule:				return PxGeometryType::eCAPSULE;
		case ECollisionShapeType::Convex:				return PxGeometryType::eCONVEXMESH;
		case ECollisionShapeType::Trimesh:				return PxGeometryType::eTRIANGLEMESH;
		case ECollisionShapeType::Heightfield:			return PxGeometryType::eHEIGHTFIELD;
	}

	return PxGeometryType::eINVALID;
}

ECollisionShapeType P2UCollisionShapeType(PxGeometryType::Enum InPType)
{
	switch(InPType)
	{
		case PxGeometryType::eSPHERE:					return ECollisionShapeType::Sphere;
		case PxGeometryType::eBOX:						return ECollisionShapeType::Box;
		case PxGeometryType::eCAPSULE:					return ECollisionShapeType::Capsule;
		case PxGeometryType::eCONVEXMESH:				return ECollisionShapeType::Convex;
		case PxGeometryType::eTRIANGLEMESH:				return ECollisionShapeType::Trimesh;
		case PxGeometryType::eHEIGHTFIELD:				return ECollisionShapeType::Heightfield;
	}

	return ECollisionShapeType::None;
}

uint32 FindFaceIndex(const PxSweepHit& PHit, const PxVec3& unitDir)
{
	PxConvexMeshGeometry convexGeom;
	if (PHit.shape->getConvexMeshGeometry(convexGeom))
	{
		//PhysX has given us the most correct face. However, we actually want the most useful face which is the one with the most opposed normal within some radius.
		//So for example, if we are sweeping against a corner we should take the plane that is most opposing, even if it's not the exact one we hit.
		static const float FindFaceInRadius = 1.f; // tolerance to determine how far from the actual contact point we want to search.

		const PxTransform pose = PHit.actor->getGlobalPose() * PHit.shape->getLocalPose();
		const PxVec3 impactPos(PHit.position);
		{
			//This is copied directly from PxFindFace. However, we made some modifications in order to favor 'most opposing' faces.
			static const PxReal gEpsilon = .01f;
			PX_ASSERT(unitDir.isFinite());
			PX_ASSERT(unitDir.isNormalized());
			PX_ASSERT(impactPos.isFinite());
			PX_ASSERT(pose.isFinite());

			const PxVec3 impact = impactPos - unitDir * gEpsilon;

			const PxVec3 localPoint = pose.transformInv(impact);
			const PxVec3 localDir = pose.rotateInv(unitDir);

			// Create shape to vertex scale transformation matrix
			const PxMeshScale& meshScale = convexGeom.scale;
			const PxMat33 rot(meshScale.rotation);
			PxMat33 shape2VertexSkew = rot.getTranspose();
			const PxMat33 diagonal = PxMat33::createDiagonal(PxVec3(1.0f / meshScale.scale.x, 1.0f / meshScale.scale.y, 1.0f / meshScale.scale.z));
			shape2VertexSkew = shape2VertexSkew * diagonal;
			shape2VertexSkew = shape2VertexSkew * rot;

			const PxU32 nbPolys = convexGeom.convexMesh->getNbPolygons();
			// BEGIN EPIC MODIFICATION Improved selection of 'most opposing' face
			bool bMinIndexValid = false;
			PxU32 minIndex = 0;
			PxReal maxD = -PX_MAX_REAL;
			PxU32 maxDIndex = 0;
			PxReal minNormalDot = PX_MAX_REAL;

			for (PxU32 j = 0; j < nbPolys; j++)
			{
				PxHullPolygon hullPolygon;
				convexGeom.convexMesh->getPolygonData(j, hullPolygon);

				// transform hull plane into shape space
				PxPlane plane;
				const PxVec3 tmp = shape2VertexSkew.transformTranspose(PxVec3(hullPolygon.mPlane[0], hullPolygon.mPlane[1], hullPolygon.mPlane[2]));
				const PxReal denom = 1.0f / tmp.magnitude();
				plane.n = tmp * denom;
				plane.d = hullPolygon.mPlane[3] * denom;

				PxReal d = plane.distance(localPoint);
				// Track plane that impact point is furthest point (will be out fallback normal)
				if (d > maxD)
				{
					maxDIndex = j;
					maxD = d;
				}

				//Because we are searching against a convex hull, we will never get multiple faces that are both in front of the contact point _and_ have an opposing normal (except the face we hit).
				//However, we may have just missed a plane which is now "behind" the contact point while still being inside the radius
				if (d < -FindFaceInRadius)
					continue;

				// Calculate direction dot plane normal
				const PxReal normalDot = plane.n.dot(localDir);
				// If this is more opposing than our current 'most opposing' normal, update 'most opposing'
				if (normalDot < minNormalDot)
				{
					minIndex = j;
					bMinIndexValid = true;
					minNormalDot = normalDot;
				}
			}

			// If we found at least one face that we are considered 'on', use best normal
			if (bMinIndexValid)
			{
				return minIndex;
			}
			// Fallback is the face that we are most in front of
			else
			{
				return maxDIndex;
			}
		}
	}

	return PHit.faceIndex;	//If no custom logic just return whatever face index they initially had
}

FPhysXShapeAdaptor::FPhysXShapeAdaptor(const FQuat& Rot, const FCollisionShape& CollisionShape)
	: Rotation(physx::PxIdentity)
{
	// Perform other kinds of zero-extent queries as zero-extent sphere queries
	if ((CollisionShape.ShapeType != ECollisionShape::Sphere) && CollisionShape.IsNearlyZero())
	{
		PtrToUnionData = UnionData.SetSubtype<PxSphereGeometry>(PxSphereGeometry(FCollisionShape::MinSphereRadius()));
	}
	else
	{
		switch (CollisionShape.ShapeType)
		{
		case ECollisionShape::Box:
		{
			PxVec3 BoxExtents = U2PVector(CollisionShape.GetBox());
			BoxExtents.x = FMath::Max(BoxExtents.x, FCollisionShape::MinBoxExtent());
			BoxExtents.y = FMath::Max(BoxExtents.y, FCollisionShape::MinBoxExtent());
			BoxExtents.z = FMath::Max(BoxExtents.z, FCollisionShape::MinBoxExtent());

			PtrToUnionData = UnionData.SetSubtype<PxBoxGeometry>(PxBoxGeometry(BoxExtents));
			Rotation = U2PQuat(Rot);
			break;
		}
		case ECollisionShape::Sphere:
			PtrToUnionData = UnionData.SetSubtype<PxSphereGeometry>(PxSphereGeometry(FMath::Max(CollisionShape.GetSphereRadius(), FCollisionShape::MinSphereRadius())));
			break;
		case ECollisionShape::Capsule:
		{
			const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
			const float CapsuleHalfHeight = CollisionShape.GetCapsuleHalfHeight();
			if (CapsuleRadius < CapsuleHalfHeight)
			{
				PtrToUnionData = UnionData.SetSubtype<PxCapsuleGeometry>(PxCapsuleGeometry(FMath::Max(CapsuleRadius, FCollisionShape::MinCapsuleRadius()), FMath::Max(CollisionShape.GetCapsuleAxisHalfLength(), FCollisionShape::MinCapsuleAxisHalfHeight())));
				Rotation = ConvertToPhysXCapsuleRot(Rot);
			}
			else
			{
				// Use a sphere instead.
				PtrToUnionData = UnionData.SetSubtype<PxSphereGeometry>(PxSphereGeometry(FMath::Max(CapsuleRadius, FCollisionShape::MinSphereRadius())));
			}
			break;
		}
		default:
			// invalid type
			ensure(false);
		}
	}
}

static const PxQuat CapsuleRotator(0.f, 0.707106781f, 0.f, 0.707106781f);

PxQuat ConvertToPhysXCapsuleRot(const FQuat& GeomRot)
{
	// Rotation required because PhysX capsule points down X, we want it down Z
	return U2PQuat(GeomRot) * CapsuleRotator;
}

FQuat ConvertToUECapsuleRot(const PxQuat & PGeomRot)
{
	return P2UQuat(PGeomRot * CapsuleRotator.getConjugate());
}

FQuat ConvertToUECapsuleRot(const FQuat & GeomRot)
{
	return GeomRot * P2UQuat(CapsuleRotator.getConjugate());
}

PxTransform ConvertToPhysXCapsulePose(const FTransform& GeomPose)
{
	PxTransform PFinalPose;

	PFinalPose.p = U2PVector(GeomPose.GetTranslation());
	// Rotation required because PhysX capsule points down X, we want it down Z
	PFinalPose.q = ConvertToPhysXCapsuleRot(GeomPose.GetRotation());
	return PFinalPose;
}


PxFilterData CreateObjectQueryFilterData(const bool bTraceComplex, const int32 MultiTrace/*=1 if multi. 0 otherwise*/, const struct FCollisionObjectQueryParams & ObjectParam)
{
	/**
	* Format for QueryData :
	*		word0 (meta data - ECollisionQuery. Extendable)
	*
	*		For object queries
	*
	*		word1 (object type queries)
	*		word2 (unused)
	*		word3 (Multi (1) or single (0) (top 8) + Flags (lower 24))
	*/

	PxFilterData PNewData;

	PNewData.word0 = (uint32)ECollisionQuery::ObjectQuery;

	if (bTraceComplex)
	{
		PNewData.word3 |= EPDF_ComplexCollision;
	}
	else
	{
		PNewData.word3 |= EPDF_SimpleCollision;
	}

	// get object param bits
	PNewData.word1 = ObjectParam.GetQueryBitfield();

	// if 'nothing', then set no bits
	PNewData.word3 |= CreateChannelAndFilter((ECollisionChannel)MultiTrace, ObjectParam.IgnoreMask);

	return PNewData;
}

PxFilterData CreateTraceQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionQueryParams& Params)
{
	/**
	* Format for QueryData :
	*		word0 (meta data - ECollisionQuery. Extendable)
	*
	*		For trace queries
	*
	*		word1 (blocking channels)
	*		word2 (touching channels)
	*		word3 (MyChannel (top 8) as ECollisionChannel + Flags (lower 24))
	*/

	PxFilterData PNewData;

	PNewData.word0 = (uint32)ECollisionQuery::TraceQuery;

	if (bTraceComplex)
	{
		PNewData.word3 |= EPDF_ComplexCollision;
	}
	else
	{
		PNewData.word3 |= EPDF_SimpleCollision;
	}

	// word1 encodes 'what i block', word2 encodes 'what i touch'
	for (int32 i = 0; i<ARRAY_COUNT(InCollisionResponseContainer.EnumArray); i++)
	{
		if (InCollisionResponseContainer.EnumArray[i] == ECR_Block)
		{
			// if i block, set that in word1
			PNewData.word1 |= CRC_TO_BITFIELD(i);
		}
		else if (InCollisionResponseContainer.EnumArray[i] == ECR_Overlap)
		{
			// if i touch, set that in word2
			PNewData.word2 |= CRC_TO_BITFIELD(i);
		}
	}

	// if 'nothing', then set no bits
	PNewData.word3 |= CreateChannelAndFilter((ECollisionChannel)MyChannel, Params.IgnoreMask);

	return PNewData;
}

#define TRACE_MULTI		1
#define TRACE_SINGLE	0

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
PxFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const struct FCollisionQueryParams& QueryParam, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace)
{
	if (ObjectParam.IsValid())
	{
		return CreateObjectQueryFilterData(bTraceComplex, (bMultitrace ? TRACE_MULTI : TRACE_SINGLE), ObjectParam);
	}
	else
	{
		return CreateTraceQueryFilterData(MyChannel, bTraceComplex, InCollisionResponseContainer, QueryParam);
	}
}

#endif